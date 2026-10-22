#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <vector>

#include <QObject>
#include <QString>
#include <QStringList>
#include <QtContainerFwd>

#include "QtRocket/gui/util/GuiServices.h"

namespace QtRocket
{
class OpenRocketDocument;
}

namespace QtRocket::Gui
{

class MainMenus;
class MainWindow;
class WindowManager;

/// The file operations of the design windows (BasicFrame's openAction(), open(), saveAction(),
/// saveAsAction(), saveAsOpenRocket() and the helpers of FileHelper, OpenFileWorker,
/// SaveFileWorker and StorageOptionChooser), for the windows of one WindowManager: File > Open,
/// Save and Save as of every window (and Open of the menus of no window), the Save of the "Design
/// not saved" question (WindowManager's save hook), the design files of the command line and of
/// the platform (ApplicationSession), the recent files and the examples (built on open() and
/// openExample()). Every question goes through UserPrompts; the load and the save run at once,
/// under a wait cursor (decision O15).
///
/// OPEN (open()): a file already open in a window activates that window (decision U4); otherwise
/// the file is loaded (GeneralRocketLoader with the services' loading context). A file that
/// cannot be opened for reading (missing, a directory, unreadable: Java's FileNotFoundException)
/// is the error "File not found: <name>", a load that fails "Unable to open file '<name>':
/// <the load's error>", both titled "Error opening file" (BasicFrame's hard-coded texts). The
/// load's warnings are shown before the window opens ("Warnings while opening file": "The
/// following problems were encountered while opening <name>." and "Some design features may not
/// have been loaded correctly.", then each warning). The document gets the file and is saved
/// (OpenFileWorker), and opens in a window of its own, which takes the place of the window the
/// open came from when that one is a New design still saved (WindowManager::addDocument()).
/// openAction() asks for the files first (File > Open: "All Files (*)" and "OpenRocket designs
/// (*.ork *.ork.gz)" in JFileChooser's order, the designs chosen, several at once, starting in the
/// default directory, which the directory the user ended in becomes) and opens each. A file
/// opened gets the absolute path of what was opened, with the platform's separators. openExample()
/// opens an example design as BasicFrame's open(URL): read as bytes without a directory
/// (GeneralRocketLoader(URL)), without a file, saved, and the rocket's configuration dialog is
/// asked for once the window is shown.
///
/// SAVE (save()): BasicFrame.saveAction(): the document is told first
/// (fireDocumentSavingEvent()); a document without a file, or of RockSim or RASAero, is saved as
/// (saveAs()); otherwise it is written to its file (saveAsOpenRocket()).
///
/// SAVE AS (saveAs()): the "Save Design Info" dialog first when the preference asks for it (the
/// rocket selected in the tree; UserPrompts::editDesignInformation(), the hook of run 4); then the
/// file ("Save as OpenRocket ork file", the OpenRocket designs only, starting in the default
/// directory (the user's home directory without one); the document's file with the extension
/// forced is proposed, in its directory, only when SaveFileChooser takes it
/// (SaveFileNames::isProposable(): a file of the default directory, not one of another directory
/// or of a subdirectory); names with an illegal character refused (SaveFileChooser); the storage
/// option offered when the design holds simulated data); the storage options are stored and
/// marked explicitly set, the directory becomes the default one, the extension "ork" is forced
/// (forceExtension()), and an existing file is overwritten only when the user agrees ("File
/// exists"). Then saveAsOpenRocket().
///
/// WRITING (saveAsOpenRocket()): the extension forced again; the storage options asked when they
/// are not set explicitly and the design holds simulated data (verifyStorageOptions(): "Save
/// options"; Cancel cancels the save); no preview image (decision O26: it comes with the 3D
/// figure of Milestone 5, so the options' image is cleared, as BasicFrame does without one); the
/// file type OPENROCKET; GeneralRocketSaver. A save that succeeds sets the document's file, marks
/// it saved and renews the window's title; then the saver's warnings are shown (decision U4:
/// "Warnings while saving file", "The following problems were encountered while saving
/// '<name>'." and "Some design features may not have exported correctly.", then each warning), or
/// its errors with its warnings ("Errors/Warnings while saving file", "<html>Please <b>correct
/// the errors</b>.</html>"). A decal image whose source file is missing asks for it
/// (UserPrompts::askDecalFile(), DecalNotFoundDialog): the chosen file becomes the image's source
/// (and its directory the default one), else the image is left out; then the save is tried
/// again. Any other failure is "An I/O error occurred while saving:" and the error's text,
/// titled "Saving failed".
///
/// The window of a file operation may be closed while one of its questions is open (another
/// window's event); the operation then stops, as if cancelled.
///
/// Deviations: the same file opened twice activates its window (U4; OpenRocket opens it again in
/// a second window, and the two then overwrite each other); the document's file is the absolute
/// path of what was opened (Java keeps a relative File of the command line as it is: the same
/// file, since the current directory never changes); the save's warnings are shown (U4; Java
/// shows none after an .ork save, only after a RASAero one, whose texts these are); no preview
/// image is written (O26); the storage option is offered only when the design holds simulated
/// data, as a question after the file dialog (Java's Save As chooser always has it beside the
/// files; it marks the options explicitly set in both); the filter's text lists both of its
/// patterns ("OpenRocket designs (*.ork *.ork.gz)"; Java: "OpenRocket designs (*.ork)", which
/// takes ".ork.gz" too); the decal question has the window as its parent (Java: none); a missing
/// decal that names no image of the document (which no design QtRocket loads can have) fails as
/// an I/O error, where Java would ask again forever; the text of an I/O error is QtRocket's
/// error text (Java: the exception's toString(), "java.io.IOException: ..."); the window does
/// not ask for the focus back after the questions (BasicFrame.restoreFocus(), for macOS: Qt gives
/// it back when a modal dialog closes); a load and a save run on the GUI thread under a wait
/// cursor, without SwingWorkerDialog's progress dialog and its Cancel (O15).
class DocumentFileOperations final : public QObject
{
    Q_OBJECT

public:
    /// The operations of the windows of @p windows, with @p services (the windows'); both must
    /// outlive this object. It becomes the manager's save hook and connects the menus of every
    /// window (those open now and those to come, and the menus of no window).
    DocumentFileOperations(WindowManager& windows, const GuiServices& services,
                           QObject* parent = nullptr);
    /// Takes the save hook back from the manager.
    ~DocumentFileOperations() override;
    DocumentFileOperations(const DocumentFileOperations&)            = delete;
    DocumentFileOperations& operator=(const DocumentFileOperations&) = delete;
    DocumentFileOperations(DocumentFileOperations&&)                 = delete;
    DocumentFileOperations& operator=(DocumentFileOperations&&)      = delete;

    // ------------------------------------------------------------------ the texts and rules

    /// FileHelper.OPENROCKET_DESIGN_FILTER as a Qt name filter: "OpenRocket designs (*.ork
    /// *.ork.gz)".
    [[nodiscard]] static QString openRocketDesignFilter();
    /// The file chooser's accept-all filter: "All Files (*)".
    [[nodiscard]] static QString allFilesFilter();
    /// The name filters of File > Open, in JFileChooser's order (its accept-all filter first);
    /// openAction() chooses openRocketDesignFilter().
    [[nodiscard]] static QStringList openFilters();

    /// FileHelper.forceExtension(@p file, @p extension): @p file when the text after the last
    /// dot of its name (a dot that is not its first character) is @p extension, ignoring case;
    /// otherwise the absolute path of @p file with "." and @p extension added ("x.ork.gz" ->
    /// "x.ork.gz.ork", ".ork" -> ".ork.ork", "x." -> "x..ork"). An @p extension that starts with
    /// a dot is added as it is. A file made absolute has the platform's separators.
    [[nodiscard]] static std::filesystem::path forceExtension(const std::filesystem::path& file,
                                                              const QString& extension);

    /// Whether @p document holds simulated data the storage options decide about
    /// (verifyStorageOptions()): a simulation that is not NOT_SIMULATED or EXTERNAL, with
    /// simulated data of which a branch has rows.
    [[nodiscard]] static bool hasSimulatedData(const OpenRocketDocument& document);

    // ------------------------------------------------------------------ the operations

    /// Connects File > Open of @p menus and, for the menus of @p window (null for the menus of
    /// no window), Save, Save as and Properties (the rocket's configuration dialog:
    /// MainWindow::requestConfigDialog()), and enables them.
    void connectMenus(MainMenus& menus, MainWindow* window);

    /// BasicFrame.openAction(): asks for the design files and opens each (open()) from
    /// @p origin, the window the request came from (null for none).
    void openAction(MainWindow* origin);

    /// BasicFrame.open(File, parent): opens @p file (see the class comment) from @p origin
    /// (null for none, as from the command line). The window of the file, or null when it could
    /// not be opened (the error was shown). A file that was opened, or whose window was
    /// activated, is told by designFileOpened().
    MainWindow* open(const std::filesystem::path& file, MainWindow* origin);

    /// Opens each of @p files (SwingStartup.handleCommandLine(), the platform's requests) from
    /// @p origin; whether any was opened.
    bool openFiles(const std::vector<std::filesystem::path>& files, MainWindow* origin);

    /// BasicFrame.open(URL, parent) of an example design (ExampleDesignFileAction): opens the
    /// design @p file holds without a file, saved, and asks for the rocket's configuration
    /// dialog once the window is shown (queued). The window, or null when it could not be
    /// opened. Not told by designFileOpened() (no recent file).
    MainWindow* openExample(const std::filesystem::path& file, MainWindow* origin);

    /// BasicFrame.saveAction() for the design of @p window; whether it was saved.
    bool save(MainWindow& window);

    /// BasicFrame.saveAsAction() for the design of @p window; whether it was saved.
    bool saveAs(MainWindow& window);

    /// The open window whose design's file is @p file (the same file by another path too), or
    /// null.
    [[nodiscard]] MainWindow* findWindowOfFile(const std::filesystem::path& file) const;

Q_SIGNALS:
    /// The design file @p file (its absolute path, QtStrings::fromPath()) was opened or its
    /// window activated (MRUDesignFile.addFile() of BasicFrame.open()).
    void designFileOpened(const QString& file);
    /// The design was saved as @p file (its absolute path) by Save as (MRUDesignFile.addFile()
    /// of BasicFrame.saveAsAction()).
    void designFileSavedAs(const QString& file);

private:
    /// The design @p file holds, loaded (OpenFileWorker): with @p asBytes as an example (no
    /// directory); errors and warnings shown over @p parent, the name @p displayName. Null when
    /// it could not be loaded.
    [[nodiscard]] std::unique_ptr<OpenRocketDocument> load(const std::filesystem::path& file,
                                                           const QString&               displayName,
                                                           MainWindow* parent, bool asBytes);
    /// BasicFrame.saveAsOpenRocket(): writes the design of @p window to @p target (the extension
    /// forced).
    [[nodiscard]] bool saveAsOpenRocket(MainWindow& window, const std::filesystem::path& target);
    /// StorageOptionChooser.verifyStorageOptions(); false when the user cancelled.
    [[nodiscard]] bool verifyStorageOptions(MainWindow& window);
    /// BasicFrame.showSaveRocketInfoDialog().
    void showSaveDesignInfoDialog(MainWindow& window);
    /// The file of the Save As of the design of @p window (openFileSaveAsDialog()), with the
    /// storage options stored; nullopt when cancelled.
    [[nodiscard]] std::optional<std::filesystem::path> askSaveAsFile(MainWindow& window);
    /// Shows what the last save warned of or failed with (decision U4).
    void showSaveWarnings(MainWindow& window, const std::filesystem::path& file,
                          const QStringList& errors, const QStringList& warnings);

    WindowManager* m_windows;
    GuiServices    m_services;
};

}  // namespace QtRocket::Gui
