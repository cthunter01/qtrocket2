// The motor chooser of a design window against OpenRocket's, for every motor mount of the 16
// example designs: the pins of MotorChooserPins.h (tools/openrocket-gui-pins, family
// motor-chooser: OpenRocket's real ThrustCurveMotorSelectionPanel over the bundled database,
// opened for each acting motor mount in each flight configuration of a design, the mount's limits
// applied, by one chooser per design as MotorConfigurationPanel keeps one), replayed through the
// question UserPrompts::chooseMotor() asks (MotorChooserDialog::choose(), one chooser per window,
// answered from its own event loop): the filter panel, the table's rows, the selection, the curve
// and delay lists at each open, the searches at a mount's first open, OK's answer and the preferred
// curves it stores.
//
// QtRocket's deliberate differences:
// - Bug 8 (plan 10.3): a configured delay is kept (OpenRocket shows and answers the closest
//   standard delay: a plugged H669N-P becomes the set's last delay). Where the chooser selects
//   the configured motor, the expected delay and its text are the configuration's (the pin's
//   configuredDelay and configuredDelayText); OpenRocket's are in the pin.
// - QtRocket's fix in ThrustCurveMotorSelectionPanel::setMotorMountAndConfig() (no row of an
//   earlier question replaces the configured motor): the opens it changes are the probe's second
//   run (kMotorChooserMountFixedOpens, kMotorChooserMountFixedPreferred), OpenRocket's own panel
//   with the fix made from outside.

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <format>
#include <memory>
#include <optional>
#include <ostream>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <QCheckBox>
#include <QComboBox>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QModelIndex>
#include <QObject>
#include <QPushButton>
#include <QString>
#include <QTableView>
#include <QTest>
#include <QWidget>
#include <Qt>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "MotorChooserPins.h"
#include "MotorChooserTestSupport.h"
#include "NextModalDialog.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/file/LoadedDocument.h"
#include "QtRocket/gui/dialogs/motor/MotorChooserDialog.h"
#include "QtRocket/gui/dialogs/motor/MotorInformationPanel.h"
#include "QtRocket/gui/dialogs/motor/MotorTableModel.h"
#include "QtRocket/gui/dialogs/motor/ThrustCurveMotorSelectionPanel.h"
#include "QtRocket/gui/util/GuiServices.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/util/UserPrompts.h"
#include "QtRocket/gui/widgets/RangeSlider.h"
#include "QtRocket/motor/MotorPreferences.h"
#include "QtRocket/motor/ThrustCurveMotor.h"
#include "QtRocket/motor/ThrustCurveMotorSet.h"
#include "QtRocket/motor/ThrustCurveMotorSetDatabase.h"
#include "QtRocket/motor/chooser/MotorRowFilter.h"
#include "QtRocket/motor/chooser/ThrustCurveMotorSelection.h"
#include "QtRocket/preferences/PreferenceKeys.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/MotorConfiguration.h"
#include "QtRocket/rocket/MotorMount.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Error.h"
#include "TestGuiServices.h"
#include "file/DesignFileEnvironment.h"
#include "goldens/GoldenExamples.h"
#include "motor/TestMotorDatabase.h"

namespace QtRocket::Gui
{

namespace
{

namespace Pins = Gui::Test;
using Gui::Test::NextModalDialog;
using Pins::MotorChooserMountOpenPin;

/// The index of @p set in @p database's sets, -1 for none (or one of the chooser's own).
[[nodiscard]] int setIndexOf(const ThrustCurveMotorSetDatabase& database,
                             const ThrustCurveMotorSet*         set)
{
    const auto& sets  = database.getMotorSets();
    const auto  found = std::ranges::find_if(
        sets, [set](const ThrustCurveMotorSet& candidate) { return &candidate == set; });
    return found == sets.end() ? -1 : static_cast<int>(found - sets.begin());
}

/// The index of the curve @p motor in @p set's curves, -1 for none (or no set).
[[nodiscard]] int curveIndexOf(const ThrustCurveMotorSet* set, const Motor* motor)
{
    if (set == nullptr || motor == nullptr)
    {
        return -1;
    }
    const auto& curves = set->getMotors();
    const auto  found =
        std::ranges::find_if(curves, [motor](const std::shared_ptr<const ThrustCurveMotor>& curve) {
            return curve.get() == motor;
        });
    return found == curves.end() ? -1 : static_cast<int>(found - curves.begin());
}

/// The set of @p database that holds the curve @p motor, or null.
[[nodiscard]] const ThrustCurveMotorSet* setHolding(const ThrustCurveMotorSetDatabase& database,
                                                    const Motor*                       motor)
{
    for (const ThrustCurveMotorSet& set : database.getMotorSets())
    {
        if (curveIndexOf(&set, motor) >= 0)
        {
            return &set;
        }
    }
    return nullptr;
}

/// Indices joined by ','.
[[nodiscard]] std::string joined(const std::vector<int>& indices)
{
    std::string text;
    for (const int index : indices)
    {
        text += (text.empty() ? "" : ",") + std::to_string(index);
    }
    return text;
}

/// The model rows the pins give the table with the MD5 @p md5, joined by ','; a test failure
/// when no table has it.
[[nodiscard]] std::string pinnedModelRows(std::string_view md5)
{
    // Through a span: the iterators of std::array are pointers in some libraries and classes in
    // others (MSVC).
    const std::span<const Pins::MotorChooserMountRowsPin> tables(Pins::kMotorChooserMountRows);
    const auto found = std::ranges::find(tables, md5, &Pins::MotorChooserMountRowsPin::md5);
    if (found == tables.end())
    {
        ADD_FAILURE() << "no pinned table " << md5;
        return {};
    }
    std::vector<int> rows;
    rows.reserve(static_cast<std::size_t>(found->rows));
    for (int row = 0; row < found->rows; ++row)
    {
        rows.push_back(Pins::kMotorChooserMountRowIndices.at(
            static_cast<std::size_t>(found->firstRow) + static_cast<std::size_t>(row)));
    }
    return joined(rows);
}

/// What the chooser shows at an open (the fields of MotorChooserMountOpenPin it shows).
struct OpenState
{
    bool                  limitDiameter = false;
    bool                  limitLength   = false;
    int                   diameterLow   = 0;
    int                   diameterHigh  = 0;
    int                   lengthLow     = 0;
    int                   lengthHigh    = 0;
    std::optional<double> minimumDiameter;
    std::optional<double> maximumDiameter;
    double                minimumLength = 0.0;
    double                maximumLength = 0.0;
    std::string           mountLabel;
    std::string           motorsLabel;
    int                   rows = 0;
    std::string           md5;
    int                   selectedRow   = -1;
    int                   selectedSet   = -1;
    int                   selectedCurve = -1;
    bool                  curvesEnabled = false;
    int                   curveSet      = -1;
    std::string           curves;
    int                   curveIndex = -1;
    std::string           delayItems;
    std::string           delayText;
    double                delay         = 0.0;
    double                nozzle        = 0.0;
    double                nozzleMaximum = 0.0;

    [[nodiscard]] bool operator==(const OpenState&) const = default;
};

/// What GoogleTest prints for a state of a failed comparison.
// NOLINTNEXTLINE(readability-identifier-naming): the name GoogleTest looks for
void PrintTo(const OpenState& state, std::ostream* out)
{
    const auto limit = [](const std::optional<double>& value) {
        return value.has_value() ? std::format("{}", *value) : std::string("none");
    };
    *out << "{limits " << state.limitDiameter << state.limitLength << ", diameter thumbs "
         << state.diameterLow << "," << state.diameterHigh << ", length thumbs " << state.lengthLow
         << "," << state.lengthHigh << ", diameter " << limit(state.minimumDiameter) << ".."
         << limit(state.maximumDiameter) << ", length " << std::format("{}", state.minimumLength)
         << ".." << std::format("{}", state.maximumLength) << ", '" << state.mountLabel << "', '"
         << state.motorsLabel << "', " << state.rows << " rows " << state.md5 << ", selected row "
         << state.selectedRow << ", set " << state.selectedSet << " curve " << state.selectedCurve
         << ", curves " << (state.curvesEnabled ? "on" : "off") << " set " << state.curveSet << " ["
         << state.curves << "] at " << state.curveIndex << ", delays [" << state.delayItems << "] '"
         << state.delayText << "' " << std::format("{}", state.delay) << ", nozzle "
         << std::format("{}", state.nozzle) << " of " << std::format("{}", state.nozzleMaximum)
         << "}";
}

/// The delay list's text the pin @p pin gives ("null", no item selected, is an empty text), or
/// with @p keepsDelay the configured delay's.
[[nodiscard]] std::string expectedDelayText(const MotorChooserMountOpenPin& pin, bool keepsDelay)
{
    if (keepsDelay)
    {
        return std::string(pin.configuredDelayText);
    }
    return pin.delayText == "null" ? std::string() : std::string(pin.delayText);
}

/// The state the pin @p pin gives, with QtRocket's bug 8: where the chooser selected the
/// configured motor, its configured delay (but a NaN one) and that delay's text.
[[nodiscard]] OpenState expectedState(const MotorChooserMountOpenPin& pin)
{
    const bool keepsDelay = pin.configuredSet >= 0 && pin.selectedSet == pin.configuredSet &&
                            pin.selectedCurve == pin.configuredCurve &&
                            !std::isnan(pin.configuredDelay);
    return {.limitDiameter = pin.limitDiameter,
            .limitLength   = pin.limitLength,
            .diameterLow   = pin.diameterLow,
            .diameterHigh  = pin.diameterHigh,
            .lengthLow     = pin.lengthLow,
            .lengthHigh    = pin.lengthHigh,
            .minimumDiameter =
                pin.hasMinimumDiameter ? std::optional(pin.minimumDiameter) : std::nullopt,
            .maximumDiameter =
                pin.hasMaximumDiameter ? std::optional(pin.maximumDiameter) : std::nullopt,
            .minimumLength = pin.minimumLength,
            .maximumLength = pin.maximumLength,
            .mountLabel    = std::string(pin.mountLabel),
            .motorsLabel   = std::string(pin.motorsLabel),
            .rows          = pin.rows,
            .md5           = std::string(pin.md5),
            .selectedRow   = pin.selectedRow,
            .selectedSet   = pin.selectedSet,
            .selectedCurve = pin.selectedCurve,
            .curvesEnabled = pin.curvesEnabled,
            .curveSet      = pin.curveSet,
            .curves        = std::string(pin.curves),
            .curveIndex    = pin.curveIndex,
            .delayItems    = std::string(pin.delayItems),
            .delayText     = expectedDelayText(pin, keepsDelay),
            .delay         = keepsDelay ? pin.configuredDelay : pin.delay,
            .nozzle        = pin.nozzle,
            .nozzleMaximum = pin.nozzleMaximum};
}

/// @p chooser's child of the type T named @p name.
template <class T>
[[nodiscard]] T& childOf(const MotorChooserDialog& chooser, std::string_view name)
{
    // The name as a std::string_view: GCC's -Wdangling-reference takes a reference returned by a
    // function given a temporary QString for one into that temporary.
    T* found = chooser.findChild<T*>(QtStrings::fromUtf8(name));
    if (found == nullptr)
    {
        bug("the motor chooser has no child " + std::string(name));
    }
    return *found;
}

[[nodiscard]] std::string labelText(const MotorChooserDialog& chooser, std::string_view name)
{
    return QtStrings::toUtf8(childOf<QLabel>(chooser, name).text());
}

/// The table's rows: their model rows joined by ',', and the selected row (-1 for none).
[[nodiscard]] std::string modelRowsOf(const MotorTableModel& model)
{
    std::vector<int> rows;
    for (int row = 0; row < model.rowCount(); ++row)
    {
        rows.push_back(static_cast<int>(model.modelRow(row)));
    }
    return joined(rows);
}

/// The delay list's items joined by '|'.
[[nodiscard]] std::string itemsOf(const QComboBox& box)
{
    std::string items;
    for (int index = 0; index < box.count(); ++index)
    {
        items += (index == 0 ? "" : "|") + QtStrings::toUtf8(box.itemText(index));
    }
    return items;
}

/// The filter panel's part of what @p chooser shows.
void readFilter(const MotorChooserDialog& chooser, OpenState& state)
{
    const ThrustCurveMotorSelection& selection = chooser.selectionPanel().selection();
    const auto&                      diameter  = childOf<RangeSlider>(chooser, "diameterSlider");
    const auto&                      length    = childOf<RangeSlider>(chooser, "lengthSlider");
    state.limitDiameter   = childOf<QCheckBox>(chooser, "limitDiameterCheckBox").isChecked();
    state.limitLength     = childOf<QCheckBox>(chooser, "limitByLengthCheckBox").isChecked();
    state.diameterLow     = diameter.valueAt(0);
    state.diameterHigh    = diameter.valueAt(1);
    state.lengthLow       = length.valueAt(0);
    state.lengthHigh      = length.valueAt(1);
    state.minimumDiameter = selection.getRowFilter().getMinimumDiameter();
    state.maximumDiameter = selection.getRowFilter().getMaximumDiameter();
    state.minimumLength   = selection.getRowFilter().getMinimumLength();
    state.maximumLength   = selection.getRowFilter().getMaximumLength();
    state.mountLabel      = labelText(chooser, "motorMountDimension");
}

/// The table's part of what @p chooser shows.
void readTable(const MotorChooserDialog& chooser, OpenState& state)
{
    const MotorTableModel& model = chooser.selectionPanel().tableModel();
    state.motorsLabel            = labelText(chooser, "nrOfMotorsLabel");
    state.rows                   = model.rowCount();
    state.md5                    = Gui::Test::motorTableMd5(model);
    const QModelIndexList rows =
        childOf<QTableView>(chooser, "table").selectionModel()->selectedRows();
    state.selectedRow = rows.isEmpty() ? -1 : rows.front().row();
}

/// The selection's part of what @p chooser shows, the sets indexed in @p database.
void readSelection(const MotorChooserDialog& chooser, const ThrustCurveMotorSetDatabase& database,
                   OpenState& state)
{
    const ThrustCurveMotorSelectionPanel& panel     = chooser.selectionPanel();
    const ThrustCurveMotorSelection&      selection = panel.selection();
    state.selectedSet = setIndexOf(database, selection.getSelectedMotorSet());
    state.selectedCurve =
        curveIndexOf(selection.getSelectedMotorSet(), panel.getSelectedMotor().get());
    // The curve list's curves are those of the details (updateData() gives both the same list).
    const auto& curves  = childOf<MotorInformationPanel>(chooser, "motorInformationPanel").curves();
    const auto& box     = childOf<QComboBox>(chooser, "curveSelectionBox");
    state.curvesEnabled = box.isEnabled();
    const ThrustCurveMotorSet* curveSet =
        curves.empty() ? nullptr : setHolding(database, curves.front().get());
    state.curveSet = setIndexOf(database, curveSet);
    std::vector<int> indices;
    indices.reserve(curves.size());
    for (const std::shared_ptr<const ThrustCurveMotor>& curve : curves)
    {
        indices.push_back(curveIndexOf(curveSet, curve.get()));
    }
    state.curves        = joined(indices);
    state.curveIndex    = box.currentIndex();
    const auto& delays  = childOf<QComboBox>(chooser, "delayBox");
    state.delayItems    = itemsOf(delays);
    state.delayText     = QtStrings::toUtf8(delays.currentText());
    state.delay         = panel.getSelectedDelay();
    state.nozzle        = panel.getSelectedNozzleExitDiameter();
    state.nozzleMaximum = selection.getMaximumNozzleExitDiameter();
}

/// What @p chooser shows, the sets indexed in @p database.
[[nodiscard]] OpenState stateOf(const MotorChooserDialog&          chooser,
                                const ThrustCurveMotorSetDatabase& database)
{
    OpenState state;
    readFilter(chooser, state);
    readTable(chooser, state);
    readSelection(chooser, database, state);
    return state;
}

/// What a search shows: its rows and their MD5.
struct SearchState
{
    int         rows = 0;
    std::string md5;
    std::string modelRows;

    [[nodiscard]] bool operator==(const SearchState&) const = default;
};

// NOLINTNEXTLINE(readability-identifier-naming): the name GoogleTest looks for
void PrintTo(const SearchState& state, std::ostream* out)
{
    *out << state.rows << " rows " << state.md5 << " [" << state.modelRows << "]";
}

/// The open of @p pin: the chooser compared with the pin (with QtRocket's differences), the
/// searches of the mount's first open typed and the field emptied, then OK.
struct CheckOpen
{
    const MotorChooserMountOpenPin*    pin;
    const ThrustCurveMotorSetDatabase* database;

    void operator()(QWidget& dialog) const
    {
        auto* chooser = qobject_cast<MotorChooserDialog*>(&dialog);
        ASSERT_NE(chooser, nullptr);
        EXPECT_EQ(stateOf(*chooser, *database), expectedState(*pin))
            << pin->design << " configuration " << pin->configuration << " mount " << pin->mount;
        // The rows in their order (the MD5 above covers their texts too).
        EXPECT_EQ(modelRowsOf(chooser->selectionPanel().tableModel()), pinnedModelRows(pin->md5))
            << pin->design << " configuration " << pin->configuration << " mount " << pin->mount;
        search(*chooser);
        QTest::mouseClick(&childOf<QPushButton>(*chooser, "okButton"), Qt::LeftButton);
    }

    void search(const MotorChooserDialog& chooser) const
    {
        auto& field = childOf<QLineEdit>(chooser, "searchField");
        for (const Pins::MotorChooserMountSearchPin& searched : Pins::kMotorChooserMountSearchRows)
        {
            if (searched.design != pin->design || searched.configuration != pin->configuration ||
                searched.mount != pin->mount)
            {
                continue;
            }
            field.setText(QtStrings::fromUtf8(searched.text));
            const MotorTableModel& model = chooser.selectionPanel().tableModel();
            EXPECT_EQ((SearchState{.rows      = model.rowCount(),
                                   .md5       = Gui::Test::motorTableMd5(model),
                                   .modelRows = modelRowsOf(model)}),
                      (SearchState{.rows      = searched.rows,
                                   .md5       = std::string(searched.md5),
                                   .modelRows = pinnedModelRows(searched.md5)}))
                << pin->design << " mount " << pin->mount << " search '" << searched.text << "'";
        }
        field.clear();
    }
};

/// The pin of the open of @p design, @p configuration and @p mount QtRocket is expected to give:
/// the fixed run's where QtRocket's fix changes it, else OpenRocket's.
[[nodiscard]] MotorChooserMountOpenPin expectedOpen(const MotorChooserMountOpenPin& pin)
{
    for (const MotorChooserMountOpenPin& fixed : Pins::kMotorChooserMountFixedOpens)
    {
        if (fixed.design == pin.design && fixed.configuration == pin.configuration &&
            fixed.mount == pin.mount)
        {
            return fixed;
        }
    }
    return pin;
}

/// OK's answer as the pin gives it (with bug 8 as expectedState()).
struct Answer
{
    int    set    = -1;
    int    curve  = -1;
    double delay  = 0.0;
    double nozzle = 0.0;

    [[nodiscard]] bool operator==(const Answer&) const = default;
};

// NOLINTNEXTLINE(readability-identifier-naming): the name GoogleTest looks for
void PrintTo(const Answer& answer, std::ostream* out)
{
    *out << "{set " << answer.set << " curve " << answer.curve << ", delay "
         << std::format("{}", answer.delay) << ", nozzle " << std::format("{}", answer.nozzle)
         << "}";
}

[[nodiscard]] Answer expectedAnswer(const MotorChooserMountOpenPin& pin)
{
    if (pin.answerSet < 0)
    {
        return {};
    }
    const bool keepsDelay = pin.answerSet == pin.configuredSet &&
                            pin.answerCurve == pin.configuredCurve &&
                            !std::isnan(pin.configuredDelay);
    return {.set    = pin.answerSet,
            .curve  = pin.answerCurve,
            .delay  = keepsDelay ? pin.configuredDelay : pin.answerDelay,
            .nozzle = pin.answerNozzle};
}

[[nodiscard]] Answer answerOf(const std::optional<UserPrompts::MotorChoice>& choice,
                              const ThrustCurveMotorSetDatabase&             database)
{
    if (!choice.has_value())
    {
        return {};
    }
    const ThrustCurveMotorSet* set = setHolding(database, choice->motor.get());
    return {.set    = setIndexOf(database, set),
            .curve  = curveIndexOf(set, choice->motor.get()),
            .delay  = choice->delay,
            .nozzle = choice->nozzleExitDiameter};
}

/// The preferred curves the pins give @p design after its opens, as "digest=value" lines.
[[nodiscard]] std::string pinnedPreferred(std::string_view design)
{
    const bool  fixed = std::ranges::find(Pins::kMotorChooserMountFixedPreferredDesigns, design) !=
                        Pins::kMotorChooserMountFixedPreferredDesigns.end();
    std::string text;
    for (const Pins::MotorChooserMountPreferredPin& pin :
         fixed ? std::span<const Pins::MotorChooserMountPreferredPin>(
                     Pins::kMotorChooserMountFixedPreferred)
               : std::span<const Pins::MotorChooserMountPreferredPin>(
                     Pins::kMotorChooserMountPreferred))
    {
        if (pin.design == design)
        {
            text += std::string(pin.digest) + "=" + (pin.value ? "true" : "false") + "\n";
        }
    }
    return text;
}

/// The node of the preferred curves of @p preferences, as pinnedPreferred() writes it.
[[nodiscard]] std::string preferredOf(const Preferences& preferences)
{
    const Preferences* node =
        preferences.findNode(QtRocket::PreferenceKeys::kPreferredThrustCurveMotorNode);
    std::string text;
    if (node == nullptr)
    {
        return text;
    }
    for (const std::string& key : node->keys())
    {
        text += key + "=" + node->get(key).value_or("?") + "\n";
    }
    return text;
}

/// A design window over an example as the probe loads it (no component presets, settled) with
/// the chooser's services: the bundled motor database the design's motors come from, fresh
/// preferences with both limit boxes on.
class DesignWindow
{
public:
    explicit DesignWindow(std::string_view design)
      : m_services(m_testServices.services().preferences(), m_testServices.services().materials(),
                   QtRocket::Test::bundledMotorDatabase(), m_testServices.services().presets(),
                   m_testServices.services().loadingContext(), m_testServices.prompts())
    {
        MotorPreferences::setLimitDiameter(m_testServices.preferences(), true);
        MotorPreferences::setLimitLength(m_testServices.preferences(), true);
        const QtRocket::Test::GoldenExample* example = QtRocket::Test::findGoldenExample(design);
        if (example == nullptr)
        {
            bug("no example " + std::string(design));
        }
        Result<LoadedDocument> loaded = QtRocket::Test::loadSettledGoldenExample(
            *example, QtRocket::Test::ExampleSource::ORIGINAL,
            QtRocket::Test::DesignFileEnvironment::Presets::NONE);
        if (!loaded.has_value())
        {
            bug("the example does not load: " + loaded.error().message);
        }
        m_document = std::move(loaded->document);
        for (RocketComponent& component : m_document->getRocket().subtree())
        {
            if (auto* mount = dynamic_cast<MotorMount*>(&component);
                mount != nullptr && mount->isMotorMount())
            {
                m_mounts.push_back(mount);
            }
        }
    }

    [[nodiscard]] Rocket&                         rocket() const { return m_document->getRocket(); }
    [[nodiscard]] const std::vector<MotorMount*>& mounts() const noexcept { return m_mounts; }
    [[nodiscard]] const GuiServices&              services() const noexcept { return m_services; }
    [[nodiscard]] const Preferences& preferences() noexcept { return m_testServices.preferences(); }
    [[nodiscard]] QWidget&           window() noexcept { return m_window; }

private:
    Gui::Test::TestGuiServices          m_testServices;
    GuiServices                         m_services;
    std::unique_ptr<OpenRocketDocument> m_document;
    std::vector<MotorMount*>            m_mounts;
    QWidget                             m_window;
};

/// A motor mount as the pins give it.
struct MountState
{
    std::string name;
    double      diameter = 0.0;
    double      length   = 0.0;

    [[nodiscard]] bool operator==(const MountState&) const = default;
};

// NOLINTNEXTLINE(readability-identifier-naming): the name GoogleTest looks for
void PrintTo(const MountState& mount, std::ostream* out)
{
    *out << mount.name << " " << std::format("{}", mount.diameter) << " x "
         << std::format("{}", mount.length);
}

/// The acting motor mounts of @p design as the pins give them, in order.
[[nodiscard]] std::vector<MountState> pinnedMounts(std::string_view design)
{
    std::vector<MountState> mounts;
    for (const Pins::MotorChooserMountPin& pin : Pins::kMotorChooserMounts)
    {
        if (pin.design == design)
        {
            mounts.push_back(
                {.name = std::string(pin.name), .diameter = pin.diameter, .length = pin.length});
        }
    }
    return mounts;
}

/// The acting motor mounts of @p window's design, in the order of the component tree.
[[nodiscard]] std::vector<MountState> mountsOf(const DesignWindow& window)
{
    std::vector<MountState> mounts;
    for (const MotorMount* mount : window.mounts())
    {
        mounts.push_back({.name     = asComponent(*mount).getName(),
                          .diameter = mount->getMotorMountDiameter(),
                          .length   = asComponent(*mount).getLength()});
    }
    return mounts;
}

/// The configured motor of @p mount in @p fcid as the pin gives it: set, curve, delay, nozzle.
[[nodiscard]] Answer configuredOf(const MotorMount& mount, const FlightConfigurationId& fcid,
                                  const ThrustCurveMotorSetDatabase& database)
{
    const MotorConfiguration&  configuration = mount.getMotorConfig(fcid);
    const ThrustCurveMotorSet* set =
        configuration.hasMotor() ? setHolding(database, configuration.getMotor().get()) : nullptr;
    return {.set    = setIndexOf(database, set),
            .curve  = curveIndexOf(set, configuration.getMotor().get()),
            .delay  = configuration.getEjectionDelay(),
            .nozzle = configuration.getNozzleExitDiameter()};
}

/// Every open of @p design's pins, in order, through the window's chooser.
void replayOpens(DesignWindow& window, std::string_view design)
{
    const std::vector<FlightConfigurationId> ids      = window.rocket().getIds();
    const ThrustCurveMotorSetDatabase&       database = window.services().motors();
    for (const MotorChooserMountOpenPin& pin : Pins::kMotorChooserMountOpens)
    {
        if (pin.design != design)
        {
            continue;
        }
        const FlightConfigurationId& fcid = ids.at(static_cast<std::size_t>(pin.configuration));
        const MotorMount& mount = *window.mounts().at(static_cast<std::size_t>(pin.mount));
        EXPECT_EQ(configuredOf(mount, fcid, database), (Answer{.set    = pin.configuredSet,
                                                               .curve  = pin.configuredCurve,
                                                               .delay  = pin.configuredDelay,
                                                               .nozzle = pin.configuredNozzle}))
            << "the design's configuration " << pin.configuration << " mount " << pin.mount;
        const MotorChooserMountOpenPin          expected = expectedOpen(pin);
        std::optional<UserPrompts::MotorChoice> choice;
        {
            const NextModalDialog answer(CheckOpen{.pin = &expected, .database = &database});
            choice = MotorChooserDialog::choose(&window.window(), window.services(), mount, fcid);
        }
        EXPECT_EQ(answerOf(choice, database), expectedAnswer(expected))
            << "OK of configuration " << pin.configuration << " mount " << pin.mount;
    }
}

/// The designs of the pins, for INSTANTIATE_TEST_SUITE_P.
[[nodiscard]] std::vector<std::string_view> pinnedDesigns()
{
    std::vector<std::string_view> designs;
    designs.reserve(Pins::kMotorChooserMountDesigns.size());
    for (const Pins::MotorChooserMountDesignPin& pin : Pins::kMotorChooserMountDesigns)
    {
        designs.push_back(pin.design);
    }
    return designs;
}

/// A test name of a design: its file name's letters and digits, '_' for the others.
[[nodiscard]] std::string designTestName(const ::testing::TestParamInfo<std::string_view>& info)
{
    std::string name;
    for (const char character : info.param.substr(0, info.param.rfind('.')))
    {
        name += (std::isalnum(static_cast<unsigned char>(character)) != 0) ? character : '_';
    }
    return name;
}

using MotorChooserMountsGoldenCommit = Gui::Test::GuiTest;

class MotorChooserMountsGolden : public Gui::Test::GuiTest,
                                 public ::testing::WithParamInterface<std::string_view>
{ };

TEST_F(MotorChooserMountsGoldenCommit, ThePinsComeFromTheGoldensCommit)
{
    EXPECT_EQ(Pins::kMotorChooserMountPinsCommit, "5f164fd0e05d6e188857b0185601293b7a6e9eaf");
    EXPECT_EQ(Pins::kMotorChooserMountDesigns.size(), QtRocket::Test::goldenExamples().size());
}

TEST_P(MotorChooserMountsGolden, OpensTheChooserForEveryMountAsOpenRocket)
{
    const std::string_view                                  design = GetParam();
    DesignWindow                                            window(design);
    const std::span<const Pins::MotorChooserMountDesignPin> designs(
        Pins::kMotorChooserMountDesigns);
    const auto pin = std::ranges::find(designs, design, &Pins::MotorChooserMountDesignPin::design);
    ASSERT_NE(pin, designs.end());
    EXPECT_EQ(static_cast<int>(window.rocket().getIds().size()), pin->configurations);
    EXPECT_EQ(static_cast<int>(window.mounts().size()), pin->mounts);
    EXPECT_EQ(mountsOf(window), pinnedMounts(design));
    replayOpens(window, design);
    // One chooser served the design's questions, and OK stored the preferred curves.
    EXPECT_EQ(window.window().findChildren<MotorChooserDialog*>(Qt::FindDirectChildrenOnly).size(),
              1);
    EXPECT_EQ(preferredOf(window.preferences()), pinnedPreferred(design));
}

INSTANTIATE_TEST_SUITE_P(Examples, MotorChooserMountsGolden, ::testing::ValuesIn(pinnedDesigns()),
                         designTestName);

}  // namespace

}  // namespace QtRocket::Gui
