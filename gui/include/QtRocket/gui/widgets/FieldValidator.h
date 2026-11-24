#pragma once

#include <functional>
#include <optional>
#include <vector>

#include <QColor>
#include <QObject>
#include <QPointer>
#include <QString>

#include "QtRocket/util/Signal.h"

class QLabel;
class QPalette;
class QTimer;
class QWidget;

namespace QtRocket::Gui
{

class Binding;

/// The validation of one field (OpenRocket's FlatLafOutlines and its Validator, swing
/// gui/util/FlatLafOutlines.java): conditions on the field's inputs, each a warning (warnIf())
/// or an error (errorIf()) with an optional message; the most severe condition that holds
/// outlines the field (Outline: NONE < WARNING < ERROR), the messages of that severity replace
/// the field's tooltip, and, with showMessagePopup(), a tooltip-like popup below the field shows
/// them for a while when the field becomes invalid or its message changes. The launch
/// conditions' fields (temperature, pressure, wind speed, rod angle) and the random seed field
/// use it.
///
/// update() evaluates the conditions in the order they were added (Java: updateNow()): the
/// outline is the highest severity among those that hold, the message their messages of that
/// severity, each trimmed, the blank ones left out, joined with line breaks. listenTo() calls
/// update() whenever a binding or a signal tells of a change, and once at once; a widget's own
/// signal (a line edit's text) is connected by the caller, who calls update() (Java's document
/// listener does).
///
/// The tooltip: while there is a message, the field's tooltip is the message as HTML (escaped,
/// line breaks as <br>, inside <html>...</html>; a message that starts with "<html" is taken as
/// it is), and the tooltip the field had before comes back once there is none (Java: the client
/// property BASE_TOOLTIP_PROPERTY).
///
/// The popup (showMessagePopup(), Java's transient message): when an update finds a message and
/// the field had none or another one, a label in the tooltip's palette and font shows it below
/// the field (2 pixels below its bottom edge, at its left edge, as Java places its Popup) for
/// the given time, then goes. It is a child of the field's window, on top of the window's other
/// children and letting the mouse through, not a window of its own (Java's PopupFactory shows a
/// lightweight popup, a component of the window, when it fits in the window; a QToolTip would be
/// a top-level window, which the GUI tests refuse to leave behind and the offscreen platform
/// warns of). It is deleted when it is hidden: none is left behind once the time is up, the field
/// is hidden, the message goes, hideMessagePopup() is called or the validator goes. It is shown
/// only while the field is visible (Java: isShowing()).
///
/// The outline (setOutline(), outlineOf()): a property of the field ("outline": "warning",
/// "error" or none, Java's client property JComponent.outline), drawn by a child widget over the
/// field's edge that lets the mouse through: a 1 pixel frame in the outline's border colour, 2
/// pixels in its focused colour while the field or a child of it has the focus, with rounded
/// corners (FlatLaf's Component.arc 5) except on a line edit (TextComponent.arc 0). The colours
/// are FlatLaf's Component.error.borderColor, .focusedBorderColor, Component.warning.borderColor
/// and .focusedBorderColor of OpenRocket's light (FlatLightLaf) and dark (FlatDarculaLaf) themes,
/// by ThemeColors::theme() of the field's palette (outlineColor()). Code may set an outline
/// without a validator (the CSV import dialog's column fields, Java's
/// FlatLafOutlines.setOutline()).
///
/// Lifetime: the validator is a child of the field and goes with it (Java closes its validator
/// when the component is no longer displayable). close() ends it: it hides the popup, gives the
/// field its own tooltip back and stops listening; the outline stays, as in Java. Its destructor
/// only stops listening and hides the popup: the field may be half destroyed by then.
///
/// Deviations from OpenRocket:
/// - The outline is drawn over the platform style's frame of the field, where FlatLaf draws the
///   field's own border in the outline's colour (QtRocket has no FlatLaf, Milestone 5's theme
///   selector aside), and its width and corners approximate FlatLaf's focus ring and arcs.
/// - The popup is a label in the tooltip's palette and font with a plain frame, where Java shows
///   the look and feel's JToolTip. It stays inside the field's window: where Java would show a
///   heavyweight popup beyond the window's edge, it goes above the field when there is no room
///   below, and moves left to end inside the window.
/// - The tooltip is the field's own; Java sets it on a spinner's editor and text field too (a
///   Qt child without a tooltip shows its parent's).
/// - update() runs on the GUI thread only: Java hands a call from another thread to the EDT.
class FieldValidator final : public QObject
{
    Q_OBJECT

public:
    /// The outlines (Java: FlatLafOutlines.Outline), in increasing severity.
    enum class Outline
    {
        /// No outline.
        NONE,
        /// The warning outline (yellow).
        WARNING,
        /// The error outline (red).
        ERROR,
    };

    /// Whether a condition holds now.
    using Predicate = std::function<bool()>;
    /// The message of a condition, read when it holds; an empty or blank one shows nothing.
    using Message = std::function<QString()>;

    /// The time every OpenRocket validator shows its popup, ms (showMessagePopup(2500)).
    static constexpr int kMessagePopupDuration = 2500;

    /// A new validator of @p field, its child (see the class comment), without conditions (Java:
    /// FlatLafOutlines.validator()): FieldValidator::of(*spinBox).warnIf(...).listenTo(...).
    [[nodiscard]] static FieldValidator& of(QWidget& field);

    /// A validator of @p field, its child, without conditions: made by of() (or makeChild()).
    /// @throws BugError when @p field is null
    explicit FieldValidator(QWidget* field);
    ~FieldValidator() override;
    FieldValidator(const FieldValidator&)            = delete;
    FieldValidator& operator=(const FieldValidator&) = delete;
    FieldValidator(FieldValidator&&)                 = delete;
    FieldValidator& operator=(FieldValidator&&)      = delete;

    /// Adds a warning condition, with the message @p message or none (Java: warnIf()).
    /// @throws BugError when @p predicate is empty
    FieldValidator& warnIf(Predicate predicate, Message message = nullptr);
    /// Adds a warning condition with the fixed message @p message.
    /// @throws BugError when @p predicate is empty
    FieldValidator& warnIf(Predicate predicate, const QString& message);

    /// Adds an error condition, with the message @p message or none (Java: errorIf()).
    /// @throws BugError when @p predicate is empty
    FieldValidator& errorIf(Predicate predicate, Message message = nullptr);
    /// Adds an error condition with the fixed message @p message.
    /// @throws BugError when @p predicate is empty
    FieldValidator& errorIf(Predicate predicate, const QString& message);

    /// Shows the message in a popup for @p durationMs milliseconds when the field becomes invalid
    /// or its message changes; 0 (and a negative time) shows none, the default (Java:
    /// showMessagePopup()).
    FieldValidator& showMessagePopup(int durationMs);

    /// Updates on every change @p binding tells of (Binding::addChangeListener()), and now
    /// (Java: listenTo(ChangeSource...)). A binding listened to already is skipped. The
    /// connection goes with the validator or the binding, whichever goes first.
    FieldValidator& listenTo(Binding& binding);

    /// Updates on every emission of @p signal (a core object's changed()), and now. A signal
    /// listened to already is skipped.
    FieldValidator& listenTo(Signal<>& signal);

    /// Evaluates the conditions and shows the result (see the class comment). Nothing once the
    /// validator is closed.
    void update();

    /// Ends the validator (see the class comment). Nothing when it is closed already.
    void close();

    [[nodiscard]] bool isClosed() const noexcept { return m_closed; }

    /// The outline of the last update(); NONE before the first.
    [[nodiscard]] Outline outline() const noexcept { return m_lastOutline; }

    /// The message of the last update() (its messages joined with "\n"); empty when there was
    /// none.
    [[nodiscard]] const QString& message() const noexcept { return m_lastMessage; }

    /// The time the popup is shown, ms; 0 for none.
    [[nodiscard]] int messagePopupDuration() const noexcept { return m_popupDuration; }

    /// The popup, a child of the field's window, while it is shown; null otherwise.
    [[nodiscard]] QLabel* messagePopup() const noexcept { return m_popup.data(); }

    /// Hides the popup now (and deletes it); nothing when none is shown.
    void hideMessagePopup();

    /// Outlines @p field with @p outline (NONE removes it), with or without a validator (Java:
    /// FlatLafOutlines.setOutline()).
    static void setOutline(QWidget& field, Outline outline);

    /// The outline of @p field; NONE when it has none (Java: getOutline()).
    [[nodiscard]] static Outline outlineOf(const QWidget& field);

    /// The colour of @p outline's frame in @p palette's theme, focused or not (see the class
    /// comment); an invalid colour for NONE.
    [[nodiscard]] static QColor outlineColor(Outline outline, bool focused,
                                             const QPalette& palette);

    /// Hides the popup when the field is hidden; the events always go on (false).
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    struct Condition
    {
        Outline   outline;
        Predicate predicate;
        Message   message;
    };

    /// A source listened to: its address, compared only, and the connection.
    struct Listened
    {
        const void*                source;
        Signal<>::ScopedConnection connection;
    };

    FieldValidator&    addCondition(Outline outline, Predicate predicate, Message message);
    FieldValidator&    listen(const void* source, Signal<>::Connection connection);
    [[nodiscard]] bool isListening(const void* source) const noexcept;
    void               setValidationToolTip(const QString& message);
    void               showPopup(const QString& message);

    QWidget*               m_field;
    std::vector<Condition> m_conditions;
    std::vector<Listened>  m_sources;
    int                    m_popupDuration{0};
    Outline                m_lastOutline{Outline::NONE};
    QString                m_lastMessage;
    /// The field's own tooltip while the validator's message replaces it.
    std::optional<QString> m_baseToolTip;
    QPointer<QLabel>       m_popup;
    QTimer*                m_popupTimer;
    bool                   m_closed{false};
};

}  // namespace QtRocket::Gui
