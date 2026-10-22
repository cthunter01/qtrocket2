#pragma once

#include <array>
#include <span>
#include <vector>

#include <QObject>
#include <QPoint>
#include <QPointF>
#include <QPointer>
#include <QSize>
#include <QWidget>
#include <Qt>

#include "QtRocket/figure/FigureView.h"
#include "QtRocket/gui/tree/DocumentSelectionModel.h"
// RocketComponent is complete here, not declared: moc's metatype of a signal's
// RocketComponent* checks for completeness, and GCC's -Wsfinae-incomplete rejects a class that
// another moc file of the same compilation sees complete later.
#include "QtRocket/rocket/RocketComponent.h"

class QCheckBox;
class QComboBox;
class QEvent;
class QGridLayout;
class QLabel;
class QMouseEvent;

namespace QtRocket
{
class OpenRocketDocument;
class Preferences;
}  // namespace QtRocket

namespace QtRocket::Gui
{

class ConfigurationComboBox;
class DesignPhysics;
class RocketFigure;
class RulerUnitSelector;
class ScaleScrollArea;
class ScaleSelector;
class ScrollAreaZoomModel;
class SingleRowScrollArea;
class StageSelector;
class ViewRotationControl;

/// The design view of a document, below the design window's tabs (OpenRocket's RocketPanel, its
/// 2D part): the rocket's figure with its controls, the CG, the CP and the information overlay.
///
/// Layout (createPanel(), MigLayout "[shrink][grow]", "[shrink 0][grow][shrink 0]", preferred
/// size 800 x 300):
/// - the ribbon across the top, in a SingleRowScrollArea: "View Type:" over the view combo box
///   (Top view, Side view, Back view; Side view at first); the zoom out button, "Zoom:" over the
///   zoom combo box, the zoom in and zoom to fit buttons (ScaleSelector, with the zoom relative
///   to the fit, 100 % fitting the rocket into the view); "Show CG/CP" (checked at first); a
///   vertical separator; "Stages:" over the stage buttons (StageSelector); "Flight
///   configuration:" over the configuration combo box (ConfigurationComboBox, at least 100
///   pixels wide);
/// - on the left the rotation control (ViewRotationControl);
/// - the figure (RocketFigure) in a ScaleScrollArea with rulers, fitted at first, at least
///   300 x 100 pixels, the unit selector of the rulers in their corner (RulerUnitSelector);
/// - the footer below the figure, 25 pixels in, in a SingleRowScrollArea: the hint
///   RocketPanel.lbl.infoMessage, 3 points smaller, and "Show warnings" (the document's
///   preference RocketPanel.showWarnings, true when unset, written back when it is toggled).
/// The numbers, the carets and the overlay are a DesignPhysics of the figure, computed whenever
/// the figure caught up with the rocket's change events; the scroll area fits the figure again
/// then (Java's componentResized() in the rocket's listener).
///
/// The mouse over the figure (the scroll pane's mouse listeners of RocketPanel):
/// - A left drag rotates the figure about the rocket's axis by the vertical distance from the
///   press: dragRotation() (dy / the scroll area's height * pi, modulo 2 pi, snapped with Shift),
///   unless the rotation lock is on: then it pans, as a middle or right drag always does (the
///   scroll area's pan buttons).
/// - A click (a press and its release with no drag in between) on no component clears the
///   selection, whatever the button. Otherwise, with the components there in the figure's order
///   (RocketFigure::componentsAt(), the topmost first):
///   - left: selects the first one, unless it is selected already; with Shift or Ctrl (Cmd on
///     macOS) adds the first one that is not selected, or removes the first one when all are;
///   - left, the second click of a double-click: asks for the configuration dialog
///     (editRequested()) of the first one, selected alone, when it was selected (else it is only
///     selected); with Shift or Ctrl and a selection, adds the first clicked one that is not
///     selected and asks for the dialog of it (or of the last selected one, when all are) with
///     the rest of the selection as the others of a multi-edit;
///   - right: selects the topmost one when none of them is selected (the user's decision U4:
///     OpenRocket ends on the last, the lowest), then asks for the component popup menu at the
///     mouse (componentPopupRequested());
///   - middle: nothing.
/// - The wheel scrolls (the scroll area's); the figure takes no keys.
/// The selection is the window's (setSelectionModel(): the tree's DocumentSelectionModel); what
/// the figure draws as selected is setHighlightedComponents()'s, which the window keeps up to
/// date with its design panel (DesignPanel::highlightedComponents(), which adds the components
/// below a selected assembly, as OpenRocket's DesignPanel does to its figure).
///
/// The document and the preferences must outlive the panel; the selection model may go first.
///
/// Deviations from OpenRocket:
/// - The 3D views, the pan mode and its button (Milestone 6), the calipers, "View settings" and
///   "Capture" (Milestone 5), the CP conditions of the component analysis (Milestone 7) and the
///   background simulation with the flight data of the overlay (Milestone 4) are not here.
/// - A double-click is Qt's: the platform's interval and distance, and the button's own clicks
///   only, where Java counts the clicks of every button in the 600 ms after the first
///   (CustomClickCountListener); a third quick click is a click of its own here (Java: a
///   double-click again).
/// - A click is a press and a release of the same button less than the platform's drag distance
///   apart (Java: AWT's mouseClicked, a press and a release at the same point on most platforms).
/// - The component popup and the configuration dialog are asked for by signals (decision O12):
///   the design window connects them to its RocketActions' popup and to
///   MainWindow::requestConfigDialog() (Java calls BasicFrame.doComponentTreePopup() and
///   ComponentConfigDialog.showDialog() with config listeners).
/// - The view combo box and the zoom combo box ignore the mouse wheel, which scrolls the ribbon
///   (a Swing JComboBox has no wheel listener).
class RocketPanel final : public QWidget
{
    Q_OBJECT

public:
    /// setPreferredSize(new Dimension(800, 300)).
    static constexpr QSize kPreferredSize{800, 300};
    /// The figure's least size ("wmin 300lp, hmin 100lp").
    static constexpr QSize kFigureMinimumSize{300, 100};
    /// The footer's indent ("gapleft 25").
    static constexpr int kFooterIndent = 25;
    /// The configuration combo box's least width ("wmin 100").
    static constexpr int kConfigurationMinimumWidth = 100;
    /// The separator's height relative to the ribbon's (0.7 * ribbon.getPreferredSize().height).
    static constexpr double kSeparatorHeight = 0.7;
    /// The views of the view combo box, in its order (VIEW_TYPE.getAvailableViewTypes(false)).
    static constexpr std::array kViewTypes{FigureView::TOP, FigureView::SIDE, FigureView::BACK};

    /// The design view of @p document, its figure at the dots per inch of @p preferences and the
    /// rotation lock read from them.
    RocketPanel(OpenRocketDocument& document, Preferences& preferences, QWidget* parent);
    ~RocketPanel() override;
    RocketPanel(const RocketPanel&)            = delete;
    RocketPanel& operator=(const RocketPanel&) = delete;
    RocketPanel(RocketPanel&&)                 = delete;
    RocketPanel& operator=(RocketPanel&&)      = delete;

    /// The rotation a left drag gives (handleMouseDragged()): @p originalRotation, the figure's at
    /// the press, less the vertical distance @p dy from the press over the scroll area's
    /// @p height times pi, plus 2 pi, modulo 2 pi (Java's %, so a drag of more than twice the
    /// height downwards gives a negative angle); snapped (ViewRotationControl::snapRotation())
    /// when @p snap (Shift held).
    [[nodiscard]] static double dragRotation(double originalRotation, int dy, int height,
                                             bool snap) noexcept;

    [[nodiscard]] OpenRocketDocument& document() const noexcept { return *m_document; }
    [[nodiscard]] RocketFigure&       figure() const noexcept { return *m_figure; }
    [[nodiscard]] ScaleScrollArea&    scrollArea() const noexcept { return *m_scrollArea; }
    /// The numbers, the carets and the overlay (extraCG, extraCP, extraText).
    [[nodiscard]] DesignPhysics&       physics() const noexcept { return *m_physics; }
    [[nodiscard]] ViewRotationControl& rotationControl() const noexcept { return *m_rotation; }
    /// The view combo box ("viewSelector").
    [[nodiscard]] QComboBox&     viewSelector() const noexcept { return *m_viewSelector; }
    [[nodiscard]] ScaleSelector& scaleSelector() const noexcept { return *m_scaleSelector; }
    /// "Show CG/CP" ("showCGCP").
    [[nodiscard]] QCheckBox&             showCgCpCheckBox() const noexcept { return *m_showCgCp; }
    [[nodiscard]] StageSelector&         stageSelector() const noexcept { return *m_stages; }
    [[nodiscard]] ConfigurationComboBox& configurationSelector() const noexcept
    {
        return *m_configurations;
    }
    /// The hint in the footer ("infoMessage").
    [[nodiscard]] QLabel& infoMessage() const noexcept { return *m_infoMessage; }
    /// "Show warnings" ("showWarnings").
    [[nodiscard]] QCheckBox& showWarningsCheckBox() const noexcept { return *m_showWarnings; }
    [[nodiscard]] SingleRowScrollArea& ribbonScrollArea() const noexcept { return *m_ribbonScroll; }
    [[nodiscard]] SingleRowScrollArea& footerScrollArea() const noexcept { return *m_footerScroll; }
    [[nodiscard]] RulerUnitSelector&   rulerUnitSelector() const noexcept { return *m_rulerUnit; }

    /// The view shown (getCurrentViewType()).
    [[nodiscard]] FigureView viewType() const noexcept;
    /// Shows @p view (setViewType()): through the view combo box, as the user's choice is made.
    void setViewType(FigureView view);

    /// The window's component selection, which the clicks change (setSelectionModel()); null
    /// (the default) for none: a click then changes nothing.
    void                                  setSelectionModel(DocumentSelectionModel* selection);
    [[nodiscard]] DocumentSelectionModel* selectionModel() const noexcept
    {
        return m_selection.data();
    }
    /// The components the figure draws as selected (RocketFigure::setSelection(); only their ids
    /// are kept).
    void setHighlightedComponents(std::span<RocketComponent* const> components);

    /// Computes the numbers again and repaints (updateExtras()): after a change that fires no
    /// event of the rocket, such as the preferences' default Mach number.
    void updateExtras();
    /// Brings the figure up to date (updateFigures()).
    void updateFigures();
    /// The rulers' unit to the default length unit (updateRulers(): after the preferences).
    void updateRulers();

    [[nodiscard]] QSize sizeHint() const override;

    /// The mouse over the figure, the wheel over the combo boxes of the ribbon.
    bool eventFilter(QObject* watched, QEvent* event) override;

Q_SIGNALS:
    /// The component popup menu is asked for at @p globalPosition (a right-click on a component,
    /// after the selection: BasicFrame.doComponentTreePopup()).
    void componentPopupRequested(const QPoint& globalPosition);
    /// The configuration dialog of @p component is asked for, with @p others, the other selected
    /// components of a multi-edit (none for the component alone). The pointers are valid during
    /// the emission only: connect directly.
    void editRequested(QtRocket::RocketComponent*                     component,
                       const std::vector<QtRocket::RocketComponent*>& others);

private:
    /// A press of a mouse button on the figure, until its release.
    struct Press
    {
        Qt::MouseButton button = Qt::NoButton;
        /// Where it was pressed, in the viewport's coordinates (Java's mousePressedLoc).
        QPoint viewportPosition;
        /// Whether it is the second press of a double-click.
        bool doubleClick = false;
        /// Whether the mouse moved beyond the drag distance since: then it is no click.
        bool moved = false;
        /// Whether the press rotates the figure (dragRotating), and the rotation it started at.
        bool   rotating         = false;
        double originalRotation = 0.0;
    };

    /// Makes the ribbon's controls in @p layout.
    void makeRibbon(QWidget& ribbon, QGridLayout& layout);
    /// Makes the footer's controls in @p footer.
    void makeFooter(QWidget& footer);
    /// The view combo box's choice of the view at @p index (applyViewTypeSelection()).
    void viewTypeChosen(int index);
    /// The rotation lock changed: a left drag pans or rotates.
    void updatePanButtons();

    /// The figure's mouse events; true when the event was taken.
    bool figureMouseEvent(QEvent& event);
    bool figurePressed(QMouseEvent& event);
    bool figureMoved(QMouseEvent& event);
    bool figureReleased(QMouseEvent& event);
    /// Where @p event happened in the viewport's coordinates.
    [[nodiscard]] QPoint viewportPosition(const QMouseEvent& event) const;

    /// A click of @p button at @p pixel of the figure (handleMouseClick()); @p clickCount 2 for
    /// the second click of a double-click.
    void click(Qt::MouseButton button, int clickCount, Qt::KeyboardModifiers modifiers,
               QPointF pixel, QPoint globalPosition);
    /// handleSingleComponentClick().
    void singleClick(const std::vector<RocketComponent*>& clicked, bool multiple);
    /// handleDoubleComponentClick().
    void doubleClick(const std::vector<RocketComponent*>& clicked, bool multiple);
    /// handleComponentPopupTrigger(), with the decision U4.
    void popupClick(const std::vector<RocketComponent*>& clicked, QPoint globalPosition);

    OpenRocketDocument*              m_document;
    Preferences*                     m_preferences;
    RocketFigure*                    m_figure{nullptr};
    ScaleScrollArea*                 m_scrollArea{nullptr};
    RulerUnitSelector*               m_rulerUnit{nullptr};
    DesignPhysics*                   m_physics{nullptr};
    ViewRotationControl*             m_rotation{nullptr};
    QComboBox*                       m_viewSelector{nullptr};
    ScrollAreaZoomModel*             m_zoomModel{nullptr};
    ScaleSelector*                   m_scaleSelector{nullptr};
    QCheckBox*                       m_showCgCp{nullptr};
    StageSelector*                   m_stages{nullptr};
    ConfigurationComboBox*           m_configurations{nullptr};
    QLabel*                          m_infoMessage{nullptr};
    QCheckBox*                       m_showWarnings{nullptr};
    SingleRowScrollArea*             m_ribbonScroll{nullptr};
    SingleRowScrollArea*             m_footerScroll{nullptr};
    QPointer<DocumentSelectionModel> m_selection;
    Press                            m_press;
};

}  // namespace QtRocket::Gui
