#include "rocket/preset/TestPresetDatabase.h"

#include <array>
#include <filesystem>
#include <string>

#include <gtest/gtest.h>

#include "QtRocket/file/preset/ComponentPresetDatabaseLoader.h"
#include "QtRocket/logging/Warning.h"
#include "QtRocket/material/BuiltinMaterials.h"
#include "QtRocket/material/MaterialStorage.h"
#include "QtRocket/rocket/preset/ComponentPresetDatabase.h"
#include "QtRocket/util/FileIo.h"
#include "TestPaths.h"

namespace QtRocket::Test
{

namespace
{

/// Reports what went wrong in @p loader's load as a test failure, naming @p what was loaded.
void reportProblems(const ComponentPresetDatabaseLoader& loader, const std::string& what)
{
    for (const ComponentPresetDatabaseLoader::Problem& problem : loader.getProblems())
    {
        ADD_FAILURE() << "loading " << what << ": " << pathToUtf8(problem.file) << ": "
                      << problem.error.toString();
    }
    for (const Warning& warning : loader.getWarnings())
    {
        ADD_FAILURE() << "loading " << what << ": " << warning.toString();
    }
}

}  // namespace

const ComponentPresetDatabase& bundledPresetDatabase()
{
    static const ComponentPresetDatabase kDatabase = [] {
        MaterialStorage materials;
        addBuiltinMaterials(materials);
        ComponentPresetDatabaseLoader loader(materials);
        loader.loadPresetComponents(dataDir() / ComponentPresetDatabaseLoader::kBundledDirectory);
        reportProblems(loader, "the bundled component presets");
        return loader.takeDatabase();
    }();
    return kDatabase;
}

ComponentPresetDatabase smallPresetDatabase()
{
    static const ComponentPresetDatabase kDatabase = [] {
        MaterialStorage materials;
        addBuiltinMaterials(materials);
        ComponentPresetDatabaseLoader              loader(materials);
        const std::array<std::filesystem::path, 3> files{
            testDataDir() / "presets" / "every-kind.orc", testDataDir() / "presets" / "legacy.orc",
            dataDir() / ComponentPresetDatabaseLoader::kBundledDirectory / "database" /
                "estes_ps2.orc"};
        loader.loadUserComponents(files);
        reportProblems(loader, "the small preset database");
        return loader.takeDatabase();
    }();
    return kDatabase;
}

}  // namespace QtRocket::Test
