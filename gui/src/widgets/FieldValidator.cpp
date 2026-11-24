#include "QtRocket/gui/widgets/FieldValidator.h"

#include <algorithm>
#include <utility>

#include <QApplication>
#include <QColor>
#include <QEvent>
#include <QFrame>
#include <QLabel>
#include <QLatin1Char>
#include <QLineEdit>
#include <QObject>
#include <QPaintEvent>
#include <QPainter>
#include <QPalette>
#include <QPen>
#include <QPoint>
#include <QRectF>
#include <QSize>
#include <QString>
#include <QStringList>
#include <QStyle>
#include <QTimer>
#include <QToolTip>
#include <QVariant>
#include <QWidget>
#include <Qt>
#include <QtContainerFwd>
#include <QtTypes>

#include "QtRocket/gui/binding/Binding.h"
#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/util/ThemeColors.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Signal.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket::Gui
{

namespace
{

/// The field's property that holds its outline (Java: the client property JComponent.outline).
constexpr const char* kOutlineProperty = "outline";
/// The vertical gap between the field and its popup, px (Java: location.y + height + 2).
constexpr int kPopupGap = 2;
/// FlatLaf's Component.arc, the diameter of the corners of a spinner's or a combo box's frame, px
/// (a text field's, TextComponent.arc, is 0).
constexpr double kArc = 5.0;
/// The frame's width, px: FlatLaf's Component.borderWidth, and with the focus its inner outline
/// (FlatLightLaf's Component.innerOutlineWidth 1; FlatDarculaLaf draws a 2 pixel focus ring).
constexpr double kFrameWidth        = 1.0;
constexpr double kFocusedFrameWidth = 2.0;

/// The values of the outline property (Java: Outline.getFlatLafValue()).
QString warningValue()
{
    return QStringLiteral("warning");
}

QString errorValue()
{
    return QStringLiteral("error");
}

/// The object name of the child that draws a field's outline.
QString frameName()
{
    return QStringLiteral("fieldOutline");
}

/// Java's String.trim(): without the leading and trailing characters at or below U+0020.
QString javaTrimmed(const QString& text)
{
    qsizetype begin = 0;
    qsizetype end   = text.size();
    while (begin < end && text.at(begin).unicode() <= u' ')
    {
        ++begin;
    }
    while (end > begin && text.at(end - 1).unicode() <= u' ')
    {
        --end;
    }
    return text.mid(begin, end - begin);
}

/// The tooltip of @p message (Java: formatTooltipText()): an HTML text kept as it is, any other
/// escaped (StringUtils.escapeHtml, line breaks as <br>) inside <html>...</html>; empty for a
/// blank message.
QString toolTipHtml(const QString& message)
{
    QString trimmed = javaTrimmed(message);
    if (trimmed.isEmpty())
    {
        return {};
    }
    if (trimmed.startsWith(QStringLiteral("<html"), Qt::CaseInsensitive))
    {
        return trimmed;
    }
    return QStringLiteral("<html>") +
           QtStrings::fromUtf8(Strings::escapeHtml(QtStrings::toUtf8(trimmed))) +
           QStringLiteral("</html>");
}

/// Where a popup of @p size goes in @p window, @p field's window: below the field at its left edge
/// (Java: location.x, location.y + height + 2); above it when there is no room below and there is
/// above, and moved left as far as it must to end inside the window, but not beyond its left edge.
[[nodiscard]] QPoint popupPosition(const QWidget& field, const QWidget& window, QSize size)
{
    const QPoint top = field.mapTo(&window, QPoint(0, 0));
    int          y   = top.y() + field.height() + kPopupGap;
    if (const int above = top.y() - kPopupGap - size.height();
        y + size.height() > window.height() && above >= 0)
    {
        y = above;
    }
    const int x = std::max(0, std::min(top.x(), window.width() - size.width()));
    return {x, y};
}

/// Whether @p field or a child of it has the keyboard focus.
[[nodiscard]] bool hasFocusWithin(const QWidget& field)
{
    const QWidget* focus = QApplication::focusWidget();
    return focus != nullptr && (focus == &field || field.isAncestorOf(focus));
}

/// Draws its parent's outline (FieldValidator::setOutline()) over the parent's edge: a child of
/// the field as large as it, on top of its other children, that lets the mouse and the focus
/// through.
class OutlineFrame final : public QWidget
{
public:
    explicit OutlineFrame(QWidget* field) : QWidget(field)
    {
        QTROCKET_ASSERT(field != nullptr);
        setObjectName(frameName());
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setAttribute(Qt::WA_NoSystemBackground);
        setFocusPolicy(Qt::NoFocus);
        setGeometry(field->rect());
        field->installEventFilter(this);
        // The focus decides the colour; a spin box's or a combo box's focus is its editor's.
        connect(qobject_cast<QApplication*>(QCoreApplication::instance()),
                &QApplication::focusChanged, this, [this] { update(); });
        raise();
    }

    /// Follows the field's size, and stays on top of the field's children.
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (watched == parentWidget())
        {
            if (event->type() == QEvent::Resize)
            {
                setGeometry(parentWidget()->rect());
            }
            else if (event->type() == QEvent::ChildAdded)
            {
                raise();
            }
        }
        return QWidget::eventFilter(watched, event);
    }

protected:
    void paintEvent(QPaintEvent* /*event*/) override
    {
        const QWidget&                field   = *parentWidget();
        const FieldValidator::Outline outline = FieldValidator::outlineOf(field);
        if (outline == FieldValidator::Outline::NONE)
        {
            return;
        }
        const bool   focused = hasFocusWithin(field);
        const double width   = focused ? kFocusedFrameWidth : kFrameWidth;
        QPainter     painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(
            QPen(FieldValidator::outlineColor(outline, focused, field.palette()), width));
        painter.setBrush(Qt::NoBrush);
        const double inset  = width / 2;
        const QRectF frame  = QRectF(rect()).adjusted(inset, inset, -inset, -inset);
        const double radius = qobject_cast<const QLineEdit*>(&field) != nullptr ? 0.0 : kArc / 2;
        painter.drawRoundedRect(frame, radius, radius);
    }
};

/// @p field, which must not be null.
[[nodiscard]] QWidget& checkedField(QWidget* field)
{
    if (field == nullptr)
    {
        bug("A field validator needs a field");
    }
    return *field;
}

}  // namespace

FieldValidator& FieldValidator::of(QWidget& field)
{
    return *makeChild<FieldValidator>(&field);
}

FieldValidator::FieldValidator(QWidget* field)
  : QObject(&checkedField(field)), m_field(field), m_popupTimer(makeChild<QTimer>(this))
{
    m_popupTimer->setSingleShot(true);
    connect(m_popupTimer, &QTimer::timeout, this, [this] { hideMessagePopup(); });
    // The popup goes when the field is hidden (Java's popup goes with the component's window).
    m_field->installEventFilter(this);
}

FieldValidator::~FieldValidator()
{
    // The field may be half destroyed (the validator is its child): its tooltip is left alone.
    m_sources.clear();
    hideMessagePopup();
}

// ------------------------------------------------------------------------------- conditions

FieldValidator& FieldValidator::addCondition(Outline outline, Predicate predicate, Message message)
{
    if (predicate == nullptr)
    {
        bug("A validation condition needs a predicate");
    }
    m_conditions.push_back(Condition{
        .outline = outline, .predicate = std::move(predicate), .message = std::move(message)});
    return *this;
}

FieldValidator& FieldValidator::warnIf(Predicate predicate, Message message)
{
    return addCondition(Outline::WARNING, std::move(predicate), std::move(message));
}

FieldValidator& FieldValidator::warnIf(Predicate predicate, const QString& message)
{
    return warnIf(std::move(predicate), [message] { return message; });
}

FieldValidator& FieldValidator::errorIf(Predicate predicate, Message message)
{
    return addCondition(Outline::ERROR, std::move(predicate), std::move(message));
}

FieldValidator& FieldValidator::errorIf(Predicate predicate, const QString& message)
{
    return errorIf(std::move(predicate), [message] { return message; });
}

FieldValidator& FieldValidator::showMessagePopup(int durationMs)
{
    m_popupDuration = std::max(0, durationMs);
    return *this;
}

// -------------------------------------------------------------------------------- listening

bool FieldValidator::isListening(const void* source) const noexcept
{
    return std::ranges::any_of(m_sources, [source](const Listened& listened) {
        return listened.source == source && listened.connection.connected();
    });
}

FieldValidator& FieldValidator::listen(const void* source, Signal<>::Connection connection)
{
    // A source that went may have left its address to a new one: its entry is dropped.
    std::erase_if(m_sources,
                  [](const Listened& listened) { return !listened.connection.connected(); });
    m_sources.push_back(Listened{.source     = source,
                                 .connection = Signal<>::ScopedConnection(std::move(connection))});
    update();
    return *this;
}

FieldValidator& FieldValidator::listenTo(Binding& binding)
{
    if (isListening(&binding))
    {
        update();
        return *this;
    }
    return listen(&binding, binding.addChangeListener([this] {
        Guard::run([this] { update(); }, m_field->objectName());
    }));
}

FieldValidator& FieldValidator::listenTo(Signal<>& signal)
{
    if (isListening(&signal))
    {
        update();
        return *this;
    }
    return listen(&signal, signal.connect([this] {
        Guard::run([this] { update(); }, m_field->objectName());
    }));
}

// ---------------------------------------------------------------------------------- update

void FieldValidator::update()
{
    if (m_closed)
    {
        return;
    }
    Outline     outline = Outline::NONE;
    QStringList messages;
    for (const Condition& condition : m_conditions)
    {
        if (!condition.predicate())
        {
            continue;
        }
        if (condition.outline > outline)
        {
            outline = condition.outline;
            messages.clear();
        }
        if (condition.outline == outline && condition.message != nullptr)
        {
            const QString text = javaTrimmed(condition.message());
            if (!text.isEmpty())
            {
                messages.append(text);
            }
        }
    }
    setOutline(*m_field, outline);
    const QString message = messages.join(QLatin1Char('\n'));
    setValidationToolTip(message);

    if (outline == Outline::NONE || message.isEmpty())
    {
        hideMessagePopup();
    }
    else if (m_popupDuration > 0 && (m_lastOutline == Outline::NONE || m_lastMessage != message))
    {
        showPopup(message);
    }
    m_lastOutline = outline;
    m_lastMessage = message;
}

void FieldValidator::setValidationToolTip(const QString& message)
{
    const QString text = toolTipHtml(message);
    if (text.isEmpty())
    {
        if (m_baseToolTip.has_value())
        {
            m_field->setToolTip(*m_baseToolTip);
            m_baseToolTip.reset();
        }
        return;
    }
    if (!m_baseToolTip.has_value())
    {
        m_baseToolTip = m_field->toolTip();
    }
    m_field->setToolTip(text);
}

void FieldValidator::close()
{
    if (m_closed)
    {
        return;
    }
    m_closed = true;
    hideMessagePopup();
    setValidationToolTip({});
    m_sources.clear();
}

// ----------------------------------------------------------------------------------- popup

void FieldValidator::showPopup(const QString& message)
{
    hideMessagePopup();
    if (!m_field->isVisible())
    {
        return;
    }
    const QString text = toolTipHtml(message);
    if (text.isEmpty())
    {
        return;
    }
    // A child of the field's window, as Java's lightweight popup is a component of the window's
    // layered pane: no window of its own.
    QWidget& window = *m_field->window();
    auto*    popup  = makeChild<QLabel>(&window, text);
    popup->setObjectName(QStringLiteral("messagePopup"));
    popup->setAttribute(Qt::WA_TransparentForMouseEvents);
    popup->setFocusPolicy(Qt::NoFocus);
    popup->setTextFormat(Qt::RichText);
    popup->setPalette(QToolTip::palette());
    popup->setFont(QToolTip::font());
    popup->setForegroundRole(QPalette::ToolTipText);
    popup->setBackgroundRole(QPalette::ToolTipBase);
    popup->setAutoFillBackground(true);
    popup->setFrameStyle(QFrame::Box | QFrame::Plain);
    popup->setMargin(
        1 + popup->style()->pixelMetric(QStyle::PM_ToolTipLabelFrameWidth, nullptr, popup));
    popup->adjustSize();
    popup->move(popupPosition(*m_field, window, popup->size()));
    m_popup = popup;
    popup->raise();
    popup->show();
    m_popupTimer->start(m_popupDuration);
}

void FieldValidator::hideMessagePopup()
{
    m_popupTimer->stop();
    if (QLabel* popup = m_popup.data(); popup != nullptr)
    {
        m_popup = nullptr;
        popup->hide();
        popup->deleteLater();
    }
}

bool FieldValidator::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_field && event->type() == QEvent::Hide)
    {
        hideMessagePopup();
    }
    return QObject::eventFilter(watched, event);
}

// --------------------------------------------------------------------------------- outlines

void FieldValidator::setOutline(QWidget& field, Outline outline)
{
    switch (outline)
    {
        case Outline::NONE:
            field.setProperty(kOutlineProperty, QVariant());
            break;
        case Outline::WARNING:
            field.setProperty(kOutlineProperty, warningValue());
            break;
        case Outline::ERROR:
            field.setProperty(kOutlineProperty, errorValue());
            break;
    }
    auto* frame = field.findChild<QWidget*>(frameName(), Qt::FindDirectChildrenOnly);
    if (outline == Outline::NONE)
    {
        if (frame != nullptr)
        {
            frame->hide();
        }
        return;
    }
    if (frame == nullptr)
    {
        frame = makeChild<OutlineFrame>(&field);
    }
    frame->setGeometry(field.rect());
    frame->raise();
    frame->show();
    frame->update();
}

FieldValidator::Outline FieldValidator::outlineOf(const QWidget& field)
{
    const QString value = field.property(kOutlineProperty).toString();
    if (value == errorValue())
    {
        return Outline::ERROR;
    }
    if (value == warningValue())
    {
        return Outline::WARNING;
    }
    return Outline::NONE;
}

QColor FieldValidator::outlineColor(Outline outline, bool focused, const QPalette& palette)
{
    // FlatLaf 3.7.2 under OpenRocket's themes (UIManager.getColor() of FlatLightLaf and
    // FlatDarculaLaf): Component.warning.borderColor, .focusedBorderColor, and the error's.
    const bool dark = ThemeColors::theme(palette) == ThemeColors::Theme::DARK;
    switch (outline)
    {
        case Outline::NONE:
            return {};
        case Outline::WARNING:
            if (dark)
            {
                return focused ? QColor(0xac, 0x79, 0x20) : QColor(0x72, 0x56, 0x27);
            }
            return focused ? QColor(0xe2, 0xa5, 0x3a) : QColor(0xfe, 0xd2, 0x84);
        case Outline::ERROR:
            if (dark)
            {
                return focused ? QColor(0x8b, 0x3c, 0x3c) : QColor(0x72, 0x55, 0x55);
            }
            return focused ? QColor(0xe5, 0x3e, 0x4d) : QColor(0xeb, 0xb8, 0xbc);
    }
    QTROCKET_UNREACHABLE();
}

}  // namespace QtRocket::Gui
