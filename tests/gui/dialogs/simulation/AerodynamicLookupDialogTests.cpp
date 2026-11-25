// The aerodynamic lookup dialog (OpenRocket's AerodynamicLookupDialog): the ports of the dialog
// halves of OpenRocket's AerodynamicLookupPreservationTest and AerodynamicLookupTruncationTest
// (their core halves are tests/core/file/openrocket/lookup_round_trip_tests.cpp's), and what the
// probe tools/openrocket-probes/simulation-options shows and does (SimulationOptionsPins.h): the
// dialog's walk, its sections' state after every step a user takes (separators chosen, texts
// edited, Clear, Refresh, OK, Load from file answered by the scripted prompts) with the options'
// tables and the messages shown, in the probe's order and form. Where QtRocket differs on purpose,
// the pinned line is edited here, with the reason: the file question's filters, Escape, the
// stability columns' order.

#include "QtRocket/gui/dialogs/simulation/AerodynamicLookupDialog.h"

#include <cstddef>
#include <filesystem>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include <QColor>
#include <QDialog>
#include <QGroupBox>
#include <QLabel>
#include <QPalette>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QString>
#include <QStringList>
#include <QTabWidget>
#include <QTextCursor>
#include <QWidget>
#include <Qt>
#include <QtContainerFwd>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/aero/lookup/CsvMachAoALookup.h"
#include "QtRocket/aero/lookup/MachAoALookup.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/OpenRocketDocumentFactory.h"
#include "QtRocket/document/StorageOptions.h"
#include "QtRocket/file/GeneralRocketLoader.h"
#include "QtRocket/file/LoadedDocument.h"
#include "QtRocket/file/openrocket/OpenRocketSaver.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/util/UserPrompts.h"
#include "QtRocket/gui/widgets/FieldSeparatorComboBox.h"
#include "QtRocket/logging/ErrorSet.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/simulation/PlotAppearance.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/simulation/SimulationOptions.h"
#include "QtRocket/simulation/extension/SimulationExtension.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"
#include "QtRocket/util/Strings.h"
#include "ScriptedUserPrompts.h"
#include "TestGuiServices.h"
#include "TestTempDir.h"
#include "dialogs/simulation/SimulationDialogWalk.h"
#include "dialogs/simulation/SimulationOptionsPins.h"
#include "file/RocketLoaderTestSupport.h"

namespace QtRocket::Gui
{

namespace
{

using AerodynamicLookupDialogTest = Test::GuiTest;
using Table                       = AerodynamicLookupDialog::Table;
using Lines                       = std::vector<std::string>;
namespace Pins                    = Test::SimulationOptionsPins;

// ------------------------------------------------------------------------- the JUnit tests

/// The JUnit tests' CSV file, which does not exist.
constexpr std::string_view kMissingCsv = "missing-beta-test-lookup.csv";

/// The table of the CSV @p rows with the value columns @p columns (Java:
/// CsvMachAoALookup.parse(rows, columns, ',')), which must parse.
[[nodiscard]] std::shared_ptr<const MachAoALookup> tableOf(const Lines& rows, const Lines& columns)
{
    Result<MachAoALookup> table = CsvMachAoALookup::parse(rows, columns, ',');
    EXPECT_TRUE(table.has_value());
    return table.has_value() ? std::make_shared<const MachAoALookup>(std::move(*table)) : nullptr;
}

/// AerodynamicLookupPreservationTest.options(): both tables from rows that are kept, of the
/// file kMissingCsv.
[[nodiscard]] std::unique_ptr<SimulationOptions> preservationOptions()
{
    auto        options = std::make_unique<SimulationOptions>();
    const Lines drag{"Mach,Cd", "0,1"};
    const Lines stability{"Mach,Cn,Cm,Cp", "0,1,1,1"};
    options->setDragLookup(pathFromUtf8(kMissingCsv), tableOf(drag, {"cd"}), drag);
    options->setStabilityLookup(pathFromUtf8(kMissingCsv), tableOf(stability, {"cn", "cm", "cp"}),
                                stability);
    return options;
}

/// The options' drag Cd and stability Cp at Mach 0 and angle of attack 0 (NaN for a missing
/// table).
struct LookupValues
{
    double cd{std::numeric_limits<double>::quiet_NaN()};
    double cp{std::numeric_limits<double>::quiet_NaN()};

    bool operator==(const LookupValues&) const = default;
};

[[nodiscard]] LookupValues valuesOf(const SimulationOptions& options)
{
    LookupValues values;
    if (const std::shared_ptr<const MachAoALookup> drag = options.getDragLookupTable())
    {
        values.cd = drag->interpolate(0, 0, "cd");
    }
    if (const std::shared_ptr<const MachAoALookup> stability = options.getStabilityLookupTable())
    {
        values.cp = stability->interpolate(0, 0, "cp");
    }
    return values;
}

/// The JUnit tests' roundTrip(): a simulation "Lookup test" of a new empty design with copies
/// of @p options, saved by OpenRocketSaver into a text and loaded back without a directory, in
/// an environment of its own (Java: a new DocumentLoadingContext whose motor finder finds none).
class LookupRoundTrip
{
public:
    explicit LookupRoundTrip(const SimulationOptions& options)
    {
        const std::unique_ptr<OpenRocketDocument> document =
            OpenRocketDocumentFactory::createEmptyRocket();
        document->addSimulation(std::make_shared<Simulation>(
            document.get(), document->getRocket(), Simulation::Status::NOT_SIMULATED, "Lookup test",
            SimulationOptions(options), std::vector<std::shared_ptr<SimulationExtension>>{},
            nullptr, std::map<std::string, PlotAppearance>{}, nullptr));
        const InMemoryPreferences preferences;
        WarningSet                warnings;
        ErrorSet                  errors;
        const Result<std::string> text =
            OpenRocketSaver::save(*document, StorageOptions{}, preferences, warnings, errors);
        if (!text.has_value())
        {
            ADD_FAILURE() << text.error().toString();
            return;
        }
        Result<LoadedDocument> loaded =
            GeneralRocketLoader(m_environment.context()).load(stringToBytes(*text));
        if (!loaded.has_value())
        {
            ADD_FAILURE() << loaded.error().toString();
            return;
        }
        m_document = std::move(loaded->document);
    }

    /// The options of the reloaded simulation; the built-in defaults, with a test failure,
    /// without one.
    [[nodiscard]] const SimulationOptions& options() const
    {
        static const SimulationOptions kNone;
        if (m_document == nullptr || m_document->getSimulationCount() == 0)
        {
            ADD_FAILURE() << "the reloaded document has no simulation";
            return kNone;
        }
        return m_document->getSimulation(0)->getOptions();
    }

private:
    /// Declared first, destroyed last: the reloaded simulations keep its preference store.
    QtRocket::Test::LoaderEnvironment   m_environment;
    std::unique_ptr<OpenRocketDocument> m_document;
};

/// The dialog of the JUnit tests' edit() over @p options, with the tests' services.
class LookupDialog
{
public:
    explicit LookupDialog(SimulationOptions& options)
      : m_dialog(options, m_services.services(), nullptr)
    {
    }

    [[nodiscard]] AerodynamicLookupDialog&    dialog() noexcept { return m_dialog; }
    [[nodiscard]] Gui::Test::TestGuiServices& services() noexcept { return m_services; }

    /// The JUnit tests' field(dialog, "dragExampleArea"/"stabilityExampleArea").setText().
    void setText(Table table, const QString& text) { m_dialog.textArea(table).setPlainText(text); }

private:
    Gui::Test::TestGuiServices m_services;
    AerodynamicLookupDialog    m_dialog;
};

/// The number of messages @p prompts showed.
[[nodiscard]] std::size_t messageCount(const Gui::Test::ScriptedUserPrompts& prompts)
{
    std::size_t count = 0;
    for (const auto& prompt : prompts.prompts())
    {
        if (prompt.kind == Gui::Test::ScriptedUserPrompts::Kind::MESSAGE)
        {
            ++count;
        }
    }
    return count;
}

// Port of AerodynamicLookupPreservationTest.testAcceptingUnchangedEditorPreservesEmbeddedData
TEST_F(AerodynamicLookupDialogTest, TestAcceptingUnchangedEditorPreservesEmbeddedData)
{
    const std::unique_ptr<SimulationOptions> options = preservationOptions();
    {
        LookupDialog edit(*options);
        EXPECT_TRUE(edit.dialog().applyAndClose());
    }

    EXPECT_EQ(valuesOf(LookupRoundTrip(*options).options()), (LookupValues{.cd = 1, .cp = 1}));
}

/// The section of the parameter of the parameterised tests: the drag table (true) or the
/// stability table.
[[nodiscard]] Table tableKind(bool drag)
{
    return drag ? Table::DRAG : Table::STABILITY;
}

/// The test's parameter: the drag table is edited, or the stability table.
class AerodynamicLookupPreservationTest : public Test::GuiTest,
                                          public ::testing::WithParamInterface<bool>
{ };

/// testEditingOneTablePreservesTheOther's edit of @p table: its value 2.
[[nodiscard]] QString preservationEdit(Table table)
{
    return table == Table::DRAG ? QStringLiteral("Mach,Cd\n0,2")
                                : QStringLiteral("Mach,Cn,Cm,Cp\n0,1,1,2");
}

/// The values after the edit of @p table: 2 for it, 1 for the other.
[[nodiscard]] LookupValues valuesAfterEditOf(Table table)
{
    return table == Table::DRAG ? LookupValues{.cd = 2, .cp = 1} : LookupValues{.cd = 1, .cp = 2};
}

// Port of AerodynamicLookupPreservationTest.testEditingOneTablePreservesTheOther(drag)
TEST_P(AerodynamicLookupPreservationTest, TestEditingOneTablePreservesTheOther)
{
    const Table                              edited  = tableKind(GetParam());
    const std::unique_ptr<SimulationOptions> options = preservationOptions();
    {
        LookupDialog edit(*options);
        edit.setText(edited, preservationEdit(edited));
        EXPECT_TRUE(edit.dialog().applyAndClose());
    }

    EXPECT_EQ(valuesOf(LookupRoundTrip(*options).options()), valuesAfterEditOf(edited));
}

INSTANTIATE_TEST_SUITE_P(AerodynamicLookupDialogTest, AerodynamicLookupPreservationTest,
                         ::testing::Bool(), [](const ::testing::TestParamInfo<bool>& paramInfo) {
                             return std::string(paramInfo.param ? "drag" : "stability");
                         });

// Port of AerodynamicLookupPreservationTest.testRetryAfterInvalidStabilityTableRetainsDragEdit:
// the error Java's test dismisses from a timer is the scripted prompts' message here.
TEST_F(AerodynamicLookupDialogTest, TestRetryAfterInvalidStabilityTableRetainsDragEdit)
{
    const std::unique_ptr<SimulationOptions> options = preservationOptions();
    {
        LookupDialog edit(*options);
        edit.setText(Table::DRAG, QStringLiteral("Mach,Cd\n0,2"));
        edit.setText(Table::STABILITY, QStringLiteral("invalid"));
        EXPECT_FALSE(edit.dialog().applyAndClose());
        const bool errorShown = messageCount(edit.services().prompts()) == 1;
        EXPECT_TRUE(errorShown);
        EXPECT_EQ(valuesOf(*options), (LookupValues{.cd = 1, .cp = 1}));
        edit.setText(Table::STABILITY, QStringLiteral("Mach,Cn,Cm,Cp\n0,1,1,3"));
        EXPECT_TRUE(edit.dialog().applyAndClose());
    }

    EXPECT_EQ(valuesOf(LookupRoundTrip(*options).options()), (LookupValues{.cd = 2, .cp = 3}));
}

/// AerodynamicLookupTruncationTest's options: the table of @p drag (or the stability table) from
/// 41 rows that are kept, of the file kMissingCsv.
[[nodiscard]] std::unique_ptr<SimulationOptions> truncationOptions(bool drag)
{
    auto  options = std::make_unique<SimulationOptions>();
    Lines rows{drag ? "Mach,Cd" : "Mach,Cn,Cm,Cp"};
    for (int i = 0; i < 40; ++i)
    {
        rows.push_back(drag ? std::to_string(i) + "," + std::to_string(i + 1)
                            : std::to_string(i) + ",1,1," + std::to_string(i + 1));
    }
    if (drag)
    {
        options->setDragLookup(pathFromUtf8(kMissingCsv), tableOf(rows, {"cd"}), rows);
    }
    else
    {
        options->setStabilityLookup(pathFromUtf8(kMissingCsv), tableOf(rows, {"cn", "cm", "cp"}),
                                    rows);
    }
    return options;
}

/// The test's parameter: the drag table, or the stability table.
class AerodynamicLookupTruncationTest : public Test::GuiTest,
                                        public ::testing::WithParamInterface<bool>
{ };

/// @p options' table of @p table.
[[nodiscard]] std::shared_ptr<const MachAoALookup> lookupOf(const SimulationOptions& options,
                                                            Table                    table)
{
    return table == Table::DRAG ? options.getDragLookupTable() : options.getStabilityLookupTable();
}

/// @p options' rows of @p table (none for none).
[[nodiscard]] Lines rowsOf(const SimulationOptions& options, Table table)
{
    return (table == Table::DRAG ? options.getDragLookupCsvRows()
                                 : options.getStabilityLookupCsvRows())
        .value_or(Lines{});
}

/// The value the truncation test reads of @p table: "cd" or "cp".
[[nodiscard]] std::string_view valueColumnOf(Table table)
{
    return table == Table::DRAG ? "cd" : "cp";
}

// Port of AerodynamicLookupTruncationTest.testEditingLongTablePreservesFinalRows(drag)
TEST_P(AerodynamicLookupTruncationTest, TestEditingLongTablePreservesFinalRows)
{
    const Table                              edited  = tableKind(GetParam());
    const std::unique_ptr<SimulationOptions> options = truncationOptions(GetParam());
    {
        LookupDialog    edit(*options);
        QPlainTextEdit& area = edit.dialog().textArea(edited);
        // A harmless comment edit must not replace the table with just its preview (Java:
        // JTextArea.append()).
        area.moveCursor(QTextCursor::End);
        area.insertPlainText(QStringLiteral("\n# edited"));
        EXPECT_TRUE(edit.dialog().applyAndClose());
    }

    const LookupRoundTrip                      loaded(*options);
    const std::shared_ptr<const MachAoALookup> table = lookupOf(loaded.options(), edited);
    ASSERT_NE(table, nullptr);
    EXPECT_EQ(39, table->getMaxMach());
    EXPECT_EQ(40, table->interpolate(39, 0, valueColumnOf(edited)));
    EXPECT_EQ(42U, rowsOf(loaded.options(), edited).size());
}

INSTANTIATE_TEST_SUITE_P(AerodynamicLookupDialogTest, AerodynamicLookupTruncationTest,
                         ::testing::Bool(), [](const ::testing::TestParamInfo<bool>& paramInfo) {
                             return std::string(paramInfo.param ? "drag" : "stability");
                         });

// ----------------------------------------------------------------------------- the transcript

/// What the probe writes of texts and paths: its clean() (line breaks as "\n", tabs as "\t"),
/// the test's directory as "<tmp>" and the current directory as "<cwd>", with '/' between a
/// path's elements on every platform.
class Transcript
{
public:
    explicit Transcript(std::filesystem::path tmp) : m_tmp(std::move(tmp))
    {
        std::error_code error;
        m_cwd = std::filesystem::current_path(error);
    }

    [[nodiscard]] std::string clean(const QString& text) const
    {
        QString cleaned = text;
        cleaned.replace(u'\\', u'/');
        cleaned.replace(generic(m_tmp), QStringLiteral("<tmp>"));
        cleaned.replace(generic(m_cwd), QStringLiteral("<cwd>"));
        cleaned.replace(u'\n', QStringLiteral("\\n"));
        cleaned.replace(u'\t', QStringLiteral("\\t"));
        return QtStrings::toUtf8(cleaned);
    }

    [[nodiscard]] std::string path(const std::optional<std::filesystem::path>& path) const
    {
        return path.has_value() ? clean(QtStrings::fromPath(*path)) : "null";
    }

    /// The probe's section(): the summary, its tooltip, the buttons, the separator, the text
    /// area's title, text, editability and colour, whether it was edited, the file and whether
    /// there is a table.
    [[nodiscard]] std::string section(const AerodynamicLookupDialog& dialog, Table table) const
    {
        const QColor colour = dialog.textArea(table).palette().color(QPalette::Text);
        return std::string("  SECTION ") + (table == Table::DRAG ? "drag" : "stability") +
               " summary=[" + clean(dialog.summaryLabel(table).text()) +
               "] tip=" + clean(dialog.summaryLabel(table).toolTip()) + " | refresh" +
               (dialog.refreshButton(table).isEnabled() ? "" : " disabled") + " clear" +
               (dialog.clearButton(table).isEnabled() ? "" : " disabled") + " | separator=[" +
               clean(dialog.separatorComboBox(table).currentText()) + "] | " +
               QtStrings::toUtf8(dialog.textBox(table).title()) + " text=[" +
               clean(dialog.textArea(table).toPlainText()) + "]" +
               (dialog.textArea(table).isReadOnly() ? "" : " editable") +
               " colour=" + std::to_string(colour.red()) + "," + std::to_string(colour.green()) +
               "," + std::to_string(colour.blue()) +
               " | modified=" + (dialog.isModified(table) ? "true" : "false") +
               " csv=" + path(dialog.csvPath(table)) +
               " table=" + (dialog.lookupTable(table) != nullptr ? "true" : "false");
    }

    /// The probe's options(): the options' tables, their values at Mach 0 and their rows.
    [[nodiscard]] std::string options(const SimulationOptions& options) const
    {
        const std::shared_ptr<const MachAoALookup> drag      = options.getDragLookupTable();
        const std::shared_ptr<const MachAoALookup> stability = options.getStabilityLookupTable();
        return "  OPTIONS drag=" + path(options.getDragLookupCsvPath()) + " " +
               (drag == nullptr ? std::string("none")
                                : "cd(0,0)=" + javaDouble(drag->interpolate(0, 0, "cd")) +
                                      " maxMach=" + javaDouble(drag->getMaxMach())) +
               " rows=" + rows(options.getDragLookupCsvRows()) +
               " | stability=" + path(options.getStabilityLookupCsvPath()) + " " +
               (stability == nullptr
                    ? std::string("none")
                    : "cp(0,0)=" + javaDouble(stability->interpolate(0, 0, "cp"))) +
               " rows=" + rows(options.getStabilityLookupCsvRows());
    }

    /// The probe's message(): the type, the title and the text of a message.
    [[nodiscard]] std::string message(const Gui::Test::ScriptedUserPrompts::Prompt& prompt) const
    {
        const QStringList lines = prompt.texts.mid(1);
        return "  MESSAGE " + QtStrings::toUtf8(prompt.texts.value(0)) + " title=[" +
               clean(prompt.title) + "] text=[" + clean(lines.join(u'\n')) + "]";
    }

    /// The probe's chooser(): the file question as Java's JFileChooser showed it, its selected
    /// file the table's name in the directory (an empty directory is the home directory, as for
    /// Java's chooser without one).
    [[nodiscard]] std::string chooser(const UserPrompts::OpenFileRequest& request) const
    {
        std::string filters;
        for (const QString& filter : request.nameFilters)
        {
            filters += " [" + QtStrings::toUtf8(filter) + "]";
        }
        const std::string directory =
            request.directory.empty() ? "<home>" : clean(QtStrings::fromPath(request.directory));
        const std::string file =
            request.fileName.isEmpty()
                ? "null"
                : clean(
                      QtStrings::fromPath(request.directory / QtStrings::toPath(request.fileName)));
        return "  CHOOSER title=[" + QtStrings::toUtf8(request.title) + "] filters" + filters +
               " selected filter=[" + QtStrings::toUtf8(request.selectedNameFilter) +
               "] directory=" + directory + " selected file=" + file +
               " mode=" + (request.selection == UserPrompts::OpenFileSelection::FILES ? "0" : "2") +
               " multi=" + (request.multipleFiles ? "true" : "false");
    }

private:
    [[nodiscard]] static QString generic(const std::filesystem::path& path)
    {
        return QtStrings::fromUtf8(pathToUtf8(path.generic_u8string()));
    }

    [[nodiscard]] static std::string javaDouble(double value)
    {
        return Strings::javaDoubleToString(value);
    }

    [[nodiscard]] std::string rows(const std::optional<Lines>& rows) const
    {
        if (!rows.has_value())
        {
            return "null";
        }
        QStringList items;
        for (const std::string& row : *rows)
        {
            items.append(QtStrings::fromUtf8(row));
        }
        return clean(QStringLiteral("[") + items.join(QStringLiteral(", ")) + QStringLiteral("]"));
    }

    std::filesystem::path m_tmp;
    std::filesystem::path m_cwd;
};

/// A dialog of the transcript over its own options, with the tests' services, and the lines it
/// gives: its steps' states, the file questions and the messages in the probe's order.
class LookupSession
{
public:
    LookupSession(const Transcript& transcript, std::unique_ptr<SimulationOptions> options,
                  Lines& lines)
      : m_transcript(transcript),
        m_options(std::move(options)),
        m_lines(lines),
        m_dialog(*m_options, m_services.services(), nullptr)
    {
    }

    [[nodiscard]] AerodynamicLookupDialog&    dialog() noexcept { return m_dialog; }
    [[nodiscard]] SimulationOptions&          options() noexcept { return *m_options; }
    [[nodiscard]] Gui::Test::TestGuiServices& services() noexcept { return m_services; }

    /// The probe's walk of the dialog's content, under its title @p title.
    void walk(std::string_view title)
    {
        m_lines.push_back("== " + std::string(title));
        for (const std::string& line : Gui::Test::dialogWalk(m_dialog))
        {
            m_lines.push_back("WALK " + line);
        }
    }

    /// The file questions and messages since the last step, then the probe's lookupState().
    void state(std::string_view step)
    {
        flush();
        m_lines.push_back("STEP " + std::string(step));
        appendSections();
    }

    /// The probe's apply(): OK, its message (or the note that none came), and the state.
    void apply(std::string_view step)
    {
        const std::size_t before = messageCount(m_services.prompts());
        const bool        closed = m_dialog.applyAndClose();
        flush();
        if (messageCount(m_services.prompts()) == before)
        {
            m_lines.emplace_back("  (apply: 0 of 1 modal dialogs)");
        }
        m_lines.push_back("STEP " + std::string(step) +
                          ": displayable=" + (closed ? "false" : "true"));
        appendSections();
    }

    /// Answers the next file question with @p file (its directory as the one chosen from), or
    /// Cancel for nullopt.
    void answer(const std::optional<std::filesystem::path>& file)
    {
        if (file.has_value())
        {
            m_services.prompts().answerOpenFiles(
                UserPrompts::OpenFileAnswer{.files = {*file}, .directory = file->parent_path()});
        }
        else
        {
            m_services.prompts().answerOpenFiles(std::nullopt);
        }
    }

    /// The file questions and messages since the last call, in the probe's form.
    void flush()
    {
        const auto& requests = m_services.prompts().openFileRequests();
        for (; m_requests < requests.size(); ++m_requests)
        {
            m_lines.push_back(m_transcript.chooser(requests[m_requests]));
        }
        const auto& prompts = m_services.prompts().prompts();
        for (; m_prompts < prompts.size(); ++m_prompts)
        {
            if (prompts[m_prompts].kind == Gui::Test::ScriptedUserPrompts::Kind::MESSAGE)
            {
                m_lines.push_back(m_transcript.message(prompts[m_prompts]));
            }
        }
    }

private:
    void appendSections()
    {
        m_lines.push_back(m_transcript.section(m_dialog, Table::DRAG));
        m_lines.push_back(m_transcript.section(m_dialog, Table::STABILITY));
        m_lines.push_back(m_transcript.options(*m_options));
    }

    const Transcript&                  m_transcript;
    std::unique_ptr<SimulationOptions> m_options;
    Lines&                             m_lines;
    Gui::Test::TestGuiServices         m_services;
    AerodynamicLookupDialog            m_dialog;
    std::size_t                        m_requests{0};
    std::size_t                        m_prompts{0};
};

/// The probe's JUnit options, as a new object.
[[nodiscard]] std::unique_ptr<SimulationOptions> junitOptions()
{
    return preservationOptions();
}

/// The probe's "the separator" steps.
void separatorSteps(const Transcript& transcript, LookupSession& session, Lines& lines)
{
    lines.emplace_back("== the separator");
    FieldSeparatorComboBox& drag = session.dialog().separatorComboBox(Table::DRAG);
    for (const std::string_view separator : {";", "SPACE", "TAB", "|", ""})
    {
        drag.setSeparatorString(QtStrings::fromUtf8(separator));
        lines.push_back("STEP drag separator [" + std::string(separator) + "] char=[" +
                        transcript.clean(QString(drag.separatorChar())) + "]");
        lines.push_back(transcript.section(session.dialog(), Table::DRAG));
    }
    drag.setSeparatorString(QStringLiteral(","));
    session.dialog().separatorComboBox(Table::STABILITY).setSeparatorString(QStringLiteral(";"));
    session.state("stability separator [;]");
    session.apply("OK without tables");
}

/// The probe's "the JUnit tests' tables" steps.
void junitSteps(const Transcript& transcript, Lines& lines)
{
    lines.emplace_back("== the JUnit tests' tables");
    {
        LookupSession session(transcript, junitOptions(), lines);
        session.walk("lookup dialog with the JUnit tests' tables");
        session.state("with stored rows");
        session.dialog().textArea(Table::DRAG).setPlainText(QStringLiteral("Mach,Cd\n0,2"));
        session.state("drag text set");
        session.dialog().separatorComboBox(Table::DRAG).setSeparatorString(QStringLiteral(";"));
        session.state("drag separator [;] after the edit");
        session.dialog().separatorComboBox(Table::DRAG).setSeparatorString(QStringLiteral(","));
        session.dialog().textArea(Table::DRAG).setPlainText(QStringLiteral("Mach,Cd\n0,2"));
        session.dialog().textArea(Table::STABILITY).setPlainText(QStringLiteral("invalid"));
        session.apply("OK with an invalid stability text");
        session.dialog()
            .textArea(Table::STABILITY)
            .setPlainText(QStringLiteral("Mach,Cn,Cm,Cp\n0,1,1,3"));
        session.apply("OK again");
    }
    {
        LookupSession session(transcript, junitOptions(), lines);
        session.dialog()
            .textArea(Table::DRAG)
            .setPlainText(QStringLiteral("  # a comment  \n\nMach , Cd\n 0, 3 \n\n"));
        session.apply("OK with comments, blank lines and spaces");
    }
    LookupSession   session(transcript, junitOptions(), lines);
    QPlainTextEdit& drag = session.dialog().textArea(Table::DRAG);
    drag.setPlainText(QStringLiteral("Mach,Cl\n0,2"));
    session.apply("OK with a drag text without Cd");
    drag.setPlainText(QStringLiteral("Mach,Cd"));
    session.apply("OK with a drag text without rows");
    drag.setPlainText(QStringLiteral("Mach,Cd\n0,x"));
    session.apply("OK with a drag text that is no number");
    drag.setPlainText(QStringLiteral("Mach;Cd\n0;4"));
    session.apply("OK with semicolons and the separator [,]");
}

/// The probe's "Clear and Refresh" steps.
void clearAndRefreshSteps(const Transcript& transcript, Lines& lines)
{
    lines.emplace_back("== Clear and Refresh");
    LookupSession session(transcript, junitOptions(), lines);
    session.dialog().clearButton(Table::DRAG).click();
    session.state("drag cleared");
    session.dialog().refreshButton(Table::STABILITY).click();
    session.state("stability refreshed (the file does not exist)");
    session.apply("OK after Clear");
}

/// The probe's files of "Load from file", in the directory "lookup" of @p tmp.
struct LookupFiles
{
    std::filesystem::path drag;
    std::filesystem::path semicolons;
    std::filesystem::path noCd;
    std::filesystem::path stability;
};

[[nodiscard]] LookupFiles writeLookupFiles(const QtRocket::Test::TempDir& tmp)
{
    // Java's Files.write(): each line followed by a line break.
    return {
        .drag       = tmp.write("lookup/drag.csv",
                                "# drag of the probe\nMach,AoA,Cd\n0.1,0,0.4\n"
                                "0.1,4,0.45\n0.9,0,0.5\n0.9,4,0.55\n"),
        .semicolons = tmp.write("lookup/semicolons.txt", "Mach;Cd\n0.2;0.3\n1.2;0.6\n"),
        .noCd       = tmp.write("lookup/no-cd.csv", "Mach,Cl\n0.2,0.3\n"),
        .stability =
            tmp.write("lookup/stability.dat", "Mach,Cn,Cm,Cp\n0.2,1,2,0.5\n1.4,1.5,2.5,0.6\n"),
    };
}

/// The probe's "Load from file" steps.
void loadSteps(const Transcript& transcript, const QtRocket::Test::TempDir& tmp,
               const LookupFiles& files, Lines& lines)
{
    lines.emplace_back("== Load from file");
    LookupSession session(transcript, std::make_unique<SimulationOptions>(), lines);
    const AerodynamicLookupDialog& dialog = session.dialog();
    session.answer(files.drag);
    dialog.loadButton(Table::DRAG).click();
    session.state("drag loaded from drag.csv");
    lines.push_back("  default directory=" +
                    transcript.path(session.services().preferences().getDefaultDirectory()));
    session.answer(std::nullopt);
    dialog.loadButton(Table::DRAG).click();
    session.state("drag load cancelled");
    dialog.separatorComboBox(Table::DRAG).setSeparatorString(QStringLiteral(";"));
    session.answer(files.semicolons);
    dialog.loadButton(Table::DRAG).click();
    session.state("drag loaded from semicolons.txt with [;]");
    dialog.separatorComboBox(Table::DRAG).setSeparatorString(QStringLiteral(","));
    session.answer(files.noCd);
    dialog.loadButton(Table::DRAG).click();
    session.state("drag load of no-cd.csv (its error above, if any)");
    session.answer(files.stability);
    dialog.loadButton(Table::STABILITY).click();
    session.state("stability loaded from stability.dat");
    QPlainTextEdit& drag = dialog.textArea(Table::DRAG);
    drag.moveCursor(QTextCursor::End);
    drag.insertPlainText(QStringLiteral("\n# edited"));
    session.state("drag text appended");
    static_cast<void>(tmp.write("lookup/drag.csv", "Mach,Cd\n0,0.7\n3,0.9\n"));
    dialog.refreshButton(Table::DRAG).click();
    session.state("drag refreshed after the file changed");
    std::error_code error;
    std::filesystem::remove(files.stability, error);
    dialog.refreshButton(Table::STABILITY).click();
    session.state("stability refreshed after its file was deleted");
    session.apply("OK after the loads");
}

/// The probe's last steps: a stored table without rows whose file is missing, the error of a
/// failed load, and those of reloads (its failedLoadMessage()).
void failureSteps(const Transcript& transcript, const QtRocket::Test::TempDir& tmp,
                  const LookupFiles& files, Lines& lines)
{
    lines.emplace_back("== a stored table whose file is missing and has no rows");
    {
        auto options = std::make_unique<SimulationOptions>();
        options->setDragLookup(tmp.resolve("lookup/gone.csv"), tableOf({"Mach,Cd", "0,1"}, {"cd"}),
                               std::nullopt);
        LookupSession session(transcript, std::move(options), lines);
        session.state("drag table without rows, file missing");
    }
    lines.emplace_back("== the error of a failed load");
    {
        LookupSession session(transcript, std::make_unique<SimulationOptions>(), lines);
        // The probe's preferences are the application's, whose default directory the loads above
        // left; each session here has its own.
        session.services().preferences().setDefaultDirectory(tmp.resolve("lookup"));
        session.answer(files.noCd);
        session.dialog().loadButton(Table::DRAG).click();
        session.state("after the chooser and the message");
    }
    // Java sets the dialog's dragCsv by reflection; here the options give the dialog its file.
    for (const std::filesystem::path& file :
         {files.noCd, tmp.resolve("lookup/nothing-here.csv"), tmp.resolve("lookup")})
    {
        auto options = std::make_unique<SimulationOptions>();
        options->setDragLookup(file, tableOf({"Mach,Cd", "0,1"}, {"cd"}), std::nullopt);
        LookupSession session(transcript, std::move(options), lines);
        session.dialog().reloadLookup(Table::DRAG);
        session.flush();
    }
}

/// The probe's transcript of the lookup dialog, from its "LOOKUP" line on (the LOOKUP line
/// itself is checked apart).
[[nodiscard]] Lines lookupTranscript(const QtRocket::Test::TempDir& tmp)
{
    const Transcript  transcript(tmp.path());
    const LookupFiles files = writeLookupFiles(tmp);
    Lines             lines;
    {
        LookupSession session(transcript, std::make_unique<SimulationOptions>(), lines);
        session.walk("lookup dialog without tables");
        session.state("without tables");
        separatorSteps(transcript, session, lines);
    }
    junitSteps(transcript, lines);
    clearAndRefreshSteps(transcript, lines);
    loadSteps(transcript, tmp, files, lines);
    failureSteps(transcript, tmp, files, lines);
    return lines;
}

/// @p lines with every "{CN, CM, CP}" written as "CN, CM, CP": the stability table's columns
/// in their order in the table (Java's Set.copyOf() order changes from run to run).
[[nodiscard]] Lines withStabilityColumnsInOrder(Lines lines)
{
    int found = 0;
    for (std::string& line : lines)
    {
        for (std::size_t at = line.find("{CN, CM, CP}"); at != std::string::npos;
             at             = line.find("{CN, CM, CP}", at))
        {
            line.replace(at, std::string_view("{CN, CM, CP}").size(), "CN, CM, CP");
            ++found;
        }
    }
    EXPECT_GT(found, 0);
    return lines;
}

/// The probe's transcript of the lookup dialog after its LOOKUP line, with QtRocket's edits.
[[nodiscard]] Lines lookupExpectation()
{
    Lines lines;
    for (const std::span<const std::string_view> section :
         {std::span<const std::string_view>(Pins::kLookupDialog).subspan(1),
          std::span<const std::string_view>(Pins::kLookupSeparator),
          std::span<const std::string_view>(Pins::kLookupJunitTables),
          std::span<const std::string_view>(Pins::kLookupClearAndRefresh),
          std::span<const std::string_view>(Pins::kLookupLoad),
          std::span<const std::string_view>(Pins::kLookupStoredWithoutRows),
          std::span<const std::string_view>(Pins::kLookupFailedLoad)})
    {
        if (section.data() == Pins::kLookupSeparator.data())
        {
            lines.emplace_back("== the separator");
        }
        else if (section.data() == Pins::kLookupJunitTables.data())
        {
            lines.emplace_back("== the JUnit tests' tables");
        }
        else if (section.data() == Pins::kLookupClearAndRefresh.data())
        {
            lines.emplace_back("== Clear and Refresh");
        }
        else if (section.data() == Pins::kLookupLoad.data())
        {
            lines.emplace_back("== Load from file");
        }
        else if (section.data() == Pins::kLookupStoredWithoutRows.data())
        {
            lines.emplace_back("== a stored table whose file is missing and has no rows");
        }
        else if (section.data() == Pins::kLookupFailedLoad.data())
        {
            lines.emplace_back("== the error of a failed load");
        }
        lines.insert(lines.end(), section.begin(), section.end());
    }
    // The file question's filters in Qt's form, the CSV filter first (Java lists its accept-all
    // filter first).
    lines = Gui::Test::replacedIn(
        std::move(lines),
        "filters [All Files] [CSV files (*.csv, *.txt, *.dat) csv,txt,dat] selected "
        "filter=[CSV files (*.csv, *.txt, *.dat)]",
        "filters [CSV files (*.csv *.txt *.dat)] [All Files (*)] selected filter=[CSV files "
        "(*.csv *.txt *.dat)]",
        6);
    return withStabilityColumnsInOrder(std::move(lines));
}

TEST_F(AerodynamicLookupDialogTest, TheDialogShowsAndDoesWhatOpenRocketsDialogShowsAndDoes)
{
    const QtRocket::Test::TempDir tmp;
    Gui::Test::expectSameLines(lookupTranscript(tmp), lookupExpectation());
}

/// The names of @p dialog's buttons that Enter could click (a default or auto-default button),
/// each followed by "; ".
[[nodiscard]] std::string defaultButtonsOf(const QDialog& dialog)
{
    std::string names;
    for (const QPushButton* button : dialog.findChildren<QPushButton*>())
    {
        if (button->isDefault() || button->autoDefault())
        {
            names += QtStrings::toUtf8(button->objectName()) + "; ";
        }
    }
    return names;
}

// "LOOKUP title=[Aerodynamic lookup tables] modality=APPLICATION_MODAL default=none
// escape=false close operation=2 resizable=true": QtRocket's Escape cancels (a QDialog's).
TEST_F(AerodynamicLookupDialogTest, TheDialogIsJavasWithoutADefaultButton)
{
    SimulationOptions                options;
    const Gui::Test::TestGuiServices services;
    const AerodynamicLookupDialog    dialog(options, services.services(), nullptr);
    EXPECT_EQ(dialog.windowTitle(), QStringLiteral("Aerodynamic lookup tables"));
    EXPECT_EQ(dialog.windowModality(), Qt::ApplicationModal);
    EXPECT_EQ(defaultButtonsOf(dialog), "");
    EXPECT_EQ(dialog.tabs().count(), 2);
}

/// The probe's FORMAT lines: formatLookupSummary() of the probe's tables.
[[nodiscard]] Lines formatTranscript()
{
    const std::vector<std::pair<std::string_view, Lines>> cases{
        {"no AoA", {"Mach,Cd", "0.3,0.35", "2,0.5"}},
        {"with AoA", {"Mach,AoA,Cd", "0.30,0,0.35", "0.30,5,0.42"}},
        {"negative zero",
         {"Mach,AoA,Cd", "0,-0.0004,0.3", "0.99999,-0.0004,0.3", "0,15.12345,1",
          "0.99999,15.12345,1"}},
        {"angleofattack and extra columns",
         {"mach, angleofattack, cd, cl", "0.5,2,0.3,0.1", "1.5,2,0.4,0.2", "0.5,4,0.3,0.1",
          "1.5,4,0.4,0.2"}},
        {"large", {"Mach,Cd", "0.0005,1", "12345.6785,2"}},
    };
    Lines lines;
    for (const auto& [name, rows] : cases)
    {
        lines.push_back("FORMAT " + std::string(name) + " [" +
                        QtStrings::toUtf8(
                            AerodynamicLookupDialog::formatLookupSummary(*tableOf(rows, {"cd"}))) +
                        "]");
    }
    const Lines stability{"Mach,AoA,Cn,Cm,Cp", "0.30,0,0.12,-0.01,1.45", "0.30,5,0.15,-0.03,1.40"};
    lines.push_back("FORMAT stability [" +
                    QtStrings::toUtf8(AerodynamicLookupDialog::formatLookupSummary(
                        *tableOf(stability, {"cn", "cm", "cp"}))) +
                    "]");
    return lines;
}

TEST_F(AerodynamicLookupDialogTest, FormatLookupSummaryIsJavas)
{
    EXPECT_EQ(formatTranscript(),
              withStabilityColumnsInOrder(Lines(Pins::kFormat.begin(), Pins::kFormat.end())));
}

TEST_F(AerodynamicLookupDialogTest, ASeparatorThatIsNoAsciiCharacterCannotBeUsed)
{
    const QtRocket::Test::TempDir tmp;
    const std::filesystem::path   file = tmp.write("drag.csv", "Mach\u00A7Cd\n0\u00A70.5\n");
    SimulationOptions             options;
    LookupDialog                  edit(options);
    edit.dialog().separatorComboBox(Table::DRAG).setSeparatorString(QString(QChar(0x00A7)));
    edit.services().prompts().answerOpenFiles(
        UserPrompts::OpenFileAnswer{.files = {file}, .directory = tmp.path()});
    edit.dialog().chooseLookup(Table::DRAG);
    EXPECT_FALSE(options.hasDragLookup());
    ASSERT_EQ(messageCount(edit.services().prompts()), 1U);
    const auto& message = edit.services().prompts().prompts().back();
    EXPECT_EQ(message.title, QStringLiteral("Failed to load CSV"));
    EXPECT_EQ(message.texts.value(2),
              QStringLiteral("The field separator must be an ASCII character."));
}

TEST_F(AerodynamicLookupDialogTest, CancelKeepsWhatALoadGaveTheOptions)
{
    const QtRocket::Test::TempDir tmp;
    const std::filesystem::path   file = tmp.write("drag.csv", "Mach,Cd\n0,0.5\n2,0.7\n");
    SimulationOptions             options;
    LookupDialog                  edit(options);
    edit.services().prompts().answerOpenFiles(
        UserPrompts::OpenFileAnswer{.files = {file}, .directory = tmp.path()});
    edit.dialog().loadButton(Table::DRAG).click();
    ASSERT_TRUE(options.hasDragLookup());
    edit.dialog().clearButton(Table::DRAG).click();
    edit.dialog().cancelButton().click();
    // As in Java: the load was given to the options at once; Clear was the dialog's own.
    EXPECT_TRUE(options.hasDragLookup());
    EXPECT_EQ(edit.dialog().result(), QDialog::Rejected);
}

TEST_F(AerodynamicLookupDialogTest, OptionsThatGoLeaveTheDialogInert)
{
    auto                             options = preservationOptions();
    const Gui::Test::TestGuiServices services;
    AerodynamicLookupDialog          dialog(*options, services.services(), nullptr);
    options.reset();
    // Nothing reaches the options that have gone (ASan checks that nothing reads them either);
    // the file question is not asked.
    EXPECT_FALSE(dialog.applyAndClose());
    dialog.chooseLookup(Table::DRAG);
    dialog.textArea(Table::DRAG).setPlainText(QStringLiteral("Mach,Cd\n0,9"));
    dialog.clearButton(Table::STABILITY).click();
    EXPECT_TRUE(dialog.isModified(Table::DRAG));
    EXPECT_EQ(dialog.lookupTable(Table::STABILITY), nullptr);
}

// A bug of OpenRocket's, found while porting, fixed: a load, a refresh or Clear of one table
// showed both sections again from the options, throwing away an edit of the other table's text.
TEST_F(AerodynamicLookupDialogTest, AnEditOfOneTableSurvivesALoadOrClearOfTheOther)
{
    const QtRocket::Test::TempDir tmp;
    const std::filesystem::path   file =
        tmp.write("stability.csv", "Mach,Cn,Cm,Cp\n0,1,1,4\n1,1,1,5\n");
    const std::unique_ptr<SimulationOptions> options = preservationOptions();
    LookupDialog                             edit(*options);
    edit.setText(Table::DRAG, QStringLiteral("Mach,Cd\n0,2"));
    edit.services().prompts().answerOpenFiles(
        UserPrompts::OpenFileAnswer{.files = {file}, .directory = tmp.path()});
    edit.dialog().loadButton(Table::STABILITY).click();
    EXPECT_EQ(edit.dialog().textArea(Table::DRAG).toPlainText(), QStringLiteral("Mach,Cd\n0,2"));
    EXPECT_TRUE(edit.dialog().isModified(Table::DRAG));
    EXPECT_TRUE(
        edit.dialog().summaryLabel(Table::DRAG).text().contains(QStringLiteral("(modified)")));
    edit.dialog().clearButton(Table::STABILITY).click();
    EXPECT_TRUE(edit.dialog().isModified(Table::DRAG));
    EXPECT_TRUE(edit.dialog().applyAndClose());
    EXPECT_EQ(valuesOf(*options).cd, 2);
    EXPECT_FALSE(options->hasStabilityLookup());
}

}  // namespace

}  // namespace QtRocket::Gui
