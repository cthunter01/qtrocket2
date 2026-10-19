#pragma once

// Helpers of the tests of the configuration dialog and its panels (tests/gui/configdialog).

#include <array>
#include <memory>
#include <span>
#include <string>
#include <utility>

#include <QString>
#include <QStringList>
#include <QVBoxLayout>
#include <QWidget>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/widgets/BindingScope.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "TestGuiServices.h"

namespace QtRocket
{
class AxialStage;
class BodyTube;
class RocketComponent;
}  // namespace QtRocket

namespace QtRocket::Gui
{
class ComponentSelection;
class RocketComponentConfig;
}  // namespace QtRocket::Gui

namespace QtRocket::Gui::Test
{

/// A design with a component of every kind (ComponentKind), each as the add buttons make it
/// with the built-in materials (ComponentFactory::createWithBuiltinMaterials()), in a document
/// whose undo history starts with the whole design (clearUndo()):
///     Rocket > Sustainer > nose cone, body tube "Body", transition;
///     Body > trapezoidal, elliptical, freeform and tube fin sets, launch lug, rail button,
///            inner tube (> engine block), tube coupler, centering ring, bulkhead, mass
///            component, shock cord, parachute, streamer, pod set (> body tube), booster set
///            (> body tube);
///     Rocket > a second stage "Booster" > body tube "Booster tube".
/// component(kind) is the first component of a kind in that order; the pointers stay valid until
/// an undo or a redo replaces the components (find them again by id then).
struct EveryKindDesign
{
    std::unique_ptr<OpenRocketDocument>                     document;
    std::array<RocketComponent*, kAllComponentKinds.size()> byKind{};
    AxialStage*                                             booster{nullptr};
    BodyTube*                                               boosterTube{nullptr};

    EveryKindDesign();

    [[nodiscard]] RocketComponent& component(ComponentKind kind) const;
};

/// TestGuiServices with OpenRocket's built-in materials in its application materials, as the
/// application has them.
class ConfigDialogServices : public TestGuiServices
{
public:
    ConfigDialogServices();
};

/// The fixture of the tests that open the configuration dialog: GuiTest, and what the closed
/// dialogs leave for the next one (the tab to select) forgotten before and after each test
/// (ComponentConfigDialog::forgetPreviousDialogs()).
class ConfigDialogTest : public GuiTest
{
protected:
    void SetUp() override;
    void TearDown() override;
};

/// The titles of the tabs of @p panel, in order.
[[nodiscard]] QStringList tabTitles(const RocketComponentConfig& panel);

/// A panel's root widget with the BindingScope of a selection, for the tests of the shared
/// panels: the scope's root is the widget, and the selection is made of @p main and @p others.
class PanelHost
{
public:
    explicit PanelHost(RocketComponent& main, std::span<RocketComponent* const> others = {});
    ~PanelHost();
    PanelHost(const PanelHost&)            = delete;
    PanelHost& operator=(const PanelHost&) = delete;
    PanelHost(PanelHost&&)                 = delete;
    PanelHost& operator=(PanelHost&&)      = delete;

    [[nodiscard]] QWidget&                                   root() noexcept { return m_root; }
    [[nodiscard]] BindingScope&                              scope() noexcept { return m_scope; }
    [[nodiscard]] const std::shared_ptr<ComponentSelection>& selection() const noexcept
    {
        return m_scope.selection();
    }

    /// Makes a Panel (a shared panel: its constructor takes the scope first, then @p args and
    /// the parent) as a child of the root, in the root's layout.
    template <class Panel, class... Args>
    Panel& make(Args&&... args)
    {
        auto* panel = makeChild<Panel>(&m_root, m_scope, std::forward<Args>(args)...);
        m_layout->addWidget(panel);
        return *panel;
    }

private:
    QWidget      m_root;
    QVBoxLayout* m_layout;
    BindingScope m_scope;
};

/// The child of @p root named @p name of the type W; null when there is none.
template <class W>
[[nodiscard]] W* childNamed(const QWidget& root, const QString& name)
{
    return root.findChild<W*>(name);
}

}  // namespace QtRocket::Gui::Test
