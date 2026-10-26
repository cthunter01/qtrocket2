#pragma once

#include <array>
#include <chrono>
#include <map>

#include <QObject>
#include <QPoint>
#include <QRect>
#include <QSize>
#include <QString>
#include <QTimer>
#include <QWidget>
#include <Qt>

class QEvent;
class QHideEvent;
class QKeyEvent;
class QMouseEvent;
class QPaintEvent;
class QStyleOptionSlider;
class QWheelEvent;

namespace QtRocket::Gui
{

/// A slider with two thumbs on one track (OpenRocket's MultiSlider and MultiSliderUI,
/// swing gui/widgets/MultiSlider.java and MultiSliderUI.java, the GeoVISTA Center's JSlider with
/// several thumbs), as the motor chooser's filter panel makes its three: "Total Impulse" over the
/// impulse classes A to O, "Nominal Diameter" over the diameter stops, and the motor length from 0
/// to 1000 mm. Each thumb holds an int between the slider's minimum and maximum (a Swing
/// DefaultBoundedRangeModel each: a value outside is clamped into the range); thumb 0 starts as the
/// lower one, thumb 1 as the upper one. The slider is horizontal, its values growing to the right,
/// and bounded (OpenRocket sets all its sliders bounded, "thumbs cannot cross").
///
/// What moves a thumb, as Java's MultiSliderUI (probed against OpenRocket's own MultiSlider under
/// FlatLaf: tools/openrocket-probes/multi-slider):
/// - A drag of a thumb moves it between its neighbours: it may meet the other thumb (equal values)
///   but never pass it. A press on a thumb makes it the current thumb; where both thumbs are under
///   the mouse (equal or close values) the first one is current, and the first move of the drag
///   picks which of them it drags: a move to the left (or none) the first, a move to the right the
///   last.
/// - A press elsewhere (beside the thumbs, also in the labels under them) moves the thumb closest
///   to the mouse by one block towards it: the nearest thumb ending left of the mouse or starting
///   right of it (a thumb whose x range holds the mouse, when the press is below it, is neither),
///   the left one when both are as near. Held, the press repeats the block after kScrollDelay and
///   then every kScrollInterval, and stops once the thumb has reached the mouse, which may move
///   meanwhile. The block is a tenth of the maximum (MultiSliderUI's scrollByBlock(): 1 for 0..14,
///   100 for 0..1000).
/// - The keys move the current thumb: Right and Up one up, Left and Down one down, Page Up and Page
///   Down a block, Home and End to the minimum and the maximum.
/// - Any mouse button does it (MultiSliderUI never looks at the button); the wheel does nothing
///   (it goes on to the widgets around, so that a scroll area scrolls).
/// The keys, a held press and setValueAt() take a thumb past the other one, as in Java: only a
/// drag is bounded.
///
/// Java's change events come as stateChanged(): after every change of a thumb's value or of its
/// adjusting flag (true while a drag or a held press moves it), never for a value set again;
/// valueChanged() comes just before stateChanged() for a change of a value. A press on a thumb
/// changes nothing until the drag moves; a press beside them sets the moved thumb adjusting, then
/// moves it; the release clears the flag.
///
/// The slider draws the platform's slider through QStyle: the groove and the tick marks once, then
/// each thumb as a slider handle, the current thumb last (on top), shown with the focus while the
/// slider has it. The labels (Java's label table) are drawn in a row under the slider, each centred
/// under the value it labels.
///
/// Deviations from OpenRocket:
/// - The look and the geometry are the platform's (QStyle's slider handle, groove and tick marks),
///   not Swing's; a thumb is always drawn at its value's position, also during a drag (Swing leaves
///   it at the mouse until the next change).
/// - A drag gives the thumb the value of the mouse's position, kept between its neighbours' values:
///   a thumb dragged onto or past the other one takes its value. Java keeps the thumb's pixel
///   between its neighbours' and reads the value back from that pixel, which can stop short of the
///   neighbour or pass it where a pixel spans several values (the length slider: a thumb dragged
///   onto 251 reads 252).
/// - Disabling or hiding the slider ends a press (Java's release does nothing on a disabled slider,
///   which leaves the thumb adjusting); another button pressed while one is held does nothing.
/// - Only the horizontal, bounded, two-thumb slider OpenRocket uses: no vertical or inverted
///   slider, no other number of thumbs, no unbounded drag, no change of the range once made. It
///   stays left to right in a right-to-left layout.
/// - The labels use the slider's font; the size hints make room for every label without overlap.
class RangeSlider final : public QWidget
{
    Q_OBJECT

public:
    /// The number of thumbs.
    static constexpr int kThumbCount = 2;
    /// The wait before a held press repeats its block (Swing's scroll timer, probed).
    static constexpr std::chrono::milliseconds kScrollDelay{300};
    /// The time between the repeats of a held press (Swing's scroll timer, probed).
    static constexpr std::chrono::milliseconds kScrollInterval{100};
    /// The preferred length of the track and its thumbs (Swing's Slider.horizontalSize).
    static constexpr int kPreferredLength = 200;
    /// The minimum length (Swing's Slider.minimumHorizontalSize), when no label needs more.
    static constexpr int kMinimumLength = 36;

    /// A slider from @p minimum to @p maximum whose thumbs 0 and 1 hold @p value0 and @p value1
    /// (Java: new MultiSlider(HORIZONTAL, min, max, val1, val2) and setBounded(true)). Thumb 0 is
    /// the current thumb. No tick marks and no labels at first.
    /// @throws BugError when @p minimum exceeds @p maximum or a value lies outside them (Java's
    ///         DefaultBoundedRangeModel: "invalid range properties"); crossed values are taken.
    RangeSlider(int minimum, int maximum, int value0, int value1, QWidget* parent);
    ~RangeSlider() override;
    RangeSlider(const RangeSlider&)            = delete;
    RangeSlider& operator=(const RangeSlider&) = delete;
    RangeSlider(RangeSlider&&)                 = delete;
    RangeSlider& operator=(RangeSlider&&)      = delete;

    [[nodiscard]] int minimum() const noexcept { return m_minimum; }
    [[nodiscard]] int maximum() const noexcept { return m_maximum; }

    /// The value of @p thumb, 0 or 1 (getValueAt()).
    /// @throws BugError for another thumb
    [[nodiscard]] int valueAt(int thumb) const;
    /// Sets the value of @p thumb to @p value clamped into [minimum(), maximum()] (setValueAt():
    /// the other thumb is no bound). Emits valueChanged() and stateChanged() when the value
    /// changes.
    /// @throws BugError for another thumb
    void setValueAt(int thumb, int value);

    /// Whether a drag or a held press is moving @p thumb (getValueIsAdjustingAt()).
    /// @throws BugError for another thumb
    [[nodiscard]] bool isAdjusting(int thumb) const;

    /// The thumb the keys move (getCurrentThumbIndex()): 0 at first, then the thumb pressed or
    /// dragged last, or the one a press beside the thumbs moved.
    [[nodiscard]] int currentThumb() const noexcept { return m_current; }

    /// The step of Page Up, Page Down and a press beside the thumbs: maximum() / 10.
    [[nodiscard]] int blockIncrement() const noexcept { return m_maximum / 10; }

    /// The tick marks under the groove: one every @p interval values from the minimum, none for 0
    /// (Java: setMajorTickSpacing() with setPaintTicks(true)). None at first.
    void              setTickInterval(int interval);
    [[nodiscard]] int tickInterval() const noexcept { return m_tickInterval; }

    /// The labels under the slider, each a text under the value it is keyed by; none when empty
    /// (Java: setLabelTable() with setPaintLabels(true)). None at first.
    /// @throws BugError for a key outside [minimum(), maximum()]
    void                                        setLabels(std::map<int, QString> labels);
    [[nodiscard]] const std::map<int, QString>& labels() const noexcept { return m_labels; }

    /// Where @p thumb is drawn and can be grabbed: the style's slider handle at its value.
    /// @throws BugError for another thumb
    [[nodiscard]] QRect thumbRect(int thumb) const;
    /// The band of the groove, the thumbs and the tick marks: the slider the style draws, above
    /// the labels and between the margins that make room for the outermost labels.
    [[nodiscard]] QRect sliderRect() const;
    /// Where the label of @p value is drawn, centred under the value's position in the row under
    /// sliderRect(); an empty rectangle when @p value has no label.
    [[nodiscard]] QRect labelRect(int value) const;
    /// The x of the centre of a thumb at @p value (Java's xPositionForValue()), @p value clamped
    /// into the range.
    [[nodiscard]] int positionForValue(int value) const;
    /// The value whose thumb centre is closest to @p x (Java's valueForXPosition()): the minimum
    /// left of the track, the maximum right of it.
    [[nodiscard]] int valueForPosition(int x) const;

    /// kPreferredLength wide (wider when the labels need it), as high as the style's slider with
    /// its tick marks and the row of labels.
    [[nodiscard]] QSize sizeHint() const override;
    /// As high as sizeHint(); wide enough for kMinimumLength and for every label to fit beside its
    /// neighbours.
    [[nodiscard]] QSize minimumSizeHint() const override;

Q_SIGNALS:
    /// The value of @p thumb changed to @p value (before stateChanged()).
    void valueChanged(int thumb, int value);
    /// A thumb's value or its adjusting flag changed (Java's ChangeEvent of the slider).
    void stateChanged();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void changeEvent(QEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    bool event(QEvent* event) override;

private:
    /// The direction of a block or unit step: down (left) or up (right) (Java's NEGATIVE_SCROLL and
    /// POSITIVE_SCROLL).
    enum class Direction
    {
        NEGATIVE = -1,
        POSITIVE = 1,
    };

    /// How the style places the thumbs in the band (bandRoom()).
    struct BandRoom
    {
        int low;
        int high;
        int overhead;
    };

    /// The thumbs a press found under the mouse, the first and the last (minmaxIndices); -1 for
    /// none.
    struct PressedThumbs
    {
        int first = -1;
        int last  = -1;
    };

    /// The style option of the slider without its rectangle and with a handle at the minimum.
    void initBaseOption(QStyleOptionSlider& option) const;
    /// The style option of the slider band (sliderRect()) with a handle at @p position.
    void initStyleOption(QStyleOptionSlider& option, int position) const;
    /// How the style places the thumbs in the band: the room left of the centre of a thumb at the
    /// minimum, right of one at the maximum, and the band's width less the distance between them.
    [[nodiscard]] BandRoom bandRoom() const;
    /// The horizontal margin on each side of the slider band that makes room for the outermost
    /// labels.
    [[nodiscard]] int labelMargin() const;
    /// The height of the row of labels: 0 without labels.
    [[nodiscard]] int labelRowHeight() const;
    /// The height of the style's slider with its tick marks.
    [[nodiscard]] int sliderHeight() const;
    /// The width of the slider band at which every label has room beside its neighbours.
    [[nodiscard]] int labelledSliderWidth() const;
    /// The handle's rectangle at @p value.
    [[nodiscard]] QRect handleRectAt(int value) const;
    /// The value of a handle whose left edge is at @p left.
    [[nodiscard]] int valueForHandleLeft(int left) const;
    /// The thumb under @p point that a press on it grabs, by MultiSliderUI's findClosest(): the
    /// nearest thumb ending left of @p point's x or starting right of it.
    [[nodiscard]] int closestThumb(QPoint point) const;
    /// Whether a held press still moves the current thumb in @p direction: it has not reached the
    /// mouse and is not at the end of the range.
    [[nodiscard]] bool shouldScroll(Direction direction) const;

    void setAdjusting(int thumb, bool adjusting);
    void scrollByUnit(Direction direction);
    void scrollByBlock(Direction direction);
    /// A press on a thumb: grabs it for a drag.
    void pressThumbs(QPoint point, PressedThumbs pressed);
    /// A press beside the thumbs: a block towards the mouse, repeated while held.
    void pressTrack(QPoint point);
    /// The drag moved the mouse to @p point.
    void drag(QPoint point);
    /// A step of the held press.
    void repeatScroll();
    /// Ends a press: no drag, no repeat, the current thumb no longer adjusting.
    void endPress();
    /// The thumb under @p point for the hover highlight; -1 for none.
    void hover(QPoint point);

    int                    m_minimum;
    int                    m_maximum;
    std::array<int, 2>     m_values;
    std::array<bool, 2>    m_adjusting{false, false};
    int                    m_current      = 0;
    int                    m_tickInterval = 0;
    std::map<int, QString> m_labels;
    QTimer                 m_scrollTimer;
    Direction              m_scrollDirection = Direction::POSITIVE;
    /// Whether a mouse button is down on the slider, and which.
    bool            m_pressed     = false;
    Qt::MouseButton m_pressButton = Qt::NoButton;
    /// Whether the press grabbed a thumb (isDragging).
    bool          m_dragging = false;
    PressedThumbs m_pressedThumbs;
    /// Where the press was (firstXY) and where the mouse is (currentMouseX/Y).
    QPoint m_pressPoint;
    QPoint m_mouse;
    /// The press's x less the left edge of the last thumb under it (offset).
    int m_offset     = 0;
    int m_hoverThumb = -1;
};

}  // namespace QtRocket::Gui
