#include "QtRocket/file/openrocket/SimulationSaver.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <functional>
#include <initializer_list>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/aero/lookup/CsvMachAoALookup.h"
#include "QtRocket/aero/lookup/MachAoALookup.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/OpenRocketDocumentFactory.h"
#include "QtRocket/document/StorageOptions.h"
#include "QtRocket/file/GeneralRocketLoader.h"
#include "QtRocket/file/LoadedDocument.h"
#include "QtRocket/file/openrocket/DocumentConfig.h"
#include "QtRocket/file/openrocket/LoaderWarnings.h"
#include "QtRocket/file/openrocket/OpenRocketSaver.h"
#include "QtRocket/file/openrocket/OrkWriter.h"
#include "QtRocket/logging/ErrorSet.h"
#include "QtRocket/logging/Message.h"
#include "QtRocket/logging/SimulationAbort.h"
#include "QtRocket/logging/Warning.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/models/GravityModelType.h"
#include "QtRocket/models/WindModel.h"
#include "QtRocket/models/WindModelType.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/simulation/FlightData.h"
#include "QtRocket/simulation/FlightDataBranch.h"
#include "QtRocket/simulation/FlightDataType.h"
#include "QtRocket/simulation/FlightEvent.h"
#include "QtRocket/simulation/LandingDispersionSettings.h"
#include "QtRocket/simulation/PlotAppearance.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/simulation/SimulationOptions.h"
#include "QtRocket/simulation/SimulationStepperMethod.h"
#include "QtRocket/simulation/extension/SimulationExtension.h"
#include "QtRocket/simulation/extension/UnknownSimulationExtension.h"
#include "QtRocket/simulation/extension/impl/JavaCode.h"
#include "QtRocket/simulation/extension/impl/ScriptingExtension.h"
#include "QtRocket/util/BigDecimal.h"
#include "QtRocket/util/Color.h"
#include "QtRocket/util/Config.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"
#include "QtRocket/util/GeodeticComputationStrategy.h"
#include "QtRocket/util/LineStyle.h"
#include "QtRocket/util/MathUtil.h"
#include "QtRocket/util/Strings.h"
#include "QtRocket/util/Uuid.h"
#include "TestTempDir.h"
#include "file/RocketLoaderTestSupport.h"
#include "rocket/TestRockets.h"
#include "simulation/SimulationRunSupport.h"

// SimulationSaver: the <simulation> elements of the .ork text. The texts expected are what
// OpenRocket's OpenRocketSaver writes for the same simulations (the probe SimulationSaverProbe of
// tier 10a S3, a generated table below); the behaviour that is QtRocket's own is pinned by the
// tests after it.

namespace
{

using QtRocket::BigDecimal;
using QtRocket::Color;
using QtRocket::Config;
using QtRocket::ErrorSet;
using QtRocket::FlightData;
using QtRocket::FlightDataBranch;
using QtRocket::FlightDataType;
using QtRocket::FlightDataTypeId;
using QtRocket::FlightEvent;
using QtRocket::GeneralRocketLoader;
using QtRocket::LandingDispersionSettings;
using QtRocket::LineStyle;
using QtRocket::LoadedDocument;
using QtRocket::MachAoALookup;
using QtRocket::MessageSource;
using QtRocket::OpenRocketDocument;
using QtRocket::OpenRocketDocumentFactory;
using QtRocket::OpenRocketSaver;
using QtRocket::OrkWriter;
using QtRocket::PlotAppearance;
using QtRocket::Result;
using QtRocket::Rocket;
using QtRocket::Simulation;
using QtRocket::SimulationAbort;
using QtRocket::SimulationExtension;
using QtRocket::SimulationOptions;
using QtRocket::SimulationSaver;
using QtRocket::StorageOptions;
using QtRocket::Uuid;
using QtRocket::Warning;
using QtRocket::WarningSet;
using QtRocket::Test::JavaTestPreferences;
using QtRocket::Test::LoaderEnvironment;
using QtRocket::Test::TempDir;
using QtRocket::Test::TestEstesAlphaIII;
using QtRocket::Test::testFcid;
namespace Strings = QtRocket::Strings;

// BEGIN GENERATED TABLE java_simulations (scripts/make_simulation_table.py of the probes of tier
// 10a S3, from the output of SimulationSaverProbe: OpenRocket 5f164fd0e on JDK 17; do not edit by
// hand)
/// OpenRocket's <simulation> elements of the document saved without the data.
constexpr std::array<std::string_view, 9> kJavaWithoutData{{
    // <name>Conditions &amp; &lt;all&gt;</name>
    "    <simulation status=\"notsimulated\">\n"
    "      <name>Conditions &amp; &lt;all&gt;</name>\n"
    "      <simulator>RK4Simulator</simulator>\n"
    "      <calculator>BarrowmanCalculator</calculator>\n"
    "      <conditions>\n"
    "        <configid>d010716e-ce0e-469d-ae46-190f3653ebbf</configid>\n"
    "        <launchrodlength>1.5</launchrodlength>\n"
    "        <launchintowind>false</launchintowind>\n"
    "        <launchrodangle>5.729577951308232</launchrodangle>\n"
    "        <launchroddirection>68.75493541569878</launchroddirection>\n"
    "        <windaverage>3.5</windaverage>\n"
    "        <windturbulence>0.19999999999999998</windturbulence>\n"
    "        <winddirection>0.3</winddirection>\n"
    "        <wind model=\"average\">\n"
    "          <speed>3.5</speed>\n"
    "          <direction>0.3</direction>\n"
    "          <standarddeviation>0.7</standarddeviation>\n"
    "        </wind>\n"
    "        <wind model=\"multilevel\" altituderef=\"agl\">\n"
    "          <windlevel altitude=\"0.0\" speed=\"0.0\" direction=\"0.0\" "
    "standarddeviation=\"0.0\"/>\n"
    "          <windlevel altitude=\"100.0\" speed=\"5.0\" direction=\"0.5\" "
    "standarddeviation=\"1.0\"/>\n"
    "          <windlevel altitude=\"1000.0\" speed=\"12.5\" direction=\"2.0\" "
    "standarddeviation=\"0.25\"/>\n"
    "        </wind>\n"
    "        <windmodeltype>MultiLevel</windmodeltype>\n"
    "        <launchaltitude>250.0</launchaltitude>\n"
    "        <launchlatitude>45.5</launchlatitude>\n"
    "        <launchlongitude>-73.25</launchlongitude>\n"
    "        <geodeticmethod>wgs84</geodeticmethod>\n"
    "        <simulationsteppermethod>rk6</simulationsteppermethod>\n"
    "        <randomseed>-42</randomseed>\n"
    "        <atmosphere model=\"extendedisa\">\n"
    "          <basetemperature>280.0</basetemperature>\n"
    "          <basepressure>95000.0</basepressure>\n"
    "          <baserelativehumidity>0.55</baserelativehumidity>\n"
    "        </atmosphere>\n"
    "        <gravity model=\"constant\">\n"
    "          <value>9.7</value>\n"
    "        </gravity>\n"
    "        <timestep>0.02</timestep>\n"
    "        <maxtime>600.0</maxtime>\n"
    "        <recoveryspeedwarning>25.0</recoveryspeedwarning>\n"
    "        <drogueLowspeedwarning>4.0</drogueLowspeedwarning>\n"
    "        <recoverydroguemainhighspeedwarning>35.0</recoverydroguemainhighspeedwarning>\n"
    "        <recoverydroguemainlowspeedwarning>12.0</recoverydroguemainlowspeedwarning>\n"
    "      </conditions>\n"
    "    </simulation>\n",
    // <name>Lookups</name>
    "    <simulation status=\"notsimulated\">\n"
    "      <name>Lookups</name>\n"
    "      <simulator>RK4Simulator</simulator>\n"
    "      <calculator>BarrowmanCalculator</calculator>\n"
    "      <conditions>\n"
    "        <configid>d010716e-ce0e-469d-ae46-190f3653ebbf</configid>\n"
    "        <launchrodlength>0.0</launchrodlength>\n"
    "        <launchintowind>false</launchintowind>\n"
    "        <launchrodangle>0.0</launchrodangle>\n"
    "        <launchroddirection>0.0</launchroddirection>\n"
    "        <windaverage>0.0</windaverage>\n"
    "        <windturbulence>0.0</windturbulence>\n"
    "        <winddirection>0.0</winddirection>\n"
    "        <wind model=\"average\">\n"
    "          <speed>0.0</speed>\n"
    "          <direction>0.0</direction>\n"
    "          <standarddeviation>0.0</standarddeviation>\n"
    "        </wind>\n"
    "        <wind model=\"multilevel\" altituderef=\"msl\">\n"
    "          <windlevel altitude=\"0.0\" speed=\"0.0\" direction=\"0.0\" "
    "standarddeviation=\"0.0\"/>\n"
    "        </wind>\n"
    "        <windmodeltype>Average</windmodeltype>\n"
    "        <launchaltitude>0.0</launchaltitude>\n"
    "        <launchlatitude>0.0</launchlatitude>\n"
    "        <launchlongitude>0.0</launchlongitude>\n"
    "        <geodeticmethod>spherical</geodeticmethod>\n"
    "        <simulationsteppermethod>rk4</simulationsteppermethod>\n"
    "        <atmosphere model=\"extendedisa\">\n"
    "          <basetemperature>288.15</basetemperature>\n"
    "          <basepressure>101325.0</basepressure>\n"
    "          <baserelativehumidity>0.0</baserelativehumidity>\n"
    "        </atmosphere>\n"
    "        <gravity model=\"wgs\"/>\n"
    "        <timestep>0.0</timestep>\n"
    "        <maxtime>1200.0</maxtime>\n"
    "        <recoveryspeedwarning>20.0</recoveryspeedwarning>\n"
    "        <drogueLowspeedwarning>3.048</drogueLowspeedwarning>\n"
    "        <recoverydroguemainhighspeedwarning>30.48</recoverydroguemainhighspeedwarning>\n"
    "        <recoverydroguemainlowspeedwarning>15.24</recoverydroguemainlowspeedwarning>\n"
    "        <draglookup file=\"@DRAG@\">\n"
    "          <row>Mach,AoA,Cd</row>\n"
    "          <row>0.3,0,0.35</row>\n"
    "          <row> 0.5, 0 ,0.4 &amp; &lt;x&gt;</row>\n"
    "        </draglookup>\n"
    "        <stabilitylookup>\n"
    "          <row>mach,cn,cm,cp</row>\n"
    "          <row>0.3,1,2,0.3</row>\n"
    "        </stabilitylookup>\n"
    "      </conditions>\n"
    "    </simulation>\n",
    // <name>Read again</name>
    "    <simulation status=\"notsimulated\">\n"
    "      <name>Read again</name>\n"
    "      <simulator>RK4Simulator</simulator>\n"
    "      <calculator>BarrowmanCalculator</calculator>\n"
    "      <conditions>\n"
    "        <configid>d010716e-ce0e-469d-ae46-190f3653ebbf</configid>\n"
    "        <launchrodlength>0.0</launchrodlength>\n"
    "        <launchintowind>false</launchintowind>\n"
    "        <launchrodangle>0.0</launchrodangle>\n"
    "        <launchroddirection>0.0</launchroddirection>\n"
    "        <windaverage>0.0</windaverage>\n"
    "        <windturbulence>0.0</windturbulence>\n"
    "        <winddirection>0.0</winddirection>\n"
    "        <wind model=\"average\">\n"
    "          <speed>0.0</speed>\n"
    "          <direction>0.0</direction>\n"
    "          <standarddeviation>0.0</standarddeviation>\n"
    "        </wind>\n"
    "        <wind model=\"multilevel\" altituderef=\"msl\">\n"
    "          <windlevel altitude=\"0.0\" speed=\"0.0\" direction=\"0.0\" "
    "standarddeviation=\"0.0\"/>\n"
    "        </wind>\n"
    "        <windmodeltype>Average</windmodeltype>\n"
    "        <launchaltitude>0.0</launchaltitude>\n"
    "        <launchlatitude>0.0</launchlatitude>\n"
    "        <launchlongitude>0.0</launchlongitude>\n"
    "        <geodeticmethod>spherical</geodeticmethod>\n"
    "        <simulationsteppermethod>rk4</simulationsteppermethod>\n"
    "        <atmosphere model=\"extendedisa\">\n"
    "          <basetemperature>288.15</basetemperature>\n"
    "          <basepressure>101325.0</basepressure>\n"
    "          <baserelativehumidity>0.0</baserelativehumidity>\n"
    "        </atmosphere>\n"
    "        <gravity model=\"wgs\"/>\n"
    "        <timestep>0.0</timestep>\n"
    "        <maxtime>1200.0</maxtime>\n"
    "        <recoveryspeedwarning>20.0</recoveryspeedwarning>\n"
    "        <drogueLowspeedwarning>3.048</drogueLowspeedwarning>\n"
    "        <recoverydroguemainhighspeedwarning>30.48</recoverydroguemainhighspeedwarning>\n"
    "        <recoverydroguemainlowspeedwarning>15.24</recoverydroguemainlowspeedwarning>\n"
    "        <draglookup file=\"@FILE@\">\n"
    "          <row>Mach,Cd</row>\n"
    "          <row>0.3,0.35</row>\n"
    "          <row> 0.5,0.40 </row>\n"
    "          <row>0.7,0.45</row>\n"
    "          <row># note</row>\n"
    "        </draglookup>\n"
    "      </conditions>\n"
    "    </simulation>\n",
    // <name>Dispersion</name>
    "    <simulation status=\"notsimulated\">\n"
    "      <name>Dispersion</name>\n"
    "      <simulator>RK4Simulator</simulator>\n"
    "      <calculator>BarrowmanCalculator</calculator>\n"
    "      <conditions>\n"
    "        <configid>d010716e-ce0e-469d-ae46-190f3653ebbf</configid>\n"
    "        <launchrodlength>0.0</launchrodlength>\n"
    "        <launchintowind>false</launchintowind>\n"
    "        <launchrodangle>0.0</launchrodangle>\n"
    "        <launchroddirection>0.0</launchroddirection>\n"
    "        <windaverage>0.0</windaverage>\n"
    "        <windturbulence>0.0</windturbulence>\n"
    "        <winddirection>0.0</winddirection>\n"
    "        <wind model=\"average\">\n"
    "          <speed>0.0</speed>\n"
    "          <direction>0.0</direction>\n"
    "          <standarddeviation>0.0</standarddeviation>\n"
    "        </wind>\n"
    "        <wind model=\"multilevel\" altituderef=\"msl\">\n"
    "          <windlevel altitude=\"0.0\" speed=\"0.0\" direction=\"0.0\" "
    "standarddeviation=\"0.0\"/>\n"
    "        </wind>\n"
    "        <windmodeltype>Average</windmodeltype>\n"
    "        <launchaltitude>0.0</launchaltitude>\n"
    "        <launchlatitude>0.0</launchlatitude>\n"
    "        <launchlongitude>0.0</launchlongitude>\n"
    "        <geodeticmethod>spherical</geodeticmethod>\n"
    "        <simulationsteppermethod>rk4</simulationsteppermethod>\n"
    "        <atmosphere model=\"extendedisa\">\n"
    "          <basetemperature>288.15</basetemperature>\n"
    "          <basepressure>101325.0</basepressure>\n"
    "          <baserelativehumidity>0.0</baserelativehumidity>\n"
    "        </atmosphere>\n"
    "        <gravity model=\"wgs\"/>\n"
    "        <timestep>0.0</timestep>\n"
    "        <maxtime>1200.0</maxtime>\n"
    "        <recoveryspeedwarning>20.0</recoveryspeedwarning>\n"
    "        <drogueLowspeedwarning>3.048</drogueLowspeedwarning>\n"
    "        <recoverydroguemainhighspeedwarning>30.48</recoverydroguemainhighspeedwarning>\n"
    "        <recoverydroguemainlowspeedwarning>15.24</recoverydroguemainlowspeedwarning>\n"
    "      </conditions>\n"
    "      <landingdispersion runs=\"750\" seed=\"-123456789\">\n"
    "        <uncertainty parameter=\"windspeed\" distribution=\"uniform\" spread=\"1.25\"/>\n"
    "        <uncertainty parameter=\"airdensity\" distribution=\"lognormal\" spread=\"0.015\"/>\n"
    "        <uncertainty parameter=\"launchguidedirection\" distribution=\"normal\" "
    "spread=\"0.07853981633974483\"/>\n"
    "      </landingdispersion>\n"
    "    </simulation>\n",
    // <name>Plot</name>
    "    <simulation status=\"notsimulated\">\n"
    "      <name>Plot</name>\n"
    "      <simulator>RK4Simulator</simulator>\n"
    "      <calculator>BarrowmanCalculator</calculator>\n"
    "      <conditions>\n"
    "        <configid>d010716e-ce0e-469d-ae46-190f3653ebbf</configid>\n"
    "        <launchrodlength>0.0</launchrodlength>\n"
    "        <launchintowind>false</launchintowind>\n"
    "        <launchrodangle>0.0</launchrodangle>\n"
    "        <launchroddirection>0.0</launchroddirection>\n"
    "        <windaverage>0.0</windaverage>\n"
    "        <windturbulence>0.0</windturbulence>\n"
    "        <winddirection>0.0</winddirection>\n"
    "        <wind model=\"average\">\n"
    "          <speed>0.0</speed>\n"
    "          <direction>0.0</direction>\n"
    "          <standarddeviation>0.0</standarddeviation>\n"
    "        </wind>\n"
    "        <wind model=\"multilevel\" altituderef=\"msl\">\n"
    "          <windlevel altitude=\"0.0\" speed=\"0.0\" direction=\"0.0\" "
    "standarddeviation=\"0.0\"/>\n"
    "        </wind>\n"
    "        <windmodeltype>Average</windmodeltype>\n"
    "        <launchaltitude>0.0</launchaltitude>\n"
    "        <launchlatitude>0.0</launchlatitude>\n"
    "        <launchlongitude>0.0</launchlongitude>\n"
    "        <geodeticmethod>spherical</geodeticmethod>\n"
    "        <simulationsteppermethod>rk4</simulationsteppermethod>\n"
    "        <atmosphere model=\"extendedisa\">\n"
    "          <basetemperature>288.15</basetemperature>\n"
    "          <basepressure>101325.0</basepressure>\n"
    "          <baserelativehumidity>0.0</baserelativehumidity>\n"
    "        </atmosphere>\n"
    "        <gravity model=\"wgs\"/>\n"
    "        <timestep>0.0</timestep>\n"
    "        <maxtime>1200.0</maxtime>\n"
    "        <recoveryspeedwarning>20.0</recoveryspeedwarning>\n"
    "        <drogueLowspeedwarning>3.048</drogueLowspeedwarning>\n"
    "        <recoverydroguemainhighspeedwarning>30.48</recoverydroguemainhighspeedwarning>\n"
    "        <recoverydroguemainlowspeedwarning>15.24</recoverydroguemainlowspeedwarning>\n"
    "      </conditions>\n"
    "      <plotappearance>\n"
    "        <series symbol=\"Az\" linestyle=\"dotted\"/>\n"
    "        <series symbol=\"M\" linestyle=\"dashdot\"/>\n"
    "        <series symbol=\"Vz\" red=\"0\" green=\"100\" blue=\"200\" alpha=\"255\"/>\n"
    "        <series symbol=\"h\" linestyle=\"dashed\" red=\"255\" green=\"0\" blue=\"128\" "
    "alpha=\"200\"/>\n"
    "      </plotappearance>\n"
    "    </simulation>\n",
    // <name>Extensions</name>
    "    <simulation status=\"notsimulated\">\n"
    "      <name>Extensions</name>\n"
    "      <simulator>RK4Simulator</simulator>\n"
    "      <calculator>BarrowmanCalculator</calculator>\n"
    "      <conditions>\n"
    "        <configid>d010716e-ce0e-469d-ae46-190f3653ebbf</configid>\n"
    "        <launchrodlength>0.0</launchrodlength>\n"
    "        <launchintowind>false</launchintowind>\n"
    "        <launchrodangle>0.0</launchrodangle>\n"
    "        <launchroddirection>0.0</launchroddirection>\n"
    "        <windaverage>0.0</windaverage>\n"
    "        <windturbulence>0.0</windturbulence>\n"
    "        <winddirection>0.0</winddirection>\n"
    "        <wind model=\"average\">\n"
    "          <speed>0.0</speed>\n"
    "          <direction>0.0</direction>\n"
    "          <standarddeviation>0.0</standarddeviation>\n"
    "        </wind>\n"
    "        <wind model=\"multilevel\" altituderef=\"msl\">\n"
    "          <windlevel altitude=\"0.0\" speed=\"0.0\" direction=\"0.0\" "
    "standarddeviation=\"0.0\"/>\n"
    "        </wind>\n"
    "        <windmodeltype>Average</windmodeltype>\n"
    "        <launchaltitude>0.0</launchaltitude>\n"
    "        <launchlatitude>0.0</launchlatitude>\n"
    "        <launchlongitude>0.0</launchlongitude>\n"
    "        <geodeticmethod>spherical</geodeticmethod>\n"
    "        <simulationsteppermethod>rk4</simulationsteppermethod>\n"
    "        <atmosphere model=\"extendedisa\">\n"
    "          <basetemperature>288.15</basetemperature>\n"
    "          <basepressure>101325.0</basepressure>\n"
    "          <baserelativehumidity>0.0</baserelativehumidity>\n"
    "        </atmosphere>\n"
    "        <gravity model=\"wgs\"/>\n"
    "        <timestep>0.0</timestep>\n"
    "        <maxtime>1200.0</maxtime>\n"
    "        <recoveryspeedwarning>20.0</recoveryspeedwarning>\n"
    "        <drogueLowspeedwarning>3.048</drogueLowspeedwarning>\n"
    "        <recoverydroguemainhighspeedwarning>30.48</recoverydroguemainhighspeedwarning>\n"
    "        <recoverydroguemainlowspeedwarning>15.24</recoverydroguemainlowspeedwarning>\n"
    "      </conditions>\n"
    "      <extension extensionid=\"info.openrocket.probe.KitchenSink\">\n"
    "        <entry key=\"flag\" type=\"boolean\">true</entry>\n"
    "        <entry key=\"byte\" type=\"number\">7</entry>\n"
    "        <entry key=\"short\" type=\"number\">-300</entry>\n"
    "        <entry key=\"int\" type=\"number\">5</entry>\n"
    "        <entry key=\"long\" type=\"number\">1234567890123</entry>\n"
    "        <entry key=\"float\" type=\"number\">0.1</entry>\n"
    "        <entry key=\"double\" type=\"number\">5.0</entry>\n"
    "        <entry key=\"big\" type=\"number\">12345678901234567890.5</entry>\n"
    "        <entry key=\"text\" type=\"string\">a&lt;b &amp; &quot;c&quot; &#39;d&#39;</entry>\n"
    "        <entry key=\"list\" type=\"list\">\n"
    "          <entry type=\"number\">1</entry>\n"
    "          <entry type=\"number\">2.5</entry>\n"
    "          <entry type=\"list\">\n"
    "            <entry type=\"boolean\">false</entry>\n"
    "            <entry type=\"string\">x</entry>\n"
    "          </entry>\n"
    "        </entry>\n"
    "      </extension>\n"
    "      <extension "
    "extensionid=\"info.openrocket.core.simulation.extension.impl.ScriptingExtension\">\n"
    "        <entry key=\"language\" type=\"string\">JavaScript</entry>\n"
    "        <entry key=\"script\" type=\"string\">if (a &lt; b &amp;&amp; c) {\n"
    "  d = &#39;e&#39;;\n"
    "}</entry>\n"
    "        <entry key=\"enabled\" type=\"boolean\">false</entry>\n"
    "      </extension>\n"
    "      <extension extensionid=\"info.openrocket.core.simulation.extension.impl.JavaCode\">\n"
    "        <entry key=\"className\" type=\"string\">com.example.Listener&lt;T&gt;</entry>\n"
    "      </extension>\n"
    "    </simulation>\n",
    // <name>External</name>
    "    <simulation status=\"notsimulated\">\n"
    "      <name>External</name>\n"
    "      <simulator>RK4Simulator</simulator>\n"
    "      <calculator>BarrowmanCalculator</calculator>\n"
    "      <conditions>\n"
    "        <configid>d010716e-ce0e-469d-ae46-190f3653ebbf</configid>\n"
    "        <launchrodlength>0.0</launchrodlength>\n"
    "        <launchintowind>false</launchintowind>\n"
    "        <launchrodangle>0.0</launchrodangle>\n"
    "        <launchroddirection>0.0</launchroddirection>\n"
    "        <windaverage>0.0</windaverage>\n"
    "        <windturbulence>0.0</windturbulence>\n"
    "        <winddirection>1.5707963267948966</winddirection>\n"
    "        <wind model=\"average\">\n"
    "          <speed>0.0</speed>\n"
    "          <direction>1.5707963267948966</direction>\n"
    "          <standarddeviation>0.0</standarddeviation>\n"
    "        </wind>\n"
    "        <wind model=\"multilevel\" altituderef=\"msl\">\n"
    "          <windlevel altitude=\"0.0\" speed=\"0.0\" direction=\"0.0\" "
    "standarddeviation=\"0.0\"/>\n"
    "        </wind>\n"
    "        <windmodeltype>Average</windmodeltype>\n"
    "        <launchaltitude>0.0</launchaltitude>\n"
    "        <launchlatitude>0.0</launchlatitude>\n"
    "        <launchlongitude>0.0</launchlongitude>\n"
    "        <geodeticmethod>spherical</geodeticmethod>\n"
    "        <simulationsteppermethod>rk4</simulationsteppermethod>\n"
    "        <atmosphere model=\"extendedisa\">\n"
    "          <basetemperature>288.15</basetemperature>\n"
    "          <basepressure>101325.0</basepressure>\n"
    "          <baserelativehumidity>0.0</baserelativehumidity>\n"
    "        </atmosphere>\n"
    "        <gravity model=\"wgs\"/>\n"
    "        <timestep>0.0</timestep>\n"
    "        <maxtime>1200.0</maxtime>\n"
    "        <recoveryspeedwarning>20.0</recoveryspeedwarning>\n"
    "        <drogueLowspeedwarning>3.048</drogueLowspeedwarning>\n"
    "        <recoverydroguemainhighspeedwarning>30.48</recoverydroguemainhighspeedwarning>\n"
    "        <recoverydroguemainlowspeedwarning>15.24</recoverydroguemainlowspeedwarning>\n"
    "      </conditions>\n"
    "      <flightdata maxaltitude=\"9.999995e4\" maxvelocity=\"3.141593\" maxmach=\"1.234568e8\" "
    "timetoapogee=\"1.25\" flighttime=\"1.25\" deploymentvelocity=\"-2.5\">\n"
    "        <warning type=\"LargeAOA\">\n"
    "          <id>66666666-6666-4666-8666-666666666666</id>\n"
    "          <description>Large angle of attack encountered (28.6\xC2\xB0)</description>\n"
    "          <priority>LOW</priority>\n"
    "          <source>11111111-1111-4111-8111-111111111111</source>\n"
    "          <source>22222222-2222-4222-8222-222222222222</source>\n"
    "          <parameter>0.5</parameter>\n"
    "          Large angle of attack encountered (28.6\xC2\xB0):  &quot;Nose Cone&quot;, "
    "&quot;Parachute&quot;\n"
    "        </warning>\n"
    "        <warning type=\"RecoveryHighSpeedDeployment\">\n"
    "          <id>bbbbbbbb-bbbb-4bbb-8bbb-bbbbbbbbbbbb</id>\n"
    "          <description>Recovery device deployment at high speed (31.9 m/s)</description>\n"
    "          <priority>NORMAL</priority>\n"
    "          <source>22222222-2222-4222-8222-222222222222</source>\n"
    "          <parameter>31.94803510503947</parameter>\n"
    "          Recovery device deployment at high speed (31.9 m/s):  &quot;Parachute&quot;\n"
    "        </warning>\n"
    "        <warning type=\"Other\">\n"
    "          <id>cccccccc-cccc-4ccc-8ccc-cccccccccccc</id>\n"
    "          <description>Free text &amp; &lt;markup&gt;</description>\n"
    "          <priority>NORMAL</priority>\n"
    "          Free text &amp; &lt;markup&gt;\n"
    "        </warning>\n"
    "        <warning type=\"EventAfterLanding\">\n"
    "          <id>88888888-8888-4888-8888-888888888888</id>\n"
    "          <description>Flight Event occurred after landing: Apogee</description>\n"
    "          <priority>HIGH</priority>\n"
    "          Flight Event occurred after landing: Apogee\n"
    "        </warning>\n"
    "        <databranch name=\"Sustainer &amp; &lt;external&gt;\" optimumAltitude=\"12345.678\" "
    "timeToOptimumAltitude=\"0.05\" types=\"time,altitude,velocity_total,mach_number\">\n"
    "          <event time=\"0\" type=\"launch\" id=\"44444444-4444-4444-8444-444444444444\"/>\n"
    "          <event time=\"0.05\" type=\"apogee\" id=\"33333333-3333-4333-8333-333333333333\"/>\n"
    "          <event time=\"0.05\" type=\"recoverydevicedeployment\" "
    "id=\"55555555-5555-4555-8555-555555555555\" "
    "source=\"22222222-2222-4222-8222-222222222222\"/>\n"
    "          <event time=\"1\" type=\"simwarn\" id=\"77777777-7777-4777-8777-777777777777\" "
    "warnid=\"66666666-6666-4666-8666-666666666666\"/>\n"
    "          <event time=\"1.25\" type=\"simwarn\" id=\"99999999-9999-4999-8999-999999999999\" "
    "warnid=\"88888888-8888-4888-8888-888888888888\" "
    "eventid=\"33333333-3333-4333-8333-333333333333\"/>\n"
    "          <datapoint>0,0,0,0</datapoint>\n"
    "          <datapoint>0.001009,0.012346,3.141593,1e-4</datapoint>\n"
    "          <datapoint>0.05,1.234568e4,-2.5,NaN</datapoint>\n"
    "          <datapoint>1.25,9.999995e4,0.001,1.234568e8</datapoint>\n"
    "        </databranch>\n"
    "      </flightdata>\n"
    "    </simulation>\n",
    // <name>Aborted</name>
    "    <simulation status=\"notsimulated\">\n"
    "      <name>Aborted</name>\n"
    "      <simulator>RK4Simulator</simulator>\n"
    "      <calculator>BarrowmanCalculator</calculator>\n"
    "      <conditions>\n"
    "        <configid>d010716e-ce0e-469d-ae46-190f3653ebbf</configid>\n"
    "        <launchrodlength>0.0</launchrodlength>\n"
    "        <launchintowind>false</launchintowind>\n"
    "        <launchrodangle>0.0</launchrodangle>\n"
    "        <launchroddirection>0.0</launchroddirection>\n"
    "        <windaverage>0.0</windaverage>\n"
    "        <windturbulence>0.0</windturbulence>\n"
    "        <winddirection>1.5707963267948966</winddirection>\n"
    "        <wind model=\"average\">\n"
    "          <speed>0.0</speed>\n"
    "          <direction>1.5707963267948966</direction>\n"
    "          <standarddeviation>0.0</standarddeviation>\n"
    "        </wind>\n"
    "        <wind model=\"multilevel\" altituderef=\"msl\">\n"
    "          <windlevel altitude=\"0.0\" speed=\"0.0\" direction=\"0.0\" "
    "standarddeviation=\"0.0\"/>\n"
    "        </wind>\n"
    "        <windmodeltype>Average</windmodeltype>\n"
    "        <launchaltitude>0.0</launchaltitude>\n"
    "        <launchlatitude>0.0</launchlatitude>\n"
    "        <launchlongitude>0.0</launchlongitude>\n"
    "        <geodeticmethod>spherical</geodeticmethod>\n"
    "        <simulationsteppermethod>rk4</simulationsteppermethod>\n"
    "        <atmosphere model=\"extendedisa\">\n"
    "          <basetemperature>288.15</basetemperature>\n"
    "          <basepressure>101325.0</basepressure>\n"
    "          <baserelativehumidity>0.0</baserelativehumidity>\n"
    "        </atmosphere>\n"
    "        <gravity model=\"wgs\"/>\n"
    "        <timestep>0.0</timestep>\n"
    "        <maxtime>1200.0</maxtime>\n"
    "        <recoveryspeedwarning>20.0</recoveryspeedwarning>\n"
    "        <drogueLowspeedwarning>3.048</drogueLowspeedwarning>\n"
    "        <recoverydroguemainhighspeedwarning>30.48</recoverydroguemainhighspeedwarning>\n"
    "        <recoverydroguemainlowspeedwarning>15.24</recoverydroguemainlowspeedwarning>\n"
    "      </conditions>\n"
    "      <flightdata maxaltitude=\"9.999995e4\" maxvelocity=\"3.141593\" maxmach=\"1.234568e8\" "
    "timetoapogee=\"1.25\" flighttime=\"1.25\" deploymentvelocity=\"-2.5\">\n"
    "      </flightdata>\n"
    "    </simulation>\n",
    // <name>Summary</name>
    "    <simulation status=\"notsimulated\">\n"
    "      <name>Summary</name>\n"
    "      <simulator>RK4Simulator</simulator>\n"
    "      <calculator>BarrowmanCalculator</calculator>\n"
    "      <conditions>\n"
    "        <configid>d010716e-ce0e-469d-ae46-190f3653ebbf</configid>\n"
    "        <launchrodlength>0.0</launchrodlength>\n"
    "        <launchintowind>false</launchintowind>\n"
    "        <launchrodangle>0.0</launchrodangle>\n"
    "        <launchroddirection>0.0</launchroddirection>\n"
    "        <windaverage>0.0</windaverage>\n"
    "        <windturbulence>0.0</windturbulence>\n"
    "        <winddirection>1.5707963267948966</winddirection>\n"
    "        <wind model=\"average\">\n"
    "          <speed>0.0</speed>\n"
    "          <direction>1.5707963267948966</direction>\n"
    "          <standarddeviation>0.0</standarddeviation>\n"
    "        </wind>\n"
    "        <wind model=\"multilevel\" altituderef=\"msl\">\n"
    "          <windlevel altitude=\"0.0\" speed=\"0.0\" direction=\"0.0\" "
    "standarddeviation=\"0.0\"/>\n"
    "        </wind>\n"
    "        <windmodeltype>Average</windmodeltype>\n"
    "        <launchaltitude>0.0</launchaltitude>\n"
    "        <launchlatitude>0.0</launchlatitude>\n"
    "        <launchlongitude>0.0</launchlongitude>\n"
    "        <geodeticmethod>spherical</geodeticmethod>\n"
    "        <simulationsteppermethod>rk4</simulationsteppermethod>\n"
    "        <atmosphere model=\"extendedisa\">\n"
    "          <basetemperature>288.15</basetemperature>\n"
    "          <basepressure>101325.0</basepressure>\n"
    "          <baserelativehumidity>0.0</baserelativehumidity>\n"
    "        </atmosphere>\n"
    "        <gravity model=\"wgs\"/>\n"
    "        <timestep>0.0</timestep>\n"
    "        <maxtime>1200.0</maxtime>\n"
    "        <recoveryspeedwarning>20.0</recoveryspeedwarning>\n"
    "        <drogueLowspeedwarning>3.048</drogueLowspeedwarning>\n"
    "        <recoverydroguemainhighspeedwarning>30.48</recoverydroguemainhighspeedwarning>\n"
    "        <recoverydroguemainlowspeedwarning>15.24</recoverydroguemainlowspeedwarning>\n"
    "      </conditions>\n"
    "      <flightdata maxaltitude=\"1234.56789\" maxvelocity=\"99\" maxmach=\"1.23456e-4\" "
    "timetoapogee=\"10\" groundhitvelocity=\"5\" launchrodvelocity=\"15\" "
    "deploymentvelocity=\"7.25\" optimumdelay=\"0\">\n"
    "      </flightdata>\n"
    "    </simulation>\n",
}};

/// The same saved with the data.
constexpr std::array<std::string_view, 9> kJavaWithData{{
    // <name>Conditions &amp; &lt;all&gt;</name>
    "    <simulation status=\"notsimulated\">\n"
    "      <name>Conditions &amp; &lt;all&gt;</name>\n"
    "      <simulator>RK4Simulator</simulator>\n"
    "      <calculator>BarrowmanCalculator</calculator>\n"
    "      <conditions>\n"
    "        <configid>d010716e-ce0e-469d-ae46-190f3653ebbf</configid>\n"
    "        <launchrodlength>1.5</launchrodlength>\n"
    "        <launchintowind>false</launchintowind>\n"
    "        <launchrodangle>5.729577951308232</launchrodangle>\n"
    "        <launchroddirection>68.75493541569878</launchroddirection>\n"
    "        <windaverage>3.5</windaverage>\n"
    "        <windturbulence>0.19999999999999998</windturbulence>\n"
    "        <winddirection>0.3</winddirection>\n"
    "        <wind model=\"average\">\n"
    "          <speed>3.5</speed>\n"
    "          <direction>0.3</direction>\n"
    "          <standarddeviation>0.7</standarddeviation>\n"
    "        </wind>\n"
    "        <wind model=\"multilevel\" altituderef=\"agl\">\n"
    "          <windlevel altitude=\"0.0\" speed=\"0.0\" direction=\"0.0\" "
    "standarddeviation=\"0.0\"/>\n"
    "          <windlevel altitude=\"100.0\" speed=\"5.0\" direction=\"0.5\" "
    "standarddeviation=\"1.0\"/>\n"
    "          <windlevel altitude=\"1000.0\" speed=\"12.5\" direction=\"2.0\" "
    "standarddeviation=\"0.25\"/>\n"
    "        </wind>\n"
    "        <windmodeltype>MultiLevel</windmodeltype>\n"
    "        <launchaltitude>250.0</launchaltitude>\n"
    "        <launchlatitude>45.5</launchlatitude>\n"
    "        <launchlongitude>-73.25</launchlongitude>\n"
    "        <geodeticmethod>wgs84</geodeticmethod>\n"
    "        <simulationsteppermethod>rk6</simulationsteppermethod>\n"
    "        <randomseed>-42</randomseed>\n"
    "        <atmosphere model=\"extendedisa\">\n"
    "          <basetemperature>280.0</basetemperature>\n"
    "          <basepressure>95000.0</basepressure>\n"
    "          <baserelativehumidity>0.55</baserelativehumidity>\n"
    "        </atmosphere>\n"
    "        <gravity model=\"constant\">\n"
    "          <value>9.7</value>\n"
    "        </gravity>\n"
    "        <timestep>0.02</timestep>\n"
    "        <maxtime>600.0</maxtime>\n"
    "        <recoveryspeedwarning>25.0</recoveryspeedwarning>\n"
    "        <drogueLowspeedwarning>4.0</drogueLowspeedwarning>\n"
    "        <recoverydroguemainhighspeedwarning>35.0</recoverydroguemainhighspeedwarning>\n"
    "        <recoverydroguemainlowspeedwarning>12.0</recoverydroguemainlowspeedwarning>\n"
    "      </conditions>\n"
    "    </simulation>\n",
    // <name>Lookups</name>
    "    <simulation status=\"notsimulated\">\n"
    "      <name>Lookups</name>\n"
    "      <simulator>RK4Simulator</simulator>\n"
    "      <calculator>BarrowmanCalculator</calculator>\n"
    "      <conditions>\n"
    "        <configid>d010716e-ce0e-469d-ae46-190f3653ebbf</configid>\n"
    "        <launchrodlength>0.0</launchrodlength>\n"
    "        <launchintowind>false</launchintowind>\n"
    "        <launchrodangle>0.0</launchrodangle>\n"
    "        <launchroddirection>0.0</launchroddirection>\n"
    "        <windaverage>0.0</windaverage>\n"
    "        <windturbulence>0.0</windturbulence>\n"
    "        <winddirection>0.0</winddirection>\n"
    "        <wind model=\"average\">\n"
    "          <speed>0.0</speed>\n"
    "          <direction>0.0</direction>\n"
    "          <standarddeviation>0.0</standarddeviation>\n"
    "        </wind>\n"
    "        <wind model=\"multilevel\" altituderef=\"msl\">\n"
    "          <windlevel altitude=\"0.0\" speed=\"0.0\" direction=\"0.0\" "
    "standarddeviation=\"0.0\"/>\n"
    "        </wind>\n"
    "        <windmodeltype>Average</windmodeltype>\n"
    "        <launchaltitude>0.0</launchaltitude>\n"
    "        <launchlatitude>0.0</launchlatitude>\n"
    "        <launchlongitude>0.0</launchlongitude>\n"
    "        <geodeticmethod>spherical</geodeticmethod>\n"
    "        <simulationsteppermethod>rk4</simulationsteppermethod>\n"
    "        <atmosphere model=\"extendedisa\">\n"
    "          <basetemperature>288.15</basetemperature>\n"
    "          <basepressure>101325.0</basepressure>\n"
    "          <baserelativehumidity>0.0</baserelativehumidity>\n"
    "        </atmosphere>\n"
    "        <gravity model=\"wgs\"/>\n"
    "        <timestep>0.0</timestep>\n"
    "        <maxtime>1200.0</maxtime>\n"
    "        <recoveryspeedwarning>20.0</recoveryspeedwarning>\n"
    "        <drogueLowspeedwarning>3.048</drogueLowspeedwarning>\n"
    "        <recoverydroguemainhighspeedwarning>30.48</recoverydroguemainhighspeedwarning>\n"
    "        <recoverydroguemainlowspeedwarning>15.24</recoverydroguemainlowspeedwarning>\n"
    "        <draglookup file=\"@DRAG@\">\n"
    "          <row>Mach,AoA,Cd</row>\n"
    "          <row>0.3,0,0.35</row>\n"
    "          <row> 0.5, 0 ,0.4 &amp; &lt;x&gt;</row>\n"
    "        </draglookup>\n"
    "        <stabilitylookup>\n"
    "          <row>mach,cn,cm,cp</row>\n"
    "          <row>0.3,1,2,0.3</row>\n"
    "        </stabilitylookup>\n"
    "      </conditions>\n"
    "    </simulation>\n",
    // <name>Read again</name>
    "    <simulation status=\"notsimulated\">\n"
    "      <name>Read again</name>\n"
    "      <simulator>RK4Simulator</simulator>\n"
    "      <calculator>BarrowmanCalculator</calculator>\n"
    "      <conditions>\n"
    "        <configid>d010716e-ce0e-469d-ae46-190f3653ebbf</configid>\n"
    "        <launchrodlength>0.0</launchrodlength>\n"
    "        <launchintowind>false</launchintowind>\n"
    "        <launchrodangle>0.0</launchrodangle>\n"
    "        <launchroddirection>0.0</launchroddirection>\n"
    "        <windaverage>0.0</windaverage>\n"
    "        <windturbulence>0.0</windturbulence>\n"
    "        <winddirection>0.0</winddirection>\n"
    "        <wind model=\"average\">\n"
    "          <speed>0.0</speed>\n"
    "          <direction>0.0</direction>\n"
    "          <standarddeviation>0.0</standarddeviation>\n"
    "        </wind>\n"
    "        <wind model=\"multilevel\" altituderef=\"msl\">\n"
    "          <windlevel altitude=\"0.0\" speed=\"0.0\" direction=\"0.0\" "
    "standarddeviation=\"0.0\"/>\n"
    "        </wind>\n"
    "        <windmodeltype>Average</windmodeltype>\n"
    "        <launchaltitude>0.0</launchaltitude>\n"
    "        <launchlatitude>0.0</launchlatitude>\n"
    "        <launchlongitude>0.0</launchlongitude>\n"
    "        <geodeticmethod>spherical</geodeticmethod>\n"
    "        <simulationsteppermethod>rk4</simulationsteppermethod>\n"
    "        <atmosphere model=\"extendedisa\">\n"
    "          <basetemperature>288.15</basetemperature>\n"
    "          <basepressure>101325.0</basepressure>\n"
    "          <baserelativehumidity>0.0</baserelativehumidity>\n"
    "        </atmosphere>\n"
    "        <gravity model=\"wgs\"/>\n"
    "        <timestep>0.0</timestep>\n"
    "        <maxtime>1200.0</maxtime>\n"
    "        <recoveryspeedwarning>20.0</recoveryspeedwarning>\n"
    "        <drogueLowspeedwarning>3.048</drogueLowspeedwarning>\n"
    "        <recoverydroguemainhighspeedwarning>30.48</recoverydroguemainhighspeedwarning>\n"
    "        <recoverydroguemainlowspeedwarning>15.24</recoverydroguemainlowspeedwarning>\n"
    "        <draglookup file=\"@FILE@\">\n"
    "          <row>Mach,Cd</row>\n"
    "          <row>0.3,0.35</row>\n"
    "          <row> 0.5,0.40 </row>\n"
    "          <row>0.7,0.45</row>\n"
    "          <row># note</row>\n"
    "        </draglookup>\n"
    "      </conditions>\n"
    "    </simulation>\n",
    // <name>Dispersion</name>
    "    <simulation status=\"notsimulated\">\n"
    "      <name>Dispersion</name>\n"
    "      <simulator>RK4Simulator</simulator>\n"
    "      <calculator>BarrowmanCalculator</calculator>\n"
    "      <conditions>\n"
    "        <configid>d010716e-ce0e-469d-ae46-190f3653ebbf</configid>\n"
    "        <launchrodlength>0.0</launchrodlength>\n"
    "        <launchintowind>false</launchintowind>\n"
    "        <launchrodangle>0.0</launchrodangle>\n"
    "        <launchroddirection>0.0</launchroddirection>\n"
    "        <windaverage>0.0</windaverage>\n"
    "        <windturbulence>0.0</windturbulence>\n"
    "        <winddirection>0.0</winddirection>\n"
    "        <wind model=\"average\">\n"
    "          <speed>0.0</speed>\n"
    "          <direction>0.0</direction>\n"
    "          <standarddeviation>0.0</standarddeviation>\n"
    "        </wind>\n"
    "        <wind model=\"multilevel\" altituderef=\"msl\">\n"
    "          <windlevel altitude=\"0.0\" speed=\"0.0\" direction=\"0.0\" "
    "standarddeviation=\"0.0\"/>\n"
    "        </wind>\n"
    "        <windmodeltype>Average</windmodeltype>\n"
    "        <launchaltitude>0.0</launchaltitude>\n"
    "        <launchlatitude>0.0</launchlatitude>\n"
    "        <launchlongitude>0.0</launchlongitude>\n"
    "        <geodeticmethod>spherical</geodeticmethod>\n"
    "        <simulationsteppermethod>rk4</simulationsteppermethod>\n"
    "        <atmosphere model=\"extendedisa\">\n"
    "          <basetemperature>288.15</basetemperature>\n"
    "          <basepressure>101325.0</basepressure>\n"
    "          <baserelativehumidity>0.0</baserelativehumidity>\n"
    "        </atmosphere>\n"
    "        <gravity model=\"wgs\"/>\n"
    "        <timestep>0.0</timestep>\n"
    "        <maxtime>1200.0</maxtime>\n"
    "        <recoveryspeedwarning>20.0</recoveryspeedwarning>\n"
    "        <drogueLowspeedwarning>3.048</drogueLowspeedwarning>\n"
    "        <recoverydroguemainhighspeedwarning>30.48</recoverydroguemainhighspeedwarning>\n"
    "        <recoverydroguemainlowspeedwarning>15.24</recoverydroguemainlowspeedwarning>\n"
    "      </conditions>\n"
    "      <landingdispersion runs=\"750\" seed=\"-123456789\">\n"
    "        <uncertainty parameter=\"windspeed\" distribution=\"uniform\" spread=\"1.25\"/>\n"
    "        <uncertainty parameter=\"airdensity\" distribution=\"lognormal\" spread=\"0.015\"/>\n"
    "        <uncertainty parameter=\"launchguidedirection\" distribution=\"normal\" "
    "spread=\"0.07853981633974483\"/>\n"
    "      </landingdispersion>\n"
    "    </simulation>\n",
    // <name>Plot</name>
    "    <simulation status=\"notsimulated\">\n"
    "      <name>Plot</name>\n"
    "      <simulator>RK4Simulator</simulator>\n"
    "      <calculator>BarrowmanCalculator</calculator>\n"
    "      <conditions>\n"
    "        <configid>d010716e-ce0e-469d-ae46-190f3653ebbf</configid>\n"
    "        <launchrodlength>0.0</launchrodlength>\n"
    "        <launchintowind>false</launchintowind>\n"
    "        <launchrodangle>0.0</launchrodangle>\n"
    "        <launchroddirection>0.0</launchroddirection>\n"
    "        <windaverage>0.0</windaverage>\n"
    "        <windturbulence>0.0</windturbulence>\n"
    "        <winddirection>0.0</winddirection>\n"
    "        <wind model=\"average\">\n"
    "          <speed>0.0</speed>\n"
    "          <direction>0.0</direction>\n"
    "          <standarddeviation>0.0</standarddeviation>\n"
    "        </wind>\n"
    "        <wind model=\"multilevel\" altituderef=\"msl\">\n"
    "          <windlevel altitude=\"0.0\" speed=\"0.0\" direction=\"0.0\" "
    "standarddeviation=\"0.0\"/>\n"
    "        </wind>\n"
    "        <windmodeltype>Average</windmodeltype>\n"
    "        <launchaltitude>0.0</launchaltitude>\n"
    "        <launchlatitude>0.0</launchlatitude>\n"
    "        <launchlongitude>0.0</launchlongitude>\n"
    "        <geodeticmethod>spherical</geodeticmethod>\n"
    "        <simulationsteppermethod>rk4</simulationsteppermethod>\n"
    "        <atmosphere model=\"extendedisa\">\n"
    "          <basetemperature>288.15</basetemperature>\n"
    "          <basepressure>101325.0</basepressure>\n"
    "          <baserelativehumidity>0.0</baserelativehumidity>\n"
    "        </atmosphere>\n"
    "        <gravity model=\"wgs\"/>\n"
    "        <timestep>0.0</timestep>\n"
    "        <maxtime>1200.0</maxtime>\n"
    "        <recoveryspeedwarning>20.0</recoveryspeedwarning>\n"
    "        <drogueLowspeedwarning>3.048</drogueLowspeedwarning>\n"
    "        <recoverydroguemainhighspeedwarning>30.48</recoverydroguemainhighspeedwarning>\n"
    "        <recoverydroguemainlowspeedwarning>15.24</recoverydroguemainlowspeedwarning>\n"
    "      </conditions>\n"
    "      <plotappearance>\n"
    "        <series symbol=\"Az\" linestyle=\"dotted\"/>\n"
    "        <series symbol=\"M\" linestyle=\"dashdot\"/>\n"
    "        <series symbol=\"Vz\" red=\"0\" green=\"100\" blue=\"200\" alpha=\"255\"/>\n"
    "        <series symbol=\"h\" linestyle=\"dashed\" red=\"255\" green=\"0\" blue=\"128\" "
    "alpha=\"200\"/>\n"
    "      </plotappearance>\n"
    "    </simulation>\n",
    // <name>Extensions</name>
    "    <simulation status=\"notsimulated\">\n"
    "      <name>Extensions</name>\n"
    "      <simulator>RK4Simulator</simulator>\n"
    "      <calculator>BarrowmanCalculator</calculator>\n"
    "      <conditions>\n"
    "        <configid>d010716e-ce0e-469d-ae46-190f3653ebbf</configid>\n"
    "        <launchrodlength>0.0</launchrodlength>\n"
    "        <launchintowind>false</launchintowind>\n"
    "        <launchrodangle>0.0</launchrodangle>\n"
    "        <launchroddirection>0.0</launchroddirection>\n"
    "        <windaverage>0.0</windaverage>\n"
    "        <windturbulence>0.0</windturbulence>\n"
    "        <winddirection>0.0</winddirection>\n"
    "        <wind model=\"average\">\n"
    "          <speed>0.0</speed>\n"
    "          <direction>0.0</direction>\n"
    "          <standarddeviation>0.0</standarddeviation>\n"
    "        </wind>\n"
    "        <wind model=\"multilevel\" altituderef=\"msl\">\n"
    "          <windlevel altitude=\"0.0\" speed=\"0.0\" direction=\"0.0\" "
    "standarddeviation=\"0.0\"/>\n"
    "        </wind>\n"
    "        <windmodeltype>Average</windmodeltype>\n"
    "        <launchaltitude>0.0</launchaltitude>\n"
    "        <launchlatitude>0.0</launchlatitude>\n"
    "        <launchlongitude>0.0</launchlongitude>\n"
    "        <geodeticmethod>spherical</geodeticmethod>\n"
    "        <simulationsteppermethod>rk4</simulationsteppermethod>\n"
    "        <atmosphere model=\"extendedisa\">\n"
    "          <basetemperature>288.15</basetemperature>\n"
    "          <basepressure>101325.0</basepressure>\n"
    "          <baserelativehumidity>0.0</baserelativehumidity>\n"
    "        </atmosphere>\n"
    "        <gravity model=\"wgs\"/>\n"
    "        <timestep>0.0</timestep>\n"
    "        <maxtime>1200.0</maxtime>\n"
    "        <recoveryspeedwarning>20.0</recoveryspeedwarning>\n"
    "        <drogueLowspeedwarning>3.048</drogueLowspeedwarning>\n"
    "        <recoverydroguemainhighspeedwarning>30.48</recoverydroguemainhighspeedwarning>\n"
    "        <recoverydroguemainlowspeedwarning>15.24</recoverydroguemainlowspeedwarning>\n"
    "      </conditions>\n"
    "      <extension extensionid=\"info.openrocket.probe.KitchenSink\">\n"
    "        <entry key=\"flag\" type=\"boolean\">true</entry>\n"
    "        <entry key=\"byte\" type=\"number\">7</entry>\n"
    "        <entry key=\"short\" type=\"number\">-300</entry>\n"
    "        <entry key=\"int\" type=\"number\">5</entry>\n"
    "        <entry key=\"long\" type=\"number\">1234567890123</entry>\n"
    "        <entry key=\"float\" type=\"number\">0.1</entry>\n"
    "        <entry key=\"double\" type=\"number\">5.0</entry>\n"
    "        <entry key=\"big\" type=\"number\">12345678901234567890.5</entry>\n"
    "        <entry key=\"text\" type=\"string\">a&lt;b &amp; &quot;c&quot; &#39;d&#39;</entry>\n"
    "        <entry key=\"list\" type=\"list\">\n"
    "          <entry type=\"number\">1</entry>\n"
    "          <entry type=\"number\">2.5</entry>\n"
    "          <entry type=\"list\">\n"
    "            <entry type=\"boolean\">false</entry>\n"
    "            <entry type=\"string\">x</entry>\n"
    "          </entry>\n"
    "        </entry>\n"
    "      </extension>\n"
    "      <extension "
    "extensionid=\"info.openrocket.core.simulation.extension.impl.ScriptingExtension\">\n"
    "        <entry key=\"language\" type=\"string\">JavaScript</entry>\n"
    "        <entry key=\"script\" type=\"string\">if (a &lt; b &amp;&amp; c) {\n"
    "  d = &#39;e&#39;;\n"
    "}</entry>\n"
    "        <entry key=\"enabled\" type=\"boolean\">false</entry>\n"
    "      </extension>\n"
    "      <extension extensionid=\"info.openrocket.core.simulation.extension.impl.JavaCode\">\n"
    "        <entry key=\"className\" type=\"string\">com.example.Listener&lt;T&gt;</entry>\n"
    "      </extension>\n"
    "    </simulation>\n",
    // <name>External</name>
    "    <simulation status=\"external\">\n"
    "      <name>External</name>\n"
    "      <simulator>RK4Simulator</simulator>\n"
    "      <calculator>BarrowmanCalculator</calculator>\n"
    "      <conditions>\n"
    "        <configid>d010716e-ce0e-469d-ae46-190f3653ebbf</configid>\n"
    "        <launchrodlength>0.0</launchrodlength>\n"
    "        <launchintowind>false</launchintowind>\n"
    "        <launchrodangle>0.0</launchrodangle>\n"
    "        <launchroddirection>0.0</launchroddirection>\n"
    "        <windaverage>0.0</windaverage>\n"
    "        <windturbulence>0.0</windturbulence>\n"
    "        <winddirection>1.5707963267948966</winddirection>\n"
    "        <wind model=\"average\">\n"
    "          <speed>0.0</speed>\n"
    "          <direction>1.5707963267948966</direction>\n"
    "          <standarddeviation>0.0</standarddeviation>\n"
    "        </wind>\n"
    "        <wind model=\"multilevel\" altituderef=\"msl\">\n"
    "          <windlevel altitude=\"0.0\" speed=\"0.0\" direction=\"0.0\" "
    "standarddeviation=\"0.0\"/>\n"
    "        </wind>\n"
    "        <windmodeltype>Average</windmodeltype>\n"
    "        <launchaltitude>0.0</launchaltitude>\n"
    "        <launchlatitude>0.0</launchlatitude>\n"
    "        <launchlongitude>0.0</launchlongitude>\n"
    "        <geodeticmethod>spherical</geodeticmethod>\n"
    "        <simulationsteppermethod>rk4</simulationsteppermethod>\n"
    "        <atmosphere model=\"extendedisa\">\n"
    "          <basetemperature>288.15</basetemperature>\n"
    "          <basepressure>101325.0</basepressure>\n"
    "          <baserelativehumidity>0.0</baserelativehumidity>\n"
    "        </atmosphere>\n"
    "        <gravity model=\"wgs\"/>\n"
    "        <timestep>0.0</timestep>\n"
    "        <maxtime>1200.0</maxtime>\n"
    "        <recoveryspeedwarning>20.0</recoveryspeedwarning>\n"
    "        <drogueLowspeedwarning>3.048</drogueLowspeedwarning>\n"
    "        <recoverydroguemainhighspeedwarning>30.48</recoverydroguemainhighspeedwarning>\n"
    "        <recoverydroguemainlowspeedwarning>15.24</recoverydroguemainlowspeedwarning>\n"
    "      </conditions>\n"
    "      <flightdata maxaltitude=\"9.999995e4\" maxvelocity=\"3.141593\" maxmach=\"1.234568e8\" "
    "timetoapogee=\"1.25\" flighttime=\"1.25\" deploymentvelocity=\"-2.5\">\n"
    "        <warning type=\"LargeAOA\">\n"
    "          <id>66666666-6666-4666-8666-666666666666</id>\n"
    "          <description>Large angle of attack encountered (28.6\xC2\xB0)</description>\n"
    "          <priority>LOW</priority>\n"
    "          <source>11111111-1111-4111-8111-111111111111</source>\n"
    "          <source>22222222-2222-4222-8222-222222222222</source>\n"
    "          <parameter>0.5</parameter>\n"
    "          Large angle of attack encountered (28.6\xC2\xB0):  &quot;Nose Cone&quot;, "
    "&quot;Parachute&quot;\n"
    "        </warning>\n"
    "        <warning type=\"RecoveryHighSpeedDeployment\">\n"
    "          <id>bbbbbbbb-bbbb-4bbb-8bbb-bbbbbbbbbbbb</id>\n"
    "          <description>Recovery device deployment at high speed (31.9 m/s)</description>\n"
    "          <priority>NORMAL</priority>\n"
    "          <source>22222222-2222-4222-8222-222222222222</source>\n"
    "          <parameter>31.94803510503947</parameter>\n"
    "          Recovery device deployment at high speed (31.9 m/s):  &quot;Parachute&quot;\n"
    "        </warning>\n"
    "        <warning type=\"Other\">\n"
    "          <id>cccccccc-cccc-4ccc-8ccc-cccccccccccc</id>\n"
    "          <description>Free text &amp; &lt;markup&gt;</description>\n"
    "          <priority>NORMAL</priority>\n"
    "          Free text &amp; &lt;markup&gt;\n"
    "        </warning>\n"
    "        <warning type=\"EventAfterLanding\">\n"
    "          <id>88888888-8888-4888-8888-888888888888</id>\n"
    "          <description>Flight Event occurred after landing: Apogee</description>\n"
    "          <priority>HIGH</priority>\n"
    "          Flight Event occurred after landing: Apogee\n"
    "        </warning>\n"
    "        <databranch name=\"Sustainer &amp; &lt;external&gt;\" optimumAltitude=\"12345.678\" "
    "timeToOptimumAltitude=\"0.05\" types=\"time,altitude,velocity_total,mach_number\">\n"
    "          <event time=\"0\" type=\"launch\" id=\"44444444-4444-4444-8444-444444444444\"/>\n"
    "          <event time=\"0.05\" type=\"apogee\" id=\"33333333-3333-4333-8333-333333333333\"/>\n"
    "          <event time=\"0.05\" type=\"recoverydevicedeployment\" "
    "id=\"55555555-5555-4555-8555-555555555555\" "
    "source=\"22222222-2222-4222-8222-222222222222\"/>\n"
    "          <event time=\"1\" type=\"simwarn\" id=\"77777777-7777-4777-8777-777777777777\" "
    "warnid=\"66666666-6666-4666-8666-666666666666\"/>\n"
    "          <event time=\"1.25\" type=\"simwarn\" id=\"99999999-9999-4999-8999-999999999999\" "
    "warnid=\"88888888-8888-4888-8888-888888888888\" "
    "eventid=\"33333333-3333-4333-8333-333333333333\"/>\n"
    "          <datapoint>0,0,0,0</datapoint>\n"
    "          <datapoint>0.001009,0.012346,3.141593,1e-4</datapoint>\n"
    "          <datapoint>0.05,1.234568e4,-2.5,NaN</datapoint>\n"
    "          <datapoint>1.25,9.999995e4,0.001,1.234568e8</datapoint>\n"
    "        </databranch>\n"
    "      </flightdata>\n"
    "    </simulation>\n",
    // <name>Aborted</name>
    "    <simulation status=\"aborted\">\n"
    "      <name>Aborted</name>\n"
    "      <simulator>RK4Simulator</simulator>\n"
    "      <calculator>BarrowmanCalculator</calculator>\n"
    "      <conditions>\n"
    "        <configid>d010716e-ce0e-469d-ae46-190f3653ebbf</configid>\n"
    "        <launchrodlength>0.0</launchrodlength>\n"
    "        <launchintowind>false</launchintowind>\n"
    "        <launchrodangle>0.0</launchrodangle>\n"
    "        <launchroddirection>0.0</launchroddirection>\n"
    "        <windaverage>0.0</windaverage>\n"
    "        <windturbulence>0.0</windturbulence>\n"
    "        <winddirection>1.5707963267948966</winddirection>\n"
    "        <wind model=\"average\">\n"
    "          <speed>0.0</speed>\n"
    "          <direction>1.5707963267948966</direction>\n"
    "          <standarddeviation>0.0</standarddeviation>\n"
    "        </wind>\n"
    "        <wind model=\"multilevel\" altituderef=\"msl\">\n"
    "          <windlevel altitude=\"0.0\" speed=\"0.0\" direction=\"0.0\" "
    "standarddeviation=\"0.0\"/>\n"
    "        </wind>\n"
    "        <windmodeltype>Average</windmodeltype>\n"
    "        <launchaltitude>0.0</launchaltitude>\n"
    "        <launchlatitude>0.0</launchlatitude>\n"
    "        <launchlongitude>0.0</launchlongitude>\n"
    "        <geodeticmethod>spherical</geodeticmethod>\n"
    "        <simulationsteppermethod>rk4</simulationsteppermethod>\n"
    "        <atmosphere model=\"extendedisa\">\n"
    "          <basetemperature>288.15</basetemperature>\n"
    "          <basepressure>101325.0</basepressure>\n"
    "          <baserelativehumidity>0.0</baserelativehumidity>\n"
    "        </atmosphere>\n"
    "        <gravity model=\"wgs\"/>\n"
    "        <timestep>0.0</timestep>\n"
    "        <maxtime>1200.0</maxtime>\n"
    "        <recoveryspeedwarning>20.0</recoveryspeedwarning>\n"
    "        <drogueLowspeedwarning>3.048</drogueLowspeedwarning>\n"
    "        <recoverydroguemainhighspeedwarning>30.48</recoverydroguemainhighspeedwarning>\n"
    "        <recoverydroguemainlowspeedwarning>15.24</recoverydroguemainlowspeedwarning>\n"
    "      </conditions>\n"
    "      <flightdata maxaltitude=\"9.999995e4\" maxvelocity=\"3.141593\" maxmach=\"1.234568e8\" "
    "timetoapogee=\"1.25\" flighttime=\"1.25\" deploymentvelocity=\"-2.5\">\n"
    "        <databranch name=\"Sustainer\" optimumAltitude=\"12345.678\" "
    "timeToOptimumAltitude=\"0.05\" types=\"time,altitude,velocity_total,mach_number\">\n"
    "          <event time=\"0\" type=\"launch\" id=\"44444444-4444-4444-8444-444444444444\"/>\n"
    "          <event time=\"0.05\" type=\"apogee\" id=\"33333333-3333-4333-8333-333333333333\"/>\n"
    "          <event time=\"0.05\" type=\"recoverydevicedeployment\" "
    "id=\"55555555-5555-4555-8555-555555555555\" "
    "source=\"22222222-2222-4222-8222-222222222222\"/>\n"
    "          <event time=\"1\" type=\"simwarn\" id=\"77777777-7777-4777-8777-777777777777\" "
    "warnid=\"66666666-6666-4666-8666-666666666666\"/>\n"
    "          <event time=\"1.25\" type=\"simwarn\" id=\"99999999-9999-4999-8999-999999999999\" "
    "warnid=\"88888888-8888-4888-8888-888888888888\" "
    "eventid=\"33333333-3333-4333-8333-333333333333\"/>\n"
    "          <event time=\"1.25\" type=\"simabort\" id=\"aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa\" "
    "cause=\"nomotorsfired\"/>\n"
    "          <datapoint>0,0,0,0</datapoint>\n"
    "          <datapoint>0.001009,0.012346,3.141593,1e-4</datapoint>\n"
    "          <datapoint>0.05,1.234568e4,-2.5,NaN</datapoint>\n"
    "          <datapoint>1.25,9.999995e4,0.001,1.234568e8</datapoint>\n"
    "        </databranch>\n"
    "      </flightdata>\n"
    "    </simulation>\n",
    // <name>Summary</name>
    "    <simulation status=\"loaded\">\n"
    "      <name>Summary</name>\n"
    "      <simulator>RK4Simulator</simulator>\n"
    "      <calculator>BarrowmanCalculator</calculator>\n"
    "      <conditions>\n"
    "        <configid>d010716e-ce0e-469d-ae46-190f3653ebbf</configid>\n"
    "        <launchrodlength>0.0</launchrodlength>\n"
    "        <launchintowind>false</launchintowind>\n"
    "        <launchrodangle>0.0</launchrodangle>\n"
    "        <launchroddirection>0.0</launchroddirection>\n"
    "        <windaverage>0.0</windaverage>\n"
    "        <windturbulence>0.0</windturbulence>\n"
    "        <winddirection>1.5707963267948966</winddirection>\n"
    "        <wind model=\"average\">\n"
    "          <speed>0.0</speed>\n"
    "          <direction>1.5707963267948966</direction>\n"
    "          <standarddeviation>0.0</standarddeviation>\n"
    "        </wind>\n"
    "        <wind model=\"multilevel\" altituderef=\"msl\">\n"
    "          <windlevel altitude=\"0.0\" speed=\"0.0\" direction=\"0.0\" "
    "standarddeviation=\"0.0\"/>\n"
    "        </wind>\n"
    "        <windmodeltype>Average</windmodeltype>\n"
    "        <launchaltitude>0.0</launchaltitude>\n"
    "        <launchlatitude>0.0</launchlatitude>\n"
    "        <launchlongitude>0.0</launchlongitude>\n"
    "        <geodeticmethod>spherical</geodeticmethod>\n"
    "        <simulationsteppermethod>rk4</simulationsteppermethod>\n"
    "        <atmosphere model=\"extendedisa\">\n"
    "          <basetemperature>288.15</basetemperature>\n"
    "          <basepressure>101325.0</basepressure>\n"
    "          <baserelativehumidity>0.0</baserelativehumidity>\n"
    "        </atmosphere>\n"
    "        <gravity model=\"wgs\"/>\n"
    "        <timestep>0.0</timestep>\n"
    "        <maxtime>1200.0</maxtime>\n"
    "        <recoveryspeedwarning>20.0</recoveryspeedwarning>\n"
    "        <drogueLowspeedwarning>3.048</drogueLowspeedwarning>\n"
    "        <recoverydroguemainhighspeedwarning>30.48</recoverydroguemainhighspeedwarning>\n"
    "        <recoverydroguemainlowspeedwarning>15.24</recoverydroguemainlowspeedwarning>\n"
    "      </conditions>\n"
    "      <flightdata maxaltitude=\"1234.56789\" maxvelocity=\"99\" maxmach=\"1.23456e-4\" "
    "timetoapogee=\"10\" groundhitvelocity=\"5\" launchrodvelocity=\"15\" "
    "deploymentvelocity=\"7.25\" optimumdelay=\"0\">\n"
    "      </flightdata>\n"
    "    </simulation>\n",
}};
// END GENERATED TABLE java_simulations

/// The id @p text names (a valid UUID of the tests).
[[nodiscard]] Uuid uuid(std::string_view text)
{
    const Result<Uuid> id = Uuid::javaFromString(text);
    EXPECT_TRUE(id.has_value()) << text;
    return id.value_or(Uuid{});
}

constexpr std::string_view kNoseId  = "11111111-1111-4111-8111-111111111111";
constexpr std::string_view kChuteId = "22222222-2222-4222-8222-222222222222";

/// The flight data type @p id.
[[nodiscard]] const FlightDataType& type(FlightDataTypeId id)
{
    return FlightDataType::builtin(id);
}

/// The document of SimulationSaverProbe: the Estes Alpha III with the nine simulations the probe
/// makes, in its order and as it makes them, with the preferences of OpenRocket's tests.
struct ProbeDocument
{
    /// Declared first, destroyed last: the simulations keep the store.
    JavaTestPreferences                 preferences;
    TempDir                             directory;
    std::filesystem::path               csvFile;
    std::unique_ptr<OpenRocketDocument> document;
    Rocket*                             rocket{nullptr};

    ProbeDocument();

    /// A new simulation of the rocket named @p name for TEST_FCID_0, added to the document.
    Simulation& addSimulation(std::string_view name);

    void addConditions();
    void addLookups();
    void addReadAgain();
    void addDispersion();
    void addPlot();
    void addExtensions();
    void addExternal();
    void addAborted();
    void addSummary();

    /// SimulationSaverProbe.branch(): a branch of four rows and its events.
    [[nodiscard]] std::shared_ptr<FlightDataBranch> branch(std::string name, bool abort) const;
};

ProbeDocument::ProbeDocument()
{
    TestEstesAlphaIII alpha;
    alpha.nose->setId(uuid(kNoseId));
    alpha.chute->setId(uuid(kChuteId));
    rocket   = alpha.rocket.get();
    document = OpenRocketDocumentFactory::createDocumentFromRocket(std::move(alpha.rocket));
    addConditions();
    addLookups();
    addReadAgain();
    addDispersion();
    addPlot();
    addExtensions();
    rocket->setSelectedConfiguration(testFcid(0));
    addExternal();
    addAborted();
    addSummary();
}

Simulation& ProbeDocument::addSimulation(std::string_view name)
{
    const auto simulation = std::make_shared<Simulation>(*rocket, preferences.store);
    simulation->setName(name);
    simulation->setFlightConfigurationId(testFcid(0));
    document->addSimulation(simulation);
    return *simulation;
}

void ProbeDocument::addConditions()
{
    SimulationOptions& o = addSimulation("Conditions & <all>").getOptions();
    o.setLaunchRodLength(1.5);
    o.setLaunchIntoWind(false);
    o.setLaunchRodAngle(0.1);
    o.setLaunchRodDirection(1.2);
    o.getAverageWindModel().setAverage(3.5);
    o.getAverageWindModel().setStandardDeviation(0.7);
    o.getAverageWindModel().setDirection(0.3);
    EXPECT_TRUE(o.getMultiLevelWindModel().addWindLevel(100.0, 5.0, 0.5, 1.0).has_value());
    EXPECT_TRUE(o.getMultiLevelWindModel().addWindLevel(1000.0, 12.5, 2.0, 0.25).has_value());
    o.getMultiLevelWindModel().setAltitudeReference(QtRocket::WindModel::AltitudeReference::AGL);
    o.setWindModelType(QtRocket::WindModelType::MULTI_LEVEL);
    o.setLaunchAltitude(250.0);
    o.setLaunchLatitude(45.5);
    o.setLaunchLongitude(-73.25);
    o.setGeodeticComputation(QtRocket::GeodeticComputationStrategy::WGS84);
    o.setSimulationStepperMethodChoice(QtRocket::SimulationStepperMethod::RK6);
    o.setRandomSeed(-42);
    o.setRandomSeedFixed(true);
    o.setIsaAtmosphere(false);
    o.setLaunchTemperature(280.0);
    o.setLaunchPressure(95000.0);
    o.setLaunchRelativeHumidity(0.55);
    o.setGravityModelType(QtRocket::GravityModelType::CONSTANT);
    o.setConstantGravity(9.7);
    o.setTimeStep(0.02);
    o.setMaxSimulationTime(600.0);
    o.setRecoverySpeedWarning(25.0);
    o.setDrogueLowSpeedWarning(4.0);
    o.setRecoveryDrogueMainHighSpeedWarning(35.0);
    o.setRecoveryDrogueMainLowSpeedWarning(12.0);
}

/// A drag table of two rows: a table the options can hold (the saver writes the rows, never the
/// table).
[[nodiscard]] std::shared_ptr<const MachAoALookup> dragTable()
{
    Result<MachAoALookup> table = MachAoALookup::dragBuilder()
                                      .addData(0.3, {{"cd", 0.35}})
                                      .addData(0.5, {{"cd", 0.4}})
                                      .build();
    EXPECT_TRUE(table.has_value());
    return table ? std::make_shared<const MachAoALookup>(std::move(*table)) : nullptr;
}

void ProbeDocument::addLookups()
{
    SimulationOptions& o = addSimulation("Lookups").getOptions();
    o.setDragLookup(
        std::filesystem::path("/data/drag & co.csv"), dragTable(),
        std::vector<std::string>{"Mach,AoA,Cd", "0.3,0,0.35", "   ", "", " 0.5, 0 ,0.4 & <x>"});
    Result<MachAoALookup> stability = MachAoALookup::stabilityBuilder()
                                          .addData(0.3, {{"cn", 1.0}, {"cm", 2.0}, {"cp", 0.3}})
                                          .build();
    ASSERT_TRUE(stability.has_value());
    o.setStabilityLookup(std::nullopt, std::make_shared<const MachAoALookup>(std::move(*stability)),
                         std::vector<std::string>{"mach,cn,cm,cp", "0.3,1,2,0.3"});
}

void ProbeDocument::addReadAgain()
{
    csvFile =
        directory.write("lookup.csv", "Mach,Cd\r\n0.3,0.35\n\n 0.5,0.40 \r0.7,0.45\r\n# note\n");
    SimulationOptions& o = addSimulation("Read again").getOptions();
    ASSERT_TRUE(o.setDragLookupCsvPath(csvFile).has_value());
}

void ProbeDocument::addDispersion()
{
    Simulation& simulation = addSimulation("Dispersion");
    using Attributes       = LandingDispersionSettings::Attributes;
    simulation.setLandingDispersionSettings(LandingDispersionSettings(
        Attributes{{"runs", "750"}, {"seed", "-123456789"}},
        {Attributes{{"parameter", "windspeed"}, {"distribution", "uniform"}, {"spread", "1.25"}},
         Attributes{
             {"parameter", "airdensity"}, {"distribution", "lognormal"}, {"spread", "0.015"}},
         Attributes{{"parameter", "launchguidedirection"},
                    {"distribution", "normal"},
                    {"spread", "0.07853981633974483"}}}));
}

void ProbeDocument::addPlot()
{
    Simulation& simulation = addSimulation("Plot");
    simulation.setPlotAppearance(type(FlightDataTypeId::TYPE_ALTITUDE),
                                 PlotAppearance(Color(255, 0, 128, 200), LineStyle::DASHED));
    simulation.setPlotAppearance(type(FlightDataTypeId::TYPE_VELOCITY_Z),
                                 PlotAppearance(Color(0, 100, 200), std::nullopt));
    simulation.setPlotAppearance(type(FlightDataTypeId::TYPE_ACCELERATION_Z),
                                 PlotAppearance(std::nullopt, LineStyle::DOTTED));
    simulation.setPlotAppearance(type(FlightDataTypeId::TYPE_MACH_NUMBER),
                                 PlotAppearance(std::nullopt, LineStyle::DASHDOT));
}

/// SimulationSaverProbe.ProbeExtension's configuration: a value of every Java type a Config holds.
[[nodiscard]] Config kitchenSinkConfig()
{
    Config config;
    config.put("flag", Config::Value(true));
    config.put("byte", Config::Value(std::int8_t{7}));
    config.put("short", Config::Value(std::int16_t{-300}));
    config.put("int", Config::Value(5));
    config.put("long", Config::Value(std::int64_t{1234567890123}));
    config.put("float", Config::Value(0.1F));
    config.put("double", Config::Value(5.0));
    config.put("big",
               Config::Value(BigDecimal::parse("12345678901234567890.5").value_or(BigDecimal())));
    config.put("text", Config::Value("a<b & \"c\" 'd'"));
    config.put("list", Config::Value(Config::List{
                           Config::Value(1), Config::Value(2.5),
                           Config::Value(Config::List{Config::Value(false), Config::Value("x")})}));
    return config;
}

void ProbeDocument::addExtensions()
{
    Simulation& simulation = addSimulation("Extensions");
    simulation.getSimulationExtensions().push_back(
        std::make_shared<QtRocket::UnknownSimulationExtension>("info.openrocket.probe.KitchenSink",
                                                               kitchenSinkConfig()));
    const auto script = std::make_shared<QtRocket::ScriptingExtension>();
    script->setLanguage("JavaScript");
    script->setScript("if (a < b && c) {\n  d = 'e';\n}");
    script->setEnabled(false);
    simulation.getSimulationExtensions().push_back(script);
    const auto code = std::make_shared<QtRocket::JavaCode>();
    code->setClassName("com.example.Listener<T>");
    simulation.getSimulationExtensions().push_back(code);
}

std::shared_ptr<FlightDataBranch> ProbeDocument::branch(std::string name, bool abort) const
{
    const auto result = std::make_shared<FlightDataBranch>(
        std::move(name),
        std::initializer_list<std::reference_wrapper<const FlightDataType>>{
            type(FlightDataTypeId::TYPE_TIME), type(FlightDataTypeId::TYPE_ALTITUDE),
            type(FlightDataTypeId::TYPE_VELOCITY_TOTAL), type(FlightDataTypeId::TYPE_MACH_NUMBER)});
    constexpr double                               kNaN = std::numeric_limits<double>::quiet_NaN();
    constexpr std::array<std::array<double, 4>, 4> kRows{{
        {0.0, 0.0, 0.0, 1e-9},
        // NOLINTNEXTLINE(modernize-use-std-numbers): a number of the Java probe, which is no pi
        {0.0010085, 0.012345678, 3.14159265, 0.0001},
        {0.05, 12345.678, -2.5, kNaN},
        {1.25, 99999.95, 1e-3, 123456789.0},
    }};
    for (const std::array<double, 4>& row : kRows)
    {
        result->addPoint();
        result->setValue(type(FlightDataTypeId::TYPE_TIME), row[0]);
        result->setValue(type(FlightDataTypeId::TYPE_ALTITUDE), row[1]);
        result->setValue(type(FlightDataTypeId::TYPE_VELOCITY_TOTAL), row[2]);
        result->setValue(type(FlightDataTypeId::TYPE_MACH_NUMBER), row[3]);
    }
    result->setOptimumAltitude(12345.678);
    result->setTimeToOptimumAltitude(0.05);
    const FlightEvent apogee(FlightEvent::Type::APOGEE, 0.05, nullptr, {},
                             uuid("33333333-3333-4333-8333-333333333333"));
    result->addEvent(FlightEvent(FlightEvent::Type::LAUNCH, 0.0, nullptr, {},
                                 uuid("44444444-4444-4444-8444-444444444444")));
    result->addEvent(apogee);
    result->addEvent(FlightEvent(FlightEvent::Type::RECOVERY_DEVICE_DEPLOYMENT, 0.0500001,
                                 rocket->findComponent(uuid(kChuteId)), {},
                                 uuid("55555555-5555-4555-8555-555555555555")));
    Warning::LargeAOA aoa(0.5);
    aoa.setId(uuid("66666666-6666-4666-8666-666666666666"));
    result->addEvent(FlightEvent(FlightEvent::Type::SIM_WARN, 1.0, nullptr,
                                 FlightEvent::warningData(aoa),
                                 uuid("77777777-7777-4777-8777-777777777777")));
    Warning::EventAfterLanding after(std::string(QtRocket::displayName(apogee.getType())),
                                     apogee.getId());
    after.setId(uuid("88888888-8888-4888-8888-888888888888"));
    result->addEvent(FlightEvent(FlightEvent::Type::SIM_WARN, 1.25, nullptr,
                                 FlightEvent::warningData(after),
                                 uuid("99999999-9999-4999-8999-999999999999")));
    if (abort)
    {
        result->addEvent(FlightEvent(FlightEvent::Type::SIM_ABORT, 1.25, nullptr,
                                     SimulationAbort(SimulationAbort::Cause::NO_MOTORS_FIRED),
                                     uuid("aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa")));
    }
    result->immute();
    return result;
}

/// SimulationSaverProbe.addWarnings(): one stored warning of every type the saver writes a
/// parameter for, an Other and the EventAfterLanding of the branch's events.
void addWarnings(FlightData& data, const Rocket& rocket, const FlightDataBranch& branch)
{
    const MessageSource nose  = MessageSource::of(*rocket.findComponent(uuid(kNoseId)));
    const MessageSource chute = MessageSource::of(*rocket.findComponent(uuid(kChuteId)));
    Warning::LargeAOA   aoa(0.5);
    aoa.setId(uuid("66666666-6666-4666-8666-666666666666"));
    aoa.setSources({nose, chute});
    data.getWarningSet().add(aoa);
    Warning::RecoveryHighSpeedDeployment speed(31.94803510503947, {chute});
    speed.setId(uuid("bbbbbbbb-bbbb-4bbb-8bbb-bbbbbbbbbbbb"));
    data.getWarningSet().add(speed);
    Warning::Other other = Warning::fromString("Free text & <markup>");
    other.setId(uuid("cccccccc-cccc-4ccc-8ccc-cccccccccccc"));
    data.getWarningSet().add(other);
    for (const FlightEvent& event : branch.getEvents())
    {
        const std::shared_ptr<const Warning> warning = event.getWarning();
        if (event.getType() == FlightEvent::Type::SIM_WARN &&
            dynamic_cast<const Warning::EventAfterLanding*>(warning.get()) != nullptr)
        {
            data.getWarningSet().add(*warning);
        }
    }
}

void ProbeDocument::addExternal()
{
    const std::shared_ptr<FlightDataBranch> external = branch("Sustainer & <external>", false);
    const auto                              data     = std::make_shared<FlightData>(
        std::initializer_list<std::shared_ptr<FlightDataBranch>>{external});
    addWarnings(*data, *rocket, *external);
    document->addSimulation(std::make_shared<Simulation>(
        document.get(), *rocket, Simulation::Status::EXTERNAL, "External",
        SimulationOptions(preferences.store), std::vector<std::shared_ptr<SimulationExtension>>{},
        data, std::map<std::string, PlotAppearance>{}, &preferences.store));
}

void ProbeDocument::addAborted()
{
    const auto data = std::make_shared<FlightData>(
        std::initializer_list<std::shared_ptr<FlightDataBranch>>{branch("Sustainer", true)});
    document->addSimulation(std::make_shared<Simulation>(
        document.get(), *rocket, Simulation::Status::UPTODATE, "Aborted",
        SimulationOptions(preferences.store), std::vector<std::shared_ptr<SimulationExtension>>{},
        data, std::map<std::string, PlotAppearance>{}, &preferences.store));
}

void ProbeDocument::addSummary()
{
    constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
    const auto data = std::make_shared<FlightData>(1234.5678901, 99.0, kNaN, 0.000123456, 10.0,
                                                   kNaN, 5.0000004, 15.0, 7.25, 0.0);
    document->addSimulation(std::make_shared<Simulation>(
        document.get(), *rocket, Simulation::Status::LOADED, "Summary",
        SimulationOptions(preferences.store), std::vector<std::shared_ptr<SimulationExtension>>{},
        data, std::map<std::string, PlotAppearance>{}, &preferences.store));
}

/// What a save gave: the text, or the Error's message, and the warnings.
struct Saved
{
    std::string text;
    std::string error;
    WarningSet  warnings;
};

/// Saves @p document with or without the simulation data, with an empty preference store.
[[nodiscard]] Saved saveOf(OpenRocketDocument& document, bool saveSimulationData)
{
    const QtRocket::InMemoryPreferences preferences;
    StorageOptions                      options;
    options.setSaveSimulationData(saveSimulationData);
    Saved                     saved;
    ErrorSet                  errors;
    const Result<std::string> text =
        OpenRocketSaver::save(document, options, preferences, saved.warnings, errors);
    if (text.has_value())
    {
        saved.text = *text;
    }
    else
    {
        saved.error = text.error().message;
    }
    return saved;
}

/// The <simulation> elements of the .ork text @p text, each with its lines and their '\n'.
[[nodiscard]] std::vector<std::string> simulationsOf(std::string_view text)
{
    std::vector<std::string>   simulations;
    constexpr std::string_view kOpen  = "    <simulation status=";
    constexpr std::string_view kClose = "    </simulation>\n";
    std::size_t                start  = text.find(kOpen);
    while (start != std::string_view::npos)
    {
        const std::size_t end = text.find(kClose, start);
        if (end == std::string_view::npos)
        {
            break;
        }
        simulations.emplace_back(text.substr(start, end + kClose.size() - start));
        start = text.find(kOpen, end);
    }
    return simulations;
}

/// @p text with each "@NAME@" of @p replacements replaced by its text.
[[nodiscard]] std::string withPaths(std::string_view text, std::string_view drag,
                                    std::string_view file)
{
    std::string result(text);
    for (const auto& [placeholder, value] :
         {std::pair<std::string_view, std::string_view>{"@DRAG@", drag}, {"@FILE@", file}})
    {
        const std::size_t at = result.find(placeholder);
        if (at != std::string::npos)
        {
            result.replace(at, placeholder.size(), value);
        }
    }
    return result;
}

/// The simulations of @p saved against @p expected, one by one: what differs, "" when nothing.
[[nodiscard]] std::string simulationProblems(const Saved&                      saved,
                                             std::span<const std::string_view> expected,
                                             const ProbeDocument&              probe)
{
    if (!saved.error.empty())
    {
        return saved.error;
    }
    const std::optional<std::filesystem::path>& drag =
        probe.document->getSimulation(1)->getOptions().getDragLookupCsvPath();
    std::size_t       dropped = 0;
    const std::string dragText =
        OrkWriter::escapeText(QtRocket::pathToUtf8(drag.value_or("")), dropped);
    const std::string fileText =
        OrkWriter::escapeText(QtRocket::pathToUtf8(probe.csvFile), dropped);
    const std::vector<std::string> simulations = simulationsOf(saved.text);
    std::string                    problems;
    if (simulations.size() != expected.size())
    {
        problems += std::format("{} simulations, not {}\n", simulations.size(), expected.size());
    }
    for (std::size_t i = 0; i < std::min(simulations.size(), expected.size()); ++i)
    {
        const std::string theirs = withPaths(expected[i], dragText, fileText);
        if (simulations[i] != theirs)
        {
            problems += std::format("simulation {}:\n--- QtRocket\n{}--- OpenRocket\n{}", i,
                                    simulations[i], theirs);
        }
    }
    return problems;
}

// The nine simulations of SimulationSaverProbe, saved without and then with the data, as
// OpenRocket saves them: every condition, a multi-level wind, the lookup tables (rows with a
// path, rows alone, a file read again: blank lines left out, \r\n, \r and \n all ending a line),
// the landing dispersion settings, the plot appearances by symbol in Java's order ("Az", "M",
// "Vz", "h"), the configurations of two extensions with every Java type, an EXTERNAL simulation
// whose branch is written either way, events of every kind the saver treats apart (a source, a
// SIM_WARN with its warning, an EventAfterLanding with its event, a SIM_ABORT with its cause),
// stored warnings of every type with their sources and parameters, the numbers of the data
// rounded as OpenRocket stores them, and summary values with NaNs left out. Saved without the
// data, every status is "notsimulated" and only the EXTERNAL simulation keeps its branch.
TEST(SimulationSaver, WritesEverySimulationAsOpenRocket)
{
    ProbeDocument probe;
    const Saved   withoutData = saveOf(*probe.document, false);
    EXPECT_EQ(simulationProblems(withoutData, kJavaWithoutData, probe), "");
    EXPECT_TRUE(withoutData.warnings.empty());
    const Saved withData = saveOf(*probe.document, true);
    EXPECT_EQ(simulationProblems(withData, kJavaWithData, probe), "");
    EXPECT_TRUE(withData.warnings.empty());
}

// ==================================================================== what is QtRocket's own

/// A design of the Estes Alpha III with one simulation for TEST_FCID_0, as a JUnit test makes it.
struct OneSimulation
{
    /// Declared first, destroyed last: the simulation keeps the store.
    JavaTestPreferences                 preferences;
    std::unique_ptr<OpenRocketDocument> document;
    std::shared_ptr<Simulation>         simulation;

    OneSimulation()
    {
        TestEstesAlphaIII alpha;
        Rocket&           rocket = *alpha.rocket;
        document   = OpenRocketDocumentFactory::createDocumentFromRocket(std::move(alpha.rocket));
        simulation = std::make_shared<Simulation>(rocket, preferences.store);
        simulation->setName("Sim");
        simulation->setFlightConfigurationId(testFcid(0));
        document->addSimulation(simulation);
    }
};

/// The lines of @p text from the first line that holds @p open to the next that holds @p close,
/// both included, each with its '\n'; empty when there are none.
[[nodiscard]] std::string elementOf(std::string_view text, std::string_view open,
                                    std::string_view close)
{
    const std::size_t at = text.find(open);
    if (at == std::string_view::npos)
    {
        return {};
    }
    const std::size_t start = text.rfind('\n', at) + 1;
    const std::size_t end   = text.find(close, at);
    if (end == std::string_view::npos)
    {
        return {};
    }
    return std::string(text.substr(start, text.find('\n', end) + 1 - start));
}

/// Loads @p text from memory, without a directory, in @p environment.
[[nodiscard]] Result<LoadedDocument> reload(LoaderEnvironment& environment, const std::string& text)
{
    const GeneralRocketLoader loader(environment.context());
    return loader.load(QtRocket::stringToBytes(text));
}

// The text of the warning of a lookup table whose rows could not be saved.
TEST(SimulationSaver, NamesTheTableAndItsFileInTheLookupWarning)
{
    EXPECT_EQ(SimulationSaver::lookupRowsNotSaved("draglookup", "Sim", "/x/drag.csv"),
              "The rows of the draglookup table of the simulation \"Sim\" were not saved: the "
              "file \"/x/drag.csv\" could not be read.");
}

// Decision O10 of tier 10: while the options hold rows, the file of the path is never read, even
// when it is there and says something else; the rows are written as they are held.
TEST(SimulationSaver, NeverReadsTheFileOfATableWhoseRowsTheOptionsHold)
{
    const TempDir               directory;
    const std::filesystem::path file = directory.write("drag.csv", "Mach,Cd\n0.3,0.99\n0.5,0.99\n");
    OneSimulation               design;
    design.simulation->getOptions().setDragLookup(
        file, dragTable(), std::vector<std::string>{"Mach,Cd", "0.3,0.35", "0.5,0.4"});
    const Saved saved   = saveOf(*design.document, false);
    std::size_t dropped = 0;
    EXPECT_EQ(elementOf(saved.text, "<draglookup", "</draglookup>"),
              std::format("        <draglookup file=\"{}\">\n"
                          "          <row>Mach,Cd</row>\n"
                          "          <row>0.3,0.35</row>\n"
                          "          <row>0.5,0.4</row>\n"
                          "        </draglookup>\n",
                          OrkWriter::escapeText(QtRocket::pathToUtf8(file), dropped)));
    EXPECT_TRUE(saved.warnings.empty());
}

/// What a save writes of a drag table that was read from a file whose contents @p replace then
/// changes (null: the file is removed), and the save's warnings, one per line.
[[nodiscard]] std::string lookupAfter(void (*replace)(const std::filesystem::path& file))
{
    const TempDir               directory;
    const std::filesystem::path file = directory.write("drag.csv", "Mach,Cd\n0.3,0.35\n");
    OneSimulation               design;
    EXPECT_TRUE(design.simulation->getOptions().setDragLookupCsvPath(file).has_value());
    if (replace == nullptr)
    {
        std::filesystem::remove(file);
    }
    else
    {
        replace(file);
    }
    const Saved saved  = saveOf(*design.document, false);
    std::string result = elementOf(saved.text, "<draglookup", "</draglookup>");
    for (const Warning& warning : saved.warnings)
    {
        result += "W " + warning.toString() + "\n";
    }
    std::size_t       dropped = 0;
    const std::string path    = OrkWriter::escapeText(QtRocket::pathToUtf8(file), dropped);
    const std::size_t at      = result.find(path);
    if (at != std::string::npos)
    {
        result.replace(at, path.size(), "FILE");
    }
    return result;
}

/// The element and warning of a table whose file could not be read again.
constexpr std::string_view kNotReadAgain =
    "        <draglookup file=\"FILE\">\n"
    "        </draglookup>\n"
    "W The rows of the draglookup table of the simulation \"Sim\" were not saved: the file \"";

// Decision O10 of tier 10: a table whose options hold no rows is read again from its file
// (CsvMachAoALookup::readLines(), Java's Files.readAllLines()); when that fails, the element
// keeps its path without rows and the save warns (Java skips a missing file and only logs a
// failed read): a file that is gone, one that is not UTF-8, one beyond the bound of 32 MiB, a
// directory in its place.
TEST(SimulationSaver, WarnsWhenTheFileOfATableCannotBeReadAgain)
{
    EXPECT_EQ(lookupAfter([](const std::filesystem::path& file) {
                  EXPECT_TRUE(QtRocket::writeTextFile(file, "Mach,Cd\n0.7,0.45\n"));
              }),
              "        <draglookup file=\"FILE\">\n"
              "          <row>Mach,Cd</row>\n"
              "          <row>0.7,0.45</row>\n"
              "        </draglookup>\n");
    const std::string gone = lookupAfter(nullptr);
    EXPECT_TRUE(gone.starts_with(kNotReadAgain)) << gone;
    const std::string notUtf8 = lookupAfter([](const std::filesystem::path& file) {
        EXPECT_TRUE(QtRocket::writeTextFile(file, "Mach,Cd\n0.3,\xFF\n"));
    });
    EXPECT_TRUE(notUtf8.starts_with(kNotReadAgain)) << notUtf8;
    const std::string beyond = lookupAfter([](const std::filesystem::path& file) {
        // A file of zeros, sparse where the file system can: its size alone is refused.
        std::filesystem::resize_file(file, QtRocket::CsvMachAoALookup::kMaxFileBytes + 1);
    });
    EXPECT_TRUE(beyond.starts_with(kNotReadAgain)) << beyond.substr(0, 300);
    const std::string directory = lookupAfter([](const std::filesystem::path& file) {
        std::filesystem::remove(file);
        std::filesystem::create_directory(file);
    });
    EXPECT_TRUE(directory.starts_with(kNotReadAgain)) << directory;
}

// Decision E3 of tier 10: the keys of an extension's configuration and the attributes of the
// landing dispersion settings, which OpenRocket writes unescaped, are escaped, and the file reads
// back with every text as it was. The attributes beyond runs and seed (and beyond parameter,
// distribution and spread) follow by name; one whose name is no XML name is left out.
TEST(SimulationSaver, EscapesTheKeysAndAttributesOpenRocketWritesUnescaped)
{
    OneSimulation design;
    Config        config;
    config.put("k<&\"", Config::Value("v<&"));
    design.simulation->getSimulationExtensions().push_back(
        std::make_shared<QtRocket::UnknownSimulationExtension>("x.y.Z", config));
    using Attributes = LandingDispersionSettings::Attributes;
    design.simulation->setLandingDispersionSettings(LandingDispersionSettings(
        Attributes{{"zeta", "z&"},
                   {"seed", "<1>"},
                   {"alpha", "a"},
                   {"runs", "\"5\""},
                   {"bad name", "x"},
                   {"1st", "y"}},
        {Attributes{{"spread", "0.1"}, {"extra", "'e'"}, {"parameter", "p&"}}}));
    const Saved saved = saveOf(*design.document, false);
    EXPECT_EQ(elementOf(saved.text, "<extension ", "</extension>"),
              "      <extension extensionid=\"x.y.Z\">\n"
              "        <entry key=\"k&lt;&amp;&quot;\" type=\"string\">v&lt;&amp;</entry>\n"
              "      </extension>\n");
    EXPECT_EQ(elementOf(saved.text, "<landingdispersion", "</landingdispersion>"),
              "      <landingdispersion runs=\"&quot;5&quot;\" seed=\"&lt;1&gt;\" alpha=\"a\" "
              "zeta=\"z&amp;\">\n"
              "        <uncertainty parameter=\"p&amp;\" spread=\"0.1\" extra=\"&#39;e&#39;\"/>\n"
              "      </landingdispersion>\n");

    LoaderEnvironment            environment;
    const Result<LoadedDocument> loaded = reload(environment, saved.text);
    ASSERT_TRUE(loaded.has_value()) << loaded.error().toString();
    const Simulation& back = *loaded->document->getSimulation(0);
    ASSERT_EQ(back.getSimulationExtensions().size(), 1U);
    EXPECT_EQ(back.getSimulationExtensions().front()->getConfig().getString("k<&\"", "none"),
              "v<&");
    EXPECT_EQ(back.getLandingDispersionSettings(),
              LandingDispersionSettings(
                  Attributes{{"zeta", "z&"}, {"seed", "<1>"}, {"alpha", "a"}, {"runs", "\"5\""}},
                  {Attributes{{"spread", "0.1"}, {"extra", "'e'"}, {"parameter", "p&"}}}));
}

// Settings without attributes and uncertainties are an element with nothing in it (OpenRocket's
// settings always have their runs and seed).
TEST(SimulationSaver, WritesEmptyLandingDispersionSettings)
{
    OneSimulation design;
    design.simulation->setLandingDispersionSettings(LandingDispersionSettings());
    EXPECT_EQ(elementOf(saveOf(*design.document, false).text, "<landingdispersion",
                        "</landingdispersion>"),
              "      <landingdispersion>\n"
              "      </landingdispersion>\n");
}

/// Two plot appearances of symbols outside ASCII, the first as a file may list them: U+FF21
/// (FULLWIDTH LATIN CAPITAL LETTER A, UTF-8 EF BC A1) and U+10000 (LINEAR B SYLLABLE B008 A,
/// UTF-8 F0 90 80 80, UTF-16 D800 DC00).
constexpr std::string_view kWidePlotAppearance =
    "      <plotappearance>\n"
    "        <series symbol=\"\xEF\xBC\xA1\" linestyle=\"dashed\"/>\n"
    "        <series symbol=\"\xF0\x90\x80\x80\" linestyle=\"dotted\"/>\n"
    "      </plotappearance>\n";

// The plot appearances are written in the order of Java's TreeMap of their symbols,
// String.compareTo(), which compares UTF-16 code units: U+10000, a surrogate pair from D800, comes
// before U+FF21, whose UTF-8 bytes come first.
TEST(SimulationSaver, OrdersThePlotAppearancesAsJavasTreeMap)
{
    OneSimulation     design;
    std::string       text       = saveOf(*design.document, false).text;
    const std::string conditions = "      </conditions>\n";
    const std::size_t at         = text.find(conditions);
    ASSERT_NE(at, std::string::npos);
    text.insert(at + conditions.size(), kWidePlotAppearance);

    LoaderEnvironment            environment;
    const Result<LoadedDocument> loaded = reload(environment, text);
    ASSERT_TRUE(loaded.has_value()) << loaded.error().toString();
    EXPECT_EQ(
        elementOf(saveOf(*loaded->document, false).text, "<plotappearance>", "</plotappearance>"),
        "      <plotappearance>\n"
        "        <series symbol=\"\xF0\x90\x80\x80\" linestyle=\"dotted\"/>\n"
        "        <series symbol=\"\xEF\xBC\xA1\" linestyle=\"dashed\"/>\n"
        "      </plotappearance>\n");
}

/// A simulation of @p design with summary flight data whose stored warnings are @p warnings,
/// added after the one there is.
void addFlightWithWarnings(OneSimulation& design, const WarningSet& warnings)
{
    constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
    const auto       data =
        std::make_shared<FlightData>(100.0, 50.0, kNaN, kNaN, 5.0, 20.0, kNaN, kNaN, kNaN, kNaN);
    data->getWarningSet() = warnings;
    Rocket& rocket        = design.document->getRocket();
    design.document->addSimulation(std::make_shared<Simulation>(
        design.document.get(), rocket, Simulation::Status::LOADED, "Flight",
        SimulationOptions(design.preferences.store),
        std::vector<std::shared_ptr<SimulationExtension>>{}, data,
        std::map<std::string, PlotAppearance>{}, &design.preferences.store));
}

// A source of a stored warning that the rocket does not have (MessageSource::removed(), as the
// loader keeps it) is written with the id it keeps, and reads back as that id (OpenRocket writes
// the id of its REMOVED component, a random one).
TEST(SimulationSaver, WritesTheKeptIdOfASourceTheRocketDoesNotHave)
{
    OneSimulation  design;
    const Uuid     gone = uuid("dddddddd-dddd-4ddd-8ddd-dddddddddddd");
    Warning::Other other("Gone");
    other.setId(uuid("eeeeeeee-eeee-4eee-8eee-eeeeeeeeeeee"));
    other.setSources({MessageSource::removed(gone)});
    WarningSet warnings;
    warnings.add(other);
    addFlightWithWarnings(design, warnings);
    const Saved saved = saveOf(*design.document, false);
    EXPECT_EQ(elementOf(saved.text, "<warning ", "</warning>"),
              "        <warning type=\"Other\">\n"
              "          <id>eeeeeeee-eeee-4eee-8eee-eeeeeeeeeeee</id>\n"
              "          <description>Gone</description>\n"
              "          <priority>NORMAL</priority>\n"
              "          <source>dddddddd-dddd-4ddd-8ddd-dddddddddddd</source>\n"
              "          Gone:  &quot;&lt;i&gt;Component Removed From Rocket&lt;/i&gt;&quot;\n"
              "        </warning>\n");

    LoaderEnvironment            environment;
    const Result<LoadedDocument> loaded = reload(environment, saved.text);
    ASSERT_TRUE(loaded.has_value()) << loaded.error().toString();
    const WarningSet& back =
        loaded->document->getSimulation(1)->getSimulatedData()->getWarningSet();
    ASSERT_EQ(back.size(), 1U);
    ASSERT_EQ(back.begin()->sources().size(), 1U);
    EXPECT_EQ(back.begin()->sources().front().id, gone);
}

/// The <event> lines of the branch of @p data, saved.
[[nodiscard]] std::string eventLinesOf(const FlightDataBranch& branch)
{
    OrkWriter writer;
    SimulationSaver::saveFlightDataBranch(branch, writer);
    std::string events;
    for (std::string_view text = writer.text(); !text.empty();)
    {
        const std::size_t      end  = text.find('\n');
        const std::string_view line = text.substr(0, end + 1);
        if (line.contains("<event "))
        {
            events += line;
        }
        text.remove_prefix(line.size());
    }
    return events;
}

// An EventAfterLanding that names no event (one a caller made without it) gives a SIM_WARN event
// without "eventid" (OpenRocket dies of a NullPointerException there), and a branch is written
// with no rows at all.
TEST(SimulationSaver, LeavesOutTheEventOfAnEventAfterLandingWithoutOne)
{
    FlightDataBranch branch("B",
                            std::initializer_list<std::reference_wrapper<const FlightDataType>>{
                                type(FlightDataTypeId::TYPE_TIME)});
    Warning::EventAfterLanding after;
    after.setId(uuid("88888888-8888-4888-8888-888888888888"));
    branch.addEvent(FlightEvent(FlightEvent::Type::SIM_WARN, 2.0, nullptr,
                                FlightEvent::warningData(after),
                                uuid("99999999-9999-4999-8999-999999999999")));
    EXPECT_EQ(eventLinesOf(branch),
              "  <event time=\"2\" type=\"simwarn\" id=\"99999999-9999-4999-8999-999999999999\" "
              "warnid=\"88888888-8888-4888-8888-888888888888\"/>\n");
    OrkWriter writer;
    SimulationSaver::saveFlightDataBranch(branch, writer);
    EXPECT_TRUE(writer.text().starts_with("<databranch name=\"B\" types=\"time\">\n"))
        << writer.text();
    EXPECT_TRUE(writer.text().ends_with("</databranch>\n"));
}

// A null extension in the simulation's list is left out (OpenRocket: a NullPointerException).
TEST(SimulationSaver, LeavesOutANullExtension)
{
    OneSimulation design;
    design.simulation->getSimulationExtensions().push_back(nullptr);
    design.simulation->getSimulationExtensions().push_back(
        std::make_shared<QtRocket::UnknownSimulationExtension>("x.y.Z"));
    const Saved saved = saveOf(*design.document, false);
    EXPECT_EQ(elementOf(saved.text, "<extension ", "</simulation>"),
              "      <extension extensionid=\"x.y.Z\">\n"
              "      </extension>\n"
              "    </simulation>\n");
}

/// A warning set that reached its cap as the loader's set of a flight's stored warnings does:
/// DocumentConfig::kMaxWarnings warnings and then the overflow warning (1001 in all), without a
/// capacity any more.
[[nodiscard]] WarningSet cappedWarnings()
{
    WarningSet capped;
    capped.setCapacity(QtRocket::DocumentConfig::kMaxWarnings,
                       Warning::fromString(QtRocket::LoaderWarnings::tooManyStoredWarnings()));
    for (std::size_t i = 0; i < QtRocket::DocumentConfig::kMaxWarnings + 200; ++i)
    {
        capped.add(std::format("Warning {}", i));
    }
    capped.clearCapacity();
    return capped;
}

/// How many times @p text holds @p part.
[[nodiscard]] std::size_t occurrences(std::string_view text, std::string_view part)
{
    std::size_t count = 0;
    for (std::size_t at = text.find(part); at != std::string_view::npos;
         at             = text.find(part, at + 1))
    {
        ++count;
    }
    return count;
}

/// How many of @p warnings are the overflow warning of the stored warnings of a flight.
[[nodiscard]] std::size_t overflowWarnings(const WarningSet& warnings)
{
    const Warning::Other overflow =
        Warning::fromString(QtRocket::LoaderWarnings::tooManyStoredWarnings());
    std::size_t count = 0;
    for (const Warning& warning : warnings)
    {
        count += warning == overflow ? 1U : 0U;
    }
    return count;
}

/// The id of the last warning of @p warnings (a nil id for none).
[[nodiscard]] Uuid lastId(const WarningSet& warnings)
{
    Uuid id;
    for (const Warning& warning : warnings)
    {
        id = warning.id();
    }
    return id;
}

// Decision U6 of tier 9 and its note for tier 10: the stored warnings of a flight whose cap was
// reached (DocumentConfig::kMaxWarnings and then the overflow warning, 1001 in all) are saved as
// 1001 warnings and read back as the same 1001, the overflow warning once, with its id.
TEST(SimulationSaver, SavesTheWarningsOfACappedFlightAsTheyAre)
{
    const WarningSet capped = cappedWarnings();
    ASSERT_EQ(capped.size(), QtRocket::DocumentConfig::kMaxWarnings + 1);

    OneSimulation design;
    addFlightWithWarnings(design, capped);
    const Saved saved = saveOf(*design.document, false);
    EXPECT_EQ(occurrences(saved.text, "<warning "), QtRocket::DocumentConfig::kMaxWarnings + 1);

    LoaderEnvironment            environment;
    const Result<LoadedDocument> loaded = reload(environment, saved.text);
    ASSERT_TRUE(loaded.has_value()) << loaded.error().toString();
    const WarningSet& back =
        loaded->document->getSimulation(1)->getSimulatedData()->getWarningSet();
    ASSERT_EQ(back.size(), QtRocket::DocumentConfig::kMaxWarnings + 1);
    EXPECT_EQ(back.toString(), capped.toString());
    EXPECT_EQ(overflowWarnings(back), 1U);
    EXPECT_EQ(lastId(back), lastId(capped));
    EXPECT_EQ(elementOf(saveOf(*loaded->document, false).text, "<flightdata", "</flightdata>"),
              elementOf(saved.text, "<flightdata", "</flightdata>"));
}

/// Whether @p stored is what the storage format makes of @p value (Strings::doubleToString()
/// with 6 decimals, read back): 0 for a value within MathUtil::kEpsilon / 2 of zero, NaN and the
/// infinities as they are; from 0.001 to below 10000 six decimals (at most 5e-7 off); otherwise
/// seven significant digits (at most 5e-7 of the value off). The bounds allow for the last bit
/// of the conversions.
[[nodiscard]] bool isStoredForm(double value, double stored)
{
    if (std::isnan(value) || std::isinf(value))
    {
        return QtRocket::Test::junitEquals(value, stored);
    }
    if (QtRocket::MathUtil::equals(value, 0.0))
    {
        return stored == 0.0;
    }
    constexpr double kHalfDigit = 5e-7 * (1.0 + 1e-9);
    const double     magnitude  = std::abs(value);
    const double     allowed    = (magnitude >= 0.001 && magnitude < 10000)
                                      ? kHalfDigit + (magnitude * 1e-15)
                                      : kHalfDigit * magnitude;
    return std::abs(stored - value) <= allowed;
}

/// What a reload changed in a flight beyond the rounding of the storage format.
struct Rounding
{
    std::size_t values{0};     ///< the values compared
    std::size_t identical{0};  ///< those that came back bit for bit
    std::string problems;
};

/// Compares every value of every branch of @p original with @p reloaded (isStoredForm()).
[[nodiscard]] Rounding roundingOf(const FlightData& original, const FlightData& reloaded)
{
    Rounding rounding;
    if (original.getBranchCount() != reloaded.getBranchCount())
    {
        rounding.problems = "another number of branches\n";
        return rounding;
    }
    for (std::size_t b = 0; b < original.getBranchCount(); ++b)
    {
        const FlightDataBranch& ours = original.getBranch(b);
        const FlightDataBranch& back = reloaded.getBranch(b);
        for (const FlightDataType* column : ours.getTypes())
        {
            const std::vector<double>* expected = ours.getView(*column);
            const std::vector<double>* actual   = back.getView(*column);
            if (actual == nullptr || actual->size() != expected->size())
            {
                rounding.problems += std::format("branch {} {}: missing\n", b, column->getName());
                continue;
            }
            for (std::size_t row = 0; row < expected->size(); ++row)
            {
                const double value  = (*expected)[row];
                const double stored = (*actual)[row];
                ++rounding.values;
                rounding.identical += QtRocket::Test::junitEquals(value, stored) ? 1U : 0U;
                if (!isStoredForm(value, stored))
                {
                    rounding.problems += std::format("branch {} {} row {}: {} stored as {}\n", b,
                                                     column->getName(), row, value, stored);
                }
            }
        }
    }
    return rounding;
}

/// The lines in which @p before and @p after differ, "<before> -> <after>" without the
/// indentation; a text of another number of lines is named instead.
[[nodiscard]] std::string changedLines(std::string_view before, std::string_view after)
{
    std::vector<std::string_view> a;
    std::vector<std::string_view> b;
    for (auto [text, lines] : {std::pair{before, &a}, std::pair{after, &b}})
    {
        while (!text.empty())
        {
            const std::size_t end = text.find('\n');
            lines->push_back(text.substr(0, end));
            text.remove_prefix(end == std::string_view::npos ? text.size() : end + 1);
        }
    }
    if (a.size() != b.size())
    {
        return std::format("{} lines -> {} lines\n", a.size(), b.size());
    }
    std::string changes;
    for (std::size_t i = 0; i < a.size(); ++i)
    {
        if (a[i] != b[i])
        {
            changes += std::format("{} -> {}\n", Strings::trim(a[i]), Strings::trim(b[i]));
        }
    }
    return changes;
}

// A flight simulated anew is stored rounded (OpenRocket's storage format: six decimals from
// 0.001 to below 10000, seven significant digits beyond, 0 within 5e-9 of zero), so it reads back
// changed within that rounding only. From the first save on, the text of the data is a fixed
// point: the summary values (which a reload works out from its first branch) and the branches of
// the second save are those of the first, and a third save is the second.
TEST(SimulationSaver, StoresAFreshFlightRoundedToItsFormat)
{
    OneSimulation      design;
    SimulationOptions& options = design.simulation->getOptions();
    options.setIsaAtmosphere(true);
    options.setTimeStep(0.05);
    options.setRandomSeed(42);
    options.setRandomSeedFixed(true);
    QtRocket::Test::simulateOrFail(*design.simulation);
    const Saved first = saveOf(*design.document, true);

    LoaderEnvironment environment;
    environment.findEveryMotor();
    const Result<LoadedDocument> once = reload(environment, first.text);
    ASSERT_TRUE(once.has_value()) << once.error().toString();
    const Rounding rounding = roundingOf(*design.simulation->getSimulatedData(),
                                         *once->document->getSimulation(0)->getSimulatedData());
    EXPECT_EQ(rounding.problems, "");
    EXPECT_GT(rounding.values, 1000U);
    EXPECT_LT(rounding.identical, rounding.values) << "nothing was rounded";

    const Saved second = saveOf(*once->document, true);
    EXPECT_EQ(elementOf(second.text, "<flightdata ", "<flightdata "),
              elementOf(first.text, "<flightdata ", "<flightdata "));
    EXPECT_EQ(elementOf(second.text, "<databranch ", "</databranch>"),
              elementOf(first.text, "<databranch ", "</databranch>"));
    const Result<LoadedDocument> twice = reload(environment, second.text);
    ASSERT_TRUE(twice.has_value()) << twice.error().toString();
    EXPECT_EQ(changedLines(second.text, saveOf(*twice->document, true).text), "");
}

}  // namespace
