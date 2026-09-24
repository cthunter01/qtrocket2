#include "QtRocket/file/motor/ZipFileMotorLoader.h"

#include <algorithm>
#include <cstddef>
#include <initializer_list>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/file/ZipArchive.h"
#include "QtRocket/file/motor/MotorLoader.h"
#include "QtRocket/file/motor/RaspMotorLoader.h"
#include "QtRocket/motor/ThrustCurveMotor.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"
#include "TestPaths.h"

namespace
{

using QtRocket::ErrorCode;
using QtRocket::MotorLoader;
using QtRocket::Result;
using QtRocket::ThrustCurveMotor;
using QtRocket::ZipFileMotorLoader;
using QtRocket::ZipWriter;

// TestMotorLoader.java's digests.
constexpr std::string_view kDigest1  = "e523030bc96d5e63313b5723aaea267d";
constexpr std::string_view kDigest2  = "6a41f0f10b7283793eb0e6b389753729";
constexpr std::string_view kDigestA8 = "8e6fb7d51ee11a4a56ab77a3467bcffe";

constexpr std::string_view kA8 = "A8 18 70 3 0.003 0.016 Estes\n0 0\n0.5 5\n1 0\n";

/// A zip archive of @p entries (name, contents).
[[nodiscard]] std::vector<std::byte> zip(
    std::initializer_list<std::pair<std::string_view, std::string_view>> entries)
{
    ZipWriter writer;
    for (const auto& [name, contents] : entries)
    {
        writer.add(std::string(name), QtRocket::stringToBytes(contents));
    }
    return writer.finish().value();
}

[[nodiscard]] std::vector<std::string> digestsOf(
    const Result<std::vector<ThrustCurveMotor::Builder>>& motors)
{
    std::vector<std::string> digests;
    for (const ThrustCurveMotor::Builder& builder : motors.value())
    {
        digests.push_back(builder.build().value().getDigest());
    }
    return digests;
}

/// A loader that records the names it is given and reads nothing but ".eng" entries.
class RecordingLoader final : public MotorLoader
{
public:
    [[nodiscard]] Result<std::vector<ThrustCurveMotor::Builder>> load(
        std::span<const std::byte> data, std::string_view filename) const override
    {
        m_names.emplace_back(filename);
        if (!filename.ends_with(".eng"))
        {
            return QtRocket::fail(ErrorCode::UNSUPPORTED_FORMAT, "Unknown file type");
        }
        return QtRocket::RaspMotorLoader().load(data, filename);
    }

    [[nodiscard]] const std::vector<std::string>& names() const noexcept { return m_names; }

private:
    mutable std::vector<std::string> m_names;
};

// TestMotorLoader.testZipMotorLoader
TEST(ZipFileMotorLoader, LoadsTestZip)
{
    const Result<std::vector<std::byte>> bytes =
        QtRocket::readFile(QtRocket::Test::testDataDir() / "motors" / "test.zip");
    ASSERT_TRUE(bytes);
    const Result<std::vector<ThrustCurveMotor::Builder>> motors =
        ZipFileMotorLoader().load(*bytes, "/file/motor/test.zip");
    ASSERT_TRUE(motors) << motors.error().toString();
    std::vector<std::string> digests = digestsOf(motors);
    std::ranges::sort(digests);
    std::vector<std::string> expected{std::string(kDigest2), std::string(kDigest1)};
    std::ranges::sort(expected);
    EXPECT_EQ(digests, expected);
}

TEST(ZipFileMotorLoader, PassesFullEntryNamesInArchiveOrderAndSkipsDirectories)
{
    const Result<std::vector<std::byte>> bytes =
        QtRocket::readFile(QtRocket::Test::testDataDir() / "motors" / "test.zip");
    ASSERT_TRUE(bytes);
    const RecordingLoader                                recording;
    const Result<std::vector<ThrustCurveMotor::Builder>> motors =
        ZipFileMotorLoader(recording).load(*bytes, "test.zip");
    ASSERT_TRUE(motors) << motors.error().toString();
    // test.zip holds test1.eng, test.txt, dir/ and dir/test2.rse.
    EXPECT_EQ(recording.names(),
              (std::vector<std::string>{"test1.eng", "test.txt", "dir/test2.rse"}));
    EXPECT_EQ(digestsOf(motors), std::vector<std::string>{std::string(kDigest1)});
}

TEST(ZipFileMotorLoader, DataThatIsNoZipHoldsNoMotors)
{
    // Java's ZipInputStream finds no entry without a local file header.
    for (const std::string_view data : {"", "garbage", "PK", "PK\x05\x06"})
    {
        const Result<std::vector<ThrustCurveMotor::Builder>> motors =
            ZipFileMotorLoader().load(QtRocket::stringToBytes(data), "x.zip");
        ASSERT_TRUE(motors) << data;
        EXPECT_TRUE(motors->empty()) << data;
    }
}

TEST(ZipFileMotorLoader, ACorruptArchiveFails)
{
    std::vector<std::byte> bytes = QtRocket::stringToBytes("PK\x03\x04");
    bytes.resize(200, std::byte{0x5A});
    const Result<std::vector<ThrustCurveMotor::Builder>> motors =
        ZipFileMotorLoader().load(bytes, "x.zip");
    EXPECT_FALSE(motors);
}

TEST(ZipFileMotorLoader, SkipsEntriesOfUnknownTypeAndReadsEmptyOnes)
{
    const Result<std::vector<ThrustCurveMotor::Builder>> motors = ZipFileMotorLoader().load(
        zip({{"a.eng", ""}, {"b.txt", "hello"}, {"c", kA8}, {"d.eng", kA8}}), "x.zip");
    ASSERT_TRUE(motors) << motors.error().toString();
    EXPECT_EQ(digestsOf(motors), std::vector<std::string>{std::string(kDigestA8)});
}

TEST(ZipFileMotorLoader, AnEntryThatFailsFailsTheArchive)
{
    // OpenRocket only skips unknown file types; anything else fails the whole archive.
    const Result<std::vector<ThrustCurveMotor::Builder>> badEng =
        ZipFileMotorLoader().load(zip({{"ok.eng", kA8}, {"bad.eng", "junk\n"}}), "x.zip");
    ASSERT_FALSE(badEng);
    EXPECT_EQ(badEng.error().message,
              "Illegal file format. Motor header line must contain 7 fields:<br>&nbsp designation "
              "diameter length delays propellantWeight totalWeight manufacturer");

    const Result<std::vector<ThrustCurveMotor::Builder>> emptyRse =
        ZipFileMotorLoader().load(zip({{"a.rse", ""}}), "x.zip");
    ASSERT_FALSE(emptyRse);
    EXPECT_EQ(emptyRse.error().message, "Premature end of file.");
}

TEST(ZipFileMotorLoader, ReadsArchivesInsideArchives)
{
    const std::vector<std::byte>                         inner = zip({{"a/b.eng", kA8}});
    const std::string                                    innerText(QtRocket::bytesToString(inner));
    const Result<std::vector<ThrustCurveMotor::Builder>> motors =
        ZipFileMotorLoader().load(zip({{"inner.zip", innerText}}), "outer.zip");
    ASSERT_TRUE(motors) << motors.error().toString();
    EXPECT_EQ(digestsOf(motors), std::vector<std::string>{std::string(kDigestA8)});
}

TEST(ZipFileMotorLoader, KeepsEveryMotorOfEveryEntryInOrder)
{
    const std::string two =
        std::string(kA8) + ";\n" + "B4 18 70 None 0.004 0.02 Estes\n0.2 3\n0.8 0\n";
    const Result<std::vector<ThrustCurveMotor::Builder>> motors =
        ZipFileMotorLoader().load(zip({{"two.eng", two}, {"one.eng", kA8}}), "x.zip");
    ASSERT_TRUE(motors);
    EXPECT_EQ(digestsOf(motors),
              (std::vector<std::string>{std::string(kDigestA8), "ac265c3503aba6955d225b35a670ecde",
                                        std::string(kDigestA8)}));
}

}  // namespace
