#pragma once

#include <string_view>

/// The preference keys of OpenRocket's ApplicationPreferences, with the same string values, so
/// that a QtRocket preference store reads like an OpenRocket one key for key.
///
/// Naming: every `public static final String NAME_IN_UPPER_CASE` of ApplicationPreferences.java
/// (and every private one its typed getters use, since the getters are ported too) is the
/// constexpr `kNameInUpperCase`: the Java name split at its underscores, each word capitalised
/// and the rest lower-cased, with a `k` in front (`LAUNCH_USE_ISA` -> kLaunchUseIsa,
/// `OPENGL_ENABLE_MSAA` -> kOpenglEnableMsaa, `OBJ_SRGB` -> kObjSrgb). The three directory names
/// ApplicationPreferences uses as string literals ("componentStyle", "componentMaterials",
/// "componentColors") are here as k...Node constants. The keys DocumentPreferences.java declares
/// (PREF_SHOW_WARNINGS, ...) are members of DocumentPreferences, as in Java.
namespace QtRocket::PreferenceKeys
{

// Well known string keys to preferences.
inline constexpr std::string_view kBodyComponentInsertPositionKey = "BodyComponentInsertPosition";
inline constexpr std::string_view kStageInsertPositionKey         = "StageInsertPosition";
inline constexpr std::string_view kUserThrustCurvesKey            = "UserThrustCurves";
inline constexpr std::string_view kUserComponentPresetsKey        = "UserComponentPresets";

inline constexpr std::string_view kDefaultMachNumber = "DefaultMachNumber";

// Preferences related to units
inline constexpr std::string_view kDisplaySecondaryStability = "DisplaySecondaryStability";

// Preferences related to data export
inline constexpr std::string_view kExportFieldSeparator      = "ExportFieldSeparator";
inline constexpr std::string_view kExportDecimalPlaces       = "ExportDecimalPlaces";
inline constexpr std::string_view kExportExponentialNotation = "ExportExponentialNotation";
inline constexpr std::string_view kExportSimulationComment   = "ExportSimulationComment";
inline constexpr std::string_view kExportFieldNameComment    = "ExportFieldDescriptionComment";
inline constexpr std::string_view kExportEventComments       = "ExportEventComments";
inline constexpr std::string_view kExportCommentCharacter    = "ExportCommentCharacter";
inline constexpr std::string_view kUserLocal                 = "locale";
inline constexpr std::string_view kDefaultDirectory          = "defaultDirectory";
inline constexpr std::string_view kFilePreviewViewType       = "FilePreviewViewType";

inline constexpr std::string_view kPlotShowPoints = "ShowPlotPoints";
inline constexpr std::string_view kPlotShowEvents = "ShowPlotEvents";

/// The welcome dialog key prefix: the stored key is kIgnoreWelcome + "_" + build version.
inline constexpr std::string_view kIgnoreWelcome = "IgnoreWelcome";

inline constexpr std::string_view kCheckUpdates          = "CheckUpdates";
inline constexpr std::string_view kUpdateCheckPermission = "UpdateCheckPermission";

inline constexpr std::string_view kIgnoreUpdateVersions      = "IgnoreUpdateVersions";
inline constexpr std::string_view kCheckBetaUpdates          = "CheckBetaUpdates";
inline constexpr std::string_view kCheckMotorDatabaseUpdates = "CheckMotorDatabaseUpdates";
inline constexpr std::string_view kAutoInstallMotorDatabaseUpdates =
    "AutoInstallMotorDatabaseUpdates";
inline constexpr std::string_view kIgnoreMotorDatabaseUpdateVersions =
    "IgnoreMotorDatabaseUpdateVersions";

inline constexpr std::string_view kMotorDiameterFilter  = "MotorDiameterMatch";
inline constexpr std::string_view kMotorHideSimilar     = "MotorHideSimilar";
inline constexpr std::string_view kMotorHideUnavailable = "MotorHideUnavailable";

inline constexpr std::string_view kMotorNameColumn = "MotorNameColumn";

inline constexpr std::string_view kMatchForeDiameter = "MatchForeDiameter";
inline constexpr std::string_view kMatchAftDiameter  = "MatchAftDiameter";

// Preferences related to multi-level wind CSV import
inline constexpr std::string_view kMultiLevelWindCsvImportHasHeader =
    "MultiLevelWindCSVImportHasHeader";
inline constexpr std::string_view kMultiLevelWindCsvImportAltitudeColumn =
    "MultiLevelWindCSVImportAltitudeColumn";
inline constexpr std::string_view kMultiLevelWindCsvImportAltitudeColumnIndex =
    "MultiLevelWindCSVImportAltitudeColumnIndex";
inline constexpr std::string_view kMultiLevelWindCsvImportAltitudeUnit =
    "MultiLevelWindCSVImportAltitudeUnit";
inline constexpr std::string_view kMultiLevelWindCsvImportSpeedColumn =
    "MultiLevelWindCSVImportSpeedColumn";
inline constexpr std::string_view kMultiLevelWindCsvImportSpeedColumnIndex =
    "MultiLevelWindCSVImportSpeedColumnIndex";
inline constexpr std::string_view kMultiLevelWindCsvImportSpeedUnit =
    "MultiLevelWindCSVImportSpeedUnit";
inline constexpr std::string_view kMultiLevelWindCsvImportDirectionColumn =
    "MultiLevelWindCSVImportDirectionColumn";
inline constexpr std::string_view kMultiLevelWindCsvImportDirectionColumnIndex =
    "MultiLevelWindCSVImportDirectionColumnIndex";
inline constexpr std::string_view kMultiLevelWindCsvImportDirectionUnit =
    "MultiLevelWindCSVImportDirectionUnit";
inline constexpr std::string_view kMultiLevelWindCsvImportStddevColumn =
    "MultiLevelWindCSVImportStddevColumn";
inline constexpr std::string_view kMultiLevelWindCsvImportStddevColumnIndex =
    "MultiLevelWindCSVImportStddevColumnIndex";
inline constexpr std::string_view kMultiLevelWindCsvImportStddevUnit =
    "MultiLevelWindCSVImportStddevUnit";

// Node names
inline constexpr std::string_view kPreferredThrustCurveMotorNode = "PreferredThrustCurveMotors";
inline constexpr std::string_view kAutoOpenLastDesign            = "AutoOpenLastDesign";
inline constexpr std::string_view kOpenLeftmostDesignTab         = "OpenLeftmostDesignTab";
inline constexpr std::string_view kShowDiscardConfirmation       = "IgnoreDiscardEditingWarning";
inline constexpr std::string_view kShowSaveRocketInfo            = "ShowSaveRocketInfo";
inline constexpr std::string_view kShowDiscardSimulationConfirmation =
    "IgnoreDiscardSimulationEditingWarning";
inline constexpr std::string_view kShowDiscardPreferencesConfirmation =
    "IgnoreDiscardPreferencesWarning";
inline constexpr std::string_view kMarkerStyleIcon          = "MarkerStyleIcon";
inline constexpr std::string_view kShowMarkers              = "ShowMarkers";
inline constexpr std::string_view kShowRasaeroFormatWarning = "ShowRASAeroFormatWarning";
inline constexpr std::string_view kShowRocksimFormatWarning = "ShowRockSimFormatWarning";
inline constexpr std::string_view kExportUserDirectories    = "ExportUserDirectories";
inline constexpr std::string_view kExportWindowInformation  = "ExportWindowInformation";

// Preferences related to 3D graphics
inline constexpr std::string_view kOpenglEnabled       = "OpenGLIsEnabled";
inline constexpr std::string_view kOpenglEnableAa      = "OpenGLAntialiasingIsEnabled";
inline constexpr std::string_view kOpenglEnableMsaa    = "OpenGLMultisampleAntialiasingIsEnabled";
inline constexpr std::string_view kOpenglRenderQuality = "OpenGLRenderQuality";
inline constexpr std::string_view kOpenglEnableShadows = "OpenGLShadowsEnabled";
inline constexpr std::string_view kOpenglEnableAmbientOcclusion = "OpenGLAmbientOcclusionEnabled";
inline constexpr std::string_view kOpenglReduceEffectsDuringInteraction =
    "OpenGLReduceEffectsDuringInteraction";
inline constexpr std::string_view kOpenglEnableRoughnessBump  = "OpenGLRoughnessBumpEnabled";
inline constexpr std::string_view kOpenglShowOriginAxes       = "OpenGLShowOriginAxes";
inline constexpr std::string_view kOpenglShowLightVisualizers = "OpenGLShowLightVisualizers";
inline constexpr std::string_view kOpenglShowCameraPointOfInterest =
    "OpenGLShowCameraPointOfInterest";
inline constexpr std::string_view kOpenglDragRotationSensitivity =
    "OpenGLDragRotationSensitivityFactor";
inline constexpr std::string_view kOpenglRotateRocketOnDrag  = "OpenGLRotateRocketOnDrag";
inline constexpr std::string_view kOpenglScaleCaretsWithView = "OpenGLScaleCaretsWithView";
inline constexpr std::string_view kOpenglUseFbo              = "OpenGLUseFBO";

inline constexpr std::string_view kRocketInfoFontSize = "RocketInfoFontSize";

// Preferences related to flight configurations
inline constexpr std::string_view kDefaultFlightConfigName = "DefaultFlightConfigName";

// Preferences Related to Simulations
inline constexpr std::string_view kConfirmDeleteSimulation = "ConfirmDeleteSimulation";
inline constexpr std::string_view kAutoRunSimulations      = "AutoRunSimulations";
inline constexpr std::string_view kLaunchRodLength         = "LaunchRodLength";
inline constexpr std::string_view kLaunchIntoWind          = "LaunchIntoWind";
inline constexpr std::string_view kLaunchRodAngle          = "LaunchRodAngle";
inline constexpr std::string_view kLaunchRodDirection      = "LaunchRodDirection";
inline constexpr std::string_view kWindDirection           = "WindDirection";
inline constexpr std::string_view kWindAverage             = "WindAverage";
inline constexpr std::string_view kWindTurbulence          = "WindTurbulence";
inline constexpr std::string_view kLaunchAltitude          = "LaunchAltitude";
inline constexpr std::string_view kLaunchLatitude          = "LaunchLatitude";
inline constexpr std::string_view kLaunchLongitude         = "LaunchLongitude";
inline constexpr std::string_view kLaunchTemperature       = "LaunchTemperature";
inline constexpr std::string_view kLaunchPressure          = "LaunchPressure";
inline constexpr std::string_view kLaunchRelativeHumidity  = "LaunchRelativeHumidity";
inline constexpr std::string_view kLaunchUseIsa            = "LaunchUseISA";
inline constexpr std::string_view kSimulationTimeStep      = "SimulationTimeStep";
inline constexpr std::string_view kSimulationMaxTime       = "SimulationMaxTime";
inline constexpr std::string_view kGeodeticComputation     = "GeodeticComputationStrategy";
inline constexpr std::string_view kGravityModel            = "GravityModel";
inline constexpr std::string_view kConstantGravityValue    = "ConstantGravityValue";
inline constexpr std::string_view kSimulationStepperMethod = "SimulationStepperMethod";
inline constexpr std::string_view kRecoverySpeedWarning    = "RecoverySpeedWarning";
inline constexpr std::string_view kDrogueLowSpeedWarning   = "DrogueLowSpeedWarning";
inline constexpr std::string_view kRecoveryDrogueMainHighSpeedWarning =
    "RecoveryDrogueMainHighSpeedWarning";
inline constexpr std::string_view kRecoveryDrogueMainLowSpeedWarning =
    "RecoveryDrogueMainLowSpeedWarning";
inline constexpr std::string_view kSimulationRandomSeed      = "SimulationRandomSeed";
inline constexpr std::string_view kSimulationRandomSeedFixed = "SimulationRandomSeedFixed";

inline constexpr std::string_view kUiTheme = "UITheme";

// OBJ Export options (the node and the keys inside it)
inline constexpr std::string_view kObjExportOptionsNode     = "OBJExportOptions";
inline constexpr std::string_view kObjExportChildren        = "ExportChildren";
inline constexpr std::string_view kObjExportAllInstances    = "ExportAllInstances";
inline constexpr std::string_view kObjExportMotors          = "ExportMotors";
inline constexpr std::string_view kObjExportAppearance      = "ExportAppearance";
inline constexpr std::string_view kObjExportAsSeparateFiles = "ExportAsSeparateFiles";
inline constexpr std::string_view kObjRemoveOffset          = "RemoveOffset";
inline constexpr std::string_view kObjTriangulate           = "Triangulate";
inline constexpr std::string_view kObjTriangulationMethod   = "TriangulationMethod";
inline constexpr std::string_view kObjSrgb                  = "sRGB";
inline constexpr std::string_view kObjLod                   = "LOD";
inline constexpr std::string_view kObjScaling               = "Scaling";
//// Coordinate transformer (a node inside the OBJ export options node)
inline constexpr std::string_view kObjTransformerNode = "CoordTransform";
inline constexpr std::string_view kObjXAxis           = "xAxis";
inline constexpr std::string_view kObjYAxis           = "yAxis";
inline constexpr std::string_view kObjZAxis           = "zAxis";
inline constexpr std::string_view kObjOrigXOffs       = "OrigXOffs";
inline constexpr std::string_view kObjOrigYOffs       = "OrigYOffs";
inline constexpr std::string_view kObjOrigZOffs       = "OrigZOffs";

// SVG export options
inline constexpr std::string_view kSvgStrokeColor    = "SVGStrokeColor";
inline constexpr std::string_view kSvgStrokeWidth    = "SVGStrokeWidth";
inline constexpr std::string_view kSvgDrawCrosshair  = "SVGDrawCrosshair";
inline constexpr std::string_view kSvgCrosshairColor = "SVGCrosshairColor";
inline constexpr std::string_view kSvgCrosshairSize  = "SVGCrosshairSize";
inline constexpr std::string_view kSvgShowLabels     = "SVGShowLabels";
inline constexpr std::string_view kSvgLabelColor     = "SVGLabelColor";
inline constexpr std::string_view kSvgPartSpacing    = "SVGPartSpacing";

// Texture generation options
inline constexpr std::string_view kTextureGenerationDpi         = "TextureGenerationDPI";
inline constexpr std::string_view kTextureGenerationDrawOutline = "TextureGenerationDrawOutline";
inline constexpr std::string_view kTextureGenerationOutlinePx   = "TextureGenerationOutlinePx";
inline constexpr std::string_view kTextureGenerationResetTransforms =
    "TextureGenerationResetTransforms";
inline constexpr std::string_view kTextureGenerationOutlineColor = "TextureGenerationOutlineColor";

// The per-component-class directories (nodes) ApplicationPreferences names with string literals.
/// Node of the default line style per component class name (getDefaultLineStyle()).
inline constexpr std::string_view kComponentStyleNode = "componentStyle";
/// Node of the default material (as a storable string) per component class name.
inline constexpr std::string_view kComponentMaterialsNode = "componentMaterials";
/// Node of the default colour ("R,G,B") per component class name (getDefaultColor()).
inline constexpr std::string_view kComponentColorsNode = "componentColors";

/// The separator of the user thrust-curve and component-preset file lists
/// (ApplicationPreferences.SPLIT_CHARACTER).
inline constexpr std::string_view kSplitCharacter = "|";

}  // namespace QtRocket::PreferenceKeys
