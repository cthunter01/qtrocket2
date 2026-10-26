// The buttons that add components (ComponentAddButtons, ComponentButton), compared with OpenRocket:
// the M2 component-tree scout's AddProbe (evidence/AddProbe.out: the enabled buttons for every kind
// selected, the positions for the insert preferences 1 and 2) and T4Probe (Milestone 2 run 2, part
// T4: the positions for every preference, out-of-range ones included, on the probes' design with a
// booster set; "asks" where OpenRocket's question would come), both OpenRocket 5f164fd0e run
// headless with JDK 17; and the design of 25 clicks, which saves as OpenRocket saves it (C1's
// reference, tests/data/component-factory).

#include "QtRocket/gui/main/ComponentAddButtons.h"

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <QCoreApplication>
#include <QFont>
#include <QIcon>
#include <QImage>
#include <QLayout>
#include <QObject>
#include <QSize>
#include <QString>
#include <QStringList>
#include <QToolButton>
#include <QWidget>
#include <Qt>
#include <QtContainerFwd>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/OpenRocketDocumentFactory.h"
#include "QtRocket/file/GeneralRocketSaver.h"
#include "QtRocket/file/openrocket/ComponentHandler.h"
#include "QtRocket/file/openrocket/DocumentConfig.h"
#include "QtRocket/gui/main/ComponentButton.h"
#include "QtRocket/gui/tree/ComponentLimits.h"
#include "QtRocket/gui/tree/ComponentTreeModel.h"
#include "QtRocket/gui/tree/ComponentTreeView.h"
#include "QtRocket/gui/tree/DocumentSelectionModel.h"
#include "QtRocket/gui/util/ComponentIcons.h"
#include "QtRocket/gui/util/GuiServices.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/util/ThemeColors.h"
#include "QtRocket/gui/util/UserPrompts.h"
#include "QtRocket/gui/widgets/FlowLayout.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/material/BuiltinMaterials.h"
#include "QtRocket/material/MaterialPreferences.h"
#include "QtRocket/material/MaterialStorage.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/preferences/PreferenceKeys.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/ComponentFactory.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/InnerTube.h"
#include "QtRocket/rocket/PodSet.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/Error.h"
#include "ScopedApplicationPalette.h"
#include "ScriptedUserPrompts.h"
#include "TestGuiServices.h"
#include "file/openrocket/AddButtonsDesign.h"
#include "rocket/ComponentFactoryReport.h"
#include "tree/ProbeDesign.h"
#include "tree/TreeEditingSupport.h"

namespace QtRocket::Gui
{

namespace
{

using Lines     = std::vector<std::string>;
using Names     = std::vector<std::string_view>;
using Placement = ComponentButton::Placement;
using Position  = ComponentButton::Position;
using Answer    = UserPrompts::InsertPositionAnswer;
using Test::fillAssembliesUpTo;
using Test::fillUpTo;
using Test::linesOf;
using Test::nestInnerTubes;
using Test::ProbeDesign;
using Test::ScriptedUserPrompts;
using Test::undoText;

using ComponentAddButtonsTest = Gui::Test::GuiTest;

/// The application's services with OpenRocket's materials (a TestGuiServices has none): the
/// built-in materials and the default component materials of OpenRocket's start-up, both insert
/// preferences "add to the end" (2) unless a test sets them.
class AddServices
{
public:
    AddServices()
      : m_services(m_test.preferences(), m_materials, m_test.services().motors(),
                   m_test.services().presets(), m_test.services().loadingContext(),
                   m_test.prompts())
    {
        addBuiltinMaterials(m_materials);
        loadDefaultComponentMaterials(m_test.preferences(), m_materials);
        setInsertPositions(2, 2);
    }

    /// Stores the insert preferences of body components and of stages.
    void setInsertPositions(int body, int stage)
    {
        m_test.preferences().putInt(PreferenceKeys::kBodyComponentInsertPositionKey, body);
        m_test.preferences().putInt(PreferenceKeys::kStageInsertPositionKey, stage);
    }

    [[nodiscard]] const GuiServices&   services() const noexcept { return m_services; }
    [[nodiscard]] InMemoryPreferences& preferences() noexcept { return m_test.preferences(); }
    [[nodiscard]] ScriptedUserPrompts& prompts() noexcept { return m_test.prompts(); }

private:
    Gui::Test::TestGuiServices m_test;
    MaterialStorage            m_materials;
    GuiServices                m_services;
};

/// @p component and every component below it, in pre-order.
[[nodiscard]] std::vector<RocketComponent*> preOrder(RocketComponent& component)
{
    std::vector<RocketComponent*> components{&component};
    for (RocketComponent* child : component.getChildren())
    {
        const std::vector<RocketComponent*> below = preOrder(*child);
        components.insert(components.end(), below.begin(), below.end());
    }
    return components;
}

/// The first component of @p rocket in pre-order named @p name; a failure and the Rocket when
/// there is none.
[[nodiscard]] RocketComponent& named(Rocket& rocket, std::string_view name)
{
    for (RocketComponent* component : preOrder(rocket))
    {
        if (component->getName() == name)
        {
            return *component;
        }
    }
    ADD_FAILURE() << "no component named " << name;
    return rocket;
}

/// The add buttons of a design window: the tree of a document with its selection and the panel,
/// with the requests for the configuration dialog recorded.
class AddWindow
{
public:
    AddWindow(OpenRocketDocument& document, const AddServices& services)
      : m_document(&document),
        m_model(document, m_preferences),
        m_view(std::make_unique<ComponentTreeView>(m_model)),
        m_selection(makeChild<DocumentSelectionModel>(m_view.get())),
        m_panel(std::make_unique<ComponentAddButtons>(*m_selection, services.services()))
    {
        QObject::connect(
            m_panel.get(), &ComponentAddButtons::addedComponentEditRequested, m_panel.get(),
            [this](RocketComponent* component) { m_dialogs.push_back(component->getName()); });
    }

    /// Selects the components @p names, in this order.
    void select(const Names& names) const
    {
        std::vector<RocketComponent*> components;
        components.reserve(names.size());
        for (const std::string_view name : names)
        {
            components.push_back(&named(m_document->getRocket(), name));
        }
        m_selection->setSelectedComponents(components);
    }

    /// The button of @p kind.
    [[nodiscard]] ComponentButton& button(ComponentKind kind) const
    {
        ComponentButton* button = m_panel->button(kind);
        if (button == nullptr)
        {
            ADD_FAILURE() << "no button for " << className(kind);
            return *m_panel->buttons().front();
        }
        return *button;
    }

    /// The enabled buttons' Java class names, each after a space (AddProbe's lines).
    [[nodiscard]] std::string enabledButtons() const
    {
        std::string text;
        for (const ComponentButton* button : m_panel->buttons())
        {
            if (button->isEnabled())
            {
                text += " " + QtStrings::toUtf8(button->objectName());
            }
        }
        return text;
    }

    [[nodiscard]] OpenRocketDocument&     document() const noexcept { return *m_document; }
    [[nodiscard]] Rocket&                 rocket() const { return m_document->getRocket(); }
    [[nodiscard]] DocumentSelectionModel& selection() const noexcept { return *m_selection; }
    [[nodiscard]] ComponentAddButtons&    panel() const noexcept { return *m_panel; }
    [[nodiscard]] const Lines&            dialogs() const noexcept { return m_dialogs; }

    /// Destroys the tree view and with it the selection.
    void destroyView() { m_view = nullptr; }

private:
    OpenRocketDocument*                  m_document;
    InMemoryPreferences                  m_preferences;
    ComponentTreeModel                   m_model;
    std::unique_ptr<ComponentTreeView>   m_view;
    DocumentSelectionModel*              m_selection;
    std::unique_ptr<ComponentAddButtons> m_panel;
    Lines                                m_dialogs;
};

// ============================================================================== the panel

/// "[Name, Name]" of the buttons' Java class names (AddProbe's "buttons:" line).
[[nodiscard]] std::string buttonNames(const ComponentAddButtons& panel)
{
    std::string text = "[";
    for (const ComponentButton* button : panel.buttons())
    {
        text += (text.size() > 1 ? ", " : "") + QtStrings::toUtf8(button->objectName());
    }
    return text + "]";
}

/// The buttons' texts, in order.
[[nodiscard]] Lines buttonTexts(const ComponentAddButtons& panel)
{
    Lines texts;
    for (const ComponentButton* button : panel.buttons())
    {
        texts.push_back(QtStrings::toUtf8(button->text()));
    }
    return texts;
}

/// The buttons' kinds and placements, as "<class> <placement>" for those not INTO_SELECTED.
[[nodiscard]] Lines specialPlacements(const ComponentAddButtons& panel)
{
    Lines placements;
    for (const ComponentButton* button : panel.buttons())
    {
        if (button->placement() == Placement::BODY_COMPONENT)
        {
            placements.push_back(std::string(className(button->kind())) + " body");
        }
        else if (button->placement() == Placement::STAGE)
        {
            placements.push_back(std::string(className(button->kind())) + " stage");
        }
    }
    return placements;
}

TEST_F(ComponentAddButtonsTest, HasOpenRocketsGroupsButtonsAndTexts)
{
    const AddServices          services;
    const ProbeDesign          design;
    const AddWindow            window(design.document(), services);
    const ComponentAddButtons& panel = window.panel();
    EXPECT_EQ(panel.groupTitles(),
              (QStringList{QStringLiteral("Assembly Components"),
                           QStringLiteral("Body Components and Fin Sets"),
                           QStringLiteral("Inner Components"), QStringLiteral("Mass Components")}));
    EXPECT_EQ(buttonNames(panel),
              "[AxialStage, ParallelStage, PodSet, NoseCone, BodyTube, Transition, "
              "TrapezoidFinSet, EllipticalFinSet, FreeformFinSet, TubeFinSet, RailButton, "
              "LaunchLug, InnerTube, TubeCoupler, CenteringRing, Bulkhead, EngineBlock, Parachute, "
              "Streamer, ShockCord, MassComponent]");
    EXPECT_EQ(
        buttonTexts(panel),
        (Lines{"Stage",          "Boosters",      "Pods",       "Nose Cone",    "Body Tube",
               "Transition",     "Trapezoidal",   "Elliptical", "Freeform",     "Tube Fins",
               "Rail Button",    "Launch Lug",    "Inner Tube", "Tube Coupler", "Centering\nRing",
               "Bulkhead",       "Engine\nBlock", "Parachute",  "Streamer",     "Shock Cord",
               "Mass\nComponent"}));
    EXPECT_EQ(specialPlacements(panel),
              (Lines{"AxialStage stage", "NoseCone body", "BodyTube body", "Transition body"}));
    EXPECT_EQ(ComponentAddButtons::title(), QStringLiteral("Add new component"));
    EXPECT_EQ(panel.button(ComponentKind::ROCKET), nullptr);
    EXPECT_EQ(panel.button(ComponentKind::STREAMER)->kind(), ComponentKind::STREAMER);
}

/// What of the buttons' sizes and layouts differs from the panel's rules, one text each.
[[nodiscard]] Lines layoutDifferences(const ComponentAddButtons& panel)
{
    Lines found;
    QSize largest;
    for (const ComponentButton* button : panel.buttons())
    {
        largest = largest.expandedTo(button->sizeHint());
    }
    if (panel.buttonSize() != largest)
    {
        found.emplace_back("the button size is not the largest size hint");
    }
    for (const ComponentButton* button : panel.buttons())
    {
        const std::string name = QtStrings::toUtf8(button->objectName());
        if (button->minimumSize() != largest || button->maximumSize() != largest)
        {
            found.push_back(name + " size");
        }
        if (button->toolButtonStyle() != Qt::ToolButtonTextUnderIcon)
        {
            found.push_back(name + " style");
        }
        const auto* flow = qobject_cast<const FlowLayout*>(button->parentWidget()->layout());
        if (flow == nullptr || flow->horizontalSpacing() != ComponentAddButtons::kButtonGap ||
            flow->verticalSpacing() != ComponentAddButtons::kButtonGap)
        {
            found.push_back(name + " flow layout");
        }
    }
    return found;
}

TEST_F(ComponentAddButtonsTest, EveryButtonHasTheSizeOfTheLargestInFlowLayoutsWithNinePixelGaps)
{
    const AddServices services;
    const ProbeDesign design;
    const AddWindow   window(design.document(), services);
    EXPECT_EQ(layoutDifferences(window.panel()), Lines{});
    EXPECT_EQ(ComponentAddButtons::kButtonGap, 9);
    // Four groups, each in a row of its own.
    const QWidget* first = window.panel().button(ComponentKind::AXIAL_STAGE)->parentWidget();
    EXPECT_EQ(window.panel().button(ComponentKind::POD_SET)->parentWidget(), first);
    EXPECT_NE(window.panel().button(ComponentKind::NOSE_CONE)->parentWidget(), first);
}

TEST_F(ComponentAddButtonsTest, TheButtonsAreMadeTheSameSizeAgainForANewFont)
{
    const AddServices services;
    const ProbeDesign design;
    const AddWindow   window(design.document(), services);
    const QSize       before = window.panel().buttonSize();
    QFont             font   = window.panel().font();
    font.setPointSizeF(font.pointSizeF() * 2);
    window.panel().setFont(font);
    EXPECT_GT(window.panel().buttonSize().width(), before.width());
    EXPECT_EQ(layoutDifferences(window.panel()), Lines{});
}

/// @p icon in @p mode drawn at @p size, at the device pixel ratio 1.
[[nodiscard]] QImage drawn(const QIcon& icon, QSize size, QIcon::Mode mode = QIcon::Normal)
{
    return icon.pixmap(size, 1.0, mode).toImage();
}

/// What of the buttons' icons differs from ComponentIcons' large ones in @p theme, one text each.
[[nodiscard]] Lines iconDifferences(const ComponentAddButtons& panel, ThemeColors::Theme theme)
{
    Lines found;
    for (const ComponentButton* button : panel.buttons())
    {
        const std::string name    = QtStrings::toUtf8(button->objectName());
        const QIcon       large   = ComponentIcons::largeIcon(button->kind(), theme);
        const QSize       natural = large.availableSizes().value(0);
        if (button->iconSize() != natural)
        {
            found.push_back(name + " icon size");
        }
        if (drawn(button->icon(), natural) != drawn(large, natural))
        {
            found.push_back(name + " icon");
        }
        if (drawn(button->icon(), natural, QIcon::Disabled) !=
            drawn(ComponentIcons::largeDisabledIcon(button->kind(), theme), natural))
        {
            found.push_back(name + " disabled icon");
        }
    }
    return found;
}

TEST_F(ComponentAddButtonsTest, IconsAreTheLargeIconsAndTheCheckerboardOnesWhenDisabled)
{
    const Gui::Test::ScopedApplicationPalette palette(Gui::Test::ScopedApplicationPalette::light());
    const AddServices                         services;
    const ProbeDesign                         design;
    const AddWindow                           window(design.document(), services);
    EXPECT_EQ(iconDifferences(window.panel(), ThemeColors::Theme::LIGHT), Lines{});
    EXPECT_EQ(window.panel().button(ComponentKind::BODY_TUBE)->iconSize(), QSize(50, 40));
    EXPECT_EQ(window.panel().button(ComponentKind::CENTERING_RING)->iconSize(), QSize(50, 30));
}

TEST_F(ComponentAddButtonsTest, IconsFollowThePalette)
{
    const Gui::Test::ScopedApplicationPalette light(Gui::Test::ScopedApplicationPalette::light());
    const AddServices                         services;
    const ProbeDesign                         design;
    const AddWindow                           window(design.document(), services);
    {
        const Gui::Test::ScopedApplicationPalette dark(Gui::Test::ScopedApplicationPalette::dark());
        // Qt hands a new application palette to the widgets through posted events.
        QCoreApplication::sendPostedEvents();
        EXPECT_EQ(iconDifferences(window.panel(), ThemeColors::Theme::DARK), Lines{});
    }
    QCoreApplication::sendPostedEvents();
    EXPECT_EQ(iconDifferences(window.panel(), ThemeColors::Theme::LIGHT), Lines{});
}

// ======================================================================== the enable rule

/// AddProbe's everyKind(): the Rocket, its stage holding a nose cone, a body tube and a
/// transition; the body tube holding a booster set, a pod set and one component of every other
/// kind; each named by its Java class.
[[nodiscard]] std::unique_ptr<OpenRocketDocument> everyKind(const AddServices& services)
{
    std::unique_ptr<OpenRocketDocument> document = OpenRocketDocumentFactory::createNewRocket();
    Rocket&                             rocket   = document->getRocket();
    RocketComponent&                    stage    = rocket.getChild(0);
    const auto                          make     = [&services](ComponentKind kind) {
        std::unique_ptr<RocketComponent> component = ComponentFactory::create(
            kind, services.services().preferences(), services.services().materials());
        component->setName(className(kind));
        return component;
    };
    static_cast<void>(stage.addChild(make(ComponentKind::NOSE_CONE)));
    RocketComponent& tube = stage.addChild(make(ComponentKind::BODY_TUBE));
    static_cast<void>(stage.addChild(make(ComponentKind::TRANSITION)));
    for (const ComponentKind kind :
         {ComponentKind::PARALLEL_STAGE, ComponentKind::POD_SET, ComponentKind::TRAPEZOID_FIN_SET,
          ComponentKind::ELLIPTICAL_FIN_SET, ComponentKind::FREEFORM_FIN_SET,
          ComponentKind::TUBE_FIN_SET, ComponentKind::LAUNCH_LUG, ComponentKind::RAIL_BUTTON,
          ComponentKind::INNER_TUBE, ComponentKind::TUBE_COUPLER, ComponentKind::CENTERING_RING,
          ComponentKind::BULKHEAD, ComponentKind::ENGINE_BLOCK, ComponentKind::PARACHUTE,
          ComponentKind::STREAMER, ComponentKind::SHOCK_CORD, ComponentKind::MASS_COMPONENT})
    {
        static_cast<void>(tube.addChild(make(kind)));
    }
    rocket.setName("Rocket");
    stage.setName("AxialStage");
    return document;
}

/// AddProbe's matrix(): the buttons, then the enabled ones for nothing selected, for each
/// component of everyKind() selected, and for [InnerTube, AxialStage].
[[nodiscard]] Lines enabledButtonsByKind()
{
    const AddServices                         services;
    const std::unique_ptr<OpenRocketDocument> document = everyKind(services);
    const AddWindow                           window(*document, services);
    Lines                                     lines{"buttons: " + buttonNames(window.panel())};
    window.select({});
    lines.push_back("(nothing):" + window.enabledButtons());
    for (const std::string_view name :
         {"Rocket",        "AxialStage",   "NoseCone",        "BodyTube",         "Transition",
          "ParallelStage", "PodSet",       "TrapezoidFinSet", "EllipticalFinSet", "FreeformFinSet",
          "TubeFinSet",    "LaunchLug",    "RailButton",      "InnerTube",        "TubeCoupler",
          "CenteringRing", "Bulkhead",     "EngineBlock",     "Parachute",        "Streamer",
          "ShockCord",     "MassComponent"})
    {
        window.select({name});
        lines.push_back(std::string(name) + ":" + window.enabledButtons());
    }
    window.select({"InnerTube", "AxialStage"});
    lines.push_back("[InnerTube, AxialStage]:" + window.enabledButtons());
    return lines;
}

// AddProbe.out, "== enabled buttons by selection" (its header's empty button list left out).
constexpr std::string_view kAddProbeEnabled =
    R"(buttons: [AxialStage, ParallelStage, PodSet, NoseCone, BodyTube, Transition, TrapezoidFinSet, EllipticalFinSet, FreeformFinSet, TubeFinSet, RailButton, LaunchLug, InnerTube, TubeCoupler, CenteringRing, Bulkhead, EngineBlock, Parachute, Streamer, ShockCord, MassComponent]
(nothing): AxialStage
Rocket: AxialStage
AxialStage: AxialStage NoseCone BodyTube Transition
NoseCone: AxialStage NoseCone BodyTube Transition FreeformFinSet InnerTube TubeCoupler CenteringRing Bulkhead EngineBlock Parachute Streamer ShockCord MassComponent
BodyTube: AxialStage ParallelStage PodSet NoseCone BodyTube Transition TrapezoidFinSet EllipticalFinSet FreeformFinSet TubeFinSet RailButton LaunchLug InnerTube TubeCoupler CenteringRing Bulkhead EngineBlock Parachute Streamer ShockCord MassComponent
Transition: AxialStage NoseCone BodyTube Transition FreeformFinSet InnerTube TubeCoupler CenteringRing Bulkhead EngineBlock Parachute Streamer ShockCord MassComponent
ParallelStage: AxialStage NoseCone BodyTube Transition
PodSet: AxialStage NoseCone BodyTube Transition
TrapezoidFinSet: AxialStage
EllipticalFinSet: AxialStage
FreeformFinSet: AxialStage
TubeFinSet: AxialStage
LaunchLug: AxialStage
RailButton: AxialStage
InnerTube: AxialStage InnerTube TubeCoupler CenteringRing Bulkhead EngineBlock Parachute Streamer ShockCord MassComponent
TubeCoupler: AxialStage InnerTube TubeCoupler CenteringRing Bulkhead EngineBlock Parachute Streamer ShockCord MassComponent
CenteringRing: AxialStage
Bulkhead: AxialStage
EngineBlock: AxialStage
Parachute: AxialStage
Streamer: AxialStage
ShockCord: AxialStage
MassComponent: AxialStage InnerTube TubeCoupler CenteringRing Bulkhead EngineBlock Parachute Streamer ShockCord MassComponent
[InnerTube, AxialStage]: AxialStage InnerTube TubeCoupler CenteringRing Bulkhead EngineBlock Parachute Streamer ShockCord MassComponent)";

TEST_F(ComponentAddButtonsTest, TheFirstSelectedComponentEnablesTheButtonsAsInOpenRocket)
{
    EXPECT_EQ(enabledButtonsByKind(), linesOf(kAddProbeEnabled));
}

// ======================================================================= the positions

/// AddProbe's pos(): "(<parent>, <index or null>)", "null (cancel)" for none.
[[nodiscard]] std::string addProbePosition(const std::optional<Position>& position)
{
    if (!position.has_value())
    {
        return "null (cancel)";
    }
    return "(" + position->parent->getName() + ", " +
           (position->index.has_value() ? std::to_string(*position->index) : "null") + ")";
}

/// AddProbe's position of the button of @p kind with @p selected selected: its pos(), or
/// "disabled".
[[nodiscard]] std::string addProbePositionOf(const AddWindow& window, AddServices& services,
                                             ComponentKind kind, RocketComponent* selected)
{
    ComponentButton& button = window.button(kind);
    if (!button.isAddable(selected))
    {
        return "disabled";
    }
    return addProbePosition(
        button.additionPosition(selected, services.preferences(), services.prompts()));
}

/// AddProbe's design of positions(): the Sustainer holding BT1 (holding IT and Pods, a pod set
/// with PBT) and BT2, and a second stage "Booster".
[[nodiscard]] std::unique_ptr<OpenRocketDocument> addProbePositionsDesign()
{
    std::unique_ptr<OpenRocketDocument> document  = OpenRocketDocumentFactory::createNewRocket();
    Rocket&                             rocket    = document->getRocket();
    RocketComponent&                    sustainer = rocket.getChild(0);
    const auto add = [](RocketComponent& parent, std::unique_ptr<RocketComponent> component,
                        std::string_view name) -> RocketComponent& {
        component->setName(name);
        return parent.addChild(std::move(component));
    };
    RocketComponent& bt1 = add(sustainer, std::make_unique<BodyTube>(), "BT1");
    static_cast<void>(add(sustainer, std::make_unique<BodyTube>(), "BT2"));
    static_cast<void>(add(bt1, std::make_unique<InnerTube>(), "IT"));
    static_cast<void>(add(rocket, std::make_unique<AxialStage>(), "Booster"));
    RocketComponent& pods = add(bt1, std::make_unique<PodSet>(), "Pods");
    static_cast<void>(add(pods, std::make_unique<BodyTube>(), "PBT"));
    return document;
}

/// AddProbe's positions() for the insert preference @p preference.
[[nodiscard]] Lines addProbePositions(int preference)
{
    AddServices services;
    services.setInsertPositions(preference, preference);
    const std::unique_ptr<OpenRocketDocument> document = addProbePositionsDesign();
    const AddWindow                           window(*document, services);
    Lines                                     lines;
    const std::string                         pref = "pref " + std::to_string(preference);
    for (const std::string_view name :
         {"Rocket", "Sustainer", "BT1", "BT2", "IT", "Booster", "Pods", "PBT"})
    {
        RocketComponent* c = &named(window.rocket(), name);
        lines.push_back(
            pref + " selected " + std::string(name) + ": body tube " +
            addProbePositionOf(window, services, ComponentKind::BODY_TUBE, c) + ", stage " +
            addProbePositionOf(window, services, ComponentKind::AXIAL_STAGE, c) + ", fins " +
            addProbePositionOf(window, services, ComponentKind::TRAPEZOID_FIN_SET, c));
    }
    lines.push_back(pref + " nothing selected: body tube " +
                    addProbePositionOf(window, services, ComponentKind::BODY_TUBE, nullptr) +
                    ", stage " +
                    addProbePositionOf(window, services, ComponentKind::AXIAL_STAGE, nullptr));
    return lines;
}

/// AddProbe's last position: the preference 0, the last component selected.
[[nodiscard]] std::string addProbeLastComponentAsks()
{
    AddServices services;
    services.setInsertPositions(0, 0);
    const std::unique_ptr<OpenRocketDocument> document =
        OpenRocketDocumentFactory::createNewRocket();
    auto bt1 = std::make_unique<BodyTube>();
    bt1->setName("BT1");
    RocketComponent& tube = document->getRocket().getChild(0).addChild(std::move(bt1));
    const AddWindow  window(*document, services);
    return "pref 0 selected last BT1: body tube " +
           addProbePosition(
               window.button(ComponentKind::BODY_TUBE)
                   .additionPosition(&tube, services.preferences(), services.prompts())) +
           ", stage " +
           addProbePosition(
               window.button(ComponentKind::AXIAL_STAGE)
                   .additionPosition(&tube, services.preferences(), services.prompts()));
}

/// AddProbe's positions().
[[nodiscard]] Lines addProbePositionLines()
{
    Lines lines{"== addition positions"};
    for (const int preference : {1, 2})
    {
        const Lines more = addProbePositions(preference);
        lines.insert(lines.end(), more.begin(), more.end());
    }
    lines.push_back(addProbeLastComponentAsks());
    return lines;
}

// AddProbe.out, "== addition positions".
constexpr std::string_view kAddProbePositions = R"(== addition positions
pref 1 selected Rocket: body tube disabled, stage (Rocket, null), fins disabled
pref 1 selected Sustainer: body tube (Sustainer, null), stage (Rocket, 1), fins disabled
pref 1 selected BT1: body tube (Sustainer, 1), stage (Rocket, 1), fins (BT1, null)
pref 1 selected BT2: body tube (Sustainer, 2), stage (Rocket, 1), fins (BT2, null)
pref 1 selected IT: body tube disabled, stage (Rocket, 1), fins disabled
pref 1 selected Booster: body tube (Booster, null), stage (Rocket, 2), fins disabled
pref 1 selected Pods: body tube (Pods, null), stage (Rocket, 1), fins disabled
pref 1 selected PBT: body tube (Pods, 1), stage (Rocket, 1), fins (PBT, null)
pref 1 nothing selected: body tube disabled, stage (Rocket, null)
pref 2 selected Rocket: body tube disabled, stage (Rocket, null), fins disabled
pref 2 selected Sustainer: body tube (Sustainer, null), stage (Rocket, null), fins disabled
pref 2 selected BT1: body tube (Sustainer, null), stage (Rocket, null), fins (BT1, null)
pref 2 selected BT2: body tube (Sustainer, null), stage (Rocket, null), fins (BT2, null)
pref 2 selected IT: body tube disabled, stage (Rocket, null), fins disabled
pref 2 selected Booster: body tube (Booster, null), stage (Rocket, null), fins disabled
pref 2 selected Pods: body tube (Pods, null), stage (Rocket, null), fins disabled
pref 2 selected PBT: body tube (Pods, null), stage (Rocket, null), fins (PBT, null)
pref 2 nothing selected: body tube disabled, stage (Rocket, null)
pref 0 selected last BT1: body tube (Sustainer, null), stage (Rocket, null))";

TEST_F(ComponentAddButtonsTest, PositionsForTheInsertPreferencesAreOpenRocketsOfAddProbe)
{
    EXPECT_EQ(addProbePositionLines(), linesOf(kAddProbePositions));
}

/// T4Probe's pos(): "-" for a disabled button, "asks" when the question came (answered with
/// Cancel here), "cancel", or "(<parent>,<index or end>)".
[[nodiscard]] std::string t4Position(const AddWindow& window, InMemoryPreferences& preferences,
                                     ScriptedUserPrompts& prompts, ComponentKind kind,
                                     RocketComponent* selected)
{
    ComponentButton& button = window.button(kind);
    if (!button.isAddable(selected))
    {
        return "-";
    }
    const std::size_t             before = prompts.prompts().size();
    const std::optional<Position> position =
        button.additionPosition(selected, preferences, prompts);
    if (prompts.prompts().size() > before)
    {
        return "asks";
    }
    if (!position.has_value())
    {
        return "cancel";
    }
    return "(" + position->parent->getName() + "," +
           (position->index.has_value() ? std::to_string(*position->index) : "end") + ")";
}

/// The number of questions in T4Probe's line @p line ("=asks").
[[nodiscard]] std::size_t questionsIn(std::string_view line)
{
    std::size_t count = 0;
    for (std::size_t at = line.find("=asks"); at != std::string_view::npos;
         at             = line.find("=asks", at + 1))
    {
        ++count;
    }
    return count;
}

/// T4Probe's line for the preference @p preference with @p name selected (empty: nothing), with
/// a Cancel scripted for each question @p javaLine (OpenRocket's line) has: a question more or
/// less than OpenRocket's fails the test (ScriptedUserPrompts).
[[nodiscard]] std::string t4PositionLine(const AddWindow& window, InMemoryPreferences& preferences,
                                         int preference, std::string_view name,
                                         std::string_view javaLine)
{
    ScriptedUserPrompts prompts;
    for (std::size_t i = 0; i < questionsIn(javaLine); ++i)
    {
        prompts.answerInsertPosition(
            {.position = UserPrompts::InsertPosition::CANCEL, .doNotAskAgain = false});
    }
    RocketComponent* selected = name.empty() ? nullptr : &named(window.rocket(), name);
    std::string      line     = "pref " + std::to_string(preference) + " " +
                                (name.empty() ? std::string("(nothing)") : std::string(name)) + ":";
    for (const ComponentKind kind :
         {ComponentKind::AXIAL_STAGE, ComponentKind::PARALLEL_STAGE, ComponentKind::POD_SET,
          ComponentKind::NOSE_CONE, ComponentKind::BODY_TUBE, ComponentKind::TRANSITION,
          ComponentKind::TRAPEZOID_FIN_SET, ComponentKind::INNER_TUBE, ComponentKind::PARACHUTE})
    {
        line += " " + std::string(className(kind)) + "=" +
                t4Position(window, preferences, prompts, kind, selected);
    }
    return line;
}

/// T4Probe's positions() for every preference, against OpenRocket's lines @p javaLines (the
/// header first).
[[nodiscard]] Lines t4Positions(const Lines& javaLines)
{
    Lines       lines{"== addition positions (- disabled, asks: the question, end: null index)"};
    std::size_t at = 1;
    for (const int preference : {0, 1, 2, 3, -1})
    {
        AddServices services;
        services.setInsertPositions(preference, preference);
        const ProbeDesign design;
        const AddWindow   window(design.document(), services);
        for (const std::string_view name :
             {"Rocket", "Sustainer", "NC", "BT1", "IT", "EB", "CR1", "Fins", "BT2", "Chute",
              "Booster", "BBT", "Side boosters", "PBT", ""})
        {
            const std::string_view javaLine =
                at < javaLines.size() ? std::string_view(javaLines[at]) : std::string_view();
            lines.push_back(
                t4PositionLine(window, services.preferences(), preference, name, javaLine));
            ++at;
        }
    }
    return lines;
}

/// T4Probe's lines after its table: each insert preference decides its own buttons, and the
/// first selected component decides.
[[nodiscard]] Lines t4PreferencesApartAndTheFirstSelected()
{
    Lines lines;
    for (const auto& [body, stage] : {std::pair{1, 2}, std::pair{2, 1}})
    {
        AddServices services;
        services.setInsertPositions(body, stage);
        const ProbeDesign design;
        const AddWindow   window(design.document(), services);
        RocketComponent*  bt1 = &design.find("BT1");
        lines.push_back("body " + std::to_string(body) + " stage " + std::to_string(stage) +
                        ", BT1: BodyTube=" +
                        t4Position(window, services.preferences(), services.prompts(),
                                   ComponentKind::BODY_TUBE, bt1) +
                        " AxialStage=" +
                        t4Position(window, services.preferences(), services.prompts(),
                                   ComponentKind::AXIAL_STAGE, bt1));
    }
    const AddServices services;
    const ProbeDesign design;
    const AddWindow   window(design.document(), services);
    for (const Names& selection : {Names{"IT", "Sustainer"}, Names{"Sustainer", "IT"}, Names{}})
    {
        window.select(selection);
        lines.push_back("enabled with " + design.names(window.selection().selectedComponents()) +
                        ":" + window.enabledButtons());
    }
    return lines;
}

// T4Probe.out, "== addition positions": its table.
constexpr std::string_view kT4Positions =
    R"(== addition positions (- disabled, asks: the question, end: null index)
pref 0 Rocket: AxialStage=(Rocket,end) ParallelStage=- PodSet=- NoseCone=- BodyTube=- Transition=- TrapezoidFinSet=- InnerTube=- Parachute=-
pref 0 Sustainer: AxialStage=asks ParallelStage=- PodSet=- NoseCone=(Sustainer,end) BodyTube=(Sustainer,end) Transition=(Sustainer,end) TrapezoidFinSet=- InnerTube=- Parachute=-
pref 0 NC: AxialStage=asks ParallelStage=- PodSet=- NoseCone=asks BodyTube=asks Transition=asks TrapezoidFinSet=- InnerTube=(NC,end) Parachute=(NC,end)
pref 0 BT1: AxialStage=asks ParallelStage=(BT1,end) PodSet=(BT1,end) NoseCone=asks BodyTube=asks Transition=asks TrapezoidFinSet=(BT1,end) InnerTube=(BT1,end) Parachute=(BT1,end)
pref 0 IT: AxialStage=asks ParallelStage=- PodSet=- NoseCone=- BodyTube=- Transition=- TrapezoidFinSet=- InnerTube=(IT,end) Parachute=(IT,end)
pref 0 EB: AxialStage=asks ParallelStage=- PodSet=- NoseCone=- BodyTube=- Transition=- TrapezoidFinSet=- InnerTube=- Parachute=-
pref 0 CR1: AxialStage=asks ParallelStage=- PodSet=- NoseCone=- BodyTube=- Transition=- TrapezoidFinSet=- InnerTube=- Parachute=-
pref 0 Fins: AxialStage=asks ParallelStage=- PodSet=- NoseCone=- BodyTube=- Transition=- TrapezoidFinSet=- InnerTube=- Parachute=-
pref 0 BT2: AxialStage=asks ParallelStage=(BT2,end) PodSet=(BT2,end) NoseCone=(Sustainer,end) BodyTube=(Sustainer,end) Transition=(Sustainer,end) TrapezoidFinSet=(BT2,end) InnerTube=(BT2,end) Parachute=(BT2,end)
pref 0 Chute: AxialStage=asks ParallelStage=- PodSet=- NoseCone=- BodyTube=- Transition=- TrapezoidFinSet=- InnerTube=- Parachute=-
pref 0 Booster: AxialStage=(Rocket,end) ParallelStage=- PodSet=- NoseCone=(Booster,end) BodyTube=(Booster,end) Transition=(Booster,end) TrapezoidFinSet=- InnerTube=- Parachute=-
pref 0 BBT: AxialStage=(Rocket,end) ParallelStage=(BBT,end) PodSet=(BBT,end) NoseCone=(Booster,end) BodyTube=(Booster,end) Transition=(Booster,end) TrapezoidFinSet=(BBT,end) InnerTube=(BBT,end) Parachute=(BBT,end)
pref 0 Side boosters: AxialStage=asks ParallelStage=- PodSet=- NoseCone=(Side boosters,end) BodyTube=(Side boosters,end) Transition=(Side boosters,end) TrapezoidFinSet=- InnerTube=- Parachute=-
pref 0 PBT: AxialStage=asks ParallelStage=(PBT,end) PodSet=(PBT,end) NoseCone=(Side boosters,end) BodyTube=(Side boosters,end) Transition=(Side boosters,end) TrapezoidFinSet=(PBT,end) InnerTube=(PBT,end) Parachute=(PBT,end)
pref 0 (nothing): AxialStage=(Rocket,end) ParallelStage=- PodSet=- NoseCone=- BodyTube=- Transition=- TrapezoidFinSet=- InnerTube=- Parachute=-
pref 1 Rocket: AxialStage=(Rocket,end) ParallelStage=- PodSet=- NoseCone=- BodyTube=- Transition=- TrapezoidFinSet=- InnerTube=- Parachute=-
pref 1 Sustainer: AxialStage=(Rocket,1) ParallelStage=- PodSet=- NoseCone=(Sustainer,end) BodyTube=(Sustainer,end) Transition=(Sustainer,end) TrapezoidFinSet=- InnerTube=- Parachute=-
pref 1 NC: AxialStage=(Rocket,1) ParallelStage=- PodSet=- NoseCone=(Sustainer,1) BodyTube=(Sustainer,1) Transition=(Sustainer,1) TrapezoidFinSet=- InnerTube=(NC,end) Parachute=(NC,end)
pref 1 BT1: AxialStage=(Rocket,1) ParallelStage=(BT1,end) PodSet=(BT1,end) NoseCone=(Sustainer,2) BodyTube=(Sustainer,2) Transition=(Sustainer,2) TrapezoidFinSet=(BT1,end) InnerTube=(BT1,end) Parachute=(BT1,end)
pref 1 IT: AxialStage=(Rocket,1) ParallelStage=- PodSet=- NoseCone=- BodyTube=- Transition=- TrapezoidFinSet=- InnerTube=(IT,end) Parachute=(IT,end)
pref 1 EB: AxialStage=(Rocket,1) ParallelStage=- PodSet=- NoseCone=- BodyTube=- Transition=- TrapezoidFinSet=- InnerTube=- Parachute=-
pref 1 CR1: AxialStage=(Rocket,1) ParallelStage=- PodSet=- NoseCone=- BodyTube=- Transition=- TrapezoidFinSet=- InnerTube=- Parachute=-
pref 1 Fins: AxialStage=(Rocket,1) ParallelStage=- PodSet=- NoseCone=- BodyTube=- Transition=- TrapezoidFinSet=- InnerTube=- Parachute=-
pref 1 BT2: AxialStage=(Rocket,1) ParallelStage=(BT2,end) PodSet=(BT2,end) NoseCone=(Sustainer,3) BodyTube=(Sustainer,3) Transition=(Sustainer,3) TrapezoidFinSet=(BT2,end) InnerTube=(BT2,end) Parachute=(BT2,end)
pref 1 Chute: AxialStage=(Rocket,1) ParallelStage=- PodSet=- NoseCone=- BodyTube=- Transition=- TrapezoidFinSet=- InnerTube=- Parachute=-
pref 1 Booster: AxialStage=(Rocket,2) ParallelStage=- PodSet=- NoseCone=(Booster,end) BodyTube=(Booster,end) Transition=(Booster,end) TrapezoidFinSet=- InnerTube=- Parachute=-
pref 1 BBT: AxialStage=(Rocket,2) ParallelStage=(BBT,end) PodSet=(BBT,end) NoseCone=(Booster,1) BodyTube=(Booster,1) Transition=(Booster,1) TrapezoidFinSet=(BBT,end) InnerTube=(BBT,end) Parachute=(BBT,end)
pref 1 Side boosters: AxialStage=(Rocket,0) ParallelStage=- PodSet=- NoseCone=(Side boosters,end) BodyTube=(Side boosters,end) Transition=(Side boosters,end) TrapezoidFinSet=- InnerTube=- Parachute=-
pref 1 PBT: AxialStage=(Rocket,0) ParallelStage=(PBT,end) PodSet=(PBT,end) NoseCone=(Side boosters,1) BodyTube=(Side boosters,1) Transition=(Side boosters,1) TrapezoidFinSet=(PBT,end) InnerTube=(PBT,end) Parachute=(PBT,end)
pref 1 (nothing): AxialStage=(Rocket,end) ParallelStage=- PodSet=- NoseCone=- BodyTube=- Transition=- TrapezoidFinSet=- InnerTube=- Parachute=-
pref 2 Rocket: AxialStage=(Rocket,end) ParallelStage=- PodSet=- NoseCone=- BodyTube=- Transition=- TrapezoidFinSet=- InnerTube=- Parachute=-
pref 2 Sustainer: AxialStage=(Rocket,end) ParallelStage=- PodSet=- NoseCone=(Sustainer,end) BodyTube=(Sustainer,end) Transition=(Sustainer,end) TrapezoidFinSet=- InnerTube=- Parachute=-
pref 2 NC: AxialStage=(Rocket,end) ParallelStage=- PodSet=- NoseCone=(Sustainer,end) BodyTube=(Sustainer,end) Transition=(Sustainer,end) TrapezoidFinSet=- InnerTube=(NC,end) Parachute=(NC,end)
pref 2 BT1: AxialStage=(Rocket,end) ParallelStage=(BT1,end) PodSet=(BT1,end) NoseCone=(Sustainer,end) BodyTube=(Sustainer,end) Transition=(Sustainer,end) TrapezoidFinSet=(BT1,end) InnerTube=(BT1,end) Parachute=(BT1,end)
pref 2 IT: AxialStage=(Rocket,end) ParallelStage=- PodSet=- NoseCone=- BodyTube=- Transition=- TrapezoidFinSet=- InnerTube=(IT,end) Parachute=(IT,end)
pref 2 EB: AxialStage=(Rocket,end) ParallelStage=- PodSet=- NoseCone=- BodyTube=- Transition=- TrapezoidFinSet=- InnerTube=- Parachute=-
pref 2 CR1: AxialStage=(Rocket,end) ParallelStage=- PodSet=- NoseCone=- BodyTube=- Transition=- TrapezoidFinSet=- InnerTube=- Parachute=-
pref 2 Fins: AxialStage=(Rocket,end) ParallelStage=- PodSet=- NoseCone=- BodyTube=- Transition=- TrapezoidFinSet=- InnerTube=- Parachute=-
pref 2 BT2: AxialStage=(Rocket,end) ParallelStage=(BT2,end) PodSet=(BT2,end) NoseCone=(Sustainer,end) BodyTube=(Sustainer,end) Transition=(Sustainer,end) TrapezoidFinSet=(BT2,end) InnerTube=(BT2,end) Parachute=(BT2,end)
pref 2 Chute: AxialStage=(Rocket,end) ParallelStage=- PodSet=- NoseCone=- BodyTube=- Transition=- TrapezoidFinSet=- InnerTube=- Parachute=-
pref 2 Booster: AxialStage=(Rocket,end) ParallelStage=- PodSet=- NoseCone=(Booster,end) BodyTube=(Booster,end) Transition=(Booster,end) TrapezoidFinSet=- InnerTube=- Parachute=-
pref 2 BBT: AxialStage=(Rocket,end) ParallelStage=(BBT,end) PodSet=(BBT,end) NoseCone=(Booster,end) BodyTube=(Booster,end) Transition=(Booster,end) TrapezoidFinSet=(BBT,end) InnerTube=(BBT,end) Parachute=(BBT,end)
pref 2 Side boosters: AxialStage=(Rocket,end) ParallelStage=- PodSet=- NoseCone=(Side boosters,end) BodyTube=(Side boosters,end) Transition=(Side boosters,end) TrapezoidFinSet=- InnerTube=- Parachute=-
pref 2 PBT: AxialStage=(Rocket,end) ParallelStage=(PBT,end) PodSet=(PBT,end) NoseCone=(Side boosters,end) BodyTube=(Side boosters,end) Transition=(Side boosters,end) TrapezoidFinSet=(PBT,end) InnerTube=(PBT,end) Parachute=(PBT,end)
pref 2 (nothing): AxialStage=(Rocket,end) ParallelStage=- PodSet=- NoseCone=- BodyTube=- Transition=- TrapezoidFinSet=- InnerTube=- Parachute=-
pref 3 Rocket: AxialStage=(Rocket,end) ParallelStage=- PodSet=- NoseCone=- BodyTube=- Transition=- TrapezoidFinSet=- InnerTube=- Parachute=-
pref 3 Sustainer: AxialStage=asks ParallelStage=- PodSet=- NoseCone=(Sustainer,end) BodyTube=(Sustainer,end) Transition=(Sustainer,end) TrapezoidFinSet=- InnerTube=- Parachute=-
pref 3 NC: AxialStage=asks ParallelStage=- PodSet=- NoseCone=asks BodyTube=asks Transition=asks TrapezoidFinSet=- InnerTube=(NC,end) Parachute=(NC,end)
pref 3 BT1: AxialStage=asks ParallelStage=(BT1,end) PodSet=(BT1,end) NoseCone=asks BodyTube=asks Transition=asks TrapezoidFinSet=(BT1,end) InnerTube=(BT1,end) Parachute=(BT1,end)
pref 3 IT: AxialStage=asks ParallelStage=- PodSet=- NoseCone=- BodyTube=- Transition=- TrapezoidFinSet=- InnerTube=(IT,end) Parachute=(IT,end)
pref 3 EB: AxialStage=asks ParallelStage=- PodSet=- NoseCone=- BodyTube=- Transition=- TrapezoidFinSet=- InnerTube=- Parachute=-
pref 3 CR1: AxialStage=asks ParallelStage=- PodSet=- NoseCone=- BodyTube=- Transition=- TrapezoidFinSet=- InnerTube=- Parachute=-
pref 3 Fins: AxialStage=asks ParallelStage=- PodSet=- NoseCone=- BodyTube=- Transition=- TrapezoidFinSet=- InnerTube=- Parachute=-
pref 3 BT2: AxialStage=asks ParallelStage=(BT2,end) PodSet=(BT2,end) NoseCone=(Sustainer,end) BodyTube=(Sustainer,end) Transition=(Sustainer,end) TrapezoidFinSet=(BT2,end) InnerTube=(BT2,end) Parachute=(BT2,end)
pref 3 Chute: AxialStage=asks ParallelStage=- PodSet=- NoseCone=- BodyTube=- Transition=- TrapezoidFinSet=- InnerTube=- Parachute=-
pref 3 Booster: AxialStage=(Rocket,end) ParallelStage=- PodSet=- NoseCone=(Booster,end) BodyTube=(Booster,end) Transition=(Booster,end) TrapezoidFinSet=- InnerTube=- Parachute=-
pref 3 BBT: AxialStage=(Rocket,end) ParallelStage=(BBT,end) PodSet=(BBT,end) NoseCone=(Booster,end) BodyTube=(Booster,end) Transition=(Booster,end) TrapezoidFinSet=(BBT,end) InnerTube=(BBT,end) Parachute=(BBT,end)
pref 3 Side boosters: AxialStage=asks ParallelStage=- PodSet=- NoseCone=(Side boosters,end) BodyTube=(Side boosters,end) Transition=(Side boosters,end) TrapezoidFinSet=- InnerTube=- Parachute=-
pref 3 PBT: AxialStage=asks ParallelStage=(PBT,end) PodSet=(PBT,end) NoseCone=(Side boosters,end) BodyTube=(Side boosters,end) Transition=(Side boosters,end) TrapezoidFinSet=(PBT,end) InnerTube=(PBT,end) Parachute=(PBT,end)
pref 3 (nothing): AxialStage=(Rocket,end) ParallelStage=- PodSet=- NoseCone=- BodyTube=- Transition=- TrapezoidFinSet=- InnerTube=- Parachute=-
pref -1 Rocket: AxialStage=(Rocket,end) ParallelStage=- PodSet=- NoseCone=- BodyTube=- Transition=- TrapezoidFinSet=- InnerTube=- Parachute=-
pref -1 Sustainer: AxialStage=asks ParallelStage=- PodSet=- NoseCone=(Sustainer,end) BodyTube=(Sustainer,end) Transition=(Sustainer,end) TrapezoidFinSet=- InnerTube=- Parachute=-
pref -1 NC: AxialStage=asks ParallelStage=- PodSet=- NoseCone=asks BodyTube=asks Transition=asks TrapezoidFinSet=- InnerTube=(NC,end) Parachute=(NC,end)
pref -1 BT1: AxialStage=asks ParallelStage=(BT1,end) PodSet=(BT1,end) NoseCone=asks BodyTube=asks Transition=asks TrapezoidFinSet=(BT1,end) InnerTube=(BT1,end) Parachute=(BT1,end)
pref -1 IT: AxialStage=asks ParallelStage=- PodSet=- NoseCone=- BodyTube=- Transition=- TrapezoidFinSet=- InnerTube=(IT,end) Parachute=(IT,end)
pref -1 EB: AxialStage=asks ParallelStage=- PodSet=- NoseCone=- BodyTube=- Transition=- TrapezoidFinSet=- InnerTube=- Parachute=-
pref -1 CR1: AxialStage=asks ParallelStage=- PodSet=- NoseCone=- BodyTube=- Transition=- TrapezoidFinSet=- InnerTube=- Parachute=-
pref -1 Fins: AxialStage=asks ParallelStage=- PodSet=- NoseCone=- BodyTube=- Transition=- TrapezoidFinSet=- InnerTube=- Parachute=-
pref -1 BT2: AxialStage=asks ParallelStage=(BT2,end) PodSet=(BT2,end) NoseCone=(Sustainer,end) BodyTube=(Sustainer,end) Transition=(Sustainer,end) TrapezoidFinSet=(BT2,end) InnerTube=(BT2,end) Parachute=(BT2,end)
pref -1 Chute: AxialStage=asks ParallelStage=- PodSet=- NoseCone=- BodyTube=- Transition=- TrapezoidFinSet=- InnerTube=- Parachute=-
pref -1 Booster: AxialStage=(Rocket,end) ParallelStage=- PodSet=- NoseCone=(Booster,end) BodyTube=(Booster,end) Transition=(Booster,end) TrapezoidFinSet=- InnerTube=- Parachute=-
pref -1 BBT: AxialStage=(Rocket,end) ParallelStage=(BBT,end) PodSet=(BBT,end) NoseCone=(Booster,end) BodyTube=(Booster,end) Transition=(Booster,end) TrapezoidFinSet=(BBT,end) InnerTube=(BBT,end) Parachute=(BBT,end)
pref -1 Side boosters: AxialStage=asks ParallelStage=- PodSet=- NoseCone=(Side boosters,end) BodyTube=(Side boosters,end) Transition=(Side boosters,end) TrapezoidFinSet=- InnerTube=- Parachute=-
pref -1 PBT: AxialStage=asks ParallelStage=(PBT,end) PodSet=(PBT,end) NoseCone=(Side boosters,end) BodyTube=(Side boosters,end) Transition=(Side boosters,end) TrapezoidFinSet=(PBT,end) InnerTube=(PBT,end) Parachute=(PBT,end)
pref -1 (nothing): AxialStage=(Rocket,end) ParallelStage=- PodSet=- NoseCone=- BodyTube=- Transition=- TrapezoidFinSet=- InnerTube=- Parachute=-)";

// T4Probe.out, "== addition positions": the lines after the table.
constexpr std::string_view kT4PositionsAfter =
    R"(body 1 stage 2, BT1: BodyTube=(Sustainer,2) AxialStage=(Rocket,end)
body 2 stage 1, BT1: BodyTube=(Sustainer,end) AxialStage=(Rocket,1)
enabled with [IT, Sustainer]: AxialStage InnerTube TubeCoupler CenteringRing Bulkhead EngineBlock Parachute Streamer ShockCord MassComponent
enabled with [Sustainer, IT]: AxialStage NoseCone BodyTube Transition
enabled with []: AxialStage)";

TEST_F(ComponentAddButtonsTest, PositionsForEveryPreferenceAndComponentAreOpenRocketsOfT4Probe)
{
    const Lines expected = linesOf(kT4Positions);
    EXPECT_EQ(t4Positions(expected), expected);
}

TEST_F(ComponentAddButtonsTest, EachPreferenceDecidesItsOwnButtonsAndTheFirstSelectedDecides)
{
    EXPECT_EQ(t4PreferencesApartAndTheFirstSelected(), linesOf(kT4PositionsAfter));
}

// =========================================================================== the question

/// The position of the button of @p kind with @p name selected, the question answered with
/// @p answer, in @p window with @p services; "asked <question>" in front when it was asked.
[[nodiscard]] std::string answered(const AddWindow& window, AddServices& services,
                                   ComponentKind kind, std::string_view name, Answer answer)
{
    ScriptedUserPrompts prompts;
    prompts.answerInsertPosition(answer);
    const std::optional<Position> position = window.button(kind).additionPosition(
        &named(window.rocket(), name), services.preferences(), prompts);
    std::string text;
    for (const ScriptedUserPrompts::Prompt& prompt : prompts.prompts())
    {
        text += "asked " + QtStrings::toUtf8(prompt.texts.join(QStringLiteral(","))) + " ";
    }
    if (!position.has_value())
    {
        return text + "nothing";
    }
    return text + "(" + position->parent->getName() + "," +
           (position->index.has_value() ? std::to_string(*position->index) : "end") + ")";
}

/// "Insert here", "Add to the end" or Cancel, with "Do not ask me again" ticked or not.
[[nodiscard]] Answer answer(UserPrompts::InsertPosition position, bool doNotAskAgain = false)
{
    return {.position = position, .doNotAskAgain = doNotAskAgain};
}

constexpr auto kInsertHere = UserPrompts::InsertPosition::INSERT_HERE;
constexpr auto kAddToEnd   = UserPrompts::InsertPosition::ADD_TO_END;
constexpr auto kCancel     = UserPrompts::InsertPosition::CANCEL;

TEST_F(ComponentAddButtonsTest, TheQuestionsAnswersAreTheInsertPreferences)
{
    AddServices services;
    services.setInsertPositions(0, 0);
    const ProbeDesign design;
    const AddWindow   window(design.document(), services);
    EXPECT_EQ(answered(window, services, ComponentKind::BODY_TUBE, "BT1", answer(kInsertHere)),
              "asked BODY_COMPONENT (Sustainer,2)");
    EXPECT_EQ(answered(window, services, ComponentKind::NOSE_CONE, "BT1", answer(kAddToEnd)),
              "asked BODY_COMPONENT (Sustainer,end)");
    EXPECT_EQ(answered(window, services, ComponentKind::TRANSITION, "NC", answer(kCancel)),
              "asked BODY_COMPONENT nothing");
    EXPECT_EQ(answered(window, services, ComponentKind::AXIAL_STAGE, "IT", answer(kInsertHere)),
              "asked STAGE (Rocket,1)");
    EXPECT_EQ(answered(window, services, ComponentKind::AXIAL_STAGE, "IT", answer(kAddToEnd)),
              "asked STAGE (Rocket,end)");
    EXPECT_EQ(answered(window, services, ComponentKind::AXIAL_STAGE, "Sustainer", answer(kCancel)),
              "asked STAGE nothing");
    // A booster set is not among the Rocket's children: Java's -1 + 1, the first place.
    EXPECT_EQ(answered(window, services, ComponentKind::AXIAL_STAGE, "PBT", answer(kInsertHere)),
              "asked STAGE (Rocket,0)");
    // Nothing was stored.
    EXPECT_EQ(services.preferences().getInt(PreferenceKeys::kBodyComponentInsertPositionKey, -9),
              0);
    EXPECT_EQ(services.preferences().getInt(PreferenceKeys::kStageInsertPositionKey, -9), 0);
}

/// The insert preference @p key of @p services, -9 for none.
[[nodiscard]] int storedPreference(AddServices& services, std::string_view key)
{
    return services.preferences().getInt(key, -9);
}

TEST_F(ComponentAddButtonsTest, DoNotAskAgainStoresTheAnswerButNotACancel)
{
    AddServices services;
    services.setInsertPositions(0, 0);
    const ProbeDesign          design;
    const AddWindow            window(design.document(), services);
    constexpr std::string_view kBody  = PreferenceKeys::kBodyComponentInsertPositionKey;
    constexpr std::string_view kStage = PreferenceKeys::kStageInsertPositionKey;
    EXPECT_EQ(answered(window, services, ComponentKind::BODY_TUBE, "NC", answer(kCancel, true)),
              "asked BODY_COMPONENT nothing");
    EXPECT_EQ(storedPreference(services, kBody), 0);
    EXPECT_EQ(answered(window, services, ComponentKind::BODY_TUBE, "NC", answer(kInsertHere, true)),
              "asked BODY_COMPONENT (Sustainer,1)");
    EXPECT_EQ(storedPreference(services, kBody), 1);
    EXPECT_EQ(storedPreference(services, kStage), 0);
    EXPECT_EQ(answered(window, services, ComponentKind::AXIAL_STAGE, "NC", answer(kAddToEnd, true)),
              "asked STAGE (Rocket,end)");
    EXPECT_EQ(storedPreference(services, kStage), 2);
    // From now on nothing is asked (a question to "none", which has no answers, would fail).
    ScriptedUserPrompts none;
    EXPECT_EQ(
        addProbePosition(window.button(ComponentKind::BODY_TUBE)
                             .additionPosition(&design.find("BT1"), services.preferences(), none)),
        "(Sustainer, 2)");
    EXPECT_EQ(
        addProbePosition(window.button(ComponentKind::AXIAL_STAGE)
                             .additionPosition(&design.find("BT1"), services.preferences(), none)),
        "(Rocket, null)");
}

// ============================================================================== the clicks

TEST_F(ComponentAddButtonsTest, AClickAddsTheComponentSelectsItAloneAndAsksForItsDialog)
{
    const AddServices services;
    const ProbeDesign design;
    const AddWindow   window(design.document(), services);
    window.select({"BT1", "NC"});
    window.button(ComponentKind::BODY_TUBE).click();
    EXPECT_EQ(design.tree(),
              "Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins], BT2[Chute], Body Tube*], "
              "Booster[BBT[Side boosters[PBT]]]]");
    EXPECT_EQ(undoText(design.document()), "undo='Add Body Tube'");
    EXPECT_EQ(design.names(window.selection().selectedComponents()), "[Body Tube*]");
    EXPECT_EQ(window.dialogs(), Lines{"Body Tube"});
    // The component is OpenRocket's new one: the default materials of the start-up.
    window.select({"BT2"});
    window.button(ComponentKind::NOSE_CONE).click();
    EXPECT_EQ(window.selection().selectedComponent()->getName(), "Nose Cone");
    EXPECT_EQ(window.dialogs(), (Lines{"Body Tube", "Nose Cone"}));
}

TEST_F(ComponentAddButtonsTest, ACancelledQuestionAddsNothing)
{
    AddServices services;
    services.setInsertPositions(0, 0);
    const ProbeDesign design;
    const AddWindow   window(design.document(), services);
    const std::size_t states = design.document().getUndoDetail().descriptions.size();
    window.select({"NC"});
    services.prompts().answerInsertPosition(answer(kCancel));
    window.button(ComponentKind::BODY_TUBE).click();
    EXPECT_EQ(design.tree(),
              "Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins], BT2[Chute]], "
              "Booster[BBT[Side boosters[PBT]]]]");
    EXPECT_EQ(design.document().getUndoDetail().descriptions.size(), states);
    EXPECT_EQ(design.names(window.selection().selectedComponents()), "[NC]");
    EXPECT_EQ(window.dialogs(), Lines{});
}

TEST_F(ComponentAddButtonsTest, ADisabledButtonAddsNothing)
{
    const AddServices services;
    const ProbeDesign design;
    const AddWindow   window(design.document(), services);
    window.select({"Fins"});
    ComponentButton& nose = window.button(ComponentKind::NOSE_CONE);
    EXPECT_FALSE(nose.isEnabled());
    nose.click();
    EXPECT_FALSE(nose.addComponent());
    EXPECT_EQ(design.tree(),
              "Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins], BT2[Chute]], "
              "Booster[BBT[Side boosters[PBT]]]]");
    EXPECT_EQ(window.dialogs(), Lines{});
}

TEST_F(ComponentAddButtonsTest, AnAdditionBeyondTheComponentBoundIsRefused)
{
    AddServices       services;
    const ProbeDesign design;
    const AddWindow   window(design.document(), services);
    fillUpTo(design.rocket(), design.find("BT2"), DocumentConfig::kMaxComponents - 1);
    window.select({"BT1"});
    window.button(ComponentKind::INNER_TUBE).click();
    EXPECT_EQ(DocumentConfig::componentCount(design.rocket()), DocumentConfig::kMaxComponents);
    EXPECT_EQ(undoText(design.document()), "undo='Add Inner Tube'");
    const std::size_t states = design.document().getUndoDetail().descriptions.size();
    window.select({"BT1"});
    window.button(ComponentKind::INNER_TUBE).click();
    EXPECT_EQ(DocumentConfig::componentCount(design.rocket()), DocumentConfig::kMaxComponents);
    EXPECT_EQ(design.document().getUndoDetail().descriptions.size(), states);
    ASSERT_EQ(services.prompts().prompts().size(), 1U);
    const ScriptedUserPrompts::Prompt& message = services.prompts().prompts().front();
    EXPECT_EQ(message.kind, ScriptedUserPrompts::Kind::MESSAGE);
    EXPECT_EQ(message.title, QStringLiteral("Add Inner Tube"));
    EXPECT_EQ(message.texts,
              (QStringList{QStringLiteral("WARNING"),
                           QStringLiteral("The rocket would have 1001 components. QtRocket opens "
                                          "at most 1000 components of a design and would ignore "
                                          "the ones beyond when it opens this design again.")}));
    EXPECT_EQ(window.dialogs(), Lines{"Inner Tube"});
    EXPECT_EQ(design.names(window.selection().selectedComponents()), "[BT1]");
}

// The bounds the M2 run 2 review added to decision O27 (ComponentLimits): the stages, booster sets
// and pod sets of the rocket (DocumentConfig::kMaxAssemblies) ...
TEST_F(ComponentAddButtonsTest, AStageBeyondTheAssemblyBoundIsRefused)
{
    AddServices       services;
    const ProbeDesign design;
    const AddWindow   window(design.document(), services);
    fillAssembliesUpTo(design.rocket(), DocumentConfig::kMaxAssemblies - 1);
    window.select({"Sustainer"});
    window.button(ComponentKind::AXIAL_STAGE).click();
    EXPECT_EQ(DocumentConfig::assemblyCount(design.rocket()), DocumentConfig::kMaxAssemblies);
    EXPECT_EQ(undoText(design.document()), "undo='Add Stage'");
    const std::size_t states     = design.document().getUndoDetail().descriptions.size();
    const std::size_t components = DocumentConfig::componentCount(design.rocket());
    window.select({"Sustainer"});
    window.button(ComponentKind::AXIAL_STAGE).click();
    EXPECT_EQ(DocumentConfig::assemblyCount(design.rocket()), DocumentConfig::kMaxAssemblies);
    EXPECT_EQ(DocumentConfig::componentCount(design.rocket()), components);
    EXPECT_EQ(design.document().getUndoDetail().descriptions.size(), states);
    ASSERT_EQ(services.prompts().prompts().size(), 1U);
    const ScriptedUserPrompts::Prompt& message = services.prompts().prompts().front();
    EXPECT_EQ(message.kind, ScriptedUserPrompts::Kind::MESSAGE);
    EXPECT_EQ(message.title, QStringLiteral("Add Stage"));
    EXPECT_EQ(
        message.texts,
        (QStringList{QStringLiteral("WARNING"),
                     QStringLiteral("The rocket would have 101 stages, booster sets and pod "
                                    "sets. QtRocket opens at most 100 of them in a design and "
                                    "would ignore the ones beyond, with everything in them, "
                                    "when it opens this design again.")}));
    EXPECT_EQ(window.dialogs(), Lines{"Stage"});
    EXPECT_EQ(design.names(window.selection().selectedComponents()), "[Sustainer]");
}

// ... and how deep a component stands below the Rocket (ComponentHandler::kMaxDepth).
TEST_F(ComponentAddButtonsTest, AComponentBeyondTheDepthBoundIsRefused)
{
    AddServices       services;
    const ProbeDesign design;
    const AddWindow   window(design.document(), services);
    // IT stands at level 3: inner tubes down to level 99.
    const RocketComponent& innermost =
        nestInnerTubes(design.rocket(), design.find("IT"), ComponentHandler::kMaxDepth - 4);
    ASSERT_EQ(ComponentLimits::levelOf(innermost), 99U);
    window.selection().setSelectedComponent(innermost);
    window.button(ComponentKind::INNER_TUBE).click();
    // The new tube, at level 100, is the selection.
    const std::vector<RocketComponent*> added = window.selection().selectedComponents();
    ASSERT_EQ(added.size(), 1U);
    EXPECT_EQ(ComponentLimits::levelOf(*added.front()), 100U);
    const std::size_t states     = design.document().getUndoDetail().descriptions.size();
    const std::size_t components = DocumentConfig::componentCount(design.rocket());
    window.button(ComponentKind::INNER_TUBE).click();
    EXPECT_EQ(DocumentConfig::componentCount(design.rocket()), components);
    EXPECT_EQ(added.front()->getChildCount(), 0U);
    EXPECT_EQ(design.document().getUndoDetail().descriptions.size(), states);
    ASSERT_EQ(services.prompts().prompts().size(), 1U);
    const ScriptedUserPrompts::Prompt& message = services.prompts().prompts().front();
    EXPECT_EQ(message.title, QStringLiteral("Add Inner Tube"));
    EXPECT_EQ(message.texts,
              (QStringList{QStringLiteral("WARNING"),
                           QStringLiteral("Components would stand 101 levels below the rocket. "
                                          "QtRocket opens components at most 100 levels deep and "
                                          "would ignore the deeper ones, with everything in them, "
                                          "when it opens this design again.")}));
    EXPECT_EQ(window.dialogs(), Lines{"Inner Tube"});
}

/// A listener of @p rocket that takes the first component added to BT1 beyond its three out of
/// the rocket again at once, while the addition delivers its event; it keeps the component (the
/// core calls childAdded() on it once the event is delivered, as Java's garbage collector keeps
/// it alive).
class AdditionTakenBack
{
public:
    explicit AdditionTakenBack(const ProbeDesign& design)
      : m_connection(design.rocket().addComponentChangeListener(
            [this, &design](const ComponentChangeEvent& event) { takeBack(design, event); }))
    {
    }

    /// The components taken out.
    [[nodiscard]] const std::vector<std::unique_ptr<RocketComponent>>& removed() const noexcept
    {
        return m_removed;
    }

private:
    void takeBack(const ProbeDesign& design, const ComponentChangeEvent& event)
    {
        RocketComponent& tube = design.find("BT1");
        if (m_removed.empty() && event.isTreeChange() && tube.getChildCount() > 3)
        {
            m_removed.push_back(tube.removeChild(tube.getChildCount() - 1));
        }
    }

    std::vector<std::unique_ptr<RocketComponent>> m_removed;
    ComponentChangeSignal::ScopedConnection       m_connection;
};

// A listener that takes the new component out of the rocket again at once leaves the button
// nothing to select and no dialog to ask for: it finds the component by id after the addition
// instead of keeping the object it added.
TEST_F(ComponentAddButtonsTest, KeepsNoComponentAcrossTheAddition)
{
    const AddServices       services;
    const ProbeDesign       design;
    const AddWindow         window(design.document(), services);
    const AdditionTakenBack takenBack(design);
    window.select({"BT1"});
    window.button(ComponentKind::INNER_TUBE).click();
    ASSERT_EQ(takenBack.removed().size(), 1U);
    EXPECT_EQ(takenBack.removed().front()->getName(), "Inner Tube");
    EXPECT_EQ(design.tree(),
              "Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins], BT2[Chute]], "
              "Booster[BBT[Side boosters[PBT]]]]");
    EXPECT_EQ(design.names(window.selection().selectedComponents()), "[BT1]");
    EXPECT_EQ(window.dialogs(), Lines{});
    EXPECT_EQ(undoText(design.document()), "undo='Add Inner Tube'");
}

TEST_F(ComponentAddButtonsTest, TheButtonsOutliveTheSelectionHarmlessly)
{
    const AddServices services;
    const ProbeDesign design;
    AddWindow         window(design.document(), services);
    window.select({"BT1"});
    ComponentButton& tube = window.button(ComponentKind::INNER_TUBE);
    window.destroyView();
    tube.click();
    EXPECT_FALSE(tube.addComponent());
    tube.updateEnabled();
    EXPECT_FALSE(tube.isEnabled());
    EXPECT_EQ(design.tree(),
              "Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins], BT2[Chute]], "
              "Booster[BBT[Side boosters[PBT]]]]");
}

/// The add buttons' design clicked as AddProbe and ComponentFactoryDumper click it: each click
/// logged as the report's section "== clicks" logs it.
class ClickScenario
{
public:
    ClickScenario() : m_window(*m_document, m_services) { }

    /// Selects @p selected and clicks the button of @p kind.
    void click(ComponentKind kind, const RocketComponent& selected)
    {
        const std::string on = selected.getName();
        m_window.selection().setSelectedComponent(selected);
        m_window.button(kind).click();
        const RocketComponent* added = m_window.selection().selectedComponent();
        if (added == nullptr || added->getParent() == nullptr)
        {
            ADD_FAILURE() << "nothing selected after the click on " << className(kind);
            return;
        }
        const RocketComponent& parent = *added->getParent();
        m_log += std::string(className(kind)) + " on " + on + ": " + undoText(*m_document) +
                 " selected=" + added->getName() + " parent=" + parent.getName() + "@" +
                 std::to_string(parent.getChildPosition(added).value_or(0)) + "\n";
    }

    /// The 25 clicks of ComponentFactoryDumper.clicks().
    void clickAll()
    {
        Rocket& rocket = m_document->getRocket();
        click(ComponentKind::NOSE_CONE, rocket.getChild(0));
        click(ComponentKind::BODY_TUBE, rocket.getChild(0).getChild(0));
        click(ComponentKind::TRANSITION, rocket.getChild(0).getChild(1));
        click(ComponentKind::BODY_TUBE, rocket.getChild(0).getChild(2));
        for (const ComponentKind kind :
             {ComponentKind::TRAPEZOID_FIN_SET, ComponentKind::ELLIPTICAL_FIN_SET,
              ComponentKind::FREEFORM_FIN_SET, ComponentKind::TUBE_FIN_SET,
              ComponentKind::RAIL_BUTTON, ComponentKind::LAUNCH_LUG, ComponentKind::INNER_TUBE,
              ComponentKind::TUBE_COUPLER, ComponentKind::CENTERING_RING, ComponentKind::BULKHEAD,
              ComponentKind::ENGINE_BLOCK, ComponentKind::PARACHUTE, ComponentKind::STREAMER,
              ComponentKind::SHOCK_CORD, ComponentKind::MASS_COMPONENT, ComponentKind::POD_SET,
              ComponentKind::PARALLEL_STAGE})
        {
            click(kind, rocket.getChild(0).getChild(1));
        }
        click(ComponentKind::AXIAL_STAGE, rocket);
        click(ComponentKind::BODY_TUBE, rocket.getChild(1));
        RocketComponent& tube = rocket.getChild(0).getChild(1);
        click(ComponentKind::BODY_TUBE, tube.getChild(15));
        click(ComponentKind::NOSE_CONE, tube.getChild(16));
    }

    /// The log, then the tree.
    [[nodiscard]] std::string logAndTree() const
    {
        return m_log + "tree=" + ProbeDesign::plainTree(m_document->getRocket()) + "\n";
    }

    [[nodiscard]] OpenRocketDocument& document() const noexcept { return *m_document; }
    [[nodiscard]] const Preferences&  preferences() noexcept { return m_services.preferences(); }
    [[nodiscard]] const Lines&        dialogs() const noexcept { return m_window.dialogs(); }

private:
    AddServices                         m_services;
    std::unique_ptr<OpenRocketDocument> m_document = OpenRocketDocumentFactory::createNewRocket();
    AddWindow                           m_window;
    std::string                         m_log;
};

TEST_F(ComponentAddButtonsTest, TwentyFiveClicksGiveOpenRocketsUndoTextsPlacesAndSelections)
{
    ClickScenario scenario;
    scenario.clickAll();
    EXPECT_EQ(scenario.logAndTree(), QtRocket::Test::componentFactoryReportBlock(
                                         "== clicks (both insert preferences 'add to the end')"));
    EXPECT_EQ(scenario.dialogs().size(), 25U);
}

TEST_F(ComponentAddButtonsTest, TwentyFiveClicksSaveAsOpenRocketsDesign)
{
    ClickScenario scenario;
    scenario.clickAll();
    GeneralRocketSaver                   saver(scenario.preferences());
    const Result<std::vector<std::byte>> bytes =
        saver.saveToBytes(scenario.document(), scenario.document().getDefaultStorageOptions());
    ASSERT_TRUE(bytes.has_value()) << bytes.error().toString();
    EXPECT_TRUE(saver.getWarnings().empty());
    const std::string expected = QtRocket::Test::openRocketsAddButtonsDesign();
    ASSERT_FALSE(expected.empty());
    EXPECT_EQ(QtRocket::Test::normalisedDesignText(QtRocket::Test::theOnlyDesignOf(*bytes)),
              QtRocket::Test::normalisedDesignText(expected));
}

}  // namespace

}  // namespace QtRocket::Gui
