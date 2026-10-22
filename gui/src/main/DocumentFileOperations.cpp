#include "QtRocket/gui/main/DocumentFileOperations.h"

#include <chrono>
#include <cstddef>
#include <expected>
#include <filesystem>
#include <fstream>
#include <ios>
#include <memory>
#include <optional>
#include <system_error>
#include <utility>
#include <vector>

#include <QAction>
#include <QChar>
#include <QCursor>
#include <QGuiApplication>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <Qt>
#include <QtContainerFwd>
#include <QtTypes>

#include "QtRocket/document/DecalImage.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/StorageOptions.h"
#include "QtRocket/document/events/DocumentChangeEvent.h"
#include "QtRocket/file/GeneralRocketLoader.h"
#include "QtRocket/file/GeneralRocketSaver.h"
#include "QtRocket/file/LoadedDocument.h"
#include "QtRocket/gui/main/DesignPanel.h"
#include "QtRocket/gui/main/MainMenus.h"
#include "QtRocket/gui/main/MainWindow.h"
#include "QtRocket/gui/main/WindowManager.h"
#include "QtRocket/gui/tree/DocumentSelectionModel.h"
#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/gui/util/GuiServices.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/util/UserPrompts.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/simulation/FlightData.h"
#include "QtRocket/simulation/FlightDataBranch.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"

namespace QtRocket::Gui
{

namespace
{

/// The wait cursor while a design is read or written (decision O15), until this ends.
class WaitCursor final
{
public:
    WaitCursor() { QGuiApplication::setOverrideCursor(QCursor(Qt::WaitCursor)); }
    ~WaitCursor() { QGuiApplication::restoreOverrideCursor(); }
    WaitCursor(const WaitCursor&)            = delete;
    WaitCursor& operator=(const WaitCursor&) = delete;
    WaitCursor(WaitCursor&&)                 = delete;
    WaitCursor& operator=(WaitCursor&&)      = delete;
};

/// @p file made absolute as Java's File.getAbsolutePath() makes it: resolved against the
/// current directory as text, nothing of it changed (std::filesystem::absolute() on Windows
/// drops the dots and spaces that end a name); @p file when the current directory is unknown.
[[nodiscard]] std::filesystem::path absoluteFile(const std::filesystem::path& file)
{
    if (file.is_absolute())
    {
        return file;
    }
    std::error_code             error;
    const std::filesystem::path current = std::filesystem::current_path(error);
    return error ? file : current / file;
}

/// Whether @p first and @p second are the same file: the same file of the file system when
/// both exist, else the same absolute path.
[[nodiscard]] bool isSameFile(const std::filesystem::path& first,
                              const std::filesystem::path& second)
{
    std::error_code error;
    if (std::filesystem::equivalent(first, second, error))
    {
        return true;
    }
    return absoluteFile(first).lexically_normal() == absoluteFile(second).lexically_normal();
}

/// Whether @p file can be opened for reading (Java's FileInputStream, whose
/// FileNotFoundException BasicFrame.open() reports as "File not found"): it exists, is no
/// directory and opens.
[[nodiscard]] bool canOpenForReading(const std::filesystem::path& file)
{
    std::error_code error;
    if (std::filesystem::is_directory(file, error))
    {
        return false;
    }
    const std::ifstream stream(file, std::ios::binary);
    return stream.is_open();
}

/// Whether @p file exists (Java: File.exists()).
[[nodiscard]] bool fileExists(const std::filesystem::path& file)
{
    std::error_code error;
    return std::filesystem::exists(file, error);
}

/// The texts of the messages of @p messages, in order (each toString()).
template <class Messages>
[[nodiscard]] QStringList messageTexts(const Messages& messages)
{
    QStringList texts;
    for (const auto& message : messages)
    {
        texts.append(QtStrings::fromUtf8(message.toString()));
    }
    return texts;
}

/// Shows @p window (back from minimized) and makes it the active window.
void activate(MainWindow& window)
{
    if (window.isMinimized())
    {
        window.setWindowState(window.windowState() & ~Qt::WindowMinimized);
    }
    window.show();
    window.raise();
    window.activateWindow();
}

/// The extension of OpenRocket's design files.
[[nodiscard]] QString orkExtension()
{
    return QStringLiteral("ork");
}

}  // namespace

DocumentFileOperations::DocumentFileOperations(WindowManager& windows, const GuiServices& services,
                                               QObject* parent)
  : QObject(parent), m_windows(&windows), m_services(services)
{
    // The Save of BasicFrame.closeAction()'s question.
    windows.setSaveHook([this](MainWindow& window) { return save(window); });
    connect(&windows, &WindowManager::menusCreated, this,
            guarded([this](MainMenus* menus, MainWindow* window) { connectMenus(*menus, window); },
                    QStringLiteral("File menus")));
    for (MainWindow* const window : windows.windows())
    {
        connectMenus(window->menus(), window);
    }
    if (MainMenus* const menus = windows.noWindowMenus(); menus != nullptr)
    {
        connectMenus(*menus, nullptr);
    }
}

DocumentFileOperations::~DocumentFileOperations()
{
    m_windows->setSaveHook({});
}

QString DocumentFileOperations::openRocketDesignFilter()
{
    // FileHelper.OPENROCKET_DESIGN_FILTER (".ork", ".ork.gz"), its patterns spelled out.
    //: FileHelper.OPENROCKET_DESIGN_FILTER
    return tr("OpenRocket designs") + QStringLiteral(" (*.ork *.ork.gz)");
}

QString DocumentFileOperations::allFilesFilter()
{
    // Swing's accept-all filter (FileChooser.acceptAllFileFilterText).
    return tr("All Files") + QStringLiteral(" (*)");
}

QStringList DocumentFileOperations::openFilters()
{
    // BasicFrame.openAction(parent, OPENROCKET_DESIGN_FILTER): the OpenRocket designs chosen,
    // and the accept-all filter (the filter of all designs comes with RockSim, Milestone 8).
    return {openRocketDesignFilter(), allFilesFilter()};
}

std::filesystem::path DocumentFileOperations::forceExtension(const std::filesystem::path& file,
                                                             const QString&               extension)
{
    // FileHelper.forceExtension(): the extension of the name is what follows its last dot,
    // unless that dot is its first character.
    const QString   name  = QtStrings::fromPath(file.filename());
    const qsizetype index = name.lastIndexOf(QChar(u'.'));
    const QString   given = index > 0 ? name.mid(index + 1) : QString();
    if (given.toLower() == extension.toLower())
    {
        return file;
    }
    QString absolute = QtStrings::fromPath(absoluteFile(file));
    if (!extension.startsWith(QChar(u'.')))
    {
        absolute += QChar(u'.');
    }
    return QtStrings::toPath(absolute + extension);
}

bool DocumentFileOperations::hasSimulatedData(const OpenRocketDocument& document)
{
    // StorageOptionChooser.verifyStorageOptions(); getStatus() works the status out, as in Java.
    for (const std::shared_ptr<Simulation>& simulation : document.getSimulations())
    {
        const Simulation::Status status = simulation->getStatus();
        if (status == Simulation::Status::NOT_SIMULATED || status == Simulation::Status::EXTERNAL)
        {
            continue;
        }
        const std::shared_ptr<FlightData>& data = simulation->getSimulatedData();
        if (data == nullptr)
        {
            continue;
        }
        for (std::size_t branch = 0; branch < data->getBranchCount(); ++branch)
        {
            if (data->getBranch(branch).getLength() > 0)
            {
                return true;
            }
        }
    }
    return false;
}

void DocumentFileOperations::connectMenus(MainMenus& menus, MainWindow* window)
{
    connect(menus.openAction(), &QAction::triggered, this,
            guarded([this, window] { openAction(window); }, QStringLiteral("Open")));
    menus.openAction()->setEnabled(true);
    if (window == nullptr)
    {
        return;
    }
    connect(menus.saveAction(), &QAction::triggered, this,
            guarded([this, window] { static_cast<void>(save(*window)); }, QStringLiteral("Save")));
    menus.saveAction()->setEnabled(true);
    connect(
        menus.saveAsAction(), &QAction::triggered, this,
        guarded([this, window] { static_cast<void>(saveAs(*window)); }, QStringLiteral("Save as")));
    menus.saveAsAction()->setEnabled(true);
    // BasicFrame: ComponentConfigDialog.showDialog(BasicFrame.this, document, rocket).
    connect(menus.propertiesAction(), &QAction::triggered, window,
            guarded([window] { window->requestConfigDialog(window->document().getRocket()); },
                    QStringLiteral("Properties")));
    menus.propertiesAction()->setEnabled(true);
}

void DocumentFileOperations::openAction(MainWindow* origin)
{
    const QPointer<MainWindow>   alive(origin);
    Preferences&                 preferences = m_services.preferences();
    UserPrompts::OpenFileRequest request;
    request.nameFilters   = openFilters();
    request.directory     = preferences.getDefaultDirectory().value_or(std::filesystem::path());
    request.multipleFiles = true;
    const std::optional<UserPrompts::OpenFileAnswer> answer =
        m_services.prompts().askOpenFiles(origin, request);
    if (!answer.has_value())
    {
        return;
    }
    preferences.setDefaultDirectory(answer->directory);
    for (const std::filesystem::path& file : answer->files)
    {
        static_cast<void>(open(file, alive.data()));
    }
}

MainWindow* DocumentFileOperations::open(const std::filesystem::path& file, MainWindow* origin)
{
    const QPointer<MainWindow>  alive(origin);
    const std::filesystem::path absolute = absoluteFile(file);
    // Decision U4: a file open in a window already is shown there (OpenRocket opens it again).
    if (MainWindow* const window = findWindowOfFile(absolute); window != nullptr)
    {
        activate(*window);
        Q_EMIT designFileOpened(QtStrings::fromPath(absolute));
        return window;
    }
    std::unique_ptr<OpenRocketDocument> document =
        load(absolute, QtStrings::fromPath(file.filename()), origin, false);
    if (document == nullptr)
    {
        return nullptr;
    }
    // OpenFileWorker.
    document->setFile(absolute);
    document->setSaved(true);
    MainWindow& window = m_windows->addDocument(std::move(document), alive.data());
    Q_EMIT designFileOpened(QtStrings::fromPath(absolute));
    return &window;
}

bool DocumentFileOperations::openFiles(const std::vector<std::filesystem::path>& files,
                                       MainWindow*                               origin)
{
    const QPointer<MainWindow> alive(origin);
    bool                       opened = false;
    for (const std::filesystem::path& file : files)
    {
        if (open(file, alive.data()) != nullptr)
        {
            opened = true;
        }
    }
    return opened;
}

MainWindow* DocumentFileOperations::openExample(const std::filesystem::path& file,
                                                MainWindow*                  origin)
{
    const QPointer<MainWindow>          alive(origin);
    std::unique_ptr<OpenRocketDocument> document =
        load(file, QtStrings::fromPath(file.filename()), origin, true);
    if (document == nullptr)
    {
        return nullptr;
    }
    // OpenFileWorker(URL): no file, saved.
    document->setFile(std::nullopt);
    document->setSaved(true);
    MainWindow& window = m_windows->addDocument(std::move(document), alive.data());
    // SwingUtilities.invokeLater(() -> ComponentConfigDialog.showDialog(frame, doc,
    // doc.getRocket())); dropped when the window is closed first.
    QTimer::singleShot(
        std::chrono::milliseconds(0), &window,
        guarded([&window] { window.requestConfigDialog(window.document().getRocket()); },
                QStringLiteral("Example design")));
    return &window;
}

bool DocumentFileOperations::save(MainWindow& window)
{
    const QPointer<MainWindow> alive(&window);
    OpenRocketDocument&        document = window.document();
    document.fireDocumentSavingEvent(DocumentChangeEvent{});
    if (alive.isNull())
    {
        return false;
    }
    const std::optional<std::filesystem::path> file = document.getFile();
    const StorageOptions::FileType type = document.getDefaultStorageOptions().getFileType();
    if (!file.has_value() || type == StorageOptions::FileType::ROCKSIM ||
        type == StorageOptions::FileType::RASAERO)
    {
        return saveAs(window);
    }
    return saveAsOpenRocket(window, *file);
}

bool DocumentFileOperations::saveAs(MainWindow& window)
{
    const QPointer<MainWindow> alive(&window);
    showSaveDesignInfoDialog(window);
    if (alive.isNull())
    {
        return false;
    }
    std::optional<std::filesystem::path> file = askSaveAsFile(window);
    if (!file.has_value() || alive.isNull())
    {
        return false;
    }
    *file = forceExtension(*file, orkExtension());
    // FileHelper.confirmWrite().
    if (fileExists(*file) &&
        (!m_services.prompts().askOverwrite(&window, QtStrings::fromPath(file->filename())) ||
         alive.isNull()))
    {
        return false;
    }
    if (!saveAsOpenRocket(window, *file))
    {
        return false;
    }
    Q_EMIT designFileSavedAs(QtStrings::fromPath(absoluteFile(*file)));
    return true;
}

MainWindow* DocumentFileOperations::findWindowOfFile(const std::filesystem::path& file) const
{
    for (MainWindow* const window : m_windows->windows())
    {
        const std::optional<std::filesystem::path>& windowFile = window->document().getFile();
        if (windowFile.has_value() && isSameFile(*windowFile, file))
        {
            return window;
        }
    }
    return nullptr;
}

std::unique_ptr<OpenRocketDocument> DocumentFileOperations::load(const std::filesystem::path& file,
                                                                 const QString& displayName,
                                                                 MainWindow* parent, bool asBytes)
{
    UserPrompts& prompts = m_services.prompts();
    // BasicFrame.open(): the hard-coded texts of its errors.
    if (!canOpenForReading(file))
    {
        prompts.showMessage(parent, {.severity = UserPrompts::Severity::ERROR,
                                     //: BasicFrame's text (not in messages.properties)
                                     .title = tr("Error opening file"),
                                     //: BasicFrame's text (not in messages.properties)
                                     .lines = {tr("File not found: %1").arg(displayName)}});
        return nullptr;
    }
    Result<LoadedDocument> loaded = [this, &file, asBytes] -> Result<LoadedDocument> {
        const WaitCursor          wait;
        const GeneralRocketLoader loader(m_services.loadingContext());
        if (!asBytes)
        {
            return loader.load(file);
        }
        // GeneralRocketLoader(URL): the bytes, without a directory of the design.
        Result<std::vector<std::byte>> bytes = readFile(file, GeneralRocketLoader::kMaxFileBytes);
        if (!bytes.has_value())
        {
            return std::unexpected(bytes.error());
        }
        return loader.load(std::move(*bytes));
    }();
    if (!loaded.has_value())
    {
        prompts.showMessage(
            parent,
            {.severity = UserPrompts::Severity::ERROR,
             //: BasicFrame's text (not in messages.properties)
             .title = tr("Error opening file"),
             //: BasicFrame's text (not in messages.properties)
             .lines = {tr("Unable to open file '%1': %2")
                           .arg(displayName, QtStrings::fromUtf8(loaded.error().message))}});
        return nullptr;
    }
    if (!loaded->warnings.empty())
    {
        prompts.showWarnings(parent,
                             {//: BasicFrame.WarningDialog.title
                              .title = tr("Warnings while opening file"),
                              .lines =
                                  {//: BasicFrame.WarningDialog.txt1
                                   tr("The following problems were encountered while opening") +
                                       QChar(u' ') + displayName + QChar(u'.'),
                                   //: BasicFrame.WarningDialog.txt2
                                   tr("Some design features may not have been loaded correctly.")},
                              .warnings = messageTexts(loaded->warnings)});
    }
    return std::move(loaded->document);
}

bool DocumentFileOperations::saveAsOpenRocket(MainWindow&                  window,
                                              const std::filesystem::path& target)
{
    const QPointer<MainWindow>  alive(&window);
    const std::filesystem::path file = forceExtension(target, orkExtension());
    if (!verifyStorageOptions(window) || alive.isNull())
    {
        return false;
    }
    OpenRocketDocument& document = window.document();
    StorageOptions&     options  = document.getDefaultStorageOptions();
    // Decision O26: no preview image before the 3D figure of Milestone 5, which BasicFrame
    // clears as when generatePreviewImage() gives none.
    options.clearPreviewImage();
    options.setFileType(StorageOptions::FileType::OPENROCKET);

    UserPrompts&       prompts = m_services.prompts();
    GeneralRocketSaver saver(m_services.preferences());
    while (true)
    {
        const Result<void> saved = [&saver, &file, &document] {
            const WaitCursor wait;
            return saver.save(file, document);
        }();
        if (saved.has_value())
        {
            document.setFile(file);
            document.setSaved(true);
            window.refreshTitle();
            showSaveWarnings(window, file, messageTexts(saver.getErrors()),
                             messageTexts(saver.getWarnings()));
            return true;
        }
        const std::shared_ptr<DecalImage> decal = saver.getMissingDecal();
        if (saved.error().code != ErrorCode::NOT_FOUND || decal == nullptr)
        {
            // BasicFrame's hard-coded texts of an IOException.
            prompts.showMessage(&window, {.severity = UserPrompts::Severity::ERROR,
                                          //: BasicFrame's text (not in messages.properties)
                                          .title = tr("Saving failed"),
                                          //: BasicFrame's text (not in messages.properties)
                                          .lines = {tr("An I/O error occurred while saving:"),
                                                    QtStrings::fromUtf8(saved.error().message)}});
            return false;
        }
        // The DecalNotFoundException: the user looks for the image's source file, or the image
        // is left out of the next save; then the save is tried again.
        Preferences&                                     preferences = m_services.preferences();
        const std::optional<UserPrompts::OpenFileAnswer> found       = prompts.askDecalFile(
            &window, QtStrings::fromUtf8(saved.error().message),
            preferences.getDefaultDirectory().value_or(std::filesystem::path()));
        if (alive.isNull())
        {
            return false;
        }
        if (found.has_value())
        {
            decal->setDecalFile(found->files.front());
            preferences.setDefaultDirectory(found->directory);
        }
        else
        {
            decal->setIgnored(true);
        }
    }
}

bool DocumentFileOperations::verifyStorageOptions(MainWindow& window)
{
    StorageOptions& options = window.document().getDefaultStorageOptions();
    if (options.isExplicitlySet() || !hasSimulatedData(window.document()))
    {
        // Set by the user, or no simulated data to store.
        return true;
    }
    const QPointer<MainWindow> alive(&window);
    const std::optional<bool>  saveSimulationData =
        m_services.prompts().askStorageOptions(&window, options.getSaveSimulationData());
    if (!saveSimulationData.has_value() || alive.isNull())
    {
        return false;
    }
    // StorageOptionChooser.storeOptions().
    options.setSaveSimulationData(*saveSimulationData);
    options.setExplicitlySet(true);
    return true;
}

void DocumentFileOperations::showSaveDesignInfoDialog(MainWindow& window)
{
    if (!m_services.preferences().isShowSaveRocketInfo())
    {
        return;
    }
    // "Select the rocket in the component tree to indicate to users that they can edit the
    // rocket info by editing the rocket".
    if (const DesignPanel* const panel = window.componentDesignPanel(); panel != nullptr)
    {
        panel->selection().setSelectedComponent(window.document().getRocket());
    }
    m_services.prompts().editDesignInformation(&window, window.document());
}

std::optional<std::filesystem::path> DocumentFileOperations::askSaveAsFile(MainWindow& window)
{
    // DesignFileSaveAsFileChooser.build(document, OPENROCKET).
    OpenRocketDocument&          document    = window.document();
    Preferences&                 preferences = m_services.preferences();
    UserPrompts::SaveFileRequest request;
    //: saveAs.openrocket.title
    request.title       = tr("Save as OpenRocket ork file");
    request.nameFilters = {openRocketDesignFilter()};
    request.directory   = preferences.getDefaultDirectory().value_or(std::filesystem::path());
    if (const std::optional<std::filesystem::path> name = document.getFileNoExtension();
        name.has_value())
    {
        // JFileChooser.setSelectedFile(): an absolute file takes the chooser to its directory.
        const std::filesystem::path proposed = forceExtension(*name, orkExtension());
        if (proposed.is_absolute())
        {
            request.directory = proposed.parent_path();
        }
        request.fileName = QtStrings::fromPath(proposed.filename());
    }
    StorageOptions& options = document.getDefaultStorageOptions();
    if (hasSimulatedData(document))
    {
        request.saveSimulationData = options.getSaveSimulationData();
    }
    const QPointer<MainWindow>                       alive(&window);
    const std::optional<UserPrompts::SaveFileAnswer> answer =
        m_services.prompts().askSaveFile(&window, request);
    if (!answer.has_value() || alive.isNull())
    {
        return std::nullopt;
    }
    // StorageOptionChooser.storeOptions(), then the chooser's directory.
    if (answer->saveSimulationData.has_value())
    {
        options.setSaveSimulationData(*answer->saveSimulationData);
    }
    options.setExplicitlySet(true);
    preferences.setDefaultDirectory(answer->directory);
    return answer->file;
}

void DocumentFileOperations::showSaveWarnings(MainWindow& window, const std::filesystem::path& file,
                                              const QStringList& errors,
                                              const QStringList& warnings)
{
    if (errors.isEmpty() && warnings.isEmpty())
    {
        return;
    }
    // BasicFrame.saveRASAeroFile()'s messages (decision U4).
    const QString first =
        //: BasicFrame.WarningDialog.saving.txt1
        tr("The following problems were encountered while saving") + QStringLiteral(" '") +
        QtStrings::fromPath(file.filename()) + QStringLiteral("'.");
    UserPrompts& prompts = m_services.prompts();
    if (errors.isEmpty())
    {
        prompts.showWarnings(
            &window,
            {//: BasicFrame.WarningDialog.saving.title
             .title    = tr("Warnings while saving file"),
             .lines    = {first,
                          //: BasicFrame.WarningDialog.saving.txt2
                          tr("Some design features may not have exported correctly.")},
             .warnings = warnings});
        return;
    }
    prompts.showErrorsAndWarnings(&window,
                                  {//: BasicFrame.ErrorWarningDialog.saving.title
                                   .title  = tr("Errors/Warnings while saving file"),
                                   .lines  = {first,
                                              //: BasicFrame.ErrorWarningDialog.txt1
                                              tr("<html>Please <b>correct the errors</b>.</html>")},
                                   .errors = errors,
                                   .warnings = warnings});
}

}  // namespace QtRocket::Gui
