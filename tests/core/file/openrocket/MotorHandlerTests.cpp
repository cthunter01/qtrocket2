#include "QtRocket/file/openrocket/MotorHandler.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <initializer_list>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/document/Attachment.h"
#include "QtRocket/file/AttachmentFactory.h"
#include "QtRocket/file/DatabaseMotorFinder.h"
#include "QtRocket/file/DocumentLoadingContext.h"
#include "QtRocket/file/FileSystemAttachmentFactory.h"
#include "QtRocket/file/ZipArchive.h"
#include "QtRocket/file/ZipFileAttachmentFactory.h"
#include "QtRocket/file/motor/RockSimMotorWriter.h"
#include "QtRocket/file/openrocket/DocumentConfig.h"
#include "QtRocket/file/simplesax/ElementHandler.h"
#include "QtRocket/logging/MessagePriority.h"
#include "QtRocket/logging/Warning.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/motor/Motor.h"
#include "QtRocket/motor/MotorDigest.h"
#include "QtRocket/motor/ThrustCurveMotor.h"
#include "QtRocket/motor/ThrustCurveMotorSetDatabase.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"
#include "QtRocket/util/Strings.h"
#include "TestTempDir.h"
#include "document/TestAttachments.h"
#include "file/ExampleMotors.h"
#include "file/openrocket/HandlerTestSupport.h"
#include "motor/TestMotorDatabase.h"

// Beyond the ten tests of OpenRocket's MotorHandlerTest, the expectations are what OpenRocket's
// MotorHandler answers for the same elements, finder and attachments (probe MotorHandlerProbe
// of part D4), but where a comment says that QtRocket differs.

namespace
{

using QtRocket::Attachment;
using QtRocket::AttachmentFactory;
using QtRocket::BugError;
using QtRocket::DocumentConfig;
using QtRocket::DocumentLoadingContext;
using QtRocket::ElementHandler;
using QtRocket::ErrorCode;
using QtRocket::MessagePriority;
using QtRocket::Motor;
using QtRocket::MotorDigest;
using QtRocket::MotorHandler;
using QtRocket::RockSimMotorWriter;
using QtRocket::ThrustCurveMotor;
using QtRocket::WarningSet;
using QtRocket::Test::ExampleMotor;
using QtRocket::Test::FailingAttachment;
using QtRocket::Test::HandlerFixture;
using QtRocket::Test::HandlerRun;
using QtRocket::Test::kExampleMotors;
using QtRocket::Test::makeEmbeddedTestMotor;
using QtRocket::Test::MemoryAttachment;
using QtRocket::Test::raspStyleDigest;
using QtRocket::Test::runHandler;
using QtRocket::Test::ScriptedMotorFinder;
using QtRocket::Test::warningTexts;

using Texts = std::vector<std::string>;

constexpr double kInfinity = std::numeric_limits<double>::infinity();

// The file version of MotorHandlerTest, and the version table agrees with it.
constexpr int kDigestFileVersion = 104;
static_assert(MotorHandler::kMotorDigestVersion == kDigestFileVersion);
static_assert(MotorHandler::kMotorDigestVersion == (1 * DocumentConfig::kFileVersionDivisor) + 4);

// ---- OpenRocket's MotorHandlerTest ----------------------------------------------------------

/// MotorHandlerTest's attachment factory: the same attachment whatever name is asked for, and
/// none where a compatible database match must not read attachments.
class OneAttachmentFactory final : public AttachmentFactory
{
public:
    explicit OneAttachmentFactory(std::shared_ptr<Attachment> attachment)
      : m_attachment(std::move(attachment))
    {
    }

    [[nodiscard]] std::shared_ptr<Attachment> getAttachment(std::string_view name) const override
    {
        m_asked.emplace_back(name);
        if (m_attachment == nullptr)
        {
            ADD_FAILURE() << "A compatible database match must not read attachments";
            return std::make_shared<FailingAttachment>(std::string(name));
        }
        return m_attachment;
    }

    [[nodiscard]] const Texts& asked() const noexcept { return m_asked; }

private:
    std::shared_ptr<Attachment> m_attachment;
    mutable Texts               m_asked;
};

/// MotorHandlerTest.createHandler(): a handler in a context of file version 1.4 whose database
/// lookup always answers @p databaseMotor (none: it always misses, so that the attachment is
/// what decides) and whose attachment is @p attachment, with @p digest as the element's digest
/// when there is one.
class JavaHandler
{
public:
    JavaHandler(std::optional<std::string_view> digest, std::shared_ptr<Attachment> attachment,
                std::shared_ptr<const Motor> databaseMotor = nullptr)
      : m_finder(std::move(databaseMotor)), m_factory(std::move(attachment)), m_handler(m_context)
    {
        m_context.setFileVersion(kDigestFileVersion);
        m_context.setMotorFinder(&m_finder);
        m_context.setAttachmentFactory(&m_factory);
        if (digest.has_value())
        {
            WarningSet warnings;
            EXPECT_TRUE(m_handler.closeElement("digest", {}, *digest, warnings).has_value());
        }
    }
    ~JavaHandler() = default;

    JavaHandler(const JavaHandler&)            = delete;
    JavaHandler& operator=(const JavaHandler&) = delete;
    JavaHandler(JavaHandler&&)                 = delete;
    JavaHandler& operator=(JavaHandler&&)      = delete;

    [[nodiscard]] std::shared_ptr<const Motor> getMotor(WarningSet& warnings) const
    {
        return m_handler.getMotor(warnings);
    }
    [[nodiscard]] const OneAttachmentFactory& factory() const noexcept { return m_factory; }

private:
    DocumentLoadingContext m_context;
    ScriptedMotorFinder    m_finder;
    OneAttachmentFactory   m_factory;
    MotorHandler           m_handler;
};

[[nodiscard]] std::shared_ptr<const ThrustCurveMotor> createMotor(std::string_view designation,
                                                                  double           peakThrust,
                                                                  std::string_view digest)
{
    return makeEmbeddedTestMotor(std::string(designation), peakThrust, std::string(digest));
}

[[nodiscard]] std::shared_ptr<Attachment> createRseAttachment(const ThrustCurveMotor& motor)
{
    return std::make_shared<MemoryAttachment>("motor.rse", RockSimMotorWriter::write(motor));
}

/// MotorHandlerTest.MissingAttachment: an attachment that models an absent zip entry.
[[nodiscard]] std::shared_ptr<Attachment> missingAttachment(std::string name)
{
    return std::make_shared<FailingAttachment>(std::move(name));
}

[[nodiscard]] bool containsWarning(const WarningSet& warnings, std::string_view text)
{
    return std::ranges::any_of(warnings, [text](const QtRocket::Warning& warning) {
        return warning.toString().contains(text);
    });
}

// MotorHandlerTest.testMatchingAttachmentPreservesRequestedDigest
TEST(MotorHandlerTest, MatchingAttachmentPreservesRequestedDigest)
{
    const std::shared_ptr<const ThrustCurveMotor> prototype       = createMotor("F12X", 12.0, "");
    const std::string                             requestedDigest = raspStyleDigest(*prototype);
    const std::shared_ptr<const ThrustCurveMotor> original =
        createMotor("F12X", 12.0, requestedDigest);
    const JavaHandler handler(requestedDigest, createRseAttachment(*original));
    WarningSet        warnings;

    const std::shared_ptr<const Motor> loaded = handler.getMotor(warnings);

    ASSERT_NE(loaded, nullptr);
    EXPECT_EQ(loaded->getDigest(), requestedDigest);
    EXPECT_EQ(loaded->getMotorType(), Motor::Type::UNKNOWN);
    EXPECT_TRUE(warnings.empty());
}

// MotorHandlerTest.testMissingAttachmentDoesNotAddCorruptionWarning
TEST(MotorHandlerTest, MissingAttachmentDoesNotAddCorruptionWarning)
{
    const std::string requestedDigest = "missing-digest";
    const JavaHandler handler(requestedDigest,
                              missingAttachment("thrustcurves/" + requestedDigest + ".rse"));
    WarningSet        warnings;

    EXPECT_EQ(handler.getMotor(warnings), nullptr);
    EXPECT_TRUE(warnings.empty());
}

// MotorHandlerTest.testCorruptAttachmentAddsWarning
TEST(MotorHandlerTest, CorruptAttachmentAddsWarning)
{
    const std::string requestedDigest = "corrupt-digest";
    const JavaHandler handler(requestedDigest,
                              std::make_shared<MemoryAttachment>(
                                  "thrustcurves/" + requestedDigest + ".rse", "<engine-database>"));
    WarningSet        warnings;

    EXPECT_EQ(handler.getMotor(warnings), nullptr);
    EXPECT_TRUE(containsWarning(warnings, "Unable to load embedded motor attachment"));
    EXPECT_TRUE(containsWarning(warnings, requestedDigest));
}

// MotorHandlerTest.testMismatchedAttachmentAddsWarning
TEST(MotorHandlerTest, MismatchedAttachmentAddsWarning)
{
    const std::shared_ptr<const ThrustCurveMotor> expected        = createMotor("F12X", 12.0, "");
    const std::string                             requestedDigest = raspStyleDigest(*expected);
    const std::shared_ptr<const ThrustCurveMotor> differentMotor =
        createMotor("F12X", 20.0, "different-digest");
    const JavaHandler handler(requestedDigest, createRseAttachment(*differentMotor));
    WarningSet        warnings;

    EXPECT_EQ(handler.getMotor(warnings), nullptr);
    EXPECT_TRUE(containsWarning(warnings, "contains no motor matching digest"));
    EXPECT_TRUE(containsWarning(warnings, requestedDigest));
}

// MotorHandlerTest.testEmbeddedCurvePreferredToApproximateDatabaseMatch
TEST(MotorHandlerTest, EmbeddedCurvePreferredToApproximateDatabaseMatch)
{
    const std::shared_ptr<const ThrustCurveMotor> prototype = createMotor("F12X", 12.0, "");
    const std::string                             digest    = raspStyleDigest(*prototype);
    const std::shared_ptr<const ThrustCurveMotor> original  = createMotor("F12X", 12.0, digest);
    const std::shared_ptr<const ThrustCurveMotor> approximate =
        createMotor("F12X", 20.0, "different-digest");
    const JavaHandler handler(digest, createRseAttachment(*original), approximate);
    WarningSet        warnings;

    const std::shared_ptr<const Motor> loaded = handler.getMotor(warnings);

    ASSERT_NE(loaded, nullptr);
    EXPECT_EQ(loaded->getDigest(), digest);
    const auto* curve = dynamic_cast<const ThrustCurveMotor*>(loaded.get());
    ASSERT_NE(curve, nullptr);
    EXPECT_EQ(curve->getThrustPoints().at(1), 12.0);
    EXPECT_TRUE(warnings.empty());
}

// MotorHandlerTest.testExactDatabaseMatchDoesNotReadAttachment
TEST(MotorHandlerTest, ExactDatabaseMatchDoesNotReadAttachment)
{
    const std::shared_ptr<const ThrustCurveMotor> motor = createMotor("F12X", 12.0, "exact-digest");
    const JavaHandler                             handler("exact-digest", nullptr, motor);
    WarningSet                                    warnings;
    EXPECT_EQ(handler.getMotor(warnings), motor);
    EXPECT_TRUE(handler.factory().asked().empty());
}

// MotorHandlerTest.testHistoricalDigestDatabaseMatchDoesNotReadAttachment
TEST(MotorHandlerTest, HistoricalDigestDatabaseMatchDoesNotReadAttachment)
{
    const std::shared_ptr<const ThrustCurveMotor> motor =
        createMotor("F12X", 12.0, "current-digest");
    const JavaHandler handler(raspStyleDigest(*motor), nullptr, motor);
    WarningSet        warnings;
    EXPECT_EQ(handler.getMotor(warnings), motor);
    EXPECT_TRUE(handler.factory().asked().empty());
}

// MotorHandlerTest.testApproximateDatabaseMatchRetainedWhenAttachmentMissing
TEST(MotorHandlerTest, ApproximateDatabaseMatchRetainedWhenAttachmentMissing)
{
    const std::shared_ptr<const ThrustCurveMotor> motor =
        createMotor("F12X", 20.0, "different-digest");
    WarningSet        warnings;
    const JavaHandler handler("missing-digest", missingAttachment("missing.rse"), motor);
    EXPECT_EQ(handler.getMotor(warnings), motor);
    EXPECT_TRUE(warnings.empty());
}

// MotorHandlerTest.testApproximateDatabaseMatchRetainedWhenAttachmentInvalid
TEST(MotorHandlerTest, ApproximateDatabaseMatchRetainedWhenAttachmentInvalid)
{
    const std::shared_ptr<const ThrustCurveMotor> motor =
        createMotor("F12X", 20.0, "different-digest");
    WarningSet                        warnings;
    const std::shared_ptr<Attachment> corrupt =
        std::make_shared<MemoryAttachment>("bad.rse", "<engine-database>");
    const JavaHandler handler("missing-digest", corrupt, motor);
    EXPECT_EQ(handler.getMotor(warnings), motor);
    EXPECT_TRUE(containsWarning(warnings, "Unable to load embedded motor attachment"));
}

// MotorHandlerTest.testLegacyFileWithoutDigestUsesDatabaseMatch
TEST(MotorHandlerTest, LegacyFileWithoutDigestUsesDatabaseMatch)
{
    const std::shared_ptr<const ThrustCurveMotor> motor =
        createMotor("F12X", 12.0, "current-digest");
    const JavaHandler handler(std::nullopt, nullptr, motor);
    WarningSet        warnings;
    EXPECT_EQ(handler.getMotor(warnings), motor);
    EXPECT_TRUE(handler.factory().asked().empty());
}

// ---- the elements ----------------------------------------------------------------------------

/// What a <motor> element with the children @p children makes of them, read in a context of
/// file version @p version whose finder finds nothing and whose attachments are all missing.
struct Element
{
    Texts       closeWarnings;  ///< the warnings of reading the children
    double      delay{0.0};     ///< getDelay()
    Texts       delayWarnings;  ///< its warnings
    double      nozzle{0.0};    ///< getNozzleExitDiameter()
    std::string query;          ///< what getMotor() asked the finder
    Texts       attachments;    ///< the attachments getMotor() asked for
};

[[nodiscard]] Element readChildren(std::string_view children,
                                   int              version = HandlerFixture::kFileVersion)
{
    HandlerFixture fixture;
    fixture.context().setFileVersion(version);
    MotorHandler     handler(fixture.context());
    const HandlerRun run = runHandler(handler, std::format("<motor>{}</motor>", children));
    EXPECT_TRUE(run.result.has_value());

    Element element;
    element.closeWarnings = run.texts();
    WarningSet delayWarnings;
    element.delay         = handler.getDelay(delayWarnings);
    element.delayWarnings = warningTexts(delayWarnings);
    element.nozzle        = handler.getNozzleExitDiameter();
    WarningSet motorWarnings;
    EXPECT_EQ(handler.getMotor(motorWarnings), nullptr);
    EXPECT_TRUE(motorWarnings.empty());
    element.query       = fixture.motorFinder().queries().empty()
                              ? std::string()
                              : fixture.motorFinder().queries().back();
    element.attachments = fixture.attachments().asked();
    return element;
}

/// The finder's query for the criteria given, the others being none.
[[nodiscard]] std::string query(std::string_view type         = "null",
                                std::string_view manufacturer = "null",
                                std::string_view designation  = "null",
                                std::string_view digest       = "null")
{
    return std::format(
        "find(type={}, manufacturer={}, designation={}, diameter=NaN, length=NaN, "
        "digest={})",
        type, manufacturer, designation, digest);
}

/// The warnings of getDelay() for a delay that is not there.
[[nodiscard]] Texts noDelay()
{
    return {"Motor delay not specified, assuming no ejection charge."};
}

/// @p texts joined by "; ".
[[nodiscard]] std::string joined(const Texts& texts)
{
    std::string result;
    for (const std::string& text : texts)
    {
        result += (result.empty() ? "" : "; ") + text;
    }
    return result;
}

/// @p function of every one of @p texts.
template <class Function>
[[nodiscard]] Texts each(std::initializer_list<std::string_view> texts, Function function)
{
    Texts results;
    for (const std::string_view text : texts)
    {
        results.emplace_back(function(text));
    }
    return results;
}

/// What a child <@p name> with the text @p text makes the handler ask the finder, and the
/// warnings of reading it: "<query> | <warnings>".
[[nodiscard]] std::string childOf(std::string_view name, std::string_view text)
{
    const Element element = readChildren(std::format("<{0}>{1}</{0}>", name, text));
    return element.query + " | " + joined(element.closeWarnings);
}

[[nodiscard]] std::string typeOf(std::string_view text)
{
    return childOf("type", text);
}

[[nodiscard]] std::string diameterOf(std::string_view text)
{
    return childOf("diameter", text);
}

[[nodiscard]] std::string lengthOf(std::string_view text)
{
    return childOf("length", text);
}

/// What a <digest> with the text @p text makes the handler ask for, in a file of the format
/// @p version: "<query> | <attachments> | <warnings>".
[[nodiscard]] std::string digestOf(std::string_view text,
                                   int              version = HandlerFixture::kFileVersion)
{
    const Element element = readChildren(std::format("<digest>{}</digest>", text), version);
    return element.query + " | " + joined(element.attachments) + " | " +
           joined(element.closeWarnings);
}

TEST(MotorHandlerElements, AnEmptyElementHasNoMotorAndThePluggedDelay)
{
    const Element element = readChildren("");
    EXPECT_EQ(element.closeWarnings, Texts{});
    EXPECT_EQ(element.delay, kInfinity);
    EXPECT_EQ(element.delayWarnings, noDelay());
    EXPECT_EQ(element.nozzle, 0.0);
    EXPECT_EQ(element.query, query());
    EXPECT_EQ(element.attachments, Texts{});
}

TEST(MotorHandlerElements, TheTypeIsOneOfTheFourNamesInLowerCase)
{
    EXPECT_EQ(each({"single", "reload", "hybrid", "unknown", " single "}, typeOf),
              (Texts{query("SINGLE") + " | ", query("RELOAD") + " | ", query("HYBRID") + " | ",
                     query("UNKNOWN") + " | ", query("SINGLE") + " | "}));
    // Anything else is no type; the warning shows the text trimmed.
    EXPECT_EQ(each({"Single", "", "bogus", "SINGLE", " Rocket "}, typeOf),
              (Texts{query() + " | Unknown motor type 'Single', ignoring.",
                     query() + " | Unknown motor type '', ignoring.",
                     query() + " | Unknown motor type 'bogus', ignoring.",
                     query() + " | Unknown motor type 'SINGLE', ignoring.",
                     query() + " | Unknown motor type 'Rocket', ignoring."}));
    // An unknown type takes back an earlier one.
    EXPECT_EQ(readChildren("<type>single</type><type>x</type>").query, query());
}

TEST(MotorHandlerElements, TheManufacturerAndTheDesignationAreTrimmed)
{
    EXPECT_EQ(readChildren("<manufacturer>  Estes  </manufacturer>").query, query("null", "Estes"));
    EXPECT_EQ(readChildren("<manufacturer></manufacturer>").query, query("null", ""));
    EXPECT_EQ(readChildren("<designation>  C6  </designation>").query, query("null", "null", "C6"));
    EXPECT_EQ(readChildren("<designation/>").query, query("null", "null", ""));
    EXPECT_EQ(readChildren("<manufacturer>  Estes  </manufacturer>").closeWarnings, Texts{});
}

TEST(MotorHandlerElements, TheDigestCountsFromFormatOnePointFour)
{
    const std::string ignored = query() + " |  | ";
    const std::string read    = query("null", "null", "null", "abc") + " | thrustcurves/abc.rse | ";
    EXPECT_EQ(digestOf(" abc ", 0), ignored);
    EXPECT_EQ(digestOf(" abc ", 103), ignored);
    EXPECT_EQ(digestOf(" abc ", 104), read);
    EXPECT_EQ(digestOf(" abc ", 111), read);
    // An empty digest is given to the finder and is no name of an attachment.
    EXPECT_EQ(digestOf(""), query("null", "null", "null", "") + " |  | ");
    EXPECT_EQ(digestOf("   "), query("null", "null", "null", "") + " |  | ");
}

TEST(MotorHandlerElements, TheDiameterAndTheLengthAreReadAndNotSearchedFor)
{
    const std::string read    = query() + " | ";
    const std::string illegal = query() + " | Illegal motor diameter specified, ignoring.";
    EXPECT_EQ(each({"0.018", " 0.018 ", "Infinity", "-Infinity", "1e2", "0x1p3", "1.5d", "-1"},
                   diameterOf),
              Texts(8, read));
    EXPECT_EQ(each({"abc", "NaN", ""}, diameterOf), Texts(3, illegal));
    // The warning names the diameter for the length too.
    EXPECT_EQ(
        each({"0.018", " 0.018 ", "Infinity", "-Infinity", "1e2", "0x1p3", "1.5d", "-1"}, lengthOf),
        Texts(8, read));
    EXPECT_EQ(each({"abc", "NaN", ""}, lengthOf), Texts(3, illegal));
}

TEST(MotorHandlerElements, KeepsTheDiameterAndTheLengthItRead)
{
    HandlerFixture fixture;
    MotorHandler   handler(fixture.context());
    EXPECT_TRUE(std::isnan(handler.getDiameter()));
    EXPECT_TRUE(std::isnan(handler.getLength()));
    const HandlerRun run =
        runHandler(handler,
                   "<motor><type>reload</type><manufacturer>AeroTech</manufacturer>"
                   "<designation>H148R</designation><digest>abc</digest><diameter>0.038</diameter>"
                   "<length>0.152</length></motor>");
    EXPECT_TRUE(run.result.has_value());
    EXPECT_EQ(handler.getType(), std::optional<Motor::Type>(Motor::Type::RELOAD));
    EXPECT_EQ(handler.getManufacturer(), std::optional<std::string>("AeroTech"));
    EXPECT_EQ(handler.getDesignation(), std::optional<std::string>("H148R"));
    EXPECT_EQ(handler.getDigest(), std::optional<std::string>("abc"));
    EXPECT_EQ(handler.getDiameter(), 0.038);
    EXPECT_EQ(handler.getLength(), 0.152);
}

/// The delay a <delay> element with the text @p text gives, as Java prints a double, with
/// the warnings of reading it and of getDelay(): "<delay> | <warnings>".
[[nodiscard]] std::string delayOf(std::string_view text)
{
    const Element element  = readChildren(std::format("<delay>{}</delay>", text));
    Texts         warnings = element.closeWarnings;
    warnings.insert(warnings.end(), element.delayWarnings.begin(), element.delayWarnings.end());
    return QtRocket::Strings::javaDoubleToString(element.delay) + " | " + joined(warnings);
}

constexpr std::string_view kIllegalDelay =
    "Infinity | Illegal motor delay specified, ignoring.; Motor delay not specified, assuming no "
    "ejection charge.";

TEST(MotorHandlerElements, TheDelayIsANumberOrNone)
{
    EXPECT_EQ(each({"none", " none ", "3", "3.5", " 3 ", "0", "-1", "0x10p0", "5d"}, delayOf),
              (Texts{"Infinity | ", "Infinity | ", "3.0 | ", "3.5 | ", "3.0 | ", "0.0 | ",
                     "-1.0 | ", "16.0 | ", "5.0 | "}));
    // Only the lower-case word is the plugged delay.
    EXPECT_EQ(each({"None", "NONE", "abc", "", "NaN", "Inf", "plugged", "P"}, delayOf),
              Texts(8, std::string(kIllegalDelay)));
}

TEST(MotorHandlerElements, AnInfiniteDelayIsThePluggedOneOrNoDelay)
{
    // The plugged delay however it is written, as in OpenRocket.
    EXPECT_EQ(each({"Infinity", "+Infinity", "1e400"}, delayOf), Texts(3, "Infinity | "));
    // Not OpenRocket's, which stores the negative infinity: no simulation runs with it.
    EXPECT_EQ(each({"-Infinity", "-1e400"}, delayOf), Texts(2, std::string(kIllegalDelay)));
}

TEST(MotorHandlerElements, AMissingDelayIsThePluggedOneWithAWarningEveryTimeItIsAsked)
{
    HandlerFixture fixture;
    MotorHandler   handler(fixture.context());
    WarningSet     warnings;
    EXPECT_EQ(handler.getDelay(warnings), Motor::kPluggedDelay);
    EXPECT_EQ(handler.getDelay(warnings), Motor::kPluggedDelay);
    EXPECT_EQ(warningTexts(warnings), noDelay());
    EXPECT_EQ(warnings.begin()->priority(), MessagePriority::NORMAL);
    // A later delay replaces an earlier one, also by an illegal one.
    EXPECT_EQ(readChildren("<delay>3</delay><delay>5</delay>").delay, 5.0);
    EXPECT_EQ(readChildren("<delay>3</delay><delay>x</delay>").delay, kInfinity);
}

/// The nozzle exit diameter a <nozzleexitdiameter> element with the text @p text gives, as Java
/// prints a double, and the warnings of reading it: "<diameter> | <warnings>".
[[nodiscard]] std::string nozzleOf(std::string_view text)
{
    const Element element =
        readChildren(std::format("<nozzleexitdiameter>{}</nozzleexitdiameter>", text));
    return QtRocket::Strings::javaDoubleToString(element.nozzle) + " | " +
           joined(element.closeWarnings);
}

TEST(MotorHandlerElements, TheNozzleExitDiameterIsFiniteAndNotNegative)
{
    EXPECT_EQ(each({"0.01", " 0.01 ", "0"}, nozzleOf), (Texts{"0.01 | ", "0.01 | ", "0.0 | "}));
    EXPECT_EQ(each({"-0.01", "abc", "NaN", "Infinity", "-Infinity", "", "1e400"}, nozzleOf),
              Texts(7, "0.0 | Illegal nozzle exit diameter specified, assuming unknown."));
    // A negative zero is not below zero.
    EXPECT_EQ(nozzleOf("-0.0"), "-0.0 | ");
}

TEST(MotorHandlerElements, AnotherChildIsWarnedOfByItsTextAndItsAttributes)
{
    EXPECT_EQ(readChildren("<bogus></bogus>").closeWarnings, Texts{});
    EXPECT_EQ(readChildren("<bogus>  </bogus>").closeWarnings, Texts{});
    EXPECT_EQ(readChildren("<bogus>text</bogus>").closeWarnings,
              Texts{"Unknown text in element 'bogus', ignoring."});
    EXPECT_EQ(readChildren("<bogus a='1'/>").closeWarnings,
              Texts{"Unknown attributes in element 'bogus', ignoring."});
    EXPECT_EQ(readChildren("<bogus a='1'>text</bogus>").closeWarnings,
              (Texts{"Unknown text in element 'bogus', ignoring.",
                     "Unknown attributes in element 'bogus', ignoring."}));
    // Element names are exact, and the attributes of a known child are not looked at.
    EXPECT_EQ(readChildren("<Type>single</Type>").closeWarnings,
              Texts{"Unknown text in element 'Type', ignoring."});
    EXPECT_EQ(readChildren("<Type>single</Type>").query, query());
    EXPECT_EQ(readChildren("<type a='1'>single</type>").closeWarnings, Texts{});
    EXPECT_EQ(readChildren("<delay a='1'>3</delay>").delay, 3.0);
    // Every child is plain text: an element in one is ignored with the text handler's warning.
    EXPECT_EQ(readChildren("<delay><seconds/></delay>").closeWarnings.at(0),
              "Unknown element seconds, ignoring.");
}

TEST(MotorHandlerElements, TheParentIsToldTheElementsAttributes)
{
    // The motor mount's handler reads the configuration id there.
    HandlerFixture   fixture;
    MotorHandler     handler(fixture.context());
    const HandlerRun run = runHandler(handler, "<motor configid='abc'><delay>3</delay></motor>");
    EXPECT_EQ(run.element, "motor");
    EXPECT_EQ(run.attributes, (ElementHandler::Attributes{{"configid", "abc"}}));
    EXPECT_EQ(run.texts(), Texts{});
}

// ---- getMotor() ------------------------------------------------------------------------------

/// The .rse file of the motor of MotorHandlerTest, as OpenRocket's RockSimMotorWriter writes
/// it; the tests below change it text by text.
constexpr std::string_view kRse = R"(<engine-database>
 <engine-list>
  <engine mfg="Estes" code="F12X" Type="unknown" dia="24" len="70" initWt="100" propWt="60" delays="3,1000" auto-calc-mass="0" auto-calc-cg="0">
   <comments>Embedded motor test</comments>
   <data>
    <eng-data t="0" f="0" m="100" cg="35"/>
    <eng-data t="0.5" f="12" m="70" cg="33"/>
    <eng-data t="1" f="0" m="40" cg="31"/>
   </data>
  </engine>
 </engine-list>
</engine-database>
)";

/// The digest of an .eng file's reader for that motor, and the motor's own.
constexpr std::string_view kRaspDigest = "254dcbdbb709665c236206b5aaf130fc";
constexpr std::string_view kOwnDigest  = "d10a1ab064680408474285c792f70c5d";

/// @p text with @p from, which it holds, replaced by @p to.
[[nodiscard]] std::string replaced(std::string_view text, std::string_view from,
                                   std::string_view to)
{
    std::string       result(text);
    const std::size_t at = result.find(from);
    if (at == std::string::npos)
    {
        QtRocket::bug("the text to replace is not there");
    }
    result.replace(at, from.size(), to);
    return result;
}

/// The <engine> element of @p rse.
[[nodiscard]] std::string engineOf(std::string_view rse)
{
    constexpr std::string_view kEnd  = "</engine>";
    const std::size_t          begin = rse.find("<engine ");
    return std::string(rse.substr(begin, rse.find(kEnd) + kEnd.size() - begin));
}

/// An .rse file with the engines of @p first and then of @p second.
[[nodiscard]] std::string twoEngines(std::string_view first, std::string_view second)
{
    return replaced(first, "</engine>", "</engine>\n" + engineOf(second));
}

/// What getMotor() gave.
struct Resolved
{
    std::shared_ptr<const Motor> motor;
    Texts                        warnings;
    Texts                        asked;  ///< the finder's queries, then the attachments asked for

    /// "<manufacturer>/<designation>/<TYPE>/digest=<digest>/peak=<the second thrust>", or "null".
    [[nodiscard]] std::string describe() const
    {
        const auto* curve = dynamic_cast<const ThrustCurveMotor*>(motor.get());
        if (curve == nullptr)
        {
            return "null";
        }
        return std::format("{}/{}/{}/digest={}/peak={}", curve->getManufacturer().getDisplayName(),
                           curve->getDesignation(), enumName(curve->getMotorType()),
                           curve->getDigest(),
                           QtRocket::Strings::javaDoubleToString(curve->getThrustPoints().at(1)));
    }
};

/// The children of a <motor> for a single-use Estes F12X with the digest @p digest.
[[nodiscard]] std::string withDigest(std::string_view digest)
{
    return std::format(
        "<type>single</type><manufacturer> Estes </manufacturer>"
        "<designation> F12X </designation><digest>{}</digest>",
        digest);
}

/// getMotor() of a <motor> with @p children, in @p fixture as the test has set it up.
[[nodiscard]] Resolved resolve(HandlerFixture& fixture, std::string_view children)
{
    MotorHandler     handler(fixture.context());
    const HandlerRun run = runHandler(handler, std::format("<motor>{}</motor>", children));
    EXPECT_TRUE(run.result.has_value());
    EXPECT_TRUE(run.warnings.empty());
    WarningSet warnings;
    Resolved   resolved;
    resolved.motor    = handler.getMotor(warnings);
    resolved.warnings = warningTexts(warnings);
    resolved.asked    = fixture.motorFinder().queries();
    resolved.asked.insert(resolved.asked.end(), fixture.attachments().asked().begin(),
                          fixture.attachments().asked().end());
    return resolved;
}

/// The setting of most tests below: the database's answer is @p databaseMotor with the warning
/// "db warning", and the attachment of @p digest holds @p rse (none: it is missing).
[[nodiscard]] Resolved resolveWith(std::string_view                    digest,
                                   const std::shared_ptr<const Motor>& databaseMotor,
                                   std::optional<std::string_view>     rse)
{
    HandlerFixture fixture;
    fixture.motorFinder().setMotor(databaseMotor);
    fixture.motorFinder().setWarning("db warning");
    if (rse.has_value())
    {
        fixture.attachments().put(std::format("thrustcurves/{}.rse", digest), *rse);
    }
    return resolve(fixture, withDigest(digest));
}

constexpr std::string_view kFromAttachment =
    "Estes/F12X/UNKNOWN/digest=254dcbdbb709665c236206b5aaf130fc/peak=12.0";

[[nodiscard]] std::shared_ptr<const ThrustCurveMotor> different()
{
    return createMotor("F12X", 20.0, "different-digest");
}

/// The warning "Unable to load embedded motor attachment ..." for the attachment of @p digest.
[[nodiscard]] std::string unable(std::string_view digest, std::string_view reason)
{
    return std::format("Unable to load embedded motor attachment 'thrustcurves/{}.rse': {}", digest,
                       reason);
}

TEST(MotorHandlerGetMotor, TheTestMotorIsWrittenAsOpenRocketWritesIt)
{
    // The tests below rest on this text and on these digests, which are OpenRocket's.
    const std::shared_ptr<const ThrustCurveMotor> prototype = createMotor("F12X", 12.0, "");
    EXPECT_EQ(RockSimMotorWriter::write(*prototype), kRse);
    EXPECT_EQ(raspStyleDigest(*prototype), kRaspDigest);
    EXPECT_EQ(MotorDigest::digestMotor(*prototype), kOwnDigest);
}

TEST(MotorHandlerGetMotor, WithoutADigestTheDatabasesMotorIsTaken)
{
    const std::shared_ptr<const ThrustCurveMotor> motor = different();
    HandlerFixture                                fixture;
    fixture.motorFinder().setMotor(motor);
    const Resolved found = resolve(fixture, "<designation>F12X</designation>");
    EXPECT_EQ(found.motor, motor);
    EXPECT_EQ(found.warnings, Texts{});
    EXPECT_EQ(found.asked, Texts{query("null", "null", "F12X")});
}

TEST(MotorHandlerGetMotor, WithoutADigestAndAMotorTheFindersWarningsArePassedOn)
{
    HandlerFixture fixture;
    fixture.motorFinder().setWarning("db warning");
    const Resolved named = resolve(fixture, "<designation>F12X</designation>");
    EXPECT_EQ(named.motor, nullptr);
    EXPECT_EQ(named.warnings, Texts{"db warning"});
    EXPECT_EQ(named.asked, Texts{query("null", "null", "F12X")});

    HandlerFixture empty;
    empty.motorFinder().setWarning("db warning");
    const Resolved nothing = resolve(empty, "");
    EXPECT_EQ(nothing.motor, nullptr);
    EXPECT_EQ(nothing.warnings, Texts{"db warning"});
    EXPECT_EQ(nothing.asked, Texts{query()});
}

TEST(MotorHandlerGetMotor, AnEmptyDigestIsNoDigest)
{
    // It is given to the finder as it is, and no attachment is asked for.
    const std::shared_ptr<const ThrustCurveMotor> motor = different();
    const Resolved                                empty = resolveWith("", motor, std::nullopt);
    EXPECT_EQ(empty.motor, motor);
    EXPECT_EQ(empty.warnings, Texts{"db warning"});
    EXPECT_EQ(empty.asked, Texts{query("SINGLE", "Estes", "F12X", "")});
    const Resolved blank = resolveWith("   ", motor, std::nullopt);
    EXPECT_EQ(blank.motor, motor);
    EXPECT_EQ(blank.warnings, Texts{"db warning"});
    EXPECT_EQ(blank.asked, Texts{query("SINGLE", "Estes", "F12X", "")});
    const Resolved none = resolveWith("", nullptr, std::nullopt);
    EXPECT_EQ(none.motor, nullptr);
    EXPECT_EQ(none.warnings, Texts{"db warning"});
    EXPECT_EQ(none.asked, Texts{query("SINGLE", "Estes", "F12X", "")});
}

TEST(MotorHandlerGetMotor, ADatabaseMotorWithACompatibleDigestIsTakenUnread)
{
    const std::shared_ptr<const ThrustCurveMotor> exact = different();
    const Resolved byDigest = resolveWith("different-digest", exact, std::nullopt);
    EXPECT_EQ(byDigest.motor, exact);
    EXPECT_EQ(byDigest.warnings, Texts{"db warning"});
    EXPECT_EQ(byDigest.asked, Texts{query("SINGLE", "Estes", "F12X", "different-digest")});

    // A digest of an older format of the motor's curve.
    const std::shared_ptr<const ThrustCurveMotor> current =
        createMotor("F12X", 12.0, "current-digest");
    const Resolved historical = resolveWith(kRaspDigest, current, std::nullopt);
    EXPECT_EQ(historical.motor, current);
    EXPECT_EQ(historical.warnings, Texts{"db warning"});
    EXPECT_EQ(historical.asked, Texts{query("SINGLE", "Estes", "F12X", kRaspDigest)});
}

TEST(MotorHandlerGetMotor, AMissingAttachmentIsSilentAndTheDatabasesMotorIsUsedAfterAll)
{
    const std::shared_ptr<const ThrustCurveMotor> motor = different();
    const Texts    asked{query("SINGLE", "Estes", "F12X", "missing-digest"),
                         "thrustcurves/missing-digest.rse"};
    const Resolved approximate = resolveWith("missing-digest", motor, std::nullopt);
    EXPECT_EQ(approximate.motor, motor);
    EXPECT_EQ(approximate.warnings, Texts{"db warning"});
    EXPECT_EQ(approximate.asked, asked);

    const Resolved none = resolveWith("missing-digest", nullptr, std::nullopt);
    EXPECT_EQ(none.motor, nullptr);
    EXPECT_EQ(none.warnings, Texts{"db warning"});
    EXPECT_EQ(none.asked, asked);
}

TEST(MotorHandlerGetMotor, TheEmbeddedCurveIsTakenAndTheFindersWarningsAreDropped)
{
    const Texts    asked{query("SINGLE", "Estes", "F12X", kRaspDigest),
                         std::format("thrustcurves/{}.rse", kRaspDigest)};
    const Resolved alone = resolveWith(kRaspDigest, nullptr, kRse);
    EXPECT_EQ(alone.describe(), kFromAttachment);
    EXPECT_EQ(alone.warnings, Texts{});
    EXPECT_EQ(alone.asked, asked);

    // Also where the database has a motor of that description with another curve.
    const std::shared_ptr<const ThrustCurveMotor> approximate = different();
    const Resolved preferred = resolveWith(kRaspDigest, approximate, kRse);
    EXPECT_NE(preferred.motor, approximate);
    EXPECT_EQ(preferred.describe(), kFromAttachment);
    EXPECT_EQ(preferred.warnings, Texts{});

    // By the motor's own digest.
    EXPECT_EQ(resolveWith(kOwnDigest, nullptr, kRse).describe(),
              std::format("Estes/F12X/UNKNOWN/digest={}/peak=12.0", kOwnDigest));
}

TEST(MotorHandlerGetMotor, TheMotorOfTheAttachmentHasTheFilesDigestAndTheCurvesData)
{
    const Resolved found = resolveWith(kRaspDigest, nullptr, kRse);
    const auto*    motor = dynamic_cast<const ThrustCurveMotor*>(found.motor.get());
    ASSERT_NE(motor, nullptr);
    // "designation=F12X commonName=F12 code= manufacturer=Estes type=Unknown diameter=0.024
    // length=0.07 delays=[3.0, Infinity] description=[Embedded motor test] launchMass=0.1
    // burnoutMass=0.04 time=[0.0, 0.5, 1.0] thrust=[0.0, 12.0, 0.0]"
    EXPECT_EQ(motor->getDesignation(), "F12X");
    EXPECT_EQ(motor->getCommonName(), "F12");
    EXPECT_EQ(motor->getManufacturer().getDisplayName(), "Estes");
    EXPECT_EQ(motor->getMotorType(), Motor::Type::UNKNOWN);
    EXPECT_DOUBLE_EQ(motor->getDiameter(), 0.024);
    EXPECT_DOUBLE_EQ(motor->getLength(), 0.07);
    EXPECT_EQ(motor->getStandardDelays(), (std::vector<double>{3.0, kInfinity}));
    EXPECT_EQ(motor->getDescription(), "Embedded motor test");
    EXPECT_DOUBLE_EQ(motor->getLaunchMass(), 0.1);
    EXPECT_DOUBLE_EQ(motor->getBurnoutMass(), 0.04);
    EXPECT_EQ(motor->getTimePoints(), (std::vector<double>{0.0, 0.5, 1.0}));
    EXPECT_EQ(motor->getThrustPoints(), (std::vector<double>{0.0, 12.0, 0.0}));
    // The digest is the file's, not the one the data would have.
    EXPECT_EQ(motor->getDigest(), kRaspDigest);
    EXPECT_EQ(MotorDigest::digestMotor(*motor), kOwnDigest);
}

TEST(MotorHandlerGetMotor, TheElementsTypeAndMakerAreNotTheAttachments)
{
    HandlerFixture fixture;
    fixture.attachments().put(std::format("thrustcurves/{}.rse", kRaspDigest), kRse);
    const Resolved found =
        resolve(fixture, std::format("<type>hybrid</type><manufacturer>AeroTech</manufacturer>"
                                     "<designation>Z1</designation><digest>{}</digest>",
                                     kRaspDigest));
    EXPECT_EQ(found.describe(), kFromAttachment);
    EXPECT_EQ(found.asked.at(0), query("HYBRID", "AeroTech", "Z1", kRaspDigest));
}

TEST(MotorHandlerGetMotor, TheLastDigestOfAnElementCounts)
{
    HandlerFixture fixture;
    fixture.attachments().put(std::format("thrustcurves/{}.rse", kRaspDigest), kRse);
    const Resolved found =
        resolve(fixture, std::format("<digest>first</digest><digest>{}</digest>", kRaspDigest));
    EXPECT_EQ(found.describe(), kFromAttachment);
    EXPECT_EQ(found.asked, (Texts{query("null", "null", "null", kRaspDigest),
                                  std::format("thrustcurves/{}.rse", kRaspDigest)}));
}

TEST(MotorHandlerGetMotor, AnAttachmentWithAnotherCurveIsWarnedOf)
{
    const std::string other   = RockSimMotorWriter::write(*different());
    const std::string warning = std::format(
        "Embedded motor attachment 'thrustcurves/{0}.rse' contains no motor matching "
        "digest '{0}'.",
        kRaspDigest);
    const Resolved none = resolveWith(kRaspDigest, nullptr, other);
    EXPECT_EQ(none.motor, nullptr);
    EXPECT_EQ(none.warnings, (Texts{warning, "db warning"}));

    // The database's motor after all, the attachment's warning before the finder's.
    const std::shared_ptr<const ThrustCurveMotor> motor = different();
    const Resolved approximate                          = resolveWith(kRaspDigest, motor, other);
    EXPECT_EQ(approximate.motor, motor);
    EXPECT_EQ(approximate.warnings, (Texts{warning, "db warning"}));

    // Digests are compared as they are written.
    const std::string upper = QtRocket::Strings::toUpper(kRaspDigest);
    EXPECT_EQ(resolveWith(upper, nullptr, kRse).warnings,
              (Texts{std::format("Embedded motor attachment 'thrustcurves/{0}.rse' contains no "
                                 "motor matching digest '{0}'.",
                                 upper),
                     "db warning"}));
}

TEST(MotorHandlerGetMotor, TheFirstCompatibleMotorOfSeveralIsTaken)
{
    const std::string other  = RockSimMotorWriter::write(*different());
    const Resolved    second = resolveWith(kRaspDigest, nullptr, twoEngines(other, kRse));
    EXPECT_EQ(second.describe(), kFromAttachment);
    EXPECT_EQ(second.warnings, Texts{});
}

TEST(MotorHandlerGetMotor, AnAttachmentWithoutMotorsIsWarnedOf)
{
    const Texts warnings{"Embedded motor attachment 'thrustcurves/d.rse' contains no motors.",
                         "db warning"};
    EXPECT_EQ(
        resolveWith("d", nullptr, "<engine-database><engine-list></engine-list></engine-database>")
            .warnings,
        warnings);
    EXPECT_EQ(resolveWith("d", nullptr, "<other/>").warnings, warnings);
}

TEST(MotorHandlerGetMotor, AnAttachmentThatIsNoRseFileIsWarnedOfWithTheReadersMessage)
{
    const Resolved unclosed = resolveWith("corrupt-digest", nullptr, "<engine-database>");
    EXPECT_EQ(unclosed.motor, nullptr);
    EXPECT_EQ(unclosed.warnings, (Texts{unable("corrupt-digest",
                                               "XML document structures must start and end within "
                                               "the same entity."),
                                        "db warning"}));
    const std::shared_ptr<const ThrustCurveMotor> motor = different();
    const Resolved kept = resolveWith("corrupt-digest", motor, "<engine-database>");
    EXPECT_EQ(kept.motor, motor);
    EXPECT_EQ(kept.warnings, unclosed.warnings);

    EXPECT_EQ(resolveWith("d", nullptr, "").warnings,
              (Texts{unable("d", "Premature end of file."), "db warning"}));
    EXPECT_EQ(resolveWith("d", nullptr, "not xml at all").warnings,
              (Texts{unable("d", "Content is not allowed in prolog."), "db warning"}));
    // An .eng file under the name of an .rse file is read as an .rse file.
    EXPECT_EQ(resolveWith("d", nullptr, "; comment\nF12X 24 70 3-P 0.06 0.1 Estes\n0.5 12\n1.0 0\n")
                  .warnings,
              (Texts{unable("d", "Content is not allowed in prolog."), "db warning"}));
    EXPECT_EQ(resolveWith("d", nullptr, replaced(kRse, R"(f="12")", R"(f="NaN")")).warnings,
              (Texts{unable("d", "Illegal motor data point encountered"), "db warning"}));
}

TEST(MotorHandlerGetMotor, AMotorThatCannotBeBuiltEndsTheSearchInTheFile)
{
    const std::string negative = replaced(kRse, R"(f="12")", R"(f="-12")");
    EXPECT_EQ(resolveWith(kRaspDigest, nullptr, negative).warnings,
              (Texts{unable(kRaspDigest, "Negative thrust."), "db warning"}));
    EXPECT_EQ(
        resolveWith(kRaspDigest, nullptr, replaced(kRse, R"(len="70")", R"(len="20")")).warnings,
        (Texts{
            unable(kRaspDigest, "Invalid CG position: 0.035000: CG is above the end of the motor."),
            "db warning"}));
    EXPECT_EQ(
        resolveWith(kRaspDigest, nullptr, replaced(kRse, R"(f="12")", R"(f="1e12")")).warnings,
        (Texts{unable(kRaspDigest, "Invalid thrust 1.0E12"), "db warning"}));

    // Also when a later motor of the file would fit; a motor after the one that fits is not
    // looked at.
    const Resolved badFirst = resolveWith(kRaspDigest, nullptr, twoEngines(negative, kRse));
    EXPECT_EQ(badFirst.motor, nullptr);
    EXPECT_EQ(badFirst.warnings, (Texts{unable(kRaspDigest, "Negative thrust."), "db warning"}));
    const Resolved goodFirst = resolveWith(kRaspDigest, nullptr, twoEngines(kRse, negative));
    EXPECT_EQ(goodFirst.describe(), kFromAttachment);
    EXPECT_EQ(goodFirst.warnings, Texts{});
}

TEST(MotorHandlerGetMotor, AThrustCurveOfOnePointIsAWarningToo)
{
    // Not OpenRocket's: its reader throws an IndexOutOfBoundsException there, which ends the
    // load of the whole design. The message is that exception's.
    const std::size_t secondPoint = kRse.find("<eng-data", kRse.find("<eng-data") + 5);
    const std::string onePoint    = std::string(kRse.substr(0, secondPoint)) +
                                    "</data>\n</engine>\n</engine-list>\n</engine-database>\n";
    const Resolved    found       = resolveWith("d", nullptr, onePoint);
    EXPECT_EQ(found.motor, nullptr);
    EXPECT_EQ(found.warnings,
              (Texts{unable("d", "Index 1 out of bounds for length 1"), "db warning"}));
}

TEST(MotorHandlerGetMotor, AnAttachmentThatCannotBeReadIsWarnedOfWithItsFailure)
{
    HandlerFixture fixture;
    fixture.motorFinder().setWarning("db warning");
    fixture.attachments().putFailure("thrustcurves/d.rse", ErrorCode::IO, "disk on fire");
    EXPECT_EQ(resolve(fixture, withDigest("d")).warnings,
              (Texts{unable("d", "disk on fire"), "db warning"}));

    // The database's motor after all.
    const std::shared_ptr<const ThrustCurveMotor> motor = different();
    HandlerFixture                                kept;
    kept.motorFinder().setMotor(motor);
    kept.attachments().putFailure("thrustcurves/d.rse", ErrorCode::PARSE, "unexpected EOF");
    const Resolved found = resolve(kept, withDigest("d"));
    EXPECT_EQ(found.motor, motor);
    EXPECT_EQ(found.warnings, Texts{unable("d", "unexpected EOF")});
}

/// The warnings of resolving a motor whose attachment fails with ErrorCode::IO and @p message.
[[nodiscard]] std::string failingWith(std::string_view message)
{
    HandlerFixture fixture;
    fixture.attachments().putFailure("thrustcurves/d.rse", ErrorCode::IO, std::string(message));
    return joined(resolve(fixture, withDigest("d")).warnings);
}

TEST(MotorHandlerGetMotor, AFailureWithoutAMessageIsNamedByItsCode)
{
    // Java prints the exception's class name ("IOException") for an empty or blank message.
    EXPECT_EQ(each({"", "   "}, failingWith), Texts(2, unable("d", "IO")));
}

/// What resolving a motor with the digest @p digest gives when the finder finds nothing and
/// adds its warning and every attachment is missing: "<motor> | <warnings> | <what was asked>".
[[nodiscard]] std::string resolvedAlone(std::string_view digest)
{
    HandlerFixture fixture;
    fixture.motorFinder().setWarning("db warning");
    const Resolved found = resolve(fixture, withDigest(digest));
    return found.describe() + " | " + joined(found.warnings) + " | " + joined(found.asked);
}

/// resolvedAlone() of a digest that is not made into an attachment name: the finder's answer
/// and nothing else, as for a missing attachment.
[[nodiscard]] std::string unasked(std::string_view digest)
{
    return "null | db warning | " + query("SINGLE", "Estes", "F12X", digest);
}

/// resolvedAlone() of a digest whose attachment is asked for and missing.
[[nodiscard]] std::string askedFor(std::string_view digest)
{
    return unasked(digest) + std::format("; thrustcurves/{}.rse", digest);
}

TEST(MotorHandlerGetMotor, ADigestThatWouldLeaveTheThrustCurvesIsNeverAnAttachmentName)
{
    // Not OpenRocket's, which asks for "thrustcurves/../../etc/passwd.rse" and its like.
    EXPECT_EQ(each({"../../etc/passwd", "a/b", "a\\b", "..", "a..b", "/", "\\"}, resolvedAlone),
              each({"../../etc/passwd", "a/b", "a\\b", "..", "a..b", "/", "\\"}, unasked));
    // A dot, a colon and a hyphen are no way out.
    EXPECT_EQ(each({"a.b", "a:b", "missing-digest", ".", "a b"}, resolvedAlone),
              each({"a.b", "a:b", "missing-digest", ".", "a b"}, askedFor));
}

TEST(MotorHandlerGetMotor, ReadsTheCurveOutOfAnArchive)
{
    // No example design reaches this step: an archive made here, read through the factory a
    // loader uses.
    QtRocket::ZipWriter writer;
    writer.add("rocket.ork", QtRocket::stringToBytes("<openrocket/>"));
    writer.add(std::format("thrustcurves/{}.rse", kRaspDigest), QtRocket::stringToBytes(kRse));
    writer.add("thrustcurves/broken.rse", QtRocket::stringToBytes("<engine-database>"));
    const QtRocket::ZipFileAttachmentFactory factory(writer.finish().value());

    HandlerFixture fixture;
    fixture.context().setAttachmentFactory(&factory);
    EXPECT_EQ(resolve(fixture, withDigest(kRaspDigest)).describe(), kFromAttachment);
    EXPECT_EQ(resolve(fixture, withDigest("missing")).warnings, Texts{});
    EXPECT_EQ(resolve(fixture, withDigest("broken")).warnings,
              Texts{unable("broken",
                           "XML document structures must start and end within the same "
                           "entity.")});
}

TEST(MotorHandlerGetMotor, AnArchiveThatCannotBeReadIsAWarning)
{
    // Hostile input: an archive cut inside its first entry.
    QtRocket::ZipWriter writer;
    writer.add("rocket.ork", std::vector<std::byte>(1000, std::byte{'x'}));
    writer.add("thrustcurves/d.rse", QtRocket::stringToBytes(kRse));
    std::vector<std::byte> bytes = writer.finish().value();
    bytes.resize(45);
    const QtRocket::ZipFileAttachmentFactory factory(std::move(bytes));
    HandlerFixture                           fixture;
    fixture.context().setAttachmentFactory(&factory);
    const Resolved found = resolve(fixture, withDigest("d"));
    EXPECT_EQ(found.motor, nullptr);
    ASSERT_EQ(found.warnings.size(), 1U);
    EXPECT_TRUE(found.warnings.at(0).starts_with(
        "Unable to load embedded motor attachment 'thrustcurves/d.rse': "));
}

TEST(MotorHandlerGetMotor, ReadsTheCurveFromAFileNextToAPlainDesign)
{
    // What the context does without a factory of its own (a design that is no archive), with
    // the design's directory as the base: OpenRocket loads the motor from there too.
    const QtRocket::Test::TempDir temp;
    static_cast<void>(temp.write(std::format("thrustcurves/{}.rse", kRaspDigest), kRse));
    static_cast<void>(temp.write("outside.rse", kRse));
    const QtRocket::FileSystemAttachmentFactory factory(temp.path());
    HandlerFixture                              fixture;
    fixture.context().setAttachmentFactory(&factory);
    EXPECT_EQ(resolve(fixture, withDigest(kRaspDigest)).describe(), kFromAttachment);
    // The file above the directory is not reached through the digest.
    EXPECT_EQ(resolve(fixture, withDigest("../outside")).motor, nullptr);
    EXPECT_EQ(resolve(fixture, withDigest("missing")).warnings, Texts{});
}

// Hostile input next to a plain design: the file the digest names is read whole, so it may
// not be of any size. OpenRocket hands the loader a stream and says of both files below
// "Unable to load embedded motor attachment 'thrustcurves/big.rse': Content is not allowed in
// prolog." (probe BigAttachmentProbe of the review); here std::bad_alloc left getMotor().

/// A file under @p dir that says it holds @p size bytes (sparse where the file system has
/// sparse files); false when it cannot be made.
[[nodiscard]] bool makeFileOfSize(const QtRocket::Test::TempDir& dir,
                                  const std::filesystem::path& name, std::uintmax_t size)
{
    const std::filesystem::path file = dir.write(name, "x");
    std::error_code             error;
    std::filesystem::resize_file(file, size, error);
    return !error;
}

TEST(MotorHandlerGetMotor, ACurveFileBeyondTheLimitIsAWarning)
{
    const QtRocket::Test::TempDir temp;
    ASSERT_TRUE(makeFileOfSize(temp, "thrustcurves/big.rse", Attachment::kMaxAttachmentBytes + 1));
    const QtRocket::FileSystemAttachmentFactory factory(temp.path());
    HandlerFixture                              fixture;
    fixture.context().setAttachmentFactory(&factory);

    const Resolved found = resolve(fixture, withDigest("big"));
    EXPECT_EQ(found.motor, nullptr);
    EXPECT_EQ(found.warnings,
              Texts{unable("big",
                           "Attachment 'thrustcurves/big.rse' exceeds the maximum size "
                           "of 33554432 bytes")});

    // With a motor from the database the design still loads with it.
    const std::shared_ptr<const ThrustCurveMotor> approximate = different();
    fixture.motorFinder().setMotor(approximate);
    EXPECT_EQ(resolve(fixture, withDigest("big")).motor, approximate);
}

/// Makes @p link under @p dir a symbolic link to the device @p device; false where there is no
/// such device or no links.
[[nodiscard]] bool makeDeviceLink(const QtRocket::Test::TempDir& dir,
                                  const std::filesystem::path&   link,
                                  const std::filesystem::path&   device)
{
    std::error_code error;
    if (!std::filesystem::is_character_file(device, error))
    {
        return false;
    }
    std::filesystem::create_directories(dir.resolve(link).parent_path(), error);
    std::filesystem::create_symlink(device, dir.resolve(link), error);
    return !error;
}

TEST(MotorHandlerGetMotor, ACurveFileThatIsADeviceIsAWarning)
{
    const QtRocket::Test::TempDir temp;
    if (!makeDeviceLink(temp, "thrustcurves/zero.rse", "/dev/zero"))
    {
        GTEST_SKIP() << "no /dev/zero or no links here";
    }
    const QtRocket::FileSystemAttachmentFactory factory(temp.path());
    HandlerFixture                              fixture;
    fixture.context().setAttachmentFactory(&factory);

    // The endless source is not opened, let alone read to the end of the memory.
    const Resolved found = resolve(fixture, withDigest("zero"));
    EXPECT_EQ(found.motor, nullptr);
    EXPECT_EQ(
        found.warnings,
        Texts{unable("zero", "cannot read '" +
                                 QtRocket::pathToUtf8(temp.path() / "thrustcurves" / "zero.rse") +
                                 "': not a regular file")});
}

TEST(MotorHandlerGetMotor, ResolvesThroughADatabaseMotorFinderAsTheLoaderWill)
{
    // The chain a loader sets up: the handler, a DatabaseMotorFinder, a database (here one of
    // two motors made for the test; the bundled database is MotorHandlerBundled's below). An
    // Estes A8 whose digest is one of an older format of the second A8 curve.
    QtRocket::ThrustCurveMotorSetDatabase         database;
    const std::shared_ptr<const ThrustCurveMotor> first  = createMotor("A8", 8.0, "first");
    const std::shared_ptr<const ThrustCurveMotor> second = createMotor("A8", 9.0, "second");
    database.addMotor(first);
    database.addMotor(second);
    const QtRocket::DatabaseMotorFinder finder(database);
    HandlerFixture                      fixture;
    fixture.context().setMotorFinder(&finder);

    const std::string children =
        "<type>unknown</type><manufacturer>Estes</manufacturer>"
        "<designation>A8</designation>";
    // By a digest of an older format of the second curve.
    EXPECT_EQ(
        resolve(fixture, children + "<digest>" + raspStyleDigest(*second) + "</digest>").motor,
        second);
    // No digest fits and no attachment is there: the first with the designation, silently.
    const Resolved approximate = resolve(fixture, children + "<digest>nothing</digest>");
    EXPECT_EQ(approximate.motor, first);
    EXPECT_EQ(approximate.warnings, Texts{});
    // A missing motor: the finder's warning comes through.
    const Resolved missing =
        resolve(fixture, "<designation>Z9</designation><digest>nothing</digest>");
    EXPECT_EQ(missing.motor, nullptr);
    EXPECT_EQ(missing.warnings, Texts{"No motor with designation 'Z9' found."});
}

// ---- the motors of the example designs, through the handler ------------------------------------

/// What the handler makes of the <motor> element of @p example as a file holds it (the type,
/// the manufacturer, the designation and the digest), in @p fixture: "<designation>: <the
/// digest of the motor found>", or "<designation>: no motor", then every warning.
[[nodiscard]] std::string resolvedExample(HandlerFixture& fixture, const ExampleMotor& example)
{
    const Resolved found =
        resolve(fixture, std::format("<type>{}</type><manufacturer>{}</manufacturer>"
                                     "<designation>{}</designation><digest>{}</digest>",
                                     QtRocket::orkName(example.type), example.manufacturer,
                                     example.designation, example.digest));
    const auto* const curve = dynamic_cast<const ThrustCurveMotor*>(found.motor.get());
    std::string       text  = std::format("{}: {}", example.designation,
                                          curve != nullptr ? curve->getDigest() : "no motor");
    for (const std::string& warning : found.warnings)
    {
        text += " warning: " + warning;
    }
    return text;
}

/// resolvedExample() of every example motor, and what the table expects of each.
struct ExampleResolutions
{
    Texts resolved;
    Texts expected;
};

[[nodiscard]] ExampleResolutions resolveExamples(HandlerFixture& fixture)
{
    ExampleResolutions all;
    for (const ExampleMotor& example : kExampleMotors)
    {
        all.resolved.push_back(resolvedExample(fixture, example));
        all.expected.push_back(std::format("{}: {}", example.designation, example.found));
    }
    return all;
}

// The exit check of the loader infrastructure, end to end: every motor reference of the 16
// example designs, read by the handler from its element in a file of format 1.10 and searched in
// the bundled database, is the motor OpenRocket loads, without a warning. The seven whose digest
// fits no curve of the database ask for an embedded curve first, find none and take the
// database's motor, as OpenRocket does with these files. (One TEST: the database is read once
// per test process.)
TEST(MotorHandlerBundled, EveryMotorOfTheExamplesResolvesAsOpenRockets)
{
    const QtRocket::DatabaseMotorFinder finder(QtRocket::Test::bundledMotorDatabase());
    HandlerFixture                      fixture;
    fixture.context().setMotorFinder(&finder);
    fixture.context().setFileVersion(110);

    const ExampleResolutions all = resolveExamples(fixture);
    EXPECT_EQ(all.resolved.size(), 41U);
    EXPECT_EQ(all.resolved, all.expected);
    // The embedded curves that were asked for: one for each reference the database's motor
    // did not fit by its digest.
    EXPECT_EQ(fixture.attachments().asked().size(), 7U);
}

TEST(MotorHandlerGetMotor, EveryCallSearchesAgain)
{
    HandlerFixture   fixture;
    MotorHandler     handler(fixture.context());
    const HandlerRun run = runHandler(handler, "<motor><designation>C6</designation></motor>");
    EXPECT_TRUE(run.result.has_value());
    WarningSet warnings;
    EXPECT_EQ(handler.getMotor(warnings), nullptr);
    const std::shared_ptr<const ThrustCurveMotor> motor = different();
    fixture.motorFinder().setMotor(motor);
    EXPECT_EQ(handler.getMotor(warnings), motor);
    EXPECT_EQ(fixture.motorFinder().queries().size(), 2U);
}

TEST(MotorHandlerGetMotor, AContextWithoutAFinderIsABug)
{
    const DocumentLoadingContext context;
    const MotorHandler           handler(context);
    WarningSet                   warnings;
    EXPECT_THROW(static_cast<void>(handler.getMotor(warnings)), BugError);
}

}  // namespace
