#pragma once

// Helpers of the tests of the inner tube's and the ring components' configuration panels
// (InnerTubeConfig, RingComponentConfig and its subclasses, ClusterSelectionPanel).

#include <memory>
#include <span>

#include <QStringList>
#include <QWidget>

#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/gui/configdialog/RingComponentConfig.h"
// The components' own headers rather than declarations: a test hands them to functions that take
// a RocketComponent&, which needs the complete class.
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/Bulkhead.h"
#include "QtRocket/rocket/CenteringRing.h"
#include "QtRocket/rocket/EngineBlock.h"
#include "QtRocket/rocket/InnerTube.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/TubeCoupler.h"

namespace QtRocket::Gui
{
class ComponentConfigDialog;
class GuiServices;
class RocketComponentConfig;
}  // namespace QtRocket::Gui

namespace QtRocket::Gui::Test
{

/// The design of part D2a's Java probe (D2aProbe.java, run on OpenRocket 5f164fd0e: its output
/// is what the tests pin): a new rocket (OpenRocketDocumentFactory::createNewRocket()) whose
/// stage holds a body tube with, in this order, an inner tube (holding an engine block), a tube
/// coupler, a centering ring and a bulkhead, each made by its constructor (Java's `new X()`), in
/// a document whose undo history starts with the whole design (clearUndo()). The pointers stay
/// valid until an undo or a redo replaces the components.
struct RingDesign
{
    std::unique_ptr<OpenRocketDocument> document;
    BodyTube*                           body{nullptr};
    InnerTube*                          inner{nullptr};
    TubeCoupler*                        coupler{nullptr};
    EngineBlock*                        block{nullptr};
    CenteringRing*                      centering{nullptr};
    Bulkhead*                           bulkhead{nullptr};

    RingDesign();
};

/// A configuration panel that ConfigPanelFactory::create() made for a selection of @p main and
/// @p others, with the widget that owns it.
class FactoryPanel
{
public:
    FactoryPanel(OpenRocketDocument& document, const GuiServices& services, RocketComponent& main,
                 std::span<RocketComponent* const> others = {});

    [[nodiscard]] RocketComponentConfig& panel() const noexcept { return *m_panel; }

    /// The panel as a Panel; null when it is another class.
    template <class Panel>
    [[nodiscard]] Panel* as() const
    {
        return dynamic_cast<Panel*>(m_panel);
    }

private:
    std::unique_ptr<QWidget> m_host = std::make_unique<QWidget>();
    RocketComponentConfig*   m_panel{nullptr};
};

/// The texts of the row labels of @p tab, in Java's order (length, outer, inner and thickness),
/// an empty text for a row it has not.
[[nodiscard]] QStringList rowLabels(const RingComponentConfig::GeneralTab& tab);

/// Opens the configuration dialog of @p main (and @p others), a component of @p document, on
/// @p window and waits until it is active (its fields then take typed text); null, with a test
/// failure, when it did not open.
[[nodiscard]] ComponentConfigDialog* showActiveDialog(
    QWidget& window, const GuiServices& services, OpenRocketDocument& document,
    RocketComponent& main, std::span<RocketComponent* const> others = {});

}  // namespace QtRocket::Gui::Test
