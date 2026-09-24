#include "QtRocket/file/motor/GeneralMotorLoader.h"

#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/motor/ThrustCurveMotor.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"
#include "TestPaths.h"

namespace
{

using QtRocket::ErrorCode;
using QtRocket::GeneralMotorLoader;
using QtRocket::Result;
using QtRocket::ThrustCurveMotor;

// TestMotorLoader.java's digests.
constexpr std::string_view kDigest1 = "e523030bc96d5e63313b5723aaea267d";
constexpr std::string_view kDigest2 = "6a41f0f10b7283793eb0e6b389753729";
constexpr std::string_view kDigest3 = "e3164a735f9a50500f2725f0a33d246b";

constexpr std::string_view kA8 = "A8 18 70 3 0.003 0.016 Estes\n0 0\n0.5 5\n1 0\n";

/// TestMotorLoader.test(): the sorted digests of the motors @p loader reads from @p file.
[[nodiscard]] std::vector<std::string> digests(const GeneralMotorLoader& loader,
                                               std::string_view          file)
{
    std::vector<std::string>             found;
    const Result<std::vector<std::byte>> bytes =
        QtRocket::readFile(QtRocket::Test::testDataDir() / "motors" / file);
    EXPECT_TRUE(bytes) << "File " << file << " not found";
    if (!bytes)
    {
        return found;
    }
    const Result<std::vector<ThrustCurveMotor::Builder>> motors =
        loader.load(*bytes, "/file/motor/" + std::string(file));
    EXPECT_TRUE(motors) << motors.error().toString();
    if (!motors)
    {
        return found;
    }
    for (const ThrustCurveMotor::Builder& builder : *motors)
    {
        found.push_back(builder.build().value().getDigest());
    }
    std::ranges::sort(found);
    return found;
}

[[nodiscard]] std::vector<std::string> sorted(std::vector<std::string> values)
{
    std::ranges::sort(values);
    return values;
}

// TestMotorLoader.testGeneralMotorLoader
TEST(GeneralMotorLoader, LoadsEveryTestFile)
{
    const GeneralMotorLoader loader;
    EXPECT_EQ(digests(loader, "test1.eng"), std::vector<std::string>{std::string(kDigest1)});
    EXPECT_EQ(digests(loader, "test2.rse"), std::vector<std::string>{std::string(kDigest2)});
    EXPECT_EQ(digests(loader, "test.zip"), sorted({std::string(kDigest2), std::string(kDigest1)}));
    EXPECT_EQ(digests(loader, "test3.rse"), std::vector<std::string>{std::string(kDigest3)});
}

TEST(GeneralMotorLoader, ChoosesTheLoaderByExtensionIgnoringCase)
{
    const GeneralMotorLoader     loader;
    const std::vector<std::byte> a8 = QtRocket::stringToBytes(kA8);
    for (const std::string_view name : {"x.eng", "X.ENG", "a.Eng", "dir/sub/x.eng", "x.tar.eng"})
    {
        const Result<std::vector<ThrustCurveMotor::Builder>> motors = loader.load(a8, name);
        ASSERT_TRUE(motors) << name;
        EXPECT_EQ(motors->size(), 1U) << name;
    }
    // The same text as RockSim XML is malformed.
    EXPECT_FALSE(loader.load(a8, "x.RSE"));
}

TEST(GeneralMotorLoader, RejectsUnknownFileTypes)
{
    const GeneralMotorLoader     loader;
    const std::vector<std::byte> a8 = QtRocket::stringToBytes(kA8);
    for (const std::string_view name : {"x.txt", "eng", ".eng", "", "x.eng.txt", "x.", "x.en"})
    {
        const Result<std::vector<ThrustCurveMotor::Builder>> motors = loader.load(a8, name);
        ASSERT_FALSE(motors) << name;
        EXPECT_EQ(motors.error().code, ErrorCode::UNSUPPORTED_FORMAT) << name;
        EXPECT_EQ(motors.error().message, "Unknown file type, filename=" + std::string(name));
    }
}

TEST(GeneralMotorLoader, CanLoadTellsTheSupportedNamesApart)
{
    // What load() would refuse by the name alone, before it reads anything.
    const GeneralMotorLoader loader;
    for (const std::string_view name : {"a.eng", "b.RSE", "dir/c.Zip", "x.y.eng"})
    {
        EXPECT_TRUE(loader.canLoad(name)) << name;
    }
    for (const std::string_view name : {"a.txt", "eng", ".eng", "a.eng.bak", "", "a."})
    {
        EXPECT_FALSE(loader.canLoad(name)) << name;
    }
}

TEST(GeneralMotorLoader, SupportedExtensions)
{
    const std::vector<std::string_view> extensions(
        GeneralMotorLoader::getSupportedExtensions().begin(),
        GeneralMotorLoader::getSupportedExtensions().end());
    EXPECT_EQ(extensions, (std::vector<std::string_view>{"rse", "eng", "zip"}));
}

TEST(GeneralMotorLoader, PassesLoaderFailuresOn)
{
    const GeneralMotorLoader                             loader;
    const Result<std::vector<ThrustCurveMotor::Builder>> motors =
        loader.load(QtRocket::stringToBytes("junk\n"), "x.eng");
    ASSERT_FALSE(motors);
    EXPECT_EQ(motors.error().code, ErrorCode::PARSE);
}

}  // namespace
