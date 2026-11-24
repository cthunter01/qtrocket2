#pragma once

#include <QString>
#include <QWidget>

class QEvent;
class QPaintEvent;
class QToolButton;

namespace QtRocket::Gui
{

/// A section with a full-width header that shows or hides its content (OpenRocket's
/// CollapsiblePanel, swing gui/components/CollapsiblePanel.java): the recovery warnings'
/// "Single Deployment Warnings" (expanded) and "Dual Deployment Warnings" (collapsed) sections.
///
/// The header is a checkable tool button (Java: a JToggleButton in FlatLaf's tool bar button
/// style, its accessible name the title): auto-raised, as wide as the panel, its text the title
/// in the label font made bold and one point smaller (at least 9 points), beside the style's
/// disclosure arrow, right when collapsed and down when expanded (Java: the look and feel's
/// Tree.collapsedIcon and Tree.expandedIcon). Clicking it, or setExpanded(), shows or hides the
/// content; the layout then gives the content's space back or takes it (Java invalidates the
/// layout up to the scroll pane, which Qt's layouts do themselves).
///
/// The panel draws a 1 pixel frame around itself and a line between the header and the content
/// in its palette's Mid colour; the content sits 4 pixels below that line, 8 from the sides and 6
/// above the bottom (Java: a line border in Component.borderColor, the content's matte border
/// and its empty border 4, 8, 6, 8).
///
/// Deviations from OpenRocket:
/// - Collapsed, the content is hidden but stays a child of the panel; Java removes it from the
///   panel (its RecoveryWarningsPanelTest finds no spinner of a collapsed section: here they are
///   hidden, isVisibleTo() the panel false).
/// - The frame's colour is the palette's Mid, where FlatLaf has its Component.borderColor (QtRocket
///   follows the platform's style), and the header's padding (Java: FlatLaf's margin 6, 8, 6, 8
///   and an icon-text gap of 8) is the style's own.
class CollapsiblePanel final : public QWidget
{
    Q_OBJECT

public:
    /// A section titled @p title showing @p content, which becomes its child, when @p expanded.
    /// @throws BugError when @p content is null
    CollapsiblePanel(const QString& title, QWidget* content, bool expanded, QWidget* parent);

    /// Shows the content (true) or hides it, and checks the header accordingly (Java:
    /// setExpanded()).
    void setExpanded(bool expanded);

    /// Whether the content is shown (Java: isExpanded()).
    [[nodiscard]] bool isExpanded() const noexcept { return m_expanded; }

    /// The header (Java: getHeaderButton()).
    [[nodiscard]] QToolButton* headerButton() const noexcept { return m_header; }

    /// The content given to the constructor.
    [[nodiscard]] QWidget* content() const noexcept { return m_content; }

protected:
    void paintEvent(QPaintEvent* event) override;
    void changeEvent(QEvent* event) override;

private:
    /// Java's updateAppearance(): the header's font from the application's label font.
    void updateAppearance();

    QToolButton* m_header;
    QWidget*     m_container;
    QWidget*     m_content;
    bool         m_expanded{false};
};

}  // namespace QtRocket::Gui
