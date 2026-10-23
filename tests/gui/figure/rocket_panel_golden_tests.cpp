// The design view's mouse and rotation against OpenRocket's own RocketPanel and
// ViewRotationControl (RocketPanelProbe, OpenRocket 5f164fd0e, made headless with SwingPreferences
// and FlatLaf Light, run under Xvfb so that Ctrl is the menu shortcut key): the snapping of
// ViewRotationControl.snapRotation(), the rotation of a left drag (handleMouseDragged()), the
// components under the probe's points of "A simple model rocket", and its transcript of clicks
// (handleMouseClick() with the click counts of a double-click) replayed as mouse events on the
// figure, with the tree's selection, the dialog asked for and the popup after each. Where QtRocket
// differs on purpose (the right-click of decision U4, Qt's double-click), the pins say so. The
// pins are RocketPanelPins.h, generated from the probe's output.

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <QApplication>
#include <QObject>
#include <QPoint>
#include <QPointF>
#include <QTransform>
#include <Qt>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/file/LoadedDocument.h"
#include "QtRocket/gui/figure/RocketFigure.h"
#include "QtRocket/gui/figure/RocketPanel.h"
#include "QtRocket/gui/figure/ViewRotationControl.h"
#include "QtRocket/gui/tree/ComponentTreeModel.h"
#include "QtRocket/gui/tree/ComponentTreeView.h"
#include "QtRocket/gui/tree/DocumentSelectionModel.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/Error.h"
#include "figure/FigureMouse.h"
#include "figure/RocketPanelPins.h"
#include "file/DesignFileEnvironment.h"
#include "goldens/GoldenExamples.h"

namespace QtRocket::Gui
{

namespace
{

using RocketPanelGolden = Gui::Test::GuiTest;
using Gui::Test::RocketPanelPointPin;
using Gui::Test::RocketPanelStepPin;

/// The probe's name of @p component: its name, and its number among the components of that name
/// in tree order when there are several ("Centering ring 2").
[[nodiscard]] std::string probeName(const Rocket& rocket, const RocketComponent& component)
{
    int count = 0;
    int index = 0;
    for (const RocketComponent* other : rocket.getAllChildren())
    {
        if (other->getName() == component.getName())
        {
            ++count;
            if (other == &component)
            {
                index = count;
            }
        }
    }
    return count > 1 ? component.getName() + " " + std::to_string(index) : component.getName();
}

/// "[A, B]" of @p components' probe names.
[[nodiscard]] std::string probeNames(const Rocket&                        rocket,
                                     const std::vector<RocketComponent*>& components)
{
    std::string text = "[";
    for (const RocketComponent* component : components)
    {
        text += (text.size() > 1 ? ", " : "") + probeName(rocket, *component);
    }
    return text + "]";
}

/// The mouse button of the probe's name @p name (left, middle, right).
[[nodiscard]] Qt::MouseButton buttonOf(std::string_view name)
{
    if (name == "left")
    {
        return Qt::LeftButton;
    }
    if (name == "middle")
    {
        return Qt::MiddleButton;
    }
    return Qt::RightButton;
}

/// "A simple model rocket" as the probe loads it (no component presets, settled), its component
/// tree and selection, and its design view, 1000 x 600 pixels and shown, the dialog's and the
/// popup's requests recorded as the probe prints them.
class SimpleRocketPanel
{
public:
    SimpleRocketPanel()
    {
        const QtRocket::Test::GoldenExample* example =
            QtRocket::Test::findGoldenExample("A simple model rocket.ork");
        if (example == nullptr)
        {
            ADD_FAILURE() << "no example";
            return;
        }
        Result<LoadedDocument> loaded = QtRocket::Test::loadSettledGoldenExample(
            *example, QtRocket::Test::ExampleSource::ORIGINAL,
            QtRocket::Test::DesignFileEnvironment::Presets::NONE);
        if (!loaded.has_value())
        {
            ADD_FAILURE() << loaded.error().message;
            return;
        }
        m_document  = std::move(loaded->document);
        m_model     = std::make_unique<ComponentTreeModel>(*m_document, m_preferences);
        m_view      = std::make_unique<ComponentTreeView>(*m_model);
        m_selection = makeChild<DocumentSelectionModel>(m_view.get());
        m_panel     = std::make_unique<RocketPanel>(*m_document, m_preferences, nullptr);
        m_panel->setSelectionModel(m_selection);
        QObject::connect(
            m_panel.get(), &RocketPanel::editRequested, m_panel.get(),
            [this](RocketComponent* component, const std::vector<RocketComponent*>& others) {
                m_dialog = "dialog " + probeName(rocket(), *component) +
                           " others=" + probeNames(rocket(), others);
            });
        QObject::connect(m_panel.get(), &RocketPanel::componentPopupRequested, m_panel.get(),
                         [this] { m_popup = true; });
        m_panel->resize(1000, 600);
        m_panel->show();
        constexpr int kPasses = 4;
        for (int pass = 0; pass < kPasses; ++pass)
        {
            QApplication::processEvents();
        }
    }

    ~SimpleRocketPanel()
    {
        // The panel and the tree before the document.
        m_panel = nullptr;
        m_view  = nullptr;
        m_model = nullptr;
    }
    SimpleRocketPanel(const SimpleRocketPanel&)            = delete;
    SimpleRocketPanel& operator=(const SimpleRocketPanel&) = delete;
    SimpleRocketPanel(SimpleRocketPanel&&)                 = delete;
    SimpleRocketPanel& operator=(SimpleRocketPanel&&)      = delete;

    [[nodiscard]] bool          isLoaded() const noexcept { return m_panel != nullptr; }
    [[nodiscard]] Rocket&       rocket() const { return m_document->getRocket(); }
    [[nodiscard]] RocketFigure& figure() const { return m_panel->figure(); }

    /// The pixel of the figure at the model point of the probe's point @p name.
    [[nodiscard]] QPoint pixel(std::string_view name) const
    {
        for (const RocketPanelPointPin& point : Gui::Test::kRocketPanelPoints)
        {
            if (point.name == name)
            {
                return figure().projection().map(QPointF(point.x, point.y)).toPoint();
            }
        }
        ADD_FAILURE() << "no point " << name;
        return {};
    }

    /// The selection's probe names, "[A, B]".
    [[nodiscard]] std::string selection() const
    {
        return probeNames(rocket(), m_selection->selectedComponents());
    }

    /// Selects the components of the probe names @p names ("A, B"), in that order.
    void select(std::string_view names) const
    {
        std::vector<RocketComponent*> components;
        for (std::string_view rest = names; !rest.empty();)
        {
            const std::size_t      comma = rest.find(", ");
            const std::string_view name  = rest.substr(0, comma);
            rest = comma == std::string_view::npos ? std::string_view() : rest.substr(comma + 2);
            std::ranges::copy_if(rocket().getAllChildren(), std::back_inserter(components),
                                 [this, name](const RocketComponent* component) {
                                     return probeName(rocket(), *component) == name;
                                 });
        }
        m_selection->setSelectedComponents(components);
    }

    /// The click of @p step on the figure; the dialog asked for ("-" for none) and whether the
    /// popup was asked for.
    [[nodiscard]] std::pair<std::string, bool> click(const RocketPanelStepPin& step)
    {
        m_dialog                     = "-";
        m_popup                      = false;
        const Qt::MouseButton button = buttonOf(step.button);
        Qt::KeyboardModifiers modifiers;
        modifiers.setFlag(Qt::ShiftModifier, step.modifiers.contains("shift"));
        modifiers.setFlag(Qt::ControlModifier, step.modifiers.contains("ctrl"));
        if (step.count >= 2)
        {
            Gui::Test::secondClickAt(figure(), pixel(step.point), button, modifiers);
        }
        else
        {
            Gui::Test::clickAt(figure(), pixel(step.point), button, modifiers);
        }
        return {m_dialog, m_popup};
    }

private:
    InMemoryPreferences                 m_preferences;
    std::unique_ptr<OpenRocketDocument> m_document;
    std::unique_ptr<ComponentTreeModel> m_model;
    std::unique_ptr<ComponentTreeView>  m_view;
    DocumentSelectionModel*             m_selection = nullptr;
    std::unique_ptr<RocketPanel>        m_panel;
    std::string                         m_dialog = "-";
    bool                                m_popup  = false;
};

TEST(RocketPanelPinsGolden, ComeFromTheGoldensOpenRocket)
{
    EXPECT_EQ(Gui::Test::kRocketPanelPinsCommit, "5f164fd0e05d6e188857b0185601293b7a6e9eaf");
}

TEST_F(RocketPanelGolden, SnapsRotationsAsOpenRocket)
{
    for (const Gui::Test::SnapRotationPin& pin : Gui::Test::kSnapRotationPins)
    {
        EXPECT_EQ(ViewRotationControl::snapRotation(pin.angle), pin.snapped) << pin.angle;
    }
}

TEST_F(RocketPanelGolden, RotatesByADragAsOpenRocket)
{
    for (const Gui::Test::DragRotationPin& pin : Gui::Test::kDragRotationPins)
    {
        EXPECT_EQ(RocketPanel::dragRotation(pin.original, pin.dy, pin.height, pin.shift),
                  pin.rotation)
            << pin.original << ' ' << pin.dy << ' ' << pin.height << ' ' << pin.shift;
    }
}

TEST_F(RocketPanelGolden, FindsTheComponentsAtTheProbesPoints)
{
    const SimpleRocketPanel shown;
    ASSERT_TRUE(shown.isLoaded());
    for (const RocketPanelPointPin& point : Gui::Test::kRocketPanelPoints)
    {
        EXPECT_EQ(probeNames(shown.rocket(),
                             shown.figure().componentsAt(QPointF(shown.pixel(point.name)))),
                  point.components)
            << point.name;
    }
}

/// Replays @p step on @p shown and checks what follows it.
void replay(SimpleRocketPanel& shown, const RocketPanelStepPin& step)
{
    if (step.select)
    {
        shown.select(step.names);
        EXPECT_EQ(shown.selection(), step.selection) << step.label;
        return;
    }
    const auto [dialog, popup] = shown.click(step);
    EXPECT_EQ(shown.selection(), step.selection) << step.label;
    EXPECT_EQ(dialog, step.dialog) << step.label;
    EXPECT_EQ(popup, step.popup) << step.label;
}

TEST_F(RocketPanelGolden, ClicksAsOpenRocket)
{
    SimpleRocketPanel shown;
    ASSERT_TRUE(shown.isLoaded());
    for (const RocketPanelStepPin& step : Gui::Test::kRocketPanelSteps)
    {
        replay(shown, step);
    }
}

}  // namespace

}  // namespace QtRocket::Gui
