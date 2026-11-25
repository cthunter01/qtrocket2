#include "QtRocket/gui/util/SimulationStatusText.h"

#include <filesystem>
#include <memory>
#include <string>
#include <utility>

#include <QString>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "KeyTranslator.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/OpenRocketDocumentFactory.h"
#include "QtRocket/file/GeneralRocketLoader.h"
#include "QtRocket/file/LoadedDocument.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/logging/SimulationAbort.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/simulation/SimulationOptions.h"
#include "QtRocket/util/Error.h"
#include "TestPaths.h"
#include "file/DesignFileEnvironment.h"

namespace QtRocket::Gui
{

namespace
{

using SimulationStatusTextTest = Test::GuiTest;

/// A design file loaded as the tests load design files.
class LoadedDesign
{
public:
    explicit LoadedDesign(const std::filesystem::path& file)
    {
        Result<LoadedDocument> loaded = GeneralRocketLoader(m_environment.context()).load(file);
        if (!loaded.has_value())
        {
            ADD_FAILURE() << "cannot load the design: " << loaded.error().message;
            m_document = OpenRocketDocumentFactory::createNewRocket();
            return;
        }
        m_document = std::move(loaded->document);
    }

    [[nodiscard]] OpenRocketDocument& document() const noexcept { return *m_document; }

private:
    QtRocket::Test::DesignFileEnvironment m_environment;
    std::unique_ptr<OpenRocketDocument>   m_document;
};

/// "A simple model rocket.ork" of data/examples.
[[nodiscard]] std::filesystem::path simpleModelRocket()
{
    return QtRocket::Test::dataDir() / "examples" / "A simple model rocket.ork";
}

/// OpenRocket's re-save of the saver-coverage document whose stored flight aborted (no motor
/// ignited).
[[nodiscard]] std::filesystem::path storedFlight()
{
    return QtRocket::Test::testDataDir() / "goldens" / "coverage-stored-flight" / "resave" /
           "rocket.ork";
}

TEST_F(SimulationStatusTextTest, IsTheCoresDescriptionWithoutATranslator)
{
    // Java: Simulation.Status.getDescription(sim), the core's English getStatusDescription().
    const LoadedDesign design(simpleModelRocket());
    Simulation&        loaded = *design.document().getSimulation(0);
    EXPECT_EQ(loaded.getStatus(), Simulation::Status::LOADED);
    EXPECT_EQ(SimulationStatusText::description(loaded),
              QtStrings::fromUtf8(loaded.getStatusDescription()));
    EXPECT_EQ(SimulationStatusText::plainDescription(loaded), QStringLiteral("Loaded from file"));

    loaded.getOptions().setLaunchRodLength(loaded.getOptions().getLaunchRodLength() + 0.5);
    EXPECT_EQ(loaded.getStatus(), Simulation::Status::OUTDATED);
    EXPECT_EQ(SimulationStatusText::description(loaded),
              QtStrings::fromUtf8(loaded.getStatusDescription()));
}

TEST_F(SimulationStatusTextTest, NamesTheAbortOfABranch)
{
    // An ABORTED simulation: the description, "<i>: ", the abort's text and "</i><br>" for every
    // branch that aborted (RocketInfo shows it without the markup).
    const LoadedDesign design(storedFlight());
    Simulation&        aborted = *design.document().getSimulation(0);
    ASSERT_EQ(aborted.getStatus(), Simulation::Status::ABORTED);
    EXPECT_EQ(SimulationStatusText::description(aborted),
              QtStrings::fromUtf8(aborted.getStatusDescription()));
    EXPECT_EQ(SimulationStatusText::plainDescription(aborted),
              QStringLiteral("Simulation Aborted: No motors ignited"));
}

TEST_F(SimulationStatusTextTest, TranslatesTheStatusAndTheCauseWithTheirKeys)
{
    const LoadedDesign                      design(storedFlight());
    Simulation&                             aborted = *design.document().getSimulation(0);
    const Gui::Test::InstalledKeyTranslator translator;
    const std::string                       cause =
        std::string(displayKey(SimulationAbort::Cause::NO_MOTORS_FIRED)) + "|No motors ignited";
    EXPECT_EQ(QtStrings::toUtf8(SimulationStatusText::description(aborted)),
              "Simulation.Status.Description.ABORTED|<i><b>Simulation Aborted</b></i><i>: " +
                  cause + "</i><br>");
    EXPECT_EQ(QtStrings::toUtf8(SimulationStatusText::plainDescription(aborted)),
              "Simulation.Status.Description.ABORTED|Simulation Aborted: " + cause);
}

}  // namespace

}  // namespace QtRocket::Gui
