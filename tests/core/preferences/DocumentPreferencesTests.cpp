#include "QtRocket/preferences/DocumentPreferences.h"

#include <cmath>
#include <limits>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/util/Color.h"
#include "QtRocket/util/Signal.h"

namespace
{

using QtRocket::Color;
using QtRocket::DocumentPreferences;
using Type               = DocumentPreferences::Type;
using DocumentPreference = DocumentPreferences::DocumentPreference;

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

/// Counts the emissions of a signal.
class ChangeCounter
{
public:
    explicit ChangeCounter(QtRocket::Signal<>& signal)
      : m_connection(signal.connect([this] { ++m_count; }))
    {
    }
    [[nodiscard]] int count() const noexcept { return m_count; }

private:
    int                                  m_count{0};
    QtRocket::Signal<>::ScopedConnection m_connection;
};

// OpenRocket has no DocumentPreferences tests; these are QtRocket's.

TEST(DocumentPreferences, KeysMatchOpenRocket)
{
    EXPECT_EQ(DocumentPreferences::kPrefShowWarnings, "RocketPanel.showWarnings");
    EXPECT_EQ(DocumentPreferences::kPref2DBackgroundColor, "RocketPanel.2DBackgroundColor");
    EXPECT_EQ(DocumentPreferences::kPref3DBackgroundColor, "RocketPanel.3DBackgroundColor");
    EXPECT_EQ(DocumentPreferences::kPref2DTextColor, "RocketPanel.2DTextColor");
    EXPECT_EQ(DocumentPreferences::kPref3DTextColor, "RocketPanel.3DTextColor");
    EXPECT_EQ(DocumentPreferences::kPref3DRenderQuality, "RocketPanel.3DRenderQuality");
    EXPECT_EQ(DocumentPreferences::kPref3DShadowsEnabled, "RocketPanel.3DShadowsEnabled");
    EXPECT_EQ(DocumentPreferences::kPref3DAmbientOcclusionEnabled,
              "RocketPanel.3DAmbientOcclusionEnabled");
    EXPECT_EQ(DocumentPreferences::kPref3DRoughnessBumpEnabled,
              "RocketPanel.3DRoughnessBumpEnabled");
    EXPECT_EQ(DocumentPreferences::kPref3DOriginAxesVisible, "RocketPanel.3DOriginAxesVisible");
    EXPECT_EQ(DocumentPreferences::kPref3DLightVisualizersVisible,
              "RocketPanel.3DLightVisualizersVisible");
    EXPECT_EQ(DocumentPreferences::kPref3DCameraPointOfInterestVisible,
              "RocketPanel.3DCameraPointOfInterestVisible");
    EXPECT_EQ(DocumentPreferences::kPref3DRotateRocketOnDrag, "RocketPanel.3DRotateRocketOnDrag");
    EXPECT_EQ(DocumentPreferences::kPref3DCaretScaleWithView, "RocketPanel.3DCaretScaleWithView");
}

TEST(DocumentPreferences, StartsEmpty)
{
    const DocumentPreferences prefs;
    EXPECT_EQ(prefs.size(), 0U);
    EXPECT_TRUE(prefs.preferences().empty());
    EXPECT_FALSE(prefs.contains("x"));
    EXPECT_EQ(prefs.find("x"), nullptr);
    EXPECT_EQ(prefs.materialCount(), 0U);
    EXPECT_TRUE(prefs.materials().empty());
}

TEST(DocumentPreferences, AbsentKeysGiveTheDefault)
{
    const DocumentPreferences prefs;
    EXPECT_TRUE(prefs.getBoolean("b", true));
    EXPECT_FALSE(prefs.getBoolean("b", false));
    EXPECT_EQ(prefs.getInt("i", 42), 42);
    EXPECT_EQ(prefs.getDouble("d", 1.5), 1.5);
    EXPECT_EQ(prefs.getString("s", "dflt"), "dflt");
    EXPECT_EQ(prefs.getColor("c"), std::nullopt);
    EXPECT_EQ(prefs.getColor("c", Color(1, 2, 3)), Color(1, 2, 3));
}

TEST(DocumentPreferences, TypedRoundTrips)
{
    DocumentPreferences prefs;
    prefs.putBoolean(DocumentPreferences::kPrefShowWarnings, false);
    prefs.putInt(DocumentPreferences::kPref3DRenderQuality, 3);
    prefs.putDouble("d", 0.25);
    prefs.putString(DocumentPreferences::kPref2DTextColor, "10,20,30");

    EXPECT_FALSE(prefs.getBoolean(DocumentPreferences::kPrefShowWarnings, true));
    EXPECT_EQ(prefs.getInt(DocumentPreferences::kPref3DRenderQuality, 0), 3);
    EXPECT_EQ(prefs.getDouble("d", 0), 0.25);
    EXPECT_EQ(prefs.getString(DocumentPreferences::kPref2DTextColor, ""), "10,20,30");
    EXPECT_EQ(prefs.size(), 4U);
    EXPECT_TRUE(prefs.contains("d"));

    // The stored type follows the putter.
    ASSERT_NE(prefs.find(DocumentPreferences::kPrefShowWarnings), nullptr);
    EXPECT_EQ(prefs.find(DocumentPreferences::kPrefShowWarnings)->type(), Type::BOOLEAN);
    EXPECT_EQ(prefs.find(DocumentPreferences::kPref3DRenderQuality)->type(), Type::INTEGER);
    EXPECT_EQ(prefs.find("d")->type(), Type::DOUBLE);
    EXPECT_EQ(prefs.find(DocumentPreferences::kPref2DTextColor)->type(), Type::STRING);
    EXPECT_EQ(std::get<int>(prefs.find(DocumentPreferences::kPref3DRenderQuality)->value()), 3);
}

TEST(DocumentPreferences, ReadingWithAnotherTypeGivesTheDefault)
{
    // Deviation from Java, which throws ClassCastException (see the header).
    DocumentPreferences prefs;
    prefs.putString("s", "true");
    prefs.putInt("i", 1);
    prefs.putDouble("d", 1.0);
    prefs.putBoolean("b", true);

    EXPECT_TRUE(prefs.getBoolean("s", true));
    EXPECT_FALSE(prefs.getBoolean("s", false));
    EXPECT_EQ(prefs.getInt("d", -1), -1);
    EXPECT_EQ(prefs.getDouble("i", -1.0), -1.0);
    EXPECT_EQ(prefs.getString("b", "x"), "x");
    EXPECT_EQ(prefs.getInt("s", 9), 9);
}

TEST(DocumentPreferences, ReplacingAValueChangesItsType)
{
    DocumentPreferences prefs;
    prefs.putInt("k", 1);
    prefs.putString("k", "one");
    EXPECT_EQ(prefs.size(), 1U);
    EXPECT_EQ(prefs.find("k")->type(), Type::STRING);
    EXPECT_EQ(prefs.getString("k", ""), "one");
    EXPECT_EQ(prefs.getInt("k", 0), 0);
}

TEST(DocumentPreferences, PutEmitsChangedOnlyForANewValue)
{
    DocumentPreferences prefs;
    const ChangeCounter counter(prefs.changed());

    prefs.putInt("i", 1);
    EXPECT_EQ(counter.count(), 1);
    prefs.putInt("i", 1);  // Objects.equals: the same value, no event
    EXPECT_EQ(counter.count(), 1);
    prefs.putInt("i", 2);
    EXPECT_EQ(counter.count(), 2);
    prefs.putDouble("i", 2.0);  // Integer(2) and Double(2.0) are not equal
    EXPECT_EQ(counter.count(), 3);
    prefs.putString("s", "a");
    prefs.putString("s", "a");
    EXPECT_EQ(counter.count(), 4);
    prefs.putBoolean("b", true);
    prefs.putBoolean("b", true);
    prefs.putBoolean("b", false);
    EXPECT_EQ(counter.count(), 6);
}

TEST(DocumentPreferences, DoublesCompareAsJavaDoubleEquals)
{
    DocumentPreferences prefs;
    const ChangeCounter counter(prefs.changed());

    prefs.putDouble("d", kNaN);
    EXPECT_EQ(counter.count(), 1);
    prefs.putDouble("d", kNaN);  // Double.equals(NaN, NaN) is true
    EXPECT_EQ(counter.count(), 1);
    EXPECT_TRUE(std::isnan(prefs.getDouble("d", 0)));

    prefs.putDouble("z", 0.0);
    EXPECT_EQ(counter.count(), 2);
    prefs.putDouble("z", -0.0);  // Double.equals(0.0, -0.0) is false
    EXPECT_EQ(counter.count(), 3);
    EXPECT_TRUE(std::signbit(prefs.getDouble("z", 1)));
    prefs.putDouble("z", -0.0);
    EXPECT_EQ(counter.count(), 3);
}

TEST(DocumentPreferences, RemoveEmitsChangedWhenTheKeyExisted)
{
    DocumentPreferences prefs;
    const ChangeCounter counter(prefs.changed());

    prefs.removePreference("missing");
    EXPECT_EQ(counter.count(), 0);
    prefs.putInt("i", 1);
    prefs.removePreference("i");
    EXPECT_EQ(counter.count(), 2);
    EXPECT_FALSE(prefs.contains("i"));
    EXPECT_EQ(prefs.getInt("i", 5), 5);
    prefs.removePreference("i");
    EXPECT_EQ(counter.count(), 2);
}

TEST(DocumentPreferences, PreferencesMapIsSortedByKey)
{
    DocumentPreferences prefs;
    prefs.putInt("b", 2);
    prefs.putInt("a", 1);
    prefs.putInt("c", 3);
    std::vector<std::string> keys;
    for (const auto& [key, pref] : prefs.preferences())
    {
        keys.push_back(key);
    }
    EXPECT_EQ(keys, (std::vector<std::string>{"a", "b", "c"}));
}

TEST(DocumentPreferences, ColorsAreStoredAsStrings)
{
    DocumentPreferences prefs;
    prefs.putColor("c", Color(255, 128, 64));
    EXPECT_EQ(prefs.getString("c", ""), "255,128,64");
    EXPECT_EQ(prefs.find("c")->type(), Type::STRING);
    EXPECT_EQ(prefs.getColor("c"), Color(255, 128, 64));
    EXPECT_EQ(prefs.getColor("c", Color::black()), Color(255, 128, 64));

    // The alpha is not stored: it reads back opaque.
    prefs.putColor("c", Color(1, 2, 3, 4));
    EXPECT_EQ(prefs.getColor("c"), Color(1, 2, 3, 255));

    // null removes (and emits).
    const ChangeCounter counter(prefs.changed());
    prefs.putColor("c", std::nullopt);
    EXPECT_FALSE(prefs.contains("c"));
    EXPECT_EQ(counter.count(), 1);
    prefs.putColor("c", std::nullopt);
    EXPECT_EQ(counter.count(), 1);
}

TEST(DocumentPreferences, GetColorNeedsAParsableString)
{
    DocumentPreferences prefs;
    prefs.putString("bad", "not a colour");
    prefs.putInt("int", 5);
    EXPECT_EQ(prefs.getColor("bad"), std::nullopt);
    EXPECT_EQ(prefs.getColor("bad", Color(9, 9, 9)), Color(9, 9, 9));
    EXPECT_EQ(prefs.getColor("int"), std::nullopt);
    EXPECT_EQ(prefs.getColor("int", Color(9, 9, 9)), Color(9, 9, 9));
    // A user-edited string with spaces parses (this parseColor trims its fields).
    prefs.putString("spaced", " 1 , 2 ,3 ");
    EXPECT_EQ(prefs.getColor("spaced"), Color(1, 2, 3));
}

TEST(DocumentPreferences, ParseColor)
{
    EXPECT_EQ(DocumentPreferences::parseColor("255,128,64"), Color(255, 128, 64));
    EXPECT_EQ(DocumentPreferences::parseColor("0,0,0"), Color(0, 0, 0));
    EXPECT_EQ(DocumentPreferences::parseColor(" 10, 20 , 30 "), Color(10, 20, 30));
    EXPECT_EQ(DocumentPreferences::parseColor("+1,+2,+3"), Color(1, 2, 3));
    // Out of range channels are clamped.
    EXPECT_EQ(DocumentPreferences::parseColor("300,-5,256"), Color(255, 0, 255));
    // Java's split drops trailing empty fields, so a trailing comma still gives three.
    EXPECT_EQ(DocumentPreferences::parseColor("1,2,3,"), Color(1, 2, 3));
    EXPECT_EQ(DocumentPreferences::parseColor("1,2,3,,"), Color(1, 2, 3));

    EXPECT_EQ(DocumentPreferences::parseColor(""), std::nullopt);
    EXPECT_EQ(DocumentPreferences::parseColor("1,2"), std::nullopt);
    EXPECT_EQ(DocumentPreferences::parseColor("1,2,3,4"), std::nullopt);
    EXPECT_EQ(DocumentPreferences::parseColor(",1,2,3"), std::nullopt);
    EXPECT_EQ(DocumentPreferences::parseColor("1,,3"), std::nullopt);
    EXPECT_EQ(DocumentPreferences::parseColor("a,b,c"), std::nullopt);
    EXPECT_EQ(DocumentPreferences::parseColor("1.0,2,3"), std::nullopt);
    EXPECT_EQ(DocumentPreferences::parseColor("1;2;3"), std::nullopt);
    EXPECT_EQ(DocumentPreferences::parseColor("99999999999,2,3"), std::nullopt);  // not an int
}

TEST(DocumentPreferences, StringifyColor)
{
    EXPECT_EQ(DocumentPreferences::stringifyColor(Color(255, 128, 64)), "255,128,64");
    EXPECT_EQ(DocumentPreferences::stringifyColor(Color(0, 0, 0, 0)), "0,0,0");
}

TEST(DocumentPreferences, TypeNamesAreTheOrkTypeAttributes)
{
    EXPECT_EQ(DocumentPreferences::typeName(Type::BOOLEAN), "boolean");
    EXPECT_EQ(DocumentPreferences::typeName(Type::INTEGER), "integer");
    EXPECT_EQ(DocumentPreferences::typeName(Type::DOUBLE), "double");
    EXPECT_EQ(DocumentPreferences::typeName(Type::STRING), "string");
}

TEST(DocumentPreferences, DocumentPreferenceKnowsItsType)
{
    EXPECT_EQ(DocumentPreference(true).type(), Type::BOOLEAN);
    EXPECT_EQ(DocumentPreference(7).type(), Type::INTEGER);
    EXPECT_EQ(DocumentPreference(7.0).type(), Type::DOUBLE);
    EXPECT_EQ(DocumentPreference(std::string("7")).type(), Type::STRING);
    EXPECT_EQ(std::get<std::string>(DocumentPreference(std::string("7")).value()), "7");

    EXPECT_EQ(DocumentPreference(7), DocumentPreference(7));
    EXPECT_NE(DocumentPreference(7), DocumentPreference(7.0));
    EXPECT_NE(DocumentPreference(7), DocumentPreference(8));
    EXPECT_EQ(DocumentPreference(kNaN), DocumentPreference(kNaN));
    EXPECT_NE(DocumentPreference(0.0), DocumentPreference(-0.0));
    EXPECT_NE(DocumentPreference(true), DocumentPreference(std::string("true")));
}

TEST(DocumentPreferences, MaterialsAreUniqueAndKeepInsertionOrder)
{
    DocumentPreferences prefs;
    const std::string   cardboard = "BULK|Cardboard|680.0|0.0|Paper";
    const std::string   nylon     = "SURFACE|Ripstop nylon|0.067|0.0|Fabric";
    const std::string   cord      = "LINE|Elastic cord (round 2 mm, 1/16 in)|0.0018|0.0|Cord";

    EXPECT_TRUE(prefs.addMaterial(nylon));
    EXPECT_TRUE(prefs.addMaterial(cardboard));
    EXPECT_FALSE(prefs.addMaterial(nylon));  // Database.add: already there
    EXPECT_TRUE(prefs.addMaterial(cord));
    EXPECT_EQ(prefs.materialCount(), 3U);
    EXPECT_EQ(prefs.materials(), (std::vector<std::string>{nylon, cardboard, cord}));

    EXPECT_TRUE(prefs.removeMaterial(cardboard));
    EXPECT_FALSE(prefs.removeMaterial(cardboard));
    EXPECT_FALSE(prefs.removeMaterial("BULK|Unknown|1.0|0.0|Other"));
    EXPECT_EQ(prefs.materials(), (std::vector<std::string>{nylon, cord}));
    EXPECT_EQ(prefs.materialCount(), 2U);
}

TEST(DocumentPreferences, MaterialsDoNotEmitChanged)
{
    DocumentPreferences prefs;
    const ChangeCounter counter(prefs.changed());
    prefs.addMaterial("BULK|Cardboard|680.0|0.0|Paper");
    prefs.removeMaterial("BULK|Cardboard|680.0|0.0|Paper");
    EXPECT_EQ(counter.count(), 0);
}

TEST(DocumentPreferences, CopyHoldsTheDataAndNoConnections)
{
    DocumentPreferences prefs;
    const ChangeCounter counter(prefs.changed());
    prefs.putInt("i", 1);
    prefs.putColor("c", Color(1, 2, 3));
    prefs.addMaterial("BULK|Cardboard|680.0|0.0|Paper");

    DocumentPreferences copy(prefs);
    EXPECT_EQ(copy.size(), 2U);
    EXPECT_EQ(copy.getInt("i", 0), 1);
    EXPECT_EQ(copy.getColor("c"), Color(1, 2, 3));
    EXPECT_EQ(copy.materials(), prefs.materials());
    EXPECT_TRUE(copy.changed().empty());

    copy.putInt("i", 2);  // the original is untouched, its listeners not called
    EXPECT_EQ(prefs.getInt("i", 0), 1);
    EXPECT_EQ(counter.count(), 2);

    DocumentPreferences assigned;
    const ChangeCounter assignedCounter(assigned.changed());
    assigned = copy;
    EXPECT_EQ(assigned.getInt("i", 0), 2);
    EXPECT_EQ(assigned.materialCount(), 1U);
    EXPECT_EQ(assigned.changed().size(), 1U);  // assignment keeps the connections
    assigned.putInt("i", 3);
    EXPECT_EQ(assignedCounter.count(), 1);
}

TEST(DocumentPreferences, MoveTakesTheConnectionsAlong)
{
    DocumentPreferences prefs;
    const ChangeCounter counter(prefs.changed());
    prefs.putInt("i", 1);

    DocumentPreferences moved(std::move(prefs));
    EXPECT_EQ(moved.getInt("i", 0), 1);
    EXPECT_EQ(moved.changed().size(), 1U);
    moved.putInt("i", 2);
    EXPECT_EQ(counter.count(), 2);
}

}  // namespace
