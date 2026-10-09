#include "QtRocket/file/DocumentLoadingContext.h"

#include <filesystem>
#include <memory>
#include <optional>

#include <gtest/gtest.h>

#include "QtRocket/document/Attachment.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/OpenRocketDocumentFactory.h"
#include "QtRocket/document/attachments/FileSystemAttachment.h"
#include "QtRocket/file/AttachmentFactory.h"
#include "QtRocket/file/DatabaseMotorFinder.h"
#include "QtRocket/file/FileSystemAttachmentFactory.h"
#include "QtRocket/material/MaterialStorage.h"
#include "QtRocket/motor/ThrustCurveMotorSetDatabase.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/rocket/preset/ComponentPresetDatabase.h"
#include "QtRocket/simulation/extension/SimulationExtensionRegistry.h"

namespace
{

using QtRocket::Attachment;
using QtRocket::AttachmentFactory;
using QtRocket::DocumentLoadingContext;
using QtRocket::FileSystemAttachment;

TEST(DocumentLoadingContext, StartsAsJavasContextDoes)
{
    const DocumentLoadingContext context;
    EXPECT_EQ(context.getFileVersion(), 0);
    EXPECT_EQ(context.getMotorFinder(), nullptr);
    EXPECT_EQ(context.getOpenRocketDocument(), nullptr);
    // What Java takes from its globals is not there until someone hands it in.
    EXPECT_EQ(context.getApplicationMaterials(), nullptr);
    EXPECT_EQ(context.getPreferences(), nullptr);
    EXPECT_EQ(context.getComponentPresetDatabase(), nullptr);
    EXPECT_EQ(context.getSimulationExtensionRegistry(), nullptr);
}

TEST(DocumentLoadingContext, TheAttachmentFactoryIsNeverNull)
{
    // Java: "attachmentFactory = new FileSystemAttachmentFactory()", a factory without a base
    // directory.
    DocumentLoadingContext   context;
    const AttachmentFactory* initial = context.getAttachmentFactory();
    ASSERT_NE(initial, nullptr);
    const std::shared_ptr<Attachment> attachment = initial->getAttachment("decals/a.png");
    const auto* file = dynamic_cast<const FileSystemAttachment*>(attachment.get());
    ASSERT_NE(file, nullptr);
    EXPECT_EQ(file->getLocation(), std::filesystem::path("decals") / "a.png");

    const QtRocket::FileSystemAttachmentFactory factory(std::filesystem::path("base"));
    context.setAttachmentFactory(&factory);
    EXPECT_EQ(context.getAttachmentFactory(), &factory);
    context.setAttachmentFactory(nullptr);
    EXPECT_EQ(context.getAttachmentFactory(), initial);
    // One such factory serves every context.
    EXPECT_EQ(DocumentLoadingContext().getAttachmentFactory(), initial);
}

TEST(DocumentLoadingContext, KeepsWhatItIsGiven)
{
    QtRocket::InMemoryPreferences                       preferences;
    const QtRocket::MaterialStorage                     materials;
    const QtRocket::ComponentPresetDatabase             presets;
    const QtRocket::ThrustCurveMotorSetDatabase         motors;
    const QtRocket::DatabaseMotorFinder                 finder(motors);
    const std::unique_ptr<QtRocket::OpenRocketDocument> document =
        QtRocket::OpenRocketDocumentFactory::createEmptyRocket();
    const QtRocket::SimulationExtensionRegistry extensions =
        QtRocket::SimulationExtensionRegistry::bundled();

    DocumentLoadingContext context;
    context.setFileVersion(110);
    context.setMotorFinder(&finder);
    context.setOpenRocketDocument(document.get());
    context.setApplicationMaterials(&materials);
    context.setPreferences(&preferences);
    context.setComponentPresetDatabase(&presets);
    context.setSimulationExtensionRegistry(&extensions);
    EXPECT_EQ(context.getFileVersion(), 110);
    EXPECT_EQ(context.getMotorFinder(), &finder);
    EXPECT_EQ(context.getOpenRocketDocument(), document.get());
    EXPECT_EQ(context.getApplicationMaterials(), &materials);
    EXPECT_EQ(context.getPreferences(), &preferences);
    EXPECT_EQ(context.getComponentPresetDatabase(), &presets);
    EXPECT_EQ(context.getSimulationExtensionRegistry(), &extensions);

    // A context is a plain value: a copy refers to the same objects.
    const DocumentLoadingContext copy = context;
    EXPECT_EQ(copy.getFileVersion(), 110);
    EXPECT_EQ(copy.getMotorFinder(), &finder);
    EXPECT_EQ(copy.getOpenRocketDocument(), document.get());
    EXPECT_EQ(copy.getPreferences(), &preferences);
    EXPECT_EQ(copy.getSimulationExtensionRegistry(), &extensions);
}

// The directory of the design that is being loaded (not OpenRocket's: a file a design names
// will be confined to it): there is none until the loader says where the design comes from,
// it is kept as it is given, relative or not, and a copy of the context has its own.
TEST(DocumentLoadingContext, KeepsTheDirectoryOfTheDesign)
{
    DocumentLoadingContext context;
    EXPECT_EQ(context.getDesignDirectory(), std::nullopt);

    context.setDesignDirectory(std::filesystem::path("designs") / "mine");
    ASSERT_TRUE(context.getDesignDirectory().has_value());
    EXPECT_EQ(context.getDesignDirectory(), std::filesystem::path("designs") / "mine");

    DocumentLoadingContext copy = context;
    EXPECT_EQ(copy.getDesignDirectory(), std::filesystem::path("designs") / "mine");
    copy.setDesignDirectory(std::filesystem::path("."));
    EXPECT_EQ(copy.getDesignDirectory(), std::filesystem::path("."));
    EXPECT_EQ(context.getDesignDirectory(), std::filesystem::path("designs") / "mine");

    context.setDesignDirectory(std::nullopt);
    EXPECT_EQ(context.getDesignDirectory(), std::nullopt);
    // An empty path is a directory that was given, not "none".
    context.setDesignDirectory(std::filesystem::path());
    EXPECT_TRUE(context.getDesignDirectory().has_value());
}

}  // namespace
