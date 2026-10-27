// The preset combo box of a component dialog (Java: the JComboBox on a PresetModel that
// RocketComponentConfig makes, RocketComponentConfig.java:160-169). OpenRocket has no JUnit test of
// it.
//
// Expectations marked "probe" are OpenRocket's own results, printed by the Java probes of
// tools/openrocket-gui-pins/preset-chooser (run on OpenRocket 5f164fd0e with JDK 17; the family's
// hand-pinned.txt keeps their transcript): PresetModelProbe (the model's items and what choosing
// does, over PresetTestSupport.h's design and presets) and PresetComboProbe (what a JComboBox shows
// of its model's selected item and which choices it hands the model).

#include "QtRocket/gui/widgets/PresetComboBox.h"

#include <array>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

#include <QComboBox>
#include <QFont>
#include <QLabel>
#include <QSignalSpy>
#include <QString>
#include <QStringList>
#include <QStyle>
#include <QVariant>
#include <QWidget>
#include <Qt>
#include <QtContainerFwd>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/gui/binding/ComponentSelection.h"
#include "QtRocket/gui/binding/PresetBinding.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/preset/ComponentPreset.h"
#include "QtRocket/rocket/preset/ComponentPresetFavorites.h"
#include "QtRocket/rocket/preset/ComponentPresetType.h"
#include "QtRocket/util/BugError.h"
#include "WidgetTestSupport.h"
#include "binding/BindingTestSupport.h"
#include "binding/PresetTestSupport.h"

namespace QtRocket::Gui
{

namespace
{

using PresetComboBoxTest = Test::GuiTest;
using Test::PresetDesign;
using Test::PresetServices;
using Test::RocketEventRecorder;
using Test::showActiveWith;
using Test::turnWheel;
using PresetPtr = PresetBinding::PresetPtr;

/// The undo text of a body tube's preset (Java: "Use Preset " + getComponentName()).
constexpr std::string_view kUsePresetBodyTube = "Use Preset Body Tube";

/// The items of @p combo.
[[nodiscard]] QStringList items(const QComboBox& combo)
{
    QStringList texts{};
    for (int index = 0; index < combo.count(); ++index)
    {
        texts.append(combo.itemText(index));
    }
    return texts;
}

/// A binding of the tube @p main (with @p others) of @p design.
[[nodiscard]] std::shared_ptr<PresetBinding> presetBindingOf(
    PresetDesign& design, const PresetServices& services, RocketComponent& main,
    std::span<RocketComponent* const> others = {})
{
    return std::make_shared<PresetBinding>(std::make_shared<ComponentSelection>(main, others),
                                           *design.document, services.presets,
                                           services.preferences);
}

/// The part number of @p tube's preset, or nullopt for none.
[[nodiscard]] std::optional<std::string> presetOf(const BodyTube& tube)
{
    const ComponentPreset* preset = tube.getPresetComponent();
    return preset != nullptr ? std::optional<std::string>{preset->toString()} : std::nullopt;
}

TEST_F(PresetComboBoxTest, NeedsABindingOfAValidSelection)
{
    PresetDesign   design;
    PresetServices services;
    EXPECT_THROW(std::ignore = PresetComboBox(nullptr, nullptr), BugError);
    const auto binding = presetBindingOf(design, services, *design.a);
    binding->getSelection()->release();
    EXPECT_THROW(std::ignore = PresetComboBox(binding, nullptr), BugError);
}

TEST_F(PresetComboBoxTest, TheTextsAreOpenRockets)
{
    EXPECT_EQ(PresetComboBox::customText(), QStringLiteral("Custom"));
    EXPECT_EQ(PresetComboBox::undoDescription(ComponentKind::BODY_TUBE),
              QStringLiteral("Use Preset Body Tube"));
    EXPECT_EQ(PresetComboBox::undoDescription(ComponentKind::INNER_TUBE),
              QStringLiteral("Use Preset Inner Tube"));
    EXPECT_EQ(PresetComboBox::undoDescription(ComponentKind::BULKHEAD),
              QStringLiteral("Use Preset Bulkhead"));
}

TEST_F(PresetComboBoxTest, CustomInItalicsThenTheFavouritesByPartNumber)
{
    PresetDesign   design;
    PresetServices services;
    // probe: "no favourite: size 1: <html><i>Custom</i></html>; selected
    // <html><i>Custom</i></html>".
    const PresetComboBox lonely(presetBindingOf(design, services, *design.a), nullptr);
    EXPECT_EQ(items(lonely), QStringList{QStringLiteral("Custom")});
    EXPECT_EQ(lonely.currentIndex(), 0);
    EXPECT_EQ(lonely.shownText(), QStringLiteral("Custom"));
    EXPECT_TRUE(lonely.shownFont().italic());
    EXPECT_TRUE(lonely.itemData(0, Qt::FontRole).value<QFont>().italic());

    // probe: "two favourites: size 3: <html><i>Custom</i></html> | 31361 | 85865; selected
    // <html><i>Custom</i></html>"; PresetComboProbe "items 3 maximum row count 25 editable false".
    services.tickFavorites();
    const PresetComboBox combo(presetBindingOf(design, services, *design.a), nullptr);
    EXPECT_EQ(items(combo), (QStringList{QStringLiteral("Custom"), QStringLiteral("31361"),
                                         QStringLiteral("85865")}));
    EXPECT_EQ(combo.shownFavorites(),
              (std::vector<PresetPtr>{services.tubes.at(2), services.tubes.at(7)}));
    EXPECT_EQ(combo.currentIndex(), 0);
    EXPECT_FALSE(combo.itemData(1, Qt::FontRole).isValid());
    EXPECT_EQ(combo.maxVisibleItems(), 25);
    EXPECT_FALSE(combo.isEditable());
    EXPECT_EQ(combo.focusPolicy(), Qt::StrongFocus);  // never the wheel's focus
    EXPECT_TRUE(combo.isEnabled());
    // PresetModel.combo.ttip.
    EXPECT_EQ(combo.toolTip(),
              QStringLiteral("<html>Select a preset model from a list of favorites (selected in "
                             "the component preset dialog), <br>or select 'Custom' when no preset "
                             "is required.</html>"));
}

TEST_F(PresetComboBoxTest, AFavouriteChosenIsLoadedInItsOwnUndoStep)
{
    PresetDesign   design;
    PresetServices services;
    services.tickFavorites();
    PresetComboBox      combo(presetBindingOf(design, services, *design.a), nullptr);
    const QSignalSpy    chosen(&combo, &PresetComboBox::presetChosen);
    RocketEventRecorder events(design.document->getRocket());
    design.document->addUndoPosition("Modify Body Tube");

    // probe: "chosen: undo 'Use Preset Body Tube' redo null; A Estes 85865 OR 0.03175 L 0.3937",
    // "selected 85865"; PresetComboProbe "first chosen: index 2 shown 85865 calls [85865@2]".
    Q_EMIT combo.activated(2);
    EXPECT_EQ(presetOf(*design.a), "85865");
    EXPECT_EQ(design.a->getLength(), 0.3937);
    EXPECT_EQ(presetOf(*design.b), std::nullopt);
    EXPECT_EQ(design.document->getUndoDescription(), kUsePresetBodyTube);
    EXPECT_EQ(events.sources(), (std::vector<std::string>{"A", "A"}));
    EXPECT_EQ(combo.currentIndex(), 2);
    EXPECT_EQ(combo.shownText(), QStringLiteral("85865"));
    EXPECT_FALSE(combo.shownFont().italic());
    EXPECT_EQ(chosen.count(), 1);
}

/// The session of the two tests below: tube A based on the favourite 85865, then the dialog's
/// undo position and an edit of the length, which keeps a body tube's preset.
void startSession(PresetDesign& design, const PresetServices& services)
{
    design.a->loadPreset(services.tubes.at(7));
    design.document->clearUndo();
    design.document->addUndoPosition("Modify Body Tube");
    design.a->setLength(0.25);
}

TEST_F(PresetComboBoxTest, TheCurrentItemChosenAgainDoesNothing)
{
    PresetDesign   design;
    PresetServices services;
    services.tickFavorites();
    PresetComboBox   combo(presetBindingOf(design, services, *design.a), nullptr);
    const QSignalSpy chosen(&combo, &PresetComboBox::presetChosen);
    startSession(design, services);
    RocketEventRecorder events(design.document->getRocket());

    // PresetComboProbe: "first chosen again: index 2 shown 85865 calls []": JComboBox leaves its
    // model alone, so nothing is loaded and no undo position is added: the next change is still
    // the session's.
    ASSERT_EQ(combo.currentIndex(), 2);
    Q_EMIT combo.activated(2);
    EXPECT_TRUE(events.types().empty());
    EXPECT_EQ(design.a->getLength(), 0.25);
    EXPECT_EQ(chosen.count(), 0);
    design.a->setLength(0.3);
    EXPECT_EQ(design.document->getUndoDescription(), "Modify Body Tube");

    // probe: "Custom again events: " (none): "Custom" while there is no preset.
    design.a->clearPreset();
    events.clear();
    ASSERT_EQ(combo.currentIndex(), 0);
    Q_EMIT combo.activated(0);
    EXPECT_TRUE(events.types().empty());
}

TEST_F(PresetComboBoxTest, ThePartsLibrarysChoiceIsTakenAlsoForTheCurrentPreset)
{
    // Java: the parts library's Close calls PresetModel.setSelectedItem() itself, so the undo
    // position comes also for the component's own preset. probe: "chosen after the edit events: "
    // (none: the same preset loads nothing), "chosen after the edit: undo 'Use Preset Body Tube'".
    PresetDesign   design;
    PresetServices services;
    services.tickFavorites();
    PresetComboBox   combo(presetBindingOf(design, services, *design.a), nullptr);
    const QSignalSpy chosen(&combo, &PresetComboBox::presetChosen);
    startSession(design, services);
    RocketEventRecorder events(design.document->getRocket());
    EXPECT_TRUE(combo.choosePreset(services.tubes.at(7)));
    EXPECT_TRUE(events.types().empty());
    EXPECT_EQ(design.a->getLength(), 0.25);
    EXPECT_EQ(chosen.count(), 1);
    EXPECT_EQ(design.document->getUndoDescription(), "Modify Body Tube");
    design.a->setLength(0.3);
    EXPECT_EQ(design.document->getUndoDescription(), kUsePresetBodyTube);

    // A preset that is no favourite: loaded, shown without an item.
    EXPECT_TRUE(combo.choosePreset(services.tubes.at(10)));
    EXPECT_EQ(presetOf(*design.a), "85868");
    EXPECT_EQ(combo.currentIndex(), -1);
    EXPECT_EQ(combo.shownText(), QStringLiteral("85868"));
    EXPECT_EQ(chosen.count(), 2);
    EXPECT_THROW(std::ignore = combo.choosePreset(nullptr), BugError);
}

TEST_F(PresetComboBoxTest, APresetThatIsNoFavouriteIsShownWithoutAnItem)
{
    PresetDesign   design;
    PresetServices services;
    services.tickFavorites();
    design.a->loadPreset(services.tubes.at(10));
    design.document->clearUndo();
    PresetComboBox      combo(presetBindingOf(design, services, *design.a), nullptr);
    RocketEventRecorder events(design.document->getRocket());

    // probe: "other: size 3: <html><i>Custom</i></html> | 31361 | 85865; selected 85868";
    // PresetComboProbe "the model selects a preset no item holds: index -1 shown 85868".
    EXPECT_EQ(items(combo), (QStringList{QStringLiteral("Custom"), QStringLiteral("31361"),
                                         QStringLiteral("85865")}));
    EXPECT_EQ(combo.currentIndex(), -1);
    EXPECT_EQ(combo.shownText(), QStringLiteral("85868"));
    EXPECT_FALSE(combo.shownFont().italic());

    // probe: "Custom events: 1 A", "Custom: undo null redo null; A null OR 0.0508 L 0.52705",
    // "selected <html><i>Custom</i></html>"; PresetComboProbe "Custom chosen while a preset no
    // item holds is selected: ... calls [<html><i>Custom</i></html>]".
    Q_EMIT combo.activated(0);
    EXPECT_EQ(events.types(), (std::vector<int>{ComponentChangeEvent::kNonFunctionalChange}));
    EXPECT_EQ(presetOf(*design.a), std::nullopt);
    EXPECT_EQ(design.a->getOuterRadius(), 0.0508);
    EXPECT_EQ(design.a->getLength(), 0.52705);
    EXPECT_EQ(design.document->getUndoDescription(), std::nullopt);
    EXPECT_EQ(combo.currentIndex(), 0);
    EXPECT_EQ(combo.shownText(), QStringLiteral("Custom"));
    EXPECT_TRUE(combo.shownFont().italic());
}

TEST_F(PresetComboBoxTest, APresetOfTheSameDigestIsTheSameItem)
{
    // Java's JComboBox compares with equals(), ComponentPreset's digest. PresetComboProbe: "copy
    // of 85865: equals true same false", "the model selects the copy: index 2 shown 85865 calls
    // []", "first chosen while the copy is selected: ... calls []".
    PresetDesign   design;
    PresetServices services;
    services.tickFavorites();
    const auto copy = std::make_shared<const ComponentPreset>(*services.tubes.at(7));
    ASSERT_NE(copy.get(), services.tubes.at(7).get());
    ASSERT_TRUE(*copy == *services.tubes.at(7));
    design.a->loadPreset(copy);
    design.document->clearUndo();
    PresetComboBox      combo(presetBindingOf(design, services, *design.a), nullptr);
    RocketEventRecorder events(design.document->getRocket());
    EXPECT_EQ(combo.currentIndex(), 2);
    EXPECT_EQ(combo.shownText(), QStringLiteral("85865"));
    Q_EMIT combo.activated(2);
    EXPECT_TRUE(events.types().empty());
    EXPECT_EQ(design.a->getPresetComponent(), copy.get());
    EXPECT_EQ(design.document->getUndoDescription(), std::nullopt);
}

TEST_F(PresetComboBoxTest, TheItemsFollowTheFavourites)
{
    PresetDesign         design;
    PresetServices       services;
    const PresetComboBox combo(presetBindingOf(design, services, *design.a), nullptr);
    EXPECT_EQ(combo.count(), 1);
    // The parts library ticked favourites: its caller reads them again.
    services.tickFavorites();
    combo.binding()->reread();
    EXPECT_EQ(items(combo), (QStringList{QStringLiteral("Custom"), QStringLiteral("31361"),
                                         QStringLiteral("85865")}));
    EXPECT_TRUE(combo.itemData(0, Qt::FontRole).value<QFont>().italic());
    // An event of the rocket reads them as well.
    ComponentPresetFavorites::setFavorite(services.preferences, *services.tubes.at(2),
                                          ComponentPresetType::BODY_TUBE, false);
    design.b->setLength(0.3);
    EXPECT_EQ(items(combo), (QStringList{QStringLiteral("Custom"), QStringLiteral("85865")}));
    EXPECT_EQ(combo.currentIndex(), 0);
}

TEST_F(PresetComboBoxTest, AChoiceReachesEverySelectedComponent)
{
    PresetDesign                          design;
    PresetServices                        services;
    const std::array<RocketComponent*, 1> others{design.b};
    services.tickFavorites();
    PresetComboBox combo(presetBindingOf(design, services, *design.a, others), nullptr);
    // probe: "two tubes, B a config listener of A": "chosen: ... A Estes 31361 ...; B Estes 31361".
    Q_EMIT combo.activated(1);
    EXPECT_EQ(presetOf(*design.a), "31361");
    EXPECT_EQ(presetOf(*design.b), "31361");
    EXPECT_EQ(design.document->getUndoDescription(), kUsePresetBodyTube);
    EXPECT_EQ(combo.currentIndex(), 1);
}

TEST_F(PresetComboBoxTest, AnUndoLeavesTheComboBoxDisabledWithItsLastState)
{
    PresetDesign   design;
    PresetServices services;
    services.tickFavorites();
    PresetComboBox combo(presetBindingOf(design, services, *design.a), nullptr);
    design.document->addUndoPosition("Modify Body Tube");
    Q_EMIT combo.activated(2);
    ASSERT_EQ(combo.shownText(), QStringLiteral("85865"));
    design.document->undo();  // replaces every component: the selection is invalid
    EXPECT_FALSE(combo.binding()->isValid());
    EXPECT_FALSE(combo.isEnabled());
    EXPECT_EQ(combo.shownText(), QStringLiteral("85865"));
    EXPECT_EQ(combo.count(), 3);
    // Nothing more is chosen.
    const QSignalSpy chosen(&combo, &PresetComboBox::presetChosen);
    Q_EMIT combo.activated(1);
    EXPECT_FALSE(combo.choosePreset(services.tubes.at(2)));
    EXPECT_EQ(chosen.count(), 0);
    EXPECT_EQ(presetOf(design.tube(0)), std::nullopt);
    combo.refresh();
    EXPECT_FALSE(combo.isEnabled());
}

TEST_F(PresetComboBoxTest, TheWheelChoosesOnlyWithTheFocus)
{
    PresetDesign   design;
    PresetServices services;
    services.tickFavorites();
    QWidget window;
    auto* combo = makeChild<PresetComboBox>(&window, presetBindingOf(design, services, *design.a));
    auto* other = makeChild<QLabel>(&window);
    other->setFocusPolicy(Qt::StrongFocus);
    ASSERT_TRUE(showActiveWith(window, {combo, other}));
    other->setFocus();
    EXPECT_FALSE(turnWheel(*combo, -120));
    EXPECT_EQ(presetOf(*design.a), std::nullopt);
    EXPECT_FALSE(combo->hasFocus());  // the wheel takes no focus

    // With the focus, Qt's rule: one notch down is the next item, where the style scrolls a combo
    // box by the wheel at all (not the macOS style).
    combo->setFocus();
    const bool scrolls =
        combo->style()->styleHint(QStyle::SH_ComboBox_AllowWheelScrolling, nullptr, combo) != 0;
    static_cast<void>(turnWheel(*combo, -120));
    EXPECT_EQ(presetOf(*design.a), scrolls ? std::optional<std::string>{"31361"} : std::nullopt);
}

}  // namespace

}  // namespace QtRocket::Gui
