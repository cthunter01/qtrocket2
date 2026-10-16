// The design files of tests/data/ork (OpenRocket 0.9 to 23.09, the file formats 1.0 to 1.9; see
// recorded_ork_files_tests.cpp) through a round trip: each is loaded as RecordedOrkFiles.h records
// it (DesignFileEnvironment: the bundled motor database, the six presets of the examples,
// OpenRocket's test preferences), saved WITH its simulation data as an .ork archive
// (GeneralRocketSaver) and loaded back from memory without a directory (decision O5 of tier 10).
//
// - The archive holds OpenRocket's entries in OpenRocket's order: those of OpenRocket's own archive
//   of the design as loaded, which the golden harness records in the manifest ("resaves.inputs",
//   archiveEntries; tools/openrocket-goldens/README.md). One design cannot be saved as an archive,
//   in OpenRocket as here: v1.6-apocd.ork, a plain XML design, names decal images that are no
//   files beside it, and the save fails with the decal-not-found error for the first of them
//   (OpenRocket's DecalNotFoundException; the manifest's archiveError). Its plain save works and is
//   what its round trip takes, loaded from a file of the same name in a directory of its own: the
//   decal images of a plain design are files beside it, and a plain design loaded from memory reads
//   none and leaves its decals out with a warning each (decision D8 of 2026-10-14).
// - A save of the reload is the first save, entry for entry and byte for byte (the names, their
//   order and the uncompressed bytes; decision O7 of tier 10: never the bytes of the archives), or
//   line for line for the plain save. OpenRocket's own round trip of these files reaches the same
//   fixed point after one save (the probe FixedPointProbe of the scouts of tier 10,
//   roundtrip-goldens: 18 of 18).
// - The state of the reload (designBytesState() of DesignFileState.h, in the lines of
//   RecordedOrkFiles.h) is the state of the file as loaded but for the changes of a first save,
//   which are OpenRocket's own (kOpenRocketStateChanges: what OpenRocket's state of its own save
//   of the file, as loaded, has in place of its state of the file, printed by the probe
//   LegacyRoundTripProbe in the same lines) but for QtRocket's own (kOwnStateChanges). The
//   changes of a first save that kOpenRocketStateChanges holds: the file version becomes 1.11
//   ("version=", eighteen files); the warnings of the load are gone, as nothing a save writes
//   gives them again (the motors a load could not find or found twice, the references to presets
//   the six presets do not hold, the unknown attributes of an appearance, the extension of an
//   unknown id; "W" lines); the modification ids of simplerocket.ork ("modids"); the number of the
//   change events of the load ("events rocket=", eighteen files); the motor a load could not find
//   and the empty motor of a configuration are not written, and a motor's ignition becomes its
//   own (the last flag of "ign=" and of a motor of a configuration); a stage loses the flags of
//   the old <overridesubcomponents> for an override it does not have; the length and the tab
//   position of the eight freeform fin sets of v1.6-apocd.ork change in the last digit; the
//   photo setting smokeOpacity of v1.8-logo-rocket.ork is gone ("photo="); and the stored data are
//   written with the precision of the saver (STORAGE_DECIMAL_PLACES), so the digests of their
//   columns change ("col"). A simulation keeps its status (v1.9-chute-release.ork's OUTDATED one
//   too) and its branches keep the order of their types. The ids the first load drew for the flight
//   configurations a file does not name (the four of simplerocket.ork's simulations with an empty
//   <configid>) are written by the save and read back: they are told apart by the ids of the
//   original document, as "random" (designBytesState(), idsOf), as in its state and in the probe's.

#include <algorithm>
#include <array>
#include <cstddef>
#include <filesystem>
#include <format>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/StorageOptions.h"
#include "QtRocket/file/GeneralRocketLoader.h"
#include "QtRocket/file/GeneralRocketSaver.h"
#include "QtRocket/file/LoadedDocument.h"
#include "QtRocket/file/ZipArchive.h"
#include "QtRocket/file/openrocket/OpenRocketSaver.h"
#include "QtRocket/logging/ErrorSet.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"
#include "TestTempDir.h"
#include "file/DesignFileEnvironment.h"
#include "file/openrocket/ComponentHandlerTestSupport.h"
#include "file/openrocket/DesignFileState.h"
#include "file/openrocket/RecordedOrkFiles.h"
#include "goldens/GoldenResaves.h"

namespace
{

using QtRocket::ErrorCode;
using QtRocket::ErrorSet;
using QtRocket::GeneralRocketLoader;
using QtRocket::GeneralRocketSaver;
using QtRocket::LoadedDocument;
using QtRocket::OpenRocketDocument;
using QtRocket::OpenRocketSaver;
using QtRocket::Result;
using QtRocket::StorageOptions;
using QtRocket::WarningSet;
using QtRocket::ZipArchive;
using QtRocket::Test::archiveChanges;
using QtRocket::Test::designBytesState;
using QtRocket::Test::DesignFileEnvironment;
using QtRocket::Test::designFileState;
using QtRocket::Test::kRecordedOrkFiles;
using QtRocket::Test::lineChanges;
using QtRocket::Test::RecordedOrkFile;
using QtRocket::Test::ResaveInput;
using QtRocket::Test::StateDetail;
using QtRocket::Test::TempDir;

/// What a round trip changes in the state of a design file (lineChanges() from the state as
/// loaded, RecordedOrkFiles.h, to the state of the reload): "- <line>" and "+ <line>".
struct LegacyStateChanges
{
    std::string_view                  file;
    std::span<const std::string_view> changes;
};

// @@BEGIN GENERATED kOpenRocketStateChanges
// GENERATED by make_legacy_changes_table.py of the probes of run 10b, part
// "legacy-and-harness", from the output of LegacyRoundTripProbe.java (OpenRocket 5f164fd0e,
// JDK 17, the environment of RecordedOrkFiles.h): do not edit by hand.
// clang-format off
namespace OpenRocketStateChanges
{
inline constexpr auto kChanges0 = std::to_array<std::string_view>({
    R"(- version=102)",
    R"(- W Multiple motors with designation 'A8' for manufacturer 'Estes' found, one chosen arbitrarily.)",
    R"(+ version=111)",
    R"(- modids mass=false aero=false tree=true functional=true snapshot=true)",
    R"(+ modids mass=true aero=true tree=false functional=true snapshot=true)",
    R"(- events rocket=84)",
    R"(+ events rocket=104)",
    R"(- |       InnerTube 'Inner Tube' axial=BOTTOM:0.005 x=0.22999999999999998 len=0.075 mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] outer=0.0095:false inner=0.009:false radial=0.0:0.0 cluster=single:1.0:0.0 mount=true overhang=0.003 ign=AUTOMATIC:0.0:true motor[da326836-0959-4c94-bcd5-49dee07235a4]=A8:3.0:0.0:AUTOMATIC:0.0:false)",
    R"(+ |       InnerTube 'Inner Tube' axial=BOTTOM:0.005 x=0.22999999999999998 len=0.075 mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] outer=0.0095:false inner=0.009:false radial=0.0:0.0 cluster=single:1.0:0.0 mount=true overhang=0.003 ign=AUTOMATIC:0.0:true motor[da326836-0959-4c94-bcd5-49dee07235a4]=A8:3.0:0.0:AUTOMATIC:0.0:true)",
    R"(- | config da326836-0959-4c94-bcd5-49dee07235a4 name='[{motors}]' preload=null active=[true] motors=1 all=[#6:A8:AUTOMATIC:0.0:false] flying=[#6:A8:AUTOMATIC:0.0:false])",
    R"(+ | config da326836-0959-4c94-bcd5-49dee07235a4 name='[{motors}]' preload=null active=[true] motors=1 all=[#6:A8:AUTOMATIC:0.0:true] flying=[#6:A8:AUTOMATIC:0.0:true])",
});
inline constexpr auto kChanges1 = std::to_array<std::string_view>({
    R"(- version=100)",
    R"(- W No motor with designation 'D7' for manufacturer 'WECO Feuerwerk' found.)",
    R"(+ version=111)",
    R"(- events rocket=90)",
    R"(+ events rocket=111)",
    R"(- |       InnerTube 'Clustered Inner Tube' axial=BOTTOM:0.005 x=0.33 len=0.075 comment='Clustering the inner tube automatically multiplies also the components attached to it.\nThe cluster configuration can be modified at any point in the "Cluster" tab.' mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] outer=0.013000000000000001:false inner=0.0125:false radial=0.0:0.0 cluster=single:1.1:0.0 mount=true overhang=0.003 ign=AUTOMATIC:0.0:true motor[e1dc7488-77c0-403a-b54f-c01db07aba2f]=D12:5.0:0.0:AUTOMATIC:0.0:false motor[d33813f0-2563-41f7-8bb3-96eae598f567]=none:3.0:0.0:AUTOMATIC:0.0:false)",
    R"(+ |       InnerTube 'Clustered Inner Tube' axial=BOTTOM:0.005 x=0.33 len=0.075 comment='Clustering the inner tube automatically multiplies also the components attached to it.\nThe cluster configuration can be modified at any point in the "Cluster" tab.' mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] outer=0.013000000000000001:false inner=0.0125:false radial=0.0:0.0 cluster=single:1.1:0.0 mount=true overhang=0.003 ign=AUTOMATIC:0.0:true motor[e1dc7488-77c0-403a-b54f-c01db07aba2f]=D12:5.0:0.0:AUTOMATIC:0.0:true)",
    R"(- | config e1dc7488-77c0-403a-b54f-c01db07aba2f name='[{motors}]' preload=null active=[true] motors=1 all=[#4:D12:AUTOMATIC:0.0:false] flying=[#4:D12:AUTOMATIC:0.0:false])",
    R"(+ | config e1dc7488-77c0-403a-b54f-c01db07aba2f name='[{motors}]' preload=null active=[true] motors=1 all=[#4:D12:AUTOMATIC:0.0:true] flying=[#4:D12:AUTOMATIC:0.0:true])",
    R"(- col acceleration_xy: min=0.0 max=1.8911 first=0.0 last=0.43151 sum=67.5307645 nans=0)",
    R"(- col aoa: min=1.8276E-4 max=1.5708 first=1.5708 last=NaN sum=2.09425627 nans=137)",
    R"(+ col acceleration_xy: min=0.0 max=1.8911 first=0.0 last=0.43151 sum=67.530765 nans=0)",
    R"(+ col aoa: min=1.8276E-4 max=1.5708 first=1.5708 last=NaN sum=2.0942563699999996 nans=137)",
    R"(- col pitch_rate: min=-0.05501 max=0.056905 first=0.0 last=NaN sum=-0.0049458700000000275 nans=137)",
    R"(- col yaw_rate: min=-0.088921 max=0.052554 first=0.0 last=NaN sum=-0.3297573 nans=137)",
    R"(+ col pitch_rate: min=-0.05501 max=0.056905 first=0.0 last=NaN sum=-0.0049453699999999975 nans=137)",
    R"(+ col yaw_rate: min=-0.088921 max=0.052554 first=0.0 last=NaN sum=-0.32975699999999997 nans=137)",
    R"(- col normal_force_coeff: min=9.9077E-4 max=0.45957 first=NaN last=NaN sum=2.20144427 nans=139)",
    R"(- col pitch_moment_coeff: min=0.0010041 max=0.40102 first=NaN last=NaN sum=1.6925413999999999 nans=139)",
    R"(+ col normal_force_coeff: min=9.9077E-4 max=0.45957 first=NaN last=NaN sum=2.20144477 nans=139)",
    R"(+ col pitch_moment_coeff: min=0.001004 max=0.40102 first=NaN last=NaN sum=1.692543 nans=139)",
    R"(- col roll_moment_coeff: min=-0.21613 max=0.15197 first=NaN last=NaN sum=-0.7121039 nans=139)",
    R"(+ col roll_moment_coeff: min=-0.21613 max=0.15197 first=NaN last=NaN sum=-0.7121040000000001 nans=139)",
    R"(- col mach_number: min=0.0016879 max=0.25864 first=0.0054322 last=0.012954 sum=5.9748263000000055 nans=0)",
    R"(+ col mach_number: min=0.001688 max=0.25864 first=0.005432 last=0.012954 sum=5.974827000000004 nans=0)",
    R"(- col reference_area: min=0.0015904 max=0.0015904 first=0.0015904 last=NaN sum=0.05407359999999997 nans=137)",
    R"(- col time_step: min=0.001 max=0.5 first=0.05 last=0.5 sum=62.2355082 nans=0)",
    R"(+ col reference_area: min=0.00159 max=0.00159 first=0.00159 last=NaN sum=0.054060000000000025 nans=137)",
    R"(+ col time_step: min=0.001 max=0.5 first=0.05 last=0.5 sum=62.235509 nans=0)",
});
inline constexpr auto kChanges2 = std::to_array<std::string_view>({
    R"(- version=104)",
    R"(- W No motor with designation 'D7' for manufacturer 'WECO Feuerwerk' found.)",
    R"(+ version=111)",
    R"(- events rocket=90)",
    R"(+ events rocket=111)",
    R"(- |       InnerTube 'Clustered Inner Tube' axial=BOTTOM:0.005 x=0.33 len=0.075 comment='Clustering the inner tube automatically multiplies also the components attached to it.\nThe cluster configuration can be modified at any point in the "Cluster" tab.' mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] outer=0.013000000000000001:false inner=0.0125:false radial=0.0:0.0 cluster=single:1.1:0.0 mount=true overhang=0.003 ign=AUTOMATIC:0.0:true motor[e1dc7488-77c0-403a-b54f-c01db07aba2f]=D12:5.0:0.0:AUTOMATIC:0.0:false motor[d33813f0-2563-41f7-8bb3-96eae598f567]=none:3.0:0.0:AUTOMATIC:0.0:false)",
    R"(+ |       InnerTube 'Clustered Inner Tube' axial=BOTTOM:0.005 x=0.33 len=0.075 comment='Clustering the inner tube automatically multiplies also the components attached to it.\nThe cluster configuration can be modified at any point in the "Cluster" tab.' mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] outer=0.013000000000000001:false inner=0.0125:false radial=0.0:0.0 cluster=single:1.1:0.0 mount=true overhang=0.003 ign=AUTOMATIC:0.0:true motor[e1dc7488-77c0-403a-b54f-c01db07aba2f]=D12:5.0:0.0:AUTOMATIC:0.0:true)",
    R"(- | config e1dc7488-77c0-403a-b54f-c01db07aba2f name='[{motors}]' preload=null active=[true] motors=1 all=[#4:D12:AUTOMATIC:0.0:false] flying=[#4:D12:AUTOMATIC:0.0:false])",
    R"(+ | config e1dc7488-77c0-403a-b54f-c01db07aba2f name='[{motors}]' preload=null active=[true] motors=1 all=[#4:D12:AUTOMATIC:0.0:true] flying=[#4:D12:AUTOMATIC:0.0:true])",
    R"(- col acceleration_xy: min=0.0 max=2.6414 first=0.0 last=0.24186 sum=72.14898930000001 nans=0)",
    R"(+ col acceleration_xy: min=0.0 max=2.6414 first=0.0 last=0.24186 sum=72.1489896 nans=0)",
    R"(- col aoa: min=5.0229E-4 max=1.5708 first=1.5708 last=NaN sum=1.8934908799999999 nans=147)",
    R"(+ col aoa: min=5.0229E-4 max=1.5708 first=1.5708 last=NaN sum=1.8934913800000002 nans=147)",
    R"(- col pitch_rate: min=-0.025757 max=0.032841 first=0.0 last=NaN sum=0.0317748 nans=147)",
    R"(- col yaw_rate: min=-0.04078 max=0.02708 first=0.0 last=NaN sum=-0.241403805 nans=147)",
    R"(+ col pitch_rate: min=-0.025757 max=0.032841 first=0.0 last=NaN sum=0.031776 nans=147)",
    R"(+ col yaw_rate: min=-0.04078 max=0.02708 first=0.0 last=NaN sum=-0.24140460499999997 nans=147)",
    R"(- col longitudinal_inertia: min=0.0033632 max=0.0040959 first=0.0040959 last=NaN sum=0.09344789999999994 nans=147)",
    R"(+ col longitudinal_inertia: min=0.003363 max=0.004096 first=0.004096 last=NaN sum=0.09344400000000003 nans=147)",
    R"(- col normal_force_coeff: min=0.0027649 max=0.79929 first=NaN last=NaN sum=2.0204028999999997 nans=148)",
    R"(- col pitch_moment_coeff: min=-0.0817 max=0.15639 first=NaN last=NaN sum=0.5890502000000001 nans=148)",
    R"(- col yaw_moment_coeff: min=-3.8434E-4 max=0.0011442 first=NaN last=NaN sum=0.0022881986 nans=148)",
    R"(+ col normal_force_coeff: min=0.002765 max=0.79929 first=NaN last=NaN sum=2.020402 nans=148)",
    R"(+ col pitch_moment_coeff: min=-0.0817 max=0.15639 first=NaN last=NaN sum=0.5890500000000001 nans=148)",
    R"(+ col yaw_moment_coeff: min=-3.8434E-4 max=0.001144 first=NaN last=NaN sum=0.0022879986 nans=148)",
    R"(- col roll_moment_coeff: min=-0.18219 max=0.45016 first=NaN last=NaN sum=-0.13013670000000005 nans=148)",
    R"(+ col roll_moment_coeff: min=-0.18219 max=0.45016 first=NaN last=NaN sum=-0.13013800000000003 nans=148)",
    R"(- col mach_number: min=0.0037114 max=0.26047 first=0.0054016 last=0.01364 sum=5.242030000000002 nans=0)",
    R"(+ col mach_number: min=0.003711 max=0.26047 first=0.005402 last=0.01364 sum=5.242030000000002 nans=0)",
    R"(- col reference_area: min=0.0015904 max=0.0015904 first=0.0015904 last=NaN sum=0.04294079999999998 nans=147)",
    R"(- col time_step: min=0.0025 max=0.5 first=0.01 last=0.39802 sum=65.59748909999999 nans=0)",
    R"(- col computation_time: min=0.0020227 max=0.46715 first=0.0020227 last=0.46715 sum=75.0811687 nans=0)",
    R"(+ col reference_area: min=0.00159 max=0.00159 first=0.00159 last=NaN sum=0.04293000000000002 nans=147)",
    R"(+ col time_step: min=0.0025 max=0.5 first=0.01 last=0.39802 sum=65.59748999999998 nans=0)",
    R"(+ col computation_time: min=0.002023 max=0.46715 first=0.002023 last=0.46715 sum=75.08116900000002 nans=0)",
});
inline constexpr auto kChanges3 = std::to_array<std::string_view>({
    R"(- version=105)",
    R"(- W No matching ComponentPreset for component Nose cone found matching SEMROC Astronautics BNC-55F)",
    R"(- W No matching ComponentPreset for component Body tube found matching SEMROC Astronautics BT-55)",
    R"(- W No matching ComponentPreset for component Centering ring found matching SEMROC Astronautics RA-5055)",
    R"(- W No matching ComponentPreset for component Inner Tube found matching SEMROC Astronautics BT-50J)",
    R"(- W No matching ComponentPreset for component Launch lug found matching SEMROC Astronautics LL-117)",
    R"(- W No matching ComponentPreset for component Parachute found matching SEMROC Astronautics PN-18)",
    R"(+ version=111)",
    R"(- events rocket=68)",
    R"(+ events rocket=80)",
    R"(- |       InnerTube 'Inner Tube' axial=BOTTOM:0.00635 x=0.3937 len=0.06985 mat=[BULK|Spiral/Glassine|799.7650217227243|0.0|Custom] outer=0.012395199999999999:false inner=0.012065:false radial=0.0:0.0 cluster=single:1.0:0.0 mount=true overhang=0.0 ign=AUTOMATIC:0.0:true motor[f08d69f3-6e6e-407f-b160-614cdf8a8700]=D12:7.0:0.0:AUTOMATIC:0.0:false)",
    R"(+ |       InnerTube 'Inner Tube' axial=BOTTOM:0.00635 x=0.3937 len=0.06985 mat=[BULK|Spiral/Glassine|799.7650217227243|0.0|Custom] outer=0.012395199999999999:false inner=0.012065:false radial=0.0:0.0 cluster=single:1.0:0.0 mount=true overhang=0.0 ign=AUTOMATIC:0.0:true motor[f08d69f3-6e6e-407f-b160-614cdf8a8700]=D12:7.0:0.0:AUTOMATIC:0.0:true)",
    R"(- | config f08d69f3-6e6e-407f-b160-614cdf8a8700 name='[{motors}]' preload=null active=[true] motors=1 all=[#6:D12:AUTOMATIC:0.0:false] flying=[#6:D12:AUTOMATIC:0.0:false])",
    R"(+ | config f08d69f3-6e6e-407f-b160-614cdf8a8700 name='[{motors}]' preload=null active=[true] motors=1 all=[#6:D12:AUTOMATIC:0.0:true] flying=[#6:D12:AUTOMATIC:0.0:true])",
});
inline constexpr auto kChanges4 = std::to_array<std::string_view>({
    R"(- version=106)",
    R"(+ version=111)",
    R"(- events rocket=83)",
    R"(+ events rocket=103)",
});
inline constexpr auto kChanges5 = std::to_array<std::string_view>({
    R"(- version=106)",
    R"(- W Unknown attributes in element 'ambient', ignoring.)",
    R"(- W Unknown attributes in element 'diffuse', ignoring.)",
    R"(- W Unknown attributes in element 'specular', ignoring.)",
    R"(+ version=111)",
    R"(- events rocket=321)",
    R"(+ events rocket=373)",
    R"(- |   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=1.9788999999999997 massovr=4.0 cgovr=1.05829 subovr=true,true,true stage=0 sep=EJECTION:0.0:200.0)",
    R"(+ |   AxialStage 'Stage' axial=AFTER:0.0 x=0.0 len=1.9788999999999997 massovr=4.0 cgovr=1.05829 subovr=true,true,false stage=0 sep=EJECTION:0.0:200.0)",
    R"(- |       FreeformFinSet 'Contre fins 1a' axial=TOP:0.1349 x=0.1349 len=0.392101 inst=1 radius=SURFACE:0.0 angle=RELATIVE:-0.0500037 finish=SMOOTH mat=[BULK|RS: Birch|680.785|0.0|Custom] fins=1 cant=0.0 thick=0.00478 cross=ROUNDED tab=0.0:0.05:MIDDLE:0.0:0.1710505 fillet=0.0:[BULK|Cardboard|680.0|4.0E8|PaperProducts] points=0.0,0.0;0.0038,0.007;0.114111,0.019915600000000002;0.272906,0.02;0.381787,0.01;0.38726900000000003,0.009;0.391,0.004;0.3921009999999999,0.0 app=[paint=187,187,187,255 shine=0.3 opacity=false])",
    R"(- |       FreeformFinSet 'Contre fins 1b' axial=TOP:0.1349 x=0.1349 len=0.392101 inst=1 radius=SURFACE:0.0 angle=RELATIVE:0.0525344 finish=SMOOTH mat=[BULK|RS: Birch|680.785|0.0|Custom] fins=1 cant=0.0 thick=0.00478 cross=ROUNDED tab=0.0:0.05:MIDDLE:0.0:0.1710505 fillet=0.0:[BULK|Cardboard|680.0|4.0E8|PaperProducts] points=0.0,0.0;0.0038,0.007;0.114,0.02;0.272906,0.02;0.381787,0.01;0.38726900000000003,0.009;0.391,0.004;0.3921009999999999,0.0 app=[paint=187,187,187,255 shine=0.3 opacity=false])",
    R"(+ |       FreeformFinSet 'Contre fins 1a' axial=TOP:0.1349 x=0.1349 len=0.3921009999999999 inst=1 radius=SURFACE:0.0 angle=RELATIVE:-0.0500037 finish=SMOOTH mat=[BULK|RS: Birch|680.785|0.0|Custom] fins=1 cant=0.0 thick=0.00478 cross=ROUNDED tab=0.0:0.05:MIDDLE:0.0:0.17105049999999997 fillet=0.0:[BULK|Cardboard|680.0|4.0E8|PaperProducts] points=0.0,0.0;0.0038,0.007;0.114111,0.019915600000000002;0.272906,0.02;0.381787,0.01;0.38726900000000003,0.009;0.391,0.004;0.3921009999999999,0.0 app=[paint=187,187,187,255 shine=0.3 opacity=false])",
    R"(+ |       FreeformFinSet 'Contre fins 1b' axial=TOP:0.1349 x=0.1349 len=0.3921009999999999 inst=1 radius=SURFACE:0.0 angle=RELATIVE:0.0525344 finish=SMOOTH mat=[BULK|RS: Birch|680.785|0.0|Custom] fins=1 cant=0.0 thick=0.00478 cross=ROUNDED tab=0.0:0.05:MIDDLE:0.0:0.17105049999999997 fillet=0.0:[BULK|Cardboard|680.0|4.0E8|PaperProducts] points=0.0,0.0;0.0038,0.007;0.114,0.02;0.272906,0.02;0.381787,0.01;0.38726900000000003,0.009;0.391,0.004;0.3921009999999999,0.0 app=[paint=187,187,187,255 shine=0.3 opacity=false])",
    R"(- |       FreeformFinSet 'Contre fins 2a' axial=TOP:0.1349 x=0.1349 len=0.392101 inst=1 radius=SURFACE:0.0 angle=RELATIVE:1.50334 finish=SMOOTH mat=[BULK|RS: Birch|680.785|0.0|Custom] fins=1 cant=0.0 thick=0.00478 cross=ROUNDED tab=0.0:0.05:MIDDLE:0.0:0.1710505 fillet=0.0:[BULK|Cardboard|680.0|4.0E8|PaperProducts] points=0.0,0.0;0.0038,0.007;0.114,0.02;0.272906,0.02;0.381787,0.01;0.38726900000000003,0.009;0.391,0.004;0.3921009999999999,0.0 app=[paint=187,187,187,255 shine=0.3 opacity=false])",
    R"(- |       FreeformFinSet 'Contre fins 2b' axial=TOP:0.1349 x=0.1349 len=0.392101 inst=1 radius=SURFACE:0.0 angle=RELATIVE:1.62988 finish=SMOOTH mat=[BULK|RS: Birch|680.785|0.0|Custom] fins=1 cant=0.0 thick=0.00478 cross=ROUNDED tab=0.0:0.05:MIDDLE:0.0:0.1710505 fillet=0.0:[BULK|Cardboard|680.0|4.0E8|PaperProducts] points=0.0,0.0;0.0038,0.007;0.114,0.02;0.272906,0.02;0.381787,0.01;0.38726900000000003,0.009;0.391,0.004;0.3921009999999999,0.0 app=[paint=187,187,187,255 shine=0.3 opacity=false])",
    R"(+ |       FreeformFinSet 'Contre fins 2a' axial=TOP:0.1349 x=0.1349 len=0.3921009999999999 inst=1 radius=SURFACE:0.0 angle=RELATIVE:1.50334 finish=SMOOTH mat=[BULK|RS: Birch|680.785|0.0|Custom] fins=1 cant=0.0 thick=0.00478 cross=ROUNDED tab=0.0:0.05:MIDDLE:0.0:0.17105049999999997 fillet=0.0:[BULK|Cardboard|680.0|4.0E8|PaperProducts] points=0.0,0.0;0.0038,0.007;0.114,0.02;0.272906,0.02;0.381787,0.01;0.38726900000000003,0.009;0.391,0.004;0.3921009999999999,0.0 app=[paint=187,187,187,255 shine=0.3 opacity=false])",
    R"(+ |       FreeformFinSet 'Contre fins 2b' axial=TOP:0.1349 x=0.1349 len=0.3921009999999999 inst=1 radius=SURFACE:0.0 angle=RELATIVE:1.62988 finish=SMOOTH mat=[BULK|RS: Birch|680.785|0.0|Custom] fins=1 cant=0.0 thick=0.00478 cross=ROUNDED tab=0.0:0.05:MIDDLE:0.0:0.17105049999999997 fillet=0.0:[BULK|Cardboard|680.0|4.0E8|PaperProducts] points=0.0,0.0;0.0038,0.007;0.114,0.02;0.272906,0.02;0.381787,0.01;0.38726900000000003,0.009;0.391,0.004;0.3921009999999999,0.0 app=[paint=187,187,187,255 shine=0.3 opacity=false])",
    R"(- |       FreeformFinSet 'Contre fins 3a' axial=TOP:0.1349 x=0.1349 len=0.392101 inst=1 radius=SURFACE:0.0 angle=RELATIVE:3.07195 finish=SMOOTH mat=[BULK|RS: Birch|680.785|0.0|Custom] fins=1 cant=0.0 thick=0.00478 cross=ROUNDED tab=0.0:0.05:MIDDLE:0.0:0.1710505 fillet=0.0:[BULK|Cardboard|680.0|4.0E8|PaperProducts] points=0.0,0.0;0.0038,0.007;0.114,0.02;0.272906,0.02;0.381787,0.01;0.38726900000000003,0.009;0.391,0.004;0.3921009999999999,0.0 app=[paint=187,187,187,255 shine=0.3 opacity=false])",
    R"(- |       FreeformFinSet 'Contre fins 3b' axial=TOP:0.1349 x=0.1349 len=0.392101 inst=1 radius=SURFACE:0.0 angle=RELATIVE:-3.09342 finish=SMOOTH mat=[BULK|RS: Birch|680.785|0.0|Custom] fins=1 cant=0.0 thick=0.00478 cross=ROUNDED tab=0.0:0.05:MIDDLE:0.0:0.1710505 fillet=0.0:[BULK|Cardboard|680.0|4.0E8|PaperProducts] points=0.0,0.0;0.0038,0.007;0.114,0.02;0.272906,0.02;0.381787,0.01;0.38726900000000003,0.009;0.391,0.004;0.3921009999999999,0.0 app=[paint=187,187,187,255 shine=0.3 opacity=false])",
    R"(+ |       FreeformFinSet 'Contre fins 3a' axial=TOP:0.1349 x=0.1349 len=0.3921009999999999 inst=1 radius=SURFACE:0.0 angle=RELATIVE:3.07195 finish=SMOOTH mat=[BULK|RS: Birch|680.785|0.0|Custom] fins=1 cant=0.0 thick=0.00478 cross=ROUNDED tab=0.0:0.05:MIDDLE:0.0:0.17105049999999997 fillet=0.0:[BULK|Cardboard|680.0|4.0E8|PaperProducts] points=0.0,0.0;0.0038,0.007;0.114,0.02;0.272906,0.02;0.381787,0.01;0.38726900000000003,0.009;0.391,0.004;0.3921009999999999,0.0 app=[paint=187,187,187,255 shine=0.3 opacity=false])",
    R"(+ |       FreeformFinSet 'Contre fins 3b' axial=TOP:0.1349 x=0.1349 len=0.3921009999999999 inst=1 radius=SURFACE:0.0 angle=RELATIVE:-3.09342 finish=SMOOTH mat=[BULK|RS: Birch|680.785|0.0|Custom] fins=1 cant=0.0 thick=0.00478 cross=ROUNDED tab=0.0:0.05:MIDDLE:0.0:0.17105049999999997 fillet=0.0:[BULK|Cardboard|680.0|4.0E8|PaperProducts] points=0.0,0.0;0.0038,0.007;0.114,0.02;0.272906,0.02;0.381787,0.01;0.38726900000000003,0.009;0.391,0.004;0.3921009999999999,0.0 app=[paint=187,187,187,255 shine=0.3 opacity=false])",
    R"(- |       FreeformFinSet 'Contre fins 4a' axial=TOP:0.1349 x=0.1349 len=0.392101 inst=1 radius=SURFACE:0.0 angle=RELATIVE:-1.50954 finish=SMOOTH mat=[BULK|RS: Birch|680.785|0.0|Custom] fins=1 cant=0.0 thick=0.00478 cross=ROUNDED tab=0.0:0.05:MIDDLE:0.0:0.1710505 fillet=0.0:[BULK|Cardboard|680.0|4.0E8|PaperProducts] points=0.0,0.0;0.0038,0.007;0.114,0.02;0.272906,0.02;0.381787,0.01;0.38726900000000003,0.009;0.391,0.004;0.3921009999999999,0.0 app=[paint=187,187,187,255 shine=0.3 opacity=false])",
    R"(- |       FreeformFinSet 'Contre fins 4b' axial=TOP:0.1349 x=0.1349 len=0.392101 inst=1 radius=SURFACE:0.0 angle=RELATIVE:-1.64916 finish=SMOOTH mat=[BULK|RS: Birch|680.785|0.0|Custom] fins=1 cant=0.0 thick=0.00478 cross=ROUNDED tab=0.0:0.05:MIDDLE:0.0:0.1710505 fillet=0.0:[BULK|Cardboard|680.0|4.0E8|PaperProducts] points=0.0,0.0;0.0038,0.007;0.114,0.02;0.272906,0.02;0.381787,0.01;0.38726900000000003,0.009;0.391,0.004;0.3921009999999999,0.0 app=[paint=187,187,187,255 shine=0.3 opacity=false])",
    R"(+ |       FreeformFinSet 'Contre fins 4a' axial=TOP:0.1349 x=0.1349 len=0.3921009999999999 inst=1 radius=SURFACE:0.0 angle=RELATIVE:-1.50954 finish=SMOOTH mat=[BULK|RS: Birch|680.785|0.0|Custom] fins=1 cant=0.0 thick=0.00478 cross=ROUNDED tab=0.0:0.05:MIDDLE:0.0:0.17105049999999997 fillet=0.0:[BULK|Cardboard|680.0|4.0E8|PaperProducts] points=0.0,0.0;0.0038,0.007;0.114,0.02;0.272906,0.02;0.381787,0.01;0.38726900000000003,0.009;0.391,0.004;0.3921009999999999,0.0 app=[paint=187,187,187,255 shine=0.3 opacity=false])",
    R"(+ |       FreeformFinSet 'Contre fins 4b' axial=TOP:0.1349 x=0.1349 len=0.3921009999999999 inst=1 radius=SURFACE:0.0 angle=RELATIVE:-1.64916 finish=SMOOTH mat=[BULK|RS: Birch|680.785|0.0|Custom] fins=1 cant=0.0 thick=0.00478 cross=ROUNDED tab=0.0:0.05:MIDDLE:0.0:0.17105049999999997 fillet=0.0:[BULK|Cardboard|680.0|4.0E8|PaperProducts] points=0.0,0.0;0.0038,0.007;0.114,0.02;0.272906,0.02;0.381787,0.01;0.38726900000000003,0.009;0.391,0.004;0.3921009999999999,0.0 app=[paint=187,187,187,255 shine=0.3 opacity=false])",
});
inline constexpr auto kChanges6 = std::to_array<std::string_view>({
    R"(- version=106)",
    R"(- W No motor with designation 'J1000-LW' for manufacturer 'Loki Research' found.)",
    R"(+ version=111)",
    R"(- events rocket=218)",
    R"(+ events rocket=256)",
    R"(- |   AxialStage 'Dart' axial=AFTER:0.0 x=0.0 len=0.87 massovr=1.53087424875 subovr=true,true,true stage=0 sep=EJECTION:0.0:200.0)",
    R"(+ |   AxialStage 'Dart' axial=AFTER:0.0 x=0.0 len=0.87 massovr=1.53087424875 subovr=true,false,false stage=0 sep=EJECTION:0.0:200.0)",
    R"(- |       InnerTube 'Inner Tube' axial=BOTTOM:0.0 x=0.5757000000000001 len=0.2193 mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] outer=0.02:false inner=0.02:false radial=0.0:0.0 cluster=single:1.0:0.0 mount=true overhang=0.0 ign=AUTOMATIC:0.0:true motor[bb09f70f-1e69-4ba8-b165-ba19cf33d78c]=none:Infinity:0.0:AUTOMATIC:0.0:true motor[f5e1eae4-7420-4fd4-826b-31b025a1bcb1]=none:Infinity:0.0:AUTOMATIC:0.0:true)",
    R"(+ |       InnerTube 'Inner Tube' axial=BOTTOM:0.0 x=0.5757000000000001 len=0.2193 mat=[BULK|Cardboard|680.0|4.0E8|PaperProducts] outer=0.02:false inner=0.02:false radial=0.0:0.0 cluster=single:1.0:0.0 mount=true overhang=0.0 ign=AUTOMATIC:0.0:true)",
});
inline constexpr auto kChanges7 = std::to_array<std::string_view>({
    R"(- version=106)",
    R"(+ version=111)",
    R"(- events rocket=135)",
    R"(+ events rocket=162)",
});
inline constexpr auto kChanges8 = std::to_array<std::string_view>({
    R"(- version=106)",
    R"(- W No matching ComponentPreset for component Nose cone found matching SEMROC Astronautics BNC-55F)",
    R"(- W No matching ComponentPreset for component Body tube found matching SEMROC Astronautics BT-55)",
    R"(- W No matching ComponentPreset for component Centering ring found matching SEMROC Astronautics RA-5055)",
    R"(- W No matching ComponentPreset for component Inner Tube found matching SEMROC Astronautics BT-50J)",
    R"(- W No matching ComponentPreset for component Launch lug found matching SEMROC Astronautics LL-117)",
    R"(- W No matching ComponentPreset for component Parachute found matching SEMROC Astronautics PN-18)",
    R"(+ version=111)",
    R"(- events rocket=73)",
    R"(+ events rocket=85)",
});
inline constexpr auto kChanges9 = std::to_array<std::string_view>({
    R"(- version=106)",
    R"(+ version=111)",
    R"(- events rocket=203)",
    R"(+ events rocket=241)",
});
inline constexpr auto kChanges10 = std::to_array<std::string_view>({
    R"(- version=106)",
    R"(+ version=111)",
    R"(- events rocket=111)",
    R"(+ events rocket=135)",
});
inline constexpr auto kChanges11 = std::to_array<std::string_view>({
    R"(- version=106)",
    R"(+ version=111)",
    R"(- events rocket=153)",
    R"(+ events rocket=198)",
});
inline constexpr auto kChanges12 = std::to_array<std::string_view>({
    R"(- version=107)",
    R"(- W Simulation extension with id 'info.openrocket.core.simulation.extension.impl.AirStart' not found.)",
    R"(+ version=111)",
    R"(- events rocket=203)",
    R"(+ events rocket=241)",
});
inline constexpr auto kChanges13 = std::to_array<std::string_view>({
    R"(- version=107)",
    R"(- W No matching ComponentPreset for component Body tube found matching FlisKits BT-50-18)",
    R"(+ version=111)",
    R"(- events rocket=24)",
    R"(+ events rocket=31)",
    R"(- col altitude: min=-0.33637 max=237.55 first=0.0 last=-0.33637 sum=40326.56881497999 nans=0)",
    R"(+ col altitude: min=-0.33637 max=237.55 first=0.0 last=-0.33637 sum=40326.56881477999 nans=0)",
    R"(- col acceleration_z: min=-99.753 max=302.76 first=-7.0186 last=0.022124 sum=3121.6582718 nans=0)",
    R"(+ col acceleration_z: min=-99.753 max=302.76 first=-7.0186 last=0.022124 sum=3121.658272 nans=0)",
    R"(- col position_y: min=-51.748 max=4.5785 first=0.0 last=-51.748 sum=-855.36850133 nans=0)",
    R"(- col position_xy: min=0.0 max=51.748 first=0.0 last=51.748 sum=1779.2589333299998 nans=0)",
    R"(- col position_direction: min=-3.1416 max=3.1416 first=0.0 last=-3.1414 sum=-229.3492251390001 nans=0)",
    R"(- col velocity_xy: min=0.0 max=10.465 first=0.0 last=2.0818 sum=725.7145206000001 nans=0)",
    R"(- col acceleration_xy: min=0.0 max=14.261 first=0.0 last=0.030398 sum=571.0794387000002 nans=0)",
    R"(+ col position_y: min=-51.748 max=4.5785 first=0.0 last=-51.748 sum=-855.36850213 nans=0)",
    R"(+ col position_xy: min=0.0 max=51.748 first=0.0 last=51.748 sum=1779.2589341299997 nans=0)",
    R"(+ col position_direction: min=-3.1416 max=3.1416 first=0.0 last=-3.1414 sum=-229.3492253390001 nans=0)",
    R"(+ col velocity_xy: min=0.0 max=10.465 first=0.0 last=2.0818 sum=725.7145210000002 nans=0)",
    R"(+ col acceleration_xy: min=0.0 max=14.261 first=0.0 last=0.030398 sum=571.0794390000002 nans=0)",
    R"(- col aoa: min=7.1303E-5 max=2.7314 first=1.5708 last=NaN sum=198.89119165500003 nans=41)",
    R"(+ col aoa: min=7.1303E-5 max=2.7314 first=1.5708 last=NaN sum=198.89119325500005 nans=41)",
    R"(- col pitch_rate: min=-0.42335 max=0.86748 first=0.0 last=NaN sum=27.523879960000013 nans=41)",
    R"(+ col pitch_rate: min=-0.42335 max=0.86748 first=0.0 last=NaN sum=27.52388036000001 nans=41)",
    R"(- col longitudinal_inertia: min=0.0060543 max=0.0062119 first=0.0062119 last=NaN sum=1.3145995999999944 nans=41)",
    R"(- col rotational_inertia: min=0.0022621 max=0.0022636 first=0.0022636 last=NaN sum=0.48867459999999835 nans=41)",
    R"(+ col longitudinal_inertia: min=0.006054 max=0.006212 first=0.006212 last=NaN sum=1.3145580000000006 nans=41)",
    R"(+ col rotational_inertia: min=0.002262 max=0.002264 first=0.002264 last=NaN sum=0.4886659999999978 nans=41)",
    R"(- col drag_force: min=-0.15422 max=7.0551 first=0.0 last=0.71695 sum=269.68171136170895 nans=0)",
    R"(+ col drag_force: min=-0.15422 max=7.0551 first=0.0 last=0.71695 sum=269.68171036170895 nans=0)",
    R"(- col axial_drag_coeff: min=-2.9578 max=3.3408 first=0.0 last=NaN sum=176.9226418020003 nans=41)",
    R"(- col normal_force_coeff: min=0.0025795 max=21.315 first=NaN last=NaN sum=1307.7827882999998 nans=63)",
    R"(- col pitch_moment_coeff: min=-25.0 max=19.482 first=NaN last=NaN sum=370.1535299999998 nans=63)",
    R"(+ col axial_drag_coeff: min=-2.9578 max=3.3408 first=0.0 last=NaN sum=176.9226415020003 nans=41)",
    R"(+ col normal_force_coeff: min=0.00258 max=21.315 first=NaN last=NaN sum=1307.7827889999999 nans=63)",
    R"(+ col pitch_moment_coeff: min=-25.0 max=19.482 first=NaN last=NaN sum=370.1535289999998 nans=63)",
    R"(- col pitch_damping_moment_coeff: min=-0.12914 max=3.3393 first=NaN last=NaN sum=51.693369866133 nans=63)",
    R"(+ col pitch_damping_moment_coeff: min=-0.12914 max=3.3393 first=NaN last=NaN sum=51.693372266133004 nans=63)",
    R"(- col mach_number: min=0.0036939 max=0.2927 first=0.0063365 last=0.067286 sum=20.818522400000006 nans=0)",
    R"(+ col mach_number: min=0.003694 max=0.2927 first=0.006337 last=0.067286 sum=20.818526000000002 nans=0)",
    R"(- col time_step: min=0.0025 max=0.5 first=0.01 last=0.5 sum=18.69997149999999 nans=0)",
    R"(- col computation_time: min=0.0020274 max=0.2082 first=0.0020274 last=0.2082 sum=30.921807900000008 nans=0)",
    R"(- col coriolis_acceleration: min=0.0 max=0.010256 first=0.0 last=0.0023807 sum=0.7471284373000001 nans=0)",
    R"(- col Position parallel to wind: min=-0.009897 max=1.9394E-5 first=0.0 last=-0.009897 sum=-0.28784551612369996 nans=0)",
    R"(- col Propellant mass: min=0.0 max=0.021092 first=0.021092 last=0.0 sum=0.8771062789999997 nans=0)",
    R"(+ col time_step: min=0.0025 max=0.5 first=0.01 last=0.5 sum=18.69997199999999 nans=0)",
    R"(+ col computation_time: min=0.002027 max=0.2082 first=0.002027 last=0.2082 sum=30.921808000000006 nans=0)",
    R"(+ col coriolis_acceleration: min=0.0 max=0.010256 first=0.0 last=0.002381 sum=0.7471378373000005 nans=0)",
    R"(+ col Position parallel to wind: min=-0.009897 max=1.9394E-5 first=0.0 last=-0.009897 sum=-0.28784891612369984 nans=0)",
    R"(+ col Propellant mass: min=0.0 max=0.021092 first=0.021092 last=0.0 sum=0.8771062789999998 nans=0)",
});
inline constexpr auto kChanges14 = std::to_array<std::string_view>({
    R"(- version=108)",
    R"(+ version=111)",
    R"(- expressions=0 photo={advance=-0.030479999999999997, ambiance=0.5000000119209289, exhaustScale=1.0, flame=true, flameAspectRatio=1.2200000000000002, flameColor=255 100 50 255, fov=1.0, lightAlt=0.17453292519943295, lightAz=-2.6878070480712677, motionBlurred=false, pitch=0.5235987755982989, roll=2.2514747350726863, sky=, skyColor=136 136 136 255, smoke=false, smokeColor=230 230 230 102, smokeOpacity=0.4, sparkConcentration=0.2, sparkWeight=0.0, sparks=false, sunlight=255 255 255 255, viewAlt=-0.14239515377446402, viewAz=1.9384807492195624, viewDistance=0.24637999999999996, yaw=0.06981317007977318} decals=[])",
    R"(- events rocket=49)",
    R"(+ expressions=0 photo={advance=-0.030479999999999997, ambiance=0.5000000119209289, exhaustScale=1.0, flame=true, flameAspectRatio=1.2200000000000002, flameColor=255 100 50 255, fov=1.0, lightAlt=0.17453292519943295, lightAz=-2.6878070480712677, motionBlurred=false, pitch=0.5235987755982989, roll=2.2514747350726863, sky=, skyColor=136 136 136 255, smoke=false, smokeColor=230 230 230 102, sparkConcentration=0.2, sparkWeight=0.0, sparks=false, sunlight=255 255 255 255, viewAlt=-0.14239515377446402, viewAz=1.9384807492195624, viewDistance=0.24637999999999996, yaw=0.06981317007977318} decals=[])",
    R"(+ events rocket=50)",
});
inline constexpr auto kChanges15 = std::to_array<std::string_view>({
    R"(- version=108)",
    R"(+ version=111)",
    R"(- events rocket=101)",
    R"(+ events rocket=120)",
});
inline constexpr auto kChanges16 = std::to_array<std::string_view>({
    R"(- version=108)",
    R"(+ version=111)",
    R"(- events rocket=112)",
    R"(+ events rocket=131)",
});
inline constexpr auto kChanges17 = std::to_array<std::string_view>({
    R"(- version=109)",
    R"(+ version=111)",
    R"(- events rocket=155)",
    R"(+ events rocket=156)",
});
}  // namespace OpenRocketStateChanges

inline constexpr std::array<LegacyStateChanges, 18> kOpenRocketStateChanges{{
    {.file = "simplerocket.ork", .changes = OpenRocketStateChanges::kChanges0},
    {.file = "v1.0-roll-stabilized.ork", .changes = OpenRocketStateChanges::kChanges1},
    {.file = "v1.4-roll-stabilized.ork", .changes = OpenRocketStateChanges::kChanges2},
    {.file = "v1.5-preset-usage.ork", .changes = OpenRocketStateChanges::kChanges3},
    {.file = "v1.6-a-simple-model-rocket.ork", .changes = OpenRocketStateChanges::kChanges4},
    {.file = "v1.6-apocd.ork", .changes = OpenRocketStateChanges::kChanges5},
    {.file = "v1.6-boosted-dart.ork", .changes = OpenRocketStateChanges::kChanges6},
    {.file = "v1.6-high-power-airstart.ork", .changes = OpenRocketStateChanges::kChanges7},
    {.file = "v1.6-preset-usage-decals-first.ork", .changes = OpenRocketStateChanges::kChanges8},
    {.file = "v1.6-simulation-listeners.ork", .changes = OpenRocketStateChanges::kChanges9},
    {.file = "v1.6-tarc-payloader.ork", .changes = OpenRocketStateChanges::kChanges10},
    {.file = "v1.6-three-stage-rocket.ork", .changes = OpenRocketStateChanges::kChanges11},
    {.file = "v1.7-simulation-extensions-and-scripting.ork", .changes = OpenRocketStateChanges::kChanges12},
    {.file = "v1.7-tube-fin.ork", .changes = OpenRocketStateChanges::kChanges13},
    {.file = "v1.8-logo-rocket.ork", .changes = OpenRocketStateChanges::kChanges14},
    {.file = "v1.8-parallel-staging-example.ork", .changes = OpenRocketStateChanges::kChanges15},
    {.file = "v1.8-pods-example.ork", .changes = OpenRocketStateChanges::kChanges16},
    {.file = "v1.9-chute-release.ork", .changes = OpenRocketStateChanges::kChanges17},
}};
// clang-format on
// @@END GENERATED

/// A change of OpenRocket's round trip that QtRocket's makes otherwise (by a decision of the
/// user), with the reason.
struct OwnStateChange
{
    std::string_view file;
    /// OpenRocket's line of kOpenRocketStateChanges.
    std::string_view openRocket;
    /// QtRocket's in its place, or none: QtRocket's round trip does not change that line.
    std::string_view qtRocket;
    std::string_view why;
};

/// The preset references QtRocket keeps where OpenRocket drops them (decision U8 of 2026-10-08):
/// the save writes them back, and the reload finds no preset again and warns again.
constexpr std::string_view kWhyU8 =
    "the six presets of the tests do not hold the part: QtRocket keeps the reference, writes it "
    "back and warns again where OpenRocket dropped it at the first load (decision U8)";

constexpr std::array<OwnStateChange, 14> kOwnStateChanges{{
    {.file       = "v1.5-preset-usage.ork",
     .openRocket = "- W No matching ComponentPreset for component Nose cone found matching SEMROC "
                   "Astronautics BNC-55F",
     .qtRocket   = {},
     .why        = kWhyU8},
    {.file       = "v1.5-preset-usage.ork",
     .openRocket = "- W No matching ComponentPreset for component Body tube found matching SEMROC "
                   "Astronautics BT-55",
     .qtRocket   = {},
     .why        = kWhyU8},
    {.file       = "v1.5-preset-usage.ork",
     .openRocket = "- W No matching ComponentPreset for component Centering ring found matching "
                   "SEMROC Astronautics RA-5055",
     .qtRocket   = {},
     .why        = kWhyU8},
    {.file       = "v1.5-preset-usage.ork",
     .openRocket = "- W No matching ComponentPreset for component Inner Tube found matching SEMROC "
                   "Astronautics BT-50J",
     .qtRocket   = {},
     .why        = kWhyU8},
    {.file       = "v1.5-preset-usage.ork",
     .openRocket = "- W No matching ComponentPreset for component Launch lug found matching SEMROC "
                   "Astronautics LL-117",
     .qtRocket   = {},
     .why        = kWhyU8},
    {.file       = "v1.5-preset-usage.ork",
     .openRocket = "- W No matching ComponentPreset for component Parachute found matching SEMROC "
                   "Astronautics PN-18",
     .qtRocket   = {},
     .why        = kWhyU8},
    {.file       = "v1.6-preset-usage-decals-first.ork",
     .openRocket = "- W No matching ComponentPreset for component Nose cone found matching SEMROC "
                   "Astronautics BNC-55F",
     .qtRocket   = {},
     .why        = kWhyU8},
    {.file       = "v1.6-preset-usage-decals-first.ork",
     .openRocket = "- W No matching ComponentPreset for component Body tube found matching SEMROC "
                   "Astronautics BT-55",
     .qtRocket   = {},
     .why        = kWhyU8},
    {.file       = "v1.6-preset-usage-decals-first.ork",
     .openRocket = "- W No matching ComponentPreset for component Centering ring found matching "
                   "SEMROC Astronautics RA-5055",
     .qtRocket   = {},
     .why        = kWhyU8},
    {.file       = "v1.6-preset-usage-decals-first.ork",
     .openRocket = "- W No matching ComponentPreset for component Inner Tube found matching SEMROC "
                   "Astronautics BT-50J",
     .qtRocket   = {},
     .why        = kWhyU8},
    {.file       = "v1.6-preset-usage-decals-first.ork",
     .openRocket = "- W No matching ComponentPreset for component Launch lug found matching SEMROC "
                   "Astronautics LL-117",
     .qtRocket   = {},
     .why        = kWhyU8},
    {.file       = "v1.6-preset-usage-decals-first.ork",
     .openRocket = "- W No matching ComponentPreset for component Parachute found matching SEMROC "
                   "Astronautics PN-18",
     .qtRocket   = {},
     .why        = kWhyU8},
    {.file       = "v1.7-tube-fin.ork",
     .openRocket = "- W No matching ComponentPreset for component Body tube found matching "
                   "FlisKits BT-50-18",
     .qtRocket   = {},
     .why        = kWhyU8},
    {.file       = "v1.7-simulation-extensions-and-scripting.ork",
     .openRocket = "- W Simulation extension with id "
                   "'info.openrocket.core.simulation.extension.impl.AirStart' not found.",
     .qtRocket   = "- W Scripts that OpenRocket trusts have been disabled: QtRocket runs no "
                   "scripts.",
     .why        = "OpenRocket dropped the extension of the unknown id at the first load, so its "
                   "save has none and its reload does not warn; QtRocket keeps it (decision D11), "
                   "writes it back and warns again. The two scripts were disabled at the first "
                   "load with QtRocket's warning (decision D1) and are saved disabled, so the "
                   "reload does not warn; OpenRocket leaves them enabled, as it trusts them."},
}};

/// The lines of @p text, sorted.
[[nodiscard]] std::vector<std::string> sortedLinesOf(std::string_view text)
{
    std::vector<std::string> lines;
    for (const std::string_view line : QtRocket::Test::lineViewsOf(text))
    {
        lines.emplace_back(line);
    }
    std::ranges::sort(lines);
    return lines;
}

/// The row of kOpenRocketStateChanges of @p file, or null.
[[nodiscard]] const LegacyStateChanges* openRocketChangesOf(std::string_view file)
{
    for (const LegacyStateChanges& changes : kOpenRocketStateChanges)
    {
        if (changes.file == file)
        {
            return &changes;
        }
    }
    return nullptr;
}

/// The row of kOwnStateChanges of @p file for OpenRocket's change @p line, or null.
[[nodiscard]] const OwnStateChange* ownChangeOf(std::string_view file, std::string_view line)
{
    for (const OwnStateChange& change : kOwnStateChanges)
    {
        if (change.file == file && change.openRocket == line)
        {
            return &change;
        }
    }
    return nullptr;
}

/// The changes QtRocket's round trip makes to the state of @p file: OpenRocket's
/// (kOpenRocketStateChanges), with each line of kOwnStateChanges replaced by QtRocket's, or left
/// out where QtRocket has none; sorted. (The changes of two round trips are compared as sorted
/// lines: where a line is taken away and another added next to it, two longest common
/// subsequences can have them in either order.)
[[nodiscard]] std::vector<std::string> expectedChangesOf(std::string_view file)
{
    const LegacyStateChanges* const openRocket = openRocketChangesOf(file);
    if (openRocket == nullptr)
    {
        return {"(no changes of OpenRocket for the file)"};
    }
    std::vector<std::string> changes;
    for (const std::string_view line : openRocket->changes)
    {
        const OwnStateChange* const own  = ownChangeOf(file, line);
        const std::string_view      ours = own == nullptr ? line : own->qtRocket;
        if (!ours.empty())
        {
            changes.emplace_back(ours);
        }
    }
    std::ranges::sort(changes);
    return changes;
}

/// @p lines, one per line.
[[nodiscard]] std::string joined(const std::vector<std::string>& lines)
{
    std::string text;
    for (const std::string& line : lines)
    {
        text += line + "\n";
    }
    return text;
}

/// The row of RecordedOrkFiles.h of the design file @p file, or null.
[[nodiscard]] const RecordedOrkFile* recordedRowOf(std::string_view file)
{
    for (const RecordedOrkFile& row : kRecordedOrkFiles)
    {
        if (row.file == file)
        {
            return &row;
        }
    }
    return nullptr;
}

/// What the round trips of the legacy files compared and found.
struct LegacyRoundTrips
{
    int         files{0};       ///< the legacy files of the manifest
    int         archives{0};    ///< those saved as an archive
    int         plainSaves{0};  ///< those saved as plain XML, their archive failing as OpenRocket's
    std::size_t entries{0};     ///< the entries of the first archives
    std::size_t stateLines{0};  ///< the lines of the states of the reloads
    std::size_t changes{0};     ///< the lines of their changes
    std::string problems;       ///< what differs, by file; empty when nothing does
};

/// A first save of a design file: an archive, loaded back from memory, or a plain save, loaded
/// back from the file it is written to.
struct FirstSave
{
    std::vector<std::byte>               bytes;
    std::optional<std::filesystem::path> file;
};

/// The names of the entries of the archive @p bytes, in their order; the failure as one name.
[[nodiscard]] std::vector<std::string> entryNamesOf(std::span<const std::byte> bytes)
{
    const Result<ZipArchive> archive = ZipArchive::fromBytes(bytes);
    if (!archive)
    {
        return {archive.error().toString()};
    }
    std::vector<std::string> names;
    for (const ZipArchive::Entry& entry : archive->entries())
    {
        names.push_back(entry.name);
    }
    return names;
}

/// The plain XML of @p document with its simulation data, written with the preferences of
/// @p environment; nothing, with the problem in @p problems, when the save fails or warns.
[[nodiscard]] std::string plainSaveOf(std::string_view name, OpenRocketDocument& document,
                                      const DesignFileEnvironment& environment,
                                      std::string&                 problems)
{
    StorageOptions options;
    options.setSaveSimulationData(true);
    WarningSet                warnings;
    ErrorSet                  errors;
    const Result<std::string> text =
        OpenRocketSaver::save(document, options, environment.preferences(), warnings, errors);
    if (!text || !warnings.empty())
    {
        problems += std::format("{}: the plain save {}\n", name,
                                text ? "warned" : "failed: " + text.error().toString());
        return {};
    }
    return *text;
}

/// Whether the archive of @p input failed as OpenRocket's did (its archiveError: the
/// decal-not-found error, for the decal of that name) in the save of @p saver, which gave
/// @p archive; else the problem in @p trips.
[[nodiscard]] bool failedAsOpenRocketsArchive(const ResaveInput&                    input,
                                              const GeneralRocketSaver&             saver,
                                              const Result<std::vector<std::byte>>& archive,
                                              LegacyRoundTrips&                     trips)
{
    // OpenRocket's DecalNotFoundException, by the name of its decal.
    constexpr std::string_view kDecalNotFound = "DecalNotFoundException: ";
    const std::string_view     error          = input.archiveError;
    const bool failed = !archive.has_value() && archive.error().code == ErrorCode::NOT_FOUND &&
                        saver.getMissingDecal() != nullptr && error.starts_with(kDecalNotFound) &&
                        saver.getMissingDecal()->getName() == error.substr(kDecalNotFound.size());
    if (!failed)
    {
        trips.problems +=
            std::format("{}: the archive {}, where OpenRocket's gave {}\n", input.name,
                        archive ? "was saved" : "failed: " + archive.error().toString(),
                        error.empty() ? std::string_view("its entries") : error);
    }
    return failed;
}

/// The first save of @p document, the design of @p input as loaded, with its simulation data: the
/// .ork archive (whose entries are to be OpenRocket's), or, for the design whose archive
/// OpenRocket cannot save, the plain XML written to a file of the design's name in @p directory,
/// after the archive failed as OpenRocket's does. Nothing, with the problem in @p trips, when
/// something else happens.
[[nodiscard]] FirstSave firstSaveOf(const ResaveInput& input, OpenRocketDocument& document,
                                    const DesignFileEnvironment& environment,
                                    const TempDir& directory, LegacyRoundTrips& trips)
{
    StorageOptions options;
    options.setSaveSimulationData(true);
    GeneralRocketSaver             saver(environment.preferences());
    Result<std::vector<std::byte>> archive = saver.saveToBytes(document, options);
    if (archive && input.archiveError.empty())
    {
        trips.archives++;
        if (entryNamesOf(*archive) != input.archiveEntries || !saver.getWarnings().empty())
        {
            trips.problems += std::format(
                "{}: the archive's entries are not OpenRocket's, or the save warned\n", input.name);
        }
        return {.bytes = std::move(*archive), .file = std::nullopt};
    }
    if (!failedAsOpenRocketsArchive(input, saver, archive, trips))
    {
        return {};
    }
    trips.plainSaves++;
    const std::string text = plainSaveOf(input.name, document, environment, trips.problems);
    if (text.empty())
    {
        return {};
    }
    return {.bytes = {},
            .file  = directory.write(QtRocket::Test::legacyFileOf(input).filename(), text)};
}

/// @p save loaded back in @p environment, from memory or from its file.
[[nodiscard]] Result<LoadedDocument> reloadOf(const FirstSave&             save,
                                              const DesignFileEnvironment& environment)
{
    const GeneralRocketLoader loader(environment.context());
    return save.file ? loader.load(*save.file) : loader.load(save.bytes);
}

/// The second save of @p save, the first save of the design of @p input, loaded back in an
/// environment of its own and saved as the first save was (an archive, or plain XML); what
/// differs from the first save (archiveChanges(), or the lines of the plain XML), or why there is
/// no second save.
[[nodiscard]] std::string secondSaveChanges(const ResaveInput& input, const FirstSave& save,
                                            LegacyRoundTrips& trips)
{
    const DesignFileEnvironment  environment;
    const Result<LoadedDocument> reload = reloadOf(save, environment);
    if (!reload)
    {
        return "the reload failed: " + reload.error().toString() + "\n";
    }
    if (save.file)
    {
        std::string       problems;
        const std::string second =
            plainSaveOf(input.name, *reload->document, environment, problems);
        const Result<std::string> first = QtRocket::readTextFile(*save.file);
        return problems + (first ? lineChanges(*first, second) : first.error().toString() + "\n");
    }
    StorageOptions options;
    options.setSaveSimulationData(true);
    GeneralRocketSaver                   saver(environment.preferences());
    const Result<std::vector<std::byte>> second = saver.saveToBytes(*reload->document, options);
    return second ? archiveChanges(save.bytes, *second, trips.entries)
                  : "the second save failed: " + second.error().toString() + "\n";
}

/// The state of @p save loaded back in an environment of its own (designBytesState(), or
/// designFileState() for a plain save), the ids of flight configurations told apart by those of
/// @p original, the original design's document.
[[nodiscard]] std::vector<std::string> stateOfTheReload(const FirstSave& save,
                                                        std::string_view original)
{
    DesignFileEnvironment environment;
    return save.file ? designFileState(environment, *save.file, StateDetail::RECORDED, original)
                     : designBytesState(environment, save.bytes, StateDetail::RECORDED, nullptr,
                                        original);
}

/// The round trip of the legacy file of @p input (see the top of this file), with @p directory
/// for a plain save; counted into @p trips.
void roundTripTheFile(const ResaveInput& input, const TempDir& directory, LegacyRoundTrips& trips)
{
    const std::filesystem::path  file = QtRocket::Test::legacyFileOf(input);
    const RecordedOrkFile* const row  = recordedRowOf(file.filename().string());
    const DesignFileEnvironment  environment;
    const GeneralRocketLoader    loader(environment.context());
    const Result<LoadedDocument> loaded = loader.load(file);
    if (row == nullptr || !loaded)
    {
        trips.problems += std::format("{}: no row in RecordedOrkFiles.h, or no load\n", input.name);
        return;
    }
    trips.files++;
    const FirstSave save = firstSaveOf(input, *loaded->document, environment, directory, trips);
    if (save.bytes.empty() && !save.file)
    {
        return;
    }
    const std::string second = secondSaveChanges(input, save, trips);
    if (!second.empty())
    {
        trips.problems +=
            std::format("{}: the save of the reload is not the first save\n{}", input.name, second);
    }
    const std::vector<std::string> state =
        stateOfTheReload(save, QtRocket::Test::documentOfDesignFile(file));
    const std::vector<std::string_view> found(state.begin(), state.end());
    const std::string                   changes = lineChanges(row->state, found);
    trips.stateLines += state.size();
    trips.changes += QtRocket::Test::lineViewsOf(changes).size();
    const std::vector<std::string> expected = expectedChangesOf(row->file);
    if (sortedLinesOf(changes) != expected)
    {
        trips.problems +=
            std::format("{}: the state of the reload changes\n{}where it is to change (sorted)\n{}",
                        input.name, changes, joined(expected));
    }
}

/// roundTripTheFile() of each legacy file of the manifest, with @p directory for a plain save.
[[nodiscard]] LegacyRoundTrips roundTripTheFiles(const TempDir& directory)
{
    LegacyRoundTrips trips;
    for (const ResaveInput& input :
         QtRocket::Test::resaveInputsOf(QtRocket::Test::kLegacyKind, trips.problems))
    {
        roundTripTheFile(input, directory, trips);
    }
    return trips;
}

// Each of the eighteen design files of tests/data/ork, saved with its simulation data as an
// archive (as plain XML where OpenRocket's archive fails too), comes back in the state of the
// file as loaded but for the changes of OpenRocket's own round trip and QtRocket's own, and a
// save of the reload is the first save (see the top of this file).
TEST(LegacyOrkRoundTripGolden, TheLegacyFilesComeBackAsOpenRocketsDo)
{
    const TempDir          directory;
    const LegacyRoundTrips trips = roundTripTheFiles(directory);
    EXPECT_EQ(trips.problems, "");
    EXPECT_EQ(trips.files, 18);
    EXPECT_EQ(trips.archives, 17);
    EXPECT_EQ(trips.plainSaves, 1);
    EXPECT_EQ(trips.entries, 55U);
    EXPECT_EQ(trips.stateLines, 1392U);
    EXPECT_EQ(trips.changes, 212U);
}

/// What is wrong with the rows of kOwnStateChanges, one line for each fault; empty when every
/// row names a file of the table and a line of OpenRocket's changes of that file, has its reason
/// and differs from OpenRocket's line.
[[nodiscard]] std::string faultsOfTheOwnChanges()
{
    std::string faults;
    for (const OwnStateChange& own : kOwnStateChanges)
    {
        const LegacyStateChanges* const openRocket = openRocketChangesOf(own.file);
        if (openRocket == nullptr ||
            std::ranges::find(openRocket->changes, own.openRocket) == openRocket->changes.end())
        {
            faults += std::format("{}: not a change of OpenRocket: {}\n", own.file, own.openRocket);
        }
        if (own.why.empty() || own.openRocket == own.qtRocket)
        {
            faults += std::format("{}: a row without a reason, or OpenRocket's line\n", own.file);
        }
    }
    return faults;
}

// Every change of OpenRocket's that QtRocket makes otherwise is one of OpenRocket's changes and
// has its reason; kOpenRocketStateChanges has a row for each of the eighteen files.
TEST(LegacyOrkRoundTripGolden, QtRocketsOwnChangesAreOpenRocketsChangesWithAReason)
{
    EXPECT_EQ(faultsOfTheOwnChanges(), "");
    EXPECT_EQ(kOpenRocketStateChanges.size(), kRecordedOrkFiles.size());
}

}  // namespace
