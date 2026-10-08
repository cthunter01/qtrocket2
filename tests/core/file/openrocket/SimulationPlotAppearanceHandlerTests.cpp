#include "QtRocket/file/openrocket/SimulationPlotAppearanceHandler.h"

#include <array>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/file/openrocket/SimulationsHandler.h"
#include "QtRocket/simulation/FlightDataType.h"
#include "QtRocket/simulation/PlotAppearance.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/util/Color.h"
#include "QtRocket/util/LineStyle.h"
#include "file/openrocket/HandlerTestSupport.h"
#include "file/openrocket/SimulationTestSupport.h"
#include "unit/DefaultUnitsGuard.h"

// The cases are <simulations> elements whose simulation has a <plotappearance> element, run
// through SimulationsHandler as the loader runs them; their expectations are what OpenRocket
// makes of the same elements (the Java probe SimProbe of run 9b, part S3; see
// SimulationTestSupport.h).

namespace
{

using QtRocket::Color;
using QtRocket::FlightDataType;
using QtRocket::FlightDataTypeId;
using QtRocket::LineStyle;
using QtRocket::PlotAppearance;
using QtRocket::Simulation;
using QtRocket::SimulationPlotAppearanceHandler;
using QtRocket::SimulationsHandler;
using QtRocket::Test::DefaultUnitsGuard;
using QtRocket::Test::expectSimulationCase;
using QtRocket::Test::HandlerRun;
using QtRocket::Test::runHandler;
using QtRocket::Test::SimulationCase;
using QtRocket::Test::simulationCaseTestName;
using QtRocket::Test::SimulationFixture;

using Texts = std::vector<std::string>;

constexpr auto kPlotAppearanceCases = std::to_array<SimulationCase>({
    // BEGIN GENERATED: plot
    {.name     = "sim: plotappearance and landingdispersion",
     .setup    = "",
     .xml      = R"xml(
<simulations>
  <simulation status="uptodate">
    <name>Sim</name>
    <simulator>RK4Simulator</simulator>
    <calculator>BarrowmanCalculator</calculator>
    <conditions>
      <configid>11111111-1111-1111-1111-111111111111</configid>
      <launchrodlength>1.5</launchrodlength>
      <launchintowind>false</launchintowind>
      <launchrodangle>5.0</launchrodangle>
      <launchroddirection>90.0</launchroddirection>
      <windaverage>2.0</windaverage>
      <windturbulence>0.1</windturbulence>
      <winddirection>1.5707963267948966</winddirection>
      <wind model="average">
        <speed>2.0</speed>
        <direction>1.5707963267948966</direction>
        <standarddeviation>0.2</standarddeviation>
      </wind>
      <wind model="multilevel" altituderef="msl">
        <windlevel altitude="0.0" speed="3.8" direction="3.0" standarddeviation="1.52"/>
      </wind>
      <windmodeltype>Average</windmodeltype>
      <launchaltitude>100.0</launchaltitude>
      <launchlatitude>32.0</launchlatitude>
      <launchlongitude>-106.0</launchlongitude>
      <geodeticmethod>spherical</geodeticmethod>
      <simulationsteppermethod>rk4</simulationsteppermethod>
      <atmosphere model="isa"/>
      <gravity model="wgs"/>
      <timestep>0.05</timestep>
      <maxtime>1200.0</maxtime>
    </conditions>
    <plotappearance>
      <series symbol="h" linestyle="dashed" red="255" green="0" blue="0" alpha="255"/>
      <series linestyle="solid"/>
      <series symbol="x"/>
      <bogus/>
    </plotappearance>
    <landingdispersion runs="10" seed="5">
      <uncertainty parameter="windspeed" distribution="normal" spread="0.1"/>
    </landingdispersion>
    <flightdata maxaltitude="10.5" maxvelocity="20.5" flighttime="3.0">
      <databranch name="Sustainer" optimumAltitude="12.5" timeToOptimumAltitude="1.5" types="time,altitude,velocity_total">
        <event time="0" type="launch" id="bbbbbbbb-0000-0000-0000-000000000001" source="aaaaaaaa-0000-0000-0000-000000000001"/>
        <datapoint>0,0,0</datapoint>
        <datapoint>1,10,20</datapoint>
        <datapoint>2,5,NaN</datapoint>
      </databranch>
    </flightdata>
  </simulation>
</simulations>
)xml",
     .java     = R"out(
  W[Other,NORMAL] Plot appearance series missing symbol, ignoring.
  W[Other,NORMAL] Unknown element 'bogus', ignoring.
  W[Other,NORMAL] Simulation status unknown, assuming outdated.
  closed=simulations {status=uptodate} []
  configs: 11111111-1111-1111-1111-111111111111
  sims: 1
  sim[0] name='Sim' stored=OUTDATED presync=OUTDATED status=OUTDATED fcid=11111111-1111-1111-1111-111111111111 simulated=equal
    rod=1.5 intoWind=false angle=0.08726646259971647 dir=1.5707963267948966
    wind=AVERAGE avg=(2.0,1.5707963267948966,0.2) multi=MSL(0.0,3.8,3.0,1.52)
    site=100.0,32.0,-106.0 geo=SPHERICAL
    stepper=RK4 dt=0.05 tmax=1200.0 maxAngle=0.05235987755982988
    atmosphere=isa:true T=287.5000511226052 p=100152.25761373011 hum=0.0
    plot 'h': color=255,0,0,255 style=DASHED
    dispersion: {runs=10, seed=5} [{distribution=normal, parameter=windspeed, spread=0.1}]
    data: branches=1 maxAlt=10.0 maxVel=20.0 maxAcc=NaN maxMach=NaN tApogee=1.0 tFlight=2.0 vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
      branch[0] 'Sustainer' rows=3 optAlt=12.5 tOptAlt=1.5 optDelay=NaN sepTime=NaN srcId=null
        types=Time{t,Time,time,s} | Altitude{h,Position and Motion,altitude,m} | Total velocity{Vt,Position and Motion,velocity_total,m/s}
        event LAUNCH t=0.0 id=bbbbbbbb-0000-0000-0000-000000000001 src=R@aaaaaaaa-0000-0000-0000-000000000001 data=[null]
        row: 0.0 0.0 0.0
        row: 1.0 10.0 20.0
        row: 2.0 5.0 NaN
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "s3: plot appearance series",
     .setup    = "",
     .xml      = R"xml(
<simulations>
  <simulation status="uptodate">
    <name>Sim</name>
    <conditions><configid>11111111-1111-1111-1111-111111111111</configid></conditions>
    <plotappearance>
      <series symbol="h" linestyle="dashed" red="255" green="0" blue="0" alpha="128"/>
      <series symbol="Vt" linestyle="solid"/>
      <series symbol="a" red="1" green="2" blue="3"/>
      <series symbol="dotted" linestyle="dotted"/>
      <series symbol="dashdot" linestyle="dashdot"/>
      <series symbol=" padded " linestyle="solid"/>
      <series symbol="&#945;" linestyle="solid"/>
      <series symbol="text" linestyle="solid">text in a series</series>
      <series symbol="extra" linestyle="solid" other="1"/>
    </plotappearance>
    <flightdata maxaltitude="1.5"/>
  </simulation>
</simulations>
)xml",
     .java     = R"out(
  closed=simulations {} []
  configs: 11111111-1111-1111-1111-111111111111
  sims: 1
  sim[0] name='Sim' stored=LOADED presync=OUTDATED status=LOADED fcid=11111111-1111-1111-1111-111111111111 simulated=equal
    plot ' padded ': color=null style=SOLID
    plot 'Vt': color=null style=SOLID
    plot 'a': color=1,2,3,255 style=null
    plot 'dashdot': color=null style=DASHDOT
    plot 'dotted': color=null style=DOTTED
    plot 'extra': color=null style=SOLID
    plot 'h': color=255,0,0,128 style=DASHED
    plot 'text': color=null style=SOLID
    plot '<U+03B1>': color=null style=SOLID
    data: branches=0 maxAlt=1.5 maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=NaN vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "s3: plot appearance series that are not stored",
     .setup    = "",
     .xml      = R"xml(
<simulations>
  <simulation status="uptodate">
    <name>Sim</name>
    <conditions><configid>11111111-1111-1111-1111-111111111111</configid></conditions>
    <plotappearance>
      <series linestyle="solid"/>
      <series symbol="" linestyle="solid"/>
      <series symbol="   " linestyle="solid"/>
      <series symbol="nothing"/>
      <series symbol="capital" linestyle="Dashed"/>
      <series symbol="constant" linestyle="DASHED"/>
      <series symbol="unknown" linestyle="wavy"/>
      <series symbol="nored" green="2" blue="3"/>
      <series symbol="nogreen" red="1" blue="3"/>
      <series symbol="noblue" red="1" green="2"/>
      <series symbol="negative" red="-1" green="2" blue="3"/>
      <series symbol="large" red="1" green="256" blue="3"/>
      <series symbol="fraction" red="1" green="2" blue="3.0"/>
      <series symbol="padded" red=" 1" green="2" blue="3"/>
      <series symbol="empty" red="" green="2" blue="3"/>
      <series symbol="badalpha" red="1" green="2" blue="3" alpha="x"/>
      <series symbol="largealpha" red="1" green="2" blue="3" alpha="256"/>
    </plotappearance>
    <flightdata maxaltitude="1.5"/>
  </simulation>
</simulations>
)xml",
     .java     = R"out(
  W[Other,NORMAL] Plot appearance series missing symbol, ignoring.
  closed=simulations {} []
  configs: 11111111-1111-1111-1111-111111111111
  sims: 1
  sim[0] name='Sim' stored=LOADED presync=OUTDATED status=LOADED fcid=11111111-1111-1111-1111-111111111111 simulated=equal
    data: branches=0 maxAlt=1.5 maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=NaN vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "s3: plot appearance line styles and colours at their limits",
     .setup    = "",
     .xml      = R"xml(
<simulations>
  <simulation status="uptodate">
    <name>Sim</name>
    <conditions><configid>11111111-1111-1111-1111-111111111111</configid></conditions>
    <plotappearance>
      <series symbol="trimmed" linestyle=" dashed "/>
      <series symbol="limits" red="0" green="255" blue="+7" alpha="0"/>
      <series symbol="zeros" red="007" green="-0" blue="000"/>
      <series symbol="styleonly" linestyle="dotted" red="x" green="2" blue="3"/>
      <series symbol="coloronly" linestyle="wavy" red="1" green="2" blue="3"/>
    </plotappearance>
    <flightdata maxaltitude="1.5"/>
  </simulation>
</simulations>
)xml",
     .java     = R"out(
  closed=simulations {} []
  configs: 11111111-1111-1111-1111-111111111111
  sims: 1
  sim[0] name='Sim' stored=LOADED presync=OUTDATED status=LOADED fcid=11111111-1111-1111-1111-111111111111 simulated=equal
    plot 'coloronly': color=1,2,3,255 style=null
    plot 'limits': color=0,255,7,0 style=null
    plot 'styleonly': color=null style=DOTTED
    plot 'trimmed': color=null style=DASHED
    plot 'zeros': color=7,0,0,255 style=null
    data: branches=0 maxAlt=1.5 maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=NaN vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "s3: a symbol twice in a plot appearance",
     .setup    = "",
     .xml      = R"xml(
<simulations>
  <simulation status="uptodate">
    <name>Sim</name>
    <conditions><configid>11111111-1111-1111-1111-111111111111</configid></conditions>
    <plotappearance>
      <series symbol="h" linestyle="dashed"/>
      <series symbol="h" linestyle="dotted" red="1" green="2" blue="3"/>
      <series symbol="v" linestyle="dashed"/>
      <series symbol="v"/>
      <series symbol="v" linestyle="wavy"/>
    </plotappearance>
    <flightdata maxaltitude="1.5"/>
  </simulation>
</simulations>
)xml",
     .java     = R"out(
  closed=simulations {} []
  configs: 11111111-1111-1111-1111-111111111111
  sims: 1
  sim[0] name='Sim' stored=LOADED presync=OUTDATED status=LOADED fcid=11111111-1111-1111-1111-111111111111 simulated=equal
    plot 'h': color=1,2,3,255 style=DOTTED
    plot 'v': color=null style=DASHED
    data: branches=0 maxAlt=1.5 maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=NaN vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "s3: a second plot appearance element replaces the first",
     .setup    = "",
     .xml      = R"xml(
<simulations>
  <simulation status="uptodate">
    <name>Sim</name>
    <conditions><configid>11111111-1111-1111-1111-111111111111</configid></conditions>
    <plotappearance a="1">
      text
      <series symbol="h" linestyle="dashed"/>
      <series symbol="v" linestyle="dashed"/>
    </plotappearance>
    <plotappearance>
      <series symbol="v" linestyle="dotted"/>
    </plotappearance>
    <flightdata maxaltitude="1.5"/>
  </simulation>
</simulations>
)xml",
     .java     = R"out(
  closed=simulations {} []
  configs: 11111111-1111-1111-1111-111111111111
  sims: 1
  sim[0] name='Sim' stored=LOADED presync=OUTDATED status=LOADED fcid=11111111-1111-1111-1111-111111111111 simulated=equal
    plot 'v': color=null style=DOTTED
    data: branches=0 maxAlt=1.5 maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=NaN vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "s3: an empty plot appearance element after one with series",
     .setup    = "",
     .xml      = R"xml(
<simulations>
  <simulation status="uptodate">
    <name>Sim</name>
    <conditions><configid>11111111-1111-1111-1111-111111111111</configid></conditions>
    <plotappearance>
      <series symbol="h" linestyle="dashed"/>
    </plotappearance>
    <plotappearance/>
    <flightdata maxaltitude="1.5"/>
  </simulation>
</simulations>
)xml",
     .java     = R"out(
  closed=simulations {} []
  configs: 11111111-1111-1111-1111-111111111111
  sims: 1
  sim[0] name='Sim' stored=LOADED presync=OUTDATED status=LOADED fcid=11111111-1111-1111-1111-111111111111 simulated=equal
    data: branches=0 maxAlt=1.5 maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=NaN vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
)out",
     .qtrocket = "",
     .why      = ""},
    {.name     = "s3: a plot appearance series with a child",
     .setup    = "",
     .xml      = R"xml(
<simulations>
  <simulation status="uptodate">
    <name>Sim</name>
    <conditions><configid>11111111-1111-1111-1111-111111111111</configid></conditions>
    <plotappearance>
      <series symbol="h" linestyle="dashed"><child symbol="c" linestyle="dotted"/></series>
      <series symbol="v" linestyle="solid"/>
    </plotappearance>
    <flightdata maxaltitude="1.5"/>
  </simulation>
</simulations>
)xml",
     .java     = R"out(
  W[Other,NORMAL] Unknown element child, ignoring.
  W[Other,NORMAL] Simulation status unknown, assuming outdated.
  closed=simulations {status=uptodate} []
  configs: 11111111-1111-1111-1111-111111111111
  sims: 1
  sim[0] name='Sim' stored=OUTDATED presync=OUTDATED status=OUTDATED fcid=11111111-1111-1111-1111-111111111111 simulated=equal
    plot 'c': color=null style=DOTTED
    plot 'v': color=null style=SOLID
    data: branches=0 maxAlt=1.5 maxVel=NaN maxAcc=NaN maxMach=NaN tApogee=NaN tFlight=NaN vGround=NaN vRod=NaN vDeploy=NaN optDelay=NaN
)out",
     .qtrocket = "",
     .why      = ""},
    // END GENERATED: plot
});

class PlotAppearanceElements : public ::testing::TestWithParam<SimulationCase>
{
private:
    DefaultUnitsGuard m_units;
};

TEST_P(PlotAppearanceElements, LoadAsInOpenRocketButWhereStated)
{
    expectSimulationCase(GetParam());
}

INSTANTIATE_TEST_SUITE_P(Probe, PlotAppearanceElements, ::testing::ValuesIn(kPlotAppearanceCases),
                         simulationCaseTestName);

// ---- the handler by itself ------------------------------------------------------------------

TEST(SimulationPlotAppearanceHandler, HasNoAppearanceUntilASeriesCloses)
{
    const SimulationPlotAppearanceHandler handler;
    EXPECT_TRUE(handler.getPlotAppearances().empty());
}

// One appearance per symbol, with the line style and the colour of the series.
TEST(SimulationPlotAppearanceHandler, ReadsTheSeriesOfTheElement)
{
    SimulationPlotAppearanceHandler handler;
    const HandlerRun                run = runHandler(
        handler,
        "<plotappearance>"
        "<series symbol='h' linestyle='dashed' red='255' green='0' blue='0' alpha='128'/>"
        "<series symbol='Vt' linestyle='dashdot'/>"
        "<series symbol='a' red='1' green='2' blue='3'/>"
        "</plotappearance>");
    EXPECT_TRUE(run.result.has_value());
    EXPECT_EQ(run.texts(), Texts{});

    const std::map<std::string, PlotAppearance> expected{
        {"Vt", PlotAppearance(std::nullopt, LineStyle::DASHDOT)},
        {"a", PlotAppearance(Color(1, 2, 3, 255), std::nullopt)},
        {"h", PlotAppearance(Color(255, 0, 0, 128), LineStyle::DASHED)},
    };
    EXPECT_TRUE(handler.getPlotAppearances() == expected);
}

// The two warnings of the handler; a series that says nothing is passed over silently.
TEST(SimulationPlotAppearanceHandler, WarnsOfASeriesWithoutSymbolAndOfAnotherChild)
{
    SimulationPlotAppearanceHandler handler;
    const HandlerRun                run = runHandler(handler,
                                                     "<plotappearance>"
                                                     "<series linestyle='solid'/>"
                                                     "<series symbol=' ' linestyle='solid'/>"
                                                     "<series symbol='nothing'/>"
                                                     "<series symbol='h' linestyle='Dashed'/>"
                                                     "<curve symbol='h' linestyle='solid'/>"
                                                     "</plotappearance>");
    EXPECT_TRUE(run.result.has_value());
    EXPECT_EQ(run.texts(), (Texts{"Plot appearance series missing symbol, ignoring.",
                                  "Unknown element 'curve', ignoring."}));
    EXPECT_TRUE(handler.getPlotAppearances().empty());
    // The ignored child left its attributes for the element's own close.
    EXPECT_EQ(run.attributes.size(), 2U);
}

// None of the appearances the handler hands out is empty: an appearance without a colour and
// a line style is not stored, as Simulation does not store one.
TEST(SimulationPlotAppearanceHandler, StoresNoEmptyAppearance)
{
    SimulationPlotAppearanceHandler handler;
    const HandlerRun                run =
        runHandler(handler,
                   "<plotappearance>"
                   "<series symbol='h' linestyle='dotted'/>"
                   "<series symbol='h' linestyle='wavy' red='x' green='0' blue='0'/>"
                   "<series symbol='v' red='256' green='0' blue='0'/>"
                   "</plotappearance>");
    EXPECT_TRUE(run.result.has_value());
    EXPECT_EQ(run.texts(), Texts{});
    ASSERT_EQ(handler.getPlotAppearances().size(), 1U);
    // The series that says nothing does not take the earlier appearance away.
    EXPECT_TRUE(handler.getPlotAppearances().at("h") ==
                PlotAppearance(std::nullopt, LineStyle::DOTTED));
}

// The symbol of a series is the symbol of a flight data type: that is how a simulation finds
// the appearance of a type.
TEST(SimulationPlotAppearanceHandler, TheSimulationFindsTheAppearanceOfAType)
{
    SimulationFixture  fixture;
    SimulationsHandler handler(fixture.context());
    const HandlerRun   run = runHandler(handler, R"(
<simulations>
  <simulation status="uptodate">
    <conditions><configid>11111111-1111-1111-1111-111111111111</configid></conditions>
    <plotappearance>
      <series symbol="h" linestyle="dashed" red="255" green="0" blue="0" alpha="255"/>
    </plotappearance>
  </simulation>
</simulations>
)");
    EXPECT_TRUE(run.result.has_value());
    EXPECT_EQ(run.texts(), Texts{});
    ASSERT_EQ(fixture.document().getSimulationCount(), 1U);
    const std::shared_ptr<Simulation> simulation = fixture.document().getSimulation(0);

    const FlightDataType& altitude = FlightDataType::builtin(FlightDataTypeId::TYPE_ALTITUDE);
    ASSERT_EQ(altitude.getSymbol(), "h");
    const std::optional<PlotAppearance> appearance = simulation->getPlotAppearance(altitude);
    EXPECT_TRUE(appearance == PlotAppearance(Color(255, 0, 0, 255), LineStyle::DASHED));
    EXPECT_FALSE(simulation->getPlotAppearance(FlightDataType::builtin(FlightDataTypeId::TYPE_TIME))
                     .has_value());
}

}  // namespace
