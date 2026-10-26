#include <array>
#include <cstddef>
#include <format>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <utility>

#include <gtest/gtest.h>

#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/OpenRocketDocumentFactory.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/util/ModId.h"
#include "document/DocumentTestSupport.h"

// OpenRocketDocument::pinFlightConfigurationNames(): "Save as default" of OpenRocket's rename
// dialog changes the default name of flight configurations, and OpenRocket keeps the names of the
// configurations that exist, in the rocket and in every undo state, because Java's
// FlightConfiguration stores the default name when it is constructed. QtRocket's looks it up
// when asked, so the GUI pins the names first (decision O-M3-7). The expected texts are the
// output of the probe of part C5 (M3 run 1, probe/C5Probe.java, defaultNames(), OpenRocket
// 5f164fd0e), in its notation: per configuration its raw name, its name and isNameOverridden().

namespace
{

using QtRocket::FlightConfiguration;
using QtRocket::FlightConfigurationId;
using QtRocket::InMemoryPreferences;
using QtRocket::OpenRocketDocument;
using QtRocket::OpenRocketDocumentFactory;
using QtRocket::Rocket;

/// What the probe prints of @p configuration.
[[nodiscard]] std::string nameOf(const FlightConfiguration& configuration,
                                 const InMemoryPreferences& preferences)
{
    return std::format(R"(raw "{}" name "{}" overridden {})", configuration.getNameRaw(preferences),
                       configuration.getName(preferences),
                       configuration.isNameOverridden(preferences));
}

/// What the probe prints of @p rocket's configurations @p ids, lettered A, B, ... in their order,
/// and of its default configuration, one per line.
[[nodiscard]] std::string namesOf(const Rocket& rocket, const InMemoryPreferences& preferences,
                                  std::span<const FlightConfigurationId> ids)
{
    std::string text;
    char        letter = 'A';
    for (const FlightConfigurationId& fcid : ids)
    {
        text += rocket.containsFlightConfigurationId(fcid)
                    ? std::format("{} {}\n", letter,
                                  nameOf(rocket.getFlightConfiguration(fcid), preferences))
                    : std::format("{} absent\n", letter);
        ++letter;
    }
    return text + "default " + nameOf(rocket.getEmptyConfiguration(), preferences) + "\n";
}

/// The probe's design: a rocket with a stage and a body tube, without motors, and the flight
/// configurations A (the default name) and B ("Named"), in a document.
class NamePinning : public ::testing::Test
{
protected:
    NamePinning()
    {
        auto                  rocket = std::make_unique<Rocket>();
        QtRocket::AxialStage& stage  = rocket->addChild(std::make_unique<QtRocket::AxialStage>());
        stage.addChild(std::make_unique<QtRocket::BodyTube>());
        rocket->createFlightConfiguration(m_ids.at(0));
        rocket->createFlightConfiguration(m_ids.at(1)).setName("Named");
        m_document = OpenRocketDocumentFactory::createDocumentFromRocket(std::move(rocket));
    }

    [[nodiscard]] Rocket& rocket() { return m_document->getRocket(); }

    /// The names of the configurations A to E (as far as @p count) and of the default one.
    [[nodiscard]] std::string names(std::size_t count)
    {
        return namesOf(rocket(), m_preferences, std::span(m_ids).first(count));
    }

    /// Creates the configuration @p index (0 for A) in the rocket.
    void create(std::size_t index) { rocket().createFlightConfiguration(m_ids.at(index)); }

    /// What the GUI does for "Save as default": pins the names, then stores @p name as the
    /// default name.
    void saveAsDefault(std::string_view name)
    {
        m_document->pinFlightConfigurationNames(m_preferences);
        m_preferences.setDefaultFlightConfigName(name);
    }

    InMemoryPreferences                  m_preferences;
    std::array<FlightConfigurationId, 5> m_ids;
    std::unique_ptr<OpenRocketDocument>  m_document;
};

/// The probe's lines "before" (A and B).
constexpr std::string_view kBefore =
    "A raw \"[{motors}]\" name \"[No motors]\" overridden false\n"
    "B raw \"Named\" name \"Named\" overridden true\n"
    "default raw \"[{motors}]\" name \"[No motors]\" overridden "
    "false\n";

/// The probe's lines once the default is "{motors} new" with A to D in the rocket: A, B, C and
/// the default configuration keep their names, D (made after the change) takes the new one.
constexpr std::string_view kAfterTheChange =
    "A raw \"[{motors}]\" name \"[No motors]\" overridden true\n"
    "B raw \"Named\" name \"Named\" overridden true\n"
    "C raw \"[{motors}]\" name \"[No motors]\" overridden true\n"
    "D raw \"{motors} new\" name \"No motors new\" overridden false\n"
    "default raw \"[{motors}]\" name \"[No motors]\" overridden true\n";

// The probe's sequence: the existing configurations keep their names when the default changes,
// a new one takes the new default, and an undo and a redo bring back the names the
// configurations had, also the undo state made before the change (state 0, of A and B).
TEST_F(NamePinning, TheConfigurationsKeepTheirNamesThroughUndoAndRedo)
{
    EXPECT_EQ(names(2), kBefore);
    m_document->addUndoPosition("Add C");
    create(2);
    EXPECT_EQ(names(3),
              "A raw \"[{motors}]\" name \"[No motors]\" overridden false\n"
              "B raw \"Named\" name \"Named\" overridden true\n"
              "C raw \"[{motors}]\" name \"[No motors]\" overridden false\n"
              "default raw \"[{motors}]\" name \"[No motors]\" overridden false\n");

    saveAsDefault("{motors} new");
    create(3);
    EXPECT_EQ(names(4), kAfterTheChange);

    m_document->undo();
    EXPECT_EQ(names(4),
              "A raw \"[{motors}]\" name \"[No motors]\" overridden true\n"
              "B raw \"Named\" name \"Named\" overridden true\n"
              "C absent\n"
              "D absent\n"
              "default raw \"[{motors}]\" name \"[No motors]\" overridden true\n");
    m_document->redo();
    EXPECT_EQ(names(4), kAfterTheChange);
}

// A name reset to the default stores the default of the moment in Java ("B reset"), so it is
// kept when the default changes again ("pref third"); here it follows the default until the
// next pinning, which gives the same names.
TEST_F(NamePinning, AResetNameKeepsTheDefaultOfItsMoment)
{
    saveAsDefault("{motors} new");
    rocket().getFlightConfiguration(m_ids.at(1)).setName("");
    EXPECT_EQ(names(2),
              "A raw \"[{motors}]\" name \"[No motors]\" overridden true\n"
              "B raw \"{motors} new\" name \"No motors new\" overridden false\n"
              "default raw \"[{motors}]\" name \"[No motors]\" overridden true\n");
    saveAsDefault("third");
    EXPECT_EQ(names(2),
              "A raw \"[{motors}]\" name \"[No motors]\" overridden true\n"
              "B raw \"{motors} new\" name \"No motors new\" overridden true\n"
              "default raw \"[{motors}]\" name \"[No motors]\" overridden true\n");
}

// The redo states are pinned too: a configuration that only a redo state holds when the default
// changes comes back with the old default.
TEST_F(NamePinning, TheRedoStatesArePinnedToo)
{
    m_document->addUndoPosition("Add C");
    create(2);
    m_document->undo();
    ASSERT_FALSE(rocket().containsFlightConfigurationId(m_ids.at(2)));
    ASSERT_TRUE(m_document->isRedoAvailable());

    saveAsDefault("{motors} new");
    m_document->redo();
    EXPECT_EQ(names(3),
              "A raw \"[{motors}]\" name \"[No motors]\" overridden true\n"
              "B raw \"Named\" name \"Named\" overridden true\n"
              "C raw \"[{motors}]\" name \"[No motors]\" overridden true\n"
              "default raw \"[{motors}]\" name \"[No motors]\" overridden true\n");
}

// Without the pinning the configurations without a name of their own follow the preference
// (QtRocket's lazy default name, FlightConfiguration's class comment), in the rocket and after
// an undo: what the helper is for.
TEST_F(NamePinning, WithoutThePinningTheNamesFollowThePreference)
{
    m_document->addUndoPosition("Add C");
    create(2);
    m_preferences.setDefaultFlightConfigName("{motors} new");
    m_document->undo();
    EXPECT_EQ(names(2),
              "A raw \"{motors} new\" name \"No motors new\" overridden false\n"
              "B raw \"Named\" name \"Named\" overridden true\n"
              "default raw \"{motors} new\" name \"No motors new\" overridden false\n");
}

// The pinning changes nothing else: no event of the document or of the rocket, the same undo
// history (positions, descriptions, clean or dirty), the document saved as it was, the rocket's
// modification id. A second pinning changes nothing at all.
TEST_F(NamePinning, ChangesNothingElse)
{
    m_document->addUndoPosition("Add C");
    create(2);
    m_document->setSaved(true);
    const std::string                   before = QtRocket::Test::state(*m_document);
    const QtRocket::ModId               modId  = rocket().getModId();
    QtRocket::Test::DocumentRecorder    documentEvents(*m_document);
    QtRocket::Test::RocketEventRecorder rocketEvents(rocket());

    m_document->pinFlightConfigurationNames(m_preferences);
    EXPECT_EQ(documentEvents.take(), "");
    EXPECT_EQ(rocketEvents.take(), "");
    EXPECT_EQ(QtRocket::Test::state(*m_document), before);
    EXPECT_TRUE(m_document->isSaved());
    EXPECT_EQ(rocket().getModId(), modId);
    EXPECT_EQ(rocket().getFlightConfiguration(m_ids.at(0)).getStoredName(), "[{motors}]");
    EXPECT_EQ(rocket().getFlightConfiguration(m_ids.at(1)).getStoredName(), "Named");
    EXPECT_EQ(rocket().getEmptyConfiguration().getStoredName(), "[{motors}]");

    m_preferences.setDefaultFlightConfigName("other");
    m_document->pinFlightConfigurationNames(m_preferences);
    EXPECT_EQ(rocket().getFlightConfiguration(m_ids.at(0)).getStoredName(), "[{motors}]")
        << "a pinned name stays";
    EXPECT_EQ(documentEvents.take(), "");
}

// Deviation: an empty default cannot be stored. Java's configuration E, made while the default is
// "", keeps "" when the default becomes "fourth" (the probe: E raw "" name "" overridden true);
// here it follows the preference.
TEST_F(NamePinning, AnEmptyDefaultCannotBePinned)
{
    saveAsDefault("");
    create(4);
    EXPECT_EQ(nameOf(rocket().getFlightConfiguration(m_ids.at(4)), m_preferences),
              R"(raw "" name "" overridden false)")
        << "the line of E the probe prints with the empty default";
    saveAsDefault("fourth");
    EXPECT_EQ(nameOf(rocket().getFlightConfiguration(m_ids.at(4)), m_preferences),
              R"(raw "fourth" name "fourth" overridden false)");
    EXPECT_EQ(nameOf(rocket().getFlightConfiguration(m_ids.at(0)), m_preferences),
              R"(raw "[{motors}]" name "[No motors]" overridden true)");
}

}  // namespace
