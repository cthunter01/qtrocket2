#include "QtRocket/file/openrocket/ComponentSaver.h"

#include <array>
#include <cstddef>
#include <format>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/OpenRocketDocumentFactory.h"
#include "QtRocket/document/StorageOptions.h"
#include "QtRocket/file/GeneralRocketLoader.h"
#include "QtRocket/file/LoadedDocument.h"
#include "QtRocket/file/openrocket/AssemblySaver.h"
#include "QtRocket/file/openrocket/ExternalComponentSaver.h"
#include "QtRocket/file/openrocket/InternalComponentSaver.h"
#include "QtRocket/file/openrocket/OpenRocketSaver.h"
#include "QtRocket/file/openrocket/OrkWriter.h"
#include "QtRocket/file/openrocket/RocketComponentSaver.h"
#include "QtRocket/logging/ErrorSet.h"
#include "QtRocket/logging/Warning.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/material/Material.h"
#include "QtRocket/material/MaterialGroup.h"
#include "QtRocket/motor/IgnitionEvent.h"
#include "QtRocket/motor/Motor.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/rocket/Appearance.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/Bulkhead.h"
#include "QtRocket/rocket/CenteringRing.h"
#include "QtRocket/rocket/ClusterConfiguration.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/Decal.h"
#include "QtRocket/rocket/DeploymentConfiguration.h"
#include "QtRocket/rocket/DesignType.h"
#include "QtRocket/rocket/EllipticalFinSet.h"
#include "QtRocket/rocket/EngineBlock.h"
#include "QtRocket/rocket/FinSet.h"
#include "QtRocket/rocket/Finish.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/FreeformFinSet.h"
#include "QtRocket/rocket/InnerTube.h"
#include "QtRocket/rocket/LaunchLug.h"
#include "QtRocket/rocket/MassComponent.h"
#include "QtRocket/rocket/MotorConfiguration.h"
#include "QtRocket/rocket/NoseCone.h"
#include "QtRocket/rocket/Parachute.h"
#include "QtRocket/rocket/ParallelStage.h"
#include "QtRocket/rocket/PodSet.h"
#include "QtRocket/rocket/RailButton.h"
#include "QtRocket/rocket/ReferenceType.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/ShockCord.h"
#include "QtRocket/rocket/StageSeparationConfiguration.h"
#include "QtRocket/rocket/Streamer.h"
#include "QtRocket/rocket/Transition.h"
#include "QtRocket/rocket/TransitionShape.h"
#include "QtRocket/rocket/TrapezoidFinSet.h"
#include "QtRocket/rocket/TubeCoupler.h"
#include "QtRocket/rocket/TubeFinSet.h"
#include "QtRocket/rocket/position/AngleMethod.h"
#include "QtRocket/rocket/position/AxialMethod.h"
#include "QtRocket/rocket/position/RadiusMethod.h"
#include "QtRocket/rocket/preset/ComponentPresetReference.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Color.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"
#include "QtRocket/util/LineStyle.h"
#include "QtRocket/util/Uuid.h"
#include "file/RocketLoaderTestSupport.h"
#include "rocket/TestRockets.h"

// The component savers (ComponentSaver and the savers it chooses: AssemblySaver,
// ExternalComponentSaver, InternalComponentSaver, RocketComponentSaver), against what
// OpenRocket's savers write for the same components.
//
// The kitchen sink is a rocket built by hand with one or more components of each of the 22
// kinds, set up so that it also holds every element no committed re-save holds: a rocket with a
// designer, a kit name, a revision, a custom reference length and a two-line comment; flight
// configurations of their own name and with an inactive stage; a stage with its mass, CG and CD
// overridden with each subcomponents flag; a nose cone with a colour, a line style, a filled
// wall, a decal and an inside appearance; a motor mount with a plugged motor of a nozzle exit
// diameter and a motor that is no thrust curve (each gets its <ignitionconfiguration>); a fin
// set with a tab height but no length, one with both, one with a tab length but no height (the
// tab is written for the second only); a drogue with a deployment of a configuration; a stage
// with a separation of a configuration; texts with the characters XML escapes. The Java
// probe KitchenSinkProbe.java of tier 10a part S2 built the same rocket with OpenRocket's classes
// and printed the lines of each component's saver (getElements()) and the <rocket> element of
// OpenRocketSaver's save of its document: the generated table below. The committed re-saves
// (327 components of 16 examples, 151 of 13 test rockets) are compared line for line by
// tests/core/goldens/ork_resave_diff_golden_tests.cpp.

namespace
{

using QtRocket::AssemblySaver;
using QtRocket::AxialMethod;
using QtRocket::BodyTube;
using QtRocket::ComponentKind;
using QtRocket::ComponentSaver;
using QtRocket::Coordinate;
using QtRocket::ErrorSet;
using QtRocket::ExternalComponentSaver;
using QtRocket::FlightConfigurationId;
using QtRocket::GeneralRocketLoader;
using QtRocket::InMemoryPreferences;
using QtRocket::InternalComponentSaver;
using QtRocket::LoadedDocument;
using QtRocket::MassComponent;
using QtRocket::Material;
using QtRocket::MaterialGroup;
using QtRocket::Motor;
using QtRocket::MotorConfiguration;
using QtRocket::OpenRocketDocument;
using QtRocket::OpenRocketDocumentFactory;
using QtRocket::OpenRocketSaver;
using QtRocket::OrkWriter;
using QtRocket::Result;
using QtRocket::Rocket;
using QtRocket::RocketComponent;
using QtRocket::RocketComponentSaver;
using QtRocket::StorageOptions;
using QtRocket::Uuid;
using QtRocket::WarningSet;
using QtRocket::Test::LoaderEnvironment;

/// What OpenRocket's saver gives for one component of the kitchen sink.
struct JavaElements
{
    std::string_view id;         ///< the component's id
    std::string_view javaClass;  ///< its Java class
    std::string_view lines;      ///< the lines of getElements(), joined with '\n'
};

// BEGIN GENERATED TABLE kitchen_sink
// KitchenSinkProbe.java of the probes of tier 10a part S2 (out/kitchensink.txt), written into
// this file by scripts/make_kitchen_sink_table.py; E1 applied (<mass>0.05</mass>, Java 0.08).
// clang-format off
constexpr std::array<JavaElements, 27> kJavaElements{{
    {.id = "00000000-0000-4000-8000-000000000001", .javaClass = "Rocket", .lines = R"xml(<rocket>
<name>Kitchen &lt;sink&gt; &amp; &#39;all&#39;</name>
<id>00000000-0000-4000-8000-000000000001</id>
<axialoffset method="absolute">0.0</axialoffset>
<position type="absolute">0.0</position>
<comment>First line
second line</comment>
<designer>Designer &amp; Co</designer>
<designtype>kitbash</designtype>
<kitname>Kit &quot;K&quot;</kitname>
<revision>Rev 2</revision>
<motorconfiguration configid="00000000-0000-4000-8000-0000000000a1">
  <name>Flight &amp; &lt;one&gt;</name>
  <stage number="0" active="true"/>
  <stage number="1" active="true"/>
  <stage number="2" active="true"/>
</motorconfiguration>
<motorconfiguration configid="00000000-0000-4000-8000-0000000000b2" default="true">
  <stage number="0" active="true"/>
  <stage number="1" active="false"/>
  <stage number="2" active="false"/>
</motorconfiguration>
<referencetype>custom</referencetype>
<customreference>0.0123</customreference>
</rocket>)xml"},
    {.id = "00000000-0000-4000-8000-000000000002", .javaClass = "AxialStage", .lines = R"xml(<stage>
<name>Sustainer</name>
<id>00000000-0000-4000-8000-000000000002</id>
<overridemass>1.25</overridemass>
<overridesubcomponentsmass>false</overridesubcomponentsmass>
<overridecg>0.4</overridecg>
<overridesubcomponentscg>true</overridesubcomponentscg>
<overridecd>0.55</overridecd>
<overridesubcomponentscd>true</overridesubcomponentscd>
</stage>)xml"},
    {.id = "00000000-0000-4000-8000-000000000003", .javaClass = "NoseCone", .lines = R"xml(<nosecone>
<name>Nose</name>
<id>00000000-0000-4000-8000-000000000003</id>
<appearance>
  <paint red="10" green="20" blue="30" alpha="255"/>
  <shine>0.3</shine>
  <opacityaffectstexture>true</opacityaffectstexture>
  <decal name="decals/a&amp;b.png" rotation="0.25" edgemode="MIRROR">
    <center x="0.5" y="0.5"/>
    <offset x="0.1" y="0.2"/>
    <scale x="1.5" y="2.0"/>
  </decal>
</appearance>
<insideappearance>
  <edgessameasinside>true</edgessameasinside>
  <insidesameasoutside>true</insidesameasoutside>
  <paint red="200" green="100" blue="50" alpha="128"/>
  <shine>0.8</shine>
  <opacityaffectstexture>false</opacityaffectstexture>
</insideappearance>
<color red="1" green="2" blue="3" alpha="4"/>
<linestyle>dashed</linestyle>
<comment>A nose</comment>
<finish>polished</finish>
<material type="bulk" density="170.0" shearModulus="1.0E8" group="Woods">Tricky &lt;wood&gt;</material>
<length>0.1</length>
<thickness>filled</thickness>
<shape>power</shape>
<shapeclipped>false</shapeclipped>
<shapeparameter>0.5</shapeparameter>
<aftradius>0.02</aftradius>
<aftshoulderradius>0.018</aftshoulderradius>
<aftshoulderlength>0.02</aftshoulderlength>
<aftshoulderthickness>0.001</aftshoulderthickness>
<aftshouldercapped>true</aftshouldercapped>
<isflipped>false</isflipped>
</nosecone>)xml"},
    {.id = "00000000-0000-4000-8000-000000000004", .javaClass = "BodyTube", .lines = R"xml(<bodytube>
<name>Body</name>
<id>00000000-0000-4000-8000-000000000004</id>
<finish>normal</finish>
<material type="bulk" density="680.0" group="Woods">Cardboard</material>
<length>0.3</length>
<thickness>0.001</thickness>
<radius>0.025</radius>
<motormount>
  <ignitionevent>burnout</ignitionevent>
  <ignitiondelay>1.5</ignitiondelay>
  <overhang>0.005</overhang>
  <motor configid="00000000-0000-4000-8000-0000000000a1">
    <type>single</type>
    <manufacturer>Estes</manufacturer>
    <digest>digest C6 test</digest>
    <designation>C6</designation>
    <diameter>0.018</diameter>
    <length>0.07</length>
    <delay>none</delay>
    <nozzleexitdiameter>0.012</nozzleexitdiameter>
  </motor>
  <ignitionconfiguration configid="00000000-0000-4000-8000-0000000000a1">
    <ignitionevent>automatic</ignitionevent>
    <ignitiondelay>0.0</ignitiondelay>
  </ignitionconfiguration>
  <motor configid="00000000-0000-4000-8000-0000000000b2">
    <type>hybrid</type>
    <designation>X &amp; &lt;Y&gt;</designation>
    <diameter>0.029</diameter>
    <length>0.1</length>
    <delay>3.0</delay>
  </motor>
  <ignitionconfiguration configid="00000000-0000-4000-8000-0000000000b2">
    <ignitionevent>launch</ignitionevent>
    <ignitiondelay>0.25</ignitiondelay>
  </ignitionconfiguration>
</motormount>
</bodytube>)xml"},
    {.id = "00000000-0000-4000-8000-000000000005", .javaClass = "TrapezoidFinSet", .lines = R"xml(<trapezoidfinset>
<name>Fins</name>
<id>00000000-0000-4000-8000-000000000005</id>
<instancecount>4</instancecount>
<fincount>4</fincount>
<radiusoffset method="surface">0.0</radiusoffset>
<angleoffset method="relative">17.188733853924695</angleoffset>
<rotation>17.188733853924695</rotation>
<axialoffset method="bottom">-0.01</axialoffset>
<position type="bottom">-0.01</position>
<finish>normal</finish>
<material type="bulk" density="680.0" shearModulus="4.0E8" group="PaperProducts">Cardboard</material>
<thickness>0.003</thickness>
<crosssection>airfoil</crosssection>
<cant>2.8647889756541165</cant>
<filletradius>0.002</filletradius>
<filletmaterial type="bulk" density="1100.0" group="Woods">Fillet</filletmaterial>
<rootchord>0.05</rootchord>
<tipchord>0.03</tipchord>
<sweeplength>0.02</sweeplength>
<height>0.04</height>
</trapezoidfinset>)xml"},
    {.id = "00000000-0000-4000-8000-000000000006", .javaClass = "EllipticalFinSet", .lines = R"xml(<ellipticalfinset>
<name>Elliptical</name>
<id>00000000-0000-4000-8000-000000000006</id>
<instancecount>3</instancecount>
<fincount>3</fincount>
<radiusoffset method="surface">0.0</radiusoffset>
<angleoffset method="relative">0.0</angleoffset>
<rotation>0.0</rotation>
<axialoffset method="bottom">0.0</axialoffset>
<position type="bottom">0.0</position>
<finish>normal</finish>
<material type="bulk" density="680.0" shearModulus="4.0E8" group="PaperProducts">Cardboard</material>
<thickness>0.003</thickness>
<crosssection>square</crosssection>
<cant>0.0</cant>
<tabheight>0.005</tabheight>
<tablength>0.02</tablength>
<tabposition relativeto="front">0.004</tabposition>
<tabposition relativeto="top">0.004</tabposition>
<filletradius>0.0</filletradius>
<filletmaterial type="bulk" density="680.0" shearModulus="4.0E8" group="PaperProducts">Cardboard</filletmaterial>
<rootchord>0.05</rootchord>
<height>0.03</height>
</ellipticalfinset>)xml"},
    {.id = "00000000-0000-4000-8000-000000000007", .javaClass = "FreeformFinSet", .lines = R"xml(<freeformfinset>
<name>Freeform</name>
<id>00000000-0000-4000-8000-000000000007</id>
<instancecount>3</instancecount>
<fincount>3</fincount>
<radiusoffset method="surface">0.0</radiusoffset>
<angleoffset method="relative">0.0</angleoffset>
<rotation>0.0</rotation>
<axialoffset method="bottom">0.0</axialoffset>
<position type="bottom">0.0</position>
<finish>normal</finish>
<material type="bulk" density="680.0" shearModulus="4.0E8" group="PaperProducts">Cardboard</material>
<thickness>0.003</thickness>
<crosssection>square</crosssection>
<cant>0.0</cant>
<filletradius>0.0</filletradius>
<filletmaterial type="bulk" density="680.0" shearModulus="4.0E8" group="PaperProducts">Cardboard</filletmaterial>
<finpoints>
  <point x="0.0" y="0.0"/>
  <point x="0.02" y="0.03"/>
  <point x="0.05" y="0.03"/>
  <point x="0.06" y="0.0"/>
</finpoints>
</freeformfinset>)xml"},
    {.id = "00000000-0000-4000-8000-000000000008", .javaClass = "TubeFinSet", .lines = R"xml(<tubefinset>
<name>Tube fins</name>
<id>00000000-0000-4000-8000-000000000008</id>
<instancecount>6</instancecount>
<fincount>6</fincount>
<radiusoffset method="coaxial">0.0</radiusoffset>
<angleoffset method="fixed">5.729577951308232</angleoffset>
<rotation>5.729577951308232</rotation>
<axialoffset method="bottom">0.0</axialoffset>
<position type="bottom">0.0</position>
<finish>normal</finish>
<material type="bulk" density="680.0" shearModulus="4.0E8" group="PaperProducts">Cardboard</material>
<fincount>6</fincount>
<rotation>5.729577951308232</rotation>
<radius>auto</radius>
<length>0.05</length>
<thickness>0.001</thickness>
</tubefinset>)xml"},
    {.id = "00000000-0000-4000-8000-000000000009", .javaClass = "LaunchLug", .lines = R"xml(<launchlug>
<name>Lug</name>
<id>00000000-0000-4000-8000-000000000009</id>
<instancecount>2</instancecount>
<instanceseparation>0.02</instanceseparation>
<angleoffset method="relative">57.29577951308232</angleoffset>
<radialdirection>57.29577951308232</radialdirection>
<axialoffset method="middle">0.0</axialoffset>
<position type="middle">0.0</position>
<finish>normal</finish>
<material type="bulk" density="680.0" shearModulus="4.0E8" group="PaperProducts">Cardboard</material>
<radius>0.003</radius>
<length>0.03</length>
<thickness>5.0E-4</thickness>
</launchlug>)xml"},
    {.id = "00000000-0000-4000-8000-000000000010", .javaClass = "RailButton", .lines = R"xml(<railbutton>
<name>Button</name>
<id>00000000-0000-4000-8000-000000000010</id>
<instancecount>2</instancecount>
<instanceseparation>0.1</instanceseparation>
<angleoffset method="relative">28.64788975654116</angleoffset>
<axialoffset method="middle">0.0</axialoffset>
<position type="middle">0.0</position>
<finish>normal</finish>
<material type="bulk" density="1420.0" shearModulus="9.46E8" group="Plastics">Delrin</material>
<outerdiameter>0.0097</outerdiameter>
<innerdiameter>0.008</innerdiameter>
<height>0.0097</height>
<baseheight>0.002</baseheight>
<flangeheight>0.002</flangeheight>
<screwheight>0.0</screwheight>
</railbutton>)xml"},
    {.id = "00000000-0000-4000-8000-000000000011", .javaClass = "InnerTube", .lines = R"xml(<innertube>
<name>Inner</name>
<id>00000000-0000-4000-8000-000000000011</id>
<axialoffset method="top">0.01</axialoffset>
<position type="top">0.01</position>
<material type="bulk" density="680.0" shearModulus="4.0E8" group="PaperProducts">Cardboard</material>
<length>0.07</length>
<radialposition>0.0</radialposition>
<radialdirection>0.0</radialdirection>
<outerradius>0.009</outerradius>
<thickness>5.000000000000004E-4</thickness>
<clusterconfiguration>3-ring</clusterconfiguration>
<clusterscale>1.2</clusterscale>
<clusterrotation>11.459155902616464</clusterrotation>
<motormount>
  <ignitionevent>automatic</ignitionevent>
  <ignitiondelay>0.0</ignitiondelay>
  <overhang>0.0</overhang>
  <motor configid="00000000-0000-4000-8000-0000000000a1">
    <type>single</type>
    <manufacturer>Estes</manufacturer>
    <digest>digest C6 test</digest>
    <designation>C6</designation>
    <diameter>0.018</diameter>
    <length>0.07</length>
    <delay>5.0</delay>
  </motor>
  <ignitionconfiguration configid="00000000-0000-4000-8000-0000000000a1">
    <ignitionevent>automatic</ignitionevent>
    <ignitiondelay>0.0</ignitiondelay>
  </ignitionconfiguration>
</motormount>
</innertube>)xml"},
    {.id = "00000000-0000-4000-8000-000000000012", .javaClass = "EngineBlock", .lines = R"xml(<engineblock>
<name>Block</name>
<id>00000000-0000-4000-8000-000000000012</id>
<axialoffset method="bottom">0.0</axialoffset>
<position type="bottom">0.0</position>
<material type="bulk" density="680.0" shearModulus="4.0E8" group="PaperProducts">Cardboard</material>
<length>0.005</length>
<radialposition>0.0</radialposition>
<radialdirection>0.0</radialdirection>
<outerradius>auto</outerradius>
<thickness>0.001</thickness>
</engineblock>)xml"},
    {.id = "00000000-0000-4000-8000-000000000013", .javaClass = "TubeCoupler", .lines = R"xml(<tubecoupler>
<name>Coupler</name>
<id>00000000-0000-4000-8000-000000000013</id>
<axialoffset method="bottom">0.0</axialoffset>
<position type="bottom">0.0</position>
<material type="bulk" density="680.0" shearModulus="4.0E8" group="PaperProducts">Cardboard</material>
<length>0.04</length>
<radialposition>0.0</radialposition>
<radialdirection>0.0</radialdirection>
<outerradius>auto</outerradius>
<thickness>0.001</thickness>
</tubecoupler>)xml"},
    {.id = "00000000-0000-4000-8000-000000000014", .javaClass = "CenteringRing", .lines = R"xml(<centeringring>
<name>Rings</name>
<id>00000000-0000-4000-8000-000000000014</id>
<instancecount>2</instancecount>
<instanceseparation>0.05</instanceseparation>
<axialoffset method="bottom">0.0</axialoffset>
<position type="bottom">0.0</position>
<material type="bulk" density="680.0" shearModulus="4.0E8" group="PaperProducts">Cardboard</material>
<length>0.003</length>
<radialposition>0.0</radialposition>
<radialdirection>0.0</radialdirection>
<outerradius>auto</outerradius>
<innerradius>auto</innerradius>
</centeringring>)xml"},
    {.id = "00000000-0000-4000-8000-000000000015", .javaClass = "Bulkhead", .lines = R"xml(<bulkhead>
<name>Bulkhead</name>
<id>00000000-0000-4000-8000-000000000015</id>
<instancecount>1</instancecount>
<instanceseparation>0.0</instanceseparation>
<axialoffset method="bottom">0.0</axialoffset>
<position type="bottom">0.0</position>
<material type="bulk" density="680.0" shearModulus="4.0E8" group="PaperProducts">Cardboard</material>
<length>0.004</length>
<radialposition>0.001</radialposition>
<radialdirection>40.10704565915762</radialdirection>
<outerradius>0.02</outerradius>
</bulkhead>)xml"},
    {.id = "00000000-0000-4000-8000-000000000016", .javaClass = "MassComponent", .lines = R"xml(<masscomponent>
<name>Altimeter</name>
<id>00000000-0000-4000-8000-000000000016</id>
<axialoffset method="top">0.0</axialoffset>
<position type="top">0.0</position>
<overridemass>0.08</overridemass>
<overridesubcomponentsmass>false</overridesubcomponentsmass>
<packedlength>0.03</packedlength>
<packedradius>0.01</packedradius>
<radialposition>0.002</radialposition>
<radialdirection>22.91831180523293</radialdirection>
<mass>0.05</mass>
<masscomponenttype>altimeter</masscomponenttype>
</masscomponent>)xml"},
    {.id = "00000000-0000-4000-8000-000000000017", .javaClass = "ShockCord", .lines = R"xml(<shockcord>
<name>Cord</name>
<id>00000000-0000-4000-8000-000000000017</id>
<axialoffset method="top">0.0</axialoffset>
<position type="top">0.0</position>
<packedlength>0.02</packedlength>
<packedradius>0.008</packedradius>
<radialposition>0.0</radialposition>
<radialdirection>0.0</radialdirection>
<cordlength>0.5</cordlength>
<material type="line" density="0.003" group="Fibers">Kevlar</material>
</shockcord>)xml"},
    {.id = "00000000-0000-4000-8000-000000000018", .javaClass = "Parachute", .lines = R"xml(<parachute>
<name>Chute</name>
<id>00000000-0000-4000-8000-000000000018</id>
<axialoffset method="top">0.0</axialoffset>
<position type="top">0.0</position>
<packedlength>0.025</packedlength>
<packedradius>0.0125</packedradius>
<radialposition>0.0</radialposition>
<radialdirection>0.0</radialdirection>
<cd>0.8</cd>
<material type="surface" density="0.05" group="Fabrics">Rip-stop</material>
<isdrogue>true</isdrogue>
<deployevent>apogee</deployevent>
<deployaltitude>200.0</deployaltitude>
<deploydelay>0.0</deploydelay>
<deploymentconfiguration configid="00000000-0000-4000-8000-0000000000a1">
  <deployevent>altitude</deployevent>
  <deployaltitude>150.0</deployaltitude>
  <deploydelay>1.0</deploydelay>
</deploymentconfiguration>
<diameter>0.3</diameter>
<linecount>8</linecount>
<linelength>auto</linelength>
<linematerial type="line" density="0.0018" group="Elastics">Elastic cord (round 2 mm, 1/16 in)</linematerial>
</parachute>)xml"},
    {.id = "00000000-0000-4000-8000-000000000019", .javaClass = "Streamer", .lines = R"xml(<streamer>
<name>Streamer</name>
<id>00000000-0000-4000-8000-000000000019</id>
<axialoffset method="top">0.0</axialoffset>
<position type="top">0.0</position>
<packedlength>0.025</packedlength>
<packedradius>0.0125</packedradius>
<radialposition>0.0</radialposition>
<radialdirection>0.0</radialdirection>
<cd>auto</cd>
<material type="surface" density="0.067" group="Fabrics">Ripstop nylon</material>
<deployevent>ejection</deployevent>
<deployaltitude>200.0</deployaltitude>
<deploydelay>0.0</deploydelay>
<striplength>0.5</striplength>
<stripwidth>0.05</stripwidth>
</streamer>)xml"},
    {.id = "00000000-0000-4000-8000-000000000020", .javaClass = "Transition", .lines = R"xml(<transition>
<name>Transition</name>
<id>00000000-0000-4000-8000-000000000020</id>
<finish>normal</finish>
<material type="bulk" density="680.0" shearModulus="4.0E8" group="PaperProducts">Cardboard</material>
<length>0.04</length>
<thickness>0.002</thickness>
<shape>ogive</shape>
<shapeparameter>1.0</shapeparameter>
<foreradius>auto 0.025</foreradius>
<aftradius>0.03</aftradius>
<foreshoulderradius>0.02</foreshoulderradius>
<foreshoulderlength>0.01</foreshoulderlength>
<foreshoulderthickness>0.001</foreshoulderthickness>
<foreshouldercapped>true</foreshouldercapped>
<aftshoulderradius>0.0</aftshoulderradius>
<aftshoulderlength>0.0</aftshoulderlength>
<aftshoulderthickness>0.0</aftshoulderthickness>
<aftshouldercapped>false</aftshouldercapped>
</transition>)xml"},
    {.id = "00000000-0000-4000-8000-000000000021", .javaClass = "AxialStage", .lines = R"xml(<stage>
<name>Booster</name>
<id>00000000-0000-4000-8000-000000000021</id>
<separationevent>burnout</separationevent>
<separationaltitude>200.0</separationaltitude>
<separationdelay>0.5</separationdelay>
<separationconfiguration configid="00000000-0000-4000-8000-0000000000b2">
    <separationevent>altitudeascending</separationevent>
    <separationaltitude>300.0</separationaltitude>
    <separationdelay>0.0</separationdelay>
</separationconfiguration>
</stage>)xml"},
    {.id = "00000000-0000-4000-8000-000000000022", .javaClass = "BodyTube", .lines = R"xml(<bodytube>
<name>Booster body</name>
<id>00000000-0000-4000-8000-000000000022</id>
<finish>normal</finish>
<material type="bulk" density="680.0" shearModulus="4.0E8" group="PaperProducts">Cardboard</material>
<length>0.2</length>
<thickness>0.001</thickness>
<radius>auto 0.03</radius>
</bodytube>)xml"},
    {.id = "00000000-0000-4000-8000-000000000023", .javaClass = "PodSet", .lines = R"xml(<podset>
<name>Pods</name>
<id>00000000-0000-4000-8000-000000000023</id>
<instancecount>2</instancecount>
<radiusoffset method="relative">0.05</radiusoffset>
<angleoffset method="relative">14.32394487827058</angleoffset>
<axialoffset method="top">0.02</axialoffset>
<position type="top">0.02</position>
</podset>)xml"},
    {.id = "00000000-0000-4000-8000-000000000024", .javaClass = "BodyTube", .lines = R"xml(<bodytube>
<name>Pod body</name>
<id>00000000-0000-4000-8000-000000000024</id>
<finish>normal</finish>
<material type="bulk" density="680.0" shearModulus="4.0E8" group="PaperProducts">Cardboard</material>
<length>0.1</length>
<thickness>5.0E-4</thickness>
<radius>0.01</radius>
</bodytube>)xml"},
    {.id = "00000000-0000-4000-8000-000000000025", .javaClass = "ParallelStage", .lines = R"xml(<parallelstage>
<name>Boosters</name>
<id>00000000-0000-4000-8000-000000000025</id>
<instancecount>3</instancecount>
<radiusoffset method="relative">0.0</radiusoffset>
<angleoffset method="relative">5.729577951308232</angleoffset>
<axialoffset method="bottom">0.0</axialoffset>
<position type="bottom">0.0</position>
<separationevent>ejection</separationevent>
<separationaltitude>200.0</separationaltitude>
<separationdelay>0.0</separationdelay>
</parallelstage>)xml"},
    {.id = "00000000-0000-4000-8000-000000000026", .javaClass = "NoseCone", .lines = R"xml(<nosecone>
<name>Booster nose</name>
<id>00000000-0000-4000-8000-000000000026</id>
<finish>normal</finish>
<material type="bulk" density="680.0" shearModulus="4.0E8" group="PaperProducts">Cardboard</material>
<length>0.05</length>
<thickness>0.002</thickness>
<shape>conical</shape>
<aftradius>0.01</aftradius>
<aftshoulderradius>0.0</aftshoulderradius>
<aftshoulderlength>0.0</aftshoulderlength>
<aftshoulderthickness>0.0</aftshoulderthickness>
<aftshouldercapped>false</aftshouldercapped>
<isflipped>false</isflipped>
</nosecone>)xml"},
    {.id = "00000000-0000-4000-8000-000000000027", .javaClass = "BodyTube", .lines = R"xml(<bodytube>
<name>Booster tube</name>
<id>00000000-0000-4000-8000-000000000027</id>
<finish>normal</finish>
<material type="bulk" density="680.0" shearModulus="4.0E8" group="PaperProducts">Cardboard</material>
<length>0.15</length>
<thickness>5.0E-4</thickness>
<radius>0.01</radius>
</bodytube>)xml"},
}};

/// The <rocket> element of OpenRocketSaver's save of the kitchen sink's document.
constexpr std::string_view kJavaRocketElement = R"xml(  <rocket>
    <name>Kitchen &lt;sink&gt; &amp; &#39;all&#39;</name>
    <id>00000000-0000-4000-8000-000000000001</id>
    <axialoffset method="absolute">0.0</axialoffset>
    <position type="absolute">0.0</position>
    <comment>First line
second line</comment>
    <designer>Designer &amp; Co</designer>
    <designtype>kitbash</designtype>
    <kitname>Kit &quot;K&quot;</kitname>
    <revision>Rev 2</revision>
    <motorconfiguration configid="00000000-0000-4000-8000-0000000000a1">
      <name>Flight &amp; &lt;one&gt;</name>
      <stage number="0" active="true"/>
      <stage number="1" active="true"/>
      <stage number="2" active="true"/>
    </motorconfiguration>
    <motorconfiguration configid="00000000-0000-4000-8000-0000000000b2" default="true">
      <stage number="0" active="true"/>
      <stage number="1" active="false"/>
      <stage number="2" active="false"/>
    </motorconfiguration>
    <referencetype>custom</referencetype>
    <customreference>0.0123</customreference>

    <subcomponents>
      <stage>
        <name>Sustainer</name>
        <id>00000000-0000-4000-8000-000000000002</id>
        <overridemass>1.25</overridemass>
        <overridesubcomponentsmass>false</overridesubcomponentsmass>
        <overridecg>0.4</overridecg>
        <overridesubcomponentscg>true</overridesubcomponentscg>
        <overridecd>0.55</overridecd>
        <overridesubcomponentscd>true</overridesubcomponentscd>

        <subcomponents>
          <nosecone>
            <name>Nose</name>
            <id>00000000-0000-4000-8000-000000000003</id>
            <appearance>
              <paint red="10" green="20" blue="30" alpha="255"/>
              <shine>0.3</shine>
              <opacityaffectstexture>true</opacityaffectstexture>
              <decal name="decals/a&amp;b.png" rotation="0.25" edgemode="MIRROR">
                <center x="0.5" y="0.5"/>
                <offset x="0.1" y="0.2"/>
                <scale x="1.5" y="2.0"/>
              </decal>
            </appearance>
            <insideappearance>
              <edgessameasinside>true</edgessameasinside>
              <insidesameasoutside>true</insidesameasoutside>
              <paint red="200" green="100" blue="50" alpha="128"/>
              <shine>0.8</shine>
              <opacityaffectstexture>false</opacityaffectstexture>
            </insideappearance>
            <color red="1" green="2" blue="3" alpha="4"/>
            <linestyle>dashed</linestyle>
            <comment>A nose</comment>
            <finish>polished</finish>
            <material type="bulk" density="170.0" shearModulus="1.0E8" group="Woods">Tricky &lt;wood&gt;</material>
            <length>0.1</length>
            <thickness>filled</thickness>
            <shape>power</shape>
            <shapeclipped>false</shapeclipped>
            <shapeparameter>0.5</shapeparameter>
            <aftradius>0.02</aftradius>
            <aftshoulderradius>0.018</aftshoulderradius>
            <aftshoulderlength>0.02</aftshoulderlength>
            <aftshoulderthickness>0.001</aftshoulderthickness>
            <aftshouldercapped>true</aftshouldercapped>
            <isflipped>false</isflipped>
          </nosecone>

          <bodytube>
            <name>Body</name>
            <id>00000000-0000-4000-8000-000000000004</id>
            <finish>normal</finish>
            <material type="bulk" density="680.0" group="Woods">Cardboard</material>
            <length>0.3</length>
            <thickness>0.001</thickness>
            <radius>0.025</radius>
            <motormount>
              <ignitionevent>burnout</ignitionevent>
              <ignitiondelay>1.5</ignitiondelay>
              <overhang>0.005</overhang>
              <motor configid="00000000-0000-4000-8000-0000000000a1">
                <type>single</type>
                <manufacturer>Estes</manufacturer>
                <digest>digest C6 test</digest>
                <designation>C6</designation>
                <diameter>0.018</diameter>
                <length>0.07</length>
                <delay>none</delay>
                <nozzleexitdiameter>0.012</nozzleexitdiameter>
              </motor>
              <ignitionconfiguration configid="00000000-0000-4000-8000-0000000000a1">
                <ignitionevent>automatic</ignitionevent>
                <ignitiondelay>0.0</ignitiondelay>
              </ignitionconfiguration>
              <motor configid="00000000-0000-4000-8000-0000000000b2">
                <type>hybrid</type>
                <designation>X &amp; &lt;Y&gt;</designation>
                <diameter>0.029</diameter>
                <length>0.1</length>
                <delay>3.0</delay>
              </motor>
              <ignitionconfiguration configid="00000000-0000-4000-8000-0000000000b2">
                <ignitionevent>launch</ignitionevent>
                <ignitiondelay>0.25</ignitiondelay>
              </ignitionconfiguration>
            </motormount>

            <subcomponents>
              <trapezoidfinset>
                <name>Fins</name>
                <id>00000000-0000-4000-8000-000000000005</id>
                <instancecount>4</instancecount>
                <fincount>4</fincount>
                <radiusoffset method="surface">0.0</radiusoffset>
                <angleoffset method="relative">17.188733853924695</angleoffset>
                <rotation>17.188733853924695</rotation>
                <axialoffset method="bottom">-0.01</axialoffset>
                <position type="bottom">-0.01</position>
                <finish>normal</finish>
                <material type="bulk" density="680.0" shearModulus="4.0E8" group="PaperProducts">Cardboard</material>
                <thickness>0.003</thickness>
                <crosssection>airfoil</crosssection>
                <cant>2.8647889756541165</cant>
                <filletradius>0.002</filletradius>
                <filletmaterial type="bulk" density="1100.0" group="Woods">Fillet</filletmaterial>
                <rootchord>0.05</rootchord>
                <tipchord>0.03</tipchord>
                <sweeplength>0.02</sweeplength>
                <height>0.04</height>
              </trapezoidfinset>

              <ellipticalfinset>
                <name>Elliptical</name>
                <id>00000000-0000-4000-8000-000000000006</id>
                <instancecount>3</instancecount>
                <fincount>3</fincount>
                <radiusoffset method="surface">0.0</radiusoffset>
                <angleoffset method="relative">0.0</angleoffset>
                <rotation>0.0</rotation>
                <axialoffset method="bottom">0.0</axialoffset>
                <position type="bottom">0.0</position>
                <finish>normal</finish>
                <material type="bulk" density="680.0" shearModulus="4.0E8" group="PaperProducts">Cardboard</material>
                <thickness>0.003</thickness>
                <crosssection>square</crosssection>
                <cant>0.0</cant>
                <tabheight>0.005</tabheight>
                <tablength>0.02</tablength>
                <tabposition relativeto="front">0.004</tabposition>
                <tabposition relativeto="top">0.004</tabposition>
                <filletradius>0.0</filletradius>
                <filletmaterial type="bulk" density="680.0" shearModulus="4.0E8" group="PaperProducts">Cardboard</filletmaterial>
                <rootchord>0.05</rootchord>
                <height>0.03</height>
              </ellipticalfinset>

              <freeformfinset>
                <name>Freeform</name>
                <id>00000000-0000-4000-8000-000000000007</id>
                <instancecount>3</instancecount>
                <fincount>3</fincount>
                <radiusoffset method="surface">0.0</radiusoffset>
                <angleoffset method="relative">0.0</angleoffset>
                <rotation>0.0</rotation>
                <axialoffset method="bottom">0.0</axialoffset>
                <position type="bottom">0.0</position>
                <finish>normal</finish>
                <material type="bulk" density="680.0" shearModulus="4.0E8" group="PaperProducts">Cardboard</material>
                <thickness>0.003</thickness>
                <crosssection>square</crosssection>
                <cant>0.0</cant>
                <filletradius>0.0</filletradius>
                <filletmaterial type="bulk" density="680.0" shearModulus="4.0E8" group="PaperProducts">Cardboard</filletmaterial>
                <finpoints>
                  <point x="0.0" y="0.0"/>
                  <point x="0.02" y="0.03"/>
                  <point x="0.05" y="0.03"/>
                  <point x="0.06" y="0.0"/>
                </finpoints>
              </freeformfinset>

              <tubefinset>
                <name>Tube fins</name>
                <id>00000000-0000-4000-8000-000000000008</id>
                <instancecount>6</instancecount>
                <fincount>6</fincount>
                <radiusoffset method="coaxial">0.0</radiusoffset>
                <angleoffset method="fixed">5.729577951308232</angleoffset>
                <rotation>5.729577951308232</rotation>
                <axialoffset method="bottom">0.0</axialoffset>
                <position type="bottom">0.0</position>
                <finish>normal</finish>
                <material type="bulk" density="680.0" shearModulus="4.0E8" group="PaperProducts">Cardboard</material>
                <fincount>6</fincount>
                <rotation>5.729577951308232</rotation>
                <radius>auto</radius>
                <length>0.05</length>
                <thickness>0.001</thickness>
              </tubefinset>

              <launchlug>
                <name>Lug</name>
                <id>00000000-0000-4000-8000-000000000009</id>
                <instancecount>2</instancecount>
                <instanceseparation>0.02</instanceseparation>
                <angleoffset method="relative">57.29577951308232</angleoffset>
                <radialdirection>57.29577951308232</radialdirection>
                <axialoffset method="middle">0.0</axialoffset>
                <position type="middle">0.0</position>
                <finish>normal</finish>
                <material type="bulk" density="680.0" shearModulus="4.0E8" group="PaperProducts">Cardboard</material>
                <radius>0.003</radius>
                <length>0.03</length>
                <thickness>5.0E-4</thickness>
              </launchlug>

              <railbutton>
                <name>Button</name>
                <id>00000000-0000-4000-8000-000000000010</id>
                <instancecount>2</instancecount>
                <instanceseparation>0.1</instanceseparation>
                <angleoffset method="relative">28.64788975654116</angleoffset>
                <axialoffset method="middle">0.0</axialoffset>
                <position type="middle">0.0</position>
                <finish>normal</finish>
                <material type="bulk" density="1420.0" shearModulus="9.46E8" group="Plastics">Delrin</material>
                <outerdiameter>0.0097</outerdiameter>
                <innerdiameter>0.008</innerdiameter>
                <height>0.0097</height>
                <baseheight>0.002</baseheight>
                <flangeheight>0.002</flangeheight>
                <screwheight>0.0</screwheight>
              </railbutton>

              <innertube>
                <name>Inner</name>
                <id>00000000-0000-4000-8000-000000000011</id>
                <axialoffset method="top">0.01</axialoffset>
                <position type="top">0.01</position>
                <material type="bulk" density="680.0" shearModulus="4.0E8" group="PaperProducts">Cardboard</material>
                <length>0.07</length>
                <radialposition>0.0</radialposition>
                <radialdirection>0.0</radialdirection>
                <outerradius>0.009</outerradius>
                <thickness>5.000000000000004E-4</thickness>
                <clusterconfiguration>3-ring</clusterconfiguration>
                <clusterscale>1.2</clusterscale>
                <clusterrotation>11.459155902616464</clusterrotation>
                <motormount>
                  <ignitionevent>automatic</ignitionevent>
                  <ignitiondelay>0.0</ignitiondelay>
                  <overhang>0.0</overhang>
                  <motor configid="00000000-0000-4000-8000-0000000000a1">
                    <type>single</type>
                    <manufacturer>Estes</manufacturer>
                    <digest>digest C6 test</digest>
                    <designation>C6</designation>
                    <diameter>0.018</diameter>
                    <length>0.07</length>
                    <delay>5.0</delay>
                  </motor>
                  <ignitionconfiguration configid="00000000-0000-4000-8000-0000000000a1">
                    <ignitionevent>automatic</ignitionevent>
                    <ignitiondelay>0.0</ignitiondelay>
                  </ignitionconfiguration>
                </motormount>

                <subcomponents>
                  <engineblock>
                    <name>Block</name>
                    <id>00000000-0000-4000-8000-000000000012</id>
                    <axialoffset method="bottom">0.0</axialoffset>
                    <position type="bottom">0.0</position>
                    <material type="bulk" density="680.0" shearModulus="4.0E8" group="PaperProducts">Cardboard</material>
                    <length>0.005</length>
                    <radialposition>0.0</radialposition>
                    <radialdirection>0.0</radialdirection>
                    <outerradius>auto</outerradius>
                    <thickness>0.001</thickness>
                  </engineblock>
                </subcomponents>
              </innertube>

              <tubecoupler>
                <name>Coupler</name>
                <id>00000000-0000-4000-8000-000000000013</id>
                <axialoffset method="bottom">0.0</axialoffset>
                <position type="bottom">0.0</position>
                <material type="bulk" density="680.0" shearModulus="4.0E8" group="PaperProducts">Cardboard</material>
                <length>0.04</length>
                <radialposition>0.0</radialposition>
                <radialdirection>0.0</radialdirection>
                <outerradius>auto</outerradius>
                <thickness>0.001</thickness>
              </tubecoupler>

              <centeringring>
                <name>Rings</name>
                <id>00000000-0000-4000-8000-000000000014</id>
                <instancecount>2</instancecount>
                <instanceseparation>0.05</instanceseparation>
                <axialoffset method="bottom">0.0</axialoffset>
                <position type="bottom">0.0</position>
                <material type="bulk" density="680.0" shearModulus="4.0E8" group="PaperProducts">Cardboard</material>
                <length>0.003</length>
                <radialposition>0.0</radialposition>
                <radialdirection>0.0</radialdirection>
                <outerradius>auto</outerradius>
                <innerradius>auto</innerradius>
              </centeringring>

              <bulkhead>
                <name>Bulkhead</name>
                <id>00000000-0000-4000-8000-000000000015</id>
                <instancecount>1</instancecount>
                <instanceseparation>0.0</instanceseparation>
                <axialoffset method="bottom">0.0</axialoffset>
                <position type="bottom">0.0</position>
                <material type="bulk" density="680.0" shearModulus="4.0E8" group="PaperProducts">Cardboard</material>
                <length>0.004</length>
                <radialposition>0.001</radialposition>
                <radialdirection>40.10704565915762</radialdirection>
                <outerradius>0.02</outerradius>
              </bulkhead>

              <masscomponent>
                <name>Altimeter</name>
                <id>00000000-0000-4000-8000-000000000016</id>
                <axialoffset method="top">0.0</axialoffset>
                <position type="top">0.0</position>
                <overridemass>0.08</overridemass>
                <overridesubcomponentsmass>false</overridesubcomponentsmass>
                <packedlength>0.03</packedlength>
                <packedradius>0.01</packedradius>
                <radialposition>0.002</radialposition>
                <radialdirection>22.91831180523293</radialdirection>
                <mass>0.05</mass>
                <masscomponenttype>altimeter</masscomponenttype>
              </masscomponent>

              <shockcord>
                <name>Cord</name>
                <id>00000000-0000-4000-8000-000000000017</id>
                <axialoffset method="top">0.0</axialoffset>
                <position type="top">0.0</position>
                <packedlength>0.02</packedlength>
                <packedradius>0.008</packedradius>
                <radialposition>0.0</radialposition>
                <radialdirection>0.0</radialdirection>
                <cordlength>0.5</cordlength>
                <material type="line" density="0.003" group="Fibers">Kevlar</material>
              </shockcord>

              <parachute>
                <name>Chute</name>
                <id>00000000-0000-4000-8000-000000000018</id>
                <axialoffset method="top">0.0</axialoffset>
                <position type="top">0.0</position>
                <packedlength>0.025</packedlength>
                <packedradius>0.0125</packedradius>
                <radialposition>0.0</radialposition>
                <radialdirection>0.0</radialdirection>
                <cd>0.8</cd>
                <material type="surface" density="0.05" group="Fabrics">Rip-stop</material>
                <isdrogue>true</isdrogue>
                <deployevent>apogee</deployevent>
                <deployaltitude>200.0</deployaltitude>
                <deploydelay>0.0</deploydelay>
                <deploymentconfiguration configid="00000000-0000-4000-8000-0000000000a1">
                  <deployevent>altitude</deployevent>
                  <deployaltitude>150.0</deployaltitude>
                  <deploydelay>1.0</deploydelay>
                </deploymentconfiguration>
                <diameter>0.3</diameter>
                <linecount>8</linecount>
                <linelength>auto</linelength>
                <linematerial type="line" density="0.0018" group="Elastics">Elastic cord (round 2 mm, 1/16 in)</linematerial>
              </parachute>

              <streamer>
                <name>Streamer</name>
                <id>00000000-0000-4000-8000-000000000019</id>
                <axialoffset method="top">0.0</axialoffset>
                <position type="top">0.0</position>
                <packedlength>0.025</packedlength>
                <packedradius>0.0125</packedradius>
                <radialposition>0.0</radialposition>
                <radialdirection>0.0</radialdirection>
                <cd>auto</cd>
                <material type="surface" density="0.067" group="Fabrics">Ripstop nylon</material>
                <deployevent>ejection</deployevent>
                <deployaltitude>200.0</deployaltitude>
                <deploydelay>0.0</deploydelay>
                <striplength>0.5</striplength>
                <stripwidth>0.05</stripwidth>
              </streamer>
            </subcomponents>
          </bodytube>

          <transition>
            <name>Transition</name>
            <id>00000000-0000-4000-8000-000000000020</id>
            <finish>normal</finish>
            <material type="bulk" density="680.0" shearModulus="4.0E8" group="PaperProducts">Cardboard</material>
            <length>0.04</length>
            <thickness>0.002</thickness>
            <shape>ogive</shape>
            <shapeparameter>1.0</shapeparameter>
            <foreradius>auto 0.025</foreradius>
            <aftradius>0.03</aftradius>
            <foreshoulderradius>0.02</foreshoulderradius>
            <foreshoulderlength>0.01</foreshoulderlength>
            <foreshoulderthickness>0.001</foreshoulderthickness>
            <foreshouldercapped>true</foreshouldercapped>
            <aftshoulderradius>0.0</aftshoulderradius>
            <aftshoulderlength>0.0</aftshoulderlength>
            <aftshoulderthickness>0.0</aftshoulderthickness>
            <aftshouldercapped>false</aftshouldercapped>
          </transition>
        </subcomponents>
      </stage>

      <stage>
        <name>Booster</name>
        <id>00000000-0000-4000-8000-000000000021</id>
        <separationevent>burnout</separationevent>
        <separationaltitude>200.0</separationaltitude>
        <separationdelay>0.5</separationdelay>
        <separationconfiguration configid="00000000-0000-4000-8000-0000000000b2">
            <separationevent>altitudeascending</separationevent>
            <separationaltitude>300.0</separationaltitude>
            <separationdelay>0.0</separationdelay>
        </separationconfiguration>

        <subcomponents>
          <bodytube>
            <name>Booster body</name>
            <id>00000000-0000-4000-8000-000000000022</id>
            <finish>normal</finish>
            <material type="bulk" density="680.0" shearModulus="4.0E8" group="PaperProducts">Cardboard</material>
            <length>0.2</length>
            <thickness>0.001</thickness>
            <radius>auto 0.03</radius>

            <subcomponents>
              <podset>
                <name>Pods</name>
                <id>00000000-0000-4000-8000-000000000023</id>
                <instancecount>2</instancecount>
                <radiusoffset method="relative">0.05</radiusoffset>
                <angleoffset method="relative">14.32394487827058</angleoffset>
                <axialoffset method="top">0.02</axialoffset>
                <position type="top">0.02</position>

                <subcomponents>
                  <bodytube>
                    <name>Pod body</name>
                    <id>00000000-0000-4000-8000-000000000024</id>
                    <finish>normal</finish>
                    <material type="bulk" density="680.0" shearModulus="4.0E8" group="PaperProducts">Cardboard</material>
                    <length>0.1</length>
                    <thickness>5.0E-4</thickness>
                    <radius>0.01</radius>
                  </bodytube>
                </subcomponents>
              </podset>

              <parallelstage>
                <name>Boosters</name>
                <id>00000000-0000-4000-8000-000000000025</id>
                <instancecount>3</instancecount>
                <radiusoffset method="relative">0.0</radiusoffset>
                <angleoffset method="relative">5.729577951308232</angleoffset>
                <axialoffset method="bottom">0.0</axialoffset>
                <position type="bottom">0.0</position>
                <separationevent>ejection</separationevent>
                <separationaltitude>200.0</separationaltitude>
                <separationdelay>0.0</separationdelay>

                <subcomponents>
                  <nosecone>
                    <name>Booster nose</name>
                    <id>00000000-0000-4000-8000-000000000026</id>
                    <finish>normal</finish>
                    <material type="bulk" density="680.0" shearModulus="4.0E8" group="PaperProducts">Cardboard</material>
                    <length>0.05</length>
                    <thickness>0.002</thickness>
                    <shape>conical</shape>
                    <aftradius>0.01</aftradius>
                    <aftshoulderradius>0.0</aftshoulderradius>
                    <aftshoulderlength>0.0</aftshoulderlength>
                    <aftshoulderthickness>0.0</aftshoulderthickness>
                    <aftshouldercapped>false</aftshouldercapped>
                    <isflipped>false</isflipped>
                  </nosecone>

                  <bodytube>
                    <name>Booster tube</name>
                    <id>00000000-0000-4000-8000-000000000027</id>
                    <finish>normal</finish>
                    <material type="bulk" density="680.0" shearModulus="4.0E8" group="PaperProducts">Cardboard</material>
                    <length>0.15</length>
                    <thickness>5.0E-4</thickness>
                    <radius>0.01</radius>
                  </bodytube>
                </subcomponents>
              </parallelstage>
            </subcomponents>
          </bodytube>
        </subcomponents>
      </stage>
    </subcomponents>
  </rocket>
)xml";
// clang-format on
// END GENERATED TABLE kitchen_sink

/// A motor that is not a ThrustCurveMotor (KitchenSinkProbe.OtherMotor).
class OtherMotor final : public Motor
{
public:
    [[nodiscard]] Type               getMotorType() const override { return Type::HYBRID; }
    [[nodiscard]] const std::string& getCode() const override { return m_code; }
    [[nodiscard]] const std::string& getCommonName() const override { return m_code; }
    [[nodiscard]] std::string getCommonName(double /*delay*/) const override { return m_code; }
    [[nodiscard]] const std::string& getDesignation() const override { return m_designation; }
    [[nodiscard]] std::string        getDesignation(double /*delay*/) const override
    {
        return m_designation;
    }
    [[nodiscard]] const std::string& getDescription() const override { return m_empty; }
    [[nodiscard]] double             getDiameter() const override { return 0.029; }
    [[nodiscard]] double             getLength() const override { return 0.1; }
    [[nodiscard]] const std::string& getDigest() const override { return m_digest; }
    [[nodiscard]] double             getLaunchCGx() const override { return 0; }
    [[nodiscard]] double             getBurnoutCGx() const override { return 0; }
    [[nodiscard]] double             getLaunchMass() const override { return 0; }
    [[nodiscard]] double             getBurnoutMass() const override { return 0; }
    [[nodiscard]] double             getBurnTimeEstimate() const override { return 0; }
    [[nodiscard]] double             getAverageThrustEstimate() const override { return 0; }
    [[nodiscard]] double             getMaxThrustEstimate() const override { return 0; }
    [[nodiscard]] double             getTotalImpulseEstimate() const override { return 0; }
    [[nodiscard]] double             getBurnTime() const override { return 0; }
    [[nodiscard]] double             getThrust(double /*motorTime*/) const override { return 0; }
    [[nodiscard]] double             getTotalMass(double /*motorTime*/) const override { return 0; }
    [[nodiscard]] double getPropellantMass(double /*motorTime*/) const override { return 0; }
    [[nodiscard]] double getCMx(double /*motorTime*/) const override { return 0; }
    [[nodiscard]] double getUnitIxx() const override { return 0; }
    [[nodiscard]] double getUnitIyy() const override { return 0; }
    [[nodiscard]] double getUnitIzz() const override { return 0; }

private:
    std::string m_code{"X"};
    std::string m_designation{"X & <Y>"};
    std::string m_empty;
    std::string m_digest{"other-digest"};
};

/// KitchenSinkProbe.id(): the id 00000000-0000-4000-8000-<n in 12 digits>.
[[nodiscard]] Uuid idOf(int n)
{
    return Uuid::javaFromString(std::format("00000000-0000-4000-8000-{:012}", n)).value();
}

/// KitchenSinkProbe.named(): @p component with the id idOf(@p n) and @p name.
template <typename T>
T& named(T& component, int n, std::string_view name)
{
    component.setId(idOf(n));
    component.setName(name);
    return component;
}

/// KitchenSinkProbe.bulk(): a bulk material of the group WOODS, not user-defined.
[[nodiscard]] Material bulk(std::string_view name, double density)
{
    return Material::newMaterial(Material::Type::BULK, std::string(name), density,
                                 MaterialGroup::WOODS, false);
}

/// KitchenSinkProbe.FCID_A and FCID_B.
[[nodiscard]] FlightConfigurationId fcidA()
{
    return FlightConfigurationId::fromString("00000000-0000-4000-8000-0000000000a1");
}
[[nodiscard]] FlightConfigurationId fcidB()
{
    return FlightConfigurationId::fromString("00000000-0000-4000-8000-0000000000b2");
}

/// KitchenSinkProbe.kitchenSink() up to the body tube's motors: the rocket, the sustainer and
/// its nose cone.
void addRocketAndNose(Rocket& rocket, QtRocket::AxialStage& sustainer)
{
    named(rocket, 1, "Kitchen <sink> & 'all'");
    rocket.setComment("First line\nsecond line");
    rocket.setDesigner("Designer & Co");
    rocket.setKitName("Kit \"K\"");
    rocket.setRevision("Rev 2");
    rocket.setDesignType(QtRocket::DesignType::KIT_BASH);
    rocket.setReferenceType(QtRocket::ReferenceType::CUSTOM);
    rocket.setCustomReferenceLength(0.0123);

    named(sustainer, 2, "Sustainer");
    sustainer.setMassOverridden(true);
    sustainer.setOverrideMass(1.25);
    sustainer.setSubcomponentsOverriddenMass(false);
    sustainer.setCGOverridden(true);
    sustainer.setOverrideCGX(0.4);
    sustainer.setSubcomponentsOverriddenCG(true);
    sustainer.setCDOverridden(true);
    sustainer.setOverrideCD(0.55);
    sustainer.setSubcomponentsOverriddenCD(true);

    auto& nose = named(sustainer.addChild(std::make_unique<QtRocket::NoseCone>(
                           QtRocket::TransitionShape::POWER, 0.1, 0.02)),
                       3, "Nose");
    nose.setShapeParameter(0.5);
    nose.setClipped(false);
    nose.setFilled(true);
    nose.setShoulderRadius(0.018);
    nose.setAftShoulderLength(0.02);
    nose.setAftShoulderThickness(0.001);
    nose.setAftShoulderCapped(true);
    nose.setColor(QtRocket::Color(1, 2, 3, 4));
    nose.setLineStyle(QtRocket::LineStyle::DASHED);
    nose.setFinish(QtRocket::Finish::POLISHED);
    nose.setMaterial(Material::newMaterial(Material::Type::BULK, "Tricky <wood>", 170.0, 1.0E8,
                                           MaterialGroup::WOODS, true));
    nose.setAppearance(QtRocket::Appearance(
        QtRocket::Color(10, 20, 30, 255), 0.3,
        QtRocket::Decal(Coordinate(0.1, 0.2), Coordinate(0.5, 0.5), Coordinate(1.5, 2.0), 0.25,
                        "decals/a&b.png", QtRocket::Decal::EdgeMode::MIRROR),
        true));
    nose.getInsideColorComponentHandler().setInsideAppearance(
        QtRocket::Appearance(QtRocket::Color(200, 100, 50, 128), 0.8));
    nose.getInsideColorComponentHandler().setEdgesSameAsInside(true);
    nose.getInsideColorComponentHandler().setSeparateInsideOutside(true);
    nose.setComment("A nose");
}

/// The body tube of the sustainer with its two motors.
BodyTube& addMotorMount(Rocket& rocket, QtRocket::AxialStage& sustainer)
{
    auto& body =
        named(sustainer.addChild(std::make_unique<BodyTube>(0.3, 0.025, 0.001)), 4, "Body");
    body.setOuterRadiusAutomatic(false);
    body.setMaterial(bulk("Cardboard", 680.0));
    body.setMotorMount(true);
    body.setMotorOverhang(0.005);
    body.getDefaultMotorConfig().setIgnitionEvent(QtRocket::IgnitionEvent::BURNOUT);
    body.getDefaultMotorConfig().setIgnitionDelay(1.5);

    rocket.createFlightConfiguration(fcidA());
    rocket.createFlightConfiguration(fcidB());
    rocket.getFlightConfiguration(fcidA()).setName("Flight & <one>");
    rocket.setSelectedConfiguration(fcidB());

    MotorConfiguration plugged(body, fcidA());
    plugged.setMotor(QtRocket::Test::motorC6());
    plugged.setEjectionDelay(Motor::kPluggedDelay);
    EXPECT_TRUE(plugged.setNozzleExitDiameter(0.012).has_value());
    body.setMotorConfig(plugged, fcidA());
    MotorConfiguration other(body, fcidB());
    other.setMotor(std::make_shared<const OtherMotor>());
    other.setEjectionDelay(3.0);
    other.setIgnitionEvent(QtRocket::IgnitionEvent::LAUNCH);
    other.setIgnitionDelay(0.25);
    body.setMotorConfig(other, fcidB());
    return body;
}

/// The fin sets, the tube fins, the lug and the rail button of the body tube.
void addFinsAndRails(BodyTube& body)
{
    auto& fins =
        named(body.addChild(std::make_unique<QtRocket::TrapezoidFinSet>(4, 0.05, 0.03, 0.02, 0.04)),
              5, "Fins");
    fins.setThickness(0.003);
    fins.setCrossSection(QtRocket::FinCrossSection::AIRFOIL);
    fins.setCantAngle(0.05);
    fins.setTabHeight(0.01);
    fins.setTabLength(0.0);
    fins.setFilletRadius(0.002);
    fins.setFilletMaterial(bulk("Fillet", 1100.0));
    fins.setAngleOffset(0.3);
    fins.setAxialMethod(AxialMethod::BOTTOM);
    fins.setAxialOffset(-0.01);

    auto& ell =
        named(body.addChild(std::make_unique<QtRocket::EllipticalFinSet>()), 6, "Elliptical");
    ell.setFinCount(3);
    ell.setLength(0.05);
    ell.setHeight(0.03);
    ell.setTabHeight(0.005);
    ell.setTabLength(0.02);
    ell.setTabOffsetMethod(AxialMethod::TOP);
    ell.setTabOffset(0.004);

    auto& free = named(body.addChild(std::make_unique<QtRocket::FreeformFinSet>()), 7, "Freeform");
    const std::array<Coordinate, 4> points{Coordinate(0, 0), Coordinate(0.02, 0.03),
                                           Coordinate(0.05, 0.03), Coordinate(0.06, 0)};
    free.setPoints(points);
    free.setTabHeight(0.0);
    free.setTabLength(0.02);

    auto& tubes = named(body.addChild(std::make_unique<QtRocket::TubeFinSet>()), 8, "Tube fins");
    tubes.setFinCount(6);
    tubes.setLength(0.05);
    tubes.setOuterRadiusAutomatic(true);
    tubes.setThickness(0.001);
    tubes.setBaseRotation(0.1);

    auto& lug = named(body.addChild(std::make_unique<QtRocket::LaunchLug>()), 9, "Lug");
    lug.setInstanceCount(2);
    lug.setInstanceSeparation(0.02);
    lug.setOuterRadius(0.003);
    lug.setThickness(0.0005);
    lug.setLength(0.03);
    lug.setAngleOffset(1.0);

    auto& button = named(body.addChild(std::make_unique<QtRocket::RailButton>()), 10, "Button");
    button.setInstanceCount(2);
    button.setInstanceSeparation(0.1);
    button.setAngleOffset(0.5);
}

/// The inner tube with its motor and engine block, and the rings of the body tube.
void addInnerTubeAndRings(BodyTube& body)
{
    auto& inner = named(body.addChild(std::make_unique<QtRocket::InnerTube>()), 11, "Inner");
    inner.setClusterConfiguration(*QtRocket::ClusterConfiguration::fromXmlName("3-ring"));
    inner.setClusterScale(1.2);
    inner.setClusterRotation(0.2);
    inner.setOuterRadius(0.009);
    inner.setThickness(0.0005);
    inner.setLength(0.07);
    inner.setAxialMethod(AxialMethod::TOP);
    inner.setAxialOffset(0.01);
    inner.setMotorMount(true);
    MotorConfiguration innerMotor(inner, fcidA());
    innerMotor.setMotor(QtRocket::Test::motorC6());
    innerMotor.setEjectionDelay(5.0);
    inner.setMotorConfig(innerMotor, fcidA());

    auto& block = named(inner.addChild(std::make_unique<QtRocket::EngineBlock>()), 12, "Block");
    block.setOuterRadiusAutomatic(true);
    block.setThickness(0.001);
    block.setLength(0.005);

    auto& coupler = named(body.addChild(std::make_unique<QtRocket::TubeCoupler>()), 13, "Coupler");
    coupler.setOuterRadiusAutomatic(true);
    coupler.setThickness(0.001);
    coupler.setLength(0.04);

    auto& ring = named(body.addChild(std::make_unique<QtRocket::CenteringRing>()), 14, "Rings");
    ring.setOuterRadiusAutomatic(true);
    ring.setInnerRadiusAutomatic(true);
    ring.setLength(0.003);
    ring.setInstanceCount(2);
    ring.setInstanceSeparation(0.05);

    auto& bulkhead = named(body.addChild(std::make_unique<QtRocket::Bulkhead>()), 15, "Bulkhead");
    bulkhead.setOuterRadius(0.02);
    bulkhead.setLength(0.004);
    bulkhead.setRadialPosition(0.001);
    bulkhead.setRadialDirection(0.7);
}

/// The mass component, the shock cord and the recovery devices of the body tube.
void addMassObjects(BodyTube& body)
{
    auto& mass =
        named(body.addChild(std::make_unique<MassComponent>(0.03, 0.01, 0.05)), 16, "Altimeter");
    mass.setMassOverridden(true);
    mass.setOverrideMass(0.08);
    mass.setMassComponentType(MassComponent::MassComponentType::ALTIMETER);
    mass.setRadialPosition(0.002);
    mass.setRadialDirection(0.4);

    auto& cord = named(body.addChild(std::make_unique<QtRocket::ShockCord>()), 17, "Cord");
    cord.setCordLength(0.5);
    cord.setMaterial(
        Material::newMaterial(Material::Type::LINE, "Kevlar", 0.003, MaterialGroup::FIBERS, false));
    cord.setRadiusAutomatic(false);
    cord.setRadius(0.008);
    cord.setLength(0.02);

    auto& chute = named(body.addChild(std::make_unique<QtRocket::Parachute>()), 18, "Chute");
    chute.setDiameter(0.3);
    chute.setLineCount(8);
    chute.setLineLengthAutomatic(true);
    chute.setCD(0.8);
    chute.setDrogue(true);
    chute.setMaterial(Material::newMaterial(Material::Type::SURFACE, "Rip-stop", 0.05,
                                            MaterialGroup::FABRICS, false));
    chute.getDeploymentConfigurations().getDefault().setDeployEvent(
        QtRocket::DeploymentConfiguration::DeployEvent::APOGEE);
    QtRocket::DeploymentConfiguration deploy;
    deploy.setDeployEvent(QtRocket::DeploymentConfiguration::DeployEvent::ALTITUDE);
    deploy.setDeployAltitude(150.0);
    deploy.setDeployDelay(1.0);
    chute.getDeploymentConfigurations().set(fcidA(), deploy);

    auto& streamer = named(body.addChild(std::make_unique<QtRocket::Streamer>()), 19, "Streamer");
    streamer.setStripLength(0.5);
    streamer.setStripWidth(0.05);
}

/// The transition of the sustainer and the booster stage with its pods and boosters.
void addTransitionAndBooster(Rocket& rocket, QtRocket::AxialStage& sustainer)
{
    auto& trans =
        named(sustainer.addChild(std::make_unique<QtRocket::Transition>()), 20, "Transition");
    trans.setShapeType(QtRocket::TransitionShape::OGIVE);
    trans.setForeRadiusAutomatic(true);
    trans.setAftRadius(0.03);
    trans.setLength(0.04);
    trans.setThickness(0.002);
    trans.setForeShoulderRadius(0.02);
    trans.setForeShoulderLength(0.01);
    trans.setForeShoulderThickness(0.001);
    trans.setForeShoulderCapped(true);

    auto& booster = named(rocket.addChild(std::make_unique<QtRocket::AxialStage>()), 21, "Booster");
    using Separation = QtRocket::StageSeparationConfiguration;
    booster.getSeparationConfigurations().getDefault().setSeparationEvent(
        Separation::SeparationEvent::BURNOUT);
    booster.getSeparationConfigurations().getDefault().setSeparationDelay(0.5);
    Separation separation;
    separation.setSeparationEvent(Separation::SeparationEvent::ALTITUDE_ASCENDING);
    separation.setSeparationAltitude(300.0);
    booster.getSeparationConfigurations().set(fcidB(), separation);

    auto& boosterBody =
        named(booster.addChild(std::make_unique<BodyTube>(0.2, 0.03, 0.001)), 22, "Booster body");
    boosterBody.setOuterRadiusAutomatic(true);

    auto& pods = named(boosterBody.addChild(std::make_unique<QtRocket::PodSet>()), 23, "Pods");
    pods.setInstanceCount(2);
    pods.setRadiusMethod(QtRocket::RadiusMethod::RELATIVE);
    pods.setRadiusOffset(0.05);
    pods.setAngleMethod(QtRocket::AngleMethod::FIXED);
    pods.setAngleOffset(0.25);
    pods.setAxialMethod(AxialMethod::TOP);
    pods.setAxialOffset(0.02);
    named(pods.addChild(std::make_unique<BodyTube>(0.1, 0.01, 0.0005)), 24, "Pod body");

    auto& boosters =
        named(boosterBody.addChild(std::make_unique<QtRocket::ParallelStage>()), 25, "Boosters");
    boosters.setInstanceCount(3);
    boosters.setAngleOffset(0.1);
    named(boosters.addChild(
              std::make_unique<QtRocket::NoseCone>(QtRocket::TransitionShape::CONICAL, 0.05, 0.01)),
          26, "Booster nose");
    named(boosters.addChild(std::make_unique<BodyTube>(0.15, 0.01, 0.0005)), 27, "Booster tube");
}

/// KitchenSinkProbe.kitchenSink() in the document of OpenRocketDocumentFactory::
/// createDocumentFromRocket(), with the events of the rocket switched on and then the second
/// stage made inactive in the second configuration, as the probe does.
[[nodiscard]] std::unique_ptr<OpenRocketDocument> kitchenSink()
{
    auto    rocket    = std::make_unique<Rocket>();
    Rocket& sink      = *rocket;
    auto&   sustainer = sink.addChild(std::make_unique<QtRocket::AxialStage>());
    addRocketAndNose(sink, sustainer);
    BodyTube& body = addMotorMount(sink, sustainer);
    addFinsAndRails(body);
    addInnerTubeAndRings(body);
    addMassObjects(body);
    addTransitionAndBooster(sink, sustainer);

    std::unique_ptr<OpenRocketDocument> document =
        OpenRocketDocumentFactory::createDocumentFromRocket(std::move(rocket));
    document->getRocket().enableEvents();
    document->getRocket().getFlightConfiguration(fcidB()).setStageActive(1, false);
    return document;
}

/// @p elements joined with '\n'.
[[nodiscard]] std::string joined(const std::vector<std::string>& elements)
{
    std::string text;
    for (const std::string& line : elements)
    {
        text += text.empty() ? "" : "\n";
        text += line;
    }
    return text;
}

/// The components of @p rocket in the order of the tree, the rocket first.
[[nodiscard]] std::vector<const RocketComponent*> componentsOf(const Rocket& rocket)
{
    std::vector<const RocketComponent*> components{&rocket};
    for (const RocketComponent* component : rocket.getAllChildren())
    {
        components.push_back(component);
    }
    return components;
}

/// What the savers give for the components of @p document that differs from kJavaElements,
/// component by component; the kinds met are added to @p kinds.
[[nodiscard]] std::string differencesFromJava(const OpenRocketDocument& document,
                                              std::set<ComponentKind>&  kinds)
{
    const InMemoryPreferences                 preferences;
    OrkWriter                                 writer;
    const std::vector<const RocketComponent*> components = componentsOf(document.getRocket());
    if (components.size() != kJavaElements.size())
    {
        return std::format("{} components, Java's rocket has {}\n", components.size(),
                           kJavaElements.size());
    }
    std::string differences;
    for (std::size_t i = 0; i < components.size(); ++i)
    {
        const RocketComponent& component = *components[i];
        kinds.insert(component.kind());
        const std::string ours =
            joined(ComponentSaver::getElements(component, writer, preferences));
        if (component.getId().toString() != kJavaElements.at(i).id ||
            ours != kJavaElements.at(i).lines)
        {
            differences +=
                std::format("--- {} {}\n{}\n--- OpenRocket\n{}\n", kJavaElements.at(i).id,
                            kJavaElements.at(i).javaClass, ours, kJavaElements.at(i).lines);
        }
    }
    return differences;
}

// Every component of the kitchen sink, one at least of each of the 22 kinds, gives the lines
// OpenRocket's saver gives: the element names, their order, the texts escaped, the numbers as
// Double.toString() prints them, the angles in degrees (the cant by Math.toDegrees()), the
// special values ("auto", "auto <radius>", "filled", "none"), the indentation the savers give
// their own lines. One line is QtRocket's own: the mass component writes its own mass, 0.05,
// where OpenRocket writes the override, 0.08 (decision E1 of 2026-10-14).
TEST(ComponentSaver, TheKitchenSinkIsSavedComponentByComponentAsOpenRocketSavesIt)
{
    const std::unique_ptr<OpenRocketDocument> document = kitchenSink();
    std::set<ComponentKind>                   kinds;
    EXPECT_EQ(differencesFromJava(*document, kinds), "");
    EXPECT_EQ(kinds.size(), QtRocket::kAllComponentKinds.size());
}

/// The <rocket> element of the .ork text @p text, from "  <rocket>" to "  </rocket>".
[[nodiscard]] std::string rocketElementOf(const std::string& text)
{
    const std::size_t open  = text.find("\n  <rocket>\n");
    const std::size_t close = text.find("\n  </rocket>\n");
    if (open == std::string::npos || close == std::string::npos)
    {
        return {};
    }
    return text.substr(open + 1, close + std::string_view("\n  </rocket>\n").size() - open - 1);
}

/// OpenRocketSaver's save of @p document, without simulation data, with an empty preference
/// store; the warnings go to @p warnings.
[[nodiscard]] std::string saveOf(OpenRocketDocument& document, WarningSet& warnings)
{
    const InMemoryPreferences preferences;
    ErrorSet                  errors;
    StorageOptions            options;
    options.setSaveSimulationData(false);
    const Result<std::string> saved =
        OpenRocketSaver::save(document, options, preferences, warnings, errors);
    return saved.has_value() ? *saved : saved.error().toString();
}

// In the whole save, OpenRocketSaver writes the lines of the savers with the subcomponents in
// between, as OpenRocket's: the rocket element of the kitchen sink is OpenRocket's, the one line
// of decision E1 aside.
TEST(ComponentSaver, TheKitchenSinkRocketElementIsOpenRockets)
{
    const std::unique_ptr<OpenRocketDocument> document = kitchenSink();
    WarningSet                                warnings;
    EXPECT_EQ(rocketElementOf(saveOf(*document, warnings)), kJavaRocketElement);
    EXPECT_TRUE(warnings.empty());
}

/// The number of different savers the table names.
[[nodiscard]] std::size_t distinctSavers()
{
    std::set<ComponentSaver::GetElements> savers;
    for (const ComponentKind kind : QtRocket::kAllComponentKinds)
    {
        savers.insert(ComponentSaver::saverOf(kind));
    }
    return savers.size();
}

// The table chooses for each kind the saver Java's reflection finds: the class's own, or that
// of the nearest superclass with one (a booster set is an AxialStage, a pod set a
// ComponentAssembly).
TEST(ComponentSaver, ChoosesTheSaverOfJavasReflection)
{
    EXPECT_EQ(ComponentSaver::saverOf(ComponentKind::ROCKET), &AssemblySaver::rocketElements);
    EXPECT_EQ(ComponentSaver::saverOf(ComponentKind::AXIAL_STAGE),
              &AssemblySaver::axialStageElements);
    EXPECT_EQ(ComponentSaver::saverOf(ComponentKind::PARALLEL_STAGE),
              &AssemblySaver::axialStageElements);
    EXPECT_EQ(ComponentSaver::saverOf(ComponentKind::POD_SET),
              &AssemblySaver::componentAssemblyElements);
    EXPECT_EQ(ComponentSaver::saverOf(ComponentKind::NOSE_CONE),
              &ExternalComponentSaver::noseConeElements);
    EXPECT_EQ(ComponentSaver::saverOf(ComponentKind::BULKHEAD),
              &InternalComponentSaver::bulkheadElements);
    // 22 kinds, 21 savers: AxialStageSaver serves two.
    EXPECT_EQ(distinctSavers(), 21U);
}

/// The lines of the trapezoidal fin set of a new design with a tab of @p height and @p length.
[[nodiscard]] std::string finSetWithTab(double height, double length)
{
    const std::unique_ptr<OpenRocketDocument> document =
        OpenRocketDocumentFactory::createNewRocket();
    auto& body = document->getRocket().getChild(0).addChild(std::make_unique<BodyTube>());
    auto& fins = body.addChild(std::make_unique<QtRocket::TrapezoidFinSet>());
    fins.setTabHeight(height);
    fins.setTabLength(length);
    const InMemoryPreferences preferences;
    OrkWriter                 writer;
    return joined(ComponentSaver::getElements(fins, writer, preferences));
}

// A fin tab is written only when it has a height and a length (FinSetSaver: compatibility with
// file versions below 1.1); a reload then puts it back in the middle of the fin.
TEST(ComponentSaver, WritesAFinTabOnlyWithAHeightAndALength)
{
    EXPECT_FALSE(finSetWithTab(0.01, 0.0).contains("<tab"));
    EXPECT_FALSE(finSetWithTab(0.0, 0.02).contains("<tab"));
    const std::string both = finSetWithTab(0.01, 0.02);
    EXPECT_TRUE(
        both.contains("<tabheight>0.01</tabheight>\n<tablength>0.02</tablength>\n"
                      "<tabposition relativeto=\"center\">0.0</tabposition>\n"
                      "<tabposition relativeto=\"middle\">0.0</tabposition>\n"))
        << both;
}

/// Loads @p text from memory in an environment of its own.
[[nodiscard]] Result<LoadedDocument> reload(LoaderEnvironment& environment, const std::string& text)
{
    const GeneralRocketLoader loader(environment.context());
    return loader.load(QtRocket::stringToBytes(text));
}

// Decision E1 of 2026-10-14: a mass component of 50 g overridden to 80 g writes its own mass
// (OpenRocket writes 0.08, the override, so that its reload has lost the 50 g), and a reload
// gives both values back.
TEST(ComponentSaver, AMassComponentKeepsItsOwnMassAndItsOverride)
{
    const std::unique_ptr<OpenRocketDocument> document =
        OpenRocketDocumentFactory::createNewRocket();
    auto& body = document->getRocket().getChild(0).addChild(std::make_unique<BodyTube>());
    auto& mass = body.addChild(std::make_unique<MassComponent>(0.03, 0.01, 0.05));
    mass.setMassOverridden(true);
    mass.setOverrideMass(0.08);

    WarningSet        warnings;
    const std::string saved = saveOf(*document, warnings);
    EXPECT_TRUE(saved.contains("<overridemass>0.08</overridemass>")) << saved;
    EXPECT_TRUE(saved.contains("<mass>0.05</mass>")) << saved;

    LoaderEnvironment            environment;
    const Result<LoadedDocument> loaded = reload(environment, saved);
    ASSERT_TRUE(loaded.has_value()) << loaded.error().toString();
    const auto& back = dynamic_cast<const MassComponent&>(
        loaded->document->getRocket().getChild(0).getChild(0).getChild(0));
    EXPECT_EQ(back.getComponentMass(), 0.05);
    EXPECT_TRUE(back.isMassOverridden());
    EXPECT_EQ(back.getOverrideMass(), 0.08);
    EXPECT_EQ(back.getMass(), 0.08);
}

/// The <preset> line the savers write for a body tube that keeps @p reference.
[[nodiscard]] std::string presetLineOf(const QtRocket::ComponentPresetReference& reference)
{
    const std::unique_ptr<OpenRocketDocument> document =
        OpenRocketDocumentFactory::createNewRocket();
    auto& body = document->getRocket().getChild(0).addChild(std::make_unique<BodyTube>());
    body.setUnresolvedPreset(reference);
    const InMemoryPreferences preferences;
    OrkWriter                 writer;
    for (const std::string& line : ComponentSaver::getElements(body, writer, preferences))
    {
        if (line.starts_with("<preset"))
        {
            return line;
        }
    }
    return "(none)";
}

// Decision U8 of 2026-10-08: the reference of an unresolved <preset> is written back, after the
// id, with the attributes the file gave (the type and the digest only when it had them), escaped.
// OpenRocket's loader forgets it, so OpenRocket's save has none.
TEST(ComponentSaver, WritesTheReferenceOfAnUnresolvedPresetBack)
{
    EXPECT_EQ(presetLineOf({.type         = "BODY_TUBE",
                            .manufacturer = "Estes",
                            .partNo       = "BT-50FE, 30359",
                            .digest       = "e9c3e97d32e96762f65e051d74cb9fd7"}),
              R"(<preset type="BODY_TUBE" manufacturer="Estes" partno="BT-50FE, 30359" )"
              R"(digest="e9c3e97d32e96762f65e051d74cb9fd7"/>)");
    EXPECT_EQ(presetLineOf({.type         = std::nullopt,
                            .manufacturer = "A&B \"C\"",
                            .partNo       = "<1>",
                            .digest       = std::nullopt}),
              R"(<preset manufacturer="A&amp;B &quot;C&quot;" partno="&lt;1&gt;"/>)");
    // Without a reference there is no <preset>.
    const std::unique_ptr<OpenRocketDocument> document =
        OpenRocketDocumentFactory::createNewRocket();
    const InMemoryPreferences preferences;
    OrkWriter                 writer;
    EXPECT_FALSE(
        joined(ComponentSaver::getElements(document->getRocket().getChild(0), writer, preferences))
            .contains("<preset"));
}

// Decision E3 of tier 10: a text of a component goes through OrkWriter::escape(), so a control
// character is dropped (one save warning) and the file reads back.
TEST(ComponentSaver, DropsWhatADesignFileCannotHoldFromTheTextsOfComponents)
{
    const std::unique_ptr<OpenRocketDocument> document =
        OpenRocketDocumentFactory::createNewRocket();
    auto& body = document->getRocket().getChild(0).addChild(std::make_unique<BodyTube>());
    body.setName("Tube\x01");
    body.setComment("one\x02 two\x1f");
    WarningSet        warnings;
    const std::string saved = saveOf(*document, warnings);
    EXPECT_TRUE(saved.contains("<name>Tube</name>")) << saved;
    EXPECT_TRUE(saved.contains("<comment>one two</comment>")) << saved;
    ASSERT_EQ(warnings.size(), 1U);
    EXPECT_EQ(warnings.begin()->toString(), OpenRocketSaver::kDroppedCharacters);

    LoaderEnvironment            environment;
    const Result<LoadedDocument> loaded = reload(environment, saved);
    ASSERT_TRUE(loaded.has_value()) << loaded.error().toString();
    EXPECT_EQ(loaded->document->getRocket().getChild(0).getChild(0).getComment(), "one two");
}

// A material of type CUSTOM is a programmer error, as in Java (BugException "Unknown material
// type"): no component can hold one (their setters refuse it), so no design gets there.
TEST(ComponentSaver, AMaterialOfTypeCustomIsABug)
{
    const Material custom =
        Material::newMaterial(Material::Type::CUSTOM, "Custom", 1.0, MaterialGroup::CUSTOM, true);
    OrkWriter writer;
    EXPECT_THROW(static_cast<void>(RocketComponentSaver::materialParam("material", custom, writer)),
                 QtRocket::BugError);
    const Material line =
        Material::newMaterial(Material::Type::LINE, "Cord", 0.002, MaterialGroup::ELASTICS, true);
    EXPECT_EQ(RocketComponentSaver::materialParam("linematerial", line, writer),
              R"(<linematerial type="line" density="0.002" shearModulus="0.0" )"
              R"(group="Elastics">Cord</linematerial>)");
}

}  // namespace
