#pragma once

#include <QObject>
#include <QString>
#include <QWidget>

#include "QtRocket/util/Signal.h"

class QLabel;
class QPushButton;

namespace QtRocket
{
class MultiLevelPinkNoiseWindModel;
}  // namespace QtRocket

namespace QtRocket::Gui
{

/// The multi-level wind's card of the launch conditions (OpenRocket: SimulationConditionsPanel's
/// addMultiLevelSettings() and updateWindLevelSummary()): a summary of the levels of a
/// MultiLevelPinkNoiseWindModel above the button "Edit levels" (simedtdlg.but.editWindLevels, the
/// icon EDIT_EDIT), which opens the wind editor.
///
/// The summary (summaryText()): "No wind levels defined" without levels; otherwise, as HTML, the
/// number of levels ("%d wind level(s)"), the altitude of the one level ("Altitude: %s") or the
/// range of the levels' altitudes ("Altitude range: %s - %s"), and the speed of the one level
/// ("Speed: %s") or the range of their speeds ("Speed range: %s - %s"), each value in the default
/// unit of its unit group with the unit's name: the altitudes in UnitGroup DISTANCE, the speeds in
/// VELOCITY (not WINDSPEED, as Java).
///
/// The button tells of its click (editLevelsRequested()); the owner opens the wind editor, a
/// modal dialog over the model (SimulationConditionsPanel::editWindLevels():
/// MultiLevelWindEditDialog), with the services it needs (the preferences of "Reset levels", the
/// questions of the CSV import), which this widget has not.
///
/// The model must stay at its address while the summary shows it (a SimulationOptions' model
/// does, for the life of the options); the summary learns that it has gone from its change
/// signal, which goes with it, and from then on keeps its last text and touches the model no
/// more.
///
/// OpenRocket bug 36, fixed (Milestone 4): Java writes the summary when the panel is made and
/// again only when the wind editor closes, so "Reset to default", an undo of the document or a
/// level changed elsewhere leave it stale; and the highest altitude and speed start from
/// Double.MIN_VALUE, the smallest positive double, so levels that all lie below sea level show
/// the highest altitude 0 ("Altitude range: -50 m - 0 m" for levels at -50 and -10 m). Here the
/// summary is written again on every change of the model (its changed() signal: a level added,
/// removed or changed, the levels cleared, reset, loaded or imported), and the highest values
/// start below every value.
class WindLevelSummary final : public QWidget
{
    Q_OBJECT

public:
    /// The summary of @p model (see the class comment).
    WindLevelSummary(MultiLevelPinkNoiseWindModel& model, QWidget* parent);
    ~WindLevelSummary() override;
    WindLevelSummary(const WindLevelSummary&)            = delete;
    WindLevelSummary& operator=(const WindLevelSummary&) = delete;
    WindLevelSummary(WindLevelSummary&&)                 = delete;
    WindLevelSummary& operator=(WindLevelSummary&&)      = delete;

    /// The summary of @p model's levels (Java: updateWindLevelSummary(), with bug 36 fixed; see
    /// the class comment).
    [[nodiscard]] static QString summaryText(const MultiLevelPinkNoiseWindModel& model);

    /// The label of the summary (object name "summaryLabel").
    [[nodiscard]] QLabel& summaryLabel() const noexcept { return *m_summaryLabel; }

    /// The button "Edit levels" (object name "editButton").
    [[nodiscard]] QPushButton& editButton() const noexcept { return *m_editButton; }

    /// Whether the model is still there (see the class comment).
    [[nodiscard]] bool hasModel() const noexcept;

    /// Writes the summary of the model again; nothing once the model has gone.
    void refresh();

Q_SIGNALS:
    /// The user clicked "Edit levels": the owner opens the wind editor over the model.
    void editLevelsRequested();

private:
    MultiLevelPinkNoiseWindModel* m_model;
    Signal<>::ScopedConnection    m_connection;
    QLabel*                       m_summaryLabel;
    QPushButton*                  m_editButton;
};

}  // namespace QtRocket::Gui
