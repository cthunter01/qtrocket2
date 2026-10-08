#include "QtRocket/file/openrocket/LandingDispersionSettingsHandler.h"

#include <array>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/file/openrocket/SimulationsHandler.h"
#include "QtRocket/file/simplesax/ElementHandler.h"
#include "QtRocket/simulation/LandingDispersionSettings.h"
#include "QtRocket/simulation/Simulation.h"
#include "file/openrocket/HandlerTestSupport.h"
#include "file/openrocket/SimulationTestSupport.h"
#include "unit/DefaultUnitsGuard.h"

// The cases are <simulations> elements whose simulation has a <landingdispersion> element, run
// through SimulationsHandler as the loader runs them; their expectations are what OpenRocket
// makes of the same elements (the Java probe SimProbe of run 9b, part S3; see
// SimulationTestSupport.h), but where a case states that QtRocket differs: QtRocket keeps the
// element as it is read and validates nothing, so every case that OpenRocket refuses something
// in is such a case.

namespace
{

using QtRocket::ElementHandler;
using QtRocket::LandingDispersionSettings;
using QtRocket::LandingDispersionSettingsHandler;
using QtRocket::OpenRocketDocument;
using QtRocket::Simulation;
using QtRocket::SimulationsHandler;
using QtRocket::Test::DefaultUnitsGuard;
using QtRocket::Test::expectSimulationCase;
using QtRocket::Test::HandlerRun;
using QtRocket::Test::runHandler;
using QtRocket::Test::SimulationCase;
using QtRocket::Test::simulationCaseTestName;
using QtRocket::Test::SimulationFixture;

using Texts = std::vector<std::string>;

constexpr auto kLandingDispersionCases = std::to_array<SimulationCase>({
    // BEGIN GENERATED: dispersion
    {.name     = "s3: landing dispersion as OpenRocket writes it",
     .setup    = "",
     .xml      = R"xml(
<simulations>
  <simulation status="uptodate">
    <name>Sim</name>
    <conditions><configid>11111111-1111-1111-1111-111111111111</configid></conditions>
    <landingdispersion runs="500" seed="12345">
      <uncertainty parameter="windspeed" distribution="normal" spread="0.5"/>
      <uncertainty parameter="airdensity" distribution="uniform" spread="0.01"/>
      <uncertainty parameter="totalmass" distribution="lognormal" spread="0.02"/>
    </landingdispersion>
    <flightdata maxaltitude="1.5"/>
  </simulation>
</simulations>
)xml",
     .java     = R"out(
  closed=simulations {} []
  configs: 11111111-1111-1111-1111-111111111111
  sims: 1
  sim[0] name='Sim' stored=LOADED presync=OUTDATED status=LOADED fcid=11111111-1111-1111-1111-111111111111 simulated=equal
    dispersion: {runs=500, seed=12345} [{distribution=normal, parameter=windspeed, spread=0.5}, {distribution=uniform, parameter=airdensity, spread=0.01}, {distribution=lognormal, parameter=totalmass, spread=0.02}]
    data: branches=0 maxAlt=1.5 maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=NaN vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "s3: landing dispersion without uncertainties",
     .setup    = "",
     .xml      = R"xml(
<simulations>
  <simulation status="uptodate">
    <name>Sim</name>
    <conditions><configid>11111111-1111-1111-1111-111111111111</configid></conditions>
    <landingdispersion runs="2" seed="-7"/>
  </simulation>
</simulations>
)xml",
     .java     = R"out(
  closed=simulations {} []
  configs: 11111111-1111-1111-1111-111111111111
  sims: 1
  sim[0] name='Sim' stored=NOT_SIMULATED presync=NOT_SIMULATED status=NOT_SIMULATED fcid=11111111-1111-1111-1111-111111111111 simulated=equal
    dispersion: {runs=2, seed=-7} []
    data: null
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "s3: landing dispersion without attributes",
     .setup    = "",
     .xml      = R"xml(
<simulations>
  <simulation status="uptodate">
    <name>Sim</name>
    <conditions><configid>11111111-1111-1111-1111-111111111111</configid></conditions>
    <landingdispersion>
      <uncertainty parameter="windspeed" distribution="normal" spread="0.5"/>
    </landingdispersion>
  </simulation>
</simulations>
)xml",
     .java     = R"out(
  W[Other,NORMAL] Missing landing-dispersion run count, ignoring settings.
  W[Other,NORMAL] Missing landing-dispersion seed, ignoring settings.
  closed=simulations {} []
  configs: 11111111-1111-1111-1111-111111111111
  sims: 1
  sim[0] name='Sim' stored=NOT_SIMULATED presync=NOT_SIMULATED status=NOT_SIMULATED fcid=11111111-1111-1111-1111-111111111111 simulated=equal
    data: null
)out",
     .qtrocket = R"out(
  closed=simulations {} []
  configs: 11111111-1111-1111-1111-111111111111
  sims: 1
  sim[0] name='Sim' stored=NOT_SIMULATED presync=NOT_SIMULATED status=NOT_SIMULATED fcid=11111111-1111-1111-1111-111111111111 simulated=equal
    dispersion: {} [{distribution=normal, parameter=windspeed, spread=0.5}]
    data: null
)out",
     .why = "The landing dispersion settings are kept as the element has them and nothing is "
            "validated (decision L9, HOOK(monte-carlo)). OpenRocket warns of the missing number of "
            "runs and seed and the simulation has no settings."},
    {.name     = "s3: landing dispersion with numbers OpenRocket refuses",
     .setup    = "",
     .xml      = R"xml(
<simulations>
  <simulation status="uptodate">
    <name>One</name>
    <conditions><configid>11111111-1111-1111-1111-111111111111</configid></conditions>
    <landingdispersion runs="ten" seed="1.5"/>
  </simulation>
  <simulation status="uptodate">
    <name>Two</name>
    <conditions><configid>11111111-1111-1111-1111-111111111111</configid></conditions>
    <landingdispersion runs="1" seed="5"/>
  </simulation>
  <simulation status="uptodate">
    <name>Three</name>
    <conditions><configid>11111111-1111-1111-1111-111111111111</configid></conditions>
    <landingdispersion runs="100001" seed="5"/>
  </simulation>
</simulations>
)xml",
     .java     = R"out(
  W[Other,NORMAL] Invalid landing-dispersion run count, ignoring settings.
  W[Other,NORMAL] Invalid landing-dispersion seed, ignoring settings.
  W[Other,NORMAL] Invalid landing-dispersion settings, ignoring.
  closed=simulations {} []
  configs: 11111111-1111-1111-1111-111111111111
  sims: 3
  sim[0] name='One' stored=NOT_SIMULATED presync=NOT_SIMULATED status=NOT_SIMULATED fcid=11111111-1111-1111-1111-111111111111 simulated=equal
    data: null
  sim[1] name='Two' stored=NOT_SIMULATED presync=NOT_SIMULATED status=NOT_SIMULATED fcid=11111111-1111-1111-1111-111111111111 simulated=equal
    data: null
  sim[2] name='Three' stored=NOT_SIMULATED presync=NOT_SIMULATED status=NOT_SIMULATED fcid=11111111-1111-1111-1111-111111111111 simulated=equal
    data: null
)out",
     .qtrocket = R"out(
  closed=simulations {} []
  configs: 11111111-1111-1111-1111-111111111111
  sims: 3
  sim[0] name='One' stored=NOT_SIMULATED presync=NOT_SIMULATED status=NOT_SIMULATED fcid=11111111-1111-1111-1111-111111111111 simulated=equal
    dispersion: {runs=ten, seed=1.5} []
    data: null
  sim[1] name='Two' stored=NOT_SIMULATED presync=NOT_SIMULATED status=NOT_SIMULATED fcid=11111111-1111-1111-1111-111111111111 simulated=equal
    dispersion: {runs=1, seed=5} []
    data: null
  sim[2] name='Three' stored=NOT_SIMULATED presync=NOT_SIMULATED status=NOT_SIMULATED fcid=11111111-1111-1111-1111-111111111111 simulated=equal
    dispersion: {runs=100001, seed=5} []
    data: null
)out",
     .why = "The landing dispersion settings are kept as the element has them and nothing is "
            "validated (decision L9, HOOK(monte-carlo)). OpenRocket refuses a number of runs or a "
            "seed that is no integer and a number of runs outside 2 to 100000, each with a "
            "warning, and the simulations have no settings."},
    {.name     = "s3: landing dispersion uncertainties OpenRocket refuses",
     .setup    = "",
     .xml      = R"xml(
<simulations>
  <simulation status="uptodate">
    <name>Sim</name>
    <conditions><configid>11111111-1111-1111-1111-111111111111</configid></conditions>
    <landingdispersion runs="10" seed="5">
      <uncertainty parameter="gusts" distribution="normal" spread="0.5"/>
      <uncertainty parameter="windspeed" distribution="bell" spread="0.5"/>
      <uncertainty distribution="normal" spread="0.5"/>
      <uncertainty parameter="windspeed" distribution="normal" spread="wide"/>
      <uncertainty parameter="windspeed" distribution="normal"/>
      <uncertainty parameter="windspeed" distribution="normal" spread="-1"/>
      <uncertainty parameter="windspeed" distribution="normal" spread="NaN"/>
      <uncertainty parameter="windspeed" distribution="normal" spread="Inf"/>
      <uncertainty parameter="windspeed" distribution="lognormal" spread="0.5"/>
    </landingdispersion>
  </simulation>
</simulations>
)xml",
     .java     = R"out(
  W[Other,NORMAL] Invalid landing-dispersion uncertainty parameter or distribution, ignoring.
  W[Other,NORMAL] Invalid landing-dispersion uncertainty spread, ignoring.
  W[Other,NORMAL] Invalid landing-dispersion uncertainty, ignoring.
  closed=simulations {} []
  configs: 11111111-1111-1111-1111-111111111111
  sims: 1
  sim[0] name='Sim' stored=NOT_SIMULATED presync=NOT_SIMULATED status=NOT_SIMULATED fcid=11111111-1111-1111-1111-111111111111 simulated=equal
    dispersion: {runs=10, seed=5} []
    data: null
)out",
     .qtrocket = R"out(
  closed=simulations {} []
  configs: 11111111-1111-1111-1111-111111111111
  sims: 1
  sim[0] name='Sim' stored=NOT_SIMULATED presync=NOT_SIMULATED status=NOT_SIMULATED fcid=11111111-1111-1111-1111-111111111111 simulated=equal
    dispersion: {runs=10, seed=5} [{distribution=normal, parameter=gusts, spread=0.5}, {distribution=bell, parameter=windspeed, spread=0.5}, {distribution=normal, spread=0.5}, {distribution=normal, parameter=windspeed, spread=wide}, {distribution=normal, parameter=windspeed}, {distribution=normal, parameter=windspeed, spread=-1}, {distribution=normal, parameter=windspeed, spread=NaN}, {distribution=normal, parameter=windspeed, spread=Inf}, {distribution=lognormal, parameter=windspeed, spread=0.5}]
    data: null
)out",
     .why      = "The landing dispersion settings are kept as the element has them and nothing is "
                 "validated (decision L9, HOOK(monte-carlo)). OpenRocket drops each of these nine "
                 "uncertainties with one of its three warnings for an uncertainty."},
    {.name     = "s3: landing dispersion with a parameter twice and a spread of zero",
     .setup    = "",
     .xml      = R"xml(
<simulations>
  <simulation status="uptodate">
    <name>Sim</name>
    <conditions><configid>11111111-1111-1111-1111-111111111111</configid></conditions>
    <landingdispersion runs="10" seed="5">
      <uncertainty parameter="thrust" distribution="normal" spread="0.5"/>
      <uncertainty parameter="windspeed" distribution="normal" spread="0.5"/>
      <uncertainty parameter="windspeed" distribution="uniform" spread="0.25"/>
      <uncertainty parameter="thrust" distribution="normal" spread="0"/>
    </landingdispersion>
  </simulation>
</simulations>
)xml",
     .java     = R"out(
  closed=simulations {} []
  configs: 11111111-1111-1111-1111-111111111111
  sims: 1
  sim[0] name='Sim' stored=NOT_SIMULATED presync=NOT_SIMULATED status=NOT_SIMULATED fcid=11111111-1111-1111-1111-111111111111 simulated=equal
    dispersion: {runs=10, seed=5} [{distribution=uniform, parameter=windspeed, spread=0.25}]
    data: null
)out",
     .qtrocket = R"out(
  closed=simulations {} []
  configs: 11111111-1111-1111-1111-111111111111
  sims: 1
  sim[0] name='Sim' stored=NOT_SIMULATED presync=NOT_SIMULATED status=NOT_SIMULATED fcid=11111111-1111-1111-1111-111111111111 simulated=equal
    dispersion: {runs=10, seed=5} [{distribution=normal, parameter=thrust, spread=0.5}, {distribution=normal, parameter=windspeed, spread=0.5}, {distribution=uniform, parameter=windspeed, spread=0.25}, {distribution=normal, parameter=thrust, spread=0}]
    data: null
)out",
     .why      = "The landing dispersion settings are kept as the element has them and nothing is "
                 "validated (decision L9, HOOK(monte-carlo)). OpenRocket keeps one uncertainty per "
                 "parameter, the last one, and none for a spread of zero."},
    {.name     = "s3: landing dispersion with numbers in other spellings",
     .setup    = "",
     .xml      = R"xml(
<simulations>
  <simulation status="uptodate">
    <name>Sim</name>
    <conditions><configid>11111111-1111-1111-1111-111111111111</configid></conditions>
    <landingdispersion runs=" 10 " seed="+5" threads="3">
      <uncertainty parameter=" windspeed " distribution="normal" spread="0.50" note="x">text</uncertainty>
    </landingdispersion>
  </simulation>
</simulations>
)xml",
     .java     = R"out(
  closed=simulations {} []
  configs: 11111111-1111-1111-1111-111111111111
  sims: 1
  sim[0] name='Sim' stored=NOT_SIMULATED presync=NOT_SIMULATED status=NOT_SIMULATED fcid=11111111-1111-1111-1111-111111111111 simulated=equal
    dispersion: {runs=10, seed=5} [{distribution=normal, parameter=windspeed, spread=0.5}]
    data: null
)out",
     .qtrocket = R"out(
  closed=simulations {} []
  configs: 11111111-1111-1111-1111-111111111111
  sims: 1
  sim[0] name='Sim' stored=NOT_SIMULATED presync=NOT_SIMULATED status=NOT_SIMULATED fcid=11111111-1111-1111-1111-111111111111 simulated=equal
    dispersion: {runs= 10 , seed=+5, threads=3} [{distribution=normal, note=x, parameter= windspeed , spread=0.50}]
    data: null
)out",
     .why = "The landing dispersion settings are kept as the element has them and nothing is "
            "validated (decision L9, HOOK(monte-carlo)). The attributes are kept as text, also the "
            "ones OpenRocket does not read; OpenRocket reads the numbers and the names and has 10 "
            "runs, the seed 5 and a spread of 0.5."},
    {.name     = "s3: landing dispersion with an unknown child",
     .setup    = "",
     .xml      = R"xml(
<simulations>
  <simulation status="uptodate">
    <name>Sim</name>
    <conditions><configid>11111111-1111-1111-1111-111111111111</configid></conditions>
    <landingdispersion runs="10" seed="5">
      <uncertainty parameter="windspeed" distribution="normal" spread="0.5"/>
      <bogus a="1"/>
      <uncertainty parameter="thrust" distribution="normal" spread="0.1"/>
    </landingdispersion>
    <flightdata maxaltitude="1.5"/>
  </simulation>
</simulations>
)xml",
     .java     = R"out(
  W[Other,NORMAL] Unknown landing-dispersion element 'bogus', ignoring.
  W[Other,NORMAL] Simulation status unknown, assuming outdated.
  W[Other,NORMAL] Unknown attributes in element 'simulation', ignoring.
  closed=simulations {status=uptodate} []
  configs: 11111111-1111-1111-1111-111111111111
  sims: 1
  sim[0] name='Sim' stored=OUTDATED presync=OUTDATED status=OUTDATED fcid=11111111-1111-1111-1111-111111111111 simulated=equal
    dispersion: {runs=10, seed=5} [{distribution=normal, parameter=windspeed, spread=0.5}, {distribution=normal, parameter=thrust, spread=0.1}]
    data: branches=0 maxAlt=1.5 maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=NaN vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "s3: a landing dispersion uncertainty with a child",
     .setup    = "",
     .xml      = R"xml(
<simulations>
  <simulation status="uptodate">
    <name>Sim</name>
    <conditions><configid>11111111-1111-1111-1111-111111111111</configid></conditions>
    <landingdispersion runs="10" seed="5">
      <uncertainty parameter="windspeed" distribution="normal" spread="0.5"><child parameter="thrust" distribution="uniform" spread="0.1"/></uncertainty>
    </landingdispersion>
    <flightdata maxaltitude="1.5"/>
  </simulation>
</simulations>
)xml",
     .java     = R"out(
  W[Other,NORMAL] Unknown element child, ignoring.
  W[Other,NORMAL] Simulation status unknown, assuming outdated.
  W[Other,NORMAL] Unknown attributes in element 'simulation', ignoring.
  closed=simulations {status=uptodate} []
  configs: 11111111-1111-1111-1111-111111111111
  sims: 1
  sim[0] name='Sim' stored=OUTDATED presync=OUTDATED status=OUTDATED fcid=11111111-1111-1111-1111-111111111111 simulated=equal
    dispersion: {runs=10, seed=5} [{distribution=uniform, parameter=thrust, spread=0.1}]
    data: branches=0 maxAlt=1.5 maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=NaN vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "s3: a second landing dispersion element replaces the first",
     .setup    = "",
     .xml      = R"xml(
<simulations>
  <simulation status="uptodate">
    <name>Sim</name>
    <conditions><configid>11111111-1111-1111-1111-111111111111</configid></conditions>
    <landingdispersion runs="10" seed="5">
      <uncertainty parameter="windspeed" distribution="normal" spread="0.5"/>
    </landingdispersion>
    <landingdispersion runs="20" seed="6">
      <uncertainty parameter="thrust" distribution="normal" spread="0.1"/>
    </landingdispersion>
  </simulation>
</simulations>
)xml",
     .java     = R"out(
  closed=simulations {} []
  configs: 11111111-1111-1111-1111-111111111111
  sims: 1
  sim[0] name='Sim' stored=NOT_SIMULATED presync=NOT_SIMULATED status=NOT_SIMULATED fcid=11111111-1111-1111-1111-111111111111 simulated=equal
    dispersion: {runs=20, seed=6} [{distribution=normal, parameter=thrust, spread=0.1}]
    data: null
)out",
     .qtrocket = "",
     .why      = ""},
    // END GENERATED: dispersion
});

class LandingDispersionElements : public ::testing::TestWithParam<SimulationCase>
{
private:
    DefaultUnitsGuard m_units;
};

TEST_P(LandingDispersionElements, LoadAsInOpenRocketButWhereStated)
{
    expectSimulationCase(GetParam());
}

INSTANTIATE_TEST_SUITE_P(Probe, LandingDispersionElements,
                         ::testing::ValuesIn(kLandingDispersionCases), simulationCaseTestName);

// ---- the handler by itself ------------------------------------------------------------------

/// The element the tests below load: settings as OpenRocket writes them.
constexpr std::string_view kElement =
    "<landingdispersion runs='500' seed='12345'>"
    "<uncertainty parameter='windspeed' distribution='normal' spread='0.5'/>"
    "<uncertainty parameter='totalmass' distribution='lognormal' spread='0.02'/>"
    "</landingdispersion>";

/// What kElement holds.
[[nodiscard]] LandingDispersionSettings expectedSettings()
{
    return LandingDispersionSettings(
        {{"runs", "500"}, {"seed", "12345"}},
        {{{"parameter", "windspeed"}, {"distribution", "normal"}, {"spread", "0.5"}},
         {{"parameter", "totalmass"}, {"distribution", "lognormal"}, {"spread", "0.02"}}});
}

// The handler starts with the attributes the element opened with.
TEST(LandingDispersionSettingsHandler, StartsWithTheAttributesOfTheElement)
{
    const ElementHandler::Attributes       attributes{{"runs", "500"}, {"seed", "12345"}};
    const LandingDispersionSettingsHandler handler(attributes);
    EXPECT_TRUE(handler.getSettings() == LandingDispersionSettings(attributes));
    EXPECT_TRUE(handler.getSettings().getUncertainties().empty());

    const LandingDispersionSettingsHandler bare{ElementHandler::Attributes{}};
    EXPECT_TRUE(bare.getSettings() == LandingDispersionSettings());
}

// The uncertainties are kept in the order of the file, each with the attributes it has.
TEST(LandingDispersionSettingsHandler, KeepsTheUncertaintiesInTheOrderOfTheFile)
{
    LandingDispersionSettingsHandler handler(
        ElementHandler::Attributes{{"runs", "500"}, {"seed", "12345"}});
    const HandlerRun run = runHandler(handler, kElement);
    EXPECT_TRUE(run.result.has_value());
    EXPECT_EQ(run.texts(), Texts{});
    EXPECT_TRUE(handler.getSettings() == expectedSettings());
    EXPECT_EQ(handler.getSettings().toString(),
              "{runs=500, seed=12345} [{distribution=normal, parameter=windspeed, spread=0.5}, "
              "{distribution=lognormal, parameter=totalmass, spread=0.02}]");
}

// OpenRocket's one warning that is not about what the settings mean is kept: a child that is
// no <uncertainty> is ignored, and the element then closes with that child's attributes.
TEST(LandingDispersionSettingsHandler, WarnsOfAChildThatIsNoUncertainty)
{
    LandingDispersionSettingsHandler handler{ElementHandler::Attributes{}};
    const HandlerRun                 run =
        runHandler(handler,
                   "<landingdispersion runs='10' seed='5'>"
                   "<certainty a='1'/>"
                   "<uncertainty parameter='thrust' distribution='normal' spread='0.1'/>"
                   "</landingdispersion>");
    EXPECT_TRUE(run.result.has_value());
    EXPECT_EQ(run.texts(), Texts{"Unknown landing-dispersion element 'certainty', ignoring."});
    ASSERT_EQ(handler.getSettings().getUncertainties().size(), 1U);
    EXPECT_EQ(handler.getSettings().getUncertainties().at(0).at("parameter"), "thrust");
    EXPECT_EQ(run.attributes, (ElementHandler::Attributes{{"a", "1"}}));
}

// None of the warnings of OpenRocket's validation is given, whatever the element holds
// (HOOK(monte-carlo)).
TEST(LandingDispersionSettingsHandler, ValidatesNothing)
{
    LandingDispersionSettingsHandler handler(ElementHandler::Attributes{{"runs", "many"}});
    const HandlerRun                 run =
        runHandler(handler,
                   "<landingdispersion runs='many'>"
                   "<uncertainty parameter='gusts' distribution='bell' spread='wide'>text"
                   "</uncertainty><uncertainty/>"
                   "</landingdispersion>");
    EXPECT_TRUE(run.result.has_value());
    EXPECT_EQ(run.texts(), Texts{});
    EXPECT_EQ(handler.getSettings().toString(),
              "{runs=many} [{distribution=bell, parameter=gusts, spread=wide}, {}]");
}

// ---- the settings of a loaded simulation ------------------------------------------------------

/// A fixture whose document has one simulation, loaded from a file with kElement.
class LoadedLandingDispersion : public ::testing::Test
{
protected:
    LoadedLandingDispersion()
    {
        SimulationsHandler handler(m_fixture.context());
        const HandlerRun   run =
            runHandler(handler, std::string("<simulations><simulation status='uptodate'>"
                                            "<name>Sim</name><conditions><configid>"
                                            "11111111-1111-1111-1111-111111111111</configid>"
                                            "</conditions>") +
                                    std::string(kElement) + "</simulation></simulations>");
        EXPECT_TRUE(run.result.has_value());
        EXPECT_EQ(run.texts(), Texts{});
        EXPECT_EQ(m_fixture.document().getSimulationCount(), 1U);
        // What the loader does when the document is read.
        m_fixture.document().clearUndo();
    }

    [[nodiscard]] OpenRocketDocument& document() noexcept { return m_fixture.document(); }
    /// The simulation of the document, looked up afresh: an undo may replace the object.
    [[nodiscard]] std::shared_ptr<Simulation> simulation()
    {
        return m_fixture.document().getSimulation(0);
    }

private:
    SimulationFixture m_fixture;
};

TEST_F(LoadedLandingDispersion, TheSimulationHasTheSettingsOfTheFile)
{
    EXPECT_TRUE(simulation()->getLandingDispersionSettings() == expectedSettings());
}

// The settings are kept through every way a simulation is copied.
TEST_F(LoadedLandingDispersion, TheSettingsAreKeptThroughCopies)
{
    const std::shared_ptr<Simulation> loaded = simulation();
    EXPECT_TRUE(loaded->copy()->getLandingDispersionSettings() == expectedSettings());
    EXPECT_TRUE(loaded->clone()->getLandingDispersionSettings() == expectedSettings());
    EXPECT_TRUE(loaded->cloneForUndo()->getLandingDispersionSettings() == expectedSettings());
    EXPECT_TRUE(loaded->duplicateForIndependentSimulation()->getLandingDispersionSettings() ==
                expectedSettings());
    // A copy is equal to the simulation, and no longer when its settings differ.
    const std::unique_ptr<Simulation> copy = loaded->copy();
    EXPECT_TRUE(*copy == *loaded);
    copy->setLandingDispersionSettings(std::nullopt);
    EXPECT_FALSE(*copy == *loaded);
}

// An undo brings back the settings a change took away, into the same simulation object.
TEST_F(LoadedLandingDispersion, AnUndoBringsBackSettingsThatWereRemoved)
{
    const std::shared_ptr<Simulation> loaded = simulation();
    document().addUndoPosition("Remove landing dispersion");
    loaded->setLandingDispersionSettings(std::nullopt);
    EXPECT_FALSE(simulation()->getLandingDispersionSettings().has_value());
    EXPECT_TRUE(document().isUndoAvailable());

    document().undo();
    EXPECT_EQ(simulation(), loaded);
    EXPECT_TRUE(simulation()->getLandingDispersionSettings() == expectedSettings());

    document().redo();
    EXPECT_FALSE(simulation()->getLandingDispersionSettings().has_value());
}

// An undo brings back changed settings as they were.
TEST_F(LoadedLandingDispersion, AnUndoBringsBackSettingsThatWereChanged)
{
    document().addUndoPosition("Change landing dispersion");
    LandingDispersionSettings changed = expectedSettings();
    changed.addUncertainty({{"parameter", "thrust"}, {"distribution", "normal"}, {"spread", "1"}});
    simulation()->setLandingDispersionSettings(changed);
    EXPECT_TRUE(simulation()->getLandingDispersionSettings() == changed);

    document().undo();
    EXPECT_TRUE(simulation()->getLandingDispersionSettings() == expectedSettings());
    document().redo();
    EXPECT_TRUE(simulation()->getLandingDispersionSettings() == changed);
}

// An undo that brings back a deleted simulation makes a new object, which has the settings.
TEST_F(LoadedLandingDispersion, AnUndoOfADeletionBringsBackTheSimulationWithItsSettings)
{
    const std::shared_ptr<Simulation> loaded = simulation();
    document().addUndoPosition("Delete simulation");
    document().removeSimulation(*loaded);
    ASSERT_EQ(document().getSimulationCount(), 0U);

    document().undo();
    ASSERT_EQ(document().getSimulationCount(), 1U);
    EXPECT_NE(simulation(), loaded);
    EXPECT_EQ(simulation()->getName(), "Sim");
    EXPECT_TRUE(simulation()->getLandingDispersionSettings() == expectedSettings());
}

}  // namespace
