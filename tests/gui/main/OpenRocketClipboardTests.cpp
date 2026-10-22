#include "QtRocket/gui/main/OpenRocketClipboard.h"

#include <string_view>
#include <utility>
#include <vector>

#include <QSignalSpy>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/document/ComponentCopy.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/Error.h"
#include "tree/ProbeDesign.h"

namespace QtRocket::Gui
{

namespace
{

using Test::ProbeDesign;
using OpenRocketClipboardTest = Test::GuiTest;

/// A copy of the component @p name of @p design.
[[nodiscard]] ComponentCopy copyOf(const ProbeDesign& design, std::string_view name)
{
    Result<ComponentCopy> copy = ComponentCopy::copyOf(
        design.document(), std::vector<const RocketComponent*>{&design.find(name)});
    EXPECT_TRUE(copy.has_value());
    return copy.has_value() ? std::move(*copy) : ComponentCopy();
}

TEST_F(OpenRocketClipboardTest, StartsEmpty)
{
    const OpenRocketClipboard clipboard;

    EXPECT_TRUE(clipboard.isEmpty());
    EXPECT_TRUE(clipboard.components().empty());
}

TEST_F(OpenRocketClipboardTest, HoldsWhatItIsGivenAndTellsSo)
{
    const ProbeDesign   design;
    OpenRocketClipboard clipboard;
    const QSignalSpy    changed(&clipboard, &OpenRocketClipboard::changed);

    clipboard.setComponents(copyOf(design, "BT1"));

    EXPECT_EQ(changed.count(), 1);
    EXPECT_FALSE(clipboard.isEmpty());
    EXPECT_EQ(design.clipboard(clipboard.components()), "[BT1*[IT*[EB*], CR1*, Fins*]]");

    // Every setting is told, as Java's setClipboard() tells its listeners every time.
    clipboard.setComponents(copyOf(design, "NC"));
    EXPECT_EQ(changed.count(), 2);
    EXPECT_EQ(design.clipboard(clipboard.components()), "[NC*]");

    clipboard.clear();
    EXPECT_EQ(changed.count(), 3);
    EXPECT_TRUE(clipboard.isEmpty());
}

TEST_F(OpenRocketClipboardTest, KeepsItsCopiesWhenTheDocumentGoes)
{
    OpenRocketClipboard clipboard;
    {
        const ProbeDesign design;
        clipboard.setComponents(copyOf(design, "BT2"));
    }

    ASSERT_EQ(clipboard.components().size(), 1U);
    EXPECT_EQ(clipboard.components().getComponent(0).getName(), "BT2");
    EXPECT_EQ(clipboard.components().getComponent(0).getChild(0).getName(), "Chute");
}

TEST_F(OpenRocketClipboardTest, TheApplicationHasOne)
{
    OpenRocketClipboard& application = OpenRocketClipboard::instance();

    EXPECT_EQ(&OpenRocketClipboard::instance(), &application);
    // Process-wide: no test may leave anything on it.
    EXPECT_TRUE(application.isEmpty());
    const ProbeDesign design;
    application.setComponents(copyOf(design, "NC"));
    EXPECT_FALSE(OpenRocketClipboard::instance().isEmpty());
    application.clear();
    EXPECT_TRUE(OpenRocketClipboard::instance().isEmpty());
}

}  // namespace

}  // namespace QtRocket::Gui
