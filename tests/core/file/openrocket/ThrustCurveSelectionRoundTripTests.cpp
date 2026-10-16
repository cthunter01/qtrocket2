// ThrustCurveSelectionRoundTripTest.java (core/src/test/.../file/openrocket), ported: when a
// design is saved with a specific thrust curve selected for a motor, the same curve (identified
// by its digest) is restored on load. Two synthetic Estes C6 motors with the same metadata but
// different thrust data, and so different digests, make the motor database of the test, as in
// Java (its TwoCurveC6Provider; no component presets). Two flight configurations each use
// another curve. The design is saved by GeneralRocketSaver, as in Java, to bytes and loaded back
// from them (decision O5; Java: a file of a temporary directory). What the Java test does not
// state, the archive's entries, the order of the configurations, the ejection delays and the
// warnings of the load (none), is what OpenRocket gives for the same tests (probe
// SaverPortsProbe of run 10b).

#include <cstddef>
#include <format>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/OpenRocketDocumentFactory.h"
#include "QtRocket/document/StorageOptions.h"
#include "QtRocket/file/GeneralRocketLoader.h"
#include "QtRocket/file/GeneralRocketSaver.h"
#include "QtRocket/file/LoadedDocument.h"
#include "QtRocket/file/ZipArchive.h"
#include "QtRocket/logging/Warning.h"
#include "QtRocket/motor/Manufacturer.h"
#include "QtRocket/motor/Motor.h"
#include "QtRocket/motor/ThrustCurveMotor.h"
#include "QtRocket/motor/ThrustCurveMotorSetDatabase.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/InnerTube.h"
#include "QtRocket/rocket/MotorConfiguration.h"
#include "QtRocket/rocket/MotorMount.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/Strings.h"
#include "file/DesignFileEnvironment.h"

namespace
{

using QtRocket::Coordinate;
using QtRocket::FlightConfigurationId;
using QtRocket::Motor;
using QtRocket::MotorConfiguration;
using QtRocket::OpenRocketDocument;
using QtRocket::Result;
using QtRocket::Rocket;
using QtRocket::ThrustCurveMotor;
using QtRocket::Test::DesignFileEnvironment;

/// ThrustCurveSelectionRoundTripTest.DIGEST_CURVE_1 and DIGEST_CURVE_2.
constexpr std::string_view kDigestCurve1 = "c6-curve-digest-1";
constexpr std::string_view kDigestCurve2 = "c6-curve-digest-2";

/// ThrustCurveSelectionRoundTripTest.buildC6(@p digest, @p time, @p thrust): an Estes C6 of the
/// common name "C6", single use, with the delays 0, 3, 5, 7 s and plugged, 18 by 70 mm, whose CG
/// stays at 35 mm while its mass falls from 25 g by 5 g a point.
[[nodiscard]] std::shared_ptr<const ThrustCurveMotor> buildC6(std::string_view    digest,
                                                              std::vector<double> time,
                                                              std::vector<double> thrust)
{
    std::vector<Coordinate> cg;
    cg.reserve(time.size());
    for (std::size_t i = 0; i < time.size(); i++)
    {
        cg.emplace_back(0.035, 0, 0, 0.025 - (static_cast<double>(i) * 0.005));
    }
    Result<ThrustCurveMotor> motor =
        ThrustCurveMotor::Builder()
            .setManufacturer(QtRocket::Manufacturer::getManufacturer("Estes"))
            .setDesignation("C6")
            .setCommonName("C6")
            .setMotorType(Motor::Type::SINGLE)
            .setStandardDelays({0, 3, 5, 7, Motor::kPluggedDelay})
            .setDiameter(0.018)
            .setLength(0.070)
            .setTimePoints(std::move(time))
            .setThrustPoints(std::move(thrust))
            .setCGPoints(std::move(cg))
            .setDigest(std::string(digest))
            .build();
    EXPECT_TRUE(motor.has_value()) << motor.error().toString();
    return motor.has_value() ? std::make_shared<const ThrustCurveMotor>(std::move(*motor))
                             : nullptr;
}

/// The two curves of the test (its @BeforeAll setup()) and the motor database of exactly these
/// two (TwoCurveC6Provider), in which both are one motor set.
class ThrustCurveSelectionRoundTripTest : public ::testing::Test
{
protected:
    ThrustCurveSelectionRoundTripTest()
    {
        m_database.addMotor(m_c6Curve1);
        m_database.addMotor(m_c6Curve2);
    }

    /// The rocket of the tests' builders: "C6RoundTripRocket" or "C6SingleCurveRocket", a stage
    /// whose body tube (24 mm long, radius 5 cm) holds an inner tube of radius 9 mm, 70 mm long,
    /// the motor mount, which is returned through @p mount.
    [[nodiscard]] static std::unique_ptr<Rocket> rocketWithMotorTube(std::string_view      name,
                                                                     QtRocket::InnerTube*& mount)
    {
        auto rocket = std::make_unique<Rocket>();
        rocket->setName(name);

        QtRocket::AxialStage& stage = rocket->addChild(std::make_unique<QtRocket::AxialStage>());

        QtRocket::BodyTube& bodyTube =
            stage.addChild(std::make_unique<QtRocket::BodyTube>(0.024, 0.05));

        auto motorTube = std::make_unique<QtRocket::InnerTube>();
        motorTube->setOuterRadius(0.009);
        motorTube->setLength(0.07);
        mount = &bodyTube.addChild(std::move(motorTube));
        return rocket;
    }

    /// Gives @p mount the motor @p motor with an ejection delay of 5 s in the new configuration
    /// @p fcid of @p rocket (Java: createFlightConfiguration(); new MotorConfiguration();
    /// setMotor(); setEjectionDelay(5.0); setMotorConfig()).
    static void addConfiguration(Rocket& rocket, QtRocket::InnerTube& mount,
                                 const FlightConfigurationId&            fcid,
                                 std::shared_ptr<const ThrustCurveMotor> motor)
    {
        rocket.createFlightConfiguration(fcid);
        MotorConfiguration config(mount, fcid);
        config.setMotor(std::move(motor));
        config.setEjectionDelay(5.0);
        mount.setMotorConfig(config, fcid);
    }

    /// buildRocketWithTwoConfigs(): the first configuration with curve 1 and selected, the
    /// second with curve 2.
    [[nodiscard]] std::unique_ptr<Rocket> buildRocketWithTwoConfigs(
        const FlightConfigurationId& fcid1, const FlightConfigurationId& fcid2) const
    {
        QtRocket::InnerTube*    motorTube = nullptr;
        std::unique_ptr<Rocket> rocket    = rocketWithMotorTube("C6RoundTripRocket", motorTube);
        // First flight configuration -> curve 1
        addConfiguration(*rocket, *motorTube, fcid1, m_c6Curve1);
        // Second flight configuration -> curve 2
        addConfiguration(*rocket, *motorTube, fcid2, m_c6Curve2);

        rocket->setSelectedConfiguration(fcid1);
        rocket->enableEvents();
        return rocket;
    }

    /// buildMinimalRocket(@p motor) in the configuration @p fcid, which is selected.
    [[nodiscard]] static std::unique_ptr<Rocket> buildMinimalRocket(
        std::shared_ptr<const ThrustCurveMotor> motor, const FlightConfigurationId& fcid)
    {
        QtRocket::InnerTube*    motorTube = nullptr;
        std::unique_ptr<Rocket> rocket    = rocketWithMotorTube("C6SingleCurveRocket", motorTube);
        addConfiguration(*rocket, *motorTube, fcid, std::move(motor));

        rocket->setSelectedConfiguration(fcid);
        rocket->enableEvents();
        return rocket;
    }

    /// What a round trip gave: the reloaded document and the archive's entries.
    struct RoundTrip
    {
        std::unique_ptr<OpenRocketDocument> loaded;
        /// The names of the entries of the saved archive, in their order.
        std::vector<std::string> entries;
        /// The warnings of the load, one per line.
        std::string warnings;
    };

    /// @p rocket in a document, saved without the simulation data by GeneralRocketSaver and
    /// loaded back against the database of the two curves; no document, with a test failure,
    /// when either fails.
    [[nodiscard]] RoundTrip saveAndLoad(std::unique_ptr<Rocket> rocket) const
    {
        const std::unique_ptr<OpenRocketDocument> doc =
            QtRocket::OpenRocketDocumentFactory::createDocumentFromRocket(std::move(rocket));
        QtRocket::StorageOptions options;
        options.setSaveSimulationData(false);

        // Save as zip (.ork)
        RoundTrip                            trip;
        QtRocket::GeneralRocketSaver         saver(m_environment.preferences());
        const Result<std::vector<std::byte>> bytes = saver.saveToBytes(*doc, options);
        if (!bytes.has_value())
        {
            ADD_FAILURE() << bytes.error().toString();
            return trip;
        }
        const Result<QtRocket::ZipArchive> archive = QtRocket::ZipArchive::fromBytes(*bytes);
        if (archive.has_value())
        {
            for (const QtRocket::ZipArchive::Entry& entry : archive->entries())
            {
                trip.entries.push_back(entry.name);
            }
        }

        // Load back
        Result<QtRocket::LoadedDocument> loaded =
            QtRocket::GeneralRocketLoader(m_environment.context()).load(*bytes);
        if (!loaded.has_value())
        {
            ADD_FAILURE() << loaded.error().toString();
            return trip;
        }
        for (const QtRocket::Warning& warning : loaded->warnings)
        {
            trip.warnings += warning.toString() + "\n";
        }
        trip.loaded = std::move(loaded->document);
        return trip;
    }

    std::shared_ptr<const ThrustCurveMotor> m_c6Curve1 =
        buildC6(kDigestCurve1, {0, 0.1, 1.7, 2.0}, {0, 14.0, 9.0, 0});
    std::shared_ptr<const ThrustCurveMotor> m_c6Curve2 =
        buildC6(kDigestCurve2, {0, 0.15, 1.6, 1.9}, {0, 11.0, 10.5, 0});
    QtRocket::ThrustCurveMotorSetDatabase m_database;
    /// Declared after the database, which it looks motors up in, and before the documents the
    /// tests load (their simulations would keep its preference store).
    DesignFileEnvironment m_environment{DesignFileEnvironment::Presets::NONE, m_database};
};

/// The tests' findMotorMount(): the first component of @p rocket that is a motor mount, or null.
[[nodiscard]] const QtRocket::MotorMount* findMotorMount(const Rocket& rocket)
{
    for (const QtRocket::RocketComponent& c : rocket.subtree(true))
    {
        const auto* const mount = dynamic_cast<const QtRocket::MotorMount*>(&c);
        if (mount != nullptr && mount->isMotorMount())
        {
            return mount;
        }
    }
    return nullptr;
}

/// The tests' getDigest(@p mount, @p fcid): the digest of the thrust-curve motor of the
/// configuration, as "<digest>", followed by " <ejection delay>" when @p withDelay; "(none)" for
/// a configuration without a motor, which the Java test fails, and "(not a thrust curve)".
[[nodiscard]] std::string getDigest(const QtRocket::MotorMount&  mount,
                                    const FlightConfigurationId& fcid, bool withDelay = false)
{
    const MotorConfiguration& mc = mount.getMotorConfig(fcid);
    if (mc.getMotor() == nullptr)
    {
        return "(none)";
    }
    const auto* const motor = dynamic_cast<const ThrustCurveMotor*>(mc.getMotor().get());
    if (motor == nullptr)
    {
        return "(not a thrust curve)";
    }
    return withDelay ? std::format("{} {}", motor->getDigest(),
                                   QtRocket::Strings::javaDoubleToString(mc.getEjectionDelay()))
                     : motor->getDigest();
}

/// The digests of the configurations @p fcids of @p rocket's motor mount, with the ejection
/// delay of each (getDigest(withDelay)); "(no motor mount)" for a rocket without one.
[[nodiscard]] std::vector<std::string> digestsOf(const Rocket&                             rocket,
                                                 const std::vector<FlightConfigurationId>& fcids)
{
    const QtRocket::MotorMount* const mount = findMotorMount(rocket);
    if (mount == nullptr)
    {
        return {"(no motor mount)"};
    }
    std::vector<std::string> digests;
    digests.reserve(fcids.size());
    for (const FlightConfigurationId& fcid : fcids)
    {
        digests.push_back(getDigest(*mount, fcid, true));
    }
    return digests;
}

/// Whether @p digest1 and @p digest2 are the two curves, in either order (the Java test's
/// assertion).
[[nodiscard]] bool areTheTwoCurves(std::string_view digest1, std::string_view digest2)
{
    return (digest1 == kDigestCurve1 && digest2 == kDigestCurve2) ||
           (digest1 == kDigestCurve2 && digest2 == kDigestCurve1);
}

// Java: testDifferentCurvesRestoredAfterRoundTrip. A design with two flight configurations each
// using a different C6 thrust curve must restore the correct curve (by digest) after a save/load
// round-trip.
TEST_F(ThrustCurveSelectionRoundTripTest, TestDifferentCurvesRestoredAfterRoundTrip)
{
    // Build rocket with two flight configurations
    const FlightConfigurationId fcid1;
    const FlightConfigurationId fcid2;
    const RoundTrip             trip = saveAndLoad(buildRocketWithTwoConfigs(fcid1, fcid2));
    ASSERT_NE(trip.loaded, nullptr);
    const Rocket& loadedRocket = trip.loaded->getRocket();

    // Collect the two flight config IDs (in insertion order; getIds() leaves the default out, as
    // the Java test does)
    const std::vector<FlightConfigurationId> fcids = loadedRocket.getIds();
    ASSERT_EQ(fcids.size(), 2U) << "Expected 2 flight configurations after round-trip";

    // Find the motor mount
    const QtRocket::MotorMount* const mount = findMotorMount(loadedRocket);
    ASSERT_NE(mount, nullptr) << "Motor mount not found in loaded rocket";

    // Verify each configuration restored the correct curve
    const std::string digest1 = getDigest(*mount, fcids[0]);
    const std::string digest2 = getDigest(*mount, fcids[1]);
    EXPECT_TRUE(areTheTwoCurves(digest1, digest2))
        << "Loaded digests [" << digest1 << ", " << digest2 << "] do not match saved digests ["
        << kDigestCurve1 << ", " << kDigestCurve2 << "]";

    // What OpenRocket saves and reads back (SaverPortsProbe): an embedded curve of each digest,
    // the configurations with their ids in their order, each with its curve and delay, the first
    // selected, and no warning.
    EXPECT_EQ(m_database.getMotorSets().size(), 1U);
    EXPECT_EQ(trip.entries,
              (std::vector<std::string>{"rocket.ork", "thrustcurves/c6-curve-digest-1.rse",
                                        "thrustcurves/c6-curve-digest-2.rse"}));
    EXPECT_EQ(fcids, (std::vector<FlightConfigurationId>{fcid1, fcid2}));
    EXPECT_EQ(digestsOf(loadedRocket, fcids),
              (std::vector<std::string>{"c6-curve-digest-1 5.0", "c6-curve-digest-2 5.0"}));
    EXPECT_EQ(loadedRocket.getSelectedConfiguration().getFlightConfigurationId(), fcid1);
    EXPECT_EQ(trip.warnings, "");
}

// Java: testSingleCurveSelectionPreserved. Saving with curve 1 selected and loading back must
// give exactly curve 1.
TEST_F(ThrustCurveSelectionRoundTripTest, TestSingleCurveSelectionPreserved)
{
    const FlightConfigurationId fcid;
    const RoundTrip             trip = saveAndLoad(buildMinimalRocket(m_c6Curve1, fcid));
    ASSERT_NE(trip.loaded, nullptr);
    const Rocket& loadedRocket = trip.loaded->getRocket();

    const QtRocket::MotorMount* const mount = findMotorMount(loadedRocket);
    ASSERT_NE(mount, nullptr) << "Motor mount not found in loaded rocket";

    const FlightConfigurationId loadedFcid =
        loadedRocket.getSelectedConfiguration().getFlightConfigurationId();
    const std::string loadedDigest = getDigest(*mount, loadedFcid);

    EXPECT_EQ(loadedDigest, kDigestCurve1)
        << "Expected curve 1 digest after round-trip, got: " << loadedDigest;

    // What OpenRocket saves and reads back (SaverPortsProbe).
    EXPECT_EQ(trip.entries,
              (std::vector<std::string>{"rocket.ork", "thrustcurves/c6-curve-digest-1.rse"}));
    EXPECT_EQ(loadedFcid, fcid);
    EXPECT_EQ(loadedRocket.getFlightConfigurationCount(), 1);
    EXPECT_EQ(getDigest(*mount, loadedFcid, true), "c6-curve-digest-1 5.0");
    EXPECT_EQ(trip.warnings, "");
}

}  // namespace
