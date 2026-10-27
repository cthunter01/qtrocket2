// The motor chooser's table under its filter panel against OpenRocket's own chooser: the scenarios
// of tests/core/motor/chooser/MotorChooserPins.h (what OpenRocket's real
// ThrustCurveMotorSelectionPanel shows for the bundled database, ChooserScenarios.rows(),
// customMotor(), hideUsed() and the inch scenarios of tools/openrocket-motor-chooser), replayed
// through the GUI: the filter panel's check list, sliders and boxes, the table's header and
// MotorTableModel's rows and texts, where the core's tests (motor_chooser_golden_tests.cpp) set
// the filter and the sort directly. The search field and the name column belong to the selection
// panel; their steps set the filter and the preference, then refresh the table, as it does.

#include <array>
#include <cstddef>
#include <format>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <QAbstractItemView>
#include <QCheckBox>
#include <QHeaderView>
#include <QListWidgetItem>
#include <QObject>
#include <QPoint>
#include <QPushButton>
#include <QRect>
#include <QString>
#include <QStringList>
#include <QTableView>
#include <QTest>
#include <QWidget>
#include <Qt>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "MotorChooserTestSupport.h"
#include "QtRocket/document/MotorChooserInput.h"
#include "QtRocket/file/motor/GeneralMotorLoader.h"
#include "QtRocket/gui/dialogs/motor/CheckList.h"
#include "QtRocket/gui/dialogs/motor/MotorFilterPanel.h"
#include "QtRocket/gui/dialogs/motor/MotorTableModel.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/widgets/RangeSlider.h"
#include "QtRocket/motor/Manufacturer.h"
#include "QtRocket/motor/Motor.h"
#include "QtRocket/motor/ThrustCurveMotor.h"
#include "QtRocket/motor/ThrustCurveMotorSet.h"
#include "QtRocket/motor/ThrustCurveMotorSetDatabase.h"
#include "QtRocket/motor/chooser/MotorRowFilter.h"
#include "QtRocket/motor/chooser/MotorTableSorter.h"
#include "QtRocket/motor/chooser/ThrustCurveMotorColumns.h"
#include "QtRocket/motor/chooser/ThrustCurveMotorSelection.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/MotorMount.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"
#include "TestPaths.h"
#include "motor/TestMotorDatabase.h"
#include "motor/chooser/MotorChooserPins.h"
#include "unit/DefaultUnitsGuard.h"
#include "widgets/WidgetTestSupport.h"

namespace QtRocket::Gui
{

namespace
{

using MotorTableGolden = Test::GuiTest;
using Test::BodyTubeMount;
using Test::InnerTubeMount;
using Test::motorTableMd5;
using Test::motorTableRows;
using Test::showActiveWith;
using Test::sortKeysText;
namespace Pins = QtRocket::Test;

/// The chooser's table and filter panel over the bundled database, in a window: the table follows
/// the panel's filterChanged(), its view's header toggles the sort keys.
class Chooser
{
public:
    Chooser()
      : m_selection(QtRocket::Test::bundledMotorDatabase(), m_preferences),
        m_panel(makeChild<MotorFilterPanel>(&m_window, m_selection, m_preferences)),
        m_model(m_selection, m_preferences, nullptr),
        m_view(makeChild<QTableView>(&m_window))
    {
        m_view->setModel(&m_model);
        m_view->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_view->setSelectionMode(QAbstractItemView::SingleSelection);
        m_model.connectHeader(*m_view->horizontalHeader());
        QObject::connect(m_panel, &MotorFilterPanel::filterChanged, &m_model,
                         [this] { m_model.refresh(); });
    }

    [[nodiscard]] QWidget&                   window() noexcept { return m_window; }
    [[nodiscard]] InMemoryPreferences&       preferences() noexcept { return m_preferences; }
    [[nodiscard]] ThrustCurveMotorSelection& selection() noexcept { return m_selection; }
    [[nodiscard]] MotorFilterPanel&          panel() noexcept { return *m_panel; }
    [[nodiscard]] MotorTableModel&           model() noexcept { return m_model; }
    [[nodiscard]] QTableView&                view() noexcept { return *m_view; }

    /// The panel's child of the type T named @p name; null when there is none.
    template <class T>
    [[nodiscard]] T* child(const QString& name) const
    {
        T* found = m_panel->findChild<T*>(name);
        EXPECT_NE(found, nullptr) << QtStrings::toUtf8(name);
        return found;
    }

    /// Clicks the header of @p column.
    void clickHeader(ThrustCurveMotorColumns column)
    {
        const QHeaderView* header  = m_view->horizontalHeader();
        const int          section = static_cast<int>(column);
        m_view->scrollTo(m_model.index(0, section));
        QTest::mouseClick(
            header->viewport(), Qt::LeftButton, {},
            QPoint(header->sectionViewportPosition(section) + (header->sectionSize(section) / 2),
                   header->height() / 2));
    }

    /// Opens the chooser for the motor configuration @p fcid of @p mount: the selection, then
    /// the filter panel (ThrustCurveMotorSelectionPanel.setMotorMountAndConfig()).
    void open(const MotorMount& mount, const FlightConfigurationId& fcid)
    {
        m_selection.setMotorMountAndConfig(MotorChooserInput::fromMount(mount, fcid));
        m_panel->setMotorMount(&mount);
    }

    /// The search field's text @p text (the selection panel's DocumentListener).
    void search(std::string_view text)
    {
        m_selection.getRowFilter().setSearchText(text);
        m_model.refresh();
    }

private:
    InMemoryPreferences       m_preferences;
    ThrustCurveMotorSelection m_selection;
    QWidget                   m_window;
    MotorFilterPanel*         m_panel;
    MotorTableModel           m_model;
    QTableView*               m_view;
};

/// The pin of the scenario @p name, or null.
[[nodiscard]] const Pins::MotorChooserRowScenarioPin* scenarioPin(std::string_view name)
{
    for (const Pins::MotorChooserRowScenarioPin& pin : Pins::kMotorChooserRowScenarioPins)
    {
        if (pin.name == name)
        {
            return &pin;
        }
    }
    return nullptr;
}

/// The lines of @p text, without their line ends.
[[nodiscard]] std::vector<std::string> linesOf(const std::string& text)
{
    std::vector<std::string> lines;
    std::size_t              start = 0;
    while (start < text.size())
    {
        const std::size_t end = text.find('\n', start);
        lines.push_back(text.substr(start, end - start));
        start = end + 1;
    }
    return lines;
}

/// How the table of @p chooser differs from OpenRocket's in the scenario @p name ("" when it does
/// not): the rows, the sort keys, the MD5 of all rows and the first rows.
[[nodiscard]] std::string tableDifferences(Chooser& chooser, std::string_view name)
{
    const Pins::MotorChooserRowScenarioPin* pin = scenarioPin(name);
    if (pin == nullptr)
    {
        return std::format("no pin for the scenario {}\n", name);
    }
    const MotorTableModel& model = chooser.model();
    std::string            differences;
    if (model.rowCount() != static_cast<int>(pin->rows))
    {
        differences +=
            std::format("{}: {} rows, OpenRocket {}\n", name, model.rowCount(), pin->rows);
    }
    if (const std::string keys = sortKeysText(chooser.selection().getSorter());
        keys != pin->sortKeys)
    {
        differences += std::format("{}: sort keys {}, OpenRocket {}\n", name, keys, pin->sortKeys);
    }
    if (const std::string md5 = motorTableMd5(model); md5 != pin->md5)
    {
        differences += std::format("{}: rows MD5 {}, OpenRocket {}\n", name, md5, pin->md5);
    }
    const std::vector<std::string> rows = linesOf(motorTableRows(model));
    for (std::size_t row = 0; row < pin->firstRowCount && row < rows.size(); ++row)
    {
        const std::string_view expected = Pins::kMotorChooserRowPins.at(pin->firstRow + row);
        if (rows[row] != expected)
        {
            differences += std::format("{}: row {} is [{}], OpenRocket's [{}]\n", name, row,
                                       rows[row], expected);
        }
    }
    // The header shows the primary key's arrow.
    const QHeaderView* header = chooser.view().horizontalHeader();
    if (const auto& keys = chooser.selection().getSorter().getSortKeys();
        !keys.empty() && header->sortIndicatorSection() != static_cast<int>(keys.front().column))
    {
        differences += std::format("{}: the header shows the arrow of another column\n", name);
    }
    return differences;
}

/// How the filter's limits differ from OpenRocket's after the step @p name.
[[nodiscard]] std::string filterDifferences(const ThrustCurveMotorSelection& selection,
                                            std::string_view                 name)
{
    for (const Pins::MotorChooserFilterPin& pin : Pins::kMotorChooserFilterPins)
    {
        if (pin.name != name)
        {
            continue;
        }
        const MotorRowFilter&       filter = selection.getRowFilter();
        const std::optional<double> minimumDiameter =
            pin.hasMinimumDiameter ? std::optional(pin.minimumDiameter) : std::nullopt;
        const std::optional<double> maximumDiameter =
            pin.hasMaximumDiameter ? std::optional(pin.maximumDiameter) : std::nullopt;
        if (filter.getMinimumDiameter() != minimumDiameter ||
            filter.getMaximumDiameter() != maximumDiameter ||
            filter.getMinimumLength() != pin.minimumLength ||
            filter.getMaximumLength() != pin.maximumLength)
        {
            return std::format("{}: the filter's limits differ from OpenRocket's\n", name);
        }
        return "";
    }
    return std::format("no filter pin for {}\n", name);
}

/// The index of the manufacturer @p simpleName in the panel's list.
[[nodiscard]] int manufacturerIndex(const ThrustCurveMotorSelection& selection,
                                    std::string_view                 simpleName)
{
    const std::vector<const Manufacturer*>& all = selection.getManufacturers();
    for (std::size_t index = 0; index < all.size(); ++index)
    {
        if (all[index]->getSimpleName() == simpleName)
        {
            return static_cast<int>(index);
        }
    }
    return -1;
}

/// The set of the bundled database whose manufacturer's display name and designation are
/// @p manufacturer and @p designation.
[[nodiscard]] const ThrustCurveMotorSet& findSet(std::string_view manufacturer,
                                                 std::string_view designation)
{
    for (const ThrustCurveMotorSet& set : QtRocket::Test::bundledMotorDatabase().getMotorSets())
    {
        if (set.getManufacturer()->getDisplayName() == manufacturer &&
            set.getDesignation() == designation)
        {
            return set;
        }
    }
    ADD_FAILURE() << "no set " << manufacturer << " " << designation;
    return QtRocket::Test::bundledMotorDatabase().getMotorSets().front();
}

/// The search texts of the probe and their scenarios.
struct Search
{
    std::string_view text;
    std::string_view scenario;
};
constexpr std::array<Search, 12> kSearches{{
    {.text = "H12", .scenario = "search [H12]"},
    {.text = "aerotech h", .scenario = "search [aerotech h]"},
    {.text = "29", .scenario = "search [29]"},
    {.text = "38 mm", .scenario = "search [38 mm]"},
    {.text = "estes c6", .scenario = "search [estes c6]"},
    {.text = "Ns", .scenario = "search [Ns]"},
    {.text = "blue", .scenario = "search [blue]"},
    {.text = "0.1", .scenario = "search [0.1]"},
    {.text = "  ", .scenario = "search [  ]"},
    {.text = "j", .scenario = "search [j]"},
    {.text = "rms", .scenario = "search [rms]"},
    {.text     = " \x01"
                 "Estes\x02\tC6 ",
     .scenario = "search [ <U+0001>Estes<U+0002><U+0009>C6 ]"},
}};

/// ChooserScenarios.rows(), up to the mount: the hide-unavailable box, two sorts, one
/// manufacturer, the impulse slider and the searches.
[[nodiscard]] std::string filterAndSortDifferences(Chooser& chooser)
{
    std::string differences = tableDifferences(chooser, "default");
    chooser.panel().setHideUnavailable(false);
    differences += tableDifferences(chooser, "hideUnavailableOff");
    chooser.model().toggleSortOrder(static_cast<int>(ThrustCurveMotorColumns::CASEINFO));
    differences += tableDifferences(chooser, "sortedByCase");
    chooser.model().toggleSortOrder(static_cast<int>(ThrustCurveMotorColumns::MANUFACTURER));
    differences += tableDifferences(chooser, "sortedByManufacturer");
    chooser.selection().getSorter().setSortKeys(MotorTableSorter::kInitialSortKeys);
    chooser.model().refresh();

    // "Clear All", then a click on Estes.
    auto* list = chooser.child<CheckList>(QStringLiteral("manufacturerCheckList"));
    QTest::mouseClick(chooser.child<QPushButton>(QStringLiteral("clearMotors")), Qt::LeftButton);
    const int estes = manufacturerIndex(chooser.selection(), "Estes");
    list->scrollToItem(list->item(estes));
    QTest::mouseClick(list->viewport(), Qt::LeftButton, {},
                      list->visualItemRect(list->item(estes)).center());
    differences += tableDifferences(chooser, "onlyEstes");
    QTest::mouseClick(chooser.child<QPushButton>(QStringLiteral("selectMotors")), Qt::LeftButton);

    // The impulse slider: D to F, then A to O (which filters nothing).
    auto* impulse = chooser.child<RangeSlider>(QStringLiteral("impulseSlider"));
    impulse->setValueAt(0, 3);
    impulse->setValueAt(1, 5);
    differences += tableDifferences(chooser, "impulseDToF");
    impulse->setValueAt(0, 0);
    impulse->setValueAt(1, 14);
    differences += tableDifferences(chooser, "impulseAToO");

    for (const Search& search : kSearches)
    {
        chooser.search(search.text);
        differences += tableDifferences(chooser, search.scenario);
    }
    chooser.search("");
    return differences;
}

/// ChooserScenarios.rows() from the mount on: the limit boxes, the header clicks and the name
/// column.
[[nodiscard]] std::string mountAndHeaderDifferences(Chooser& chooser)
{
    InnerTubeMount mount;
    std::string    differences;
    if (mount.mount().getMotorMountDiameter() != Pins::kMotorChooserMountDiameter ||
        mount.mount().getLength() != Pins::kMotorChooserMountLength)
    {
        differences += "the mount's dimensions differ from OpenRocket's\n";
    }
    chooser.open(mount.mount(), mount.fcid());
    differences += tableDifferences(chooser, "mountNoLimits");
    differences += filterDifferences(chooser.selection(), "mountNoLimits");
    auto* limitDiameter = chooser.child<QCheckBox>(QStringLiteral("limitDiameterCheckBox"));
    auto* limitLength   = chooser.child<QCheckBox>(QStringLiteral("limitByLengthCheckBox"));
    limitDiameter->click();
    differences += tableDifferences(chooser, "mountLimitDiameter");
    differences += filterDifferences(chooser.selection(), "mountLimitDiameter");
    limitLength->click();
    differences += tableDifferences(chooser, "mountLimitDiameterAndLength");
    differences += filterDifferences(chooser.selection(), "mountLimitDiameterAndLength");
    limitLength->click();
    limitDiameter->click();
    differences += tableDifferences(chooser, "mountLimitsOff");
    differences += filterDifferences(chooser.selection(), "mountLimitsOff");

    chooser.clickHeader(ThrustCurveMotorColumns::NAME);
    differences += tableDifferences(chooser, "clickName");
    chooser.clickHeader(ThrustCurveMotorColumns::NAME);
    differences += tableDifferences(chooser, "clickNameAgain");
    chooser.clickHeader(ThrustCurveMotorColumns::MANUFACTURER);
    differences += tableDifferences(chooser, "clickManufacturer");
    chooser.clickHeader(ThrustCurveMotorColumns::CASEINFO);
    differences += tableDifferences(chooser, "clickCase");
    chooser.preferences().setMotorNameColumn(false);
    chooser.model().refresh();
    differences += tableDifferences(chooser, "commonNameColumn");
    return differences;
}

/// The curve of tests/data/motors/test1.eng, which no set of the database holds.
[[nodiscard]] std::shared_ptr<const ThrustCurveMotor> test1Motor()
{
    const auto bytes = readFile(QtRocket::Test::testDataDir() / "motors" / "test1.eng");
    if (!bytes)
    {
        ADD_FAILURE() << bytes.error().message;
        return nullptr;
    }
    const GeneralMotorLoader loader;
    auto                     builders = loader.load(*bytes, "test1.eng");
    if (!builders || builders->empty())
    {
        ADD_FAILURE() << "test1.eng holds no motor";
        return nullptr;
    }
    Result<ThrustCurveMotor> motor = builders->front().build();
    if (!motor)
    {
        ADD_FAILURE() << motor.error().message;
        return nullptr;
    }
    return std::make_shared<const ThrustCurveMotor>(std::move(*motor));
}

TEST_F(MotorTableGolden, FiltersAndSortsAsOpenRocketsChooser)
{
    Chooser chooser;
    ASSERT_TRUE(showActiveWith(chooser.window(), {&chooser.panel(), &chooser.view()}));
    EXPECT_EQ(filterAndSortDifferences(chooser), "");
    EXPECT_EQ(mountAndHeaderDifferences(chooser), "");
}

TEST_F(MotorTableGolden, AddsAMotorOfNoSetAsOpenRocket)
{
    Chooser    chooser;
    const auto motor = test1Motor();
    ASSERT_NE(motor, nullptr);
    BodyTubeMount               mount;
    const FlightConfigurationId fcid = mount.configure(motor, 3.3);
    chooser.open(mount.mount(), fcid);
    EXPECT_EQ(tableDifferences(chooser, "customMotor"), "");
    const ThrustCurveMotorSet* own = chooser.selection().getSelectedMotorSet();
    ASSERT_NE(own, nullptr);
    EXPECT_GE(chooser.model().rowOf(*own), 0);
}

/// "Hide motors already used in the mount": the first mount's motors, then the second mount's
/// alone (QtRocket fixes M3's bug 9: OpenRocket keeps hiding the first mount's motors, its
/// hideUsedSecondMount).
TEST_F(MotorTableGolden, HidesTheMotorsOfTheMountOnly)
{
    Chooser                     chooser;
    BodyTubeMount               first;
    const FlightConfigurationId a = first.configure(findSet("Estes", "C6").getMotors()[1], 5);
    first.configure(findSet("Estes", "D12").getMotors()[0], Motor::kPluggedDelay);
    BodyTubeMount               second;
    const FlightConfigurationId c =
        second.configure(findSet("AeroTech", "H128W").getMotors()[0], 6);

    chooser.open(first.mount(), a);
    chooser.child<QCheckBox>(QStringLiteral("hideUsedBox"))->click();
    EXPECT_EQ(tableDifferences(chooser, "hideUsed"), "");
    chooser.open(second.mount(), c);
    EXPECT_EQ(tableDifferences(chooser, "hideUsedOnlySecondMount"), "");
    EXPECT_NE(tableDifferences(chooser, "hideUsedSecondMount"), "");
}

TEST_F(MotorTableGolden, ShowsInchesAsOpenRocket)
{
    const QtRocket::Test::DefaultUnitsGuard units;
    ASSERT_TRUE(unitGroup(UnitGroupId::MOTOR_DIMENSIONS).setDefaultUnit("in"));
    Chooser chooser;
    EXPECT_EQ(tableDifferences(chooser, "inchNoFilter"), "");
    chooser.search("1.14");
    EXPECT_EQ(tableDifferences(chooser, "inchSearch [1.14]"), "");
    // The diameter slider in inches.
    const auto*    diameter = chooser.child<RangeSlider>(QStringLiteral("diameterSlider"));
    InnerTubeMount mount;
    chooser.open(mount.mount(), mount.fcid());
    ASSERT_EQ(diameter->labels().size(), Pins::kMotorChooserDiameterLabelsInches.size());
    for (std::size_t stop = 0; stop < Pins::kMotorChooserDiameterLabelsInches.size(); ++stop)
    {
        EXPECT_EQ(QtStrings::toUtf8(diameter->labels().at(static_cast<int>(stop))),
                  Pins::kMotorChooserDiameterLabelsInches.at(stop));
    }
}

TEST_F(MotorTableGolden, ListsTheManufacturersAndLabelsTheDiametersAsOpenRocket)
{
    const Chooser chooser;
    const auto*   list = chooser.child<CheckList>(QStringLiteral("manufacturerCheckList"));
    ASSERT_EQ(static_cast<std::size_t>(list->count()), Pins::kMotorChooserManufacturerPins.size());
    for (std::size_t index = 0; index < Pins::kMotorChooserManufacturerPins.size(); ++index)
    {
        EXPECT_EQ(QtStrings::toUtf8(list->item(static_cast<int>(index))->text()),
                  Pins::kMotorChooserManufacturerPins.at(index).displayName);
    }
    const auto* diameter = chooser.child<RangeSlider>(QStringLiteral("diameterSlider"));
    ASSERT_EQ(diameter->labels().size(), Pins::kMotorChooserDiameterLabelsMillimetres.size());
    for (std::size_t stop = 0; stop < Pins::kMotorChooserDiameterLabelsMillimetres.size(); ++stop)
    {
        EXPECT_EQ(QtStrings::toUtf8(diameter->labels().at(static_cast<int>(stop))),
                  Pins::kMotorChooserDiameterLabelsMillimetres.at(stop));
    }
}

}  // namespace

}  // namespace QtRocket::Gui
