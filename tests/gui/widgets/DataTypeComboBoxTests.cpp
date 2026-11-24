// The combo box of a flight data type (Java: GroupableAndSearchableComboBox over FlightDataTypes,
// as PlotTypeSelector and PlotPanel make it).
//
// Expectations marked "probe" are OpenRocket's own results, printed by the Java probe
// tools/openrocket-gui-pins/plot-widgets/probe/PlotWidgetsProbe.java (OpenRocket 5f164fd0e, JDK 17;
// its transcript is plot-widgets/hand-pinned.txt, which generate.sh checks): a PlotTypeSelector
// over FlightDataType.ALL_TYPES (FlightDataType::allTypes() here) with "Damping moment coefficient
// (aerodynamic)" selected, its type selector's model, groups popup (read by reflection: a group
// line is a submenu's title, " [checked]" when Java draws the check mark on it, and its items,
// "[x] " before a checked one) and filter() searches; the same after other selections; and
// smaller lists of types that compare equal.

#include "QtRocket/gui/widgets/DataTypeComboBox.h"

#include <array>
#include <cstddef>
#include <initializer_list>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include <QAction>
#include <QChar>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMenu>
#include <QObject>
#include <QPoint>
#include <QRegion>
#include <QSignalSpy>
#include <QSize>
#include <QString>
#include <QTest>
#include <QWidget>
#include <Qt>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/widgets/GroupedSearchPopup.h"
#include "QtRocket/simulation/FlightDataType.h"
#include "QtRocket/simulation/FlightDataTypeGroup.h"
#include "QtRocket/util/BugError.h"
#include "RecordingGuardHook.h"
#include "WidgetTestSupport.h"
#include "figure/RecordingPaintDevice.h"

namespace QtRocket::Gui
{

namespace
{

using DataTypeComboBoxTest = Test::GuiTest;
using Test::OffscreenPopupWarnings;
using Test::RecordingGuardHook;
using Test::showActive;
using Test::showActiveWith;
using Test::turnWheel;

/// probe: the groups popup over ALL_TYPES with "Damping moment coefficient (aerodynamic)"
/// selected.
constexpr std::array<std::string_view, 12> kAerodynamicGroups{
    "Time: Time",
    "Position and Motion: Altitude | Altitude above sea level | Vertical velocity | Total velocity "
    "| Vertical acceleration | Acceleration to the East | Acceleration to the North | X body "
    "acceleration | Y body acceleration | Z body acceleration | Total acceleration | Position East "
    "of launch | Position North of launch | Lateral distance | Lateral direction | Lateral "
    "velocity | Lateral acceleration | Latitude | Longitude",
    "Orientation: Angle of attack | Roll rate (Z) | Pitch rate (Y) | Yaw rate (X) | Vertical "
    "orientation (zenith) | Lateral orientation (azimuth)",
    "Mass and Inertia: Mass | Motor mass | Longitudinal moment of inertia | Rotational moment of "
    "inertia | Gravitational acceleration",
    "Stability: CP location | CG location | Stability margin calibers | Damping ratio | Natural "
    "frequency",
    "Thrust and Drag: Thrust | Thrust-to-weight ratio | Drag force | Drag coefficient (CD) | "
    "Friction drag coefficient (CD_friction) | Pressure drag coefficient (CD_pressure) | Base drag "
    "coefficient (CD_base) | Axial drag coefficient (CA)",
    "Coefficients [checked]: Normal force coefficient (CN) | Normal force coefficient derivative "
    "(CN\u03b1) | Pitch moment coefficient (Cm) | Yaw moment coefficient | Side force coefficient "
    "| Roll moment coefficient | Roll forcing coefficient | Roll damping coefficient | Pitch "
    "damping coefficient | Yaw damping coefficient | Damping moment coefficient | [x] Damping "
    "moment coefficient (aerodynamic) | Damping moment coefficient (propulsive) | Corrective "
    "moment coefficient",
    "Atmospheric Conditions: Wind velocity | Wind direction | Air temperature | Air pressure | Air "
    "density | Speed of sound",
    "Characteristic Numbers: Mach number | Reynolds number",
    "Reference Values: Reference length | Reference area",
    "Simulation Information: Simulation time step | Computation time",
    "Custom: Coriolis acceleration",
};

/// probe: the model of that type selector (its items: a HashSet's order).
constexpr std::array<std::string_view, 71> kAllTypesModel{
    "Reference area",
    "Wind velocity",
    "Total velocity",
    "Roll forcing coefficient",
    "Roll moment coefficient",
    "Natural frequency",
    "Reynolds number",
    "Mach number",
    "Air pressure",
    "Simulation time step",
    "Normal force coefficient (CN)",
    "Damping moment coefficient",
    "Lateral direction",
    "Damping moment coefficient (propulsive)",
    "Drag coefficient (CD)",
    "Longitude",
    "Side force coefficient",
    "Vertical velocity",
    "Pitch damping coefficient",
    "Z body acceleration",
    "Wind direction",
    "CG location",
    "Gravitational acceleration",
    "Corrective moment coefficient",
    "Base drag coefficient (CD_base)",
    "Pitch rate (Y)",
    "Thrust",
    "Speed of sound",
    "Coriolis acceleration",
    "Altitude",
    "Thrust-to-weight ratio",
    "Lateral velocity",
    "Lateral acceleration",
    "Vertical orientation (zenith)",
    "Reference length",
    "Latitude",
    "Mass",
    "Position East of launch",
    "Yaw rate (X)",
    "X body acceleration",
    "Air temperature",
    "Altitude above sea level",
    "Damping moment coefficient (aerodynamic)",
    "Y body acceleration",
    "Acceleration to the North",
    "Computation time",
    "Yaw damping coefficient",
    "Total acceleration",
    "Position North of launch",
    "Normal force coefficient derivative (CN\u03b1)",
    "Air density",
    "Longitudinal moment of inertia",
    "Pitch moment coefficient (Cm)",
    "Lateral distance",
    "Friction drag coefficient (CD_friction)",
    "Rotational moment of inertia",
    "Angle of attack",
    "Pressure drag coefficient (CD_pressure)",
    "Yaw moment coefficient",
    "Roll damping coefficient",
    "Roll rate (Z)",
    "Lateral orientation (azimuth)",
    "Damping ratio",
    "Motor mass",
    "Drag force",
    "Stability margin calibers",
    "CP location",
    "Vertical acceleration",
    "Time",
    "Axial drag coefficient (CA)",
    "Acceleration to the East",
};

/// probe: the lines of the groups popup that change when Altitude is selected instead.
constexpr std::string_view kAltitudePositionAndMotion{
    "Position and Motion [checked]: [x] Altitude | Altitude above sea level | Vertical velocity | "
    "Total velocity | Vertical acceleration | Acceleration to the East | Acceleration to the North "
    "| "
    "X body acceleration | Y body acceleration | Z body acceleration | Total acceleration | "
    "Position "
    "East of launch | Position North of launch | Lateral distance | Lateral direction | Lateral "
    "velocity | Lateral acceleration | Latitude | Longitude"};
constexpr std::string_view kUncheckedCoefficients{
    "Coefficients: Normal force coefficient (CN) | Normal force coefficient derivative (CN\u03b1) "
    "| "
    "Pitch moment coefficient (Cm) | Yaw moment coefficient | Side force coefficient | Roll moment "
    "coefficient | Roll forcing coefficient | Roll damping coefficient | Pitch damping coefficient "
    "| Yaw damping coefficient | Damping moment coefficient | Damping moment coefficient "
    "(aerodynamic) | Damping moment coefficient (propulsive) | Corrective moment coefficient"};
/// probe: ... and when Thrust Pressure Correction (no item of ALL_TYPES) is selected.
constexpr std::string_view kCheckedThrustAndDrag{
    "Thrust and Drag [checked]: Thrust | Thrust-to-weight ratio | Drag force | Drag coefficient "
    "(CD) | Friction drag coefficient (CD_friction) | Pressure drag coefficient (CD_pressure) | "
    "Base "
    "drag coefficient (CD_base) | Axial drag coefficient (CA)"};
/// probe: the Thrust and Drag line over ALL_TYPES and Thrust Pressure Correction (a fresh flight's
/// branch), Altitude selected.
constexpr std::string_view kFreshThrustAndDrag{
    "Thrust and Drag: Thrust Pressure Correction | Thrust | Thrust-to-weight ratio | Drag force | "
    "Drag coefficient (CD) | Friction drag coefficient (CD_friction) | Pressure drag coefficient "
    "(CD_pressure) | Base drag coefficient (CD_base) | Axial drag coefficient (CA)"};
/// probe: where its model holds Thrust Pressure Correction among the types of kAllTypesModel.
constexpr std::size_t kFreshThrustCorrectionIndex = 19;

/// A search of the popup: the text typed and the rows the search list shows.
struct Search
{
    std::string_view              text;
    std::vector<std::string_view> rows;
};

/// probe: the searches with "Damping moment coefficient (aerodynamic)" selected.
[[nodiscard]] std::vector<Search> aerodynamicSearches()
{
    return {
        {.text = "alt", .rows = {"Altitude", "Altitude above sea level"}},
        {.text = "ALT", .rows = {"Altitude", "Altitude above sea level"}},
        {.text = "coef",
         .rows = {"Axial drag coefficient (CA)", "Base drag coefficient (CD_base)",
                  "Corrective moment coefficient", "Damping moment coefficient",
                  "\u2713 Damping moment coefficient (aerodynamic)",
                  "Damping moment coefficient (propulsive)", "Drag coefficient (CD)",
                  "Friction drag coefficient (CD_friction)", "Normal force coefficient (CN)",
                  "Normal force coefficient derivative (CN\u03b1)", "Pitch damping coefficient",
                  "Pitch moment coefficient (Cm)", "Pressure drag coefficient (CD_pressure)",
                  "Roll damping coefficient", "Roll forcing coefficient", "Roll moment coefficient",
                  "Side force coefficient", "Yaw damping coefficient", "Yaw moment coefficient"}},
        {.text = "(",
         .rows = {"Axial drag coefficient (CA)", "Base drag coefficient (CD_base)",
                  "\u2713 Damping moment coefficient (aerodynamic)",
                  "Damping moment coefficient (propulsive)", "Drag coefficient (CD)",
                  "Friction drag coefficient (CD_friction)", "Lateral orientation (azimuth)",
                  "Normal force coefficient (CN)", "Normal force coefficient derivative (CN\u03b1)",
                  "Pitch moment coefficient (Cm)", "Pitch rate (Y)",
                  "Pressure drag coefficient (CD_pressure)", "Roll rate (Z)",
                  "Vertical orientation (zenith)", "Yaw rate (X)"}},
        {.text = "moment",
         .rows = {"Corrective moment coefficient", "Damping moment coefficient",
                  "\u2713 Damping moment coefficient (aerodynamic)",
                  "Damping moment coefficient (propulsive)", "Longitudinal moment of inertia",
                  "Pitch moment coefficient (Cm)", "Roll moment coefficient",
                  "Rotational moment of inertia", "Yaw moment coefficient"}},
        {.text = "zzz", .rows = {}},
        {.text = " ",
         .rows = {"Acceleration to the East",
                  "Acceleration to the North",
                  "Air density",
                  "Air pressure",
                  "Air temperature",
                  "Altitude above sea level",
                  "Angle of attack",
                  "Axial drag coefficient (CA)",
                  "Base drag coefficient (CD_base)",
                  "CG location",
                  "Computation time",
                  "Coriolis acceleration",
                  "Corrective moment coefficient",
                  "CP location",
                  "Damping moment coefficient",
                  "\u2713 Damping moment coefficient (aerodynamic)",
                  "Damping moment coefficient (propulsive)",
                  "Damping ratio",
                  "Drag coefficient (CD)",
                  "Drag force",
                  "Friction drag coefficient (CD_friction)",
                  "Gravitational acceleration",
                  "Lateral acceleration",
                  "Lateral direction",
                  "Lateral distance",
                  "Lateral orientation (azimuth)",
                  "Lateral velocity",
                  "Longitudinal moment of inertia",
                  "Mach number",
                  "Motor mass",
                  "Natural frequency",
                  "Normal force coefficient (CN)",
                  "Normal force coefficient derivative (CN\u03b1)",
                  "Pitch damping coefficient",
                  "Pitch moment coefficient (Cm)",
                  "Pitch rate (Y)",
                  "Position East of launch",
                  "Position North of launch",
                  "Pressure drag coefficient (CD_pressure)",
                  "Reference area",
                  "Reference length",
                  "Reynolds number",
                  "Roll damping coefficient",
                  "Roll forcing coefficient",
                  "Roll moment coefficient",
                  "Roll rate (Z)",
                  "Rotational moment of inertia",
                  "Side force coefficient",
                  "Simulation time step",
                  "Speed of sound",
                  "Stability margin calibers",
                  "Thrust-to-weight ratio",
                  "Total acceleration",
                  "Total velocity",
                  "Vertical acceleration",
                  "Vertical orientation (zenith)",
                  "Vertical velocity",
                  "Wind direction",
                  "Wind velocity",
                  "X body acceleration",
                  "Y body acceleration",
                  "Yaw damping coefficient",
                  "Yaw moment coefficient",
                  "Yaw rate (X)",
                  "Z body acceleration"}},
        {.text = "Mach", .rows = {"Mach number"}},
        {.text = "x",
         .rows = {"Axial drag coefficient (CA)", "X body acceleration", "Yaw rate (X)"}},
        {.text = "damping moment",
         .rows = {"Damping moment coefficient", "\u2713 Damping moment coefficient (aerodynamic)",
                  "Damping moment coefficient (propulsive)"}},
    };
}

/// The built-in type @p id.
[[nodiscard]] const FlightDataType* type(FlightDataTypeId id)
{
    return &FlightDataType::builtin(id);
}

/// FlightDataType.ALL_TYPES.
[[nodiscard]] std::vector<const FlightDataType*> allTypes()
{
    const auto all = FlightDataType::allTypes();
    return {all.begin(), all.end()};
}

/// The probe's line of each group submenu the popup of @p combo shows, in order.
[[nodiscard]] std::vector<std::string> groupLines(const DataTypeComboBox& combo)
{
    std::vector<std::string> lines;
    for (const QAction* action : combo.popupMenu().actions())
    {
        const QMenu* menu = action->menu();
        if (menu == nullptr || !action->isVisible())
        {
            continue;
        }
        QString line = action->text();
        if (action->isChecked())
        {
            line += QStringLiteral(" [checked]");
        }
        QString separator = QStringLiteral(": ");
        for (const QAction* item : menu->actions())
        {
            line += separator;
            if (item->isChecked())
            {
                line += QStringLiteral("[x] ");
            }
            line += item->text();
            separator = QStringLiteral(" | ");
        }
        lines.push_back(QtStrings::toUtf8(line));
    }
    return lines;
}

/// The rows of the search list of @p combo.
[[nodiscard]] std::vector<std::string> searchRows(const DataTypeComboBox& combo)
{
    std::vector<std::string> rows;
    const QListWidget&       list = combo.searchList();
    rows.reserve(static_cast<std::size_t>(list.count()));
    for (int row = 0; row < list.count(); ++row)
    {
        rows.push_back(QtStrings::toUtf8(list.item(row)->text()));
    }
    return rows;
}

/// The items of @p combo.
[[nodiscard]] std::vector<std::string> itemTexts(const DataTypeComboBox& combo)
{
    std::vector<std::string> texts;
    texts.reserve(static_cast<std::size_t>(combo.count()));
    for (int index = 0; index < combo.count(); ++index)
    {
        texts.push_back(QtStrings::toUtf8(combo.itemText(index)));
    }
    return texts;
}

/// @p texts as strings.
template <class Texts>
[[nodiscard]] std::vector<std::string> strings(const Texts& texts)
{
    return {texts.begin(), texts.end()};
}

/// kAerodynamicGroups with the line @p index replaced by @p line.
[[nodiscard]] std::vector<std::string> aerodynamicGroupsWith(
    std::initializer_list<std::pair<std::size_t, std::string_view>> lines)
{
    std::vector<std::string> groups = strings(kAerodynamicGroups);
    for (const auto& [index, line] : lines)
    {
        groups.at(index) = std::string(line);
    }
    return groups;
}

/// The action of the item @p text in the submenu of @p group of @p combo; null when none.
[[nodiscard]] QAction* itemAction(const DataTypeComboBox& combo, FlightDataTypeGroup group,
                                  const QString& text)
{
    for (QAction* action : combo.groupMenu(group).actions())
    {
        if (action->text() == text)
        {
            return action;
        }
    }
    return nullptr;
}

/// A shown, active window holding a data type combo box and a line edit to take the focus.
class Window
{
public:
    explicit Window(const std::vector<const FlightDataType*>& types)
      : m_combo(makeChild<DataTypeComboBox>(&m_window, types)),
        m_other(makeChild<QLineEdit>(&m_window)),
        m_active(showActiveWith(m_window, {m_combo, m_other}))
    {
    }

    [[nodiscard]] bool              isActive() const noexcept { return m_active; }
    [[nodiscard]] DataTypeComboBox& combo() const noexcept { return *m_combo; }
    [[nodiscard]] QLineEdit&        other() const noexcept { return *m_other; }

private:
    QWidget           m_window;
    DataTypeComboBox* m_combo{nullptr};
    QLineEdit*        m_other{nullptr};
    bool              m_active{false};
};

TEST_F(DataTypeComboBoxTest, TakesNoNullType)
{
    EXPECT_THROW(
        std::ignore = DataTypeComboBox({type(FlightDataTypeId::TYPE_TIME), nullptr}, nullptr),
        BugError);
}

TEST_F(DataTypeComboBoxTest, TheItemsAreInTheOrderOfOpenRocketsModel)
{
    // probe: the model of 71 types, a HashSet's order; nothing selected yet.
    const DataTypeComboBox combo(allTypes(), nullptr);
    EXPECT_EQ(itemTexts(combo), strings(kAllTypesModel));
    EXPECT_EQ(combo.types().size(), kAllTypesModel.size());
    EXPECT_EQ(combo.selectedType(), nullptr);
    EXPECT_EQ(combo.currentIndex(), -1);
    EXPECT_TRUE(combo.shownText().isEmpty());
    EXPECT_TRUE(combo.toolTip().isEmpty());
    EXPECT_EQ(combo.focusPolicy(), Qt::StrongFocus);
    EXPECT_FALSE(combo.isEditable());

    // probe: with Thrust Pressure Correction (a fresh flight's branch) the same order, that type
    // where its hash code puts it.
    std::vector<const FlightDataType*> fresh = allTypes();
    fresh.push_back(type(FlightDataTypeId::TYPE_THRUST_CORRECTION));
    const DataTypeComboBox   freshCombo(fresh, nullptr);
    std::vector<std::string> expected = strings(kAllTypesModel);
    expected.insert(expected.begin() + static_cast<std::ptrdiff_t>(kFreshThrustCorrectionIndex),
                    "Thrust Pressure Correction");
    EXPECT_EQ(itemTexts(freshCombo), expected);
}

TEST_F(DataTypeComboBoxTest, TypesEqualToAnEarlierOneAreLeftOut)
{
    // Java's HashSet keeps the first of equal types.
    const DataTypeComboBox combo(
        {type(FlightDataTypeId::TYPE_ALTITUDE), type(FlightDataTypeId::TYPE_TIME),
         type(FlightDataTypeId::TYPE_ALTITUDE)},
        nullptr);
    EXPECT_EQ(itemTexts(combo), (std::vector<std::string>{"Altitude", "Time"}));
}

TEST_F(DataTypeComboBoxTest, TheGroupsPopupAsOpenRocket)
{
    const Window window(allTypes());
    ASSERT_TRUE(window.isActive());
    DataTypeComboBox& combo = window.combo();
    combo.setSelectedType(type(FlightDataTypeId::TYPE_DAMPING_MOMENT_COEFF_AERODYNAMIC));
    const OffscreenPopupWarnings warnings;
    combo.showPopup();
    EXPECT_TRUE(combo.popupMenu().isVisible());
    // probe: placeholder='Enter the data type'
    EXPECT_EQ(combo.searchField().placeholderText(), QStringLiteral("Enter the data type"));
    EXPECT_TRUE(combo.searchField().hasFocus());
    EXPECT_FALSE(combo.isSearching());
    EXPECT_EQ(groupLines(combo), strings(kAerodynamicGroups));
    combo.hidePopup();
    EXPECT_FALSE(combo.popupMenu().isVisible());
}

TEST_F(DataTypeComboBoxTest, TheSearchAsOpenRocket)
{
    const Window window(allTypes());
    ASSERT_TRUE(window.isActive());
    DataTypeComboBox& combo = window.combo();
    combo.setSelectedType(type(FlightDataTypeId::TYPE_DAMPING_MOMENT_COEFF_AERODYNAMIC));
    const OffscreenPopupWarnings warnings;
    combo.showPopup();
    for (const auto& [text, rows] : aerodynamicSearches())
    {
        combo.searchField().clear();
        combo.searchField().insert(QtStrings::fromUtf8(text));
        EXPECT_TRUE(combo.isSearching()) << text;
        EXPECT_EQ(searchRows(combo), strings(rows)) << text;
        EXPECT_EQ(combo.searchList().currentRow(), rows.empty() ? -1 : 0) << text;
    }
}

TEST_F(DataTypeComboBoxTest, ASelectionTellsOfItAndMovesTheCheckMarks)
{
    // probe: setSelectedItem(Altitude): tooltip='Altitude', an item event; again: no event. The
    // popup then checks Position and Motion and Altitude.
    const Window window(allTypes());
    ASSERT_TRUE(window.isActive());
    DataTypeComboBox& combo = window.combo();
    combo.setSelectedType(type(FlightDataTypeId::TYPE_DAMPING_MOMENT_COEFF_AERODYNAMIC));
    const QSignalSpy changes(&combo, &DataTypeComboBox::selectedTypeChanged);
    combo.setSelectedType(type(FlightDataTypeId::TYPE_ALTITUDE));
    EXPECT_EQ(changes.count(), 1);
    EXPECT_EQ(combo.selectedType(), type(FlightDataTypeId::TYPE_ALTITUDE));
    EXPECT_EQ(combo.toolTip(), QStringLiteral("Altitude"));
    EXPECT_EQ(combo.currentText(), QStringLiteral("Altitude"));
    EXPECT_EQ(combo.shownText(), QStringLiteral("Altitude"));
    combo.setSelectedType(type(FlightDataTypeId::TYPE_ALTITUDE));
    EXPECT_EQ(changes.count(), 1);

    const OffscreenPopupWarnings warnings;
    combo.showPopup();
    EXPECT_EQ(groupLines(combo), aerodynamicGroupsWith({{1, kAltitudePositionAndMotion},
                                                        {6, kUncheckedCoefficients}}));
    QTest::keyClicks(&combo.searchField(), QStringLiteral("alt"));
    EXPECT_EQ(searchRows(combo),
              (std::vector<std::string>{"\u2713 Altitude", "Altitude above sea level"}));
}

TEST_F(DataTypeComboBoxTest, ATypeThatIsNoItemIsShownApart)
{
    // probe: setSelectedItem(Thrust Pressure Correction, not an item): index=-1, the tooltip its
    // name, an item event; the popup checks Thrust and Drag and no item.
    const Window window(allTypes());
    ASSERT_TRUE(window.isActive());
    DataTypeComboBox& combo = window.combo();
    combo.setSelectedType(type(FlightDataTypeId::TYPE_ALTITUDE));
    const QSignalSpy changes(&combo, &DataTypeComboBox::selectedTypeChanged);
    combo.setSelectedType(type(FlightDataTypeId::TYPE_THRUST_CORRECTION));
    EXPECT_EQ(changes.count(), 1);
    EXPECT_EQ(combo.currentIndex(), -1);
    EXPECT_EQ(combo.shownText(), QStringLiteral("Thrust Pressure Correction"));
    EXPECT_EQ(combo.toolTip(), QStringLiteral("Thrust Pressure Correction"));

    const OffscreenPopupWarnings warnings;
    combo.showPopup();
    EXPECT_EQ(groupLines(combo),
              aerodynamicGroupsWith({{5, kCheckedThrustAndDrag}, {6, kUncheckedCoefficients}}));
}

TEST_F(DataTypeComboBoxTest, NoTypeSelectedClearsTheTooltip)
{
    // probe: setSelectedItem(null): an item event (DESELECTED). Deviation: Java's tooltip keeps
    // the last name.
    DataTypeComboBox combo(allTypes(), nullptr);
    combo.setSelectedType(type(FlightDataTypeId::TYPE_TIME));
    const QSignalSpy changes(&combo, &DataTypeComboBox::selectedTypeChanged);
    combo.setSelectedType(nullptr);
    EXPECT_EQ(changes.count(), 1);
    EXPECT_EQ(combo.selectedType(), nullptr);
    EXPECT_EQ(combo.currentIndex(), -1);
    EXPECT_TRUE(combo.shownText().isEmpty());
    EXPECT_TRUE(combo.toolTip().isEmpty());
    combo.setSelectedType(nullptr);
    EXPECT_EQ(changes.count(), 1);
}

/// Checks the combo box of @p types, Time selected, against the probe's ties: Java's model is
/// "Altitude | Thrust Pressure Correction | Thrust | Time" and Thrust and Drag lists Thrust
/// Pressure Correction first (the model's order, once a type is selected), whatever the order of
/// @p types; and the searches 't' and 'THRUST'.
void expectTheTiesInTheModelsOrder(const std::vector<const FlightDataType*>& types)
{
    const Window window(types);
    ASSERT_TRUE(window.isActive());
    DataTypeComboBox& combo = window.combo();
    combo.setSelectedType(type(FlightDataTypeId::TYPE_TIME));
    EXPECT_EQ(itemTexts(combo), (std::vector<std::string>{"Altitude", "Thrust Pressure Correction",
                                                          "Thrust", "Time"}));
    const OffscreenPopupWarnings warnings;
    combo.showPopup();
    EXPECT_EQ(groupLines(combo),
              (std::vector<std::string>{"Time [checked]: [x] Time", "Position and Motion: Altitude",
                                        "Thrust and Drag: Thrust Pressure Correction | Thrust"}));
    QTest::keyClicks(&combo.searchField(), QStringLiteral("t"));
    EXPECT_EQ(searchRows(combo),
              (std::vector<std::string>{"Altitude", "Thrust", "Thrust Pressure Correction",
                                        "\u2713 Time"}));
    combo.searchField().clear();
    combo.searchField().insert(QStringLiteral("THRUST"));
    EXPECT_EQ(searchRows(combo),
              (std::vector<std::string>{"Thrust", "Thrust Pressure Correction"}));
    combo.hidePopup();
}

TEST_F(DataTypeComboBoxTest, TypesThatCompareEqualAreInTheModelsOrder)
{
    // probe: Thrust Pressure Correction, Altitude, Thrust, Time and the same the other way round.
    expectTheTiesInTheModelsOrder(
        {type(FlightDataTypeId::TYPE_THRUST_CORRECTION), type(FlightDataTypeId::TYPE_ALTITUDE),
         type(FlightDataTypeId::TYPE_THRUST_FORCE), type(FlightDataTypeId::TYPE_TIME)});
    expectTheTiesInTheModelsOrder(
        {type(FlightDataTypeId::TYPE_TIME), type(FlightDataTypeId::TYPE_THRUST_FORCE),
         type(FlightDataTypeId::TYPE_ALTITUDE), type(FlightDataTypeId::TYPE_THRUST_CORRECTION)});
}

TEST_F(DataTypeComboBoxTest, AFreshFlightsThrustTypesAreInTheModelsOrder)
{
    // probe: a fresh flight's types (ALL_TYPES and Thrust Pressure Correction), Altitude selected.
    std::vector<const FlightDataType*> fresh = allTypes();
    fresh.push_back(type(FlightDataTypeId::TYPE_THRUST_CORRECTION));
    const Window window(fresh);
    ASSERT_TRUE(window.isActive());
    window.combo().setSelectedType(type(FlightDataTypeId::TYPE_ALTITUDE));
    const OffscreenPopupWarnings warnings;
    window.combo().showPopup();
    EXPECT_EQ(groupLines(window.combo()), aerodynamicGroupsWith({{1, kAltitudePositionAndMotion},
                                                                 {5, kFreshThrustAndDrag},
                                                                 {6, kUncheckedCoefficients}}));
}

TEST_F(DataTypeComboBoxTest, AGroupItemSelectsTheType)
{
    const Window window(allTypes());
    ASSERT_TRUE(window.isActive());
    DataTypeComboBox& combo = window.combo();
    combo.setSelectedType(type(FlightDataTypeId::TYPE_TIME));
    const QSignalSpy             changes(&combo, &DataTypeComboBox::selectedTypeChanged);
    const OffscreenPopupWarnings warnings;
    combo.showPopup();
    QAction* mach = itemAction(combo, FlightDataTypeGroup::CHARACTERISTIC_NUMBERS,
                               QStringLiteral("Mach number"));
    ASSERT_NE(mach, nullptr);
    mach->trigger();
    EXPECT_EQ(combo.selectedType(), type(FlightDataTypeId::TYPE_MACH_NUMBER));
    EXPECT_EQ(changes.count(), 1);
    EXPECT_FALSE(combo.popupMenu().isVisible());
    EXPECT_EQ(combo.currentText(), QStringLiteral("Mach number"));
    EXPECT_EQ(combo.toolTip(), QStringLiteral("Mach number"));
}

TEST_F(DataTypeComboBoxTest, TheSearchListTakesTheKeysAndTheMouse)
{
    const Window window(allTypes());
    ASSERT_TRUE(window.isActive());
    DataTypeComboBox& combo = window.combo();
    combo.setSelectedType(type(FlightDataTypeId::TYPE_TIME));
    const OffscreenPopupWarnings warnings;
    combo.showPopup();
    QTest::keyClicks(&combo.searchField(), QStringLiteral("x"));
    // probe: search 'x': Axial drag coefficient (CA) | X body acceleration | Yaw rate (X)
    ASSERT_EQ(combo.searchList().count(), 3);
    QTest::keyClick(&combo.searchField(), Qt::Key_Down);
    QTest::keyClick(&combo.searchField(), Qt::Key_Return);
    EXPECT_EQ(combo.selectedType(), type(FlightDataTypeId::TYPE_ACCELERATION_BODYX));
    EXPECT_FALSE(combo.popupMenu().isVisible());

    combo.showPopup();
    QTest::keyClicks(&combo.searchField(), QStringLiteral("mach"));
    ASSERT_EQ(combo.searchList().count(), 1);
    Q_EMIT combo.searchList().itemClicked(combo.searchList().item(0));
    EXPECT_EQ(combo.selectedType(), type(FlightDataTypeId::TYPE_MACH_NUMBER));
    EXPECT_FALSE(combo.popupMenu().isVisible());

    combo.showPopup();
    QTest::keyClicks(&combo.searchField(), QStringLiteral("alt"));
    QTest::keyClick(&combo.searchField(), Qt::Key_Escape);
    EXPECT_FALSE(combo.popupMenu().isVisible());
    EXPECT_EQ(combo.selectedType(), type(FlightDataTypeId::TYPE_MACH_NUMBER));
}

TEST_F(DataTypeComboBoxTest, AChoiceOnTheClosedComboBoxSelectsTheType)
{
    DataTypeComboBox combo(allTypes(), nullptr);
    const QSignalSpy changes(&combo, &DataTypeComboBox::selectedTypeChanged);
    const int        altitude = combo.findText(QStringLiteral("Altitude"));
    ASSERT_GE(altitude, 0);
    combo.setCurrentIndex(altitude);
    Q_EMIT combo.activated(altitude);
    EXPECT_EQ(combo.selectedType(), type(FlightDataTypeId::TYPE_ALTITUDE));
    EXPECT_EQ(changes.count(), 1);
    Q_EMIT combo.activated(altitude);
    EXPECT_EQ(changes.count(), 1);
}

TEST_F(DataTypeComboBoxTest, TheWheelChoosesOnlyWithTheFocus)
{
    const Window window(allTypes());
    ASSERT_TRUE(window.isActive());
    DataTypeComboBox& combo = window.combo();
    combo.setSelectedType(type(FlightDataTypeId::TYPE_TIME));
    window.other().setFocus();
    EXPECT_FALSE(turnWheel(combo, -120));
    EXPECT_EQ(combo.selectedType(), type(FlightDataTypeId::TYPE_TIME));
}

TEST_F(DataTypeComboBoxTest, AListenersErrorIsReported)
{
    DataTypeComboBox combo(allTypes(), nullptr);
    combo.setObjectName(QStringLiteral("domainTypeSelector"));
    QObject::connect(&combo, &DataTypeComboBox::selectedTypeChanged, &combo,
                     [] { bug("a broken listener"); });
    const RecordingGuardHook hook;
    Q_EMIT combo.activated(0);
    ASSERT_EQ(hook.failures().size(), 1U);
    EXPECT_EQ(hook.failures().front().context, QStringLiteral("domainTypeSelector"));
}

/// The texts @p combo draws, painted into a RecordingPaintDevice.
[[nodiscard]] std::vector<QString> drawnTexts(DataTypeComboBox& combo)
{
    Test::RecordingPaintDevice device(combo.size());
    combo.render(&device, QPoint(), QRegion(), {});
    std::vector<QString> texts;
    for (const Test::RecordedPaint& paint : Test::joinTextRuns(device.records()))
    {
        if (paint.kind == Test::RecordedPaint::Kind::TEXT)
        {
            texts.push_back(paint.text);
        }
    }
    return texts;
}

/// Checks that @p combo draws its text, @p full, cut short with an ellipsis when it is too long
/// for the field, as Java's label cuts it (which it is unless the font is very narrow).
void expectCutShortToTheField(DataTypeComboBox& combo, const QString& full)
{
    const std::vector<QString> texts = drawnTexts(combo);
    ASSERT_EQ(texts.size(), 1U);
    const bool tooLong =
        combo.fontMetrics().horizontalAdvance(full) > DataTypeComboBox::kFixedWidth;
    EXPECT_EQ(texts.front().endsWith(QChar(0x2026)), tooLong) << texts.front().toStdString();
    EXPECT_TRUE(full.startsWith(tooLong ? texts.front().chopped(1) : texts.front()))
        << texts.front().toStdString();
}

TEST_F(DataTypeComboBoxTest, LongNamesKeepTheWidthAndShowInTheTooltip)
{
    // Port of PlotTypeSelectorTest.testDataTypeSelectorsUseFixedWidthAndFullNameTooltips, the
    // type selector's half (its PlotTypeSelector and PlotPanel half, the "gapright unrel" of the
    // rows, comes with them): a long name must not widen the selector, and the whole name is the
    // tooltip after every selection. probe: constraints typeSelector='width 200lp!, ...'
    // (DATA_TYPE_SELECTOR_WIDTH), tooltip='Damping moment coefficient (aerodynamic)', then
    // 'Altitude'; the same of PlotPanel's domainTypeSelector.
    const FlightDataType* longType  = type(FlightDataTypeId::TYPE_DAMPING_MOMENT_COEFF_AERODYNAMIC);
    const FlightDataType* shortType = type(FlightDataTypeId::TYPE_ALTITUDE);
    QWidget               window;
    auto*                 layout = makeChild<QHBoxLayout>(&window);
    auto* combo = makeChild<DataTypeComboBox>(&window, std::vector{longType, shortType});
    layout->addWidget(combo, 1);
    combo->setSelectedType(longType);
    EXPECT_EQ(combo->toolTip(), QStringLiteral("Damping moment coefficient (aerodynamic)"));
    EXPECT_EQ(combo->minimumWidth(), DataTypeComboBox::kFixedWidth);
    EXPECT_EQ(combo->maximumWidth(), DataTypeComboBox::kFixedWidth);
    window.resize(800, 100);
    ASSERT_TRUE(showActive(window));
    EXPECT_EQ(combo->width(), DataTypeComboBox::kFixedWidth);
    expectCutShortToTheField(*combo, QStringLiteral("Damping moment coefficient (aerodynamic)"));

    combo->setSelectedType(shortType);
    EXPECT_EQ(combo->toolTip(), QStringLiteral("Altitude"));
    EXPECT_EQ(combo->width(), DataTypeComboBox::kFixedWidth);
    EXPECT_EQ(drawnTexts(*combo), std::vector<QString>{QStringLiteral("Altitude")});
}

TEST_F(DataTypeComboBoxTest, TheGroupSubmenusAreTheCombosOwn)
{
    const DataTypeComboBox combo(allTypes(), nullptr);
    EXPECT_EQ(combo.groupMenu(FlightDataTypeGroup::TIME).title(), QStringLiteral("Time"));
    EXPECT_EQ(combo.groupMenu(FlightDataTypeGroup::CUSTOM).title(), QStringLiteral("Custom"));
    EXPECT_EQ(combo.popupMenu().parentWidget(), &combo);
}

}  // namespace

}  // namespace QtRocket::Gui
