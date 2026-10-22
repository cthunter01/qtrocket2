#pragma once

#include <QFrame>
#include <QList>
#include <QObject>
#include <QRectF>

class QMouseEvent;
class QPaintEvent;
class QWidget;

namespace QtRocket
{
class ClusterConfiguration;
}  // namespace QtRocket

namespace QtRocket::Gui
{

/// One button of the cluster selection of an inner tube (OpenRocket's
/// ClusterSelectionPanel.ClusterButton, in swing gui/configdialog/InnerTubeConfig.java): a panel
/// of kButtonSize x kButtonSize pixels with a lowered bevel border, filled red while its layout is
/// the selected one and with the window's background colour otherwise, showing each motor of the
/// layout as a green circle of kMotorDiameter pixels with a black outline (motorCircles()). Its
/// tooltip is the layout's xml name ("3-ring"). A click of the left button (a press and a release
/// inside the button) emits clicked(); the panel (ClusterSelectionPanel) then gives the layout to
/// the selected inner tubes.
///
/// It takes no focus and no keys, as Java's JPanel; Java's mouseClicked() also wants the mouse not
/// to move between the press and the release, which is left out here.
class ClusterButton : public QFrame
{
    Q_OBJECT

public:
    /// The size of the button (BUTTON_SIZE), in pixels.
    static constexpr int kButtonSize = 50;
    /// The diameter of a motor's circle (MOTOR_DIAMETER), in pixels.
    static constexpr int kMotorDiameter = 10;

    /// The button of @p configuration, unselected.
    ClusterButton(const ClusterConfiguration& configuration, QWidget* parent);

    [[nodiscard]] const ClusterConfiguration& configuration() const noexcept
    {
        return *m_configuration;
    }

    /// Whether the button shows its layout as the selected one.
    [[nodiscard]] bool isSelected() const noexcept { return m_selected; }
    void               setSelected(bool selected);

    /// The circles of the motors of @p configuration in a button's coordinates: for each point
    /// (x, y) the square of side kMotorDiameter centred on (kButtonSize / 2 + x * kMotorDiameter,
    /// kButtonSize / 2 - y * kMotorDiameter), as Java's Ellipse2D.setFrameFromCenter().
    [[nodiscard]] static QList<QRectF> motorCircles(const ClusterConfiguration& configuration);

Q_SIGNALS:
    /// The left mouse button was pressed and released inside the button.
    void clicked();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    const ClusterConfiguration* m_configuration;
    bool                        m_selected{false};
    /// Set by a press of the left button inside the button, cleared by the release.
    bool m_pressed{false};
};

}  // namespace QtRocket::Gui
