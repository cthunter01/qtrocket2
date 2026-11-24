#pragma once

#include <QColor>
#include <QIcon>
#include <QObject>
#include <QPushButton>
#include <QSize>

class QWidget;

namespace QtRocket::Gui
{

class UserPrompts;

/// A button that shows a colour and asks for another (OpenRocket's ColorChooserButton, swing
/// gui/components/, with its ColorIcon): the plot tab's colour of each Y axis row, and Milestone
/// 5's appearance panel. Its icon is the selected colour (selectedColor()), a kIconSize rectangle
/// filled with it, or only its outline in that colour while the button is disabled (Java's
/// ColorIcon); it has no text. A click asks for a colour (UserPrompts::askColor(), titled "Select
/// color", the selected colour to start with; Java: JColorChooser.createDialog()), and the colour
/// chosen with OK becomes the selected one; Cancel keeps it.
///
/// setSelectedColor() takes a valid colour that differs from the selected one (Java's
/// Color.equals(): the red, green, blue and alpha values, QColor::rgba()): it shows it and tells of
/// it with selectedColorChanged() (Java: the property change of COLOR_KEY); an invalid colour
/// (Java: null) or the selected one changes nothing. A button made without a valid colour
/// has none (selectedColor() is invalid) and no icon until it is given one.
///
/// Deviations from OpenRocket:
/// - The colour changes once the dialog is closed with OK; Java's chooser shows each colour picked
///   in the button at once (a ChangeListener on its selection model) and puts the first one back
///   on Cancel. The colour after OK or Cancel is the same.
/// - selectedColorChanged() is emitted once per change; Java's listeners hear each change twice
///   (setSelectedColor() fires the property change, and so does its putClientProperty() of the
///   same key).
/// - Java's dialog is one JColorChooser per button, kept between clicks; QtRocket asks UserPrompts
///   each time (WidgetUserPrompts: Qt's own colour dialog).
class ColorChooserButton final : public QPushButton
{
    Q_OBJECT

public:
    /// ColorIcon's size, px.
    static constexpr QSize kIconSize{25, 15};

    /// A button of the colour @p initial (none when it is invalid), which asks @p prompts for
    /// another; @p prompts must outlive the button.
    ColorChooserButton(const QColor& initial, UserPrompts& prompts, QWidget* parent);
    ~ColorChooserButton() override;
    ColorChooserButton(const ColorChooserButton&)            = delete;
    ColorChooserButton& operator=(const ColorChooserButton&) = delete;
    ColorChooserButton(ColorChooserButton&&)                 = delete;
    ColorChooserButton& operator=(ColorChooserButton&&)      = delete;

    /// The selected colour; invalid for none (Java: null).
    [[nodiscard]] const QColor& selectedColor() const noexcept { return m_color; }
    /// Selects @p color (see the class comment).
    void setSelectedColor(const QColor& color);

    /// Asks for a colour, as a click does.
    void chooseColor();

    /// The icon of @p color: Java's ColorIcon, drawn by a QIconEngine (filled in every mode but
    /// QIcon::Disabled, which draws the outline).
    [[nodiscard]] static QIcon colorIcon(const QColor& color);

Q_SIGNALS:
    /// The selected colour changed to @p color.
    void selectedColorChanged(const QColor& color);

private:
    UserPrompts& m_prompts;
    QColor       m_color;
};

}  // namespace QtRocket::Gui
