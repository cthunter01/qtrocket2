#pragma once

#include <QObject>
#include <QSize>
#include <QSplitter>
#include <Qt>

class QEvent;
class QResizeEvent;
class QShowEvent;
class QWidget;

namespace QtRocket::Gui
{

/// A splitter of two sides that behaves as Swing's JSplitPane does in OpenRocket's windows (the
/// main window's split between the tabs and the rocket's figure, the design panel's between the
/// component tree and the add buttons):
/// - its divider is placed at dividerLocation() of its length when it is first shown with a
///   length (JSplitPane.setDividerLocation(double): (int) ((length - divider size) *
///   location) before the divider), or, when its window is being shown, when the window's own
///   show event comes, after every layout of the window has settled (BasicFrame places its
///   dividers once the frame is validated; a tab widget lays its pages out only in its own show
///   event, after theirs);
/// - after that, a change of its length is shared between the sides by resizeWeight(), the first
///   side's part (JSplitPane.setResizeWeight(): BasicSplitPaneUI's distributeSpace(), where a
///   QSplitter alone shares it in proportion to the sides' sizes); a side that would go below its
///   minimum takes what it lacks from the other one, if that one keeps its own minimum;
/// - its sides cannot be collapsed (JSplitPane keeps their minimum sizes too).
/// A side's minimum is the one QSplitter keeps: its minimum size when one is set, else nothing
/// for a side whose size policy is Ignored, else its minimum size hint (or its size hint when its
/// policy does not let it shrink).
class SplitPane final : public QSplitter
{
    Q_OBJECT

public:
    /// A split pane of @p orientation whose first side takes @p resizeWeight (0 to 1) of a
    /// change of its length and whose divider goes to @p dividerLocation (0 to 1) of its length
    /// when it is first shown.
    SplitPane(Qt::Orientation orientation, double resizeWeight, double dividerLocation,
              QWidget* parent);

    /// The first side's part of a change of the length (JSplitPane.getResizeWeight()).
    [[nodiscard]] double resizeWeight() const noexcept { return m_resizeWeight; }

    /// Where the divider goes, as a part of the length, when the split pane is first shown.
    [[nodiscard]] double dividerLocation() const noexcept { return m_dividerLocation; }

    /// Whether the divider has been placed at dividerLocation() (see the class comment); until
    /// then a change of the length is the layout's own.
    [[nodiscard]] bool isDividerPlaced() const noexcept { return m_dividerPlaced; }

    /// Places the divider when the window's show event comes, if it waits for it.
    bool eventFilter(QObject* watched, QEvent* event) override;

protected:
    /// Shares a change of the length between the sides by the resize weight, before QSplitter
    /// lays them out.
    void resizeEvent(QResizeEvent* event) override;
    /// Places the divider the first time (or waits for the window's show event).
    void showEvent(QShowEvent* event) override;

private:
    /// Places the divider at dividerLocation() of the length, once, when there are two sides
    /// and a length.
    void placeDivider();
    /// The length of the splitter or of @p size along the orientation.
    [[nodiscard]] int lengthOf(const QSize& size) const;
    /// The length below which the side @p index does not go (JSplitPane's
    /// getMinimumSizeOfComponent()).
    [[nodiscard]] int sideMinimum(int index) const;
    /// BasicSplitPaneUI.distributeSpace(): the sides' lengths after the length of the splitter
    /// changed by @p change.
    void distributeSpace(int change);

    double m_resizeWeight;
    double m_dividerLocation;
    bool   m_dividerPlaced{false};
    bool   m_waitingForWindow{false};
};

}  // namespace QtRocket::Gui
