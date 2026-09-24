#pragma once

#include <array>
#include <filesystem>
#include <numbers>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "QtRocket/util/Color.h"
#include "QtRocket/util/LineStyle.h"
#include "QtRocket/util/Signal.h"

namespace QtRocket
{

/// A component class and its ancestors, nearest first, by simple class name, e.g. {"BodyTube",
/// "SymmetricComponent", "BodyComponent", "ExternalComponent", "RocketComponent"}. The per-class
/// lookups (default colour, line style, material) walk it the way ApplicationPreferences.get()
/// walks Class.getSuperclass() up to RocketComponent; the rocket group builds one per component
/// kind.
using ComponentClassChain = std::span<const std::string_view>;

/// A table of (component class simple name, value) fallbacks consulted after the store: Java's
/// `Map<Class<?>, String>` default maps (DEFAULT_COLORS, StaticFieldHolder.DEFAULT_LINE_STYLES).
using ComponentDefaults = std::span<const std::pair<std::string_view, std::string_view>>;

/// The application's preference store (OpenRocket: preferences/ApplicationPreferences and the
/// ORPreferences interface, over java.util.prefs.Preferences). The store part is abstract:
/// InMemoryPreferences is the map-backed implementation, and the GUI adds a QSettings-backed one.
/// Everything typed sits on top of it in this class, as in Java, so every implementation gets the
/// same defaults and conversions.
///
/// ORPreferences itself (the four typed get/put pairs that ApplicationPreferences and
/// DocumentPreferences share) has no C++ counterpart: OpenRocket never uses it polymorphically,
/// so the two classes simply declare the same eight methods and nothing takes "a preferences" of
/// either kind. A group that needs the common interface introduces it then.
///
/// Store model (java.util.prefs): a node holds string values by key and named child nodes; the
/// typed accessors convert as AbstractPreferences does. getBoolean() reads "true" / "false"
/// without regard to case and gives the default for anything else; getInt() parses as
/// Integer.parseInt (Strings::parseInt); getDouble() as Double.parseDouble (Strings::parseDouble);
/// each gives the default when the key is absent or unparsable (getDouble() also rejects the
/// .ork spellings "Inf" / "-Inf" that Strings::parseDouble takes, since Double.parseDouble
/// does not know them). putBoolean() writes "true" / "false", putInt() the decimal digits,
/// putDouble() the shortest digits that read back as the same value, with "NaN", "Infinity"
/// and "-Infinity" spelt as Java does. Deviation: Java's Double.toString spelling ("1.0",
/// "1.0E-5") is not reproduced; the stored text never reaches a .ork file, and it reads back to
/// the same double.
///
/// Change notification (Java: ChangeSource, addChangeListener()/removeChangeListener() are
/// changed().connect()/disconnect()): changed() is emitted by the typed setters that call
/// fireChangeEvent() in ApplicationPreferences (the simulation defaults, the 3D interaction
/// flags), not by the raw put*() calls, as in Java. Note that getLaunchRodDirection() may store
/// and emit (it syncs the launch rod with the wind when launching into the wind), as it does in
/// Java, so it is not const. Deviation: the wind setters (setWindAverage(),
/// setWindTurbulenceIntensity(), setWindDirection() and setWindStandardDeviation() through
/// them) emit changed() too, where Java's ApplicationPreferences stays silent on a wind change:
/// there the PinkNoiseWindModel fires its own listeners and ApplicationPreferences.stateChanged()
/// answers with storeWindModelState(), three putDouble() calls and no fireChangeEvent(). A GUI
/// refreshing on changed() wants the emission, and there is no model here to listen to instead.
///
/// Not ported, to be added by the groups that own their types (each mirrors a method of
/// ApplicationPreferences.java, in its order there):
/// - getPreferences(): the raw java.util.prefs node is this object itself.
/// - getUserComponentPresetFiles(), getUserComponentPresetFilesAsString(),
///   getDefaultUserComponentFile(), setUserComponentPresetFiles(List<File>),
///   getUserThrustCurveFiles(), getUserThrustCurveFilesAsString(), getDefaultUserThrustCurveFile(),
///   setUserThrustCurveFiles(List<File>): need SystemInfo.getUserApplicationDirectory() and create
///   directories; the keys (kUserComponentPresetsKey, kUserThrustCurvesKey) and the separator
///   (kSplitCharacter) are in PreferenceKeys.
/// - getAverageWindModel(), loadWindModelState(), storeWindModelState(), stateChanged(): the
///   PinkNoiseWindModel round trip; the wind keys are read and written directly here
///   (getWindAverage() and friends), and a wind model can be loaded from and stored to them.
/// - getAtmosphericModel(): AtmosphericModel; the ISA conditions setLaunchAltitude() and
///   setISAAtmosphere() need are computed privately here until models/atmosphere exists.
/// - getGeodeticComputation()/setGeodeticComputation(GeodeticComputationStrategy),
///   getGravityModel()/setGravityModel(GravityModelType),
///   getSimulationStepperMethodChoice()/setSimulationStepperMethodChoice(SimulationStepperMethod):
///   ported as the ...Name() pairs on the enum constant names; getEnum()/putEnum() as
///   getEnumName()/putEnumName().
/// - getDefaultComponentMaterial(Class, Material.Type)/setDefaultComponentMaterial(Class,
/// Material):
///   the string halves are getDefaultComponentMaterialString()/setDefaultComponentMaterialString();
///   the Material parsing, type check and the three built-in fallbacks (Databases.findMaterial of
///   "Elastic cord (round 2 mm, 1/16 in)", "Ripstop nylon", "Cardboard") are the material group's.
/// - addUserMaterial(Material), getUserMaterials(), removeUserMaterial(Material),
///   setComponentFavorite(ComponentPreset, Type, boolean), getComponentFavorites(Type): abstract in
///   Java, typed on Material and ComponentPreset.
/// - getUITheme()/setUITheme(Object): the GUI's.
/// - saveOBJExportOptions(OBJExportOptions)/loadOBJExportOptions(Rocket): typed on the OBJ export
///   options and Rocket; the node and key names are in PreferenceKeys (kObj...).
/// - getMultiLevelWindCsvImport{Altitude,Speed,Direction,Stddev}Unit()/set...Unit(Unit): typed on
///   Unit and UnitGroup; the keys are in PreferenceKeys.
/// - getCheckUpdates()'s and getCheckBetaUpdates()'s defaults come from OpenRocket's
///   build.properties (both true); here they are the literals.
class Preferences
{
public:
    virtual ~Preferences() = default;

    Preferences(const Preferences&)            = delete;
    Preferences& operator=(const Preferences&) = delete;
    Preferences(Preferences&&)                 = delete;
    Preferences& operator=(Preferences&&)      = delete;

    // ---------------------------------------------------------------- the store (abstract)

    /// The value stored under @p key in this node, or nullopt (java.util.prefs get(key, null)).
    [[nodiscard]] virtual std::optional<std::string> get(std::string_view key) const = 0;
    /// Stores @p value under @p key in this node (java.util.prefs put()).
    virtual void put(std::string_view key, std::string_view value) = 0;
    /// Removes @p key from this node; nothing happens when it is absent.
    virtual void remove(std::string_view key) = 0;
    /// Removes every key of this node; the child nodes stay (java.util.prefs clear()).
    virtual void clear() = 0;
    /// The keys of this node, sorted.
    [[nodiscard]] virtual std::vector<std::string> keys() const = 0;
    /// The names of this node's child nodes, sorted.
    [[nodiscard]] virtual std::vector<std::string> childrenNames() const = 0;
    /// The child node @p name, created when it does not exist yet (java.util.prefs node()). The
    /// name may be a '/'-separated relative path ("a/b" is getNode("a").getNode("b")); an empty
    /// name or segment is a programming error (BugError). The reference stays valid as long as
    /// this node exists and is not reset.
    [[nodiscard]] virtual Preferences& getNode(std::string_view name) = 0;
    /// The child node @p name, or nullptr when it does not exist; never creates it. Takes the
    /// same paths as getNode().
    [[nodiscard]] virtual const Preferences* findNode(std::string_view name) const noexcept = 0;

    // ---------------------------------------------------- the ORPreferences typed accessors

    [[nodiscard]] bool        getBoolean(std::string_view key, bool defaultValue) const;
    void                      putBoolean(std::string_view key, bool value);
    [[nodiscard]] int         getInt(std::string_view key, int defaultValue) const;
    void                      putInt(std::string_view key, int value);
    [[nodiscard]] double      getDouble(std::string_view key, double defaultValue) const;
    void                      putDouble(std::string_view key, double value);
    [[nodiscard]] std::string getString(std::string_view key, std::string_view defaultValue) const;
    /// Stores @p value, or removes the key for nullopt (Java: putString(key, null) removes).
    void putString(std::string_view key, std::optional<std::string_view> value);

    /// The (directory, key) forms: the value under @p key in the child node @p directory.
    /// getInNode() gives nullopt when the node or the key is absent (Java: getString(dir, key,
    /// null)); neither reading form creates the node. (It is not an overload of the store
    /// primitive get(), which an override in a concrete store would hide.)
    [[nodiscard]] std::optional<std::string> getInNode(std::string_view directory,
                                                       std::string_view key) const;
    [[nodiscard]] std::string getString(std::string_view directory, std::string_view key,
                                        std::string_view defaultValue) const;
    /// Stores @p value under @p key in the child node @p directory (created), or removes the key
    /// for nullopt.
    void putString(std::string_view directory, std::string_view key,
                   std::optional<std::string_view> value);

    /// Emitted by the typed setters below when they change a value (see the class comment).
    [[nodiscard]] Signal<>& changed() noexcept { return m_changed; }

    // ------------------------------------------------------------------- generic helpers

    /// A limited-range integer: the stored value, or @p defaultValue when it is negative or above
    /// @p max (Java: getChoice(String, int, int)).
    [[nodiscard]] int getChoice(std::string_view key, int max, int defaultValue) const;
    /// A limited-range double: the stored value, or @p defaultValue when it is negative or above
    /// @p max; a NaN passes both tests and is returned (Java: getChoice(String, double, double)).
    [[nodiscard]] double getChoice(std::string_view key, double max, double defaultValue) const;
    /// putInt (Java: putChoice).
    void putChoice(std::string_view key, int value);

    /// An enum stored by its constant name: the stored string when it is one of @p names
    /// (compared exactly, as Enum.valueOf), else @p defaultName (Java: getEnum()).
    [[nodiscard]] std::string getEnumName(std::string_view                  key,
                                          std::span<const std::string_view> names,
                                          std::string_view                  defaultName) const;
    /// Stores the constant name, or removes the key for nullopt (Java: putEnum()).
    void putEnumName(std::string_view key, std::optional<std::string_view> name);

    /// A colour stored as "R,G,B": the parsed colour, or @p defaultValue when the key is absent or
    /// does not parse (Java: getORColor()).
    [[nodiscard]] Color getColor(std::string_view key, const Color& defaultValue) const;
    /// Stores stringifyColor(value) (Java: putColor()).
    void putColor(std::string_view key, const Color& value);
    /// Parses "R,G,B" as ApplicationPreferences.parseColor does: exactly three comma-separated
    /// fields (Java's split, so trailing empty fields are ignored), each an Integer.parseInt
    /// integer with no surrounding whitespace, clamped to 0-255. Anything else is nullopt.
    [[nodiscard]] static std::optional<Color> parseColor(std::string_view text);
    /// "R,G,B" (the alpha is not stored).
    [[nodiscard]] static std::string stringifyColor(const Color& color);

    // ------------------------------------------------------------- welcome dialog, updates

    /// Java: setIgnoreWelcome(version, ignore); the key is kIgnoreWelcome + "_" + version.
    void               setIgnoreWelcome(std::string_view version, bool ignore);
    [[nodiscard]] bool getIgnoreWelcome(std::string_view version) const;

    [[nodiscard]] bool getCheckUpdates() const;
    void               setCheckUpdates(bool check);
    /// True once the user has answered the one-time update check question.
    [[nodiscard]] bool isUpdateCheckPermissionSet() const;
    /// Stores the answer after applying it to both update checkers, so an interrupted write asks
    /// again rather than letting a checker go online without a recorded answer.
    void setUpdateCheckPermission(bool allowed);
    /// The stored "\n"-joined list split as Java does: an empty store gives {""} (one empty
    /// version), trailing empty lines are dropped.
    [[nodiscard]] std::vector<std::string> getIgnoreUpdateVersions() const;
    void               setIgnoreUpdateVersions(std::span<const std::string> versions);
    [[nodiscard]] bool getCheckBetaUpdates() const;
    void               setCheckBetaUpdates(bool check);
    [[nodiscard]] bool getCheckMotorDatabaseUpdates() const;
    void               setCheckMotorDatabaseUpdates(bool check);
    [[nodiscard]] bool getAutoInstallMotorDatabaseUpdates() const;
    void               setAutoInstallMotorDatabaseUpdates(bool autoInstall);
    [[nodiscard]] std::vector<std::string> getIgnoreMotorDatabaseUpdateVersions() const;
    void setIgnoreMotorDatabaseUpdateVersions(std::span<const std::string> versions);

    // -------------------------------------------------------------------- 3D interaction

    /// Java: shouldReduceEffectsDuring3DInteraction(); the setter emits changed().
    [[nodiscard]] bool shouldReduceEffectsDuring3DInteraction() const;
    void               setReduceEffectsDuring3DInteraction(bool enabled);
    /// Java: isMSAAEnabled(); the setter emits changed().
    [[nodiscard]] bool isMsaaEnabled() const;
    void               setMsaaEnabled(bool enabled);

    // ---------------------------------------------------------------------- units, files

    [[nodiscard]] bool isDisplaySecondaryStability() const;
    void               setDisplaySecondaryStability(bool check);

    /// The stored directory, or nullopt when none is stored (Java: getDefaultDirectory(), a
    /// File; ported with std::filesystem::path).
    [[nodiscard]] std::optional<std::filesystem::path> getDefaultDirectory() const;
    /// Stores the absolute form of @p directory, or removes the key for nullopt (Java:
    /// setDefaultDirectory(File), which stores File.getAbsolutePath()): a relative path is
    /// resolved against the current directory and an empty one is the current directory itself.
    /// The text stored is the path's UTF-8 form (path::u8string()), so a name outside the
    /// Windows code page survives the round trip as Java's UTF-16 String does.
    void setDefaultDirectory(const std::optional<std::filesystem::path>& directory);

    // ------------------------------------------------------------- flight configurations

    /// Default "[{motors}]" (FlightConfiguration.DEFAULT_CONFIG_NAME).
    [[nodiscard]] std::string getDefaultFlightConfigName() const;
    void                      setDefaultFlightConfigName(std::string_view name);

    // ------------------------------------------------------------------ simulation flags

    [[nodiscard]] bool getConfirmSimDeletion() const;
    void               setConfirmSimDeletion(bool check);
    [[nodiscard]] bool getAutoRunSimulations() const;
    void               setAutoRunSimulations(bool check);
    [[nodiscard]] bool getLaunchIntoWind() const;
    void               setLaunchIntoWind(bool check);
    [[nodiscard]] bool getShowRasaeroFormatWarning() const;
    void               setShowRasaeroFormatWarning(bool check);
    [[nodiscard]] bool getShowRocksimFormatWarning() const;
    void               setShowRocksimFormatWarning(bool check);
    [[nodiscard]] bool getExportUserDirectories() const;
    void               setExportUserDirectories(bool check);
    [[nodiscard]] bool getExportWindowInformation() const;
    void               setExportWindowInformation(bool check);

    // --------------------------------------------------------- simulation defaults (SI)

    /// getChoice(kDefaultMachNumber, 0.9, 0.3). Deviation: Java reads the global application
    /// preferences here; this reads this instance.
    [[nodiscard]] double getDefaultMach() const;
    /// Emits changed() when the value differs (MathUtil::equals).
    void setDefaultMach(double mach);

    /// Launch rod length in m, default 1.
    [[nodiscard]] double getLaunchRodLength() const;
    void                 setLaunchRodLength(double launchRodLength);
    /// Launch rod angle in rad, default 0; the setter clamps to +-kMaxLaunchRodAngle.
    [[nodiscard]] double getLaunchRodAngle() const;
    void                 setLaunchRodAngle(double launchRodAngle);
    /// Launch rod direction in rad, default pi/2. When launching into the wind the wind direction
    /// is stored as the rod direction and returned, which may emit changed().
    [[nodiscard]] double getLaunchRodDirection();
    /// Stores the direction reduced to 0..2pi.
    void setLaunchRodDirection(double launchRodDirection);

    /// The wind keys, read and written directly (Java goes through getAverageWindModel(), a
    /// PinkNoiseWindModel loaded from and stored to these keys). Average wind speed in m/s,
    /// default 2.0; a negative speed is stored as its magnitude blowing the other way (the
    /// direction is turned by pi), as PinkNoiseWindModel.setAverage() does. The setters emit
    /// changed() (see the class comment).
    ///
    /// Deviation: PinkNoiseWindModel keeps the standard deviation and derives the intensity, so
    /// in OpenRocket the stored intensity collapses to 0 (or 1 when the deviation was non-zero)
    /// whenever the average is set to 0, and is not restored when the average becomes non-zero
    /// again (setAverage(0) then setAverage(2) leaves 0); an intensity set while the average is 0
    /// is lost the same way, since the deviation it implies is 0. Here the intensity is what is
    /// stored: it survives a zero average, and setWindTurbulenceIntensity() stores the value
    /// given whatever the average is.
    [[nodiscard]] double getWindAverage() const;
    void                 setWindAverage(double average);
    /// Turbulence intensity = standard deviation / average, default 0.1, never negative.
    [[nodiscard]] double getWindTurbulenceIntensity() const;
    void                 setWindTurbulenceIntensity(double intensity);
    /// Wind speed standard deviation in m/s: turbulence intensity * average. Setting it stores
    /// the intensity PinkNoiseWindModel.getTurbulenceIntensity() derives: deviation / average,
    /// or with a zero average 0 for a zero deviation and 1 otherwise.
    [[nodiscard]] double getWindStandardDeviation() const;
    void                 setWindStandardDeviation(double standardDeviation);
    /// Wind direction in rad, default pi/2 (a wind from the east); stored reduced to 0..2pi.
    [[nodiscard]] double getWindDirection() const;
    void                 setWindDirection(double direction);

    /// Launch site altitude in m, default 0. When the ISA atmosphere is in use the setter also
    /// stores the ISA temperature, pressure and humidity of the new altitude.
    [[nodiscard]] double getLaunchAltitude() const;
    void                 setLaunchAltitude(double altitude);
    /// Launch site latitude in degrees, default 28.61; the setter clamps to +-90.
    [[nodiscard]] double getLaunchLatitude() const;
    void                 setLaunchLatitude(double launchLatitude);
    /// Launch site longitude in degrees, default -80.60; the setter clamps to +-180.
    [[nodiscard]] double getLaunchLongitude() const;
    void                 setLaunchLongitude(double launchLongitude);
    /// Launch site temperature in K, default kStandardTemperature.
    [[nodiscard]] double getLaunchTemperature() const;
    void                 setLaunchTemperature(double launchTemperature);
    /// Launch site pressure in Pa, default kStandardPressure.
    [[nodiscard]] double getLaunchPressure() const;
    void                 setLaunchPressure(double launchPressure);
    /// Launch site relative humidity 0..1, default kStandardRelativeHumidity.
    [[nodiscard]] double getLaunchRelativeHumidity() const;
    void                 setLaunchRelativeHumidity(double launchHumidity);
    /// Whether the launch conditions follow the ISA, default true. Switching it on stores the
    /// ISA temperature, pressure and humidity of the launch altitude.
    [[nodiscard]] bool isIsaAtmosphere() const;
    void               setIsaAtmosphere(bool isa);

    /// The GeodeticComputationStrategy constant name: "FLAT", "SPHERICAL" (default) or "WGS84".
    [[nodiscard]] std::string getGeodeticComputationName() const;
    /// @p name must be one of kGeodeticComputationNames (BugError otherwise).
    void setGeodeticComputationName(std::string_view name);
    /// The GravityModelType constant name: "WGS" (default) or "CONSTANT".
    [[nodiscard]] std::string getGravityModelName() const;
    /// @p name must be one of kGravityModelNames (BugError otherwise).
    void setGravityModelName(std::string_view name);
    /// Gravity in m/s^2 for the CONSTANT gravity model, default 9.807.
    [[nodiscard]] double getConstantGravityValue() const;
    void                 setConstantGravityValue(double value);
    /// The SimulationStepperMethod constant name: "RK4" (default) or "RK6".
    [[nodiscard]] std::string getSimulationStepperMethodName() const;
    /// @p name must be one of kSimulationStepperMethodNames (BugError otherwise).
    void setSimulationStepperMethodName(std::string_view name);

    /// The default random seed of new simulations (a signed 32-bit seed), default 0.
    [[nodiscard]] int getRandomSeed() const;
    void              setRandomSeed(int randomSeed);
    /// Whether new simulations reuse getRandomSeed(), default false.
    [[nodiscard]] bool isRandomSeedFixed() const;
    void               setRandomSeedFixed(bool randomSeedFixed);

    /// Simulation time step in s, default kRecommendedTimeStep.
    [[nodiscard]] double getTimeStep() const;
    void                 setTimeStep(double timeStep);
    /// Maximum simulation time in s, default kRecommendedMaxTime, which a stored 0 also gives.
    [[nodiscard]] double getMaxSimulationTime() const;
    void                 setMaxSimulationTime(double maxTime);

    /// Recovery warning thresholds in m/s: 20.0, 3.048, 30.48 and 15.24.
    [[nodiscard]] double getRecoverySpeedWarning() const;
    void                 setRecoverySpeedWarning(double value);
    [[nodiscard]] double getDrogueLowSpeedWarning() const;
    void                 setDrogueLowSpeedWarning(double value);
    [[nodiscard]] double getRecoveryDrogueMainHighSpeedWarning() const;
    void                 setRecoveryDrogueMainHighSpeedWarning(double value);
    [[nodiscard]] double getRecoveryDrogueMainLowSpeedWarning() const;
    void                 setRecoveryDrogueMainLowSpeedWarning(double value);

    // ------------------------------------------------------------------- GUI behaviour

    void               setAutoOpenLastDesignOnStartup(bool enabled);
    [[nodiscard]] bool isAutoOpenLastDesignOnStartupEnabled() const;
    void               setAlwaysOpenLeftmostTab(bool enabled);
    [[nodiscard]] bool isAlwaysOpenLeftmostTab() const;
    [[nodiscard]] bool isShowDiscardConfirmation() const;
    void               setShowDiscardConfirmation(bool enabled);
    [[nodiscard]] bool isShowSaveRocketInfo() const;
    void               setShowSaveRocketInfo(bool enabled);
    [[nodiscard]] bool isShowDiscardSimulationConfirmation() const;
    void               setShowDiscardSimulationConfirmation(bool enabled);
    [[nodiscard]] bool isShowDiscardPreferencesConfirmation() const;
    void               setShowDiscardPreferencesConfirmation(bool enabled);
    /// Whether pod set / booster markers are shown only while selected (default false: always).
    void               setShowMarkers(bool enabled);
    [[nodiscard]] bool isShowMarkers() const;
    void               setMatchForeDiameter(bool enabled);
    [[nodiscard]] bool isMatchForeDiameter() const;
    void               setMatchAftDiameter(bool enabled);
    [[nodiscard]] bool isMatchAftDiameter() const;
    /// True to show the designation, false the common name, in the motor table's name column.
    [[nodiscard]] bool getMotorNameColumn() const;
    void               setMotorNameColumn(bool value);

    // ------------------------------------------------ per component class defaults

    /// The default line style of a component class: the first "componentStyle" entry along
    /// @p classChain, else the first defaultLineStyles() entry along it, else SOLID; a stored
    /// name that is not a style also gives SOLID (Java: getDefaultLineStyle()). Deviation: the
    /// stored name is matched by lineStyleFromString(), which also takes the .ork spelling and
    /// ignores case, where Java's LineStyle.valueOf takes the exact name only.
    [[nodiscard]] LineStyle getDefaultLineStyle(ComponentClassChain classChain) const;
    /// Stores lineStyleName(style) under @p className in "componentStyle" (Java:
    /// setDefaultLineStyle()).
    void setDefaultLineStyle(std::string_view className, LineStyle style);
    /// The "RocketComponent" -> "SOLID", "MassObject" -> "DASHED" table.
    [[nodiscard]] static ComponentDefaults defaultLineStyles() noexcept;

    /// The stored default material of a component class, as Material.toStorableString() text:
    /// the first "componentMaterials" entry along @p classChain, or nullopt (the string half of
    /// Java's getDefaultComponentMaterial(); see the class comment).
    [[nodiscard]] std::optional<std::string> getDefaultComponentMaterialString(
        ComponentClassChain classChain) const;
    /// Stores @p storableString under @p className in "componentMaterials", or removes it for
    /// nullopt (the string half of Java's setDefaultComponentMaterial()).
    void setDefaultComponentMaterialString(std::string_view                className,
                                           std::optional<std::string_view> storableString);

    /// The default colour of a component class: the first "componentColors" entry along
    /// @p classChain, else the first defaultComponentColors() entry along it, parsed with
    /// parseColor(); black when there is none or it does not parse (Java: getDefaultColor()).
    [[nodiscard]] Color getDefaultColor(ComponentClassChain classChain) const;
    /// Stores stringifyColor(color) under @p className in "componentColors"
    /// (SwingPreferences.setDefaultColor()).
    void setDefaultColor(std::string_view className, const Color& color);
    /// The fallback colour table (Java: the protected DEFAULT_COLORS map, which the Swing GUI
    /// fills from its theme). By default OpenRocket's light theme: body components blue
    /// "0,0,240", fin sets "0,0,200", ..., recovery devices red "255,0,0". A GUI overrides this
    /// to follow its theme.
    [[nodiscard]] virtual ComponentDefaults defaultComponentColors() const noexcept;

    // --------------------------------------------------------------------- SVG export

    /// Stroke colour, default black.
    [[nodiscard]] Color getSvgStrokeColor() const;
    void                setSvgStrokeColor(const Color& color);
    /// Stroke width in mm, default 0.1.
    [[nodiscard]] double getSvgStrokeWidth() const;
    void                 setSvgStrokeWidth(double width);
    [[nodiscard]] bool   isSvgDrawCrosshair() const;
    void                 setSvgDrawCrosshair(bool drawCrosshair);
    /// Crosshair colour, default java.awt.Color.GRAY (128, 128, 128).
    [[nodiscard]] Color getSvgCrosshairColor() const;
    void                setSvgCrosshairColor(const Color& color);
    /// Crosshair size in mm (the length of one full line), default 2.0.
    [[nodiscard]] double getSvgCrosshairSize() const;
    void                 setSvgCrosshairSize(double size);
    [[nodiscard]] bool   isSvgShowLabels() const;
    void                 setSvgShowLabels(bool showLabels);
    /// Label colour, default black.
    [[nodiscard]] Color getSvgLabelColor() const;
    void                setSvgLabelColor(const Color& color);
    /// Part spacing in m, default 0.01.
    [[nodiscard]] double getSvgPartSpacing() const;
    void                 setSvgPartSpacing(double spacing);

    // -------------------------------------------------------------- texture generation

    /// Dots per inch, default 300.
    [[nodiscard]] double getTextureGenerationDpi() const;
    void                 setTextureGenerationDpi(double dpi);
    [[nodiscard]] bool   isTextureGenerationDrawOutline() const;
    void                 setTextureGenerationDrawOutline(bool drawOutline);
    /// Outline thickness in px, default 1; the setter stores max(0, value).
    [[nodiscard]] int  getTextureGenerationOutlinePx() const;
    void               setTextureGenerationOutlinePx(int outlinePx);
    [[nodiscard]] bool isTextureGenerationResetTransforms() const;
    void               setTextureGenerationResetTransforms(bool reset);
    /// Outline colour, default (0, 0, 0).
    [[nodiscard]] Color getTextureGenerationOutlineColor() const;
    void                setTextureGenerationOutlineColor(const Color& color);

    // ------------------------------------------------------ multi-level wind CSV import

    [[nodiscard]] bool        isMultiLevelWindCsvImportHeader() const;
    void                      setMultiLevelWindCsvImportHeader(bool hasHeader);
    [[nodiscard]] std::string getMultiLevelWindCsvImportAltitudeColumn() const;
    void                      setMultiLevelWindCsvImportAltitudeColumn(std::string_view columnName);
    [[nodiscard]] int         getMultiLevelWindCsvImportAltitudeColumnIndex() const;
    void                      setMultiLevelWindCsvImportAltitudeColumnIndex(int columnIndex);
    [[nodiscard]] std::string getMultiLevelWindCsvImportSpeedColumn() const;
    void                      setMultiLevelWindCsvImportSpeedColumn(std::string_view columnName);
    [[nodiscard]] int         getMultiLevelWindCsvImportSpeedColumnIndex() const;
    void                      setMultiLevelWindCsvImportSpeedColumnIndex(int columnIndex);
    [[nodiscard]] std::string getMultiLevelWindCsvImportDirectionColumn() const;
    void              setMultiLevelWindCsvImportDirectionColumn(std::string_view columnName);
    [[nodiscard]] int getMultiLevelWindCsvImportDirectionColumnIndex() const;
    void              setMultiLevelWindCsvImportDirectionColumnIndex(int columnIndex);
    [[nodiscard]] std::string getMultiLevelWindCsvImportStddevColumn() const;
    void                      setMultiLevelWindCsvImportStddevColumn(std::string_view columnName);
    [[nodiscard]] int         getMultiLevelWindCsvImportStddevColumnIndex() const;
    void                      setMultiLevelWindCsvImportStddevColumnIndex(int columnIndex);

    // ------------------------------------------------------------------------ constants

    /// The launch rod angle limit in rad (SimulationOptions.MAX_LAUNCH_ROD_ANGLE = pi/3).
    static constexpr double kMaxLaunchRodAngle = std::numbers::pi / 3;
    /// ISA sea level conditions (ExtendedISAModel.STANDARD_*).
    static constexpr double kStandardTemperature      = 288.15;
    static constexpr double kStandardPressure         = 101325;
    static constexpr double kStandardRelativeHumidity = 0;
    /// AbstractRKSimulationStepper.RECOMMENDED_TIME_STEP and RECOMMENDED_MAX_TIME.
    static constexpr double kRecommendedTimeStep = 0.05;
    static constexpr double kRecommendedMaxTime  = 1200;
    /// FlightConfiguration.DEFAULT_CONFIG_NAME.
    static constexpr std::string_view kDefaultConfigName = "[{motors}]";

    /// The constant names of GeodeticComputationStrategy, GravityModelType and
    /// SimulationStepperMethod, in declaration order.
    static constexpr std::array<std::string_view, 3> kGeodeticComputationNames{"FLAT", "SPHERICAL",
                                                                               "WGS84"};
    static constexpr std::array<std::string_view, 2> kGravityModelNames{"WGS", "CONSTANT"};
    static constexpr std::array<std::string_view, 2> kSimulationStepperMethodNames{"RK4", "RK6"};

protected:
    Preferences() = default;

    /// Emits changed() (Java: fireChangeEvent()).
    void fireChanged() const { m_changed.emit(); }

private:
    /// ApplicationPreferences.get(directory, componentClass, defaultMap): the first value stored
    /// under a name of @p classChain in the node @p directory, else the first @p defaults entry
    /// for a name of the chain, else nullopt.
    [[nodiscard]] std::optional<std::string> lookup(std::string_view    directory,
                                                    ComponentClassChain classChain,
                                                    ComponentDefaults   defaults) const;

    /// Store @p value under @p key unless it equals (MathUtil::equals / ==) what getDouble(key,
    /// defaultValue) gives now; true and changed() emitted when it was stored.
    void setDoubleIfChanged(std::string_view key, double defaultValue, double value);
    void setBoolIfChanged(std::string_view key, bool defaultValue, bool value);
    void setIntIfChanged(std::string_view key, int defaultValue, int value);

    /// Stores the ISA temperature, pressure and humidity of getLaunchAltitude().
    void storeIsaConditions();

    Signal<> m_changed;
};

}  // namespace QtRocket
