// The motor chooser's "Filter Motors" tab (Java: MotorFilterPanel). OpenRocket has no JUnit test
// of it (MotorRowFilterTest, ported in tests/core/motor/chooser, tests the filter alone); these
// tests follow MotorFilterPanel.java's listeners, over the eleven sets of smallMotorDatabase(). The
// table under the panel's filters for the whole database is compared with OpenRocket's own in
// motor_table_golden_tests.cpp.

#include "QtRocket/gui/dialogs/motor/MotorFilterPanel.h"

#include <cstddef>
#include <format>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <QChar>
#include <QCheckBox>
#include <QCoreApplication>
#include <QDialog>
#include <QEvent>
#include <QGroupBox>
#include <QLabel>
#include <QPoint>
#include <QPushButton>
#include <QRect>
#include <QSignalSpy>
#include <QString>
#include <QTest>
#include <QVBoxLayout>
#include <QWidget>
#include <Qt>
#include <QtTypes>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "MotorChooserTestSupport.h"
#include "QtRocket/document/MotorChooserInput.h"
#include "QtRocket/gui/binding/DoubleBinding.h"
#include "QtRocket/gui/dialogs/motor/CheckList.h"
#include "QtRocket/gui/dialogs/motor/MotorTableModel.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/widgets/RangeSlider.h"
#include "QtRocket/gui/widgets/UnitSelector.h"
#include "QtRocket/gui/widgets/UnitSpinBox.h"
#include "QtRocket/motor/Manufacturer.h"
#include "QtRocket/motor/MotorPreferences.h"
#include "QtRocket/motor/ThrustCurveMotorSet.h"
#include "QtRocket/motor/ThrustCurveMotorSetDatabase.h"
#include "QtRocket/motor/chooser/ImpulseClass.h"
#include "QtRocket/motor/chooser/MotorRowFilter.h"
#include "QtRocket/motor/chooser/ThrustCurveMotorSelection.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/preferences/PreferenceKeys.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Strings.h"
#include "RecordingGuardHook.h"
#include "motor/TestMotorDatabase.h"
#include "unit/DefaultUnitsGuard.h"
#include "widgets/WidgetTestSupport.h"

namespace QtRocket::Gui
{

namespace
{

using MotorFilterPanelTest = Test::GuiTest;
using Test::BodyTubeMount;
using Test::InnerTubeMount;
using Test::RecordingGuardHook;
using Test::showActiveWith;
using Test::typeInto;

/// The filter panel of a chooser over smallMotorDatabase() in a window, with its table, which
/// follows the panel's filterChanged() as the selection panel's table does.
class Panel
{
public:
    /// What the preferences hold before the chooser is made.
    using Prepare = std::function<void(InMemoryPreferences&)>;

    /// With the preferences @p prepare stores before the chooser is made.
    explicit Panel(const Prepare& prepare = nullptr)
      : m_database(QtRocket::Test::smallMotorDatabase()),
        m_selection(prepared(prepare), m_preferences),
        m_panel(makeChild<MotorFilterPanel>(&m_window, m_selection, m_preferences)),
        m_table(m_selection, m_preferences, nullptr),
        m_changes(m_panel, &MotorFilterPanel::filterChanged)
    {
        QObject::connect(m_panel, &MotorFilterPanel::filterChanged, &m_table,
                         [this] { m_table.refresh(); });
    }

    [[nodiscard]] QWidget&                   window() noexcept { return m_window; }
    [[nodiscard]] MotorFilterPanel&          panel() noexcept { return *m_panel; }
    [[nodiscard]] InMemoryPreferences&       preferences() noexcept { return m_preferences; }
    [[nodiscard]] ThrustCurveMotorSelection& selection() noexcept { return m_selection; }
    [[nodiscard]] MotorRowFilter&  filter() noexcept { return m_selection.getRowFilter(); }
    [[nodiscard]] MotorTableModel& table() noexcept { return m_table; }

    /// How many times filterChanged() came since the last call.
    [[nodiscard]] qsizetype takeChanges()
    {
        const qsizetype count = m_changes.count();
        m_changes.clear();
        return count;
    }

    /// The panel's child of the type T named @p name.
    template <class T>
    [[nodiscard]] T& child(const QString& name) const
    {
        T* found = m_panel->findChild<T*>(name);
        if (found == nullptr)
        {
            bug("The filter panel has no child " + QtStrings::toUtf8(name));
        }
        return *found;
    }

    [[nodiscard]] CheckList& manufacturers() const
    {
        return child<CheckList>(QStringLiteral("manufacturerCheckList"));
    }
    [[nodiscard]] RangeSlider& impulseSlider() const
    {
        return child<RangeSlider>(QStringLiteral("impulseSlider"));
    }
    [[nodiscard]] RangeSlider& diameterSlider() const
    {
        return child<RangeSlider>(QStringLiteral("diameterSlider"));
    }
    [[nodiscard]] RangeSlider& lengthSlider() const
    {
        return child<RangeSlider>(QStringLiteral("lengthSlider"));
    }
    [[nodiscard]] QCheckBox& limitDiameter() const
    {
        return child<QCheckBox>(QStringLiteral("limitDiameterCheckBox"));
    }
    [[nodiscard]] QCheckBox& limitLength() const
    {
        return child<QCheckBox>(QStringLiteral("limitByLengthCheckBox"));
    }
    [[nodiscard]] UnitSpinBox& minimumLength() const
    {
        return child<UnitSpinBox>(QStringLiteral("MinimumLength"));
    }
    [[nodiscard]] UnitSpinBox& maximumLength() const
    {
        return child<UnitSpinBox>(QStringLiteral("MaximumLength"));
    }

    /// The index of the manufacturer @p simpleName in the list.
    [[nodiscard]] int manufacturerIndex(std::string_view simpleName) const
    {
        const std::vector<const Manufacturer*>& all = m_selection.getManufacturers();
        for (std::size_t index = 0; index < all.size(); ++index)
        {
            if (all[index]->getSimpleName() == simpleName)
            {
                return static_cast<int>(index);
            }
        }
        ADD_FAILURE() << "no manufacturer " << simpleName;
        return -1;
    }

    /// The set of the database made by @p manufacturer (a display name) as @p designation.
    [[nodiscard]] const ThrustCurveMotorSet& set(std::string_view manufacturer,
                                                 std::string_view designation) const
    {
        for (const ThrustCurveMotorSet& candidate : m_database.getMotorSets())
        {
            if (candidate.getManufacturer()->getDisplayName() == manufacturer &&
                candidate.getDesignation() == designation)
            {
                return candidate;
            }
        }
        ADD_FAILURE() << "no set " << manufacturer << " " << designation;
        return m_database.getMotorSets().front();
    }

private:
    /// The database the chooser shows, after @p prepare stored its preferences.
    const ThrustCurveMotorSetDatabase& prepared(const Prepare& prepare)
    {
        if (prepare != nullptr)
        {
            prepare(m_preferences);
        }
        return m_database;
    }

    InMemoryPreferences         m_preferences;
    ThrustCurveMotorSetDatabase m_database;
    ThrustCurveMotorSelection   m_selection;
    QWidget                     m_window;
    MotorFilterPanel*           m_panel;
    MotorTableModel             m_table;
    QSignalSpy                  m_changes;
};

/// Stores both limit boxes on.
void storeLimitBoxesOn(InMemoryPreferences& preferences)
{
    MotorPreferences::setLimitDiameter(preferences, true);
    MotorPreferences::setLimitLength(preferences, true);
}

/// Stores Estes and Quest as excluded manufacturers.
void storeEstesAndQuestExcluded(InMemoryPreferences& preferences)
{
    const std::vector<const Manufacturer*> excluded{&Manufacturer::getManufacturer("Estes"),
                                                    &Manufacturer::getManufacturer("Quest")};
    MotorPreferences::setExcludedMotorManufacturers(preferences, excluded);
}

TEST_F(MotorFilterPanelTest, HasOpenRocketsRowsInTheirFirstState)
{
    Panel panel;
    EXPECT_EQ(panel.panel().objectName(), QStringLiteral("motorFilterPanel"));

    const auto& hideUsed = panel.child<QCheckBox>(QStringLiteral("hideUsedBox"));
    EXPECT_EQ(hideUsed.text(), QStringLiteral("Hide motors already used in the mount"));
    EXPECT_FALSE(hideUsed.isChecked());

    EXPECT_EQ(panel.child<QGroupBox>(QStringLiteral("manufacturerPanel")).title(),
              QStringLiteral("Manufacturer"));
    EXPECT_EQ(panel.child<QGroupBox>(QStringLiteral("impulsePanel")).title(),
              QStringLiteral("Total Impulse"));
    EXPECT_EQ(panel.child<QGroupBox>(QStringLiteral("dimensionsPanel")).title(),
              QStringLiteral("Motor Dimensions"));
    EXPECT_TRUE(panel.child<QGroupBox>(QStringLiteral("dimensionsPanel")).font().bold());
    EXPECT_FALSE(hideUsed.font().bold());
    EXPECT_EQ(panel.child<QPushButton>(QStringLiteral("clearMotors")).text(),
              QStringLiteral("Clear All"));
    EXPECT_EQ(panel.child<QPushButton>(QStringLiteral("selectMotors")).text(),
              QStringLiteral("Select All"));

    // Sorted by simple name, shown by display name, all checked.
    const CheckList& list = panel.manufacturers();
    ASSERT_EQ(list.count(), 5);
    EXPECT_EQ(list.item(0)->text(), QStringLiteral("AeroTech"));
    EXPECT_EQ(list.item(1)->text(), QStringLiteral("Cesaroni Technology Inc."));
    EXPECT_EQ(list.item(4)->text(), QStringLiteral("Quest"));
    EXPECT_EQ(list.checkedIndices(), (std::vector<int>{0, 1, 2, 3, 4}));

    const RangeSlider& impulse = panel.impulseSlider();
    EXPECT_EQ(impulse.minimum(), 0);
    EXPECT_EQ(impulse.maximum(), 14);
    EXPECT_EQ(impulse.valueAt(0), 0);
    EXPECT_EQ(impulse.valueAt(1), 14);
    EXPECT_EQ(impulse.tickInterval(), 1);
    ASSERT_EQ(impulse.labels().size(), 15U);
    EXPECT_EQ(impulse.labels().at(0), QStringLiteral("A"));
    EXPECT_EQ(impulse.labels().at(14), QStringLiteral("O"));

    EXPECT_EQ(panel.child<QLabel>(QStringLiteral("motorMountDimension")).text(), QString());
    EXPECT_EQ(panel.limitDiameter().text(),
              QStringLiteral("Limit motor diameter to mount diameter"));
    EXPECT_FALSE(panel.limitDiameter().isChecked());
    const RangeSlider& diameter = panel.diameterSlider();
    EXPECT_EQ(diameter.maximum(), 11);
    EXPECT_EQ(diameter.valueAt(1), 11);
    EXPECT_EQ(diameter.tickInterval(), 1);
    EXPECT_EQ(diameter.labels().at(7), QStringLiteral("38"));
    EXPECT_EQ(diameter.labels().at(11), QStringLiteral("+"));
    EXPECT_TRUE(diameter.isEnabled());

    EXPECT_EQ(panel.limitLength().text(), QStringLiteral("Limit motor length to mount length"));
    EXPECT_FALSE(panel.limitLength().isChecked());
    const RangeSlider& length = panel.lengthSlider();
    EXPECT_EQ(length.maximum(), 1000);
    EXPECT_EQ(length.valueAt(0), 0);
    EXPECT_EQ(length.valueAt(1), 1000);
    EXPECT_EQ(length.tickInterval(), 0);
    EXPECT_TRUE(length.labels().empty());
    EXPECT_TRUE(length.isEnabled());
    // The filter's lengths: 0 and the largest double, shown in mm.
    EXPECT_EQ(panel.minimumLength().text(), QStringLiteral("0"));
    EXPECT_EQ(panel.maximumLength().text(),
              QtStrings::unitText(*unitGroup(UnitGroupId::MOTOR_DIMENSIONS).getUnit("mm"),
                                  std::numeric_limits<double>::max()));
    EXPECT_TRUE(panel.minimumLength().isFieldEnabled());
    EXPECT_EQ(panel.child<UnitSelector>(QStringLiteral("MinimumLength")).text(),
              QStringLiteral("mm"));

    // Nothing set the filter yet.
    const MotorRowFilter& filter = panel.filter();
    EXPECT_FALSE(filter.getMinimumImpulse().has_value());
    EXPECT_FALSE(filter.getMaximumDiameter().has_value());
    EXPECT_EQ(filter.getMaximumLength(), std::numeric_limits<double>::max());
    EXPECT_TRUE(filter.getExcludedManufacturers().empty());
}

TEST_F(MotorFilterPanelTest, ItsButtonsAreNoDialogsDefaultButton)
{
    // A QPushButton in a QDialog is autoDefault: the dialog makes the first one its default button
    // when it is shown, and Return in a field of the chooser then clicked "Clear All". Java's are
    // plain JButtons (MotorChooserDialogTests shows the chooser).
    InMemoryPreferences               preferences;
    const ThrustCurveMotorSetDatabase database = QtRocket::Test::smallMotorDatabase();
    ThrustCurveMotorSelection         selection(database, preferences);
    QDialog                           dialog;
    auto*                             layout = makeChild<QVBoxLayout>(&dialog);
    layout->addWidget(makeChild<MotorFilterPanel>(&dialog, selection, preferences));
    for (const QString& name : {QStringLiteral("clearMotors"), QStringLiteral("selectMotors")})
    {
        const auto* button = dialog.findChild<QPushButton*>(name);
        ASSERT_NE(button, nullptr) << QtStrings::toUtf8(name);
        EXPECT_FALSE(button->autoDefault()) << QtStrings::toUtf8(name);
        EXPECT_FALSE(button->isDefault()) << QtStrings::toUtf8(name);
    }
}

TEST_F(MotorFilterPanelTest, StartsFromTheStoredManufacturersAndLimits)
{
    Panel panel(&storeLimitBoxesOn);
    EXPECT_TRUE(panel.limitDiameter().isChecked());
    EXPECT_TRUE(panel.limitLength().isChecked());
    // Enabled while the box is off.
    EXPECT_FALSE(panel.diameterSlider().isEnabled());
    EXPECT_FALSE(panel.lengthSlider().isEnabled());
    EXPECT_FALSE(panel.minimumLength().isFieldEnabled());
    EXPECT_FALSE(panel.maximumLength().isFieldEnabled());
    // The unit selectors stay enabled.
    EXPECT_TRUE(panel.child<UnitSelector>(QStringLiteral("MaximumLength")).isEnabled());
    EXPECT_EQ(panel.takeChanges(), 0);
}

TEST_F(MotorFilterPanelTest, StartsWithTheStoredManufacturersUnchecked)
{
    Panel panel(&storeEstesAndQuestExcluded);
    ASSERT_EQ(MotorPreferences::getExcludedMotorManufacturers(panel.preferences()).size(), 2U);
    EXPECT_EQ(panel.manufacturers().checkedIndices(), (std::vector<int>{0, 1, 3}));
    EXPECT_EQ(panel.filter().getExcludedManufacturers().size(), 2U);
    // Less Estes' three sets and Quest's one.
    EXPECT_EQ(panel.table().rowCount(), 7);
}

TEST_F(MotorFilterPanelTest, TheManufacturerListExcludesAndStoresTheUncheckedOnes)
{
    Panel            panel;
    const CheckList& list = panel.manufacturers();
    ASSERT_TRUE(showActiveWith(panel.window(), {&panel.panel()}));

    QTest::mouseClick(&panel.child<QPushButton>(QStringLiteral("clearMotors")), Qt::LeftButton);
    EXPECT_EQ(panel.takeChanges(), 1);
    EXPECT_EQ(panel.table().rowCount(), 0);
    EXPECT_EQ(panel.filter().getExcludedManufacturers().size(), 5U);
    EXPECT_EQ(MotorPreferences::getExcludedMotorManufacturers(panel.preferences()).size(), 5U);

    // A click on Estes checks it.
    const int   estes = panel.manufacturerIndex("Estes");
    const QRect rect  = list.visualItemRect(list.item(estes));
    QTest::mouseClick(list.viewport(), Qt::LeftButton, {}, rect.center());
    EXPECT_EQ(panel.takeChanges(), 1);
    EXPECT_EQ(panel.table().rowCount(), 3);
    // getUncheckedItems(): from the last item to the first.
    const std::vector<const Manufacturer*>& excluded = panel.filter().getExcludedManufacturers();
    ASSERT_EQ(excluded.size(), 4U);
    EXPECT_EQ(excluded.front()->getSimpleName(), "Quest");
    EXPECT_EQ(excluded.back()->getSimpleName(), "AeroTech");
    EXPECT_EQ(MotorPreferences::getExcludedMotorManufacturers(panel.preferences()).size(), 4U);

    QTest::mouseClick(&panel.child<QPushButton>(QStringLiteral("selectMotors")), Qt::LeftButton);
    EXPECT_EQ(panel.takeChanges(), 1);
    EXPECT_EQ(panel.table().rowCount(), 11);
    EXPECT_TRUE(panel.filter().getExcludedManufacturers().empty());
    EXPECT_TRUE(MotorPreferences::getExcludedMotorManufacturers(panel.preferences()).empty());
    const Preferences* node =
        panel.preferences().findNode(PreferenceKeys::kExcludedMotorManufacturersNode);
    ASSERT_NE(node, nullptr);
    EXPECT_TRUE(node->keys().empty());
}

TEST_F(MotorFilterPanelTest, TheImpulseSliderSetsTheImpulseClasses)
{
    Panel        panel;
    RangeSlider& slider = panel.impulseSlider();
    slider.setValueAt(0, 2);
    EXPECT_EQ(panel.takeChanges(), 1);
    EXPECT_EQ(panel.filter().getMinimumImpulse(), ImpulseClass::C);
    EXPECT_EQ(panel.filter().getMaximumImpulse(), ImpulseClass::O);
    // C: above 5 Ns. Estes C6 (9), Klima C6 (10) and the larger ones.
    EXPECT_EQ(panel.table().rowCount(), 8);
    slider.setValueAt(1, 4);
    EXPECT_EQ(panel.filter().getMaximumImpulse(), ImpulseClass::E);
    EXPECT_EQ(panel.table().rowCount(), 4);
    EXPECT_EQ(panel.takeChanges(), 1);
}

TEST_F(MotorFilterPanelTest, TheDiameterSliderSetsTheDiameters)
{
    Panel        panel;
    RangeSlider& slider = panel.diameterSlider();
    slider.setValueAt(1, 4);
    EXPECT_EQ(panel.takeChanges(), 1);
    EXPECT_EQ(panel.filter().getMinimumDiameter(), 0.0);
    EXPECT_EQ(panel.filter().getMaximumDiameter(), 0.024);
    EXPECT_EQ(panel.table().rowCount(), 6);
    slider.setValueAt(0, 3);
    EXPECT_EQ(panel.filter().getMinimumDiameter(), 0.018);
    EXPECT_EQ(panel.table().rowCount(), 5);
    // The last stop is no upper limit.
    slider.setValueAt(1, 11);
    EXPECT_FALSE(panel.filter().getMaximumDiameter().has_value());
    EXPECT_EQ(panel.table().rowCount(), 10);
}

TEST_F(MotorFilterPanelTest, ShowsTheMountsDimensionsAndStoresTheLimitBoxes)
{
    Panel          panel;
    InnerTubeMount mount;
    panel.panel().setMotorMount(&mount.mount());
    EXPECT_EQ(panel.child<QLabel>(QStringLiteral("motorMountDimension")).text(),
              QStringLiteral("Motor mount dimensions: 28 mm x 100 mm"));
    EXPECT_EQ(panel.takeChanges(), 1);
    // setLimitLength() and setLimitDiameter() store the boxes, also unchanged.
    EXPECT_EQ(panel.preferences().get(PreferenceKeys::kMotorFilterLimitLength), "false");
    EXPECT_EQ(panel.preferences().get(PreferenceKeys::kMotorFilterLimitDiameter), "false");
    // No box is on: no limit.
    EXPECT_EQ(panel.diameterSlider().valueAt(1), 11);
    EXPECT_EQ(panel.lengthSlider().valueAt(1), 1000);

    panel.panel().setMotorMount(nullptr);
    EXPECT_EQ(panel.child<QLabel>(QStringLiteral("motorMountDimension")).text(), QString());
}

TEST_F(MotorFilterPanelTest, LimitingTheDiameterToTheMountIsStickyWhenTurnedOff)
{
    Panel          panel;
    InnerTubeMount mount;
    panel.panel().setMotorMount(&mount.mount());
    static_cast<void>(panel.takeChanges());

    panel.limitDiameter().click();
    EXPECT_EQ(panel.preferences().get(PreferenceKeys::kMotorFilterLimitDiameter), "true");
    // The largest nominal diameter not above 28 mm: 24 mm.
    EXPECT_EQ(panel.diameterSlider().valueAt(1), 4);
    EXPECT_EQ(panel.filter().getMaximumDiameter(), 0.024);
    EXPECT_FALSE(panel.diameterSlider().isEnabled());
    // The slider's listener, then the box's.
    EXPECT_EQ(panel.takeChanges(), 2);

    panel.limitDiameter().click();
    EXPECT_EQ(panel.preferences().get(PreferenceKeys::kMotorFilterLimitDiameter), "false");
    EXPECT_EQ(panel.diameterSlider().valueAt(1), 4);
    EXPECT_EQ(panel.filter().getMaximumDiameter(), 0.024);
    EXPECT_TRUE(panel.diameterSlider().isEnabled());
    EXPECT_EQ(panel.takeChanges(), 1);
}

TEST_F(MotorFilterPanelTest, LimitingTheLengthToTheMountIsStickyWhenTurnedOff)
{
    Panel          panel;
    InnerTubeMount mount;
    panel.panel().setMotorMount(&mount.mount());
    static_cast<void>(panel.takeChanges());

    panel.limitLength().click();
    EXPECT_EQ(panel.preferences().get(PreferenceKeys::kMotorFilterLimitLength), "true");
    EXPECT_EQ(panel.lengthSlider().valueAt(0), 0);
    EXPECT_EQ(panel.lengthSlider().valueAt(1), 100);
    EXPECT_EQ(panel.filter().getMinimumLength(), 0.0);
    EXPECT_EQ(panel.filter().getMaximumLength(), mount.mount().getLength());
    EXPECT_EQ(panel.maximumLength().text(), QStringLiteral("100"));
    EXPECT_FALSE(panel.lengthSlider().isEnabled());
    EXPECT_FALSE(panel.minimumLength().isFieldEnabled());
    EXPECT_FALSE(panel.maximumLength().isFieldEnabled());
    // The rows of at most 0.1 m.
    EXPECT_EQ(panel.table().rowCount(), 6);

    panel.limitLength().click();
    EXPECT_EQ(panel.preferences().get(PreferenceKeys::kMotorFilterLimitLength), "false");
    EXPECT_EQ(panel.filter().getMaximumLength(), mount.mount().getLength());
    EXPECT_EQ(panel.lengthSlider().valueAt(1), 100);
    EXPECT_TRUE(panel.lengthSlider().isEnabled());
    EXPECT_TRUE(panel.maximumLength().isFieldEnabled());
    EXPECT_EQ(panel.table().rowCount(), 6);
}

/// What the probe prints of the lengths after a step (tools/openrocket-probes/motor-filter-panel,
/// MotorFilterPanelProbe, its STATE lines): the filter's minimum and maximum lengths
/// (Double.toString()), the length slider's thumbs and what the two spin boxes show, with "inf" for
/// the infinity sign.
[[nodiscard]] std::string lengthState(Panel& panel)
{
    std::string spinners = std::format("[{}][{}]", QtStrings::toUtf8(panel.minimumLength().text()),
                                       QtStrings::toUtf8(panel.maximumLength().text()));
    const std::string infinity = QtStrings::toUtf8(QString(QChar(0x221E)));
    for (std::size_t at = spinners.find(infinity); at != std::string::npos;
         at             = spinners.find(infinity))
    {
        spinners.replace(at, infinity.size(), "inf");
    }
    return std::format("minL={} maxL={} thumbs={},{} spinners={}",
                       Strings::javaDoubleToString(panel.filter().getMinimumLength()),
                       Strings::javaDoubleToString(panel.filter().getMaximumLength()),
                       panel.lengthSlider().valueAt(0), panel.lengthSlider().valueAt(1), spinners);
}

/// The text the probe prints for a diameter limit: Double.toString(), or "null" for none.
[[nodiscard]] std::string diameterText(std::optional<double> value)
{
    return value.has_value() ? Strings::javaDoubleToString(*value) : std::string("null");
}

/// What the probe prints of the diameters and the enabled controls after a step: the filter's
/// minimum and maximum diameters, the diameter slider's thumbs, whether the length slider, the
/// minimum length's spin box and the diameter slider are enabled, and the limit boxes.
[[nodiscard]] std::string diameterState(Panel& panel)
{
    return std::format(
        "minD={} maxD={} dthumbs={},{} enabled=length {} min {} diameter {} boxes={},{}",
        diameterText(panel.filter().getMinimumDiameter()),
        diameterText(panel.filter().getMaximumDiameter()), panel.diameterSlider().valueAt(0),
        panel.diameterSlider().valueAt(1), panel.lengthSlider().isEnabled(),
        panel.minimumLength().isFieldEnabled(), panel.diameterSlider().isEnabled(),
        panel.limitDiameter().isChecked(), panel.limitLength().isChecked());
}

/// Types @p text into @p spinBox and presses Enter, as the probe sets its spinner model's text.
void typeAndCommit(UnitSpinBox& spinBox, const QString& text)
{
    typeInto(spinBox, text);
    QTest::keyClick(&spinBox, Qt::Key_Return);
}

// The probe's lengths(): the length fields and slider follow each other through Java's listeners.
TEST_F(MotorFilterPanelTest, TheLengthFieldsAndTheSliderFollowEachOtherAsOpenRockets)
{
    Panel panel;
    ASSERT_TRUE(showActiveWith(panel.window(), {&panel.panel()}));
    UnitSpinBox& minimum = panel.minimumLength();
    UnitSpinBox& maximum = panel.maximumLength();
    RangeSlider& slider  = panel.lengthSlider();
    EXPECT_EQ(lengthState(panel),
              "minL=0.0 maxL=1.7976931348623157E308 thumbs=0,1000 spinners=[0][inf]");
    // The slider at 50 mm, whose listener sets 50 mm and the slider's end, infinity; the field's
    // listener patches the minimum back to 50.5 mm.
    typeAndCommit(minimum, QStringLiteral("50.5"));
    EXPECT_EQ(lengthState(panel), "minL=0.0505 maxL=Infinity thumbs=50,1000 spinners=[50.5][inf]");
    EXPECT_GT(panel.takeChanges(), 0);
    // All but Estes 1/2A3 (45 mm).
    EXPECT_EQ(panel.table().rowCount(), 10);
    // A maximum below the minimum lowers the minimum.
    typeAndCommit(maximum, QStringLiteral("40"));
    EXPECT_EQ(lengthState(panel), "minL=0.04 maxL=0.04 thumbs=40,40 spinners=[40][40]");
    EXPECT_EQ(panel.table().rowCount(), 0);
    slider.setValueAt(1, 1000);
    EXPECT_EQ(lengthState(panel), "minL=0.04 maxL=Infinity thumbs=40,1000 spinners=[40][inf]");
    slider.setValueAt(0, 70);
    EXPECT_EQ(lengthState(panel), "minL=0.07 maxL=Infinity thumbs=70,1000 spinners=[70][inf]");
    EXPECT_EQ(panel.table().rowCount(), 10);
    // Beyond the slider's end: the slider at its end, the value kept.
    typeAndCommit(minimum, QStringLiteral("2000"));
    EXPECT_EQ(lengthState(panel), "minL=2.0 maxL=Infinity thumbs=1000,1000 spinners=[2000][inf]");
    EXPECT_EQ(panel.table().rowCount(), 0);
    typeAndCommit(minimum, QStringLiteral("0"));
    EXPECT_EQ(lengthState(panel), "minL=0.0 maxL=Infinity thumbs=0,1000 spinners=[0][inf]");
    typeAndCommit(maximum, QStringLiteral("0.0505"));
    EXPECT_EQ(lengthState(panel), "minL=0.0 maxL=5.05E-5 thumbs=0,0 spinners=[0][0.05]");
    typeAndCommit(maximum, QStringLiteral("1500"));
    EXPECT_EQ(lengthState(panel), "minL=0.0 maxL=1.5 thumbs=0,1000 spinners=[0][1500]");
    slider.setValueAt(1, 300);
    EXPECT_EQ(lengthState(panel), "minL=0.0 maxL=0.3 thumbs=0,300 spinners=[0][300]");
    slider.setValueAt(1, 1000);
    EXPECT_EQ(lengthState(panel), "minL=0.0 maxL=Infinity thumbs=0,1000 spinners=[0][inf]");
    typeAndCommit(minimum, QStringLiteral("120"));
    EXPECT_EQ(lengthState(panel), "minL=0.12 maxL=Infinity thumbs=120,1000 spinners=[120][inf]");
    // E16W, G12ST, 117G69-14A, J145H_2JET_STD and M1850W.
    EXPECT_EQ(panel.table().rowCount(), 5);
}

// The probe's lengths() from the mount on: the limit boxes over a minimum above the mount's length,
// and the diameter slider moved by hand.
TEST_F(MotorFilterPanelTest, TheLimitBoxesAndTheSlidersFollowTheMountAsOpenRockets)
{
    Panel panel;
    ASSERT_TRUE(showActiveWith(panel.window(), {&panel.panel()}));
    typeAndCommit(panel.minimumLength(), QStringLiteral("120"));
    InnerTubeMount mount;
    panel.panel().setMotorMount(&mount.mount());
    EXPECT_EQ(lengthState(panel), "minL=0.12 maxL=Infinity thumbs=120,1000 spinners=[120][inf]");
    EXPECT_EQ(diameterState(panel),
              "minD=null maxD=null dthumbs=0,11 enabled=length true min true "
              "diameter true boxes=false,false");
    // The mount's length lowers the maximum, which lowers the minimum.
    panel.limitLength().click();
    EXPECT_EQ(lengthState(panel), "minL=0.1 maxL=0.1 thumbs=100,100 spinners=[100][100]");
    EXPECT_EQ(diameterState(panel),
              "minD=null maxD=null dthumbs=0,11 enabled=length false min false "
              "diameter true boxes=false,true");
    panel.limitLength().click();
    EXPECT_EQ(lengthState(panel), "minL=0.1 maxL=0.1 thumbs=100,100 spinners=[100][100]");
    EXPECT_EQ(diameterState(panel),
              "minD=null maxD=null dthumbs=0,11 enabled=length true min true "
              "diameter true boxes=false,false");
    panel.limitDiameter().click();
    EXPECT_EQ(diameterState(panel),
              "minD=0.0 maxD=0.024 dthumbs=0,4 enabled=length true min true "
              "diameter false boxes=true,false");
    // A thumb set while the slider is disabled (the code may) sets the filter.
    panel.diameterSlider().setValueAt(1, 11);
    EXPECT_EQ(diameterState(panel),
              "minD=0.0 maxD=null dthumbs=0,11 enabled=length true min true "
              "diameter false boxes=true,false");
    panel.limitDiameter().click();
    EXPECT_EQ(diameterState(panel),
              "minD=0.0 maxD=null dthumbs=0,11 enabled=length true min true "
              "diameter true boxes=false,false");
    // setValueAt() can cross the thumbs, and the filter takes them as they are.
    panel.diameterSlider().setValueAt(1, 2);
    panel.diameterSlider().setValueAt(0, 5);
    EXPECT_EQ(diameterState(panel),
              "minD=0.029 maxD=0.013 dthumbs=5,2 enabled=length true min true "
              "diameter true boxes=false,false");
    EXPECT_EQ(panel.table().rowCount(), 0);
    panel.panel().setMotorMount(&mount.mount());
    EXPECT_EQ(lengthState(panel), "minL=0.1 maxL=0.1 thumbs=100,100 spinners=[100][100]");
    EXPECT_EQ(diameterState(panel),
              "minD=0.029 maxD=0.013 dthumbs=5,2 enabled=length true min true "
              "diameter true boxes=false,false");
}

// The probe's storedLimits(): both limit boxes stored on, then the mount.
TEST_F(MotorFilterPanelTest, StoredLimitBoxesApplyWhenTheChooserOpensForAMountAsOpenRockets)
{
    Panel panel(&storeLimitBoxesOn);
    EXPECT_EQ(lengthState(panel),
              "minL=0.0 maxL=1.7976931348623157E308 thumbs=0,1000 spinners=[0][inf]");
    EXPECT_EQ(diameterState(panel),
              "minD=null maxD=null dthumbs=0,11 enabled=length false min false "
              "diameter false boxes=true,true");
    InnerTubeMount mount;
    panel.panel().setMotorMount(&mount.mount());
    EXPECT_EQ(lengthState(panel), "minL=0.0 maxL=0.1 thumbs=0,100 spinners=[0][100]");
    EXPECT_EQ(diameterState(panel),
              "minD=0.0 maxD=0.024 dthumbs=0,4 enabled=length false min false "
              "diameter false boxes=true,true");
    // The six sets of at most 24 mm, all of them at most 0.1 m long.
    EXPECT_EQ(panel.table().rowCount(), 6);
}

TEST_F(MotorFilterPanelTest, TheMountResetsTheLengthUnitsToTheDefault)
{
    Panel              panel;
    const UnitSpinBox& minimum = panel.minimumLength();
    minimum.binding()->setCurrentUnit(*unitGroup(UnitGroupId::MOTOR_DIMENSIONS).getUnit("in"));
    panel.maximumLength().binding()->setCurrentUnit(
        *unitGroup(UnitGroupId::MOTOR_DIMENSIONS).getUnit("in"));
    EXPECT_EQ(panel.child<UnitSelector>(QStringLiteral("MinimumLength")).text(),
              QStringLiteral("in"));
    InnerTubeMount mount;
    panel.panel().setMotorMount(&mount.mount());
    EXPECT_EQ(panel.child<UnitSelector>(QStringLiteral("MinimumLength")).text(),
              QStringLiteral("mm"));
    EXPECT_EQ(panel.child<UnitSelector>(QStringLiteral("MaximumLength")).text(),
              QStringLiteral("mm"));
}

TEST_F(MotorFilterPanelTest, TheMountRelabelsTheDiameterSliderInTheDefaultUnit)
{
    const QtRocket::Test::DefaultUnitsGuard units;
    Panel                                   panel;
    ASSERT_TRUE(unitGroup(UnitGroupId::MOTOR_DIMENSIONS).setDefaultUnit("in"));
    InnerTubeMount mount;
    panel.panel().setMotorMount(&mount.mount());
    const std::map<int, QString>& labels = panel.diameterSlider().labels();
    EXPECT_EQ(labels.at(1), QStringLiteral(".24"));
    EXPECT_EQ(labels.at(10), QStringLiteral("3.9"));
    EXPECT_EQ(labels.at(11), QStringLiteral("+"));
    EXPECT_TRUE(panel.child<QLabel>(QStringLiteral("motorMountDimension"))
                    .text()
                    .startsWith(QStringLiteral("Motor mount dimensions: ")));
    EXPECT_TRUE(panel.child<QLabel>(QStringLiteral("motorMountDimension"))
                    .text()
                    .contains(QStringLiteral("in x ")));
}

TEST_F(MotorFilterPanelTest, HideUsedHidesTheMotorsOfTheMount)
{
    Panel                       panel;
    BodyTubeMount               mount;
    const ThrustCurveMotorSet&  estesC6 = panel.set("Estes", "C6");
    const FlightConfigurationId fcid    = mount.configure(estesC6.getMotors().front(), 5);
    panel.selection().setMotorMountAndConfig(MotorChooserInput::fromMount(mount.mount(), fcid));
    panel.panel().setMotorMount(&mount.mount());
    EXPECT_EQ(panel.table().rowCount(), 11);
    static_cast<void>(panel.takeChanges());
    const std::vector<std::string> keys = panel.preferences().keys();

    auto& hideUsed = panel.child<QCheckBox>(QStringLiteral("hideUsedBox"));
    hideUsed.click();
    EXPECT_TRUE(panel.filter().isHideUsedMotors());
    EXPECT_EQ(panel.takeChanges(), 1);
    EXPECT_EQ(panel.table().rowCount(), 10);
    EXPECT_EQ(panel.table().rowOf(estesC6), -1);
    hideUsed.click();
    EXPECT_FALSE(panel.filter().isHideUsedMotors());
    EXPECT_EQ(panel.table().rowCount(), 11);
    // Not stored.
    EXPECT_EQ(panel.preferences().keys(), keys);
}

TEST_F(MotorFilterPanelTest, HandsHideUnavailableToTheSelection)
{
    Panel panel;
    EXPECT_TRUE(panel.selection().isHideUnavailable());
    panel.panel().setHideUnavailable(false);
    EXPECT_FALSE(panel.selection().isHideUnavailable());
    EXPECT_FALSE(panel.filter().isHideUnavailable());
    EXPECT_FALSE(MotorPreferences::isHideUnavailable(panel.preferences()));
    EXPECT_EQ(panel.takeChanges(), 1);
}

TEST_F(MotorFilterPanelTest, GoesWithoutReachingItsListenersWhileASliderIsHeld)
{
    const RecordingGuardHook guard;
    auto                     panel = std::make_unique<Panel>();
    ASSERT_TRUE(showActiveWith(panel->window(), {&panel->panel()}));
    RangeSlider& slider = panel->impulseSlider();
    // A press beside the thumbs, held: the thumb adjusts until the release, which never comes.
    const QPoint between((slider.positionForValue(0) + slider.positionForValue(14)) / 2,
                         slider.thumbRect(0).center().y());
    QTest::mousePress(&slider, Qt::LeftButton, {}, between);
    EXPECT_TRUE(slider.isAdjusting(0) || slider.isAdjusting(1));
    panel.reset();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    EXPECT_TRUE(guard.failures().empty());
}

}  // namespace

}  // namespace QtRocket::Gui
