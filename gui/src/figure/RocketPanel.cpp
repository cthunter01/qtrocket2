#include "QtRocket/gui/figure/RocketPanel.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iterator>
#include <numbers>
#include <span>
#include <vector>

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QEvent>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLayout>
#include <QMouseEvent>
#include <QObject>
#include <QPoint>
#include <QPointF>
#include <QSize>
#include <QSizePolicy>
#include <QSpacerItem>
#include <QString>
#include <QToolButton>
#include <QWidget>
#include <Qt>

#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/figure/FigureView.h"
#include "QtRocket/gui/figure/DesignPhysics.h"
#include "QtRocket/gui/figure/RocketFigure.h"
#include "QtRocket/gui/figure/RulerUnitSelector.h"
#include "QtRocket/gui/figure/ScaleScrollArea.h"
#include "QtRocket/gui/figure/ScaleSelector.h"
#include "QtRocket/gui/figure/ScrollAreaZoomModel.h"
#include "QtRocket/gui/figure/ViewRotationControl.h"
#include "QtRocket/gui/tree/DocumentSelectionModel.h"
#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/widgets/ConfigurationComboBox.h"
#include "QtRocket/gui/widgets/SingleRowScrollArea.h"
#include "QtRocket/gui/widgets/StageSelector.h"
#include "QtRocket/gui/widgets/StyledLabel.h"
#include "QtRocket/preferences/DocumentPreferences.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/BugError.h"

namespace QtRocket::Gui
{

namespace
{

/// MigLayout's paragraph gap ("gapleft para", "gapright para") between the groups of the ribbon.
constexpr int kParagraphGap = 12;
/// The info message's size relative to the default font (new StyledLabel(..., -3)).
constexpr double kInfoMessageSize = -3.0;

/// The context of the panel's reports (Guard).
QString context()
{
    return QStringLiteral("Rocket design view");
}

/// Whether @p components holds @p component (Java: List.contains(), by identity).
[[nodiscard]] bool holds(const std::vector<RocketComponent*>& components,
                         const RocketComponent*               component)
{
    return std::ranges::find(components, component) != components.end();
}

/// A spacer of @p width pixels for a gap of the ribbon's grid.
void addGap(QGridLayout& layout, int column, int width)
{
    layout.addItem(makeQtOwned<QSpacerItem>(width, 0, QSizePolicy::Fixed, QSizePolicy::Minimum), 1,
                   column);
}

/// The index of @p view in the view combo box (RocketPanel::kViewTypes).
[[nodiscard]] int viewIndex(FigureView view)
{
    for (std::size_t index = 0; index < RocketPanel::kViewTypes.size(); ++index)
    {
        if (RocketPanel::kViewTypes.at(index) == view)
        {
            return static_cast<int>(index);
        }
    }
    bug("A view that the design view does not show");
}

/// Moves @p widget from the layout of @p from into @p layout at @p row, @p column.
void moveInto(QWidget& from, QWidget& widget, QGridLayout& layout, int row, int column)
{
    from.layout()->removeWidget(&widget);
    layout.addWidget(&widget, row, column);
}

}  // namespace

RocketPanel::RocketPanel(OpenRocketDocument& document, Preferences& preferences, QWidget* parent)
  : QWidget(parent),
    m_document(&document),
    m_preferences(&preferences),
    // The figure in its scroll area with the rulers' unit selector in the corner; addExtras():
    // the carets and the overlay; the rotation control.
    m_figure(makeChild<RocketFigure>(this, document.getRocket(), preferences)),
    m_scrollArea(makeChild<ScaleScrollArea>(this, m_figure)),
    m_rulerUnit(RulerUnitSelector::install(*m_scrollArea)),
    m_physics(makeChild<DesignPhysics>(this, *m_figure, preferences)),
    m_rotation(makeChild<ViewRotationControl>(this, *m_figure, preferences))
{
    setObjectName(QStringLiteral("rocketPanel"));

    // The figure fitted (scrollPane.setFitting(true)), and fitted again whenever the figure
    // caught up with the rocket and the numbers were computed (the rocket listener's
    // scrollPane.componentResized()).
    m_scrollArea->setObjectName(QStringLiteral("scrollPane"));
    m_scrollArea->setMinimumSize(kFigureMinimumSize);
    m_scrollArea->setFitting(true);
    connect(m_figure, &RocketFigure::rocketChanged, m_scrollArea,
            guarded([this] { m_scrollArea->updateScaling(); }, context()));

    connect(m_rotation, &ViewRotationControl::dragRotationLockChanged, this,
            [this] { updatePanButtons(); });
    updatePanButtons();

    // The ribbon ("insets 0, fill, hidemode 2") and the footer ("fillx, gapy 0, ins 0"), each
    // in a single row scroll pane.
    auto* ribbon = makeChild<QWidget>(this);
    ribbon->setObjectName(QStringLiteral("ribbon"));
    auto* ribbonLayout = makeChild<QGridLayout>(ribbon);
    makeRibbon(*ribbon, *ribbonLayout);
    m_ribbonScroll = makeChild<SingleRowScrollArea>(this, ribbon);
    m_ribbonScroll->setObjectName(QStringLiteral("ribbonScroll"));
    auto* footer = makeChild<QWidget>(this);
    footer->setObjectName(QStringLiteral("bottomRow"));
    makeFooter(*footer);
    auto* footerHolder = makeChild<QWidget>(this);
    m_footerScroll     = makeChild<SingleRowScrollArea>(footerHolder, footer);
    m_footerScroll->setObjectName(QStringLiteral("bottomRowScroll"));
    auto* footerLayout = makeChild<QHBoxLayout>(footerHolder);
    footerLayout->setContentsMargins(kFooterIndent, 0, 0, 0);
    footerLayout->addWidget(m_footerScroll);

    // createPanel(): MigLayout "", "[shrink][grow]", "[shrink 0][grow][shrink 0]": the ribbon
    // ("growx, span"), the rotation control ("growx, growy") and the figure ("grow"), the footer
    // ("skip, growx, gapleft 25").
    auto* layout = makeChild<QGridLayout>(this);
    layout->addWidget(m_ribbonScroll, 0, 0, 1, 2);
    layout->addWidget(m_rotation, 1, 0);
    layout->addWidget(m_scrollArea, 1, 1);
    layout->addWidget(footerHolder, 2, 1);
    layout->setColumnStretch(1, 1);
    layout->setRowStretch(1, 1);

    m_figure->installEventFilter(this);
    m_viewSelector->installEventFilter(this);
    m_scaleSelector->scaleCombo()->installEventFilter(this);
}

RocketPanel::~RocketPanel()
{
    m_figure->removeEventFilter(this);
    m_viewSelector->removeEventFilter(this);
    m_scaleSelector->scaleCombo()->removeEventFilter(this);
}

void RocketPanel::makeRibbon(QWidget& ribbon, QGridLayout& layout)
{
    layout.setContentsMargins(0, 0, 0, 0);
    Rocket& rocket = m_document->getRocket();

    // cell 0: the view type.
    //: RocketPanel.lbl.ViewType
    layout.addWidget(makeChild<QLabel>(&ribbon, tr("View Type:")), 0, 0);
    m_viewSelector = makeChild<QComboBox>(&ribbon);
    m_viewSelector->setObjectName(QStringLiteral("viewSelector"));
    //: RocketPanel.FigTypeAct.TopView
    m_viewSelector->addItem(tr("Top view"));
    //: RocketPanel.FigTypeAct.SideView
    m_viewSelector->addItem(tr("Side view"));
    //: RocketPanel.FigTypeAct.BackView
    m_viewSelector->addItem(tr("Back view"));
    // VIEW_TYPE.getDefaultViewType(): the side view.
    m_viewSelector->setCurrentIndex(viewIndex(FigureView::SIDE));
    connect(m_viewSelector, &QComboBox::currentIndexChanged, this,
            guarded([this](int index) { viewTypeChosen(index); }, context()));
    layout.addWidget(m_viewSelector, 1, 0);

    // cells 1-3: the zoom (the scale relative to the fit), its controls taken from the selector
    // into the ribbon as Java's RocketPanel takes them: zoom out ("gapleft para"), "Zoom:" over
    // the combo box ("spanx 2"), zoom in and zoom to fit ("split 3"; the pan button is
    // Milestone 6's).
    m_zoomModel     = makeChild<ScrollAreaZoomModel>(this, m_scrollArea,
                                                     ScrollAreaZoomModel::Kind::RELATIVE_TO_FIT);
    m_scaleSelector = makeChild<ScaleSelector>(&ribbon, m_zoomModel);
    m_scaleSelector->setObjectName(QStringLiteral("scaleSelector"));
    m_scaleSelector->hide();
    addGap(layout, 1, kParagraphGap);
    moveInto(*m_scaleSelector, *m_scaleSelector->zoomOutButton(), layout, 1, 2);
    //: RocketPanel.lbl.Zoom
    layout.addWidget(makeChild<QLabel>(&ribbon, tr("Zoom:")), 0, 3, 1, 2);
    moveInto(*m_scaleSelector, *m_scaleSelector->scaleCombo(), layout, 1, 3);
    moveInto(*m_scaleSelector, *m_scaleSelector->zoomInButton(), layout, 1, 4);
    moveInto(*m_scaleSelector, *m_scaleSelector->zoomFitButton(), layout, 1, 5);

    // cell 4: Show CG/CP ("gapleft para"; the calipers below it are Milestone 5's).
    addGap(layout, 6, kParagraphGap);
    //: RocketPanel.checkbox.ShowCGCP
    m_showCgCp = makeChild<QCheckBox>(&ribbon, tr("Show CG/CP"));
    m_showCgCp->setObjectName(QStringLiteral("showCGCP"));
    m_showCgCp->setChecked(true);
    //: RocketPanel.checkbox.ShowCGCP.ttip
    m_showCgCp->setToolTip(
        tr("Disabling this checkbox hides the CG and CP markings in the rocket view."));
    connect(m_showCgCp, &QCheckBox::toggled, this,
            guarded(
                [this](bool checked) {
                    m_figure->setDrawCarets(checked);
                    updateFigures();
                },
                context()));
    layout.addWidget(m_showCgCp, 0, 7);

    // cell 6: the vertical separator ("spany 2, gapright para"), 0.7 of the ribbon's height so
    // far.
    auto* separator = makeChild<QFrame>(&ribbon);
    separator->setObjectName(QStringLiteral("ribbonSeparator"));
    separator->setFrameShape(QFrame::VLine);
    separator->setFrameShadow(QFrame::Sunken);
    separator->setFixedHeight(static_cast<int>(kSeparatorHeight * layout.sizeHint().height()));
    layout.addWidget(separator, 0, 8, 2, 1, Qt::AlignVCenter);
    addGap(layout, 9, kParagraphGap);

    // cell 7: the stages ("pushx": the column takes the room left).
    //: RocketPanel.lbl.Stages
    layout.addWidget(makeChild<QLabel>(&ribbon, tr("Stages:")), 0, 10);
    m_stages = makeChild<StageSelector>(&ribbon, rocket);
    m_stages->setObjectName(QStringLiteral("stageSelector"));
    layout.addWidget(m_stages, 1, 10);
    layout.setColumnStretch(10, 1);

    // cell 8: the flight configuration ("width 16%, wmin 100").
    //: RocketPanel.lbl.Flightcfg
    layout.addWidget(makeChild<QLabel>(&ribbon, tr("Flight configuration:")), 0, 11);
    m_configurations = makeChild<ConfigurationComboBox>(&ribbon, rocket, *m_preferences);
    m_configurations->setObjectName(QStringLiteral("configComboBox"));
    m_configurations->setMinimumWidth(kConfigurationMinimumWidth);
    layout.addWidget(m_configurations, 1, 11);
}

void RocketPanel::makeFooter(QWidget& footer)
{
    auto* layout = makeChild<QHBoxLayout>(&footer);
    layout->setContentsMargins(0, 0, 0, 0);

    // The hint ("wmin 0, growx, pushx"); "View settings" and "Capture" are Milestone 5's.
    //: RocketPanel.lbl.infoMessage
    m_infoMessage = makeChild<StyledLabel>(
        &footer,
        tr("<html>Click to select &nbsp;&nbsp; Shift+click to select other &nbsp;&nbsp; "
           "Double-click to edit &nbsp;&nbsp; Click+drag to move"),
        kInfoMessageSize);
    m_infoMessage->setObjectName(QStringLiteral("infoMessage"));
    layout->addWidget(m_infoMessage, 1);

    // Show warnings: the document's preference.
    //: RocketPanel.check.showWarnings
    m_showWarnings = makeChild<QCheckBox>(&footer, tr("Show warnings"));
    m_showWarnings->setObjectName(QStringLiteral("showWarnings"));
    m_showWarnings->setChecked(m_document->getDocumentPreferences().getBoolean(
        DocumentPreferences::kPrefShowWarnings, true));
    //: RocketPanel.check.showWarnings.ttip
    m_showWarnings->setToolTip(tr("Show/hide geometry warnings in the rocket design view."));
    m_physics->setShowWarnings(m_showWarnings->isChecked());
    connect(m_showWarnings, &QCheckBox::toggled, this,
            guarded(
                [this](bool checked) {
                    m_document->getDocumentPreferences().putBoolean(
                        DocumentPreferences::kPrefShowWarnings, checked);
                    m_physics->setShowWarnings(checked);
                },
                context()));
    layout->addWidget(m_showWarnings);
}

double RocketPanel::dragRotation(double originalRotation, int dy, int height, bool snap) noexcept
{
    // handleMouseDragged(), operation for operation.
    const double rotationOffset = (static_cast<double>(dy) / height) * std::numbers::pi;
    double       rotation       = originalRotation - rotationOffset;
    // Ensure the rotation is within the range [0, 2*PI] (Java's %: the sign of the dividend).
    rotation = std::fmod(rotation + (2 * std::numbers::pi), 2 * std::numbers::pi);
    if (snap)
    {
        rotation = ViewRotationControl::snapRotation(rotation);
    }
    return rotation;
}

FigureView RocketPanel::viewType() const noexcept
{
    return m_figure->view();
}

void RocketPanel::setViewType(FigureView view)
{
    m_viewSelector->setCurrentIndex(viewIndex(view));
}

void RocketPanel::viewTypeChosen(int index)
{
    if (index < 0 || static_cast<std::size_t>(index) >= kViewTypes.size())
    {
        return;
    }
    // applyViewTypeSelection(): the view, the numbers again (the carets of the back view are
    // cleared), the rulers' unit to the default.
    m_figure->setView(kViewTypes.at(static_cast<std::size_t>(index)));
    updateExtras();
    updateRulers();
}

void RocketPanel::updatePanButtons()
{
    // shouldPanOnDrag(): the middle and the right button always, the left one while the rotation
    // lock is on.
    Qt::MouseButtons buttons = Qt::MiddleButton | Qt::RightButton;
    if (m_rotation->isDragRotationLocked())
    {
        buttons |= Qt::LeftButton;
    }
    m_scrollArea->setPanButtons(buttons);
}

void RocketPanel::setSelectionModel(DocumentSelectionModel* selection)
{
    m_selection = selection;
}

void RocketPanel::setHighlightedComponents(std::span<RocketComponent* const> components)
{
    const std::vector<const RocketComponent*> highlighted(components.begin(), components.end());
    m_figure->setSelection(highlighted);
}

void RocketPanel::updateExtras()
{
    m_physics->update();
}

void RocketPanel::updateFigures()
{
    m_figure->updateFigure();
}

void RocketPanel::updateRulers()
{
    m_scrollArea->updateRulerUnit();
}

QSize RocketPanel::sizeHint() const
{
    return kPreferredSize;
}

bool RocketPanel::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_figure)
    {
        bool taken = false;
        Guard::run([this, event, &taken] { taken = figureMouseEvent(*event); }, context());
        return taken;
    }
    if (event->type() == QEvent::Wheel &&
        (watched == m_viewSelector || watched == m_scaleSelector->scaleCombo()))
    {
        // A Swing combo box takes no wheel: on to the ribbon's scroll pane.
        event->ignore();
        return true;
    }
    return QWidget::eventFilter(watched, event);
}

bool RocketPanel::figureMouseEvent(QEvent& event)
{
    if (event.type() == QEvent::ContextMenu)
    {
        // The right button's click asks for the popup (Java's mouseClicked()); the platform's
        // context menu event goes no further, where a window would show a menu of its own.
        event.accept();
        return true;
    }
    auto* const mouse = dynamic_cast<QMouseEvent*>(&event);
    if (mouse == nullptr)
    {
        return false;
    }
    switch (event.type())
    {
        case QEvent::MouseButtonPress:
        case QEvent::MouseButtonDblClick:
            return figurePressed(*mouse);
        case QEvent::MouseMove:
            return figureMoved(*mouse);
        case QEvent::MouseButtonRelease:
            return figureReleased(*mouse);
        default:
            return false;
    }
}

bool RocketPanel::figurePressed(QMouseEvent& event)
{
    if (m_press.button != Qt::NoButton && m_press.button != event.button() &&
        event.buttons().testFlag(m_press.button))
    {
        return false;  // another button is down: this one is neither a click nor a drag
    }
    // A press whose release never came (the mouse released elsewhere) is forgotten.
    m_press = Press{.button           = event.button(),
                    .viewportPosition = viewportPosition(event),
                    .doubleClick      = event.type() == QEvent::MouseButtonDblClick};
    // shouldRotateOnDrag(): the left button, the rotation lock off; any other press is the scroll
    // area's to pan (its pan buttons), as the figure leaves it unaccepted.
    if (event.button() == Qt::LeftButton && !m_rotation->isDragRotationLocked())
    {
        m_press.rotating         = true;
        m_press.originalRotation = m_figure->rotation();
        event.accept();
        return true;
    }
    return false;
}

bool RocketPanel::figureMoved(QMouseEvent& event)
{
    if (m_press.button == Qt::NoButton)
    {
        return false;
    }
    const QPoint position = viewportPosition(event);
    if ((position - m_press.viewportPosition).manhattanLength() >=
        QApplication::startDragDistance())
    {
        m_press.moved = true;
    }
    if (!m_press.rotating)
    {
        return false;
    }
    event.accept();
    // handleMouseDragged(): nothing while the lock is on.
    if (!m_rotation->isDragRotationLocked())
    {
        const int dy = position.y() - m_press.viewportPosition.y();
        m_figure->setRotation(dragRotation(m_press.originalRotation, dy, m_scrollArea->height(),
                                           event.modifiers().testFlag(Qt::ShiftModifier)));
    }
    return true;
}

bool RocketPanel::figureReleased(QMouseEvent& event)
{
    if (m_press.button == Qt::NoButton || event.button() != m_press.button)
    {
        return false;
    }
    const Press press = m_press;
    m_press           = Press{};
    if (!press.moved)
    {
        click(press.button, press.doubleClick ? 2 : 1, event.modifiers(), event.position(),
              event.globalPosition().toPoint());
    }
    if (press.rotating)
    {
        event.accept();
        return true;
    }
    return false;
}

QPoint RocketPanel::viewportPosition(const QMouseEvent& event) const
{
    return m_figure->mapToParent(event.position().toPoint());
}

void RocketPanel::click(Qt::MouseButton button, int clickCount, Qt::KeyboardModifiers modifiers,
                        QPointF pixel, QPoint globalPosition)
{
    // handleMouseClick(): the components at the point, the topmost first.
    const std::vector<RocketComponent*> clicked   = m_figure->componentsAt(pixel);
    DocumentSelectionModel*             selection = m_selection.data();
    if (selection == nullptr)
    {
        return;
    }
    if (clicked.empty())
    {
        selection->clearComponentSelection();
        return;
    }
    // handleComponentMouseClick().
    if (button == Qt::RightButton)
    {
        popupClick(clicked, globalPosition);
        return;
    }
    if (button != Qt::LeftButton)
    {
        return;
    }
    // GUIUtil.isMenuShortcutDown(): Qt's Control is the Command key on macOS.
    const bool multiple =
        modifiers.testFlag(Qt::ShiftModifier) || modifiers.testFlag(Qt::ControlModifier);
    if (clickCount >= 2)
    {
        doubleClick(clicked, multiple);
    }
    else
    {
        singleClick(clicked, multiple);
    }
}

void RocketPanel::singleClick(const std::vector<RocketComponent*>& clicked, bool multiple)
{
    DocumentSelectionModel&             selection = *m_selection;
    const std::vector<RocketComponent*> selected  = selection.selectedComponents();
    if (!multiple)
    {
        // Select the first clicked component, unless it is selected already.
        if (!holds(selected, clicked.front()))
        {
            selection.setSelectedComponent(*clicked.front());
        }
        return;
    }
    // Add the first clicked component that is not selected; when all are, take the first out.
    std::vector<RocketComponent*> paths      = selected;
    const auto                    unselected = std::ranges::find_if(
        clicked,
        [&selected](const RocketComponent* component) { return !holds(selected, component); });
    if (unselected != clicked.end())
    {
        paths.push_back(*unselected);
    }
    else
    {
        std::erase(paths, clicked.front());
    }
    selection.setSelectedComponents(paths);
}

void RocketPanel::doubleClick(const std::vector<RocketComponent*>& clicked, bool multiple)
{
    DocumentSelectionModel&             selection = *m_selection;
    const std::vector<RocketComponent*> selected  = selection.selectedComponents();
    if (!selected.empty() && multiple)
    {
        // A multi-edit: the last selected component, or the first clicked one that is not
        // selected, which joins the selection; the rest of the selection are the others.
        std::vector<RocketComponent*> current    = selected;
        RocketComponent*              component  = selected.back();
        const auto                    unselected = std::ranges::find_if(
            clicked, [&selected](const RocketComponent* c) { return !holds(selected, c); });
        if (unselected != clicked.end())
        {
            current.push_back(*unselected);
            selection.setSelectedComponents(current);
            current   = selection.selectedComponents();
            component = *unselected;
        }
        std::vector<RocketComponent*> others;
        std::ranges::copy_if(current, std::back_inserter(others),
                             [component](const RocketComponent* c) { return c != component; });
        Q_EMIT editRequested(component, others);
        return;
    }
    // A double-click on a component that was not selected only selects it; on a selected one,
    // the selection becomes that component alone and its dialog is asked for.
    RocketComponent* first       = clicked.front();
    const bool       wasSelected = holds(selected, first);
    selection.setSelectedComponent(*first);
    if (wasSelected)
    {
        Q_EMIT editRequested(first, {});
    }
}

void RocketPanel::popupClick(const std::vector<RocketComponent*>& clicked, QPoint globalPosition)
{
    DocumentSelectionModel&             selection       = *m_selection;
    const std::vector<RocketComponent*> selected        = selection.selectedComponents();
    const bool                          clickedSelected = std::ranges::any_of(
        clicked, [&selected](const RocketComponent* c) { return holds(selected, c); });
    if (!clickedSelected)
    {
        // The user's decision U4: the topmost clicked component. OpenRocket selects each clicked
        // one in turn and so ends on the last, the lowest (the body tube under an inner tube).
        selection.setSelectedComponent(*clicked.front());
    }
    Q_EMIT componentPopupRequested(globalPosition);
}

}  // namespace QtRocket::Gui
