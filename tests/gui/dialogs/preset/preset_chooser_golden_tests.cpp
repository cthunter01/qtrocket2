// The parts library against OpenRocket's own (PresetChooserPins.h, written by
// tools/openrocket-gui-pins/generate.sh preset-chooser from PresetChooserProbe.java, OpenRocket's
// ComponentPresetChooserDialog run on 5f164fd0e with JDK 17 under xvfb): for every kind of
// component that takes presets, in the probe's two designs, over the bundled presets (the probe
// walks data/components as QtRocket's DirectoryIterator does, so both databases hold the same
// presets in the same order), the table's columns, the boxes, the diameter filters, the "Always
// open" box and the table's rows after each step of the probe (their number and the MD5 of their
// digests in view order); then what Close, a double click and the "Always open" box do, and the
// dialog as a window: its size, default button, Escape, focus order and the table's Enter keys.
//
// The designs are built here as the probe builds them, and the dialog is asked for as the
// configuration dialog asks for it (PresetDiameterFilters::forComponent()). Where QtRocket fixes an
// OpenRocket bug the pins hold the fixed result, which the probe computes with OpenRocket's
// classes: the flipped nose cone behind a transition compares with the transition's aft diameter
// (bug 16 of the plan's section 10.3; openRocketAftDiameter is OpenRocket's value). The probe's
// texts never match the true/false text of a Boolean column (bug 17).

#include <cstddef>
#include <format>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <QAbstractItemView>
#include <QCheckBox>
#include <QDialog>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QModelIndex>
#include <QObject>
#include <QPushButton>
#include <QSignalSpy>
#include <QSize>
#include <QString>
#include <QTest>
#include <QWidget>
#include <Qt>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "PresetChooserPins.h"
#include "PresetTableTestSupport.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/OpenRocketDocumentFactory.h"
#include "QtRocket/gui/binding/PresetDiameterFilters.h"
#include "QtRocket/gui/dialogs/preset/ComponentPresetChooserDialog.h"
#include "QtRocket/gui/dialogs/preset/ComponentPresetFilterModel.h"
#include "QtRocket/gui/dialogs/preset/ComponentPresetTable.h"
#include "QtRocket/gui/dialogs/preset/ComponentPresetTableModel.h"
#include "QtRocket/gui/settings/GuiPreferenceKeys.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/util/UserPrompts.h"
#include "QtRocket/gui/widgets/StyledLabel.h"
#include "QtRocket/gui/widgets/TableRowTraversal.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/Bulkhead.h"
#include "QtRocket/rocket/CenteringRing.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/EngineBlock.h"
#include "QtRocket/rocket/InnerTube.h"
#include "QtRocket/rocket/LaunchLug.h"
#include "QtRocket/rocket/NoseCone.h"
#include "QtRocket/rocket/Parachute.h"
#include "QtRocket/rocket/RailButton.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/Streamer.h"
#include "QtRocket/rocket/SymmetricComponent.h"
#include "QtRocket/rocket/Transition.h"
#include "QtRocket/rocket/TubeCoupler.h"
#include "QtRocket/rocket/TubeFinSet.h"
#include "QtRocket/rocket/preset/ComponentPresetDatabase.h"
#include "QtRocket/rocket/preset/ComponentPresetType.h"
#include "TestGuiServices.h"
#include "rocket/preset/TestPresetDatabase.h"

namespace QtRocket::Gui
{

namespace
{

using PresetChooserPinsGolden = Gui::Test::GuiTest;
using Dialog                  = ComponentPresetChooserDialog;
using Gui::Test::kPresetChooserActions;
using Gui::Test::kPresetChooserColumns;
using Gui::Test::kPresetChooserFocusOrder;
using Gui::Test::kPresetChooserKeys;
using Gui::Test::kPresetChooserRadii;
using Gui::Test::kPresetChooserScenarios;
using Gui::Test::kPresetChooserViews;
using Gui::Test::kPresetChooserWindow;
using Gui::Test::PresetChooserActionKind;
using Gui::Test::PresetChooserActionPin;
using Gui::Test::PresetChooserAlwaysOpen;
using Gui::Test::PresetChooserBoxPin;
using Gui::Test::PresetChooserKey;
using Gui::Test::PresetChooserKeyPin;
using Gui::Test::PresetChooserScenarioPin;
using Gui::Test::PresetChooserStep;
using Gui::Test::PresetChooserViewPin;
using Gui::Test::presetDigestOfView;
using Gui::Test::TestGuiServices;

/// The probe's first design's body tubes.
constexpr double kFrontRadius = 0.0124;
constexpr double kBackRadius  = 0.0208;
constexpr double kTubeLength  = 0.3;

/// The probe's two designs, and its scenarios' components in its order.
struct ProbeDesigns
{
    std::unique_ptr<OpenRocketDocument> first  = OpenRocketDocumentFactory::createNewRocket();
    std::unique_ptr<OpenRocketDocument> second = OpenRocketDocumentFactory::createNewRocket();
    std::vector<const RocketComponent*> scenarios;

    ProbeDesigns()
    {
        // The scouting's design: every kind that takes presets.
        RocketComponent& stage      = first->getRocket().getChild(0);
        auto&            nose       = stage.addChild(std::make_unique<NoseCone>());
        auto&            front      = tube(stage, kFrontRadius);
        auto&            transition = stage.addChild(std::make_unique<Transition>());
        auto&            back       = tube(stage, kBackRadius);
        auto&            tail       = stage.addChild(std::make_unique<NoseCone>());
        tail.setFlipped(true);
        const auto& innerTube = front.addChild(std::make_unique<InnerTube>());
        const auto& ring      = front.addChild(std::make_unique<CenteringRing>());
        const auto& bulkhead  = front.addChild(std::make_unique<Bulkhead>());
        const auto& block     = front.addChild(std::make_unique<EngineBlock>());
        const auto& coupler   = front.addChild(std::make_unique<TubeCoupler>());
        const auto& parachute = front.addChild(std::make_unique<Parachute>());
        const auto& streamer  = front.addChild(std::make_unique<Streamer>());
        const auto& lug       = front.addChild(std::make_unique<LaunchLug>());
        const auto& button    = front.addChild(std::make_unique<RailButton>());
        const auto& tubeFins  = back.addChild(std::make_unique<TubeFinSet>());

        // A flipped nose cone right behind a transition of other radii (bug 16).
        RocketComponent& secondStage = second->getRocket().getChild(0);
        secondStage.addChild(std::make_unique<NoseCone>());
        tube(secondStage, kFrontRadius);
        auto& secondTransition = secondStage.addChild(std::make_unique<Transition>());
        secondTransition.setAftRadiusAutomatic(false);
        secondTransition.setAftRadius(kBackRadius);
        auto& secondTail = secondStage.addChild(std::make_unique<NoseCone>());
        secondTail.setFlipped(true);

        scenarios = {&nose,     &front,     &transition, &back,       &tail,  &innerTube,
                     &ring,     &bulkhead,  &block,      &coupler,    &lug,   &button,
                     &streamer, &parachute, &tubeFins,   &secondTail, &front, &transition};
    }

    static BodyTube& tube(RocketComponent& stage, double radius)
    {
        auto& tube = stage.addChild(std::make_unique<BodyTube>());
        tube.setOuterRadiusAutomatic(false);
        tube.setOuterRadius(radius);
        tube.setLength(kTubeLength);
        return tube;
    }
};

/// The designs, built once for the process.
[[nodiscard]] const ProbeDesigns& designs()
{
    static const ProbeDesigns kDesigns;
    return kDesigns;
}

/// Test services over a copy of the bundled presets with a scenario's preferences.
struct BundledServices
{
    TestGuiServices test;

    BundledServices(bool matchFore, bool matchAft)
    {
        test.presets() = ComponentPresetDatabase(QtRocket::Test::bundledPresetDatabase());
        test.preferences().setMatchForeDiameter(matchFore);
        test.preferences().setMatchAftDiameter(matchAft);
    }
};

/// The request the configuration dialog makes for @p component.
[[nodiscard]] UserPrompts::PresetRequest requestFor(const RocketComponent& component)
{
    const PresetDiameterFilters filters = PresetDiameterFilters::forComponent(component);
    return {.componentKind   = component.kind(),
            .foreDiameter    = filters.foreDiameter,
            .aftDiameter     = filters.aftDiameter,
            .flippedNoseCone = filters.flippedNoseCone};
}

/// The row @p offset of a pin table's part that starts at @p first.
template <class Table>
[[nodiscard]] const auto& pinAt(const Table& table, int first, int offset)
{
    return table.at(static_cast<std::size_t>(first) + static_cast<std::size_t>(offset));
}

/// The table's columns in its header's visual order (hidden ones included) as
/// "<header> <model column> <shown>" lines, the pins' or the table's.
[[nodiscard]] std::vector<std::string> pinnedColumns(const PresetChooserScenarioPin& pin)
{
    std::vector<std::string> lines;
    for (int i = 0; i < pin.columnCount; ++i)
    {
        const auto& column = pinAt(kPresetChooserColumns, pin.firstColumn, i);
        lines.push_back(std::string(column.header) + " " + std::to_string(column.modelColumn) +
                        (column.shown ? " shown" : " hidden"));
    }
    return lines;
}

[[nodiscard]] std::vector<std::string> tableColumns(const ComponentPresetTable& table)
{
    std::vector<std::string> lines;
    const QHeaderView&       header = *table.horizontalHeader();
    for (int visual = 0; visual < header.count(); ++visual)
    {
        const int column = header.logicalIndex(visual);
        if (table.isTableColumn(column))
        {
            lines.push_back(QtStrings::toUtf8(ComponentPresetTableModel::columnName(column)) + " " +
                            std::to_string(column) +
                            (table.isColumnVisible(column) ? " shown" : " hidden"));
        }
    }
    return lines;
}

/// The design's symmetric components as "<name> <fore radius> <aft radius>" lines (exact
/// doubles), the pins' or the design's.
[[nodiscard]] std::vector<std::string> pinnedRadii(const PresetChooserScenarioPin& pin)
{
    std::vector<std::string> lines;
    for (int i = 0; i < pin.radiiCount; ++i)
    {
        const auto& radii = pinAt(kPresetChooserRadii, pin.firstRadii, i);
        lines.push_back(std::format("{} {:a} {:a}", radii.component, radii.fore, radii.aft));
    }
    return lines;
}

[[nodiscard]] std::vector<std::string> designRadii(const RocketComponent& component)
{
    std::vector<std::string> lines;
    const RocketComponent&   stage = component.getRocket().getChild(0);
    for (std::size_t i = 0; i < stage.getChildCount(); ++i)
    {
        if (const auto* symmetric = dynamic_cast<const SymmetricComponent*>(&stage.getChild(i));
            symmetric != nullptr)
        {
            lines.push_back(std::format("{} {:a} {:a}", displayName(symmetric->kind()),
                                        symmetric->getForeRadius(), symmetric->getAftRadius()));
        }
    }
    return lines;
}

/// Compares the box @p box (null: not made) with @p pin.
void expectBox(const PresetChooserBoxPin& pin, const QCheckBox* box, std::string_view name)
{
    SCOPED_TRACE(name);
    if (!pin.shown)
    {
        // OpenRocket makes a diameter box it never adds; QtRocket makes none.
        EXPECT_EQ(box, nullptr);
        return;
    }
    ASSERT_NE(box, nullptr);
    EXPECT_EQ(QtStrings::toUtf8(box->text()), pin.text);
    EXPECT_EQ(QtStrings::toUtf8(box->toolTip()), pin.toolTip);
    EXPECT_EQ(box->isChecked(), pin.checked);
}

/// Does a view pin's step to @p dialog.
void doStep(const Dialog& dialog, const PresetChooserViewPin& view)
{
    switch (view.step)
    {
        case PresetChooserStep::OPEN:
            return;
        case PresetChooserStep::FORE_OFF:
        case PresetChooserStep::FORE_ON:
            ASSERT_NE(dialog.foreDiameterFilterCheckBox(), nullptr);
            dialog.foreDiameterFilterCheckBox()->setChecked(view.step ==
                                                            PresetChooserStep::FORE_ON);
            return;
        case PresetChooserStep::AFT_OFF:
        case PresetChooserStep::AFT_ON:
            ASSERT_NE(dialog.aftDiameterFilterCheckBox(), nullptr);
            dialog.aftDiameterFilterCheckBox()->setChecked(view.step == PresetChooserStep::AFT_ON);
            return;
        case PresetChooserStep::LEGACY_ON:
        case PresetChooserStep::LEGACY_OFF:
            dialog.showLegacyCheckBox().setChecked(view.step == PresetChooserStep::LEGACY_ON);
            return;
        case PresetChooserStep::TEXT:
            dialog.filterText().setText(QtStrings::fromUtf8(view.text));
            return;
    }
}

/// The diameter filter of the request against the pins: none, or the value and the tolerance.
void expectFilter(std::optional<double> diameter, bool pinned, double value, double tolerance,
                  std::string_view name)
{
    SCOPED_TRACE(name);
    if (!diameter.has_value())
    {
        EXPECT_FALSE(pinned);
        return;
    }
    EXPECT_TRUE(pinned);
    EXPECT_EQ(*diameter, value);
    EXPECT_EQ(ComponentPresetFilterModel::diameterTolerance(*diameter), tolerance);
}

/// The scenario's component (its name and preset type) and its design's radii against the pins.
void expectComponent(const PresetChooserScenarioPin& pin, const RocketComponent& component)
{
    EXPECT_EQ(displayName(component.kind()), pin.componentName);
    const std::optional<ComponentPresetType> type = presetTypeOf(component.kind());
    if (!type.has_value())
    {
        ADD_FAILURE() << "a component without presets";
        return;
    }
    EXPECT_EQ(componentPresetTypeName(*type), pin.presetType);
    EXPECT_EQ(designRadii(component), pinnedRadii(pin));
}

/// The table's columns, the boxes and "Always open this dialog" against the pins.
void expectParts(const PresetChooserScenarioPin& pin, const Dialog& dialog)
{
    const ComponentPresetTable& table = dialog.table();
    EXPECT_EQ(tableColumns(table), pinnedColumns(pin));
    EXPECT_EQ(table.isTableColumn(ComponentPresetTableModel::kLegacyColumn)
                  ? ComponentPresetTableModel::kLegacyColumn
                  : -1,
              pin.legacyColumn);
    EXPECT_EQ(table.filterModel().foreDiameterColumn(), pin.foreColumn);
    EXPECT_EQ(table.filterModel().aftDiameterColumn(), pin.aftColumn);
    expectBox(pin.legacyBox, &dialog.showLegacyCheckBox(), "legacy box");
    expectBox(pin.foreBox, dialog.foreDiameterFilterCheckBox(), "fore box");
    expectBox(pin.aftBox, dialog.aftDiameterFilterCheckBox(), "aft box");
    EXPECT_EQ(QtStrings::toUtf8(dialog.alwaysOpenPresetCheckBox().text()), pin.alwaysOpenText);
    EXPECT_EQ(dialog.alwaysOpenPresetCheckBox().isChecked(), pin.alwaysOpenChecked);
}

/// The rows after each of the probe's steps.
void expectViews(const PresetChooserScenarioPin& pin, const Dialog& dialog)
{
    for (int i = 0; i < pin.viewCount; ++i)
    {
        const PresetChooserViewPin& view = pinAt(kPresetChooserViews, pin.firstView, i);
        SCOPED_TRACE("step " + std::to_string(i) + " '" + std::string(view.text) + "'");
        doStep(dialog, view);
        EXPECT_EQ(presetDigestOfView(dialog.table().filterModel()), view.view);
    }
}

/// The dialog of scenario @p index against its pins.
void expectScenario(std::size_t index)
{
    const PresetChooserScenarioPin& pin = kPresetChooserScenarios.at(index);
    SCOPED_TRACE(std::string(pin.name));
    const RocketComponent& component = *designs().scenarios.at(index);
    expectComponent(pin, component);

    BundledServices services(pin.matchFore, pin.matchAft);
    if (pin.alwaysOpen != PresetChooserAlwaysOpen::UNSET)
    {
        services.test.preferences().putBoolean(
            GuiPreferenceKeys::alwaysOpenPresetKey(component.kind()),
            pin.alwaysOpen == PresetChooserAlwaysOpen::ON);
    }
    const UserPrompts::PresetRequest request = requestFor(component);
    expectFilter(request.foreDiameter, pin.hasForeFilter, pin.foreDiameter, pin.foreTolerance,
                 "fore filter");
    expectFilter(request.aftDiameter, pin.hasAftFilter, pin.aftDiameter, pin.aftTolerance,
                 "aft filter");

    const Dialog dialog(services.test.services(), request);
    expectParts(pin, dialog);
    expectViews(pin, dialog);
    EXPECT_EQ(services.test.preferences().isMatchForeDiameter(), pin.matchForeAfter);
    EXPECT_EQ(services.test.preferences().isMatchAftDiameter(), pin.matchAftAfter);
}

void expectScenarios(std::size_t first, std::size_t count)
{
    for (std::size_t index = first; index < first + count; ++index)
    {
        expectScenario(index);
    }
}

/// The "tables" node as the pins write it: "key=value" in byte order separated by ';', a width as
/// "cw".
[[nodiscard]] std::string storedTables(const Preferences& preferences)
{
    const Preferences* const node = preferences.findNode(GuiPreferenceKeys::kNodeTables);
    std::string              line;
    if (node == nullptr)
    {
        return line;
    }
    for (const std::string& key : node->keys())
    {
        line += (line.empty() ? "" : ";") + key + "=" +
                (key.contains(".cw.") ? "cw" : node->get(key).value_or(""));
    }
    return line;
}

/// The digest of what @p dialog answered, empty for no preset.
[[nodiscard]] std::string applied(const Dialog& dialog)
{
    return dialog.answer().preset == nullptr ? std::string() : dialog.answer().preset->getDigest();
}

/// Selects view row @p row with the cell of view column @p viewColumn current (Java:
/// changeSelection(row, column, false, false)).
void selectCell(const ComponentPresetTable& table, int row, int viewColumn)
{
    const std::vector<int> columns = table.visibleColumns();
    table.selectionModel()->setCurrentIndex(
        table.filterModel().index(row, columns.at(static_cast<std::size_t>(viewColumn))),
        QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
}

/// A CLOSE or DOUBLE_CLICK action on a new dialog.
void expectCloseAction(const PresetChooserActionPin& pin)
{
    const RocketComponent& component =
        *designs().scenarios.at(static_cast<std::size_t>(pin.scenario));
    BundledServices services(false, false);
    const Dialog    dialog(services.test.services(), requestFor(component));
    if (pin.kind == PresetChooserActionKind::CLOSE)
    {
        if (pin.viewRow >= 0)
        {
            dialog.table().selectRow(pin.viewRow);
        }
        dialog.closeButton().click();
        EXPECT_EQ(dialog.result(), QDialog::Accepted);
    }
    else
    {
        // MouseAdapter.mouseClicked() with a click count of 2 and the cell selected; the double
        // click itself is QAbstractItemView's (ComponentPresetChooserDialogTests.cpp).
        selectCell(dialog.table(), pin.viewRow, pin.viewColumn);
        Q_EMIT dialog.table().doubleClicked(dialog.table().currentIndex());
    }
    EXPECT_EQ(applied(dialog), pin.applied);
    EXPECT_EQ(storedTables(services.test.preferences()), pin.tables);
}

TEST_F(PresetChooserPinsGolden, TheBundledPresetsAreTheProbes)
{
    EXPECT_EQ(QtRocket::Test::bundledPresetDatabase().size(),
              static_cast<std::size_t>(Gui::Test::kPresetChooserPresetsKept));
    ASSERT_EQ(kPresetChooserScenarios.size(), designs().scenarios.size());
}

TEST_F(PresetChooserPinsGolden, TheDialogsTextsAreOpenRockets)
{
    const BundledServices services(true, true);
    const Dialog dialog(services.test.services(), {.componentKind   = ComponentKind::BODY_TUBE,
                                                   .foreDiameter    = std::nullopt,
                                                   .aftDiameter     = std::nullopt,
                                                   .flippedNoseCone = false});
    EXPECT_EQ(QtStrings::toUtf8(dialog.windowTitle()), Gui::Test::kPresetChooserTitle);
    EXPECT_EQ(QtStrings::toUtf8(dialog.filterLabel().text()), Gui::Test::kPresetChooserFilterLabel);
    EXPECT_EQ(QtStrings::toUtf8(dialog.favoritesLabel().text()),
              Gui::Test::kPresetChooserFavoritesLabel);
    EXPECT_EQ(QtStrings::toUtf8(dialog.closeButton().text()), Gui::Test::kPresetChooserCloseButton);
}

TEST_F(PresetChooserPinsGolden, TheSymmetricComponentsDialogsAreOpenRockets)
{
    // The nose cone, the two body tubes, the transition and the flipped nose cone.
    expectScenarios(0, 5);
}

TEST_F(PresetChooserPinsGolden, TheInnerComponentsDialogsAreOpenRockets)
{
    // The inner tube, centering ring, bulkhead, engine block, tube coupler, launch lug and rail
    // button.
    expectScenarios(5, 7);
}

TEST_F(PresetChooserPinsGolden, TheRecoveryAndTubeFinDialogsAreOpenRockets)
{
    // The streamer, the parachute and the tube fin set.
    expectScenarios(12, 3);
}

TEST_F(PresetChooserPinsGolden, TheFixedFlippedNoseConeAndThePreferencesAreOpenRockets)
{
    // Bug 16: OpenRocket compares with the transition's fore diameter, QtRocket with its aft one.
    const PresetChooserScenarioPin& fixed = kPresetChooserScenarios.at(15);
    EXPECT_NE(fixed.aftDiameter, fixed.openRocketAftDiameter);
    EXPECT_EQ(fixed.aftDiameter, 2.0 * kBackRadius);
    expectScenarios(15, 3);
}

TEST_F(PresetChooserPinsGolden, CloseAndADoubleClickAreOpenRockets)
{
    for (const PresetChooserActionPin& pin : kPresetChooserActions)
    {
        if (pin.kind != PresetChooserActionKind::ALWAYS_OPEN_CLICK)
        {
            SCOPED_TRACE("action " + std::to_string(static_cast<int>(pin.kind)) + " row " +
                         std::to_string(pin.viewRow) + " column " + std::to_string(pin.viewColumn));
            expectCloseAction(pin);
        }
    }
}

/// The ALWAYS_OPEN_CLICK actions, one click each on one dialog.
void expectAlwaysOpenClicks()
{
    std::unique_ptr<BundledServices> services;
    std::unique_ptr<Dialog>          dialog;
    for (const PresetChooserActionPin& pin : kPresetChooserActions)
    {
        if (pin.kind != PresetChooserActionKind::ALWAYS_OPEN_CLICK)
        {
            continue;
        }
        const RocketComponent& component =
            *designs().scenarios.at(static_cast<std::size_t>(pin.scenario));
        if (dialog == nullptr)
        {
            services = std::make_unique<BundledServices>(false, false);
            dialog   = std::make_unique<Dialog>(services->test.services(), requestFor(component));
        }
        dialog->alwaysOpenPresetCheckBox().click();
        const std::string key = GuiPreferenceKeys::alwaysOpenPresetKey(component.kind());
        EXPECT_EQ(key + "=" + services->test.preferences().get(key).value_or("unset"),
                  pin.alwaysOpenPreference);
        EXPECT_EQ(dialog->alwaysOpenPresetCheckBox().isChecked(), pin.alwaysOpenChecked);
        EXPECT_EQ(applied(*dialog), pin.applied);
    }
}

TEST_F(PresetChooserPinsGolden, AlwaysOpenThisDialogIsOpenRockets)
{
    expectAlwaysOpenClicks();
}

/// A widget's descriptor in the focus order as the probe writes it (its Swing class and text).
[[nodiscard]] std::string descriptor(const QWidget& widget)
{
    if (qobject_cast<const QLineEdit*>(&widget) != nullptr)
    {
        return "JTextField";
    }
    if (const auto* box = qobject_cast<const QCheckBox*>(&widget); box != nullptr)
    {
        return "JCheckBox " + QtStrings::toUtf8(box->text());
    }
    if (const auto* button = qobject_cast<const QPushButton*>(&widget); button != nullptr)
    {
        return "JButton " + QtStrings::toUtf8(button->text());
    }
    if (qobject_cast<const ComponentPresetTable*>(&widget) != nullptr)
    {
        return "ComponentPresetTable";
    }
    return "?" + QtStrings::toUtf8(widget.objectName());
}

/// The focus order of @p dialog: from the widget that has the focus as it opens, the widgets Tab
/// reaches, once round.
[[nodiscard]] std::vector<std::string> focusOrder(const QDialog& dialog)
{
    std::vector<std::string> order;
    const QWidget* const     first  = dialog.focusWidget();
    const QWidget*           widget = first;
    while (widget != nullptr)
    {
        if ((widget->focusPolicy() & Qt::TabFocus) != 0 && widget->isEnabledTo(&dialog) &&
            widget->isVisibleTo(&dialog))
        {
            order.push_back(descriptor(*widget));
        }
        widget = widget->nextInFocusChain();
        if (widget == first)
        {
            break;
        }
    }
    return order;
}

[[nodiscard]] std::vector<std::string> pinnedFocusOrder()
{
    return {kPresetChooserFocusOrder.begin(), kPresetChooserFocusOrder.end()};
}

/// A main window of the pins' size with a configuration dialog holding a panel, the parent the
/// dialog is made for.
struct WindowChain
{
    QWidget  main;
    QDialog* config = makeChild<QDialog>(&main);
    QWidget* panel  = makeChild<QWidget>(config);

    WindowChain()
    {
        main.resize(kPresetChooserWindow.ownerWidth, kPresetChooserWindow.ownerHeight);
    }
};

TEST_F(PresetChooserPinsGolden, TheWindowIsOpenRockets)
{
    const RocketComponent& component =
        *designs().scenarios.at(static_cast<std::size_t>(kPresetChooserWindow.scenario));
    const BundledServices services(false, false);
    const WindowChain     chain;
    Dialog                dialog(services.test.services(), requestFor(component), chain.panel);
    EXPECT_EQ(dialog.size(), QSize(kPresetChooserWindow.width, kPresetChooserWindow.height));
    EXPECT_TRUE(dialog.closeButton().isDefault());
    EXPECT_EQ(QtStrings::toUtf8(dialog.closeButton().text()), kPresetChooserWindow.defaultButton);
    EXPECT_EQ(dialog.table().filterModel().rowCount(), kPresetChooserWindow.rows);
    EXPECT_EQ(static_cast<int>(dialog.table().visibleColumns().size()),
              kPresetChooserWindow.columns);
    EXPECT_EQ(focusOrder(dialog), pinnedFocusOrder());
    const QSignalSpy rejected(&dialog, &QDialog::rejected);
    QTest::keyClick(&dialog.table(), Qt::Key_Escape);
    EXPECT_EQ(rejected.count() == 1, kPresetChooserWindow.escapeCloses);
    EXPECT_EQ(dialog.answer().outcome, UserPrompts::PresetOutcome::CLOSED);
}

/// Gives the table of @p dialog the start of @p pin: the text no preset has, or the cell.
void startKey(const Dialog& dialog, const PresetChooserKeyPin& pin)
{
    if (pin.emptyTable)
    {
        dialog.filterText().setText(QtStrings::fromUtf8(Gui::Test::kPresetChooserNoMatchText));
        EXPECT_EQ(dialog.table().filterModel().rowCount(), kPresetChooserWindow.noMatchRows);
    }
    else if (pin.startRow >= 0)
    {
        selectCell(dialog.table(), pin.startRow, pin.startColumn);
    }
}

/// Presses the key of @p pin in the table of @p dialog and compares the selection after it.
void pressKey(const Dialog& dialog, const PresetChooserKeyPin& pin)
{
    ComponentPresetTable& table = dialog.table();
    QTest::keyClick(&table, Qt::Key_Return,
                    pin.key == PresetChooserKey::ENTER ? Qt::NoModifier : Qt::ShiftModifier);
    EXPECT_EQ(TableRowTraversal::selectedRow(table), pin.row);
    if (pin.row >= 0)
    {
        EXPECT_EQ(table.viewColumn(table.currentIndex().column()), pin.column);
        EXPECT_EQ(table.currentIndex().row(), pin.rowLead);
    }
}

/// The key pins, each on a new dialog when it starts from one, else on the dialog of the one
/// before.
void expectKeys()
{
    const RocketComponent& component =
        *designs().scenarios.at(static_cast<std::size_t>(kPresetChooserWindow.scenario));
    const BundledServices   services(false, false);
    std::unique_ptr<Dialog> dialog;
    for (const PresetChooserKeyPin& pin : kPresetChooserKeys)
    {
        SCOPED_TRACE("key from " + std::to_string(pin.startRow) + ", " +
                     std::to_string(pin.startColumn));
        if (!pin.emptyTable && pin.startRow < 0)
        {
            dialog = std::make_unique<Dialog>(services.test.services(), requestFor(component));
        }
        ASSERT_NE(dialog, nullptr);
        startKey(*dialog, pin);
        pressKey(*dialog, pin);
    }
}

TEST_F(PresetChooserPinsGolden, EnterInTheTableIsOpenRockets)
{
    expectKeys();
}

}  // namespace

}  // namespace QtRocket::Gui
