// The model of a component's preset combo box (Java: PresetModel, swing gui/adaptors/
// PresetModel.java). OpenRocket has no JUnit test of it.
//
// Expectations marked "probe" are OpenRocket's own results, printed by
// tools/openrocket-gui-pins/preset-chooser/probe/PresetModelProbe.java (run on OpenRocket
// 5f164fd0e with JDK 17; tools/openrocket-gui-pins/preset-chooser/hand-pinned.txt keeps its
// transcript): PresetModel over a new design whose stage holds the default body tubes "A" and "B",
// the presets of smallPresetDatabase() (the same three files in the same order), the favourites in
// SwingPreferences. Its body tubes in database order are those of kTubes below; the probe ticks
// the eighth and the third as favourites ("Estes|85865", "Estes|31361").

#include "QtRocket/gui/binding/PresetBinding.h"

#include <array>
#include <cstddef>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "BindingTestSupport.h"
#include "GuiTestSupport.h"
#include "PresetTestSupport.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/OpenRocketDocumentFactory.h"
#include "QtRocket/gui/binding/ComponentSelection.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/NoseCone.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/preset/ComponentPreset.h"
#include "QtRocket/rocket/preset/ComponentPresetDatabase.h"
#include "QtRocket/rocket/preset/ComponentPresetFavorites.h"
#include "QtRocket/rocket/preset/ComponentPresetType.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Signal.h"

namespace QtRocket::Gui
{

namespace
{

using PresetBindingTest = Test::GuiTest;
using Test::ChangeCounter;
using Test::PresetDesign;
using Test::PresetServices;
using Test::RocketEventRecorder;
using PresetPtr = PresetBinding::PresetPtr;

constexpr int kAeromass      = ComponentChangeEvent::kAeromassChange;
constexpr int kNonFunctional = ComponentChangeEvent::kNonFunctionalChange;

/// The undo text the dialog passes (Java: "Use Preset " + getComponentName()).
constexpr std::string_view kUsePresetBodyTube = "Use Preset Body Tube";

// probe: the body tubes of the database, in its order ("<preference key> OR <outer radius>").
constexpr std::array<std::string_view, 12> kTubes{
    "Estes|30615", "Estes|31360", "Estes|31361", "Estes|31380", "Estes|31390", "Estes|31751",
    "Estes|31799", "Estes|85865", "Estes|85866", "Estes|85867", "Estes|85868", "Estes|BT-20",
};

/// "<manufacturer> <part number> OR <outer radius> L <length>" or "null ..." as the probe prints a
/// tube.
struct TubeState
{
    std::optional<std::string> preset;
    double                     outerRadius{0};
    double                     length{0};

    bool operator==(const TubeState&) const = default;
};

[[nodiscard]] TubeState stateOf(const BodyTube& tube)
{
    const ComponentPreset* preset = tube.getPresetComponent();
    return TubeState{
        .preset = preset != nullptr ? std::optional<std::string>{preset->toString()} : std::nullopt,
        .outerRadius = tube.getOuterRadius(),
        .length      = tube.getLength()};
}

/// A binding of the tube @p main (with @p others) of @p design.
[[nodiscard]] std::shared_ptr<PresetBinding> bindingOf(
    PresetDesign& design, const PresetServices& services, RocketComponent& main,
    std::span<RocketComponent* const> others = {})
{
    return std::make_shared<PresetBinding>(std::make_shared<ComponentSelection>(main, others),
                                           *design.document, services.presets,
                                           services.preferences);
}

/// The part numbers of the binding's favourites.
[[nodiscard]] std::vector<std::string> favoriteTexts(const PresetBinding& binding)
{
    std::vector<std::string> texts;
    for (const PresetPtr& preset : binding.favorites())
    {
        texts.push_back(preset->toString());
    }
    return texts;
}

/// A setter for apply(): the length of a body tube (the session's edit before the preset).
struct SetLength
{
    double length;

    void operator()(BodyTube& tube) const { tube.setLength(length); }
};

/// A change listener that chooses a preset while the binding tells it (a combo box writing back
/// what it is told) and records what choose() answered.
struct ChooseWhileFiring
{
    PresetBinding*     binding;
    PresetPtr          preset;
    std::vector<bool>* answers;

    void operator()() const { answers->push_back(binding->choose(preset, kUsePresetBodyTube)); }
};

}  // namespace

TEST_F(PresetBindingTest, TheBodyTubesOfTheSmallDatabaseAreTheProbes)
{
    const PresetServices services;
    ASSERT_EQ(services.tubes.size(), kTubes.size());
    for (std::size_t i = 0; i < kTubes.size(); ++i)
    {
        EXPECT_EQ(services.tubes.at(i)->preferenceKey(), kTubes.at(i)) << i;
    }
}

TEST_F(PresetBindingTest, TheFavouritesAreListedInTheDatabasesOrder)
{
    PresetDesign   design;
    PresetServices services;
    const auto     binding = bindingOf(design, services, *design.a);
    // probe: "no favourite: size 1: Custom; selected Custom".
    EXPECT_EQ(binding->getType(), ComponentPresetType::BODY_TUBE);
    EXPECT_TRUE(binding->favorites().empty());
    EXPECT_EQ(binding->value(), nullptr);
    EXPECT_EQ(binding->valueText(), "");
    EXPECT_EQ(binding->valueIndex(), -1);

    // probe: "favourites [Estes|31361, Estes|85865]", "two favourites: size 3: Custom | 31361 |
    // 85865": the order of the database, not of the ticks.
    const ChangeCounter changes(*binding);
    services.tickFavorites();
    binding->reread();
    EXPECT_EQ(favoriteTexts(*binding), (std::vector<std::string>{"31361", "85865"}));
    EXPECT_EQ(binding->favorites()[0], services.tubes[2]);
    EXPECT_EQ(binding->favorites()[1], services.tubes[7]);
    EXPECT_EQ(changes.count(), 1);
    // Nothing changed: nothing told.
    binding->reread();
    EXPECT_EQ(changes.count(), 1);
}

TEST_F(PresetBindingTest, AnEventOfTheRocketReadsTheFavouritesAgain)
{
    PresetDesign   design;
    PresetServices services;
    const auto     binding = bindingOf(design, services, *design.a);
    services.tickFavorites();
    EXPECT_TRUE(binding->favorites().empty());
    design.b->setLength(0.3);
    EXPECT_EQ(favoriteTexts(*binding), (std::vector<std::string>{"31361", "85865"}));
    ComponentPresetFavorites::setFavorite(services.preferences, *services.tubes[2],
                                          ComponentPresetType::BODY_TUBE, false);
    design.b->setLength(0.35);
    EXPECT_EQ(favoriteTexts(*binding), (std::vector<std::string>{"85865"}));
}

TEST_F(PresetBindingTest, APresetChosenInACleanSessionRenamesItsStep)
{
    PresetDesign   design;
    PresetServices services;
    services.tickFavorites();
    const auto          binding = bindingOf(design, services, *design.a);
    RocketEventRecorder events(design.document->getRocket());
    const ChangeCounter changes(*binding);
    const PresetPtr     first = services.tubes[7];

    // probe: "chosen events: 6 A, 1 A", "chosen: undo 'Use Preset Body Tube' redo null; A Estes
    // 85865 OR 0.03175 L 0.3937", "selected 85865".
    design.document->addUndoPosition("Modify Body Tube");
    EXPECT_TRUE(binding->choose(first, kUsePresetBodyTube));
    EXPECT_EQ(events.types(), (std::vector<int>{kAeromass, kNonFunctional}));
    EXPECT_EQ(events.sources(), (std::vector<std::string>{"A", "A"}));
    EXPECT_EQ(design.document->getUndoDescription(), kUsePresetBodyTube);
    EXPECT_FALSE(design.document->isRedoAvailable());
    EXPECT_EQ(stateOf(*design.a),
              (TubeState{.preset = "85865", .outerRadius = 0.03175, .length = 0.3937}));
    EXPECT_EQ(binding->value(), first.get());
    EXPECT_EQ(binding->valueText(), "85865");
    EXPECT_EQ(binding->valueIndex(), 1);
    EXPECT_GE(changes.count(), 1);

    // probe: "chosen again events: " (none), "undo 'Use Preset Body Tube'".
    events.clear();
    EXPECT_TRUE(binding->choose(first, kUsePresetBodyTube));
    EXPECT_TRUE(events.types().empty());
    EXPECT_EQ(design.document->getUndoDescription(), kUsePresetBodyTube);

    // probe: "length edited ... L 0.25" (a body tube keeps its preset), "chosen after the edit
    // events: " (none: the same preset loads nothing), then one undo takes back the length and the
    // second the preset.
    design.a->setLength(0.25);
    events.clear();
    EXPECT_TRUE(binding->choose(first, kUsePresetBodyTube));
    EXPECT_TRUE(events.types().empty());
    EXPECT_EQ(stateOf(*design.a),
              (TubeState{.preset = "85865", .outerRadius = 0.03175, .length = 0.25}));
    EXPECT_EQ(design.document->getUndoDescription(), kUsePresetBodyTube);

    design.document->undo();
    EXPECT_FALSE(binding->isValid());
    EXPECT_EQ(design.document->getUndoDescription(), kUsePresetBodyTube);
    EXPECT_EQ(design.document->getRedoDescription(), kUsePresetBodyTube);
    EXPECT_EQ(stateOf(design.tube(0)),
              (TubeState{.preset = "85865", .outerRadius = 0.03175, .length = 0.3937}));
    design.document->undo();
    EXPECT_EQ(design.document->getUndoDescription(), std::nullopt);
    EXPECT_EQ(design.document->getRedoDescription(), kUsePresetBodyTube);
    EXPECT_EQ(stateOf(design.tube(0)),
              (TubeState{.preset = std::nullopt, .outerRadius = 0.025, .length = 0.2}));

    // An invalid binding accepts nothing.
    EXPECT_FALSE(binding->choose(first, kUsePresetBodyTube));
    EXPECT_FALSE(binding->clear());
    binding->reread();
}

TEST_F(PresetBindingTest, CustomClearsThePresetWithoutAnUndoPosition)
{
    PresetDesign   design;
    PresetServices services;
    services.tickFavorites();
    design.a->loadPreset(services.tubes[10]);
    const auto          binding = bindingOf(design, services, *design.a);
    RocketEventRecorder events(design.document->getRocket());

    // probe: "other: ... selected 85868": a preset that is no favourite is the value, not an item.
    EXPECT_EQ(binding->value(), services.tubes[10].get());
    EXPECT_EQ(binding->valueText(), "85868");
    EXPECT_EQ(binding->valueIndex(), -1);
    EXPECT_EQ(favoriteTexts(*binding), (std::vector<std::string>{"31361", "85865"}));

    // probe: "Custom events: 1 A", "Custom: undo null redo null; A null OR 0.0508 L 0.52705",
    // "selected Custom".
    EXPECT_TRUE(binding->clear());
    EXPECT_EQ(events.types(), (std::vector<int>{kNonFunctional}));
    EXPECT_EQ(events.sources(), (std::vector<std::string>{"A"}));
    EXPECT_EQ(design.document->getUndoDescription(), std::nullopt);
    EXPECT_EQ(design.document->getRedoDescription(), std::nullopt);
    EXPECT_EQ(stateOf(*design.a),
              (TubeState{.preset = std::nullopt, .outerRadius = 0.0508, .length = 0.52705}));
    EXPECT_EQ(binding->value(), nullptr);
    EXPECT_EQ(binding->valueText(), "");

    // probe: "Custom again events: " (none).
    events.clear();
    EXPECT_TRUE(binding->clear());
    EXPECT_TRUE(events.types().empty());
}

TEST_F(PresetBindingTest, APresetReachesEverySelectedComponentTheOthersFirst)
{
    PresetDesign                          design;
    PresetServices                        services;
    const std::array<RocketComponent*, 1> others{design.b};
    const auto                            binding = bindingOf(design, services, *design.a, others);
    design.document->addUndoPosition("Modify Body Tube");
    binding->getSelection()->apply<BodyTube>(SetLength{0.4});
    RocketEventRecorder events(design.document->getRocket());

    // probe: "chosen: undo 'Use Preset Body Tube' redo null; A Estes 31361 OR 0.024384 L 0.0127;
    // B Estes 31361 OR 0.024384 L 0.0127", "chosen events: 6 A, 1 A". Java drops the events of
    // the config listener B (ComponentSelection.h, U3); here each component fires its own, B's
    // first.
    EXPECT_TRUE(binding->choose(services.tubes[2], kUsePresetBodyTube));
    EXPECT_EQ(events.types(),
              (std::vector<int>{kAeromass, kNonFunctional, kAeromass, kNonFunctional}));
    EXPECT_EQ(events.sources(), (std::vector<std::string>{"B", "B", "A", "A"}));
    const TubeState loaded{.preset = "31361", .outerRadius = 0.024384, .length = 0.0127};
    EXPECT_EQ(stateOf(*design.a), loaded);
    EXPECT_EQ(stateOf(*design.b), loaded);
    EXPECT_EQ(design.document->getUndoDescription(), kUsePresetBodyTube);

    // probe: "one undo: undo 'Modify Body Tube' redo 'Use Preset Body Tube'; A null OR 0.025 L
    // 0.4; B null OR 0.025 L 0.4": the session's edit is a step of its own; "redo: ...".
    design.document->undo();
    EXPECT_EQ(design.document->getUndoDescription(), "Modify Body Tube");
    EXPECT_EQ(design.document->getRedoDescription(), kUsePresetBodyTube);
    const TubeState edited{.preset = std::nullopt, .outerRadius = 0.025, .length = 0.4};
    EXPECT_EQ(stateOf(design.tube(0)), edited);
    EXPECT_EQ(stateOf(design.tube(1)), edited);
    design.document->redo();
    EXPECT_EQ(design.document->getUndoDescription(), kUsePresetBodyTube);
    EXPECT_EQ(stateOf(design.tube(0)), loaded);
    EXPECT_EQ(stateOf(design.tube(1)), loaded);
}

TEST_F(PresetBindingTest, TheMainComponentsOwnPresetStillReachesTheOthers)
{
    PresetDesign   design;
    PresetServices services;
    design.a->loadPreset(services.tubes[7]);
    design.b->loadPreset(services.tubes[2]);
    design.document->clearUndo();
    const std::array<RocketComponent*, 1> others{design.b};
    const auto                            binding = bindingOf(design, services, *design.a, others);
    design.document->addUndoPosition("Modify Body Tube");
    RocketEventRecorder events(design.document->getRocket());

    // probe: "chosen events: " (none), "chosen: undo null redo null; A Estes 85865 OR 0.03175 L
    // 0.3937; B Estes 85865 OR 0.03175 L 0.3937". Java's loadPreset() forwards to B before it
    // finds A's preset the same; B's events are dropped, so OpenRocket's document sees no change
    // and B's new preset is in no undo step. Here B's events reach the document: the session's
    // step, renamed, holds it.
    EXPECT_TRUE(binding->choose(services.tubes[7], kUsePresetBodyTube));
    EXPECT_EQ(events.sources(), (std::vector<std::string>{"B", "B"}));
    const TubeState first{.preset = "85865", .outerRadius = 0.03175, .length = 0.3937};
    EXPECT_EQ(stateOf(*design.a), first);
    EXPECT_EQ(stateOf(*design.b), first);
    EXPECT_EQ(design.document->getUndoDescription(), kUsePresetBodyTube);

    // probe: "Custom events: 1 A", "Custom: undo 'Use Preset Body Tube' redo null; A null OR
    // 0.03175 L 0.3937; B null ...": both cleared; here in one merged event (apply()).
    events.clear();
    EXPECT_TRUE(binding->clear());
    EXPECT_EQ(events.types(), (std::vector<int>{kNonFunctional}));
    EXPECT_EQ(events.sources(), (std::vector<std::string>{"A"}));
    const TubeState cleared{.preset = std::nullopt, .outerRadius = 0.03175, .length = 0.3937};
    EXPECT_EQ(stateOf(*design.a), cleared);
    EXPECT_EQ(stateOf(*design.b), cleared);
    EXPECT_EQ(design.document->getUndoDescription(), kUsePresetBodyTube);
}

TEST_F(PresetBindingTest, AChoiceWhileTheBindingTellsItsListenersIsDropped)
{
    PresetDesign                     design;
    PresetServices                   services;
    const auto                       binding = bindingOf(design, services, *design.a);
    std::vector<bool>                answers;
    const Signal<>::ScopedConnection echo{binding->addChangeListener(ChooseWhileFiring{
        .binding = binding.get(), .preset = services.tubes[0], .answers = &answers})};
    EXPECT_TRUE(binding->choose(services.tubes[7], kUsePresetBodyTube));
    ASSERT_FALSE(answers.empty());
    EXPECT_EQ(answers, std::vector<bool>(answers.size(), false));
    EXPECT_EQ(stateOf(*design.a).preset, "85865");
}

TEST_F(PresetBindingTest, AWrongSelectionOrPresetIsABug)
{
    PresetDesign         design;
    const PresetServices services;
    EXPECT_THROW((PresetBinding{nullptr, *design.document, services.presets, services.preferences}),
                 BugError);

    // Several kinds (Java shows no preset combo then).
    RocketComponent& nose =
        design.document->getRocket().getChild(0).addChild(std::make_unique<NoseCone>(), 0);
    const std::array<RocketComponent*, 1> mixed{&nose};
    EXPECT_THROW(std::ignore = bindingOf(design, services, *design.a, mixed), BugError);
    // A component that takes no presets.
    EXPECT_THROW(
        std::ignore = bindingOf(design, services, design.document->getRocket().getChild(0)),
        BugError);
    // Another document.
    const PresetDesign other;
    EXPECT_THROW((PresetBinding{std::make_shared<ComponentSelection>(*design.a), *other.document,
                                services.presets, services.preferences}),
                 BugError);

    const auto binding = bindingOf(design, services, *design.a);
    EXPECT_THROW(std::ignore = binding->choose(nullptr, kUsePresetBodyTube), BugError);
    const std::vector<PresetPtr> noses =
        services.presets.listForType(ComponentPresetType::NOSE_CONE);
    ASSERT_FALSE(noses.empty());
    EXPECT_THROW(std::ignore = binding->choose(noses.front(), kUsePresetBodyTube), BugError);
    EXPECT_EQ(design.a->getPresetComponent(), nullptr);
    EXPECT_EQ(design.document->getUndoDescription(), std::nullopt);
}

TEST_F(PresetBindingTest, AnUndoEndsTheBindingBeforeTheComponentsGo)
{
    // Rocket::loadFrom() replaces every component: the binding lets go of the main component's
    // preset pointer with the selection and reads nothing afterwards (proved under ASan).
    PresetDesign   design;
    PresetServices services;
    services.tickFavorites();
    const auto          binding = bindingOf(design, services, *design.a);
    const ChangeCounter changes(*binding);
    design.document->addUndoPosition("Modify Body Tube");
    EXPECT_TRUE(binding->choose(services.tubes[7], kUsePresetBodyTube));
    const int before = changes.count();
    design.document->undo();
    EXPECT_FALSE(binding->isValid());
    EXPECT_EQ(changes.count(), before + 1);
    EXPECT_EQ(binding->valueText(), "85865");
}

}  // namespace QtRocket::Gui
