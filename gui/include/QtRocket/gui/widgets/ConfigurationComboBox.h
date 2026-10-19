#pragma once

#include <vector>

#include <QComboBox>
#include <QObject>

#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/FlightConfigurationId.h"

class QWheelEvent;
class QWidget;

namespace QtRocket
{
class Preferences;
class Rocket;
}  // namespace QtRocket

namespace QtRocket::Gui
{

/// A combo box of a rocket's flight configurations (OpenRocket's ConfigurationComboBox): the
/// default configuration first, then the others in the rocket's order
/// (Rocket::getFlightConfigurationByIndex(i, true)), each shown by its name with the motors
/// substituted (FlightConfiguration::getName(): "[No motors]", "[A8-3]", "[None; F50T-9]").
///
/// - It selects the rocket's selected configuration: picking an item calls
///   Rocket::setSelectedConfiguration() (which fires NONFUNCTIONAL_CHANGE when it changes),
///   and the combo box shows the rocket's selected configuration whatever selected it. A combo
///   box made with @p updateRocketConfig false (the simulation dialog's, Milestone 4) keeps its
///   own selection instead, which starts at the rocket's selected configuration and leaves the
///   rocket alone.
/// - The items follow the rocket: every change event brings them and the selection up to date
///   (refresh()), and so does opening the popup (Java rebuilds its model then). A change that
///   fires no event (the default configuration name or the motor naming preference) shows after
///   refresh().
/// - The items keep the configurations' ids, never the configuration objects (an undo or a redo
///   replaces every configuration).
/// - The mouse wheel does not change the selection: the event goes on to the parent (a Swing
///   JComboBox has no wheel listener, so its parent scroll pane scrolls), as the design view's
///   ribbon scrolls with it.
///
/// The rocket and the preferences must outlive the combo box.
///
/// Deviations from OpenRocket:
/// - Java's combo box draws each name when it paints, so it is always current; here the names
///   are those of the last refresh() (every change event and every popup).
/// - In the combo box with its own selection, a configuration that the rocket no longer has is
///   the default configuration (Rocket::getFlightConfiguration()); Java keeps the removed
///   object selected.
/// - configurationActivated() is emitted for the user's choice only; Java's ActionListeners are
///   also told of a setSelectedItem() by the program.
class ConfigurationComboBox final : public QComboBox
{
    Q_OBJECT

public:
    /// The configurations of @p rocket, selecting the rocket's selected configuration
    /// (ConfigurationComboBox(rocket)); the names are made with @p preferences.
    ConfigurationComboBox(Rocket& rocket, const Preferences& preferences, QWidget* parent);
    /// The same, with its own selection when @p updateRocketConfig is false.
    ConfigurationComboBox(Rocket& rocket, const Preferences& preferences, bool updateRocketConfig,
                          QWidget* parent);

    [[nodiscard]] Rocket& rocket() const noexcept { return *m_rocket; }

    /// Whether picking an item selects the configuration in the rocket.
    [[nodiscard]] bool updatesRocketConfig() const noexcept { return m_updateRocketConfig; }

    /// The id of the configuration shown as selected (getSelectedItem()): the rocket's selected
    /// one, or the combo box's own selection.
    [[nodiscard]] FlightConfigurationId selectedConfigurationId() const;

    /// The id of the configuration of the item at @p index.
    /// @throws BugError when @p index is not an item's
    [[nodiscard]] const FlightConfigurationId& configurationIdAt(int index) const;

    /// Selects the configuration of @p id (setSelectedItem()): in the rocket, or as the combo
    /// box's own selection (the default when the rocket has no such configuration).
    void setSelectedConfiguration(const FlightConfigurationId& id);

    /// Brings the items and the selection up to date with the rocket.
    void refresh();

    /// Refreshes the items, then opens the popup.
    void showPopup() override;

Q_SIGNALS:
    /// The user picked the configuration @p id, and it is selected now.
    void configurationActivated(const QtRocket::FlightConfigurationId& id);

protected:
    /// Ignores the wheel (see the class comment).
    void wheelEvent(QWheelEvent* event) override;

private:
    /// The user's pick of the item at @p index.
    void itemActivated(int index);

    /// Shows the selected configuration's item as the current one.
    void updateCurrentIndex();

    Rocket*                                 m_rocket;
    const Preferences*                      m_preferences;
    bool                                    m_updateRocketConfig;
    FlightConfigurationId                   m_selectedId;
    std::vector<FlightConfigurationId>      m_itemIds;
    ComponentChangeSignal::ScopedConnection m_rocketConnection;
};

}  // namespace QtRocket::Gui
