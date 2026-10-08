#include <memory>
#include <optional>

#include <gtest/gtest.h>

#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/OpenRocketDocumentFactory.h"
#include "QtRocket/document/events/DocumentChangeEvent.h"
#include "QtRocket/preferences/DocumentPreferences.h"
#include "QtRocket/util/Color.h"
#include "document/DocumentTestSupport.h"

// OpenRocketDocumentPreferencesTest.java, its three cases. Java counts the calls of a
// DocumentChangeListener and asserts the source of each inside it; here a DocumentRecorder
// records the emissions of documentChanged() and names the source of each ("D(DocumentPreferences)"
// is an event of kind DOCUMENT whose source is the preferences of this very document), and the
// last event is checked for the object.

namespace
{

using QtRocket::Color;
using QtRocket::DocumentChangeEvent;
using QtRocket::DocumentPreferences;
using QtRocket::OpenRocketDocument;
using QtRocket::OpenRocketDocumentFactory;
using QtRocket::Test::DocumentRecorder;

// OpenRocketDocumentPreferencesTest.changingSavedViewPreferenceMarksDocumentChanged
TEST(OpenRocketDocumentPreferencesTest, ChangingSavedViewPreferenceMarksDocumentChanged)
{
    const std::unique_ptr<OpenRocketDocument> document =
        OpenRocketDocumentFactory::createNewRocket();
    DocumentPreferences& preferences = document->getDocumentPreferences();
    DocumentRecorder     changeEvents(*document);

    EXPECT_TRUE(document->isSaved());
    // Java: Color.BLUE
    preferences.putColor(DocumentPreferences::kPref3DBackgroundColor, Color{0, 0, 255});

    EXPECT_FALSE(document->isSaved());
    // Java: assertSame(preferences, event.getSource()) in the listener, and one event.
    EXPECT_EQ(changeEvents.take(), "D(DocumentPreferences)");
    ASSERT_TRUE(changeEvents.lastEvent().has_value());
    const DocumentChangeEvent event = changeEvents.lastEvent().value_or(DocumentChangeEvent{});
    EXPECT_EQ(event.getPreferences(), &preferences);
}

// OpenRocketDocumentPreferencesTest.unchangedOrMissingPreferenceDoesNotMarkDocumentChanged
TEST(OpenRocketDocumentPreferencesTest, UnchangedOrMissingPreferenceDoesNotMarkDocumentChanged)
{
    const std::unique_ptr<OpenRocketDocument> document =
        OpenRocketDocumentFactory::createNewRocket();
    DocumentPreferences& preferences = document->getDocumentPreferences();
    preferences.putBoolean(DocumentPreferences::kPref3DShadowsEnabled, true);
    document->setSaved(true);
    DocumentRecorder changeEvents(*document);

    preferences.putBoolean(DocumentPreferences::kPref3DShadowsEnabled, true);
    preferences.removePreference("missing.preference");

    EXPECT_TRUE(document->isSaved());
    EXPECT_EQ(changeEvents.take(), "");
}

// OpenRocketDocumentPreferencesTest.removingSavedViewPreferenceMarksDocumentChanged
TEST(OpenRocketDocumentPreferencesTest, RemovingSavedViewPreferenceMarksDocumentChanged)
{
    const std::unique_ptr<OpenRocketDocument> document =
        OpenRocketDocumentFactory::createNewRocket();
    DocumentPreferences& preferences = document->getDocumentPreferences();
    preferences.putBoolean(DocumentPreferences::kPref3DShadowsEnabled, true);
    document->setSaved(true);

    preferences.removePreference(DocumentPreferences::kPref3DShadowsEnabled);

    EXPECT_FALSE(document->isSaved());
}

}  // namespace
