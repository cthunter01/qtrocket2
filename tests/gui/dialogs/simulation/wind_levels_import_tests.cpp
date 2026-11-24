// The wind editor's "Import levels" (OpenRocket's MultiLevelWindEditDialog.importLevels(), Java
// lines 220-306), with the questions answered by ScriptedUserPrompts and CSV files in a temporary
// directory: the settings asked first (starting with the stored ones), stored, then the file
// ("Import wind levels", the CSV filter chosen), the default directory stored, the overwrite
// question while there are levels, the import, and the table's units those of the settings; a
// failed import resets the levels and says why. OpenRocket bug 34 (SPACE and TAB separated files,
// the separator stored itself) and bug 37 (a file not ending in ".csv" is imported as chosen) are
// fixed; the editor or the model may go during a question.

#include <cstddef>
#include <filesystem>
#include <functional>
#include <memory>
#include <numbers>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <QPushButton>
#include <QRadioButton>
#include <QString>
#include <QStringList>
#include <QWidget>
#include <QtContainerFwd>

#include <gtest/gtest.h>

#include "ForwardingUserPrompts.h"
#include "GuiTestSupport.h"
#include "QtRocket/gui/dialogs/simulation/CsvImportSettingsDialog.h"
#include "QtRocket/gui/dialogs/simulation/MultiLevelWindEditDialog.h"
#include "QtRocket/gui/dialogs/simulation/MultiLevelWindTableModel.h"
#include "QtRocket/gui/dialogs/simulation/MultiLevelWindTableView.h"
#include "QtRocket/gui/plot/WindProfilePlot.h"
#include "QtRocket/gui/util/FileNames.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/util/UserPrompts.h"
#include "QtRocket/gui/widgets/UnitSelector.h"
#include "QtRocket/models/MultiLevelPinkNoiseWindModel.h"
#include "QtRocket/models/WindModel.h"
#include "QtRocket/preferences/PreferenceKeys.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/unit/Unit.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/Signal.h"
#include "ScriptedUserPrompts.h"
#include "TestGuiServices.h"
#include "TestTempDir.h"

namespace QtRocket::Gui
{

namespace
{

using WindLevelsImportTest = Gui::Test::GuiTest;
using Kind                 = Gui::Test::ScriptedUserPrompts::Kind;
using Settings             = UserPrompts::WindCsvImportSettings;
using Model                = MultiLevelWindTableModel;

/// Degrees in radians.
constexpr double kDegree = std::numbers::pi / 180;

/// A file with a header row, three levels out of order, comma separated, in m, m/s and degrees.
constexpr std::string_view kCommaFile =
    "altitude,speed,direction,stddev\n"
    "0,2,90,0.5\n"
    "1000,4,180,1\n"
    "500,3,135,0.8\n";

/// A level's values: altitude, speed, direction, standard deviation (SI units).
struct LevelValues
{
    double altitude;
    double speed;
    double direction;
    double deviation;
};

/// The editor of three levels (0, 500 and 1250.5 m) with the tests' services (a fresh preference
/// store, scripted questions).
class Editor
{
public:
    Editor()
    {
        m_wind.clearLevels();
        EXPECT_TRUE(m_wind.addWindLevel(0, 3, std::numbers::pi / 2, 0.3).has_value());
        EXPECT_TRUE(m_wind.addWindLevel(500, 7.5, std::numbers::pi, 1.0).has_value());
        EXPECT_TRUE(m_wind.addWindLevel(1250.5, 12.25, 0.0, 3.0).has_value());
        m_dialog = std::make_unique<MultiLevelWindEditDialog>(m_wind, m_services.services());
    }

    [[nodiscard]] MultiLevelWindEditDialog&     dialog() const noexcept { return *m_dialog; }
    [[nodiscard]] MultiLevelPinkNoiseWindModel& wind() noexcept { return m_wind; }
    [[nodiscard]] Preferences& preferences() noexcept { return m_services.preferences(); }
    [[nodiscard]] Gui::Test::ScriptedUserPrompts& prompts() noexcept
    {
        return m_services.prompts();
    }
    [[nodiscard]] const QtRocket::Test::TempDir& directory() const noexcept { return m_directory; }

    /// The settings stored now.
    [[nodiscard]] Settings stored()
    {
        return CsvImportSettingsDialog::storedSettings(m_services.preferences());
    }

    /// Scripts the questions of an import of @p file with @p settings: the settings, the file
    /// (chosen in the temporary directory) and, when @p overwrite says so, the overwrite question.
    void script(const Settings& settings, const std::filesystem::path& file,
                std::optional<bool> overwrite = true)
    {
        prompts().answerWindCsvImportSettings(settings);
        prompts().answerOpenFiles(
            UserPrompts::OpenFileAnswer{.files = {file}, .directory = m_directory.path()});
        if (overwrite.has_value())
        {
            prompts().answerYesNo(*overwrite);
        }
    }

    /// The kinds of the questions and messages so far.
    [[nodiscard]] std::vector<Kind> kinds() const
    {
        std::vector<Kind> kinds;
        for (const Gui::Test::ScriptedUserPrompts::Prompt& prompt : m_services.prompts().prompts())
        {
            kinds.push_back(prompt.kind);
        }
        return kinds;
    }

    /// The model's levels' values in their order.
    [[nodiscard]] std::vector<LevelValues> levels() const
    {
        std::vector<LevelValues> values;
        for (const MultiLevelPinkNoiseWindModel::LevelWindModel* const level :
             std::as_const(m_wind).getLevels())
        {
            values.push_back({.altitude  = level->getAltitude(),
                              .speed     = level->getSpeed(),
                              .direction = level->getDirection(),
                              .deviation = level->getStandardDeviation()});
        }
        return values;
    }

    /// The table's unit of @p column.
    [[nodiscard]] std::string columnUnit(int column) const
    {
        return m_dialog->tableModel().columnUnit(column).getUnit();
    }

private:
    Gui::Test::TestGuiServices                m_services;
    QtRocket::Test::TempDir                   m_directory;
    MultiLevelPinkNoiseWindModel              m_wind{m_services.preferences()};
    std::unique_ptr<MultiLevelWindEditDialog> m_dialog;
};

/// Expects @p actual to be @p expected (level @p index), the direction within rounding.
void expectLevel(const LevelValues& actual, const LevelValues& expected, std::size_t index)
{
    EXPECT_DOUBLE_EQ(actual.altitude, expected.altitude) << "level " << index;
    EXPECT_DOUBLE_EQ(actual.speed, expected.speed) << "level " << index;
    EXPECT_NEAR(actual.direction, expected.direction, 1e-12) << "level " << index;
    EXPECT_DOUBLE_EQ(actual.deviation, expected.deviation) << "level " << index;
}

/// Expects @p actual to be @p expected, level by level (expectLevel()).
void expectLevels(const std::vector<LevelValues>& actual, const std::vector<LevelValues>& expected)
{
    ASSERT_EQ(actual.size(), expected.size());
    for (std::size_t index = 0; index < actual.size(); ++index)
    {
        expectLevel(actual[index], expected[index], index);
    }
}

/// The unit of @p group named @p name (a test failure when there is none).
[[nodiscard]] const Unit* unitNamed(UnitGroupId group, std::string_view name)
{
    const Unit* const unit = unitGroup(group).getUnit(name);
    EXPECT_NE(unit, nullptr) << name;
    return unit;
}

/// The levels of kCommaFile.
[[nodiscard]] std::vector<LevelValues> commaFileLevels()
{
    return {{.altitude = 0, .speed = 2, .direction = 90 * kDegree, .deviation = 0.5},
            {.altitude = 500, .speed = 3, .direction = 135 * kDegree, .deviation = 0.8},
            {.altitude = 1000, .speed = 4, .direction = 180 * kDegree, .deviation = 1}};
}

/// A double that runs @p hook in one question (the settings or the file) before it passes the
/// question on: the event loop of a modal dialog may run what closes the editor or removes the
/// simulation.
class HookInQuestion final : public Gui::Test::ForwardingUserPrompts
{
public:
    enum class Question
    {
        SETTINGS,
        FILE,
    };

    HookInQuestion(UserPrompts& delegate, Question question, std::function<void()> hook)
      : ForwardingUserPrompts(delegate), m_question(question), m_hook(std::move(hook))
    {
    }

    [[nodiscard]] std::optional<WindCsvImportSettings> askWindCsvImportSettings(
        QWidget* parent, const WindCsvImportSettings& initial) override
    {
        if (m_question == Question::SETTINGS)
        {
            m_hook();
        }
        return delegate().askWindCsvImportSettings(parent, initial);
    }

    [[nodiscard]] std::optional<OpenFileAnswer> askOpenFiles(
        QWidget* parent, const OpenFileRequest& request) override
    {
        if (m_question == Question::FILE)
        {
            m_hook();
        }
        return delegate().askOpenFiles(parent, request);
    }

private:
    Question              m_question;
    std::function<void()> m_hook;
};

}  // namespace

TEST_F(WindLevelsImportTest, TheImportReplacesTheLevelsWithTheFilesLevels)
{
    Editor                      editor;
    const std::filesystem::path file    = editor.directory().write("wind.csv", kCommaFile);
    const Settings              initial = editor.stored();
    editor.script(initial, file);
    editor.dialog().importButton().click();

    // The settings (starting with the stored ones), the file, the overwrite question.
    EXPECT_EQ(editor.kinds(),
              (std::vector<Kind>{Kind::WIND_CSV_IMPORT_SETTINGS, Kind::OPEN_FILES, Kind::YES_NO}));
    ASSERT_EQ(editor.prompts().windCsvImportRequests().size(), 1U);
    EXPECT_EQ(editor.prompts().windCsvImportRequests().front(), initial);
    ASSERT_EQ(editor.prompts().openFileRequests().size(), 1U);
    const UserPrompts::OpenFileRequest& request = editor.prompts().openFileRequests().front();
    EXPECT_EQ(request.title, QStringLiteral("Import wind levels"));
    EXPECT_EQ(request.nameFilters,
              (QStringList{FileNames::csvFilter(), FileNames::allFilesFilter()}));
    EXPECT_TRUE(request.selectedNameFilter.isEmpty());
    EXPECT_FALSE(request.multipleFiles);
    EXPECT_EQ(request.selection, UserPrompts::OpenFileSelection::FILES);
    // No default directory is stored yet: the dialog's own.
    EXPECT_TRUE(request.directory.empty());
    const Gui::Test::ScriptedUserPrompts::Prompt& overwrite = editor.prompts().prompts().back();
    EXPECT_EQ(overwrite.title, QStringLiteral("Overwrite wind levels?"));

    // The file's levels, sorted; the dialog's directory is the default one now.
    expectLevels(editor.levels(), commaFileLevels());
    EXPECT_EQ(editor.dialog().tableModel().rowCount(), 3);
    EXPECT_EQ(editor.preferences().getDefaultDirectory(),
              std::optional<std::filesystem::path>(editor.directory().path()));
}

TEST_F(WindLevelsImportTest, TheModelChangesOnce)
{
    // The levels are read into a model of their own and copied at once (Java changes the model
    // once per level, which the table, the profile and the launch conditions follow: minutes for
    // 2000 levels); the model's listeners hear of the import once.
    Editor      editor;
    std::string text = "altitude,speed,direction\n";
    for (int level = 0; level < 200; ++level)
    {
        text += std::to_string(level * 10) + ",5,90\n";
    }
    const std::filesystem::path file = editor.directory().write("many.csv", text);
    editor.script(editor.stored(), file);
    int                              changes = 0;
    const Signal<>::ScopedConnection counting(
        editor.wind().changed().connect([&changes] { ++changes; }));
    editor.dialog().importLevels();
    EXPECT_EQ(changes, 1);
    EXPECT_EQ(editor.wind().getLevels().size(), 200U);
    EXPECT_EQ(editor.dialog().tableModel().rowCount(), 200);
    EXPECT_EQ(editor.wind().getAltitudeReference(), WindModel::AltitudeReference::MSL);
}

TEST_F(WindLevelsImportTest, TheAltitudeReferenceStays)
{
    // Java imports into the model itself, which keeps its reference.
    Editor editor;
    editor.wind().setAltitudeReference(WindModel::AltitudeReference::AGL);
    const std::filesystem::path file = editor.directory().write("wind.csv", kCommaFile);
    editor.script(editor.stored(), file);
    editor.dialog().importLevels();
    EXPECT_EQ(editor.wind().getAltitudeReference(), WindModel::AltitudeReference::AGL);
    EXPECT_TRUE(editor.dialog().aglButton().isChecked());
    expectLevels(editor.levels(), commaFileLevels());
}

TEST_F(WindLevelsImportTest, TheSettingsAreStoredAndTheTableTakesTheirUnits)
{
    Editor editor;
    editor.preferences().setDefaultDirectory(editor.directory().path());
    const std::filesystem::path file = editor.directory().write(
        "wind.csv", "# feet, knots and radians\nAlt;Wind;From;Gust\n1000;10;2.5;2\n0;5;0;1\n");
    Settings settings = editor.stored();
    settings.altitude = {
        .name = QStringLiteral("Alt"), .index = 5, .unit = unitNamed(UnitGroupId::DISTANCE, "ft")};
    settings.speed     = {.name  = QStringLiteral("Wind"),
                          .index = 6,
                          .unit  = unitNamed(UnitGroupId::WINDSPEED, "kt")};
    settings.direction = {
        .name = QStringLiteral("From"), .index = 7, .unit = unitNamed(UnitGroupId::ANGLE, "rad")};
    settings.standardDeviation       = {.name  = QStringLiteral("Gust"),
                                        .index = 8,
                                        .unit  = unitNamed(UnitGroupId::WINDSPEED, "mph")};
    settings.separator               = QStringLiteral(";");
    const std::string turbulenceUnit = editor.columnUnit(Model::kTurbulenceColumn);
    editor.script(settings, file);
    editor.dialog().importLevels();

    // Java: the 12 MultiLevelWindCSVImport... preferences and the export's separator.
    EXPECT_EQ(editor.stored(), settings);
    EXPECT_EQ(editor.prompts().openFileRequests().front().directory, editor.directory().path());
    const double knot = unitNamed(UnitGroupId::WINDSPEED, "kt")->fromUnit(1);
    const double mph  = unitNamed(UnitGroupId::WINDSPEED, "mph")->fromUnit(1);
    expectLevels(editor.levels(),
                 {{.altitude = 0, .speed = 5 * knot, .direction = 0, .deviation = mph},
                  {.altitude  = unitNamed(UnitGroupId::DISTANCE, "ft")->fromUnit(1000),
                   .speed     = 10 * knot,
                   .direction = 2.5,
                   .deviation = 2 * mph}});
    // Java: windTable.setAltitudeUnit(...), ...: the table (and the profile) show the file's units.
    EXPECT_EQ(editor.columnUnit(Model::kAltitudeColumn), "ft");
    EXPECT_EQ(editor.columnUnit(Model::kSpeedColumn), "kt");
    EXPECT_EQ(editor.columnUnit(Model::kDirectionColumn), "rad");
    EXPECT_EQ(editor.columnUnit(Model::kDeviationColumn), "mph");
    EXPECT_EQ(
        editor.dialog().table().unitSelector(Model::kAltitudeColumn).getSelectedUnit().getUnit(),
        "ft");
    EXPECT_EQ(editor.dialog().visualization().units().speed->getUnit(), "kt");
    // The turbulence column keeps its unit.
    EXPECT_EQ(editor.columnUnit(Model::kTurbulenceColumn), turbulenceUnit);
}

TEST_F(WindLevelsImportTest, SpaceAndTabSeparatedFilesAreImported)
{
    // OpenRocket bug 34: Java splits at the separator box's text "SPACE" or "TAB" (and fails:
    // 'altitude column "altitude" not found in header row.') and stores it as the export's
    // separator; the separator itself is used and stored.
    for (const auto& [separator, text] :
         {std::pair{QStringLiteral(" "),
                    std::string_view("altitude speed direction stddev\n"
                                     "0 2 90 0.5\n1000 4 180 1\n500 3 135 0.8\n")},
          std::pair{QStringLiteral("\t"),
                    std::string_view("altitude\tspeed\tdirection\tstddev\n"
                                     "0\t2\t90\t0.5\n1000\t4\t180\t1\n500\t3\t135\t0.8\n")}})
    {
        Editor                      editor;
        const std::filesystem::path file     = editor.directory().write("wind.csv", text);
        Settings                    settings = editor.stored();
        settings.separator                   = separator;
        editor.script(settings, file);
        editor.dialog().importLevels();
        expectLevels(editor.levels(), commaFileLevels());
        EXPECT_EQ(editor.preferences().getString(PreferenceKeys::kExportFieldSeparator, ""),
                  QtStrings::toUtf8(separator));
        // The next import starts with that separator.
        EXPECT_EQ(editor.stored().separator, separator);
    }
}

TEST_F(WindLevelsImportTest, AFileNotEndingInCsvIsImportedAsChosen)
{
    // OpenRocket bug 37: Java forces ".csv" onto the file chosen ("Could not load the file.
    // 'wind.txt.csv'"); the file chosen through "All Files" is imported.
    Editor                      editor;
    const std::filesystem::path file = editor.directory().write("wind.txt", kCommaFile);
    editor.script(editor.stored(), file);
    editor.dialog().importLevels();
    expectLevels(editor.levels(), commaFileLevels());
    EXPECT_EQ(editor.kinds(),
              (std::vector<Kind>{Kind::WIND_CSV_IMPORT_SETTINGS, Kind::OPEN_FILES, Kind::YES_NO}));
}

TEST_F(WindLevelsImportTest, AFileWithoutAHeaderRowIsReadByIndex)
{
    // Without a header row the columns are indices; the standard deviation's is always given
    // (Java's getStdDeviationColumn()), also without a name.
    Editor   editor;
    Settings settings                = editor.stored();
    settings.hasHeader               = false;
    settings.altitude.index          = 2;
    settings.speed.index             = 0;
    settings.direction.index         = 1;
    settings.standardDeviation.name  = QString();
    settings.standardDeviation.index = 3;
    const std::filesystem::path file =
        editor.directory().write("wind.csv", "2,90,0,0.5\n4,180,1000,1\n3,135,500,0.8\n");
    editor.script(settings, file);
    editor.dialog().importLevels();
    expectLevels(editor.levels(), commaFileLevels());

    // A file of three columns then has too few (Java: "Line 1 does not have enough columns.").
    const std::filesystem::path three =
        editor.directory().write("three.csv", "2,90,0\n4,180,1000\n");
    editor.script(settings, three);
    editor.dialog().importLevels();
    const Gui::Test::ScriptedUserPrompts::Prompt& message = editor.prompts().prompts().back();
    EXPECT_EQ(message.kind, Kind::MESSAGE);
    EXPECT_EQ(message.texts,
              (QStringList{QStringLiteral("ERROR"),
                           QStringLiteral("An issue occurred while importing wind levels:"),
                           QStringLiteral("Line 1 does not have enough columns.")}));
}

TEST_F(WindLevelsImportTest, AFailedImportResetsTheLevelsAndSaysWhy)
{
    Editor                      editor;
    const std::filesystem::path file =
        editor.directory().write("wind.csv", "altitude,speed,dir\n0,2,90\n");
    editor.script(editor.stored(), file);
    editor.dialog().importLevels();

    // Java: resetLevels(), then "Import Wind Levels Failed".
    // The one level of the preferences' average wind (a new model's).
    const MultiLevelPinkNoiseWindModel                  reset(editor.preferences());
    const MultiLevelPinkNoiseWindModel::LevelWindModel& level = *reset.getLevels().front();
    expectLevels(editor.levels(), {{.altitude  = 0,
                                    .speed     = level.getSpeed(),
                                    .direction = level.getDirection(),
                                    .deviation = level.getStandardDeviation()}});
    EXPECT_EQ(editor.kinds(), (std::vector<Kind>{Kind::WIND_CSV_IMPORT_SETTINGS, Kind::OPEN_FILES,
                                                 Kind::YES_NO, Kind::MESSAGE}));
    const Gui::Test::ScriptedUserPrompts::Prompt& message = editor.prompts().prompts().back();
    EXPECT_EQ(message.title, QStringLiteral("Import Wind Levels Failed"));
    EXPECT_EQ(
        message.texts,
        (QStringList{QStringLiteral("ERROR"),
                     QStringLiteral("An issue occurred while importing wind levels:"),
                     QStringLiteral("direction column \"direction\" not found in header row.")}));
    // The table keeps its units.
    EXPECT_EQ(editor.columnUnit(Model::kAltitudeColumn), "m");

    // A file that is not there.
    editor.script(editor.stored(), editor.directory().resolve("missing.csv"));
    editor.dialog().importLevels();
    EXPECT_EQ(editor.prompts().prompts().back().texts,
              (QStringList{QStringLiteral("ERROR"),
                           QStringLiteral("An issue occurred while importing wind levels:"),
                           QStringLiteral("Could not load the file. 'missing.csv'")}));
    EXPECT_EQ(editor.wind().getLevels().size(), 1U);
}

TEST_F(WindLevelsImportTest, CancellingTheSettingsChangesNothing)
{
    Editor editor;
    editor.prompts().answerWindCsvImportSettings(std::nullopt);
    editor.dialog().importLevels();
    EXPECT_EQ(editor.kinds(), std::vector<Kind>{Kind::WIND_CSV_IMPORT_SETTINGS});
    EXPECT_EQ(editor.levels().size(), 3U);
    EXPECT_EQ(editor.preferences().getString(PreferenceKeys::kExportFieldSeparator, "none"),
              "none");
}

TEST_F(WindLevelsImportTest, CancellingTheFileKeepsTheLevelsButTheSettingsAreStored)
{
    // Java stores the settings before it asks for the file.
    Editor   editor;
    Settings settings   = editor.stored();
    settings.speed.name = QStringLiteral("wind");
    editor.prompts().answerWindCsvImportSettings(settings);
    editor.prompts().answerOpenFiles(std::nullopt);
    editor.dialog().importLevels();
    EXPECT_EQ(editor.kinds(),
              (std::vector<Kind>{Kind::WIND_CSV_IMPORT_SETTINGS, Kind::OPEN_FILES}));
    EXPECT_EQ(editor.levels().size(), 3U);
    EXPECT_EQ(editor.stored(), settings);
    EXPECT_FALSE(editor.preferences().getDefaultDirectory().has_value());
}

TEST_F(WindLevelsImportTest, NoToTheOverwriteQuestionKeepsTheLevels)
{
    Editor                      editor;
    const std::filesystem::path file = editor.directory().write("wind.csv", kCommaFile);
    editor.script(editor.stored(), file, false);
    editor.dialog().importLevels();
    EXPECT_EQ(editor.levels().size(), 3U);
    EXPECT_DOUBLE_EQ(editor.levels().back().altitude, 1250.5);
    // Java stores the directory before it asks.
    EXPECT_EQ(editor.preferences().getDefaultDirectory(),
              std::optional<std::filesystem::path>(editor.directory().path()));
}

TEST_F(WindLevelsImportTest, AModelWithoutLevelsIsNotAskedAbout)
{
    // Java: the question only when model.getLevels() is not empty.
    Editor editor;
    editor.wind().clearLevels();
    const std::filesystem::path file = editor.directory().write("wind.csv", kCommaFile);
    editor.script(editor.stored(), file, std::nullopt);
    editor.dialog().importLevels();
    EXPECT_EQ(editor.kinds(),
              (std::vector<Kind>{Kind::WIND_CSV_IMPORT_SETTINGS, Kind::OPEN_FILES}));
    expectLevels(editor.levels(), commaFileLevels());
}

TEST_F(WindLevelsImportTest, TheModelMayGoDuringTheFileQuestion)
{
    // Under ASan: the simulation's wind goes while the file is asked for; nothing more is asked
    // or imported.
    Gui::Test::TestGuiServices    services;
    const QtRocket::Test::TempDir directory;
    auto           wind = std::make_unique<MultiLevelPinkNoiseWindModel>(services.preferences());
    HookInQuestion prompts(services.prompts(), HookInQuestion::Question::FILE,
                           [&wind] { wind.reset(); });
    MultiLevelWindEditDialog dialog(*wind, services.services().withPrompts(prompts));
    services.prompts().answerWindCsvImportSettings(
        CsvImportSettingsDialog::storedSettings(services.preferences()));
    services.prompts().answerOpenFiles(UserPrompts::OpenFileAnswer{
        .files = {directory.write("wind.csv", kCommaFile)}, .directory = directory.path()});
    dialog.importLevels();
    EXPECT_TRUE(wind == nullptr);
    EXPECT_EQ(services.prompts().prompts().size(), 2U);
    EXPECT_FALSE(dialog.tableModel().hasModel());
}

TEST_F(WindLevelsImportTest, TheEditorMayGoDuringTheSettingsQuestion)
{
    // Under ASan: the editor goes while the settings are asked (its window closed in the
    // question's event loop); nothing is stored or asked after.
    Gui::Test::TestGuiServices                services;
    MultiLevelPinkNoiseWindModel              wind(services.preferences());
    std::unique_ptr<MultiLevelWindEditDialog> dialog;
    HookInQuestion prompts(services.prompts(), HookInQuestion::Question::SETTINGS,
                           [&dialog] { dialog.reset(); });
    dialog =
        std::make_unique<MultiLevelWindEditDialog>(wind, services.services().withPrompts(prompts));
    services.prompts().answerWindCsvImportSettings(
        CsvImportSettingsDialog::storedSettings(services.preferences()));
    MultiLevelWindEditDialog* const editor = dialog.get();
    editor->importLevels();
    EXPECT_TRUE(dialog == nullptr);
    EXPECT_EQ(services.prompts().prompts().size(), 1U);
    EXPECT_EQ(services.preferences().getString(PreferenceKeys::kExportFieldSeparator, "none"),
              "none");
    EXPECT_EQ(wind.getLevels().size(), 1U);
}

}  // namespace QtRocket::Gui
