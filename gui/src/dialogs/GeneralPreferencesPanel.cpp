#include "QtRocket/gui/dialogs/GeneralPreferencesPanel.h"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <QChar>
#include <QCheckBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMargins>
#include <QPointer>
#include <QPushButton>
#include <QString>
#include <QStringList>
#include <QVBoxLayout>
#include <QWidget>
#include <QtContainerFwd>

#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/gui/dialogs/PreferencesStore.h"
#include "QtRocket/gui/settings/GuiPreferenceKeys.h"
#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/util/UserPrompts.h"
#include "QtRocket/gui/widgets/DescriptionArea.h"
#include "QtRocket/preferences/PreferenceKeys.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/FileIo.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket::Gui
{

namespace
{

using UserFiles = GeneralPreferencesPanel::UserFiles;

/// The page's top inset (MigLayout "ins 30lp n n n").
constexpr int kTopInset = 30;
/// The gap above the reset button ("gaptop 20lp").
constexpr int kResetGap = 20;
/// The least width of a list's field ("w 100px").
constexpr int kFieldMinimumWidth = 100;
/// The description's rows and size change (DescriptionArea(text, 3, -1.5f, false)).
constexpr int    kDescriptionRows = 3;
constexpr double kDescriptionSize = -1.5;

/// The context of the page's reports (Guard).
QString context()
{
    return QStringLiteral("General preferences");
}

/// The preference key of @p files, which its field is named after.
[[nodiscard]] std::string_view preferenceKey(UserFiles files)
{
    switch (files)
    {
        case UserFiles::THRUST_CURVES:
            return PreferenceKeys::kUserThrustCurvesKey;
        case UserFiles::COMPONENT_PRESETS:
            return PreferenceKeys::kUserComponentPresetsKey;
    }
    QTROCKET_UNREACHABLE();
}

/// The text a field of @p files shows (getUserThrustCurveFilesAsString(),
/// getUserComponentPresetFilesAsString()): the list joined by ';', which is the default
/// directory while the preference is unset; nothing then without a default directory.
[[nodiscard]] QString filesText(const Preferences& preferences, UserFiles files,
                                const std::optional<std::filesystem::path>& defaultDirectory)
{
    if (!defaultDirectory.has_value() && !preferences.get(preferenceKey(files)).has_value())
    {
        return {};
    }
    // Only an unset preference reads the default directory.
    const std::filesystem::path directory = defaultDirectory.value_or(std::filesystem::path());
    switch (files)
    {
        case UserFiles::THRUST_CURVES:
            return QtStrings::fromUtf8(preferences.getUserThrustCurveFilesAsString(directory));
        case UserFiles::COMPONENT_PRESETS:
            return QtStrings::fromUtf8(preferences.getUserComponentPresetFilesAsString(directory));
    }
    QTROCKET_UNREACHABLE();
}

/// The field's DocumentListener: the list in @p text (split at ';', each part trimmed) stored as
/// the list of @p files.
void storeFiles(Preferences& preferences, UserFiles files, const QString& text)
{
    const std::vector<std::filesystem::path> list =
        Preferences::parseUserFilesText(QtStrings::toUtf8(text));
    switch (files)
    {
        case UserFiles::THRUST_CURVES:
            preferences.setUserThrustCurveFiles(list);
            return;
        case UserFiles::COMPONENT_PRESETS:
            preferences.setUserComponentPresetFiles(list);
            return;
    }
    QTROCKET_UNREACHABLE();
}

/// setUserThrustCurveFiles(null), setUserComponentPresetFiles(null): the preference of @p files
/// removed.
void resetFiles(Preferences& preferences, UserFiles files)
{
    switch (files)
    {
        case UserFiles::THRUST_CURVES:
            preferences.resetUserThrustCurveFiles();
            return;
        case UserFiles::COMPONENT_PRESETS:
            preferences.resetUserComponentPresetFiles();
            return;
    }
    QTROCKET_UNREACHABLE();
}

/// @p file as File.getAbsolutePath() spells it (Java's File normalisation, then made absolute):
/// as the list's text has its paths.
[[nodiscard]] QString absoluteText(const std::filesystem::path& file)
{
    return QtStrings::fromPath(absolutePath(withoutRedundantSeparators(file)));
}

/// The label above the block of @p files.
[[nodiscard]] QString label(UserFiles files)
{
    switch (files)
    {
        case UserFiles::THRUST_CURVES:
            //: pref.dlg.lbl.User-definedthrust
            return GeneralPreferencesPanel::tr("User-defined thrust curves:");
        case UserFiles::COMPONENT_PRESETS:
            //: pref.dlg.lbl.User-definedComponentPreset
            return GeneralPreferencesPanel::tr("User-defined component presets:");
    }
    QTROCKET_UNREACHABLE();
}

/// The file filters of Add for @p files, the first chosen (Java's SimpleFileFilters, with the
/// accept-all filter left out). Qt reads the patterns of a filter from its last parentheses: a
/// text whose own list holds more than patterns ("...; directories") is followed by the patterns.
[[nodiscard]] QStringList nameFilters(UserFiles files)
{
    switch (files)
    {
        case UserFiles::THRUST_CURVES:
            return {//: pref.dlg.Allthrustcurvefiles
                    GeneralPreferencesPanel::tr(
                        "All thrust curve files (*.eng; *.rse; *.zip; *.db; directories)") +
                        QStringLiteral(" (*.eng *.rse *.zip *.db)"),
                    //: pref.dlg.RASPfiles
                    GeneralPreferencesPanel::tr("RASP motor files (*.eng)"),
                    //: pref.dlg.RockSimfiles
                    GeneralPreferencesPanel::tr("RockSim engine files (*.rse)"),
                    //: pref.dlg.ZIParchives
                    GeneralPreferencesPanel::tr("ZIP archives (*.zip)")};
        case UserFiles::COMPONENT_PRESETS:
            return {//: pref.dlg.AllComponentPresetfiles
                    GeneralPreferencesPanel::tr("All component preset files (*.orc; directories)") +
                        QStringLiteral(" (*.orc)"),
                    //: pref.dlg.ORCfiles
                    GeneralPreferencesPanel::tr("OpenRocket component files (*.orc)")};
    }
    QTROCKET_UNREACHABLE();
}

}  // namespace

GeneralPreferencesPanel::GeneralPreferencesPanel(Preferences& preferences, UserPrompts& prompts,
                                                 QWidget* parent)
  : GeneralPreferencesPanel(preferences, prompts, Environment{}, parent)
{
}

GeneralPreferencesPanel::GeneralPreferencesPanel(Preferences& preferences, UserPrompts& prompts,
                                                 Environment environment, QWidget* parent)
  : QWidget(parent),
    m_preferences(&preferences),
    m_prompts(&prompts),
    m_environment(std::move(environment))
{
    setObjectName(QStringLiteral("generalPreferencesPanel"));
    // MigLayout("fillx, ins 30lp n n n"): one item a row, the buttons at the bottom ("pushy,
    // bottom").
    auto*    layout  = makeChild<QVBoxLayout>(this);
    QMargins margins = layout->contentsMargins();
    margins.setTop(kTopInset);
    layout->setContentsMargins(margins);

    addUserFilesBlock(*layout, UserFiles::THRUST_CURVES);
    // DescriptionArea(text, 3, -1.5f, false), on the page's background in its text colour.
    m_thrustCurvesDescription = makeChild<DescriptionArea>(
        this,
        //: pref.dlg.DescriptionArea.Adddirectories
        tr("Add directories, RASP motor files (*.eng), RockSim engine files (*.rse), SQLite motor "
           "databases (*.db) or ZIP archives separated by a semicolon (;) to load external thrust "
           "curves.  Changes will take effect the next time you start QtRocket."),
        kDescriptionRows, kDescriptionSize, false);
    m_thrustCurvesDescription->setObjectName(QStringLiteral("userThrustCurvesDescription"));
    layout->addWidget(m_thrustCurvesDescription);
    addUserFilesBlock(*layout, UserFiles::COMPONENT_PRESETS);

    auto* separator = makeChild<QFrame>(this);
    separator->setObjectName(QStringLiteral("separator"));
    separator->setFrameShape(QFrame::HLine);
    separator->setFrameShadow(QFrame::Sunken);
    layout->addWidget(separator);

    // Java's ActionListener: the user's clicks only.
    //: pref.dlg.but.openlast
    m_openLast = makeChild<QCheckBox>(this, tr("Open last design file on startup"));
    m_openLast->setObjectName(QtStrings::fromUtf8(PreferenceKeys::kAutoOpenLastDesign));
    m_openLast->setChecked(preferences.isAutoOpenLastDesignOnStartupEnabled());
    connect(
        m_openLast, &QCheckBox::clicked, this,
        guarded([this](bool checked) { m_preferences->setAutoOpenLastDesignOnStartup(checked); },
                context()));
    layout->addWidget(m_openLast);

    // Java's ItemListener: every change of the state.
    //: pref.dlg.checkbox.ShowDiscardPreferencesConfirmation
    m_discard =
        makeChild<QCheckBox>(this, tr("Show confirmation dialog when discarding preferences"));
    m_discard->setObjectName(
        QtStrings::fromUtf8(PreferenceKeys::kShowDiscardPreferencesConfirmation));
    m_discard->setChecked(preferences.isShowDiscardPreferencesConfirmation());
    connect(
        m_discard, &QCheckBox::toggled, this,
        guarded(
            [this](bool checked) { m_preferences->setShowDiscardPreferencesConfirmation(checked); },
            context()));
    layout->addWidget(m_discard);

    // Java's ItemListener too (SwingPreferences.isAutoOpenPartsLibrary(),
    // setAutoOpenPartsLibrary()).
    //: pref.dlg.checkbox.AutoOpenPartsLibrary
    m_partsLibrary = makeChild<QCheckBox>(this, tr("Automatically open the Parts Library dialog"));
    m_partsLibrary->setObjectName(QtStrings::fromUtf8(GuiPreferenceKeys::kAutoOpenPartsLibrary));
    //: pref.dlg.checkbox.AutoOpenPartsLibrary.ttip
    m_partsLibrary->setToolTip(
        tr("<html>If checked, the Parts Library dialog will automatically open when you add a new "
           "part,<br>unless you already disabled it in the Parts Library dialog.</html>"));
    m_partsLibrary->setChecked(preferences.getBoolean(
        GuiPreferenceKeys::kAutoOpenPartsLibrary, GuiPreferenceKeys::kAutoOpenPartsLibraryDefault));
    connect(m_partsLibrary, &QCheckBox::toggled, this,
            guarded(
                [this](bool checked) {
                    m_preferences->putBoolean(GuiPreferenceKeys::kAutoOpenPartsLibrary, checked);
                },
                context()));
    layout->addWidget(m_partsLibrary);
    layout->addStretch(1);

    // The buttons' panel (JPanel(new MigLayout("fillx, ins 0"))): the reset button at its right
    // ("pushx, right, gaptop 20lp").
    auto* buttonPanel = makeChild<QWidget>(this);
    buttonPanel->setObjectName(QStringLiteral("buttonPanel"));
    auto* buttons = makeChild<QHBoxLayout>(buttonPanel);
    buttons->setContentsMargins(0, kResetGap, 0, 0);
    buttons->addStretch(1);
    //: pref.dlg.but.resetAllPreferences
    m_reset = makeChild<QPushButton>(buttonPanel, tr("Reset all preferences"));
    m_reset->setObjectName(QStringLiteral("resetAllPreferences"));
    //: pref.dlg.but.resetAllPreferences.ttip
    m_reset->setToolTip(
        tr("Reset all the preferences, including cached preferences (UI "
           "settings, recent files, etc.)"));
    connect(m_reset, &QPushButton::clicked, this,
            guarded([this] { static_cast<void>(resetAllPreferences()); }, context()));
    buttons->addWidget(m_reset);
    layout->addWidget(buttonPanel);
}

GeneralPreferencesPanel::~GeneralPreferencesPanel() = default;

QLineEdit* GeneralPreferencesPanel::userFilesField(UserFiles files) const noexcept
{
    return block(files).field;
}

QPushButton* GeneralPreferencesPanel::addUserFilesButton(UserFiles files) const noexcept
{
    return block(files).add;
}

QPushButton* GeneralPreferencesPanel::resetUserFilesButton(UserFiles files) const noexcept
{
    return block(files).reset;
}

bool GeneralPreferencesPanel::resetAllPreferences()
{
    // The question and the message run event loops, in which the page may go with its dialog's
    // window: nothing of it is used once it is gone.
    const QPointer<GeneralPreferencesPanel> self(this);
    // JOptionPane.showConfirmDialog(parent, ..., YES_NO_OPTION, WARNING_MESSAGE), the parent the
    // dialog.
    const bool reset = m_prompts->askYesNo(
        window(), {.severity = UserPrompts::Severity::WARNING,
                   //: pref.dlg.clearCachedPreferences.title
                   .title = tr("Reset preferences?"),
                   //: pref.dlg.clearCachedPreferences.message
                   .text = tr("Are you sure you want to reset all your preferences?")});
    if (!reset || self.isNull())
    {
        return false;
    }
    // O-M3-7: Java's configurations keep the name they were made with when the default name goes
    // back to OpenRocket's; QtRocket's read the default when asked, so their names are pinned
    // first.
    if (m_environment.openDocuments)
    {
        for (OpenRocketDocument* document : m_environment.openDocuments())
        {
            document->pinFlightConfigurationNames(*m_preferences);
        }
    }
    PreferencesStore::clearPreferences(*m_preferences);
    // OpenRocket's texts with QtRocket's name.
    m_prompts->showMessage(
        window(),
        {.severity = UserPrompts::Severity::WARNING,
         //: generalprefs.ImportWarning.title
         .title = tr("Reload QtRocket"),
         //: generalprefs.ImportWarning.msg
         .lines = {
             tr("You may need to restart QtRocket for some of the changes to take effect.")}});
    if (self.isNull())
    {
        // The preferences are reset; there is no dialog left to show them anew.
        return true;
    }
    Q_EMIT preferencesCleared();
    return true;
}

void GeneralPreferencesPanel::addUserFilesBlock(QVBoxLayout& layout, UserFiles files)
{
    UserFilesBlock& widgets = block(files);
    layout.addWidget(makeChild<QLabel>(this, label(files)));

    // "w 100px, gapright unrel, spanx, growx, split", then Add ("gapright unrel") and Reset: one
    // row.
    auto* row = makeChild<QWidget>(this);
    row->setObjectName(QtStrings::fromUtf8(preferenceKey(files)) + QStringLiteral("Row"));
    auto* rowLayout = makeChild<QHBoxLayout>(row);
    rowLayout->setContentsMargins(0, 0, 0, 0);
    widgets.field = makeChild<QLineEdit>(row);
    widgets.field->setObjectName(QtStrings::fromUtf8(preferenceKey(files)));
    widgets.field->setMinimumWidth(kFieldMinimumWidth);
    // Set before the DocumentListener is added: showing the list stores nothing.
    widgets.field->setText(filesText(*m_preferences, files, defaultDirectory(files)));
    // The DocumentListener: every change, typed or set (Add, Reset), stores the list.
    connect(widgets.field, &QLineEdit::textChanged, this,
            guarded([this, files](const QString& text) { storeFiles(*m_preferences, files, text); },
                    context()));
    rowLayout->addWidget(widgets.field, 1);

    //: pref.dlg.but.add
    widgets.add = makeChild<QPushButton>(row, tr("Add"));
    widgets.add->setObjectName(QStringLiteral("add") + widgets.field->objectName());
    connect(widgets.add, &QPushButton::clicked, this,
            guarded([this, files] { addUserFile(files); }, context()));
    rowLayout->addWidget(widgets.add);

    //: pref.dlg.but.reset
    widgets.reset = makeChild<QPushButton>(row, tr("Reset"));
    widgets.reset->setObjectName(QStringLiteral("reset") + widgets.field->objectName());
    connect(widgets.reset, &QPushButton::clicked, this,
            guarded([this, files] { resetUserFiles(files); }, context()));
    rowLayout->addWidget(widgets.reset);
    layout.addWidget(row);
}

void GeneralPreferencesPanel::addUserFile(UserFiles files)
{
    UserPrompts::OpenFileRequest request;
    // JFileChooser.showDialog(panel, "Add"): a dialog of its own kind, titled with the text of
    // its accepting button (the probe), for a file or a directory, starting in the default
    // directory (the user's home directory without one).
    //: pref.dlg.Add
    request.title              = tr("Add");
    request.nameFilters        = nameFilters(files);
    request.selectedNameFilter = request.nameFilters.front();
    if (const std::optional<std::filesystem::path> directory = m_preferences->getDefaultDirectory())
    {
        request.directory = *directory;
    }
    request.selection = UserPrompts::OpenFileSelection::FILES_AND_DIRECTORIES;
    //: pref.dlg.Add
    request.approveButtonText = tr("Add");
    // The question runs an event loop, in which the page may go with its dialog's window. Its
    // parent is the Add button, below which the menu "File..." / "Directory..." opens.
    const QPointer<GeneralPreferencesPanel>          self(this);
    const std::optional<UserPrompts::OpenFileAnswer> answer =
        m_prompts->askOpenFiles(block(files).add, request);
    if (self.isNull() || !answer.has_value() || answer->files.empty())
    {
        return;
    }
    QLineEdit&        field   = *block(files).field;
    const std::string current = QtStrings::toUtf8(field.text());
    // getText().trim(), a ';' after a text that is not empty, then the absolute path.
    QString text = QtStrings::fromUtf8(Strings::trim(current));
    if (!text.isEmpty())
    {
        text += QChar(u';');
    }
    text += absoluteText(answer->files.front());
    // The DocumentListener stores the list.
    field.setText(text);
    m_preferences->setDefaultDirectory(answer->directory);
}

void GeneralPreferencesPanel::resetUserFiles(UserFiles files)
{
    // "First one sets to the default, but does not un-set the pref": the field stores the
    // default directory, then the preference is removed.
    const std::optional<std::filesystem::path>& directory = defaultDirectory(files);
    block(files).field->setText(directory.has_value() ? absoluteText(*directory) : QString());
    resetFiles(*m_preferences, files);
}

GeneralPreferencesPanel::UserFilesBlock& GeneralPreferencesPanel::block(UserFiles files) noexcept
{
    return files == UserFiles::THRUST_CURVES ? m_thrustCurveFiles : m_componentPresetFiles;
}

const GeneralPreferencesPanel::UserFilesBlock& GeneralPreferencesPanel::block(
    UserFiles files) const noexcept
{
    return files == UserFiles::THRUST_CURVES ? m_thrustCurveFiles : m_componentPresetFiles;
}

const std::optional<std::filesystem::path>& GeneralPreferencesPanel::defaultDirectory(
    UserFiles files) const noexcept
{
    return files == UserFiles::THRUST_CURVES ? m_environment.defaultUserThrustCurveDirectory
                                             : m_environment.defaultUserComponentDirectory;
}

}  // namespace QtRocket::Gui
