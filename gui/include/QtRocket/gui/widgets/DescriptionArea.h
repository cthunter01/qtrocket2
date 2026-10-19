#pragma once

#include <QObject>
#include <QSize>
#include <QString>
#include <QTextBrowser>
#include <QUrl>

class QColor;
class QFont;
class QWidget;

namespace QtRocket::Gui
{

/// A read-only area of HTML text, a given number of rows high and scrolling vertically when the
/// text is longer, never horizontally (OpenRocket's DescriptionArea): the configuration
/// dialogs' component descriptions and notes. The text is shown in the area's font changed by
/// a number of points (one point smaller by default). A link opens in the system's browser
/// (URLUtil.openWebpage(): QDesktopServices::openUrl()), after linkActivated() is emitted.
///
/// Its preferred height is OpenRocket's: the height of one line times the rows, plus what one
/// line has beyond its line height, plus 2 (and the frame).
///
/// Deviations from OpenRocket: a link into OpenRocket's jar (a "jar:" URL, which OpenRocket
/// copies to a temporary file first) has no counterpart; QtRocket's own texts have no links
/// before the About and License dialogs (Milestone 5). setTextFont(null) (honour the display
/// properties) is what the area does anyway.
class DescriptionArea : public QTextBrowser
{
    Q_OBJECT

public:
    /// The size change OpenRocket's constructors without a size use: one point smaller.
    static constexpr double kDefaultSize = -1.0;

    DescriptionArea(int rows, QWidget* parent);
    DescriptionArea(int rows, double size, QWidget* parent);
    DescriptionArea(const QString& text, int rows, QWidget* parent);
    DescriptionArea(const QString& text, int rows, double size, QWidget* parent);
    /// @p opaque false: the background is the window's (a JPanel's), not the text field's.
    DescriptionArea(const QString& text, int rows, double size, bool opaque, QWidget* parent);

    /// Shows the HTML text @p text (plain text is read as HTML too, as Java's text/html pane
    /// does), scrolled to the top (Java's setText(); QTextEdit::setText() guesses the format).
    void setDescription(const QString& text);

    /// The text last given to setDescription().
    [[nodiscard]] const QString& description() const noexcept { return m_text; }

    /// The size change of the text, in points.
    [[nodiscard]] double textSize() const noexcept { return m_size; }

    /// The font the text is shown in, before the size change (setTextFont()).
    void setTextFont(const QFont& font);

    /// The text colour and the background (setForeground(), setBackground()).
    void setForegroundColor(const QColor& color);
    void setBackgroundColor(const QColor& color);

    /// Whether a link opens in the system's browser (true at first); false leaves it to the
    /// receivers of linkActivated().
    void               setOpensLinks(bool opensLinks) noexcept { m_opensLinks = opensLinks; }
    [[nodiscard]] bool opensLinks() const noexcept { return m_opensLinks; }

    /// Scrolls back to the beginning of the text.
    void scrollToTop();

    /// The text browser's width, OpenRocket's height for the rows.
    [[nodiscard]] QSize sizeHint() const override;

Q_SIGNALS:
    /// A link of the text was clicked.
    void linkActivated(const QUrl& url);

private:
    /// The height of @p rows rows in the current font (DescriptionArea's constructor).
    [[nodiscard]] int heightOfRows(int rows) const;
    /// The text's font: the area's with the size change.
    [[nodiscard]] QFont textFont() const;
    void                openLink(const QUrl& url);

    double  m_size;
    int     m_rows;
    QString m_text;
    bool    m_opensLinks = true;
};

}  // namespace QtRocket::Gui
