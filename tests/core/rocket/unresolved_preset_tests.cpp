// The reference to a preset that was not found (the user's decision of 2026-10-08): what a
// design file says about the preset of a component, kept on the component when the application
// does not know the preset, so that a save can write it back. OpenRocket has no such field, so
// nothing here is compared with it; the tests pin what RocketComponent.h says about it: who
// sets it, what drops it, that it is copied with the component and takes part in nothing else.

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/FinSet.h"
#include "QtRocket/rocket/FreeformFinSet.h"
#include "QtRocket/rocket/NoseCone.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/preset/ComponentPreset.h"
#include "QtRocket/rocket/preset/ComponentPresetDatabase.h"
#include "QtRocket/rocket/preset/ComponentPresetReference.h"
#include "QtRocket/util/ModId.h"
#include "rocket/TestRockets.h"
#include "rocket/preset/ExamplePresets.h"

namespace
{

using QtRocket::AxialStage;
using QtRocket::BodyTube;
using QtRocket::ComponentChangeEvent;
using QtRocket::ComponentChangeSignal;
using QtRocket::ComponentPreset;
using QtRocket::ComponentPresetDatabase;
using QtRocket::ComponentPresetReference;
using QtRocket::FinSet;
using QtRocket::FreeformFinSet;
using QtRocket::ModId;
using QtRocket::NoseCone;
using QtRocket::Rocket;
using QtRocket::RocketComponent;

/// What the body tube of the example "Pods--powered with recovery deployment" says about its
/// preset.
[[nodiscard]] ComponentPresetReference tubeReference()
{
    return {.type         = "BODY_TUBE",
            .manufacturer = "Estes",
            .partNo       = "BT-50, 30352",
            .digest       = "a59dec8e4034a2fee5955dbf4ff07f1c"};
}

/// Another reference: a part without a type and a digest.
[[nodiscard]] ComponentPresetReference otherReference()
{
    return {.type = std::nullopt, .manufacturer = "Acme", .partNo = "X-1", .digest = std::nullopt};
}

/// The preset tubeReference() names, of a database that is gone when this returns: the
/// component that loads it shares it.
[[nodiscard]] std::shared_ptr<const ComponentPreset> tubePreset()
{
    const ComponentPresetDatabase database = QtRocket::Test::makeExamplePresetDatabase();
    const std::vector<std::shared_ptr<const ComponentPreset>> found =
        database.find("Estes", "BT-50, 30352");
    return found.empty() ? nullptr : found.front();
}

/// The Estes Alpha III of TestRockets with the events of its rocket recorded: each as the
/// kinds it is of ("NONFUNCTIONAL", "MASS AERODYNAMIC").
class Alpha
{
public:
    Alpha()
      : m_rocket(takeRocket()),
        m_connection(m_rocket->addComponentChangeListener(
            [this](const ComponentChangeEvent& e) { m_events.push_back(describe(e)); }))
    {
    }

    [[nodiscard]] Rocket&     rocket() const noexcept { return *m_rocket; }
    [[nodiscard]] AxialStage& stage() const
    {
        return dynamic_cast<AxialStage&>(m_rocket->getChild(0));
    }
    [[nodiscard]] NoseCone& nose() const { return dynamic_cast<NoseCone&>(stage().getChild(0)); }
    [[nodiscard]] BodyTube& body() const { return dynamic_cast<BodyTube&>(stage().getChild(1)); }
    /// The fin set, the first child of the body tube.
    [[nodiscard]] FinSet& fins() const { return dynamic_cast<FinSet&>(body().getChild(0)); }

    /// The events since the last call, separated by "; ".
    [[nodiscard]] std::string takeEvents()
    {
        std::string text;
        for (const std::string& event : m_events)
        {
            text += (text.empty() ? "" : "; ") + event;
        }
        m_events.clear();
        return text;
    }

private:
    /// The rocket of a new Estes Alpha III.
    [[nodiscard]] static std::unique_ptr<Rocket> takeRocket()
    {
        QtRocket::Test::TestEstesAlphaIII alpha;
        return std::move(alpha.rocket);
    }

    [[nodiscard]] static std::string describe(const ComponentChangeEvent& event)
    {
        std::string text = event.isNonFunctionalChange() ? "NONFUNCTIONAL" : "";
        if (event.isMassChange())
        {
            text += text.empty() ? "MASS" : " MASS";
        }
        if (event.isAerodynamicChange())
        {
            text += text.empty() ? "AERODYNAMIC" : " AERODYNAMIC";
        }
        if (event.isUndoChange())
        {
            text += text.empty() ? "UNDO" : " UNDO";
        }
        return text;
    }

    std::unique_ptr<Rocket>                 m_rocket;
    std::vector<std::string>                m_events;
    ComponentChangeSignal::ScopedConnection m_connection;
};

TEST(UnresolvedPreset, AComponentHasNoneUntilItIsGivenOne)
{
    const Alpha alpha;
    EXPECT_EQ(alpha.body().getUnresolvedPreset(), std::nullopt);
    EXPECT_EQ(alpha.rocket().getUnresolvedPreset(), std::nullopt);
    EXPECT_EQ(BodyTube().getUnresolvedPreset(), std::nullopt);
}

TEST(UnresolvedPreset, IsKeptAsItIsGivenAndALaterOneReplacesIt)
{
    BodyTube tube;
    tube.setUnresolvedPreset(tubeReference());
    EXPECT_EQ(tube.getUnresolvedPreset(), tubeReference());
    // The component has no preset for it.
    EXPECT_EQ(tube.getPresetComponent(), nullptr);

    tube.setUnresolvedPreset(otherReference());
    EXPECT_EQ(tube.getUnresolvedPreset(), otherReference());

    tube.clearUnresolvedPreset();
    EXPECT_EQ(tube.getUnresolvedPreset(), std::nullopt);
}

// The loader sets it where OpenRocket's only warns: the rocket hears nothing and its
// modification id stays, so that the loaded rocket is the one OpenRocket's loader leaves.
TEST(UnresolvedPreset, SettingAndClearingItTellNobody)
{
    Alpha       alpha;
    const ModId before = alpha.rocket().getModId();

    alpha.body().setUnresolvedPreset(tubeReference());
    alpha.body().setUnresolvedPreset(otherReference());
    alpha.body().clearUnresolvedPreset();
    alpha.rocket().setUnresolvedPreset(tubeReference());

    EXPECT_EQ(alpha.takeEvents(), "");
    EXPECT_EQ(alpha.rocket().getModId(), before);
}

// The values of the component are not touched, nor is its name.
TEST(UnresolvedPreset, IsNoValueOfTheComponent)
{
    const Alpha  alpha;
    BodyTube&    body   = alpha.body();
    const double radius = body.getOuterRadius();
    const double length = body.getLength();
    const double mass   = body.getComponentMass();

    body.setUnresolvedPreset(tubeReference());

    EXPECT_EQ(body.getOuterRadius(), radius);
    EXPECT_EQ(body.getLength(), length);
    EXPECT_EQ(body.getComponentMass(), mass);
    EXPECT_EQ(body.getName(), "Body Tube");
    EXPECT_EQ(body.getPresetComponent(), nullptr);
}

// A component has a preset or such a reference, never both: the preset is what a save writes.
TEST(UnresolvedPreset, AComponentThatHasAPresetKeepsNone)
{
    const std::shared_ptr<const ComponentPreset> preset = tubePreset();
    ASSERT_NE(preset, nullptr);
    BodyTube tube;
    tube.loadPreset(preset);

    tube.setUnresolvedPreset(otherReference());

    EXPECT_EQ(tube.getUnresolvedPreset(), std::nullopt);
    EXPECT_EQ(tube.getPresetComponent(), preset.get());
}

TEST(UnresolvedPreset, APresetThatIsLoadedTakesItsPlace)
{
    const std::shared_ptr<const ComponentPreset> preset = tubePreset();
    ASSERT_NE(preset, nullptr);
    const Alpha alpha;
    alpha.body().setUnresolvedPreset(otherReference());

    alpha.body().loadPreset(preset);

    EXPECT_EQ(alpha.body().getUnresolvedPreset(), std::nullopt);
    EXPECT_EQ(alpha.body().getPresetComponent(), preset.get());
}

// The non-owning form of loadPreset() is the same call.
TEST(UnresolvedPreset, APresetThatIsOnlyPointedAtTakesItsPlaceToo)
{
    const std::shared_ptr<const ComponentPreset> preset = tubePreset();
    ASSERT_NE(preset, nullptr);
    BodyTube tube;
    tube.setUnresolvedPreset(otherReference());

    tube.loadPreset(preset.get());

    EXPECT_EQ(tube.getUnresolvedPreset(), std::nullopt);
    EXPECT_EQ(tube.getPresetComponent(), preset.get());
}

/// A body tube whose protected setPresetComponent() a test can call: the way a subclass stores
/// a preset without loading its values.
class TubeThatStoresAPreset : public BodyTube
{
public:
    using BodyTube::setPresetComponent;
};

// A component has a preset or a reference, never both, whichever way the preset comes: the
// setter that only stores it drops the reference as loadPreset() does. Storing no preset
// leaves the reference alone (that is no preset to have beside it).
TEST(UnresolvedPreset, APresetThatIsOnlyStoredTakesItsPlaceToo)
{
    const std::shared_ptr<const ComponentPreset> preset = tubePreset();
    ASSERT_NE(preset, nullptr);
    TubeThatStoresAPreset tube;
    tube.setUnresolvedPreset(otherReference());

    tube.setPresetComponent(nullptr);
    EXPECT_EQ(tube.getUnresolvedPreset(), otherReference());
    EXPECT_EQ(tube.getPresetComponent(), nullptr);

    tube.setPresetComponent(preset);
    EXPECT_EQ(tube.getUnresolvedPreset(), std::nullopt);
    EXPECT_EQ(tube.getPresetComponent(), preset.get());

    // And with the preset stored, no reference is taken.
    tube.setUnresolvedPreset(tubeReference());
    EXPECT_EQ(tube.getUnresolvedPreset(), std::nullopt);
}

// clearPreset() forgets it as it forgets a preset, with the event of a preset that is
// forgotten: what a save writes has changed. (OpenRocket's clearPreset() does nothing for a
// component without a preset.)
TEST(UnresolvedPreset, ClearPresetForgetsItAndFiresANonFunctionalChange)
{
    Alpha alpha;
    alpha.body().setUnresolvedPreset(tubeReference());
    const ModId before = alpha.rocket().getModId();

    alpha.body().clearPreset();

    EXPECT_EQ(alpha.body().getUnresolvedPreset(), std::nullopt);
    EXPECT_EQ(alpha.takeEvents(), "NONFUNCTIONAL");
    EXPECT_NE(alpha.rocket().getModId(), before);

    // Once more: there is nothing left to forget, and nothing happens, as in OpenRocket.
    alpha.body().clearPreset();
    EXPECT_EQ(alpha.takeEvents(), "");
}

// loadPreset() of no preset is clearPreset().
TEST(UnresolvedPreset, LoadingNoPresetForgetsIt)
{
    Alpha alpha;
    alpha.body().setUnresolvedPreset(tubeReference());

    alpha.body().loadPreset(std::shared_ptr<const ComponentPreset>());

    EXPECT_EQ(alpha.body().getUnresolvedPreset(), std::nullopt);
    EXPECT_EQ(alpha.takeEvents(), "NONFUNCTIONAL");
}

// While a file loads, the elements behind <preset> set the component's own values, and the
// handler has clearPreset() ignored for them: the reference stays, as a preset would.
TEST(UnresolvedPreset, StaysWhilePresetClearingIsIgnored)
{
    Alpha alpha;
    alpha.body().setUnresolvedPreset(tubeReference());
    alpha.body().setIgnorePresetClearing(true);

    alpha.body().clearPreset();
    alpha.body().loadPreset(std::shared_ptr<const ComponentPreset>());
    alpha.body().setOuterRadius(alpha.body().getOuterRadius() * 2);

    EXPECT_EQ(alpha.body().getUnresolvedPreset(), tubeReference());
    // The one event is the radius's.
    EXPECT_EQ(alpha.takeEvents(), "MASS AERODYNAMIC");

    alpha.body().setIgnorePresetClearing(false);
    alpha.body().setOuterRadius(alpha.body().getOuterRadius() * 2);
    EXPECT_EQ(alpha.body().getUnresolvedPreset(), std::nullopt);
}

// A change of a value that a preset gives: the component is no longer the part the file named.
// The events are those of a component whose preset was found: the change, then the forgetting.
TEST(UnresolvedPreset, ASetterThatWouldClearThePresetForgetsIt)
{
    Alpha alpha;
    alpha.body().setUnresolvedPreset(tubeReference());

    alpha.body().setOuterRadius(alpha.body().getOuterRadius() * 2);

    EXPECT_EQ(alpha.body().getUnresolvedPreset(), std::nullopt);
    EXPECT_EQ(alpha.takeEvents(), "MASS AERODYNAMIC; NONFUNCTIONAL");
}

// A setter that changes nothing does not reach clearPreset(), and one that no preset has a
// say in does not call it: the name, the comment, the position.
TEST(UnresolvedPreset, StaysThroughWhatWouldNotClearAPreset)
{
    const Alpha alpha;
    alpha.body().setUnresolvedPreset(tubeReference());

    alpha.body().setOuterRadius(alpha.body().getOuterRadius());
    alpha.body().setName("Renamed");
    alpha.body().setComment("A comment");

    EXPECT_EQ(alpha.body().getUnresolvedPreset(), tubeReference());
    // The other components are not asked.
    alpha.nose().setLength(alpha.nose().getLength() * 2);
    EXPECT_EQ(alpha.body().getUnresolvedPreset(), tubeReference());
}

TEST(UnresolvedPreset, IsCopiedWithTheComponent)
{
    const Alpha alpha;
    alpha.body().setUnresolvedPreset(tubeReference());
    alpha.fins().setUnresolvedPreset(otherReference());

    // A component by itself, with its children.
    const std::unique_ptr<RocketComponent> sameIds = alpha.body().copyWithOriginalId();
    EXPECT_EQ(sameIds->getUnresolvedPreset(), tubeReference());
    EXPECT_EQ(sameIds->getChild(0).getUnresolvedPreset(), otherReference());
    const std::unique_ptr<RocketComponent> newIds = alpha.body().copyWithNewIds();
    EXPECT_EQ(newIds->getUnresolvedPreset(), tubeReference());
    EXPECT_EQ(newIds->getChild(0).getUnresolvedPreset(), otherReference());

    // The whole rocket, as an undo state and a simulation copy it.
    const std::unique_ptr<Rocket> rocket = alpha.rocket().copyRocketWithOriginalId();
    const RocketComponent&        body   = rocket->getChild(0).getChild(1);
    EXPECT_EQ(body.getUnresolvedPreset(), tubeReference());
    EXPECT_EQ(body.getChild(0).getUnresolvedPreset(), otherReference());
    EXPECT_EQ(rocket->getChild(0).getChild(0).getUnresolvedPreset(), std::nullopt);
}

TEST(UnresolvedPreset, ACopyHasItsOwn)
{
    const Alpha alpha;
    alpha.body().setUnresolvedPreset(tubeReference());
    const std::unique_ptr<RocketComponent> copy = alpha.body().copyWithOriginalId();

    copy->clearPreset();
    EXPECT_EQ(copy->getUnresolvedPreset(), std::nullopt);
    EXPECT_EQ(alpha.body().getUnresolvedPreset(), tubeReference());

    alpha.body().setUnresolvedPreset(otherReference());
    const std::unique_ptr<RocketComponent> second = alpha.body().copyWithOriginalId();
    alpha.body().clearUnresolvedPreset();
    EXPECT_EQ(second->getUnresolvedPreset(), otherReference());
}

// Rocket::loadFrom(), what an undo and a redo are made of: the components of the rocket are
// replaced by copies of the state's, which have what the state's have.
TEST(UnresolvedPreset, ComesBackWithTheComponentsOfAStateThatIsLoaded)
{
    const Alpha alpha;
    alpha.body().setUnresolvedPreset(tubeReference());
    const std::unique_ptr<Rocket> state = alpha.rocket().copyRocketWithOriginalId();

    alpha.body().setOuterRadius(alpha.body().getOuterRadius() * 2);
    ASSERT_EQ(alpha.body().getUnresolvedPreset(), std::nullopt);
    alpha.nose().setUnresolvedPreset(otherReference());

    alpha.rocket().loadFrom(*state);

    EXPECT_EQ(alpha.body().getUnresolvedPreset(), tubeReference());
    // And what the state did not have is gone.
    EXPECT_EQ(alpha.nose().getUnresolvedPreset(), std::nullopt);
}

// The rocket itself is not replaced by loadFrom(), and copyFrom() leaves its preset alone (as
// OpenRocket's does): the same holds for this reference, so the rocket keeps the one it has.
TEST(UnresolvedPreset, TheRocketItselfKeepsItsOwnWhenAStateIsLoaded)
{
    const Alpha alpha;
    alpha.rocket().setUnresolvedPreset(tubeReference());
    const std::unique_ptr<Rocket> with = alpha.rocket().copyRocketWithOriginalId();
    EXPECT_EQ(with->getUnresolvedPreset(), tubeReference());
    alpha.rocket().clearUnresolvedPreset();
    const std::unique_ptr<Rocket> without = alpha.rocket().copyRocketWithOriginalId();

    // A state with the reference is loaded into a rocket without it, ...
    alpha.rocket().loadFrom(*with);
    EXPECT_EQ(alpha.rocket().getUnresolvedPreset(), std::nullopt);
    // ... and a state without it into a rocket that has it.
    alpha.rocket().setUnresolvedPreset(otherReference());
    alpha.rocket().loadFrom(*without);
    EXPECT_EQ(alpha.rocket().getUnresolvedPreset(), otherReference());
}

// A fin set that is converted to a freeform one is another part than the file named: the new
// fin set has no preset (as in OpenRocket) and no reference. The fin set that was converted,
// which is out of the rocket, still has its own.
TEST(UnresolvedPreset, AFinSetConvertedToAFreeformOneHasNone)
{
    const Alpha alpha;
    alpha.fins().setUnresolvedPreset(tubeReference());

    const FreeformFinSet::ConversionResult converted = FreeformFinSet::convertFinSet(alpha.fins());

    ASSERT_NE(converted.freeform, nullptr);
    EXPECT_EQ(converted.freeform->getUnresolvedPreset(), std::nullopt);
    ASSERT_NE(converted.original, nullptr);
    EXPECT_EQ(converted.original->getUnresolvedPreset(), tubeReference());
}

// Two components are the same component by their class and their id (equals()), whatever a
// file said about their presets.
TEST(UnresolvedPreset, TakesNoPartInTheIdentityOfAComponent)
{
    const Alpha                            alpha;
    const std::unique_ptr<RocketComponent> copy = alpha.body().copyWithOriginalId();

    copy->setUnresolvedPreset(tubeReference());

    EXPECT_TRUE(copy->equals(alpha.body()));
    EXPECT_TRUE(alpha.body().equals(*copy));
}

}  // namespace
