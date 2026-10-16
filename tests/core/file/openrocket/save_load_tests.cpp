// The save and load halves of OpenRocket's ParachuteTest and ShockCordTest
// (core/src/test/.../rocketcomponent): an automatic length is saved as automatic and comes back
// automatic, with the length it then has. The rest of both test classes is ported in
// rocket/ParachuteTests.cpp and rocket/ShockCordTests.cpp; these halves need the document and
// the file format, which a test of rocket/ does not use. Each design is saved by OpenRocketSaver
// with new storage options, as the Java tests save it, and loaded back from the text (Java: a
// file of a temporary directory, which holds nothing else). What the Java tests do not state,
// the saved elements, the warnings of the reload (none) and the numbers, is what OpenRocket
// gives for the same tests (probe SaverPortsProbe of run 10b).

#include <memory>
#include <string>
#include <utility>

#include <gtest/gtest.h>

#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/OpenRocketDocumentFactory.h"
#include "QtRocket/document/StorageOptions.h"
#include "QtRocket/file/GeneralRocketLoader.h"
#include "QtRocket/file/LoadedDocument.h"
#include "QtRocket/file/openrocket/OpenRocketSaver.h"
#include "QtRocket/logging/ErrorSet.h"
#include "QtRocket/logging/Warning.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/Parachute.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/ShockCord.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"
#include "QtRocket/util/MathUtil.h"
#include "file/RocketLoaderTestSupport.h"

namespace
{

using QtRocket::OpenRocketDocument;
using QtRocket::Parachute;
using QtRocket::Result;
using QtRocket::Rocket;
using QtRocket::ShockCord;
using QtRocket::Test::LoaderEnvironment;

/// ParachuteTest.EPSILON and ShockCordTest.EPSILON (MathUtil.EPSILON).
constexpr double kEpsilon = QtRocket::MathUtil::kEpsilon;

/// ParachuteTest.createDocumentWithRecoveryComponents() (and ShockCordTest's, the same): a new
/// design whose stage holds a body tube 1 m long of radius 0.05 m holding a parachute and a
/// shock cord.
[[nodiscard]] std::unique_ptr<OpenRocketDocument> createDocumentWithRecoveryComponents()
{
    std::unique_ptr<OpenRocketDocument> document =
        QtRocket::OpenRocketDocumentFactory::createNewRocket();
    Rocket& rocket = document->getRocket();
    auto&   stage  = dynamic_cast<QtRocket::AxialStage&>(rocket.getChild(0));
    auto    body   = std::make_unique<QtRocket::BodyTube>();
    body->setLength(1.0);
    body->setOuterRadius(0.05);
    QtRocket::BodyTube& added = stage.addChild(std::move(body));
    added.addChild(std::make_unique<Parachute>());
    added.addChild(std::make_unique<ShockCord>());
    return document;
}

/// The first component of the type @p T in @p rocket (the tests' findComponent()), or null with
/// a test failure.
template <typename T>
[[nodiscard]] T* findComponent(Rocket& rocket)
{
    for (QtRocket::RocketComponent& component : rocket.subtree(true))
    {
        if (auto* const found = dynamic_cast<T*>(&component))
        {
            return found;
        }
    }
    ADD_FAILURE() << "Expected component not found";
    return nullptr;
}

/// A design saved and loaded back, with the environment of the load.
struct Reloaded
{
    /// Declared first, destroyed last: the document's simulations would keep its preferences.
    std::unique_ptr<LoaderEnvironment>  environment = std::make_unique<LoaderEnvironment>();
    std::unique_ptr<OpenRocketDocument> document;
    /// The text OpenRocketSaver wrote.
    std::string saved;
    /// The warnings of the load, one per line.
    std::string warnings;
};

/// The tests' saveAndLoad(@p document): saved by OpenRocketSaver with new storage options and
/// loaded back from the text. The document is null, with a test failure, when either fails.
[[nodiscard]] Reloaded saveAndLoad(OpenRocketDocument& document)
{
    Reloaded                            reloaded;
    const QtRocket::InMemoryPreferences preferences;
    QtRocket::WarningSet                saveWarnings;
    QtRocket::ErrorSet                  errors;
    const Result<std::string>           text = QtRocket::OpenRocketSaver::save(
        document, QtRocket::StorageOptions{}, preferences, saveWarnings, errors);
    if (!text.has_value())
    {
        ADD_FAILURE() << "Failed to save test rocket: " << text.error().toString();
        return reloaded;
    }
    EXPECT_TRUE(saveWarnings.empty());
    EXPECT_TRUE(errors.empty());
    reloaded.saved = *text;
    Result<QtRocket::LoadedDocument> loaded =
        QtRocket::GeneralRocketLoader(reloaded.environment->context())
            .load(QtRocket::stringToBytes(reloaded.saved));
    if (!loaded.has_value())
    {
        ADD_FAILURE() << "Failed to load test rocket: " << loaded.error().toString();
        return reloaded;
    }
    for (const QtRocket::Warning& warning : loaded->warnings)
    {
        reloaded.warnings += warning.toString() + "\n";
    }
    reloaded.document = std::move(loaded->document);
    return reloaded;
}

// Port of ParachuteTest.testParachuteLineLengthSaveLoadAutomatic: the line length of the
// parachute is written as "auto" and is automatic again after the load, 1.5 times the diameter
// (OpenRocket: 0.3 m and 0.44999999999999996 m).
TEST(ParachuteSaveLoad, TestParachuteLineLengthSaveLoadAutomatic)
{
    const std::unique_ptr<OpenRocketDocument> document = createDocumentWithRecoveryComponents();
    auto* const parachute = findComponent<Parachute>(document->getRocket());
    ASSERT_NE(parachute, nullptr);
    parachute->setLineLengthAutomatic(true);

    const Reloaded loadedDocument = saveAndLoad(*document);
    ASSERT_NE(loadedDocument.document, nullptr);
    auto* const loadedParachute = findComponent<Parachute>(loadedDocument.document->getRocket());
    ASSERT_NE(loadedParachute, nullptr);

    EXPECT_TRUE(loadedParachute->isLineLengthAutomatic()) << "Line length automatic should persist";
    EXPECT_NEAR(loadedParachute->getDiameter() * 1.5, loadedParachute->getLineLength(), kEpsilon)
        << "Loaded parachute should keep auto line length";

    // What OpenRocket writes and reads back (SaverPortsProbe).
    EXPECT_TRUE(loadedDocument.saved.contains("<linelength>auto</linelength>"));
    EXPECT_EQ(loadedDocument.warnings, "");
    EXPECT_EQ(loadedParachute->getDiameter(), 0.3);
    EXPECT_EQ(loadedParachute->getLineLength(), 0.44999999999999996);
}

// Port of ShockCordTest.testShockCordLengthSaveLoadAutomatic: the length of the shock cord is
// written as "auto" and is automatic again after the load, 3 times the rocket's length
// (OpenRocket: 1.0 m and 3.0 m).
TEST(ShockCordSaveLoad, TestShockCordLengthSaveLoadAutomatic)
{
    const std::unique_ptr<OpenRocketDocument> document = createDocumentWithRecoveryComponents();
    auto* const cord = findComponent<ShockCord>(document->getRocket());
    ASSERT_NE(cord, nullptr);
    cord->setCordLengthAutomatic(true);

    const Reloaded loadedDocument = saveAndLoad(*document);
    ASSERT_NE(loadedDocument.document, nullptr);
    Rocket&     loadedRocket = loadedDocument.document->getRocket();
    auto* const loadedCord   = findComponent<ShockCord>(loadedRocket);
    ASSERT_NE(loadedCord, nullptr);

    EXPECT_TRUE(loadedCord->isCordLengthAutomatic())
        << "Shock cord length automatic should persist";
    EXPECT_NEAR(loadedRocket.getLength() * 3.0, loadedCord->getCordLength(), kEpsilon)
        << "Loaded shock cord should keep auto length";

    // What OpenRocket writes and reads back (SaverPortsProbe).
    EXPECT_TRUE(loadedDocument.saved.contains("<cordlength>auto</cordlength>"));
    EXPECT_EQ(loadedDocument.warnings, "");
    EXPECT_EQ(loadedRocket.getLength(), 1.0);
    EXPECT_EQ(loadedCord->getCordLength(), 3.0);
}

}  // namespace
