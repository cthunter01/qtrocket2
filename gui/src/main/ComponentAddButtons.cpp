#include "QtRocket/gui/main/ComponentAddButtons.h"

#include <algorithm>
#include <vector>

#include <QEvent>
#include <QLabel>
#include <QSize>
#include <QString>
#include <QStringList>
#include <QVBoxLayout>
#include <QWidget>
#include <Qt>
#include <QtContainerFwd>

#include "QtRocket/gui/main/ComponentButton.h"
#include "QtRocket/gui/tree/DocumentSelectionModel.h"
#include "QtRocket/gui/util/GuiServices.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/widgets/FlowLayout.h"
#include "QtRocket/rocket/ComponentKind.h"

namespace QtRocket::Gui
{

ComponentAddButtons::ComponentAddButtons(DocumentSelectionModel& selection,
                                         const GuiServices& services, QWidget* parent)
  : QWidget(parent), m_selection(&selection), m_services(services)
{
    setObjectName(QStringLiteral("componentAddButtons"));
    using Placement = ComponentButton::Placement;
    auto* layout    = makeChild<QVBoxLayout>(this);

    //: compaddbuttons.ComponentAssembly
    addGroup(*layout, tr("Assembly Components"),
             {
                 //: compaddbuttons.AxialStage
                 {.kind      = ComponentKind::AXIAL_STAGE,
                  .text      = tr("Stage"),
                  .placement = Placement::STAGE},
                 //: compaddbuttons.ParallelStage
                 {.kind      = ComponentKind::PARALLEL_STAGE,
                  .text      = tr("Boosters"),
                  .placement = Placement::INTO_SELECTED},
                 //: compaddbuttons.Pods
                 {.kind      = ComponentKind::POD_SET,
                  .text      = tr("Pods"),
                  .placement = Placement::INTO_SELECTED},
             },
             true);
    //: compaddbuttons.Bodycompandfinsets
    addGroup(*layout, tr("Body Components and Fin Sets"),
             {
                 //: compaddbuttons.Nosecone
                 {.kind      = ComponentKind::NOSE_CONE,
                  .text      = tr("Nose Cone"),
                  .placement = Placement::BODY_COMPONENT},
                 //: compaddbuttons.Bodytube
                 {.kind      = ComponentKind::BODY_TUBE,
                  .text      = tr("Body Tube"),
                  .placement = Placement::BODY_COMPONENT},
                 //: compaddbuttons.Transition
                 {.kind      = ComponentKind::TRANSITION,
                  .text      = tr("Transition"),
                  .placement = Placement::BODY_COMPONENT},
                 //: compaddbuttons.Trapezoidal
                 {.kind      = ComponentKind::TRAPEZOID_FIN_SET,
                  .text      = tr("Trapezoidal"),
                  .placement = Placement::INTO_SELECTED},
                 //: compaddbuttons.Elliptical
                 {.kind      = ComponentKind::ELLIPTICAL_FIN_SET,
                  .text      = tr("Elliptical"),
                  .placement = Placement::INTO_SELECTED},
                 //: compaddbuttons.Freeform
                 {.kind      = ComponentKind::FREEFORM_FIN_SET,
                  .text      = tr("Freeform"),
                  .placement = Placement::INTO_SELECTED},
                 //: compaddbuttons.Tubefin
                 {.kind      = ComponentKind::TUBE_FIN_SET,
                  .text      = tr("Tube Fins"),
                  .placement = Placement::INTO_SELECTED},
                 //: compaddbuttons.RailButton
                 {.kind      = ComponentKind::RAIL_BUTTON,
                  .text      = tr("Rail Button"),
                  .placement = Placement::INTO_SELECTED},
                 //: compaddbuttons.Launchlug
                 {.kind      = ComponentKind::LAUNCH_LUG,
                  .text      = tr("Launch Lug"),
                  .placement = Placement::INTO_SELECTED},
             },
             false);
    //: compaddbuttons.InnerComponent
    addGroup(*layout, tr("Inner Components"),
             {
                 //: compaddbuttons.Innertube
                 {.kind      = ComponentKind::INNER_TUBE,
                  .text      = tr("Inner Tube"),
                  .placement = Placement::INTO_SELECTED},
                 //: compaddbuttons.Coupler
                 {.kind      = ComponentKind::TUBE_COUPLER,
                  .text      = tr("Tube Coupler"),
                  .placement = Placement::INTO_SELECTED},
                 //: compaddbuttons.Centeringring
                 {.kind      = ComponentKind::CENTERING_RING,
                  .text      = tr("Centering\nRing"),
                  .placement = Placement::INTO_SELECTED},
                 //: compaddbuttons.Bulkhead
                 {.kind      = ComponentKind::BULKHEAD,
                  .text      = tr("Bulkhead"),
                  .placement = Placement::INTO_SELECTED},
                 //: compaddbuttons.Engineblock
                 {.kind      = ComponentKind::ENGINE_BLOCK,
                  .text      = tr("Engine\nBlock"),
                  .placement = Placement::INTO_SELECTED},
             },
             false);
    //: compaddbuttons.MassComponents
    addGroup(*layout, tr("Mass Components"),
             {
                 //: compaddbuttons.Parachute
                 {.kind      = ComponentKind::PARACHUTE,
                  .text      = tr("Parachute"),
                  .placement = Placement::INTO_SELECTED},
                 //: compaddbuttons.Streamer
                 {.kind      = ComponentKind::STREAMER,
                  .text      = tr("Streamer"),
                  .placement = Placement::INTO_SELECTED},
                 //: compaddbuttons.Shockcord
                 {.kind      = ComponentKind::SHOCK_CORD,
                  .text      = tr("Shock Cord"),
                  .placement = Placement::INTO_SELECTED},
                 //: compaddbuttons.MassComponent
                 {.kind      = ComponentKind::MASS_COMPONENT,
                  .text      = tr("Mass\nComponent"),
                  .placement = Placement::INTO_SELECTED},
             },
             false);
    // Java's last row: an empty panel that takes the rest of the height.
    layout->addStretch(1);
    uniformButtonSizes();
}

void ComponentAddButtons::addGroup(QVBoxLayout& layout, const QString& title,
                                   const std::vector<ButtonSpec>& specs, bool first)
{
    if (!first)
    {
        layout.addSpacing(kGroupGap);
    }
    auto* label = makeChild<QLabel>(this, title);
    label->setTextFormat(Qt::PlainText);
    layout.addWidget(label);
    m_titles.push_back(label);

    auto* group = makeChild<QWidget>(this);
    auto* flow  = makeChild<FlowLayout>(group, 0, kButtonGap, kButtonGap);
    for (const ButtonSpec& spec : specs)
    {
        auto* button = makeChild<ComponentButton>(group, *m_selection, m_services, spec.kind,
                                                  spec.text, spec.placement);
        connect(button, &ComponentButton::addedComponentEditRequested, this,
                &ComponentAddButtons::addedComponentEditRequested);
        flow->addWidget(button);
        m_buttons.push_back(button);
    }
    m_rows.push_back(flow);
    layout.addWidget(group);
}

ComponentButton* ComponentAddButtons::button(ComponentKind kind) const
{
    const auto found = std::ranges::find_if(
        m_buttons, [kind](const ComponentButton* button) { return button->kind() == kind; });
    return found == m_buttons.end() ? nullptr : *found;
}

QString ComponentAddButtons::title()
{
    //: BasicFrame.title.Addnewcomp
    return tr("Add new component");
}

QStringList ComponentAddButtons::groupTitles() const
{
    QStringList titles;
    for (const QLabel* title : m_titles)
    {
        titles.append(title->text());
    }
    return titles;
}

void ComponentAddButtons::changeEvent(QEvent* event)
{
    QWidget::changeEvent(event);
    if (event->type() == QEvent::StyleChange || event->type() == QEvent::FontChange)
    {
        uniformButtonSizes();
    }
}

void ComponentAddButtons::uniformButtonSizes()
{
    QSize size;
    for (const ComponentButton* button : m_buttons)
    {
        size = size.expandedTo(button->sizeHint());
    }
    m_buttonSize = size;
    for (ComponentButton* button : m_buttons)
    {
        button->setFixedSize(size);
    }
}

}  // namespace QtRocket::Gui
