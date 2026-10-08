#pragma once

// What the tests of the .ork loader's handlers share: a loading context with everything a
// handler asks for, and a way to run a snippet of XML through one handler without the loader
// around it. Test-only.

#include <cstddef>
#include <format>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "QtRocket/document/Attachment.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/OpenRocketDocumentFactory.h"
#include "QtRocket/file/AttachmentFactory.h"
#include "QtRocket/file/DocumentLoadingContext.h"
#include "QtRocket/file/MotorFinder.h"
#include "QtRocket/file/simplesax/ElementHandler.h"
#include "QtRocket/file/simplesax/SimpleSax.h"
#include "QtRocket/logging/Warning.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/material/MaterialStorage.h"
#include "QtRocket/motor/Manufacturer.h"
#include "QtRocket/motor/Motor.h"
#include "QtRocket/motor/MotorDigest.h"
#include "QtRocket/motor/ThrustCurveMotor.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"
#include "QtRocket/util/Strings.h"
#include "document/TestAttachments.h"

namespace QtRocket::Test
{

/// The texts of @p warnings (Message::toString()), in the order they were added.
[[nodiscard]] inline std::vector<std::string> warningTexts(const WarningSet& warnings)
{
    std::vector<std::string> texts;
    for (const Warning& warning : warnings)
    {
        texts.push_back(warning.toString());
    }
    return texts;
}

/// A motor as OpenRocket's MotorHandlerTest makes its motors (createMotor()): an Estes motor of
/// unknown type, 24 mm by 70 mm, with the delays 3 s and plugged, three points (0 s, 0.5 s,
/// 1 s) whose middle thrust is @p peakThrust, a mass of 100 g falling to 40 g, and @p digest as
/// its digest, whatever its data.
[[nodiscard]] inline std::shared_ptr<const ThrustCurveMotor> makeEmbeddedTestMotor(
    std::string designation, double peakThrust, std::string digest)
{
    Result<ThrustCurveMotor> motor =
        ThrustCurveMotor::Builder()
            // A known manufacturer default proves that explicit UNKNOWN is preserved.
            .setManufacturer(Manufacturer::getManufacturer("Estes"))
            .setDesignation(std::move(designation))
            .setDescription("Embedded motor test")
            .setMotorType(Motor::Type::UNKNOWN)
            .setStandardDelays({3, Motor::kPluggedDelay})
            .setDiameter(0.024)
            .setLength(0.070)
            .setTimePoints({0.0, 0.5, 1.0})
            .setThrustPoints({0.0, peakThrust, 0.0})
            .setCGPoints({Coordinate(0.035, 0, 0, 0.100), Coordinate(0.033, 0, 0, 0.070),
                          Coordinate(0.031, 0, 0, 0.040)})
            .setDigest(std::move(digest))
            .build();
    if (!motor)
    {
        bug("the test motor does not build: " + motor.error().message);
    }
    return std::make_shared<const ThrustCurveMotor>(std::move(*motor));
}

/// The digest an .eng file's reader gives @p motor (MotorHandlerTest.createRaspStyleDigest()):
/// the times, the launch and burnout mass and the thrusts. It is one of the digests of older
/// file formats that MotorDigest::isDigestCompatible() takes for the motor.
[[nodiscard]] inline std::string raspStyleDigest(const ThrustCurveMotor& motor)
{
    MotorDigest digest;
    digest.update(MotorDigest::DataType::TIME_ARRAY, motor.getTimePoints());
    digest.update(MotorDigest::DataType::MASS_SPECIFIC,
                  {motor.getLaunchMass(), motor.getBurnoutMass()});
    digest.update(MotorDigest::DataType::FORCE_PER_TIME, motor.getThrustPoints());
    return digest.getDigest();
}

/// A motor finder that gives every query the same scripted answer: a motor (or none) and, when
/// one is set, a warning, as a database search would add one. It records what it was asked.
class ScriptedMotorFinder final : public MotorFinder
{
public:
    ScriptedMotorFinder() = default;
    /// A finder that answers every query with @p motor.
    explicit ScriptedMotorFinder(std::shared_ptr<const Motor> motor) : m_motor(std::move(motor)) { }

    /// The motor every query is answered with from now on; null for none.
    void setMotor(std::shared_ptr<const Motor> motor) { m_motor = std::move(motor); }
    /// The text of the warning every query adds from now on; none for no warning.
    void setWarning(std::optional<std::string> text) { m_warning = std::move(text); }

    [[nodiscard]] std::shared_ptr<const Motor> findMotor(
        std::optional<Motor::Type> type, std::optional<std::string_view> manufacturer,
        std::optional<std::string_view> designation, double diameter, double length,
        std::optional<std::string_view> digest, WarningSet& warnings) const override
    {
        m_queries.push_back(
            std::format("find(type={}, manufacturer={}, designation={}, diameter={}, length={}, "
                        "digest={})",
                        type.has_value() ? enumName(*type) : "null", manufacturer.value_or("null"),
                        designation.value_or("null"), Strings::javaDoubleToString(diameter),
                        Strings::javaDoubleToString(length), digest.value_or("null")));
        if (m_warning.has_value())
        {
            warnings.add(*m_warning);
        }
        return m_motor;
    }

    /// Every query so far, as "find(type=SINGLE, manufacturer=Estes, designation=C6,
    /// diameter=NaN, length=NaN, digest=null)": the type by the name of its enum constant, a
    /// criterion that was not given as null, the numbers as Java prints a double.
    [[nodiscard]] const std::vector<std::string>& queries() const noexcept { return m_queries; }

private:
    std::shared_ptr<const Motor>     m_motor;
    std::optional<std::string>       m_warning;
    mutable std::vector<std::string> m_queries;
};

/// An attachment factory over a map of names to bytes: the attachment of a name it has returns
/// those bytes, one it was told to fail returns that failure, and any other is missing (its
/// getBytes() fails with ErrorCode::NOT_FOUND, as an archive without the entry). It records the
/// names it was asked for.
class MapAttachmentFactory final : public AttachmentFactory
{
public:
    /// Gives the attachment named @p name the contents @p text.
    void put(std::string name, std::string_view text)
    {
        m_contents.insert_or_assign(std::move(name), std::string(text));
    }
    /// Gives the attachment named @p name the contents @p bytes.
    void put(std::string name, const std::vector<std::byte>& bytes)
    {
        m_contents.insert_or_assign(std::move(name), bytesToString(bytes));
    }
    /// Makes the attachment named @p name one that is there and cannot be read: its getBytes()
    /// fails with @p code and @p message.
    void putFailure(std::string name, ErrorCode code, std::string message)
    {
        m_failures.insert_or_assign(
            std::move(name), Error{.code = code, .message = std::move(message), .where = {}});
    }

    [[nodiscard]] std::shared_ptr<Attachment> getAttachment(std::string_view name) const override
    {
        m_asked.emplace_back(name);
        if (const auto contents = m_contents.find(name); contents != m_contents.end())
        {
            return std::make_shared<MemoryAttachment>(std::string(name), contents->second);
        }
        if (const auto failure = m_failures.find(name); failure != m_failures.end())
        {
            return std::make_shared<FailingAttachment>(std::string(name), failure->second.code,
                                                       failure->second.message);
        }
        return std::make_shared<FailingAttachment>(std::string(name));
    }

    /// The names getAttachment() was called with, in order.
    [[nodiscard]] const std::vector<std::string>& asked() const noexcept { return m_asked; }

private:
    std::map<std::string, std::string, std::less<>> m_contents;
    std::map<std::string, Error, std::less<>>       m_failures;
    mutable std::vector<std::string>                m_asked;
};

/// What running a snippet through a handler gave.
struct HandlerRun
{
    /// The failure of the handler (or of the XML), if any.
    Result<void> result;
    /// The warnings the handlers added, in order.
    WarningSet warnings;
    /// What the handler's parent is told when the snippet's element closes: its name, its
    /// attributes and its text. A parent handler in the loader acts on these (the motor
    /// mount's handler reads the configuration id of a <motor> there).
    std::string                element;
    ElementHandler::Attributes attributes;
    std::string                content;

    /// warningTexts(warnings).
    [[nodiscard]] std::vector<std::string> texts() const { return warningTexts(warnings); }
};

/// Runs @p xml, one element with its content, through @p handler as the loader would: the
/// handler is what the element's parent returns for the element, so it gets openElement() and
/// closeElement() for every child of the element and endHandler() when the element closes.
/// The parent itself is a stand-in that records what its closeElement() is given (see
/// HandlerRun).
///
///     MotorHandler handler(fixture.context());
///     const HandlerRun run = runHandler(handler, "<motor configid='a'><delay>3</delay></motor>");
///
/// The XML goes through SimpleSax, so the handler sees what the real reader gives it, the
/// quirks of DelegatorHandler included.
[[nodiscard]] inline HandlerRun runHandler(ElementHandler& handler, std::string_view xml)
{
    /// The parent of the element: it hands the element to the handler under test.
    class Parent final : public ElementHandler
    {
    public:
        Parent(ElementHandler& handler, HandlerRun& run) : m_handler(&handler), m_run(&run) { }

        [[nodiscard]] Result<ElementHandler*> openElement(std::string_view /*element*/,
                                                          const Attributes& /*attributes*/,
                                                          WarningSet& /*warnings*/) override
        {
            return m_handler;
        }
        [[nodiscard]] Result<void> closeElement(std::string_view  element,
                                                const Attributes& attributes,
                                                std::string_view  content,
                                                WarningSet& /*warnings*/) override
        {
            m_run->element    = std::string(element);
            m_run->attributes = attributes;
            m_run->content    = std::string(content);
            return {};
        }
        [[nodiscard]] Result<void> endHandler(std::string_view /*element*/,
                                              const Attributes& /*attributes*/,
                                              std::string_view /*content*/,
                                              WarningSet& /*warnings*/) override
        {
            return {};
        }

    private:
        ElementHandler* m_handler;
        HandlerRun*     m_run;
    };

    HandlerRun run;
    Parent     parent(handler, run);
    run.result = SimpleSax::readXml(xml, parent, run.warnings);
    return run;
}

/// The environment of a handler under test: a DocumentLoadingContext whose every part is there.
/// - The document is the one a loader starts from (OpenRocketDocumentFactory::
///   createEmptyRocket()): a rocket without a stage, its events enabled.
/// - The motor finder is a ScriptedMotorFinder: it finds nothing until a test gives it a motor.
/// - The attachment factory is a MapAttachmentFactory: every attachment is missing until a
///   test puts one.
/// - The preference store is an empty InMemoryPreferences. A test that compares with what
///   OpenRocket computes under its test preferences stores those first
///   (storeJavaTestPreferences() of simulation/SimulationRunSupport.h).
/// - The application's materials are a MaterialStorage as it is made (OpenRocket's own
///   materials).
/// - The file version is the newest, 1.11, so that a motor's digest counts.
/// - There are no component presets and no extension providers until a test sets some
///   (context().setComponentPresetDatabase(), context().setSimulationExtensionRegistry()).
///
/// The members are declared so that the preference store outlives the document, whose
/// simulations refer to it.
class HandlerFixture
{
public:
    /// The file version of the context: 1.11.
    static constexpr int kFileVersion = 111;

    HandlerFixture() : m_document(OpenRocketDocumentFactory::createEmptyRocket())
    {
        m_context.setFileVersion(kFileVersion);
        m_context.setMotorFinder(&m_motorFinder);
        m_context.setAttachmentFactory(&m_attachments);
        m_context.setOpenRocketDocument(m_document.get());
        m_context.setApplicationMaterials(&m_materials);
        m_context.setPreferences(&m_preferences);
    }
    ~HandlerFixture() = default;

    // The context points at the members.
    HandlerFixture(const HandlerFixture&)            = delete;
    HandlerFixture& operator=(const HandlerFixture&) = delete;
    HandlerFixture(HandlerFixture&&)                 = delete;
    HandlerFixture& operator=(HandlerFixture&&)      = delete;

    [[nodiscard]] DocumentLoadingContext& context() noexcept { return m_context; }
    [[nodiscard]] OpenRocketDocument&     document() noexcept { return *m_document; }
    [[nodiscard]] Rocket&                 rocket() { return m_document->getRocket(); }
    [[nodiscard]] ScriptedMotorFinder&    motorFinder() noexcept { return m_motorFinder; }
    [[nodiscard]] MapAttachmentFactory&   attachments() noexcept { return m_attachments; }
    [[nodiscard]] InMemoryPreferences&    preferences() noexcept { return m_preferences; }
    [[nodiscard]] MaterialStorage&        materials() noexcept { return m_materials; }

private:
    InMemoryPreferences                 m_preferences;
    MaterialStorage                     m_materials;
    ScriptedMotorFinder                 m_motorFinder;
    MapAttachmentFactory                m_attachments;
    std::unique_ptr<OpenRocketDocument> m_document;
    DocumentLoadingContext              m_context;
};

}  // namespace QtRocket::Test
