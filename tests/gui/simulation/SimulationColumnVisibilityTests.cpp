// The simulations table's column chooser (Java: SimulationPanel's ColumnVisibilityController,
// SimulationPanel.java:1787-2009). OpenRocket has no JUnit test of it. What OpenRocket stores and
// shows for the same choices comes from
// tools/openrocket-gui-pins/simulation-table/probe/SimulationTableViewProbe.java, pinned here by
// hand (the family's hand-pinned.txt keeps its transcript: the COLS and MENU lines; their
// "apppref" field is not compared, see below). QtRocket differs in two ways the tests say where
// they meet them:
// - an empty stored text means "no column hidden" (an OpenRocket bug found while porting, fixed:
//   SimulationColumnVisibility.h);
// - reading the application preference writes nothing: OpenRocket's SwingPreferences.getString()
//   stores the default it is given ("") when the key is absent, so the probe's apppref is ""
//   after its first table where QtRocket's stays absent (the core's Preferences::getString()).

#include "QtRocket/gui/simulation/SimulationColumnVisibility.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <QAction>
#include <QApplication>
#include <QContextMenuEvent>
#include <QHeaderView>
#include <QList>
#include <QMenu>
#include <QObject>
#include <QPoint>
#include <QString>
#include <Qt>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/OpenRocketDocumentFactory.h"
#include "QtRocket/gui/settings/GuiPreferenceKeys.h"
#include "QtRocket/gui/simulation/SimulationColumns.h"
#include "QtRocket/gui/simulation/SimulationTableView.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/preferences/DocumentPreferences.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/preferences/Preferences.h"
#include "simulation/SimulationTableTestSupport.h"
#include "widgets/WidgetTestSupport.h"

namespace QtRocket::Gui
{

namespace
{

using SimulationColumnVisibilityTest = Gui::Test::GuiTest;
using Column                         = SimulationColumns::Column;

constexpr std::string_view kDocumentKey = GuiPreferenceKeys::kSimulationTableHiddenColumns;
constexpr std::string_view kApplicationKey =
    GuiPreferenceKeys::kSimulationTableHiddenColumnsDefault;

/// The probe's view of a table without the stepper column.
constexpr std::string_view kWithoutStepper =
    "status,warnings,name,configuration,launchRodVelocity,apogee,deploymentVelocity,"
    "optimumCoastTime,maxVelocity,maxAcceleration,timeToApogee,flightTime,groundHitVelocity";
/// The probe's view of a table with every column.
constexpr std::string_view kEveryColumn =
    "status,warnings,name,configuration,simulationStepper,launchRodVelocity,apogee,"
    "deploymentVelocity,optimumCoastTime,maxVelocity,maxAcceleration,timeToApogee,flightTime,"
    "groundHitVelocity";

/// A design's table with its column chooser, made after the design was marked saved (as the
/// probe marks it): the chooser goes first.
class ChosenTable
{
public:
    ChosenTable(std::unique_ptr<OpenRocketDocument> document, Preferences& application)
      : m_table(saved(std::move(document))),
        m_chooser(m_table.view(), m_table.document().getDocumentPreferences(), application, nullptr)
    {
    }

    [[nodiscard]] Gui::Test::SimulationTableFixture& table() noexcept { return m_table; }
    [[nodiscard]] SimulationColumnVisibility&        chooser() noexcept { return m_chooser; }
    [[nodiscard]] OpenRocketDocument& document() const noexcept { return m_table.document(); }

    /// The probe's COLS line from "docpref" on, without "apppref": "docpref=apogee
    /// saved=false hidden=apogee view=status,...", tab-separated.
    [[nodiscard]] std::string state() const
    {
        const DocumentPreferences& preferences = m_table.document().getDocumentPreferences();
        const std::string          docpref     = preferences.contains(kDocumentKey)
                                                     ? preferences.getString(kDocumentKey, "")
                                                     : std::string("<unset>");
        return "docpref=" + docpref +
               "\tsaved=" + (m_table.document().isSaved() ? "true" : "false") + "\thidden=" +
               SimulationColumnVisibility::formatHiddenColumns(m_chooser.hiddenColumns()) +
               "\tview=" + m_table.visibleIds();
    }

    /// The probe's MENU line from its first item on: each check box item's text, [x] or [ ] and
    /// (disabled); --- for the separator; the last item's text and its tooltip in braces.
    [[nodiscard]] std::string menu()
    {
        QMenu menu;
        m_chooser.populateMenu(menu);
        return menuText(menu);
    }

    /// Clicks the item of @p column in a new menu (QAction::trigger(): its check toggles).
    void click(Column column)
    {
        QMenu menu;
        m_chooser.populateMenu(menu);
        auto* item = menu.findChild<QAction*>(QString::fromUtf8(SimulationColumns::id(column)));
        ASSERT_NE(item, nullptr);
        item->trigger();
    }

    [[nodiscard]] static std::string menuText(const QMenu& menu)
    {
        std::string text;
        for (const QAction* item : menu.actions())
        {
            if (!text.empty())
            {
                text += '\t';
            }
            if (item->isSeparator())
            {
                text += "---";
                continue;
            }
            text += QtStrings::toUtf8(item->text());
            if (item->isCheckable())
            {
                text += item->isChecked() ? "[x]" : "[ ]";
            }
            text += item->isEnabled() ? "" : "(disabled)";
            if (!item->isCheckable())
            {
                text += "{" + QtStrings::toUtf8(item->toolTip()) + "}";
            }
        }
        return text;
    }

private:
    [[nodiscard]] static std::unique_ptr<OpenRocketDocument> saved(
        std::unique_ptr<OpenRocketDocument> document)
    {
        if (document != nullptr)
        {
            document->setSaved(true);
        }
        return document;
    }

    Gui::Test::SimulationTableFixture m_table;
    SimulationColumnVisibility        m_chooser;
};

/// "A simple model rocket" with its table and chooser in an application of @p application.
[[nodiscard]] std::unique_ptr<ChosenTable> simpleModelRocket(Preferences& application)
{
    std::unique_ptr<OpenRocketDocument> document =
        Gui::Test::loadSimulationExample("A simple model rocket.ork");
    return document == nullptr ? nullptr
                               : std::make_unique<ChosenTable>(std::move(document), application);
}

/// A new design whose document preference is @p text (none for nullopt), with its table and
/// chooser.
[[nodiscard]] std::unique_ptr<ChosenTable> newDesign(Preferences&                    application,
                                                     std::optional<std::string_view> text = {})
{
    std::unique_ptr<OpenRocketDocument> document = OpenRocketDocumentFactory::createNewRocket();
    if (text.has_value())
    {
        document->getDocumentPreferences().putString(kDocumentKey, *text);
    }
    return std::make_unique<ChosenTable>(std::move(document), application);
}

/// The probe's MENU of a table without the stepper column.
constexpr std::string_view kNewDesignMenu =
    "Status[x]\tWarnings[x]\tName[x]\tConfiguration[x]\tSim. Stepper[ ]\tVelocity off rod[x]\t"
    "Apogee[x]\tVelocity at deployment[x]\tOptimum delay[x]\tMax. velocity[x]\t"
    "Max. acceleration[x]\tTime to apogee[x]\tFlight time[x]\tGround hit velocity[x]\t---\t"
    "Save as default{Save the selected visibility settings for all new designs}";

// ------------------------------------------------------------------------------ the text

TEST_F(SimulationColumnVisibilityTest, ReadsAndWritesTheStoredTextAsOpenRocket)
{
    using Columns = std::vector<Column>;
    EXPECT_EQ(SimulationColumnVisibility::parseHiddenColumns(""), Columns{});
    EXPECT_EQ(SimulationColumnVisibility::parseHiddenColumns(","), Columns{});
    EXPECT_EQ(SimulationColumnVisibility::parseHiddenColumns("apogee"), Columns{Column::APOGEE});
    // Blanks (Java's trim(): every character up to ' '), empty and unknown ids, a repeat.
    EXPECT_EQ(SimulationColumnVisibility::parseHiddenColumns(" name , bogus,,flightTime ,name"),
              (Columns{Column::NAME, Column::FLIGHT_TIME}));
    EXPECT_EQ(SimulationColumnVisibility::parseHiddenColumns("\tstatus\n,\x01warnings"),
              (Columns{Column::STATUS, Column::WARNINGS}));
    EXPECT_EQ(SimulationColumnVisibility::parseHiddenColumns("Apogee,apogee "),
              Columns{Column::APOGEE});
    EXPECT_EQ(SimulationColumnVisibility::formatHiddenColumns({}), "");
    EXPECT_EQ(SimulationColumnVisibility::formatHiddenColumns(
                  Columns{Column::STATUS, Column::APOGEE, Column::SIMULATION_STEPPER}),
              "status,apogee,simulationStepper");
}

TEST_F(SimulationColumnVisibilityTest, FindsTheStoredHiddenColumns)
{
    InMemoryPreferences application;
    DocumentPreferences document;
    using Columns = std::vector<Column>;
    // Neither: the stepper column.
    EXPECT_EQ(SimulationColumnVisibility::storedHiddenColumns(document, application),
              Columns{Column::SIMULATION_STEPPER});
    application.putString(kApplicationKey, "apogee,status");
    EXPECT_EQ(SimulationColumnVisibility::storedHiddenColumns(document, application),
              (Columns{Column::APOGEE, Column::STATUS}));
    document.putString(kDocumentKey, "flightTime");
    EXPECT_EQ(SimulationColumnVisibility::storedHiddenColumns(document, application),
              Columns{Column::FLIGHT_TIME});
    // An empty text: no column hidden (OpenRocket would go on to the application's text).
    document.putString(kDocumentKey, "");
    EXPECT_EQ(SimulationColumnVisibility::storedHiddenColumns(document, application), Columns{});
    // A document preference of another type: none (OpenRocket throws a ClassCastException).
    document.putBoolean(kDocumentKey, true);
    EXPECT_EQ(SimulationColumnVisibility::storedHiddenColumns(document, application),
              (Columns{Column::APOGEE, Column::STATUS}));
    application.putString(kApplicationKey, "");
    EXPECT_EQ(SimulationColumnVisibility::storedHiddenColumns(document, application), Columns{});
}

// ------------------------------------------------------------------------------ the probe

TEST_F(SimulationColumnVisibilityTest, HidesTheStepperColumnOfADesignWithoutAChoice)
{
    // COLS new design, MENU new design, COLS loaded.
    InMemoryPreferences                application;
    const std::unique_ptr<ChosenTable> fresh = newDesign(application);
    EXPECT_EQ(fresh->state(), "docpref=<unset>\tsaved=true\thidden=simulationStepper\tview=" +
                                  std::string(kWithoutStepper));
    EXPECT_EQ(fresh->menu(), kNewDesignMenu);
    const std::unique_ptr<ChosenTable> loaded = simpleModelRocket(application);
    ASSERT_NE(loaded, nullptr);
    EXPECT_EQ(loaded->state(), "docpref=<unset>\tsaved=true\thidden=simulationStepper\tview=" +
                                   std::string(kWithoutStepper));
    // Nothing is written when a table is made.
    EXPECT_FALSE(application.get(kApplicationKey).has_value());
}

TEST_F(SimulationColumnVisibilityTest, StoresEveryChoiceInTheDesignAsOpenRocket)
{
    // The probe's COLS lines "show simulationStepper" to "hide apogee again" and its MENU
    // "status and apogee hidden".
    InMemoryPreferences                application;
    const std::unique_ptr<ChosenTable> table = simpleModelRocket(application);
    ASSERT_NE(table, nullptr);
    SimulationColumnVisibility&      chooser       = table->chooser();
    const std::optional<std::string> undo          = table->document().getUndoDescription();
    const bool                       undoAvailable = table->document().isUndoAvailable();
    chooser.setColumnVisible(Column::SIMULATION_STEPPER, true);
    EXPECT_EQ(table->state(), "docpref=\tsaved=false\thidden=\tview=" + std::string(kEveryColumn));
    chooser.setColumnVisible(Column::APOGEE, false);
    EXPECT_EQ(table->state(),
              "docpref=apogee\tsaved=false\thidden=apogee\tview=status,warnings,name,"
              "configuration,simulationStepper,launchRodVelocity,deploymentVelocity,"
              "optimumCoastTime,maxVelocity,maxAcceleration,timeToApogee,flightTime,"
              "groundHitVelocity");
    chooser.setColumnVisible(Column::STATUS, false);
    EXPECT_EQ(table->state(),
              "docpref=apogee,status\tsaved=false\thidden=apogee,status\tview=warnings,name,"
              "configuration,simulationStepper,launchRodVelocity,deploymentVelocity,"
              "optimumCoastTime,maxVelocity,maxAcceleration,timeToApogee,flightTime,"
              "groundHitVelocity");
    chooser.setColumnVisible(Column::APOGEE, true);
    EXPECT_EQ(table->state(),
              "docpref=status\tsaved=false\thidden=status\tview=warnings,name,configuration,"
              "simulationStepper,launchRodVelocity,apogee,deploymentVelocity,optimumCoastTime,"
              "maxVelocity,maxAcceleration,timeToApogee,flightTime,groundHitVelocity");
    chooser.setColumnVisible(Column::APOGEE, false);
    EXPECT_EQ(table->state(),
              "docpref=status,apogee\tsaved=false\thidden=status,apogee\tview=warnings,name,"
              "configuration,simulationStepper,launchRodVelocity,deploymentVelocity,"
              "optimumCoastTime,maxVelocity,maxAcceleration,timeToApogee,flightTime,"
              "groundHitVelocity");
    EXPECT_EQ(table->menu(),
              "Status[ ]\tWarnings[x]\tName[x]\tConfiguration[x]\tSim. Stepper[x]\t"
              "Velocity off rod[x]\tApogee[ ]\tVelocity at deployment[x]\tOptimum delay[x]\t"
              "Max. velocity[x]\tMax. acceleration[x]\tTime to apogee[x]\tFlight time[x]\t"
              "Ground hit velocity[x]\t---\t"
              "Save as default{Save the selected visibility settings for all new designs}");
    // No undo position: the design is only unsaved.
    EXPECT_EQ(table->document().getUndoDescription(), undo);
    EXPECT_EQ(table->document().isUndoAvailable(), undoAvailable);
    // Nothing of the application's.
    EXPECT_FALSE(application.get(kApplicationKey).has_value());
}

TEST_F(SimulationColumnVisibilityTest, ChoosesThroughItsMenu)
{
    InMemoryPreferences                application;
    const std::unique_ptr<ChosenTable> table = simpleModelRocket(application);
    ASSERT_NE(table, nullptr);
    table->click(Column::SIMULATION_STEPPER);
    table->click(Column::APOGEE);
    EXPECT_EQ(table->chooser().hiddenColumns(), std::vector<Column>{Column::APOGEE});
    EXPECT_FALSE(table->chooser().isColumnVisible(Column::APOGEE));
    EXPECT_TRUE(table->chooser().isColumnVisible(Column::SIMULATION_STEPPER));
    // "Save as default".
    QMenu menu;
    table->chooser().populateMenu(menu);
    EXPECT_TRUE(menu.toolTipsVisible());
    auto* save = menu.findChild<QAction*>(QStringLiteral("saveAsDefault"));
    ASSERT_NE(save, nullptr);
    save->trigger();
    EXPECT_EQ(application.get(kApplicationKey), std::optional<std::string>("apogee"));
}

TEST_F(SimulationColumnVisibilityTest, SavesTheChoiceAsTheApplicationsDefault)
{
    // The probe's COLS "save as default", "new design after save as default" and "docpref
    // flightTime, apppref set".
    InMemoryPreferences                application;
    const std::unique_ptr<ChosenTable> table = simpleModelRocket(application);
    ASSERT_NE(table, nullptr);
    table->chooser().setColumnVisible(Column::SIMULATION_STEPPER, true);
    table->chooser().setColumnVisible(Column::STATUS, false);
    table->chooser().setColumnVisible(Column::APOGEE, false);
    table->chooser().saveAsDefault();
    EXPECT_EQ(application.get(kApplicationKey), std::optional<std::string>("status,apogee"));
    EXPECT_EQ(table->document().getDocumentPreferences().getString(kDocumentKey, "<unset>"),
              "status,apogee");
    const std::unique_ptr<ChosenTable> fresh = newDesign(application);
    EXPECT_EQ(fresh->state(),
              "docpref=<unset>\tsaved=true\thidden=status,apogee\tview=warnings,name,"
              "configuration,simulationStepper,launchRodVelocity,deploymentVelocity,"
              "optimumCoastTime,maxVelocity,maxAcceleration,timeToApogee,flightTime,"
              "groundHitVelocity");
    const std::unique_ptr<ChosenTable> own = newDesign(application, "flightTime");
    EXPECT_EQ(own->state(),
              "docpref=flightTime\tsaved=true\thidden=flightTime\tview=status,warnings,name,"
              "configuration,simulationStepper,launchRodVelocity,apogee,deploymentVelocity,"
              "optimumCoastTime,maxVelocity,maxAcceleration,timeToApogee,groundHitVelocity");
}

TEST_F(SimulationColumnVisibilityTest, TakesAnEmptyStoredTextForNoHiddenColumn)
{
    // An OpenRocket bug found while porting, fixed (SimulationColumnVisibility.h). OpenRocket's
    // probe: COLS "empty docpref, apppref set" hidden=status,apogee (the application's), and
    // after "show simulationStepper, save as default" (apppref=) COLS "new design after an
    // empty default" hidden=simulationStepper.
    InMemoryPreferences application;
    application.putString(kApplicationKey, "status,apogee");
    const std::unique_ptr<ChosenTable> empty = newDesign(application, "");
    EXPECT_EQ(empty->state(), "docpref=\tsaved=true\thidden=\tview=" + std::string(kEveryColumn));

    const std::unique_ptr<ChosenTable> shown = simpleModelRocket(application);
    ASSERT_NE(shown, nullptr);
    shown->chooser().setColumnVisible(Column::STATUS, true);
    shown->chooser().setColumnVisible(Column::APOGEE, true);
    shown->chooser().saveAsDefault();
    EXPECT_EQ(application.get(kApplicationKey), std::optional<std::string>(""));
    const std::unique_ptr<ChosenTable> fresh = newDesign(application);
    EXPECT_EQ(fresh->state(),
              "docpref=<unset>\tsaved=true\thidden=\tview=" + std::string(kEveryColumn));
}

TEST_F(SimulationColumnVisibilityTest, ReadsTheDesignsTextAsOpenRocket)
{
    // The probe's COLS "docpref with blanks, an unknown id and a repeat" and "then hide apogee".
    InMemoryPreferences                application;
    const std::unique_ptr<ChosenTable> odd =
        newDesign(application, " name , bogus,,flightTime ,name");
    EXPECT_EQ(odd->state(),
              "docpref= name , bogus,,flightTime ,name\tsaved=true\thidden=name,flightTime\t"
              "view=status,warnings,configuration,simulationStepper,launchRodVelocity,apogee,"
              "deploymentVelocity,optimumCoastTime,maxVelocity,maxAcceleration,timeToApogee,"
              "groundHitVelocity");
    odd->chooser().setColumnVisible(Column::APOGEE, false);
    EXPECT_EQ(odd->state(),
              "docpref=name,flightTime,apogee\tsaved=false\thidden=name,flightTime,apogee\t"
              "view=status,warnings,configuration,simulationStepper,launchRodVelocity,"
              "deploymentVelocity,optimumCoastTime,maxVelocity,maxAcceleration,timeToApogee,"
              "groundHitVelocity");
}

TEST_F(SimulationColumnVisibilityTest, ShowsTheFirstColumnWhenEveryColumnIsHidden)
{
    // The probe's COLS and MENU "docpref hides every column", COLS "then show name".
    InMemoryPreferences                application;
    const std::unique_ptr<ChosenTable> all = newDesign(
        application,
        "status,warnings,name,configuration,simulationStepper,launchRodVelocity,apogee,"
        "deploymentVelocity,optimumCoastTime,maxVelocity,maxAcceleration,timeToApogee,flightTime,"
        "groundHitVelocity");
    EXPECT_EQ(all->state(),
              "docpref=status,warnings,name,configuration,simulationStepper,launchRodVelocity,"
              "apogee,deploymentVelocity,optimumCoastTime,maxVelocity,maxAcceleration,"
              "timeToApogee,flightTime,groundHitVelocity\tsaved=true\thidden=warnings,name,"
              "configuration,simulationStepper,launchRodVelocity,apogee,deploymentVelocity,"
              "optimumCoastTime,maxVelocity,maxAcceleration,timeToApogee,flightTime,"
              "groundHitVelocity\tview=status");
    EXPECT_EQ(all->menu(),
              "Status[x](disabled)\tWarnings[ ]\tName[ ]\tConfiguration[ ]\tSim. Stepper[ ]\t"
              "Velocity off rod[ ]\tApogee[ ]\tVelocity at deployment[ ]\tOptimum delay[ ]\t"
              "Max. velocity[ ]\tMax. acceleration[ ]\tTime to apogee[ ]\tFlight time[ ]\t"
              "Ground hit velocity[ ]\t---\t"
              "Save as default{Save the selected visibility settings for all new designs}");
    all->chooser().setColumnVisible(Column::NAME, true);
    EXPECT_EQ(all->state(),
              "docpref=warnings,configuration,simulationStepper,launchRodVelocity,apogee,"
              "deploymentVelocity,optimumCoastTime,maxVelocity,maxAcceleration,timeToApogee,"
              "flightTime,groundHitVelocity\tsaved=false\thidden=warnings,configuration,"
              "simulationStepper,launchRodVelocity,apogee,deploymentVelocity,optimumCoastTime,"
              "maxVelocity,maxAcceleration,timeToApogee,flightTime,groundHitVelocity\t"
              "view=status,name");
}

TEST_F(SimulationColumnVisibilityTest, KeepsOneColumnShown)
{
    // The probe's COLS "hide all but groundHitVelocity" to "then show maxVelocity" and its MENU
    // "one column".
    InMemoryPreferences                application;
    const std::unique_ptr<ChosenTable> table = simpleModelRocket(application);
    ASSERT_NE(table, nullptr);
    for (const Column column : SimulationColumns::kAllColumns)
    {
        if (column != Column::GROUND_HIT_VELOCITY)
        {
            table->chooser().setColumnVisible(column, false);
        }
    }
    const std::string oneColumn =
        "docpref=simulationStepper,status,warnings,name,configuration,launchRodVelocity,apogee,"
        "deploymentVelocity,optimumCoastTime,maxVelocity,maxAcceleration,timeToApogee,"
        "flightTime\tsaved=false\thidden=simulationStepper,status,warnings,name,configuration,"
        "launchRodVelocity,apogee,deploymentVelocity,optimumCoastTime,maxVelocity,"
        "maxAcceleration,timeToApogee,flightTime\tview=groundHitVelocity";
    EXPECT_EQ(table->state(), oneColumn);
    EXPECT_EQ(table->menu(),
              "Status[ ]\tWarnings[ ]\tName[ ]\tConfiguration[ ]\tSim. Stepper[ ]\t"
              "Velocity off rod[ ]\tApogee[ ]\tVelocity at deployment[ ]\tOptimum delay[ ]\t"
              "Max. velocity[ ]\tMax. acceleration[ ]\tTime to apogee[ ]\tFlight time[ ]\t"
              "Ground hit velocity[x](disabled)\t---\t"
              "Save as default{Save the selected visibility settings for all new designs}");
    table->chooser().setColumnVisible(Column::GROUND_HIT_VELOCITY, false);
    EXPECT_EQ(table->state(), oneColumn);
    table->chooser().setColumnVisible(Column::MAX_VELOCITY, true);
    EXPECT_EQ(table->state(),
              "docpref=simulationStepper,status,warnings,name,configuration,launchRodVelocity,"
              "apogee,deploymentVelocity,optimumCoastTime,maxAcceleration,timeToApogee,"
              "flightTime\tsaved=false\thidden=simulationStepper,status,warnings,name,"
              "configuration,launchRodVelocity,apogee,deploymentVelocity,optimumCoastTime,"
              "maxAcceleration,timeToApogee,flightTime\tview=maxVelocity,groundHitVelocity");
}

TEST_F(SimulationColumnVisibilityTest, ShowsAColumnAgainAmongTheShownColumnsAsOpenRocket)
{
    // The probe's COLS "move apogee to the left" to "hide and show warnings".
    InMemoryPreferences                application;
    const std::unique_ptr<ChosenTable> table = simpleModelRocket(application);
    ASSERT_NE(table, nullptr);
    SimulationColumnVisibility& chooser = table->chooser();
    table->table().moveColumn(Column::APOGEE, 0);
    EXPECT_EQ(table->state(),
              "docpref=<unset>\tsaved=true\thidden=simulationStepper\tview=apogee,status,warnings,"
              "name,configuration,launchRodVelocity,deploymentVelocity,optimumCoastTime,"
              "maxVelocity,maxAcceleration,timeToApogee,flightTime,groundHitVelocity");
    chooser.setColumnVisible(Column::NAME, false);
    EXPECT_EQ(table->state(),
              "docpref=simulationStepper,name\tsaved=false\thidden=simulationStepper,name\t"
              "view=apogee,status,warnings,configuration,launchRodVelocity,deploymentVelocity,"
              "optimumCoastTime,maxVelocity,maxAcceleration,timeToApogee,flightTime,"
              "groundHitVelocity");
    chooser.setColumnVisible(Column::NAME, true);
    EXPECT_EQ(table->state(),
              "docpref=simulationStepper\tsaved=false\thidden=simulationStepper\tview=apogee,"
              "status,name,warnings,configuration,launchRodVelocity,deploymentVelocity,"
              "optimumCoastTime,maxVelocity,maxAcceleration,timeToApogee,flightTime,"
              "groundHitVelocity");
    const QHeaderView& header = *table->table().view().horizontalHeader();
    table->table().moveColumn(Column::STATUS, header.count() - 1);
    EXPECT_EQ(table->table().visibleIds(),
              "apogee,name,warnings,configuration,launchRodVelocity,deploymentVelocity,"
              "optimumCoastTime,maxVelocity,maxAcceleration,timeToApogee,flightTime,"
              "groundHitVelocity,status");
    chooser.setColumnVisible(Column::SIMULATION_STEPPER, true);
    EXPECT_EQ(table->state(),
              "docpref=\tsaved=false\thidden=\tview=apogee,name,warnings,configuration,"
              "simulationStepper,launchRodVelocity,deploymentVelocity,optimumCoastTime,"
              "maxVelocity,maxAcceleration,timeToApogee,flightTime,groundHitVelocity,status");
    chooser.setColumnVisible(Column::WARNINGS, false);
    chooser.setColumnVisible(Column::WARNINGS, true);
    EXPECT_EQ(table->state(),
              "docpref=\tsaved=false\thidden=\tview=apogee,warnings,name,configuration,"
              "simulationStepper,launchRodVelocity,deploymentVelocity,optimumCoastTime,"
              "maxVelocity,maxAcceleration,timeToApogee,flightTime,groundHitVelocity,status");
}

TEST_F(SimulationColumnVisibilityTest, ShowsItsMenuOnTheHeader)
{
    InMemoryPreferences                application;
    const std::unique_ptr<ChosenTable> table = simpleModelRocket(application);
    ASSERT_NE(table, nullptr);
    const SimulationTableView& view   = table->table().view();
    const QHeaderView&         header = *view.horizontalHeader();
    EXPECT_EQ(header.contextMenuPolicy(), Qt::CustomContextMenu);
    table->table().show();
    {
        const Gui::Test::OffscreenPopupWarnings warnings;
        const QPoint      position = table->table().headerCentre(Column::APOGEE);
        QContextMenuEvent event(QContextMenuEvent::Mouse, position,
                                header.viewport()->mapToGlobal(position));
        QApplication::sendEvent(header.viewport(), &event);
        auto* menu = view.findChild<QMenu*>();
        ASSERT_NE(menu, nullptr);
        EXPECT_TRUE(menu->isVisible());
        EXPECT_EQ(ChosenTable::menuText(*menu), kNewDesignMenu);
        auto* apogee = menu->findChild<QAction*>(QStringLiteral("apogee"));
        ASSERT_NE(apogee, nullptr);
        apogee->trigger();
        menu->close();
        EXPECT_FALSE(view.isColumnVisible(Column::APOGEE));
        // Shown again: the same menu, filled anew.
        QApplication::sendEvent(header.viewport(), &event);
        EXPECT_EQ(view.findChildren<QMenu*>().size(), 1);
        std::string expected(kNewDesignMenu);
        expected.replace(expected.find("Apogee[x]"), 9, "Apogee[ ]");
        EXPECT_EQ(ChosenTable::menuText(*menu), expected);
        menu->close();
    }
    EXPECT_EQ(table->document().getDocumentPreferences().getString(kDocumentKey, "<unset>"),
              "simulationStepper,apogee");
}

}  // namespace

}  // namespace QtRocket::Gui
