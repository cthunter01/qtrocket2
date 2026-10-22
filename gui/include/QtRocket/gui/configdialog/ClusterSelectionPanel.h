#pragma once

#include <memory>

#include <QList>
#include <QObject>
#include <QWidget>

#include "QtRocket/gui/binding/ComponentSelection.h"

namespace QtRocket
{
class ClusterConfiguration;
}  // namespace QtRocket

namespace QtRocket::Gui
{

class BindingScope;
class ClusterButton;

/// The cluster layouts an inner tube can take (OpenRocket's ClusterSelectionPanel, in swing
/// gui/configdialog/InnerTubeConfig.java): a ClusterButton for each layout of
/// ClusterConfiguration::configurations(), in their order, four to a row in cells of the
/// buttons' size without gaps (Java's MigLayout "gap 0 0" with 50-pixel columns and rows: the 14
/// layouts make three rows of four and a row of two), the main component's layout shown selected.
/// A click on a button gives its layout to every selected inner tube (select(); Java: the main
/// component's setClusterConfiguration(), which forwards it to its config listeners first).
///
/// The panel shows the main component's layout after every change of the rocket (Java: each
/// button listens to the component and repaints): it reads it when the selection's bindings read
/// (ComponentSelection::Phase::READ) and shows it when they notify; once the selection is invalid
/// it shows none selected and a click does nothing. Its selection must be that of the panel's
/// BindingScope, whose main component is an inner tube.
class ClusterSelectionPanel : public QWidget
{
    Q_OBJECT

public:
    /// The layouts of the inner tubes of @p scope's selection.
    /// @throws BugError when the scope has no valid selection or its main component is no inner
    ///         tube
    ClusterSelectionPanel(BindingScope& scope, QWidget* parent);

    /// The number of buttons: one per layout.
    [[nodiscard]] int buttonCount() const noexcept;

    /// The button of the layout @p index of ClusterConfiguration::configurations().
    /// @throws BugError when @p index is out of range
    [[nodiscard]] ClusterButton& button(int index) const;

    /// The main component's layout, as shown; null once the selection is invalid.
    [[nodiscard]] const ClusterConfiguration* selectedConfiguration() const noexcept
    {
        return m_selected;
    }

    /// Gives @p configuration to every selected inner tube (Java: ClusterButton.mouseClicked()),
    /// in one change of the rocket (ComponentSelection::apply()). Nothing once the selection is
    /// invalid.
    void select(const ClusterConfiguration& configuration);

private:
    void selectionChanged(ComponentSelection::Phase phase);
    /// Marks the button of the shown layout selected and the others not.
    void showSelected();

    std::shared_ptr<ComponentSelection>               m_selection;
    ComponentSelection::PhaseSignal::ScopedConnection m_connection;
    QList<ClusterButton*>                             m_buttons;
    const ClusterConfiguration*                       m_selected{nullptr};
};

}  // namespace QtRocket::Gui
