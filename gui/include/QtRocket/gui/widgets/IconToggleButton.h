#pragma once

#include <QAbstractButton>
#include <QIcon>
#include <QObject>
#include <QSize>

class QPaintEvent;
class QWidget;

namespace QtRocket::Gui
{

/// A toggle button that is only its icon (OpenRocket's IconToggleButton): no border, no
/// background, no margin, and it takes no keyboard focus. Once setSelectedIcon() has set the
/// icon of the selected state, the unselected state shows that icon greyed out (the style's
/// disabled version, as Java's L&F derives it); a button made with an icon alone shows it in
/// both states. setIconScale() scales the icon (and the button) by a factor.
///
/// The button paints the icon over its whole area, centred: QIcon::Normal when it is checked or
/// pressed, QIcon::Disabled when it is unchecked after setSelectedIcon() or when the button is
/// disabled.
///
/// Deviation from OpenRocket: the constructors with a text are not ported (the GUI uses the
/// icon alone).
class IconToggleButton : public QAbstractButton
{
    Q_OBJECT

public:
    explicit IconToggleButton(QWidget* parent);
    IconToggleButton(const QIcon& icon, QWidget* parent);
    IconToggleButton(const QIcon& icon, bool selected, QWidget* parent);

    /// The icon of the selected state, from which the unselected state's greyed icon comes
    /// (setSelectedIcon()).
    void setSelectedIcon(const QIcon& icon);

    /// The factor the icon size is scaled by (setIconScale(); 1 at first).
    void                 setIconScale(double scale);
    [[nodiscard]] double iconScale() const noexcept { return m_iconScale; }

    /// Whether the unselected state shows the selected icon greyed out (after setSelectedIcon()).
    [[nodiscard]] bool greysUnselectedIcon() const noexcept { return m_greyUnselected; }

    /// The icon size times the icon scale: the button is its icon.
    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    void initSettings();

    double m_iconScale      = 1.0;
    bool   m_greyUnselected = false;
};

}  // namespace QtRocket::Gui
