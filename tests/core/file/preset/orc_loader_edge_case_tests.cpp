// The edge cases of the probe tools/openrocket-probes/orc-loader (OrcEdgeProbe, its expected.txt)
// replayed through OpenRocketComponentLoader: for every one of the 337 documents, the presets
// QtRocket makes, each written as the probe writes the presets OpenRocket makes
// (OrcProbeTranscript.h) and pinned from the probe's transcript, or the refusal of the file and the
// warnings. A case whose comment names no difference gives exactly what OpenRocket gives. Where
// OpenRocket throws, the comment gives its exception: an IOException refuses the file there too,
// and any other one (a RuntimeException of a DTO) ends OpenRocket's whole preset load, the user's
// files included, where QtRocket refuses the one file (bug 18). The other differences are the
// deviations OpenRocketComponentLoader.h documents: no <PartNumber> refuses the file and a
// <LineCount> that is no integer skips the preset (D5), a number that is not finite skips it (bug
// 19), and those of ComponentPresetFactory (the recovery devices' material types) and of XmlScanner
// (a document type declaration that declares an entity).
//
// orc_edge_case_tests.cpp pins the same documents one level down (OrcDocument, OrcElement,
// JaxbLexical).

#include <cstddef>
#include <initializer_list>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/file/preset/OpenRocketComponentLoader.h"
#include "QtRocket/logging/Warning.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/material/BuiltinMaterials.h"
#include "QtRocket/material/MaterialStorage.h"
#include "QtRocket/util/Error.h"
#include "file/preset/OrcEdgeCases.h"
#include "file/preset/OrcProbeTranscript.h"

namespace
{

using QtRocket::OpenRocketComponentLoader;

/// What the loader makes of one case of the probe, read as "case.orc" (the probe's file name).
struct LoaderCase
{
    /// The case (OrcEdgeCase::id).
    std::string_view id;
    /// The probe's lines of the presets, in order (OrcProbeTranscript.h).
    std::vector<std::string_view> presets;
    /// "<ErrorCode>: <message>" of the refusal of the file, the message escaped as the probe
    /// escapes it; empty when the file is read.
    std::string_view refusal;
    /// The texts of the warnings, escaped as the probe escapes them.
    std::vector<std::string_view> warnings;
};

/// A case whose file is read: its presets and its warnings.
[[nodiscard]] LoaderCase read(std::string_view id, std::vector<std::string_view> presets,
                              std::vector<std::string_view> warnings = {})
{
    return {
        .id = id, .presets = std::move(presets), .refusal = {}, .warnings = std::move(warnings)};
}

/// A case whose file is refused.
[[nodiscard]] LoaderCase refused(std::string_view id, std::string_view refusal)
{
    return {.id = id, .presets = {}, .refusal = refusal, .warnings = {}};
}

// clang-format off
/// The probe's body tube (case 01): Cardboard from the section, 0.95, 0.976 and 18 in.
constexpr std::string_view kTube =
    "Legacy=false; Manufacturer=Estes; PartNo=BT-X; Description=d; Material=[Bulk 'Cardboard' 680.0 shear 4.0E8 PaperProducts builtin]; InnerDiameter=0.02413; OuterDiameter=0.024790399999999997; Length=0.4572; Type=BODY_TUBE; Thickness=3.3019999999999924E-4; digest=72e2ef64f8f85e6788d7c9919d4ca7d2";
/// The probe's bulkhead without Filled.
constexpr std::string_view kBulkhead =
    "Legacy=false; Manufacturer=Estes; PartNo=BH; OuterDiameter=0.02; Length=0.01; Type=BULK_HEAD; digest=afb643882af5d920147e5096ea3f10a3";
/// The body tube with a Length JAXB does not read: 0.0.
constexpr std::string_view kTubeOfLength0 =
    "Legacy=false; Manufacturer=Estes; PartNo=BT-X; Description=d; Material=[Bulk 'Cardboard' 680.0 shear 4.0E8 PaperProducts builtin]; InnerDiameter=0.02413; OuterDiameter=0.024790399999999997; Length=0.0; Type=BODY_TUBE; Thickness=3.3019999999999924E-4; digest=6f55dff0a66120b43013d935550d14b9";
/// The bulkhead with Filled false.
constexpr std::string_view kBulkheadNotFilled =
    "Legacy=false; Manufacturer=Estes; PartNo=BH; Filled=false; OuterDiameter=0.02; Length=0.01; Type=BULK_HEAD; digest=a22ee1904a8ff6c2690f8199cb0a2421";
/// The bulkhead with Filled true.
constexpr std::string_view kBulkheadFilled =
    "Legacy=false; Manufacturer=Estes; PartNo=BH; Filled=true; OuterDiameter=0.02; Length=0.01; Type=BULK_HEAD; digest=8ba07ee49dacf916529b8367daff00bd";
/// The body tube with a Length of 18 m (no Unit, or Unit="m").
constexpr std::string_view kTubeOf18Metres =
    "Legacy=false; Manufacturer=Estes; PartNo=BT-X; Description=d; Material=[Bulk 'Cardboard' 680.0 shear 4.0E8 PaperProducts builtin]; InnerDiameter=0.02413; OuterDiameter=0.024790399999999997; Length=18.0; Type=BODY_TUBE; Thickness=3.3019999999999924E-4; digest=118972bc33c03b817e2567ce3d4aabf4";
/// The parachute of batch 2 with 6 Sides.
constexpr std::string_view kChuteOf6Sides =
    "Legacy=false; Manufacturer=Estes; PartNo=PK; Material=[Surface 'Ripstop nylon' 0.067 shear 0.0 Fabrics builtin]; Type=PARACHUTE; Diameter=0.3; LineCount=6; LineLength=0.3; Sides=6; digest=64105d09f4e36a62435ff424e47da97f";
/// The parachute with Sides null.
constexpr std::string_view kChuteWithoutSides =
    "Legacy=false; Manufacturer=Estes; PartNo=PK; Material=[Surface 'Ripstop nylon' 0.067 shear 0.0 Fabrics builtin]; Type=PARACHUTE; Diameter=0.3; LineCount=6; LineLength=0.3; digest=95bb232c226d7ab682eb7e48623e2ca7";
/// The parachute with 0 Sides.
constexpr std::string_view kChuteOf0Sides =
    "Legacy=false; Manufacturer=Estes; PartNo=PK; Material=[Surface 'Ripstop nylon' 0.067 shear 0.0 Fabrics builtin]; Type=PARACHUTE; Diameter=0.3; LineCount=6; LineLength=0.3; Sides=0; digest=64105d09f4e36a62435ff424e47da97f";
/// The parachute with -6 Sides.
constexpr std::string_view kChuteOfMinus6Sides =
    "Legacy=false; Manufacturer=Estes; PartNo=PK; Material=[Surface 'Ripstop nylon' 0.067 shear 0.0 Fabrics builtin]; Type=PARACHUTE; Diameter=0.3; LineCount=6; LineLength=0.3; Sides=-6; digest=64105d09f4e36a62435ff424e47da97f";
/// The nose cone with an OGIVE Shape.
constexpr std::string_view kOgiveNoseCone =
    "Legacy=false; Manufacturer=Estes; PartNo=NC; Material=[Bulk 'Cardboard' 680.0 shear 4.0E8 PaperProducts builtin]; Shape=Ogive; AftOuterDiameter=0.02; AftShoulderLength=0.01; AftShoulderDiameter=0.019; Length=0.05; Type=NOSE_CONE; digest=193af28d454e03cdd20ef59d9510f0b4";
/// The body tube with an empty Description.
constexpr std::string_view kTubeWithEmptyDescription =
    "Legacy=false; Manufacturer=Estes; PartNo=BT-X; Description=; Material=[Bulk 'Cardboard' 680.0 shear 4.0E8 PaperProducts builtin]; InnerDiameter=0.02413; OuterDiameter=0.024790399999999997; Length=0.4572; Type=BODY_TUBE; Thickness=3.3019999999999924E-4; digest=36f0262d8a3bf80f30a5ea3aca49bfdc";
/// The body tube with an empty Thumbnail image.
constexpr std::string_view kTubeWithEmptyImage =
    "Legacy=false; Manufacturer=Estes; PartNo=BT-X; Description=d; Material=[Bulk 'Cardboard' 680.0 shear 4.0E8 PaperProducts builtin]; Image=bytes0:; InnerDiameter=0.02413; OuterDiameter=0.024790399999999997; Length=0.4572; Type=BODY_TUBE; Thickness=3.3019999999999924E-4; digest=88ddbe03a488913456ab8159a7674a61";
/// The body tube whose section Cardboard has a ShearModulus of 1e9.
constexpr std::string_view kTubeOfShearModulus1e9 =
    "Legacy=false; Manufacturer=Estes; PartNo=BT-X; Description=d; Material=[Bulk 'Cardboard' 680.0 shear 1.0E9 Other user]; InnerDiameter=0.02413; OuterDiameter=0.024790399999999997; Length=0.4572; Type=BODY_TUBE; Thickness=3.3019999999999924E-4; digest=72e2ef64f8f85e6788d7c9919d4ca7d2";
/// The body tube with the image of the one byte 41.
constexpr std::string_view kTubeWithImage41 =
    "Legacy=false; Manufacturer=Estes; PartNo=BT-X; Description=d; Material=[Bulk 'Cardboard' 680.0 shear 4.0E8 PaperProducts builtin]; Image=bytes1:41; InnerDiameter=0.02413; OuterDiameter=0.024790399999999997; Length=0.4572; Type=BODY_TUBE; Thickness=3.3019999999999924E-4; digest=88ddbe03a488913456ab8159a7674a61";
/// The body tube whose Cardboard is a new material of density 0.
constexpr std::string_view kTubeOfNewCardboard =
    "Legacy=false; Manufacturer=Estes; PartNo=BT-X; Description=d; Material=[Bulk 'Cardboard' 0.0 shear 0.0 Other user]; InnerDiameter=0.02413; OuterDiameter=0.024790399999999997; Length=0.4572; Type=BODY_TUBE; Thickness=3.3019999999999924E-4; digest=3bf52f38f0e90551382cc4f1ab1ed1f7";

[[nodiscard]] const std::vector<LoaderCase>& batch1StructureCases()
{
    static const std::vector<LoaderCase> kCases{
        // 01 baseline
        read("01", {kTube}),
        // 02 unknown element in component
        read("02", {kTube}),
        // 03 unknown component kind
        read("03", {kTube}),
        // 04 missing Length: OpenRocket: NullPointerException "Cannot invoke "info.openrocket.core.preset.xml.BaseComponentDTO$AnnotatedLengthDTO.getValue()" because "this.length" is null", which ends its whole preset load
        refused("04", "PARSE: Unable to parse component preset file: case.orc: the BodyTube 'BT-X' (component 1) has no <Length>"),
        // 05 missing Manufacturer: OpenRocket: NullPointerException "Cannot invoke "String.toLowerCase(java.util.Locale)" because "str" is null", which ends its whole preset load
        refused("05", "PARSE: Unable to parse component preset file: case.orc: the BodyTube 'BT-X' (component 1) has no <Manufacturer>"),
        // 06 missing PartNumber: deviation D5: OpenRocket makes the preset with PartNo=<null>
        refused("06", "PARSE: Unable to parse component preset file: case.orc: the BodyTube (component 1) has no <PartNumber>"),
        // 07 bad number abc
        read("07", {kTubeOfLength0}),
        // 08 empty number
        read("08", {kTubeOfLength0}),
        // 09 number with spaces
        read("09", {kTube}),
        // 10 number 1.8e1
        read("10", {kTube}),
        // 11 number +18
        read("11", {kTube}),
        // 12 number NaN: bug 19: OpenRocket loads the preset with that Length
        read("12", {}, {"Component preset file case.orc: the BodyTube 'BT-X' (component 1) was skipped: its <Length> is not a finite number (NaN)"}),
        // 13 number INF: bug 19: OpenRocket loads the preset with that Length
        read("13", {}, {"Component preset file case.orc: the BodyTube 'BT-X' (component 1) was skipped: its <Length> is not a finite number (Infinity)"}),
        // 14 number Infinity
        read("14", {kTubeOfLength0}),
        // 15 number 18d
        read("15", {kTubeOfLength0}),
        // 16 number 0x12p0
        read("16", {kTube}),
        // 17 number 18,0
        read("17", {kTubeOfLength0}),
        // 18 number .5
        read("18", {"Legacy=false; Manufacturer=Estes; PartNo=BT-X; Description=d; Material=[Bulk 'Cardboard' 680.0 shear 4.0E8 PaperProducts builtin]; InnerDiameter=0.02413; OuterDiameter=0.024790399999999997; Length=0.0127; Type=BODY_TUBE; Thickness=3.3019999999999924E-4; digest=a75959542b6d25a662703a42e6bd996d"}),
        // 19 number -0
        read("19", {"Legacy=false; Manufacturer=Estes; PartNo=BT-X; Description=d; Material=[Bulk 'Cardboard' 680.0 shear 4.0E8 PaperProducts builtin]; InnerDiameter=0.02413; OuterDiameter=0.024790399999999997; Length=-0.0; Type=BODY_TUBE; Thickness=3.3019999999999924E-4; digest=d5dc9346e19ce2131dcf0b4e40f9c766"}),
        // 20 unknown Unit: OpenRocket: IllegalArgumentException "name=furlong", which ends its whole preset load
        refused("20", "PARSE: Unable to parse component preset file: case.orc: the BodyTube 'BT-X' (component 1) gives its <Length> in the unknown unit 'furlong'"),
        // 21 no Unit attribute
        read("21", {kTubeOf18Metres}),
        // 22 Unit in/64
        read("22", {kTube}),
        // 23 Unit ft
        read("23", {"Legacy=false; Manufacturer=Estes; PartNo=BT-X; Description=d; Material=[Bulk 'Cardboard' 680.0 shear 4.0E8 PaperProducts builtin]; InnerDiameter=0.02413; OuterDiameter=0.024790399999999997; Length=5.486400000000001; Type=BODY_TUBE; Thickness=3.3019999999999924E-4; digest=d2668ba2878241eb834e83dcf1d86460"}),
        // 24 Unit with space: OpenRocket: IllegalArgumentException "name= in", which ends its whole preset load
        refused("24", "PARSE: Unable to parse component preset file: case.orc: the BodyTube 'BT-X' (component 1) gives its <Length> in the unknown unit ' in'"),
        // 25 two Length elements
        read("25", {"Legacy=false; Manufacturer=Estes; PartNo=BT-X; Description=d; Material=[Bulk 'Cardboard' 680.0 shear 4.0E8 PaperProducts builtin]; InnerDiameter=0.02413; OuterDiameter=0.024790399999999997; Length=0.508; Type=BODY_TUBE; Thickness=3.3019999999999924E-4; digest=805b055d1f5476804dbf23bc298f8214"}),
        // 26 material not in section
        read("26", {"Legacy=false; Manufacturer=Estes; PartNo=BT-X; Description=d; Material=[Bulk 'Unobtainium' 0.0 shear 0.0 Other user]; InnerDiameter=0.02413; OuterDiameter=0.024790399999999997; Length=0.4572; Type=BODY_TUBE; Thickness=3.3019999999999924E-4; digest=3bf52f38f0e90551382cc4f1ab1ed1f7"}),
        // 27 material bad Type: OpenRocket: IllegalArgumentException "Invalid material type FOO specified for Component", which ends its whole preset load
        refused("27", "PARSE: Unable to parse component preset file: case.orc: the BodyTube 'BT-X' (component 1) has a <Material> of the Type 'FOO', which is none of BULK, SURFACE and LINE"),
        // 28 material no Type: OpenRocket: IllegalArgumentException "Invalid material type null specified for Component", which ends its whole preset load
        refused("28", "PARSE: Unable to parse component preset file: case.orc: the BodyTube 'BT-X' (component 1) has a <Material> without a Type"),
        // 29 material g/cm3 section
        read("29", {"Legacy=false; Manufacturer=Estes; PartNo=BT-X; Description=d; Material=[Bulk 'Paper test' 800.0 shear 0.0 Other user]; InnerDiameter=0.02413; OuterDiameter=0.024790399999999997; Length=0.4572; Type=BODY_TUBE; Thickness=3.3019999999999924E-4; digest=f9abefe80b9ef1eb37a60c38b0728689"}),
        // 30 material name with spaces
        read("30", {kTube}),
        // 31 no Material
        read("31", {"Legacy=false; Manufacturer=Estes; PartNo=BT-X; Description=d; InnerDiameter=0.02413; OuterDiameter=0.024790399999999997; Length=0.4572; Type=BODY_TUBE; Thickness=3.3019999999999924E-4; digest=dc56ba2a478295eecb6dbefb922099ab"}),
        // 32 Mass in g
        read("32", {"Legacy=false; Manufacturer=Estes; PartNo=BT-X; Description=d; Material=[Bulk 'Cardboard' 861.9991407417643 shear 0.0 Custom user]; Mass=0.01; InnerDiameter=0.02413; OuterDiameter=0.024790399999999997; Length=0.4572; Type=BODY_TUBE; Thickness=3.3019999999999924E-4; digest=7117d447de7276b615c78c205225181c"}),
        // 33 Mass unknown unit: OpenRocket: IllegalArgumentException "name=stone", which ends its whole preset load
        refused("33", "PARSE: Unable to parse component preset file: case.orc: the BodyTube 'BT-X' (component 1) gives its <Mass> in the unknown unit 'stone'"),
        // 34 wrong root: OpenRocket: IOException "Unable to parse component preset file: case.orc (invalid format or corrupted file)"
        refused("34", "PARSE: Unable to parse component preset file: case.orc (invalid format or corrupted file): the root element is <Foo>, not <OpenRocketComponent>"),
        // 35 malformed: OpenRocket: IOException "Unable to parse component preset file: case.orc (invalid format or corrupted file)"
        refused("35", "PARSE: Unable to parse component preset file: case.orc (invalid format or corrupted file): XML document structures must start and end within the same entity."),
        // 36 empty: OpenRocket: IOException "Unable to parse component preset file: case.orc (invalid format or corrupted file)"
        refused("36", "PARSE: Unable to parse component preset file: case.orc (invalid format or corrupted file): Premature end of file."),
        // 37 namespace root: OpenRocket: IOException "Unable to parse component preset file: case.orc (invalid format or corrupted file)"
        refused("37", "PARSE: Unable to parse component preset file: case.orc (invalid format or corrupted file): the root element <OpenRocketComponent> is in the namespace 'urn:x'"),
        // 38 no Version
        read("38", {kTube}),
        // 39 Version 9.9
        read("39", {kTube}),
        // 40 Legacy element
        read("40", {"Legacy=true; Manufacturer=Estes; PartNo=BT-X; Description=d; Material=[Bulk 'Cardboard' 680.0 shear 4.0E8 PaperProducts builtin]; InnerDiameter=0.02413; OuterDiameter=0.024790399999999997; Length=0.4572; Type=BODY_TUBE; Thickness=3.3019999999999924E-4; digest=72e2ef64f8f85e6788d7c9919d4ca7d2"}),
        // 41 no Components
        read("41", {}),
        // 42 components outside wrapper
        read("42", {}),
        // 43 two Components wrappers
        read("43", {"Legacy=false; Manufacturer=Estes; PartNo=BT-Y; Description=d; Material=[Bulk 'Cardboard' 680.0 shear 4.0E8 PaperProducts builtin]; InnerDiameter=0.02413; OuterDiameter=0.024790399999999997; Length=0.4572; Type=BODY_TUBE; Thickness=3.3019999999999924E-4; digest=7267980a8c6635ef22ab5b5063eef2f4"}),
        // 44 material section bad uom: OpenRocket: IOException "Unable to parse component preset file: case.orc (invalid format or corrupted file)"
        refused("44", "PARSE: Unable to parse component preset file: case.orc (invalid format or corrupted file): material 1 of <Materials> gives its density in the unknown unit 'lb/in\u00B3'"),
        // 45 material section bad density
        read("45", {kTubeOfNewCardboard}),
        // 46 material section no uom
        read("46", {kTube}),
        // 47 material section Group
        read("47", {kTube}),
        // 48 material section bad Group
        read("48", {kTube}),
        // 49 material section bad Type: OpenRocket: NullPointerException "Cannot invoke "info.openrocket.core.preset.xml.MaterialTypeDTO.name()" because the return value of "info.openrocket.core.preset.xml.MaterialDTO.getType()" is null", which ends its whole preset load
        refused("49", "PARSE: Unable to parse component preset file: case.orc: the BodyTube 'BT-X' (component 1) has a <Material>, but material 1 of <Materials> has no <Type> that is LINE, SURFACE or BULK"),
        // 50 Filled yes
        read("50", {kBulkhead}),
        // 51 Filled 1
        read("51", {kBulkheadFilled}),
        // 52 Filled TRUE
        read("52", {kBulkhead}),
        // 53 parachute baseline
        read("53", {"Legacy=false; Manufacturer=Estes; PartNo=PK; Description=chute; Material=[Surface 'Ripstop nylon' 0.067 shear 0.0 Fabrics builtin]; Type=PARACHUTE; Diameter=0.30479999999999996; LineCount=6; LineLength=0.30479999999999996; Sides=6; LineMaterial=[Line 'Carpet Thread' 0.0 shear 0.0 Other user]; digest=a657bb6f2c87ca026839fe4f148dbb64"}),
        // 54 parachute no LineCount: deviation D5: OpenRocket makes the preset with LineCount=<null>
        read("54", {}, {"Component preset file case.orc: the Parachute 'PK' (component 1) was skipped: it has no <LineCount> that is an integer"}),
        // 55 parachute LineCount 6.0: deviation D5: OpenRocket makes the preset with LineCount=<null>
        read("55", {}, {"Component preset file case.orc: the Parachute 'PK' (component 1) was skipped: it has no <LineCount> that is an integer"}),
        // 56 parachute LineCount abc: deviation D5: OpenRocket makes the preset with LineCount=<null>
        read("56", {}, {"Component preset file case.orc: the Parachute 'PK' (component 1) was skipped: it has no <LineCount> that is an integer"}),
        // 57 parachute no LineLength: OpenRocket: IOException "Unable to parse component preset file: case.orc" (InvalidComponentPresetException: Invalid preset specification.)
        refused("57", "PARSE: Unable to parse component preset file: case.orc: the Parachute 'PK' (component 1) is not a valid preset: Invalid preset specification. No LineLength specified"),
        // 58 parachute DragCoefficient Unit in
        read("58", {"Legacy=false; Manufacturer=Estes; PartNo=PK; Description=chute; Material=[Surface 'Ripstop nylon' 0.067 shear 0.0 Fabrics builtin]; Type=PARACHUTE; Diameter=0.30479999999999996; DragCoefficient=0.02032; LineCount=6; LineLength=0.30479999999999996; Sides=6; LineMaterial=[Line 'Carpet Thread' 0.0 shear 0.0 Other user]; digest=b43376e5721121aad391c4d12f51338c"}),
        // 59 parachute line material BULK: ComponentPresetFactory's deviation (a recovery device's materials must be of their types): OpenRocket loads it
        refused("59", "PARSE: Unable to parse component preset file: case.orc: the Parachute 'PK' (component 1) is not a valid preset: Invalid preset specification. LineMaterial \"Cardboard\" is not a LINE material"),
        // 60 parachute canopy BULK: ComponentPresetFactory's deviation (a recovery device's materials must be of their types): OpenRocket loads it
        refused("60", "PARSE: Unable to parse component preset file: case.orc: the Parachute 'PK' (component 1) is not a valid preset: Invalid preset specification. Material \"Cardboard\" is not a SURFACE material"),
        // 61 nose cone bad shape: OpenRocket: NullPointerException "Cannot invoke "info.openrocket.core.preset.xml.ShapeDTO.getORShape()" because "this.shape" is null", which ends its whole preset load
        refused("61", "PARSE: Unable to parse component preset file: case.orc: the NoseCone 'NC' (component 1) has the <Shape> 'BLUNT', which is none of CONICAL, OGIVE, ELLIPSOID, POWER, PARABOLIC and HAACK"),
        // 62 nose cone shape lowercase: OpenRocket: NullPointerException "Cannot invoke "info.openrocket.core.preset.xml.ShapeDTO.getORShape()" because "this.shape" is null", which ends its whole preset load
        refused("62", "PARSE: Unable to parse component preset file: case.orc: the NoseCone 'NC' (component 1) has the <Shape> 'ogive', which is none of CONICAL, OGIVE, ELLIPSOID, POWER, PARABOLIC and HAACK"),
        // 63 nose cone no shoulder
        read("63", {"Legacy=false; Manufacturer=Estes; PartNo=NC; Material=[Bulk 'Cardboard' 2738.3331024360555 shear 0.0 Custom user]; Mass=0.01; Shape=Ogive; AftOuterDiameter=0.02; Length=0.05; Type=NOSE_CONE; digest=fe42923918d613480bf01b5973bfe520"}),
        // 64 transition no fore shoulder: OpenRocket: NullPointerException "Cannot invoke "info.openrocket.core.preset.xml.BaseComponentDTO$AnnotatedLengthDTO.getValue()" because "this.foreShoulderDiameter" is null", which ends its whole preset load
        refused("64", "PARSE: Unable to parse component preset file: case.orc: the Transition 'TR' (component 1) has no <ForeShoulderDiameter>"),
        // 65 streamer no thickness: OpenRocket: NullPointerException "Cannot invoke "info.openrocket.core.preset.xml.BaseComponentDTO$AnnotatedLengthDTO.getValue()" because "this.thickness" is null", which ends its whole preset load
        refused("65", "PARSE: Unable to parse component preset file: case.orc: the Streamer 'ST' (component 1) has no <Thickness>"),
        // 66 rail button no NutMass: OpenRocket: NullPointerException "Cannot invoke "info.openrocket.core.preset.xml.BaseComponentDTO$AnnotatedMassDTO.getValue()" because "this.nutMass" is null", which ends its whole preset load
        refused("66", "PARSE: Unable to parse component preset file: case.orc: the RailButton 'RB' (component 1) has no <NutMass>"),
        // 67 tube no OD and no ID: OpenRocket: NullPointerException "Cannot invoke "info.openrocket.core.preset.xml.BaseComponentDTO$AnnotatedLengthDTO.getValue()" because "this.insideDiameter" is null", which ends its whole preset load
        refused("67", "PARSE: Unable to parse component preset file: case.orc: the BodyTube 'BT' (component 1) has no <InsideDiameter>"),
        // 68 element order reversed
        read("68", {"Legacy=false; Manufacturer=Estes; PartNo=BT-X; Material=[Bulk 'Cardboard' 680.0 shear 4.0E8 PaperProducts builtin]; InnerDiameter=0.02413; OuterDiameter=0.024790399999999997; Length=0.4572; Type=BODY_TUBE; Thickness=3.3019999999999924E-4; digest=b1227727dcbcf858fef8d5ddf5bbb34e"}),
        // 69 CDATA and comment in number
        read("69", {"Legacy=false; Manufacturer=Estes; PartNo=BT-X; Description=d; Material=[Bulk 'Cardboard' 680.0 shear 4.0E8 PaperProducts builtin]; InnerDiameter=0.02413; OuterDiameter=0.024790399999999997; Length=0.4699; Type=BODY_TUBE; Thickness=3.3019999999999924E-4; digest=9cb2433c7f787148bb8484f2fea22a80"}),
        // 70 attribute with namespace prefix
        read("70", {kTubeOf18Metres}),
        // 71 element in other namespace
        read("71", {kTube}),
        // 72 DOCTYPE with entity: XmlScanner's deviation (a document type declaration that declares anything is refused): OpenRocket expands the entity and loads the body tube
        refused("72", "UNSUPPORTED_FORMAT: Unable to parse component preset file: case.orc (invalid format or corrupted file): Unsupported document type declaration: markup declarations"),
        // 73 Manufacturer with spaces
        read("73", {kTube}),
        // 74 same preset twice
        read("74", {kTube, kTube}),
        // 75 thumbnail
        read("75", {"Legacy=false; Manufacturer=Estes; PartNo=BT-X; Description=d; Material=[Bulk 'Cardboard' 680.0 shear 4.0E8 PaperProducts builtin]; Image=bytes5:68656c6c6f; InnerDiameter=0.02413; OuterDiameter=0.024790399999999997; Length=0.4572; Type=BODY_TUBE; Thickness=3.3019999999999924E-4; digest=88ddbe03a488913456ab8159a7674a61"}),
        // 76 thumbnail bad base64
        read("76", {kTube}),
        // 77 version attribute on root
        read("77", {kTube}),
        // 78 utf8 BOM: OpenRocket: IOException "Unable to parse component preset file: case.orc (invalid format or corrupted file)"
        refused("78", "PARSE: Unable to parse component preset file: case.orc (invalid format or corrupted file): Content is not allowed in prolog."),
        // 79 negative length
        read("79", {"Legacy=false; Manufacturer=Estes; PartNo=BT-X; Description=d; Material=[Bulk 'Cardboard' 680.0 shear 4.0E8 PaperProducts builtin]; InnerDiameter=0.02413; OuterDiameter=0.024790399999999997; Length=-0.4572; Type=BODY_TUBE; Thickness=3.3019999999999924E-4; digest=d9871327698cc4cc3f1d3f5f35e7fc13"}),
        // 80 huge exponent: bug 19: OpenRocket loads the preset with that Length
        read("80", {}, {"Component preset file case.orc: the BodyTube 'BT-X' (component 1) was skipped: its <Length> is not a finite number (Infinity)"}),
        // 81 latin1 bytes in name (invalid UTF-8)
        read("81", {"Legacy=false; Manufacturer=Estes; PartNo=BT-X; Description=caf\uFFFD; Material=[Bulk 'Cardboard' 680.0 shear 4.0E8 PaperProducts builtin]; InnerDiameter=0.02413; OuterDiameter=0.024790399999999997; Length=0.4572; Type=BODY_TUBE; Thickness=3.3019999999999924E-4; digest=5aec841c8238d6a6e2b0a8e603ea7f8a"}),
        // 82 UTF-16 document: OpenRocket: IOException "Unable to parse component preset file: case.orc (invalid format or corrupted file)"
        refused("82", "PARSE: Unable to parse component preset file: case.orc (invalid format or corrupted file): Content is not allowed in prolog."),
    };
    return kCases;
}

[[nodiscard]] const std::vector<LoaderCase>& batch2NumbersCases()
{
    static const std::vector<LoaderCase> kCases{
        // N00 double [18]
        read("N00", {kTubeOf18Metres}),
        // N01 double [18.]
        read("N01", {kTubeOf18Metres}),
        // N02 double [1.8E1]
        read("N02", {kTubeOf18Metres}),
        // N03 double [1.8e+1]
        read("N03", {kTubeOf18Metres}),
        // N04 double [0x1.2p4]
        read("N04", {kTubeOf18Metres}),
        // N05 double [0X12]
        read("N05", {kTubeOfLength0}),
        // N06 double [18f]
        read("N06", {kTubeOfLength0}),
        // N07 double [18 ]
        read("N07", {kTubeOf18Metres}),
        // N08 double [\t18\n]
        read("N08", {kTubeOf18Metres}),
        // N09 double [1 8]
        read("N09", {kTubeOfLength0}),
        // N10 double [--18]
        read("N10", {kTubeOfLength0}),
        // N11 double [+-18]
        read("N11", {kTubeOfLength0}),
        // N12 double [1e]
        read("N12", {kTubeOfLength0}),
        // N13 double [e1]
        read("N13", {kTubeOfLength0}),
        // N14 double [.]
        read("N14", {kTubeOfLength0}),
        // N15 double [-.5]
        read("N15", {"Legacy=false; Manufacturer=Estes; PartNo=BT-X; Description=d; Material=[Bulk 'Cardboard' 680.0 shear 4.0E8 PaperProducts builtin]; InnerDiameter=0.02413; OuterDiameter=0.024790399999999997; Length=-0.5; Type=BODY_TUBE; Thickness=3.3019999999999924E-4; digest=67192a3fd4f10d58e34d071ce0d189e5"}),
        // N16 double [5.-]
        read("N16", {kTubeOfLength0}),
        // N17 double [<U+00A0>18]
        read("N17", {kTubeOfLength0}),
        // N18 double [<U+2009>18]
        read("N18", {kTubeOfLength0}),
        // N19 double [<U+0661><U+0668>]
        read("N19", {kTubeOfLength0}),
        // N20 double [18<U+00A0>]
        read("N20", {kTubeOfLength0}),
        // N21 double [1_8]
        read("N21", {kTubeOfLength0}),
        // N22 double [0018]
        read("N22", {kTubeOf18Metres}),
        // N23 double [-INF]: bug 19: OpenRocket loads the preset with that Length
        read("N23", {}, {"Component preset file case.orc: the BodyTube 'BT-X' (component 1) was skipped: its <Length> is not a finite number (-Infinity)"}),
        // N24 double [ INF ]: bug 19: OpenRocket loads the preset with that Length
        read("N24", {}, {"Component preset file case.orc: the BodyTube 'BT-X' (component 1) was skipped: its <Length> is not a finite number (Infinity)"}),
        // N25 double [NaN ]: bug 19: OpenRocket loads the preset with that Length
        read("N25", {}, {"Component preset file case.orc: the BodyTube 'BT-X' (component 1) was skipped: its <Length> is not a finite number (NaN)"}),
        // N26 double [+NaN]
        read("N26", {kTubeOfLength0}),
        // N27 double [1e-400]
        read("N27", {kTubeOfLength0}),
        // N28 double [4.9e-324]
        read("N28", {"Legacy=false; Manufacturer=Estes; PartNo=BT-X; Description=d; Material=[Bulk 'Cardboard' 680.0 shear 4.0E8 PaperProducts builtin]; InnerDiameter=0.02413; OuterDiameter=0.024790399999999997; Length=4.9E-324; Type=BODY_TUBE; Thickness=3.3019999999999924E-4; digest=aabd944c32e07c9cb7c07f64b15f5994"}),
        // N29 double [2.2250738585072011e-308]
        read("N29", {"Legacy=false; Manufacturer=Estes; PartNo=BT-X; Description=d; Material=[Bulk 'Cardboard' 680.0 shear 4.0E8 PaperProducts builtin]; InnerDiameter=0.02413; OuterDiameter=0.024790399999999997; Length=2.225073858507201E-308; Type=BODY_TUBE; Thickness=3.3019999999999924E-4; digest=4d0f03d2d9432e58e257d8b5f519944b"}),
        // N30 double [9007199254740993]
        read("N30", {"Legacy=false; Manufacturer=Estes; PartNo=BT-X; Description=d; Material=[Bulk 'Cardboard' 680.0 shear 4.0E8 PaperProducts builtin]; InnerDiameter=0.02413; OuterDiameter=0.024790399999999997; Length=9.007199254740992E15; Type=BODY_TUBE; Thickness=3.3019999999999924E-4; digest=db2389053d6b9ff703f648a4eb997a19"}),
        // N31 double [0.1]
        read("N31", {"Legacy=false; Manufacturer=Estes; PartNo=BT-X; Description=d; Material=[Bulk 'Cardboard' 680.0 shear 4.0E8 PaperProducts builtin]; InnerDiameter=0.02413; OuterDiameter=0.024790399999999997; Length=0.1; Type=BODY_TUBE; Thickness=3.3019999999999924E-4; digest=7da6751dc5a9822076e4bdb667d8a767"}),
        // N32 double [1.7976931348623157e309]: bug 19: OpenRocket loads the preset with that Length
        read("N32", {}, {"Component preset file case.orc: the BodyTube 'BT-X' (component 1) was skipped: its <Length> is not a finite number (Infinity)"}),
    };
    return kCases;
}

[[nodiscard]] const std::vector<LoaderCase>& batch2IntegersCases()
{
    static const std::vector<LoaderCase> kCases{
        // I00 int [6]
        read("I00", {kChuteOf6Sides}),
        // I01 int [ 6 ]
        read("I01", {kChuteOf6Sides}),
        // I02 int [+6]
        read("I02", {kChuteOf6Sides}),
        // I03 int [-6]
        read("I03", {kChuteOfMinus6Sides}),
        // I04 int [6.0]
        read("I04", {kChuteWithoutSides}),
        // I05 int [1 2]
        read("I05", {"Legacy=false; Manufacturer=Estes; PartNo=PK; Material=[Surface 'Ripstop nylon' 0.067 shear 0.0 Fabrics builtin]; Type=PARACHUTE; Diameter=0.3; LineCount=6; LineLength=0.3; Sides=12; digest=64105d09f4e36a62435ff424e47da97f"}),
        // I06 int [5-]
        read("I06", {"Legacy=false; Manufacturer=Estes; PartNo=PK; Material=[Surface 'Ripstop nylon' 0.067 shear 0.0 Fabrics builtin]; Type=PARACHUTE; Diameter=0.3; LineCount=6; LineLength=0.3; Sides=-5; digest=64105d09f4e36a62435ff424e47da97f"}),
        // I07 int [--5]
        read("I07", {"Legacy=false; Manufacturer=Estes; PartNo=PK; Material=[Surface 'Ripstop nylon' 0.067 shear 0.0 Fabrics builtin]; Type=PARACHUTE; Diameter=0.3; LineCount=6; LineLength=0.3; Sides=-5; digest=64105d09f4e36a62435ff424e47da97f"}),
        // I08 int [2147483648]
        read("I08", {"Legacy=false; Manufacturer=Estes; PartNo=PK; Material=[Surface 'Ripstop nylon' 0.067 shear 0.0 Fabrics builtin]; Type=PARACHUTE; Diameter=0.3; LineCount=6; LineLength=0.3; Sides=-2147483648; digest=64105d09f4e36a62435ff424e47da97f"}),
        // I09 int [99999999999]
        read("I09", {"Legacy=false; Manufacturer=Estes; PartNo=PK; Material=[Surface 'Ripstop nylon' 0.067 shear 0.0 Fabrics builtin]; Type=PARACHUTE; Diameter=0.3; LineCount=6; LineLength=0.3; Sides=1215752191; digest=64105d09f4e36a62435ff424e47da97f"}),
        // I10 int []
        read("I10", {kChuteOf0Sides}),
        // I11 int [0x6]
        read("I11", {kChuteWithoutSides}),
        // I12 int [<U+0666>]
        read("I12", {kChuteWithoutSides}),
        // I13 int [6e0]
        read("I13", {kChuteWithoutSides}),
        // I14 int [+]
        read("I14", {kChuteOf0Sides}),
        // I15 int [-]
        read("I15", {kChuteOf0Sides}),
    };
    return kCases;
}

[[nodiscard]] const std::vector<LoaderCase>& batch2BooleansCases()
{
    static const std::vector<LoaderCase> kCases{
        // B00 bool [true]
        read("B00", {kBulkheadFilled}),
        // B01 bool [false]
        read("B01", {kBulkheadNotFilled}),
        // B02 bool [1]
        read("B02", {kBulkheadFilled}),
        // B03 bool [0]
        read("B03", {kBulkheadNotFilled}),
        // B04 bool [ true ]
        read("B04", {kBulkheadFilled}),
        // B05 bool [\ntrue\n]
        read("B05", {kBulkheadFilled}),
        // B06 bool [True]
        read("B06", {kBulkhead}),
        // B07 bool [t]
        read("B07", {kBulkhead}),
        // B08 bool [tr]
        read("B08", {kBulkhead}),
        // B09 bool [truex]
        read("B09", {kBulkhead}),
        // B10 bool [true1]
        read("B10", {kBulkhead}),
        // B11 bool [f]
        read("B11", {kBulkhead}),
        // B12 bool [fals]
        read("B12", {kBulkhead}),
        // B13 bool [falsey]
        read("B13", {kBulkhead}),
        // B14 bool [2]
        read("B14", {kBulkheadNotFilled}),
        // B15 bool []
        read("B15", {kBulkhead}),
        // B16 bool [yes]
        read("B16", {kBulkhead}),
        // B17 bool [01]
        read("B17", {kBulkhead}),
        // B18 bool [10]
        read("B18", {kBulkhead}),
        // B19 bool [ 1 ]
        read("B19", {kBulkheadFilled}),
    };
    return kCases;
}

[[nodiscard]] const std::vector<LoaderCase>& batch2ShapesCases()
{
    static const std::vector<LoaderCase> kCases{
        // S00 shape [OGIVE]
        read("S00", {kOgiveNoseCone}),
        // S01 shape [ OGIVE ]: OpenRocket: NullPointerException "Cannot invoke "info.openrocket.core.preset.xml.ShapeDTO.getORShape()" because "this.shape" is null", which ends its whole preset load
        refused("S01", "PARSE: Unable to parse component preset file: case.orc: the NoseCone 'NC' (component 1) has the <Shape> ' OGIVE ', which is none of CONICAL, OGIVE, ELLIPSOID, POWER, PARABOLIC and HAACK"),
        // S02 shape [\nOGIVE\n]: OpenRocket: NullPointerException "Cannot invoke "info.openrocket.core.preset.xml.ShapeDTO.getORShape()" because "this.shape" is null", which ends its whole preset load
        refused("S02", R"(PARSE: Unable to parse component preset file: case.orc: the NoseCone 'NC' (component 1) has the <Shape> '\nOGIVE\n', which is none of CONICAL, OGIVE, ELLIPSOID, POWER, PARABOLIC and HAACK)"),
        // S03 shape [OGIVE<U+00A0>]: OpenRocket: NullPointerException "Cannot invoke "info.openrocket.core.preset.xml.ShapeDTO.getORShape()" because "this.shape" is null", which ends its whole preset load
        refused("S03", "PARSE: Unable to parse component preset file: case.orc: the NoseCone 'NC' (component 1) has the <Shape> 'OGIVE\u00A0', which is none of CONICAL, OGIVE, ELLIPSOID, POWER, PARABOLIC and HAACK"),
        // S04 shape [Ogive]: OpenRocket: NullPointerException "Cannot invoke "info.openrocket.core.preset.xml.ShapeDTO.getORShape()" because "this.shape" is null", which ends its whole preset load
        refused("S04", "PARSE: Unable to parse component preset file: case.orc: the NoseCone 'NC' (component 1) has the <Shape> 'Ogive', which is none of CONICAL, OGIVE, ELLIPSOID, POWER, PARABOLIC and HAACK"),
        // S05 shape [HAACK]
        read("S05", {"Legacy=false; Manufacturer=Estes; PartNo=NC; Material=[Bulk 'Cardboard' 680.0 shear 4.0E8 PaperProducts builtin]; Shape=Haack series; AftOuterDiameter=0.02; AftShoulderLength=0.01; AftShoulderDiameter=0.019; Length=0.05; Type=NOSE_CONE; digest=42401b6a1f7d56e27dfdb748b702e900"}),
        // S06 shape [POWER]
        read("S06", {"Legacy=false; Manufacturer=Estes; PartNo=NC; Material=[Bulk 'Cardboard' 680.0 shear 4.0E8 PaperProducts builtin]; Shape=Power series; AftOuterDiameter=0.02; AftShoulderLength=0.01; AftShoulderDiameter=0.019; Length=0.05; Type=NOSE_CONE; digest=2e3351b419eb13199fa1c009eff62da0"}),
    };
    return kCases;
}

[[nodiscard]] const std::vector<LoaderCase>& batch2TextsCases()
{
    static const std::vector<LoaderCase> kCases{
        // T01 text split by child element
        read("T01", {"Legacy=false; Manufacturer=Estes; PartNo=BT-X; Description=d; Material=[Bulk 'Cardboard' 680.0 shear 4.0E8 PaperProducts builtin]; InnerDiameter=0.02413; OuterDiameter=0.024790399999999997; Length=0.2032; Type=BODY_TUBE; Thickness=3.3019999999999924E-4; digest=9f88af1392f21a76a76c671be47bd392"}),
        // T02 empty Description
        read("T02", {kTubeWithEmptyDescription}),
        // T03 Description with entity and CR LF
        read("T03", {R"(Legacy=false; Manufacturer=Estes; PartNo=BT-X; Description= a&b\nc\t; Material=[Bulk 'Cardboard' 680.0 shear 4.0E8 PaperProducts builtin]; InnerDiameter=0.02413; OuterDiameter=0.024790399999999997; Length=0.4572; Type=BODY_TUBE; Thickness=3.3019999999999924E-4; digest=89bd44c404f448154e37a8f60c60f5d9)"}),
        // T04 Material type attribute with spaces: OpenRocket: IllegalArgumentException "Invalid material type  BULK specified for Component", which ends its whole preset load
        refused("T04", "PARSE: Unable to parse component preset file: case.orc: the BodyTube 'BT-X' (component 1) has a <Material> of the Type ' BULK', which is none of BULK, SURFACE and LINE"),
        // T05 Unit attribute lb on Mass
        read("T05", {"Legacy=false; Manufacturer=Estes; PartNo=BT-X; Description=d; Material=[Bulk 'Cardboard' 19549.81165935102 shear 0.0 Custom user]; Mass=0.226796185; InnerDiameter=0.02413; OuterDiameter=0.024790399999999997; Length=0.4572; Type=BODY_TUBE; Thickness=3.3019999999999924E-4; digest=c9c453dba3b0e1c564962eab21a506a3"}),
        // T06 Unit kg on Length: OpenRocket: IllegalArgumentException "name=kg", which ends its whole preset load
        refused("T06", "PARSE: Unable to parse component preset file: case.orc: the BodyTube 'BT-X' (component 1) gives its <Length> in the unknown unit 'kg'"),
        // T07 two Materials wrappers
        read("T07", {"Legacy=false; Manufacturer=Estes; PartNo=BT-X; Description=d; Material=[Bulk 'Paper test' 0.0 shear 0.0 Other user]; InnerDiameter=0.02413; OuterDiameter=0.024790399999999997; Length=0.4572; Type=BODY_TUBE; Thickness=3.3019999999999924E-4; digest=3bf52f38f0e90551382cc4f1ab1ed1f7"}),
        // T08 Legacy empty
        read("T08", {"Legacy=true; Manufacturer=Estes; PartNo=BT-X; Description=d; Material=[Bulk 'Cardboard' 680.0 shear 4.0E8 PaperProducts builtin]; InnerDiameter=0.02413; OuterDiameter=0.024790399999999997; Length=0.4572; Type=BODY_TUBE; Thickness=3.3019999999999924E-4; digest=72e2ef64f8f85e6788d7c9919d4ca7d2"}),
        // T09 text inside Components
        read("T09", {kTube}),
        // T10 Material element with child element
        read("T10", {"Legacy=false; Manufacturer=Estes; PartNo=BT-X; Description=d; Material=[Bulk 'board' 0.0 shear 0.0 Other user]; InnerDiameter=0.02413; OuterDiameter=0.024790399999999997; Length=0.4572; Type=BODY_TUBE; Thickness=3.3019999999999924E-4; digest=3bf52f38f0e90551382cc4f1ab1ed1f7"}),
        // T11 Manufacturer empty
        read("T11", {"Legacy=false; Manufacturer=; PartNo=BT-X; Description=d; Material=[Bulk 'Cardboard' 680.0 shear 4.0E8 PaperProducts builtin]; InnerDiameter=0.02413; OuterDiameter=0.024790399999999997; Length=0.4572; Type=BODY_TUBE; Thickness=3.3019999999999924E-4; digest=d79d1cdbc36b18463dba7a8c17127717"}),
        // T12 PartNumber empty
        read("T12", {"Legacy=false; Manufacturer=Estes; PartNo=; Description=d; Material=[Bulk 'Cardboard' 680.0 shear 4.0E8 PaperProducts builtin]; InnerDiameter=0.02413; OuterDiameter=0.024790399999999997; Length=0.4572; Type=BODY_TUBE; Thickness=3.3019999999999924E-4; digest=415b49856ebf005544ac21ee95b04a3b"}),
        // T13 section density uom g/m2 for BULK: OpenRocket: IOException "Unable to parse component preset file: case.orc (invalid format or corrupted file)"
        refused("T13", "PARSE: Unable to parse component preset file: case.orc (invalid format or corrupted file): material 1 of <Materials> gives its density in the unknown unit 'g/m\u00B2'"),
        // T14 section Material Type with spaces: OpenRocket: NullPointerException "Cannot invoke "info.openrocket.core.preset.xml.MaterialTypeDTO.name()" because the return value of "info.openrocket.core.preset.xml.MaterialDTO.getType()" is null", which ends its whole preset load
        refused("T14", "PARSE: Unable to parse component preset file: case.orc: the BodyTube 'BT-X' (component 1) has a <Material>, but material 1 of <Materials> has no <Type> that is LINE, SURFACE or BULK"),
        // T15 section ShearModulus
        read("T15", {kTubeOfShearModulus1e9}),
        // T16 attribute Unit on root ignored and unknown attribute
        read("T16", {kTube}),
        // T17 component with xmlns default
        read("T17", {}),
        // T18 Length with xmlns empty
        read("T18", {kTube}),
        // T19 Length with xmlns default other: OpenRocket: NullPointerException "Cannot invoke "info.openrocket.core.preset.xml.BaseComponentDTO$AnnotatedLengthDTO.getValue()" because "this.length" is null", which ends its whole preset load
        refused("T19", "PARSE: Unable to parse component preset file: case.orc: the BodyTube 'BT-X' (component 1) has no <Length>"),
        // T20 Version element with child
        read("T20", {kTube}),
    };
    return kCases;
}

[[nodiscard]] const std::vector<LoaderCase>& batch3NumbersCases()
{
    static const std::vector<LoaderCase> kCases{
        // XN00 double [+INF]
        read("XN00", {kTubeOfLength0}),
        // XN01 double [inf]
        read("XN01", {kTubeOfLength0}),
        // XN02 double [-NaN]
        read("XN02", {kTubeOfLength0}),
        // XN03 double [INF\t]: bug 19: OpenRocket loads the preset with that Length
        read("XN03", {}, {"Component preset file case.orc: the BodyTube 'BT-X' (component 1) was skipped: its <Length> is not a finite number (Infinity)"}),
        // XN04 double [&#13;18&#13;]
        read("XN04", {kTubeOf18Metres}),
        // XN05 double [ ]
        read("XN05", {kTubeOfLength0}),
        // XN06 double [+.5]
        read("XN06", {"Legacy=false; Manufacturer=Estes; PartNo=BT-X; Description=d; Material=[Bulk 'Cardboard' 680.0 shear 4.0E8 PaperProducts builtin]; InnerDiameter=0.02413; OuterDiameter=0.024790399999999997; Length=0.5; Type=BODY_TUBE; Thickness=3.3019999999999924E-4; digest=5a5112dd7b1a408ec855a2b64e7d5d7e"}),
        // XN07 double [1e+]
        read("XN07", {kTubeOfLength0}),
        // XN08 double [-0x1.8p1]
        read("XN08", {"Legacy=false; Manufacturer=Estes; PartNo=BT-X; Description=d; Material=[Bulk 'Cardboard' 680.0 shear 4.0E8 PaperProducts builtin]; InnerDiameter=0.02413; OuterDiameter=0.024790399999999997; Length=-3.0; Type=BODY_TUBE; Thickness=3.3019999999999924E-4; digest=bca05d5dd4d96b47eca7a083c0aa1afe"}),
        // XN09 double [1&#160;]
        read("XN09", {kTubeOfLength0}),
        // XN10 double [\n]
        read("XN10", {kTubeOfLength0}),
        // XN11 double [18<b/>x]
        read("XN11", {kTubeOf18Metres}),
        // XN12 double [<b/>18]
        read("XN12", {kTubeOf18Metres}),
    };
    return kCases;
}

[[nodiscard]] const std::vector<LoaderCase>& batch3IntegersCases()
{
    static const std::vector<LoaderCase> kCases{
        // XI00 int [+-6]
        read("XI00", {kChuteOfMinus6Sides}),
        // XI01 int [-+6]
        read("XI01", {kChuteOfMinus6Sides}),
        // XI02 int [6\t]
        read("XI02", {kChuteOf6Sides}),
        // XI03 int [\n6\n]
        read("XI03", {kChuteOf6Sides}),
        // XI04 int [&#13;6]
        read("XI04", {kChuteOf6Sides}),
        // XI05 int [-0]
        read("XI05", {kChuteOf0Sides}),
        // XI06 int [4294967302]
        read("XI06", {kChuteOf6Sides}),
        // XI07 int [-2147483649]
        read("XI07", {"Legacy=false; Manufacturer=Estes; PartNo=PK; Material=[Surface 'Ripstop nylon' 0.067 shear 0.0 Fabrics builtin]; Type=PARACHUTE; Diameter=0.3; LineCount=6; LineLength=0.3; Sides=2147483647; digest=64105d09f4e36a62435ff424e47da97f"}),
        // XI08 int [ ]
        read("XI08", {kChuteOf0Sides}),
        // XI09 int [6 -]
        read("XI09", {kChuteOfMinus6Sides}),
        // XI10 int [0006]
        read("XI10", {kChuteOf6Sides}),
        // XI11 int [&#160;6]
        read("XI11", {kChuteWithoutSides}),
        // XI12 int [6<b/>x]
        read("XI12", {kChuteOf6Sides}),
        // XI13 int [<b/>x]
        read("XI13", {kChuteWithoutSides}),
        // XI14 int [6<b/>]
        read("XI14", {kChuteOf0Sides}),
        // XI15 int [x<b/>6]
        read("XI15", {kChuteOf6Sides}),
        // XI16 int [6<b/><c/>x]
        read("XI16", {kChuteOf6Sides}),
        // XI17 int [ <b/>x]
        read("XI17", {kChuteWithoutSides}),
    };
    return kCases;
}

[[nodiscard]] const std::vector<LoaderCase>& batch3BooleansCases()
{
    static const std::vector<LoaderCase> kCases{
        // XB00 bool [ ]
        read("XB00", {kBulkheadNotFilled}),
        // XB01 bool [\t\t]
        read("XB01", {kBulkheadNotFilled}),
        // XB02 bool [\n]
        read("XB02", {kBulkheadNotFilled}),
        // XB03 bool [x]
        read("XB03", {kBulkheadNotFilled}),
        // XB04 bool [x ]
        read("XB04", {kBulkheadNotFilled}),
        // XB05 bool [ 2 ]
        read("XB05", {kBulkheadNotFilled}),
        // XB06 bool [&#13;1]
        read("XB06", {kBulkheadFilled}),
        // XB07 bool [true\t]
        read("XB07", {kBulkheadFilled}),
        // XB08 bool [false ]
        read("XB08", {kBulkheadNotFilled}),
        // XB09 bool [0 0]
        read("XB09", {kBulkhead}),
        // XB10 bool [1\n]
        read("XB10", {kBulkheadFilled}),
        // XB11 bool [Z]
        read("XB11", {kBulkheadNotFilled}),
        // XB12 bool [t rue]
        read("XB12", {kBulkhead}),
        // XB13 bool [&#160;1]
        read("XB13", {kBulkhead}),
        // XB14 bool [ f]
        read("XB14", {kBulkhead}),
        // XB15 bool [-]
        read("XB15", {kBulkheadNotFilled}),
        // XB16 bool [falsetrue]
        read("XB16", {kBulkhead}),
        // XB17 bool [true<b/>x]
        read("XB17", {kBulkheadNotFilled}),
        // XB18 bool [true<b/>]
        read("XB18", {kBulkhead}),
        // XB19 bool [x<b/>true]
        read("XB19", {kBulkheadFilled}),
        // XB20 bool [true<b/>t]
        read("XB20", {kBulkheadFilled}),
        // XB21 bool [true<b/>01]
        read("XB21", {kBulkhead}),
        // XB22 bool [true<b/>trux]
        read("XB22", {kBulkhead}),
        // XB23 bool [true<b/>fals]
        read("XB23", {kBulkhead}),
        // XB24 bool [true<b/>tr]
        read("XB24", {kBulkhead}),
        // XB25 bool [true<b/>tru]
        read("XB25", {kBulkhead}),
        // XB26 bool [true<b/>f]
        read("XB26", {kBulkheadFilled}),
        // XB27 bool [true<b/>fa]
        read("XB27", {kBulkhead}),
        // XB28 bool [true<b/>fal]
        read("XB28", {kBulkhead}),
        // XB29 bool [true<b/>tx]
        read("XB29", {kBulkhead}),
        // XB30 bool [true<b/>fx]
        read("XB30", {kBulkhead}),
        // XB31 bool [true<b/> t]
        read("XB31", {kBulkheadFilled}),
        // XB32 bool [true<b/>t ]
        read("XB32", {kBulkhead}),
        // XB33 bool [<U+00A0>]
        read("XB33", {kBulkheadNotFilled}),
        // XB34 bool [<U+1F680>]
        read("XB34", {kBulkhead}),
        // XB35 bool [<U+00E9>]
        read("XB35", {kBulkheadNotFilled}),
    };
    return kCases;
}

[[nodiscard]] const std::vector<LoaderCase>& batch3ShapesCases()
{
    static const std::vector<LoaderCase> kCases{
        // XS00 shape [<![CDATA[OGIVE]]>]
        read("XS00", {kOgiveNoseCone}),
        // XS01 shape [OG<!-- c -->IVE]
        read("XS01", {kOgiveNoseCone}),
        // XS02 shape [X<b/>OGIVE]
        read("XS02", {kOgiveNoseCone}),
        // XS03 shape [OGIVE<b/>]: OpenRocket: NullPointerException "Cannot invoke "info.openrocket.core.preset.xml.ShapeDTO.getORShape()" because "this.shape" is null", which ends its whole preset load
        refused("XS03", "PARSE: Unable to parse component preset file: case.orc: the NoseCone 'NC' (component 1) has the <Shape> '', which is none of CONICAL, OGIVE, ELLIPSOID, POWER, PARABOLIC and HAACK"),
        // XS04 shape []: OpenRocket: NullPointerException "Cannot invoke "info.openrocket.core.preset.xml.ShapeDTO.getORShape()" because "this.shape" is null", which ends its whole preset load
        refused("XS04", "PARSE: Unable to parse component preset file: case.orc: the NoseCone 'NC' (component 1) has the <Shape> '', which is none of CONICAL, OGIVE, ELLIPSOID, POWER, PARABOLIC and HAACK"),
        // XS05 shape [&#79;GIVE]
        read("XS05", {kOgiveNoseCone}),
        // XS06 shape [OGIVE<b>x</b>]: OpenRocket: NullPointerException "Cannot invoke "info.openrocket.core.preset.xml.ShapeDTO.getORShape()" because "this.shape" is null", which ends its whole preset load
        refused("XS06", "PARSE: Unable to parse component preset file: case.orc: the NoseCone 'NC' (component 1) has the <Shape> '', which is none of CONICAL, OGIVE, ELLIPSOID, POWER, PARABOLIC and HAACK"),
    };
    return kCases;
}

[[nodiscard]] const std::vector<LoaderCase>& batch3TextsCases()
{
    static const std::vector<LoaderCase> kCases{
        // XT01 text around a child element with text
        read("XT01", {"Legacy=false; Manufacturer=Estes; PartNo=BT-X; Description=d; Material=[Bulk 'Cardboard' 680.0 shear 4.0E8 PaperProducts builtin]; InnerDiameter=0.02413; OuterDiameter=0.024790399999999997; Length=0.2032; Type=BODY_TUBE; Thickness=3.3019999999999924E-4; digest=9f88af1392f21a76a76c671be47bd392"}),
        // XT02 text before an empty child only
        read("XT02", {kTube}),
        // XT03 processing instruction in number
        read("XT03", {kTube}),
        // XT04 comment between digits
        read("XT04", {kTube}),
        // XT05 Description with child element text
        read("XT05", {kTubeWithEmptyDescription}),
        // XT06 Description in CDATA
        read("XT06", {"Legacy=false; Manufacturer=Estes; PartNo=BT-X; Description=x<y; Material=[Bulk 'Cardboard' 680.0 shear 4.0E8 PaperProducts builtin]; InnerDiameter=0.02413; OuterDiameter=0.024790399999999997; Length=0.4572; Type=BODY_TUBE; Thickness=3.3019999999999924E-4; digest=83da73c88012c14446b90f51cf976d28"}),
        // XT07 Description in two text runs around a comment
        read("XT07", {"Legacy=false; Manufacturer=Estes; PartNo=BT-X; Description=a b; Material=[Bulk 'Cardboard' 680.0 shear 4.0E8 PaperProducts builtin]; InnerDiameter=0.02413; OuterDiameter=0.024790399999999997; Length=0.4572; Type=BODY_TUBE; Thickness=3.3019999999999924E-4; digest=eeba39297d9a6af2deaab8f2547a2808"}),
        // XT08 Description with characters beyond the BMP
        read("XT08", {"Legacy=false; Manufacturer=Estes; PartNo=BT-X; Description=\U0001F680\U0001F680; Material=[Bulk 'Cardboard' 680.0 shear 4.0E8 PaperProducts builtin]; InnerDiameter=0.02413; OuterDiameter=0.024790399999999997; Length=0.4572; Type=BODY_TUBE; Thickness=3.3019999999999924E-4; digest=a3d8ddf0694b7f2ea3aa25de4df40b7c"}),
        // XT09 xsi:nil on Length
        read("XT09", {kTube}),
        // XT10 xsi:nil on Description
        read("XT10", {kTube}),
        // XT11 character reference in Unit
        read("XT11", {kTube}),
        // XT12 tab in Unit: OpenRocket: IllegalArgumentException "name=in ", which ends its whole preset load
        refused("XT12", "PARSE: Unable to parse component preset file: case.orc: the BodyTube 'BT-X' (component 1) gives its <Length> in the unknown unit 'in '"),
        // XT13 tab reference in Unit: OpenRocket: IllegalArgumentException "name=in\t", which ends its whole preset load
        refused("XT13", R"(PARSE: Unable to parse component preset file: case.orc: the BodyTube 'BT-X' (component 1) gives its <Length> in the unknown unit 'in\t')"),
        // XT14 two Unit attributes by prefix
        read("XT14", {"Legacy=false; Manufacturer=Estes; PartNo=BT-X; Description=d; Material=[Bulk 'Cardboard' 680.0 shear 4.0E8 PaperProducts builtin]; InnerDiameter=0.02413; OuterDiameter=0.024790399999999997; Length=0.18; Type=BODY_TUBE; Thickness=3.3019999999999924E-4; digest=f967404d6770a04f3beda06074d95060"}),
        // XT15 Material Type attribute by reference
        read("XT15", {kTube}),
        // XT16 PartNumber with trailing space and tab
        read("XT16", {R"(Legacy=false; Manufacturer=Estes; PartNo=BT-X \t; Description=d; Material=[Bulk 'Cardboard' 680.0 shear 4.0E8 PaperProducts builtin]; InnerDiameter=0.02413; OuterDiameter=0.024790399999999997; Length=0.4572; Type=BODY_TUBE; Thickness=3.3019999999999924E-4; digest=202757020efe2b931bc7d9ad101f13f8)"}),
        // XT17 Legacy with a namespace
        read("XT17", {kTube}),
        // XT18 Materials wrapper in another namespace
        read("XT18", {"Legacy=false; Manufacturer=Estes; PartNo=BT-X; Description=d; Material=[Bulk 'Paper test' 0.0 shear 0.0 Other user]; InnerDiameter=0.02413; OuterDiameter=0.024790399999999997; Length=0.4572; Type=BODY_TUBE; Thickness=3.3019999999999924E-4; digest=3bf52f38f0e90551382cc4f1ab1ed1f7"}),
        // XT19 root with an empty default namespace
        read("XT19", {kTube}),
        // XT20 root in a prefixed namespace: OpenRocket: IOException "Unable to parse component preset file: case.orc (invalid format or corrupted file)"
        refused("XT20", "PARSE: Unable to parse component preset file: case.orc (invalid format or corrupted file): the root element <OpenRocketComponent> is in the namespace 'urn:p'"),
        // XT21 empty Components wrapper after a full one
        read("XT21", {}),
        // XT22 component element with text only: OpenRocket: NullPointerException "Cannot invoke "String.toLowerCase(java.util.Locale)" because "str" is null", which ends its whole preset load
        refused("XT22", "PARSE: Unable to parse component preset file: case.orc: the BodyTube (component 1) has no <Manufacturer>"),
        // XT23 document with only a root
        read("XT23", {}),
        // XT24 comment and PI before the root
        read("XT24", {kTube}),
        // XT25 Description with an empty child only
        read("XT25", {kTubeWithEmptyDescription}),
        // XT26 XML 1.1 control character before a number
        read("XT26", {kTubeOfLength0}),
        // XT27 XML 1.1 control character after a number
        read("XT27", {kTubeOfLength0}),
        // XT28 XML 1.1 control character before an int
        read("XT28", {kChuteWithoutSides}),
        // XT29 XML 1.1 control character before a boolean
        read("XT29", {kBulkhead}),
        // XT30 two Length elements, the second not a number
        read("XT30", {kTubeOfLength0}),
        // XT31 two Sides elements, the second not a number
        read("XT31", {kChuteOf6Sides}),
        // XT32 two Sides elements, the second empty
        read("XT32", {kChuteOf0Sides}),
        // XT33 two Density elements, the second not a number
        read("XT33", {kTubeOfNewCardboard}),
        // XT34 two Filled elements, the second malformed
        read("XT34", {kBulkheadFilled}),
        // XT35 two Description elements, the second empty
        read("XT35", {kTubeWithEmptyDescription}),
        // XT36 two Shape elements, the second unknown: OpenRocket: NullPointerException "Cannot invoke "info.openrocket.core.preset.xml.ShapeDTO.getORShape()" because "this.shape" is null", which ends its whole preset load
        refused("XT36", "PARSE: Unable to parse component preset file: case.orc: the NoseCone 'NC' (component 1) has the <Shape> 'BLUNT', which is none of CONICAL, OGIVE, ELLIPSOID, POWER, PARABOLIC and HAACK"),
        // XT37 two ShearModulus elements, the second not a number
        read("XT37", {kTubeOfShearModulus1e9}),
        // XT38 Density with text after a child element
        read("XT38", {kTube}),
        // XT39 two Density elements, the second empty
        read("XT39", {kTubeOfNewCardboard}),
        // XT40 two Density elements, the first not a number
        read("XT40", {kTube}),
        // XT41 two ShearModulus elements, the second empty
        read("XT41", {kTubeOfShearModulus1e9}),
        // XT42 ShearModulus with text after a child element
        read("XT42", {kTubeOfShearModulus1e9}),
        // XT43 ShearModulus not a number
        read("XT43", {kTube}),
        // XT44 two Length elements, the first not a number
        read("XT44", {kTube}),
        // XT45 Description with white space before a child element
        read("XT45", {"Legacy=false; Manufacturer=Estes; PartNo=BT-X; Description=x; Material=[Bulk 'Cardboard' 680.0 shear 4.0E8 PaperProducts builtin]; InnerDiameter=0.02413; OuterDiameter=0.024790399999999997; Length=0.4572; Type=BODY_TUBE; Thickness=3.3019999999999924E-4; digest=6276507e3e2598b08cc38d2b1346272b"}),
        // XT46 Description with white space after a child element
        read("XT46", {"Legacy=false; Manufacturer=Estes; PartNo=BT-X; Description= ; Material=[Bulk 'Cardboard' 680.0 shear 4.0E8 PaperProducts builtin]; InnerDiameter=0.02413; OuterDiameter=0.024790399999999997; Length=0.4572; Type=BODY_TUBE; Thickness=3.3019999999999924E-4; digest=a6bbb49f06d35cf45f55298dc8b10b3d"}),
        // XT47 Description with text between two child elements
        read("XT47", {kTubeWithEmptyDescription}),
        // XT48 Description with a carriage return by reference
        read("XT48", {R"(Legacy=false; Manufacturer=Estes; PartNo=BT-X; Description=a\rb\r\nc\nd; Material=[Bulk 'Cardboard' 680.0 shear 4.0E8 PaperProducts builtin]; InnerDiameter=0.02413; OuterDiameter=0.024790399999999997; Length=0.4572; Type=BODY_TUBE; Thickness=3.3019999999999924E-4; digest=b6721e0aff6781d1e6e958fe0d0da331)"}),
    };
    return kCases;
}

[[nodiscard]] const std::vector<LoaderCase>& batch3ThumbnailsCases()
{
    static const std::vector<LoaderCase> kCases{
        // XH00 thumbnail [aGVsbG8]
        read("XH00", {kTube}),
        // XH01 thumbnail [aGVs bG8=]
        read("XH01", {kTube}),
        // XH02 thumbnail [aGVsbG8=\n]
        read("XH02", {kTube}),
        // XH03 thumbnail []
        read("XH03", {kTubeWithEmptyImage}),
        // XH04 thumbnail [QQ==]
        read("XH04", {kTubeWithImage41}),
        // XH05 thumbnail [QQ]
        read("XH05", {kTube}),
        // XH06 thumbnail [Q]
        read("XH06", {kTube}),
        // XH07 thumbnail [aGV!sbG8=]
        read("XH07", {kTube}),
        // XH08 thumbnail [====]
        read("XH08", {"Legacy=false; Manufacturer=Estes; PartNo=BT-X; Description=d; Material=[Bulk 'Cardboard' 680.0 shear 4.0E8 PaperProducts builtin]; Image=bytes1:ff; InnerDiameter=0.02413; OuterDiameter=0.024790399999999997; Length=0.4572; Type=BODY_TUBE; Thickness=3.3019999999999924E-4; digest=88ddbe03a488913456ab8159a7674a61"}),
        // XH09 thumbnail [QUJD]
        read("XH09", {"Legacy=false; Manufacturer=Estes; PartNo=BT-X; Description=d; Material=[Bulk 'Cardboard' 680.0 shear 4.0E8 PaperProducts builtin]; Image=bytes3:414243; InnerDiameter=0.02413; OuterDiameter=0.024790399999999997; Length=0.4572; Type=BODY_TUBE; Thickness=3.3019999999999924E-4; digest=88ddbe03a488913456ab8159a7674a61"}),
        // XH10 thumbnail [ QUJD ]
        read("XH10", {kTube}),
        // XH11 thumbnail [QUJD=]
        read("XH11", {kTube}),
        // XH12 thumbnail [QU=JD]
        read("XH12", {kTube}),
        // XH13 thumbnail [4pyI]
        read("XH13", {"Legacy=false; Manufacturer=Estes; PartNo=BT-X; Description=d; Material=[Bulk 'Cardboard' 680.0 shear 4.0E8 PaperProducts builtin]; Image=bytes3:e29c88; InnerDiameter=0.02413; OuterDiameter=0.024790399999999997; Length=0.4572; Type=BODY_TUBE; Thickness=3.3019999999999924E-4; digest=88ddbe03a488913456ab8159a7674a61"}),
        // XH14 thumbnail [QU==]
        read("XH14", {kTubeWithImage41}),
        // XH15 thumbnail [QUI=]
        read("XH15", {"Legacy=false; Manufacturer=Estes; PartNo=BT-X; Description=d; Material=[Bulk 'Cardboard' 680.0 shear 4.0E8 PaperProducts builtin]; Image=bytes2:4142; InnerDiameter=0.02413; OuterDiameter=0.024790399999999997; Length=0.4572; Type=BODY_TUBE; Thickness=3.3019999999999924E-4; digest=88ddbe03a488913456ab8159a7674a61"}),
        // XH16 thumbnail [QUJDQQ==]
        read("XH16", {"Legacy=false; Manufacturer=Estes; PartNo=BT-X; Description=d; Material=[Bulk 'Cardboard' 680.0 shear 4.0E8 PaperProducts builtin]; Image=bytes4:41424341; InnerDiameter=0.02413; OuterDiameter=0.024790399999999997; Length=0.4572; Type=BODY_TUBE; Thickness=3.3019999999999924E-4; digest=88ddbe03a488913456ab8159a7674a61"}),
        // XH17 thumbnail [Q===]
        read("XH17", {"Legacy=false; Manufacturer=Estes; PartNo=BT-X; Description=d; Material=[Bulk 'Cardboard' 680.0 shear 4.0E8 PaperProducts builtin]; Image=bytes1:47; InnerDiameter=0.02413; OuterDiameter=0.024790399999999997; Length=0.4572; Type=BODY_TUBE; Thickness=3.3019999999999924E-4; digest=88ddbe03a488913456ab8159a7674a61"}),
        // XH18 thumbnail [=QUJ]
        read("XH18", {"Legacy=false; Manufacturer=Estes; PartNo=BT-X; Description=d; Material=[Bulk 'Cardboard' 680.0 shear 4.0E8 PaperProducts builtin]; Image=bytes3:fd0509; InnerDiameter=0.02413; OuterDiameter=0.024790399999999997; Length=0.4572; Type=BODY_TUBE; Thickness=3.3019999999999924E-4; digest=88ddbe03a488913456ab8159a7674a61"}),
        // XH19 thumbnail [QU=D]
        read("XH19", {"Legacy=false; Manufacturer=Estes; PartNo=BT-X; Description=d; Material=[Bulk 'Cardboard' 680.0 shear 4.0E8 PaperProducts builtin]; Image=bytes2:41c3; InnerDiameter=0.02413; OuterDiameter=0.024790399999999997; Length=0.4572; Type=BODY_TUBE; Thickness=3.3019999999999924E-4; digest=88ddbe03a488913456ab8159a7674a61"}),
        // XH20 thumbnail [QQ==QUJD]
        read("XH20", {"Legacy=false; Manufacturer=Estes; PartNo=BT-X; Description=d; Material=[Bulk 'Cardboard' 680.0 shear 4.0E8 PaperProducts builtin]; Image=bytes4:41414243; InnerDiameter=0.02413; OuterDiameter=0.024790399999999997; Length=0.4572; Type=BODY_TUBE; Thickness=3.3019999999999924E-4; digest=88ddbe03a488913456ab8159a7674a61"}),
        // XH21 thumbnail [QUJD====]
        read("XH21", {"Legacy=false; Manufacturer=Estes; PartNo=BT-X; Description=d; Material=[Bulk 'Cardboard' 680.0 shear 4.0E8 PaperProducts builtin]; Image=bytes4:414243ff; InnerDiameter=0.02413; OuterDiameter=0.024790399999999997; Length=0.4572; Type=BODY_TUBE; Thickness=3.3019999999999924E-4; digest=88ddbe03a488913456ab8159a7674a61"}),
        // XH22 thumbnail [AAAA]
        read("XH22", {"Legacy=false; Manufacturer=Estes; PartNo=BT-X; Description=d; Material=[Bulk 'Cardboard' 680.0 shear 4.0E8 PaperProducts builtin]; Image=bytes3:000000; InnerDiameter=0.02413; OuterDiameter=0.024790399999999997; Length=0.4572; Type=BODY_TUBE; Thickness=3.3019999999999924E-4; digest=88ddbe03a488913456ab8159a7674a61"}),
        // XH23 thumbnail [////]
        read("XH23", {"Legacy=false; Manufacturer=Estes; PartNo=BT-X; Description=d; Material=[Bulk 'Cardboard' 680.0 shear 4.0E8 PaperProducts builtin]; Image=bytes3:ffffff; InnerDiameter=0.02413; OuterDiameter=0.024790399999999997; Length=0.4572; Type=BODY_TUBE; Thickness=3.3019999999999924E-4; digest=88ddbe03a488913456ab8159a7674a61"}),
        // XH24 thumbnail [++++]
        read("XH24", {"Legacy=false; Manufacturer=Estes; PartNo=BT-X; Description=d; Material=[Bulk 'Cardboard' 680.0 shear 4.0E8 PaperProducts builtin]; Image=bytes3:fbefbe; InnerDiameter=0.02413; OuterDiameter=0.024790399999999997; Length=0.4572; Type=BODY_TUBE; Thickness=3.3019999999999924E-4; digest=88ddbe03a488913456ab8159a7674a61"}),
        // XH25 thumbnail [-_-_]
        read("XH25", {kTubeWithEmptyImage}),
        // XH26 thumbnail [QU!D]
        read("XH26", {kTubeWithEmptyImage}),
        // XH27 thumbnail [QU D]
        read("XH27", {kTubeWithEmptyImage}),
        // XH28 thumbnail [QQ=]
        read("XH28", {kTube}),
        // XH29 thumbnail [QQ=A]
        read("XH29", {"Legacy=false; Manufacturer=Estes; PartNo=BT-X; Description=d; Material=[Bulk 'Cardboard' 680.0 shear 4.0E8 PaperProducts builtin]; Image=bytes2:41c0; InnerDiameter=0.02413; OuterDiameter=0.024790399999999997; Length=0.4572; Type=BODY_TUBE; Thickness=3.3019999999999924E-4; digest=88ddbe03a488913456ab8159a7674a61"}),
        // XH30 thumbnail [A===]
        read("XH30", {"Legacy=false; Manufacturer=Estes; PartNo=BT-X; Description=d; Material=[Bulk 'Cardboard' 680.0 shear 4.0E8 PaperProducts builtin]; Image=bytes1:07; InnerDiameter=0.02413; OuterDiameter=0.024790399999999997; Length=0.4572; Type=BODY_TUBE; Thickness=3.3019999999999924E-4; digest=88ddbe03a488913456ab8159a7674a61"}),
        // XH31 thumbnail [AA==]
        read("XH31", {"Legacy=false; Manufacturer=Estes; PartNo=BT-X; Description=d; Material=[Bulk 'Cardboard' 680.0 shear 4.0E8 PaperProducts builtin]; Image=bytes1:00; InnerDiameter=0.02413; OuterDiameter=0.024790399999999997; Length=0.4572; Type=BODY_TUBE; Thickness=3.3019999999999924E-4; digest=88ddbe03a488913456ab8159a7674a61"}),
        // XH32 thumbnail [QUJ<U+00E9>]
        read("XH32", {kTube}),
        // XH33 thumbnail [QUJDQUJD]
        read("XH33", {"Legacy=false; Manufacturer=Estes; PartNo=BT-X; Description=d; Material=[Bulk 'Cardboard' 680.0 shear 4.0E8 PaperProducts builtin]; Image=bytes6:414243414243; InnerDiameter=0.02413; OuterDiameter=0.024790399999999997; Length=0.4572; Type=BODY_TUBE; Thickness=3.3019999999999924E-4; digest=88ddbe03a488913456ab8159a7674a61"}),
        // XH34 thumbnail [QUJD\tQUJD]
        read("XH34", {kTube}),
        // XH35 thumbnail [QR==]
        read("XH35", {kTubeWithImage41}),
        // XH36 thumbnail [QUK=]
        read("XH36", {"Legacy=false; Manufacturer=Estes; PartNo=BT-X; Description=d; Material=[Bulk 'Cardboard' 680.0 shear 4.0E8 PaperProducts builtin]; Image=bytes2:4142; InnerDiameter=0.02413; OuterDiameter=0.024790399999999997; Length=0.4572; Type=BODY_TUBE; Thickness=3.3019999999999924E-4; digest=88ddbe03a488913456ab8159a7674a61"}),
    };
    return kCases;
}

// clang-format on

/// The texts of @p warnings, escaped as the probe escapes them.
[[nodiscard]] std::vector<std::string> escapedTexts(const QtRocket::WarningSet& warnings)
{
    std::vector<std::string> texts;
    for (const QtRocket::Warning& warning : warnings)
    {
        texts.push_back(QtRocket::Test::orcProbeEscape(warning.toString()));
    }
    return texts;
}

/// Loads the case @p expected names and compares what comes out with it.
void expectCase(const LoaderCase& expected)
{
    SCOPED_TRACE(QtRocket::Test::orcEdgeCase(expected.id).name);
    QtRocket::MaterialStorage materials;
    QtRocket::addBuiltinMaterials(materials);
    const OpenRocketComponentLoader loader(materials);
    QtRocket::WarningSet            warnings;
    const auto                      presets =
        loader.load(QtRocket::Test::orcEdgeCaseBytes(expected.id), "case.orc", warnings);
    if (!presets)
    {
        EXPECT_EQ(std::string(QtRocket::toString(presets.error().code)) + ": " +
                      QtRocket::Test::orcProbeEscape(presets.error().message),
                  expected.refusal);
        EXPECT_TRUE(warnings.empty());
        return;
    }
    EXPECT_EQ(expected.refusal, "");
    EXPECT_EQ(QtRocket::Test::orcProbeLines(*presets),
              std::vector<std::string>(expected.presets.begin(), expected.presets.end()));
    EXPECT_EQ(escapedTexts(warnings),
              std::vector<std::string>(expected.warnings.begin(), expected.warnings.end()));
}

void expectCases(const std::vector<LoaderCase>& cases)
{
    for (const LoaderCase& expected : cases)
    {
        expectCase(expected);
    }
}

TEST(OrcLoaderEdgeCases, PinEveryCaseOfTheProbeOnce)
{
    std::multiset<std::string_view> pinned;
    for (const std::vector<LoaderCase>* cases :
         {&batch1StructureCases(), &batch2NumbersCases(), &batch2IntegersCases(),
          &batch2BooleansCases(), &batch2ShapesCases(), &batch2TextsCases(), &batch3NumbersCases(),
          &batch3IntegersCases(), &batch3BooleansCases(), &batch3ShapesCases(), &batch3TextsCases(),
          &batch3ThumbnailsCases()})
    {
        for (const LoaderCase& expected : *cases)
        {
            pinned.insert(expected.id);
        }
    }
    std::multiset<std::string_view> probed;
    for (const QtRocket::Test::OrcEdgeCase& edgeCase : QtRocket::Test::orcEdgeCases())
    {
        probed.insert(edgeCase.id);
    }
    EXPECT_EQ(pinned, probed);
    EXPECT_EQ(pinned.size(), std::size_t{337});
}

// Batch 1, the scouting's OrcEdgeProbe: structure, units, materials, required elements,
// encodings.
TEST(OrcLoaderEdgeCases, Batch1Structure)
{
    expectCases(batch1StructureCases());
}

// Batch 2, the scouting's OrcEdgeProbe2: the lexical rules of numbers, integers, booleans,
// enums and texts.
TEST(OrcLoaderEdgeCases, Batch2Numbers)
{
    expectCases(batch2NumbersCases());
}

TEST(OrcLoaderEdgeCases, Batch2Integers)
{
    expectCases(batch2IntegersCases());
}

TEST(OrcLoaderEdgeCases, Batch2Booleans)
{
    expectCases(batch2BooleansCases());
}

TEST(OrcLoaderEdgeCases, Batch2Shapes)
{
    expectCases(batch2ShapesCases());
}

TEST(OrcLoaderEdgeCases, Batch2Texts)
{
    expectCases(batch2TextsCases());
}

// Batch 3, part O1's: what the first two leave open, and the base64 thumbnails.
TEST(OrcLoaderEdgeCases, Batch3Numbers)
{
    expectCases(batch3NumbersCases());
}

TEST(OrcLoaderEdgeCases, Batch3Integers)
{
    expectCases(batch3IntegersCases());
}

TEST(OrcLoaderEdgeCases, Batch3Booleans)
{
    expectCases(batch3BooleansCases());
}

TEST(OrcLoaderEdgeCases, Batch3Shapes)
{
    expectCases(batch3ShapesCases());
}

TEST(OrcLoaderEdgeCases, Batch3Texts)
{
    expectCases(batch3TextsCases());
}

TEST(OrcLoaderEdgeCases, Batch3Thumbnails)
{
    expectCases(batch3ThumbnailsCases());
}

}  // namespace
