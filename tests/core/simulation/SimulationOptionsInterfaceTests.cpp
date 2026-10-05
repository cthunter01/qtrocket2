#include "QtRocket/simulation/SimulationOptionsInterface.h"

#include <memory>
#include <type_traits>

#include <gtest/gtest.h>

#include "QtRocket/models/PinkNoiseWindModel.h"
#include "QtRocket/simulation/SimulationOptions.h"
#include "QtRocket/util/GeodeticComputationStrategy.h"
#include "QtRocket/util/Signal.h"
#include "simulation/SimulationOptionsSupport.h"

namespace
{

using QtRocket::GeodeticComputationStrategy;
using QtRocket::PinkNoiseWindModel;
using QtRocket::Signal;
using QtRocket::SimulationOptions;
using QtRocket::SimulationOptionsInterface;
using QtRocket::Test::ChangeCounter;

// OpenRocket has no test of the interface. These show that it is what Java's is: enough to
// copy the launch conditions between any two implementations (DefaultSimulationOptionFactory's
// copyLaunchConditions(), which copies between the preferences and a simulation's options there),
// with SimulationOptions as one implementation and a plain holder as the other.

static_assert(std::is_abstract_v<SimulationOptionsInterface>);
static_assert(std::has_virtual_destructor_v<SimulationOptionsInterface>);
static_assert(std::is_base_of_v<SimulationOptionsInterface, SimulationOptions>);
static_assert(std::is_final_v<SimulationOptions>);

/// An implementation that only stores what it is given: no clamping, no launch-into-wind rule,
/// one change event per setter call.
class PlainLaunchConditions final : public SimulationOptionsInterface
{
public:
    [[nodiscard]] Signal<>& changed() noexcept override { return m_changed; }

    [[nodiscard]] double getLaunchRodLength() const override { return m_launchRodLength; }
    void                 setLaunchRodLength(double launchRodLength) override
    {
        m_launchRodLength = launchRodLength;
        m_changed.emit();
    }

    [[nodiscard]] bool getLaunchIntoWind() const override { return m_launchIntoWind; }
    void               setLaunchIntoWind(bool launchIntoWind) override
    {
        m_launchIntoWind = launchIntoWind;
        m_changed.emit();
    }

    [[nodiscard]] double getLaunchRodAngle() const override { return m_launchRodAngle; }
    void                 setLaunchRodAngle(double launchRodAngle) override
    {
        m_launchRodAngle = launchRodAngle;
        m_changed.emit();
    }

    [[nodiscard]] double getLaunchRodDirection() const override { return m_launchRodDirection; }
    void                 setLaunchRodDirection(double launchRodDirection) override
    {
        m_launchRodDirection = launchRodDirection;
        m_changed.emit();
    }

    [[nodiscard]] PinkNoiseWindModel& getAverageWindModel() noexcept override { return m_wind; }
    [[nodiscard]] const PinkNoiseWindModel& getAverageWindModel() const noexcept override
    {
        return m_wind;
    }

    [[nodiscard]] double getLaunchAltitude() const override { return m_launchAltitude; }
    void                 setLaunchAltitude(double altitude) override
    {
        m_launchAltitude = altitude;
        m_changed.emit();
    }

    [[nodiscard]] double getLaunchLatitude() const override { return m_launchLatitude; }
    void                 setLaunchLatitude(double launchLatitude) override
    {
        m_launchLatitude = launchLatitude;
        m_changed.emit();
    }

    [[nodiscard]] double getLaunchLongitude() const override { return m_launchLongitude; }
    void                 setLaunchLongitude(double launchLongitude) override
    {
        m_launchLongitude = launchLongitude;
        m_changed.emit();
    }

    [[nodiscard]] GeodeticComputationStrategy getGeodeticComputation() const override
    {
        return m_geodeticComputation;
    }
    void setGeodeticComputation(GeodeticComputationStrategy geodeticComputation) override
    {
        m_geodeticComputation = geodeticComputation;
        m_changed.emit();
    }

    [[nodiscard]] bool isIsaAtmosphere() const override { return m_isa; }
    void               setIsaAtmosphere(bool isa) override
    {
        m_isa = isa;
        m_changed.emit();
    }

    [[nodiscard]] double getLaunchTemperature() const override { return m_launchTemperature; }
    void                 setLaunchTemperature(double launchTemperature) override
    {
        m_launchTemperature = launchTemperature;
        m_changed.emit();
    }

    [[nodiscard]] double getLaunchPressure() const override { return m_launchPressure; }
    void                 setLaunchPressure(double launchPressure) override
    {
        m_launchPressure = launchPressure;
        m_changed.emit();
    }

    [[nodiscard]] double getLaunchRelativeHumidity() const override { return m_launchHumidity; }
    void                 setLaunchRelativeHumidity(double launchHumidity) override
    {
        m_launchHumidity = launchHumidity;
        m_changed.emit();
    }

private:
    Signal<>                    m_changed;
    PinkNoiseWindModel          m_wind{1};
    double                      m_launchRodLength{0};
    bool                        m_launchIntoWind{false};
    double                      m_launchRodAngle{0};
    double                      m_launchRodDirection{0};
    double                      m_launchAltitude{0};
    double                      m_launchLatitude{0};
    double                      m_launchLongitude{0};
    GeodeticComputationStrategy m_geodeticComputation{GeodeticComputationStrategy::FLAT};
    bool                        m_isa{false};
    double                      m_launchTemperature{0};
    double                      m_launchPressure{0};
    double                      m_launchHumidity{0};
};

/// DefaultSimulationOptionFactory.copyLaunchConditions() as Java writes it, over the interface
/// alone.
void copyLaunchConditions(const SimulationOptionsInterface& source,
                          SimulationOptionsInterface&       destination)
{
    const PinkNoiseWindModel& sourceWind      = source.getAverageWindModel();
    PinkNoiseWindModel&       destinationWind = destination.getAverageWindModel();
    destinationWind.setAverage(sourceWind.getAverage());
    destinationWind.setStandardDeviation(sourceWind.getStandardDeviation());
    destinationWind.setDirection(sourceWind.getDirection());

    destination.setLaunchLatitude(source.getLaunchLatitude());
    destination.setLaunchLongitude(source.getLaunchLongitude());

    destination.setIsaAtmosphere(source.isIsaAtmosphere());
    destination.setLaunchAltitude(source.getLaunchAltitude());
    destination.setLaunchTemperature(source.getLaunchTemperature());
    destination.setLaunchPressure(source.getLaunchPressure());
    destination.setLaunchRelativeHumidity(source.getLaunchRelativeHumidity());

    destination.setLaunchIntoWind(source.getLaunchIntoWind());
    destination.setLaunchRodLength(source.getLaunchRodLength());
    destination.setLaunchRodAngle(source.getLaunchRodAngle());
    destination.setLaunchRodDirection(source.getLaunchRodDirection());
}

/// Options with launch conditions that are all different from the defaults.
[[nodiscard]] SimulationOptions launchSite()
{
    SimulationOptions options;
    options.getAverageWindModel().setAverage(7.5);
    options.getAverageWindModel().setStandardDeviation(1.25);
    options.getAverageWindModel().setDirection(2.75);
    options.setLaunchLatitude(-33.5);
    options.setLaunchLongitude(151.25);
    options.setIsaAtmosphere(false);
    options.setLaunchAltitude(80.0);
    options.setLaunchTemperature(301.25);
    options.setLaunchPressure(99000.0);
    options.setLaunchRelativeHumidity(0.375);
    options.setLaunchIntoWind(false);
    options.setLaunchRodLength(2.5);
    options.setLaunchRodAngle(0.75);
    options.setLaunchRodDirection(2.125);
    return options;
}

TEST(SimulationOptionsInterface, CopiesTheLaunchConditionsOutOfSimulationOptions)
{
    const SimulationOptions options = launchSite();
    PlainLaunchConditions   plain;
    const ChangeCounter     events(plain.changed());

    copyLaunchConditions(options, plain);

    const SimulationOptionsInterface& copy = plain;
    EXPECT_EQ(copy.getAverageWindModel().getAverage(), 7.5);
    EXPECT_EQ(copy.getAverageWindModel().getStandardDeviation(), 1.25);
    EXPECT_EQ(copy.getAverageWindModel().getDirection(), 2.75);
    EXPECT_EQ(copy.getLaunchLatitude(), -33.5);
    EXPECT_EQ(copy.getLaunchLongitude(), 151.25);
    EXPECT_FALSE(copy.isIsaAtmosphere());
    EXPECT_EQ(copy.getLaunchAltitude(), 80.0);
    EXPECT_EQ(copy.getLaunchTemperature(), 301.25);
    EXPECT_EQ(copy.getLaunchPressure(), 99000.0);
    EXPECT_EQ(copy.getLaunchRelativeHumidity(), 0.375);
    EXPECT_FALSE(copy.getLaunchIntoWind());
    EXPECT_EQ(copy.getLaunchRodLength(), 2.5);
    EXPECT_EQ(copy.getLaunchRodAngle(), 0.75);
    EXPECT_EQ(copy.getLaunchRodDirection(), 2.125);
    // The eleven setters of the interface that the copy calls on the holder itself.
    EXPECT_EQ(events.count(), 11);
}

TEST(SimulationOptionsInterface, CopiesTheLaunchConditionsIntoSimulationOptions)
{
    PlainLaunchConditions plain;
    copyLaunchConditions(launchSite(), plain);
    SimulationOptions   options;
    const ChangeCounter events(options.changed());

    copyLaunchConditions(plain, options);

    const SimulationOptions expected = launchSite();
    EXPECT_EQ(options.getAverageWindModel().getAverage(), 7.5);
    EXPECT_EQ(options.getAverageWindModel().getStandardDeviation(), 1.25);
    EXPECT_EQ(options.getAverageWindModel().getDirection(), 2.75);
    EXPECT_EQ(options.getLaunchLatitude(), expected.getLaunchLatitude());
    EXPECT_EQ(options.getLaunchLongitude(), expected.getLaunchLongitude());
    EXPECT_EQ(options.isIsaAtmosphere(), expected.isIsaAtmosphere());
    EXPECT_EQ(options.getLaunchAltitude(), expected.getLaunchAltitude());
    EXPECT_EQ(options.getLaunchTemperature(), expected.getLaunchTemperature());
    EXPECT_EQ(options.getLaunchPressure(), expected.getLaunchPressure());
    EXPECT_EQ(options.getLaunchRelativeHumidity(), expected.getLaunchRelativeHumidity());
    EXPECT_EQ(options.getLaunchIntoWind(), expected.getLaunchIntoWind());
    EXPECT_EQ(options.getLaunchRodLength(), expected.getLaunchRodLength());
    EXPECT_EQ(options.getLaunchRodAngle(), expected.getLaunchRodAngle());
    EXPECT_EQ(options.getLaunchRodDirection(), expected.getLaunchRodDirection());
    // Three changes of the wind model, which the options forward, and eleven of their own.
    EXPECT_EQ(events.count(), 14);
}

TEST(SimulationOptionsInterface, TheWindIsCopiedBeforeTheRodDirection)
{
    // Launching into the wind, the source's rod direction is its wind direction, whatever it
    // stores itself; the copy's order (the wind first) makes the destination agree.
    SimulationOptions source;
    source.setLaunchRodDirection(1.0);
    source.getAverageWindModel().setDirection(4.0);
    SimulationOptions destination;

    copyLaunchConditions(source, destination);

    EXPECT_EQ(destination.getLaunchRodDirection(), 4.0);
    destination.setLaunchIntoWind(false);
    EXPECT_EQ(destination.getLaunchRodDirection(), 4.0);
}

TEST(SimulationOptionsInterface, SimulationOptionsAppliesItsRulesThroughTheInterface)
{
    SimulationOptions           options;
    SimulationOptionsInterface& launchConditions = options;
    const ChangeCounter         events(launchConditions.changed());

    launchConditions.setLaunchRodAngle(5.0);
    EXPECT_EQ(launchConditions.getLaunchRodAngle(), SimulationOptions::kMaxLaunchRodAngle);
    launchConditions.setLaunchLatitude(100);
    EXPECT_EQ(launchConditions.getLaunchLatitude(), 90.0);
    launchConditions.setGeodeticComputation(GeodeticComputationStrategy::WGS84);
    EXPECT_EQ(launchConditions.getGeodeticComputation(), GeodeticComputationStrategy::WGS84);
    EXPECT_EQ(events.count(), 3);
    // The same change again is no change.
    launchConditions.setGeodeticComputation(GeodeticComputationStrategy::WGS84);
    EXPECT_EQ(events.count(), 3);

    EXPECT_EQ(&launchConditions.getAverageWindModel(), &options.getAverageWindModel());
    EXPECT_EQ(&launchConditions.changed(), &options.changed());
    launchConditions.getAverageWindModel().setAverage(3.0);
    EXPECT_EQ(events.count(), 4);
}

TEST(SimulationOptionsInterface, AnImplementationIsDestroyedThroughTheInterface)
{
    std::unique_ptr<SimulationOptionsInterface> options = std::make_unique<SimulationOptions>();
    options->setLaunchRodLength(2.0);
    EXPECT_EQ(options->getLaunchRodLength(), 2.0);
    options = std::make_unique<PlainLaunchConditions>();
    EXPECT_EQ(options->getLaunchRodLength(), 0.0);
    options.reset();
    EXPECT_EQ(options, nullptr);
}

}  // namespace
