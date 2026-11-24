#pragma once

#include "QtRocket/models/PinkNoiseWindModel.h"
#include "QtRocket/simulation/SimulationOptionsInterface.h"
#include "QtRocket/util/GeodeticComputationStrategy.h"
#include "QtRocket/util/Signal.h"

namespace QtRocket
{

class Preferences;

/// The launch preferences as launch conditions (OpenRocket: ApplicationPreferences in its role
/// of a SimulationOptionsInterface): what the Launch preference page edits with the same panel
/// as a simulation's launch conditions (SimulationConditionsPanel over a
/// SimulationOptionsInterface, the average wind only). Preferences does not implement the
/// interface (see SimulationOptionsInterface); this adapter does it over a Preferences, which
/// must outlive it.
///
/// Every value but the wind is the preferences' own: each getter reads the store and each
/// setter writes it, through Preferences' typed accessors (getLaunchRodLength(),
/// setLaunchAltitude(), ...), with their defaults, their clamping, their ISA rule and their
/// change events, as Java's ApplicationPreferences has them. getLaunchRodDirection() is
/// Preferences::getLaunchRodDirection(), which, as in Java, stores the wind direction as the rod
/// direction when launching into the wind (and then may emit changed()): the interface's getter
/// is const, and the store is not this object's state. getGeodeticComputation() and
/// setGeodeticComputation() go through the preference's constant name
/// (Preferences::getGeodeticComputationName()).
///
/// The average wind: a PinkNoiseWindModel of this object, as Java's preferences keep one. It is
/// loaded from the wind keys when this object is made (PinkNoiseWindModel::loadFrom(const
/// Preferences&), Java's loadWindModelState() on the first getAverageWindModel()) and then stored
/// to them, and stored again on every change of the model (PinkNoiseWindModel::storeTo(), Java's
/// stateChanged() and storeWindModelState()). So, as in Java, the keys describe the model, which
/// may rewrite a stored turbulence intensity that was a rounding step away from the one the
/// model derives. Change the wind through getAverageWindModel(), as the panel does.
///
/// Change events: changed() forwards Preferences::changed(), every emission (Java's panel
/// listens to the ApplicationPreferences themselves, which fire for any of their typed
/// setters); Preferences' wind setters emit it (a deviation of Preferences), so a change of the
/// model announces itself through the model's changed() and then through this changed().
///
/// Deviations from OpenRocket:
/// - The wind model follows the wind keys: when Preferences::changed() is emitted and the three
///   wind keys hold other values than after the model was last stored or loaded (something else
///   wrote them: DefaultSimulationOptionFactory::saveDefault(), which writes the keys through a
///   model of its own, or a reset of the preferences), the model is loaded from them again,
///   without storing (its changed() announces it), before the change is forwarded. In Java
///   every writer goes through the one model the preferences keep, so the question does not
///   arise; a cleared store leaves Java's model as it was until the program restarts.
/// - The model is loaded when this object is made, Java's on the first getAverageWindModel(), and
///   stored once it is loaded. Java stores it after each setter of the loading, which passes
///   through half-loaded states (the turbulence intensity 0 of the calm new model after the
///   average was set) and ends with the same keys.
/// - setLaunchIntoWind() and setGeodeticComputation() emit changed() when they change the value.
///   Java's ApplicationPreferences stores both without an event (putBoolean(), putEnum()), and
///   so does Preferences; the GUI binds to this object's changed().
/// - setGeodeticComputation() stores the constant name (Java: putEnum(), the same text).
///
/// Not copyable and not movable: the connections to the preferences and to the model point at
/// it. It belongs to the thread that owns the preferences' change signal (see Preferences,
/// "Threads").
class LaunchPreferences final : public SimulationOptionsInterface
{
public:
    /// The launch conditions of @p preferences, which must outlive this object. Loads the average
    /// wind from them, which may store it (see the class comment).
    explicit LaunchPreferences(Preferences& preferences);
    ~LaunchPreferences() override;

    LaunchPreferences(const LaunchPreferences&)            = delete;
    LaunchPreferences& operator=(const LaunchPreferences&) = delete;
    LaunchPreferences(LaunchPreferences&&)                 = delete;
    LaunchPreferences& operator=(LaunchPreferences&&)      = delete;

    /// The preferences these launch conditions are.
    [[nodiscard]] Preferences& getPreferences() const noexcept { return *m_preferences; }

    /// Emitted whenever the preferences emit theirs, and by setLaunchIntoWind() and
    /// setGeodeticComputation() (see the class comment).
    [[nodiscard]] Signal<>& changed() noexcept override { return m_changed; }

    /// Preferences::getLaunchRodLength() / setLaunchRodLength().
    [[nodiscard]] double getLaunchRodLength() const override;
    void                 setLaunchRodLength(double launchRodLength) override;

    /// Preferences::getLaunchIntoWind(); the setter stores the flag and emits changed() when it
    /// changed (see the class comment).
    [[nodiscard]] bool getLaunchIntoWind() const override;
    void               setLaunchIntoWind(bool launchIntoWind) override;

    /// Preferences::getLaunchRodAngle() / setLaunchRodAngle(), which clamps.
    [[nodiscard]] double getLaunchRodAngle() const override;
    void                 setLaunchRodAngle(double launchRodAngle) override;

    /// Preferences::getLaunchRodDirection(): when launching into the wind, the stored wind
    /// direction, which it stores as the rod direction (see the class comment).
    [[nodiscard]] double getLaunchRodDirection() const override;
    /// Preferences::setLaunchRodDirection(), which reduces to 0 ... 2 pi.
    void setLaunchRodDirection(double launchRodDirection) override;

    /// The average wind, stored to the wind keys on every change (see the class comment).
    [[nodiscard]] PinkNoiseWindModel& getAverageWindModel() noexcept override
    {
        return m_averageWindModel;
    }
    [[nodiscard]] const PinkNoiseWindModel& getAverageWindModel() const noexcept override
    {
        return m_averageWindModel;
    }

    /// Preferences::getLaunchAltitude() / setLaunchAltitude() (with the ISA rule).
    [[nodiscard]] double getLaunchAltitude() const override;
    void                 setLaunchAltitude(double altitude) override;

    /// Preferences::getLaunchLatitude() / setLaunchLatitude(), which clamps.
    [[nodiscard]] double getLaunchLatitude() const override;
    void                 setLaunchLatitude(double launchLatitude) override;

    /// Preferences::getLaunchLongitude() / setLaunchLongitude(), which clamps.
    [[nodiscard]] double getLaunchLongitude() const override;
    void                 setLaunchLongitude(double launchLongitude) override;

    /// The strategy whose constant name the preferences hold (SPHERICAL by default); the setter
    /// stores the name and emits changed() when it changed (see the class comment).
    [[nodiscard]] GeodeticComputationStrategy getGeodeticComputation() const override;
    void setGeodeticComputation(GeodeticComputationStrategy geodeticComputation) override;

    /// Preferences::isIsaAtmosphere() / setIsaAtmosphere() (with the ISA rule).
    [[nodiscard]] bool isIsaAtmosphere() const override;
    void               setIsaAtmosphere(bool isa) override;

    /// Preferences::getLaunchTemperature() / setLaunchTemperature().
    [[nodiscard]] double getLaunchTemperature() const override;
    void                 setLaunchTemperature(double launchTemperature) override;

    /// Preferences::getLaunchPressure() / setLaunchPressure().
    [[nodiscard]] double getLaunchPressure() const override;
    void                 setLaunchPressure(double launchPressure) override;

    /// Preferences::getLaunchRelativeHumidity() / setLaunchRelativeHumidity().
    [[nodiscard]] double getLaunchRelativeHumidity() const override;
    void                 setLaunchRelativeHumidity(double launchHumidity) override;

private:
    /// The values of the three wind keys (Preferences::getWindAverage(),
    /// getWindTurbulenceIntensity(), getWindDirection()).
    struct WindKeys
    {
        double average{0};
        double turbulenceIntensity{0};
        double direction{0};
    };

    /// The wind keys as @p preferences hold them now.
    [[nodiscard]] static WindKeys readWindKeys(const Preferences& preferences);

    /// Whether @p a and @p b are the same values (Double.compare(), so a NaN is a NaN).
    [[nodiscard]] static bool sameWindKeys(const WindKeys& a, const WindKeys& b) noexcept;

    /// Loads the model from the wind keys (Java: loadWindModelState()), without storing it.
    void loadWind();

    /// The model's change: stores it to the wind keys (Java: stateChanged()), unless the model
    /// is being loaded.
    void storeWind();

    /// The preferences' change: loads the model again when the wind keys changed other than by
    /// storeWind() and outside it (see the class comment), then forwards the change.
    void preferencesChanged();

    Preferences*       m_preferences;
    Signal<>           m_changed;
    PinkNoiseWindModel m_averageWindModel;
    /// The wind keys after the model was last stored or loaded.
    WindKeys m_syncedWind;
    /// True while storeWind() writes the keys.
    bool m_storingWind{false};
    /// True while loadWind() loads the model.
    bool m_loadingWind{false};

    /// Declared last: disconnected first.
    Signal<>::ScopedConnection m_windConnection;
    Signal<>::ScopedConnection m_preferencesConnection;
};

}  // namespace QtRocket
