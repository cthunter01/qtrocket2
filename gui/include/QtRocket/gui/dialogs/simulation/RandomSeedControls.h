#pragma once

#include <memory>
#include <optional>

#include <QObject>
#include <QString>

#include "QtRocket/gui/binding/BindingContext.h"

class QCheckBox;
class QEvent;
class QLineEdit;
class QWidget;

namespace QtRocket
{
class SimulationOptions;
}  // namespace QtRocket

namespace QtRocket::Gui
{

class FieldValidator;
class ObjectSelection;

/// The random seed of the simulation options tab (OpenRocket's SimulationOptionsPanel, swing
/// gui/simulation/: its fixedRandomSeedCheckBox, randomSeedField and randomSeedValidator, with
/// their listeners): the check box "Use fixed random seed:" and the text field of the seed, a
/// signed 32-bit integer, over a simulation's SimulationOptions. The two widgets are children of
/// the parent given, which places them (the panel's grid: the check box in its label column, the
/// field beside it); the controls are a QObject child of the same parent.
///
/// What they do, as Java's:
/// - The check box shows SimulationOptions::isRandomSeedFixed(), the field the seed while it is
///   fixed and nothing (disabled) while it is not.
/// - Checking the box draws a new seed (SimulationOptions::randomizeSeed()), shows it and fixes
///   it; unchecking unfixes the seed and empties the field.
/// - Every change of the field's text whose text is a seed (parseRandomSeed()) sets it as the
///   options' seed, while the box is checked; another text changes nothing.
/// - The field's validator (FieldValidator, with its message popup) shows the error "Enter a
///   signed 32-bit integer." while the box is checked and the text is no seed.
/// - The focus leaving the field for good (not for a popup or another window) with a text that is
///   no seed while the box is checked draws a new seed and shows it (Java's non-temporary
///   focusLost(): replaceInvalidRandomSeed()).
/// - prepareForSimulation() (the settings dialog's OK, "Save as default") resolves what the
///   controls show: a seed in the field while the box is checked fixes it, anything else unfixes
///   the seed and empties the field (applyRandomSeedInput()).
///
/// parseRandomSeed() is Java's Integer.valueOf(text.trim()): the text without the characters up to
/// U+0020 at either end, an optional '+' or '-', then decimal digits (any of Unicode's, as Java's
/// Character.digit() takes them: the Arabic-Indic digits one, two and three are 123), within the
/// range of an int.
///
/// Lifetime: @p options is a selection of the SimulationOptions (ObjectSelection::make<
/// SimulationOptions>()), which the owner keeps (a BindingScope) and releases when its dialog
/// closes; once it is invalid (released, or the options gone) the controls write and read
/// nothing and keep what they showed.
///
/// Deviation from OpenRocket: the controls follow the options. When the seed or its flag change
/// while the controls do not change them, as the launch conditions tab's "Reset to default" does
/// (SimulationOptions::copyConditionsFrom() takes the seed), the controls show the options'
/// state again; a text that is no seed is left as it is (its error still shows). Java's are set
/// when the panel is made and then only by the panel itself, so that the settings dialog's OK
/// (prepareForSimulation()) would put back the seed they still showed.
class RandomSeedControls final : public QObject
{
    Q_OBJECT

public:
    /// The controls of the options of @p options (see the class comment), their widgets children
    /// of @p parent (objectNames fixedRandomSeedCheckBox and randomSeedField, Java's fields).
    /// @throws BugError when @p parent or @p options is null or @p options does not hold a
    ///         SimulationOptions
    RandomSeedControls(std::shared_ptr<ObjectSelection> options, QWidget* parent);
    ~RandomSeedControls() override;
    RandomSeedControls(const RandomSeedControls&)            = delete;
    RandomSeedControls& operator=(const RandomSeedControls&) = delete;
    RandomSeedControls(RandomSeedControls&&)                 = delete;
    RandomSeedControls& operator=(RandomSeedControls&&)      = delete;

    /// The minimum width of the field, Java's "w 130lp!".
    static constexpr int kFieldWidth = 130;

    /// "Use fixed random seed:" (tooltip simedtdlg.checkbox.ttip.FixedRandomSeed).
    [[nodiscard]] QCheckBox& checkBox() const noexcept { return *m_checkBox; }
    /// The seed's text field (tooltip simedtdlg.lbl.ttip.RandomSeed, 12 columns).
    [[nodiscard]] QLineEdit& field() const noexcept { return *m_field; }
    /// The field's validator (see the class comment).
    [[nodiscard]] FieldValidator& validator() const noexcept { return *m_validator; }

    /// Java's prepareForSimulation(): applyRandomSeedInput() of what the controls show, then the
    /// controls show the result (the box checked only for a fixed seed, the field emptied
    /// otherwise). Nothing once the options' selection is invalid.
    void prepareForSimulation();

    /// Java's updateRandomSeedControlsFromOptions(): the box and the field show the options' seed
    /// and its flag (the validator is not updated, as in Java).
    void showOptions();

    /// Java's static applyRandomSeedInput(): with @p fixedSeedRequested and a seed in
    /// @p seedText (parseRandomSeed()), sets the seed on @p options and fixes it, and returns
    /// true; otherwise unfixes the seed and returns false.
    static bool applyRandomSeedInput(SimulationOptions& options, bool fixedSeedRequested,
                                     const QString& seedText);

    /// Java's parseRandomSeed(): the seed @p text holds (see the class comment), or nullopt.
    [[nodiscard]] static std::optional<int> parseRandomSeed(const QString& text);

    /// The field's focus loss (see the class comment); the events always go on (false).
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    /// The user checked or unchecked the box (Java: its ActionListener).
    void checkBoxClicked(bool checked);
    /// The field's text changed (Java: its DocumentListener's updateRandomSeedFromField()).
    void seedFromField();
    /// Java's replaceInvalidRandomSeed().
    void replaceInvalidRandomSeed();
    /// The options changed (see the deviation in the class comment).
    void optionsChanged(BindingContext::Phase phase);
    /// The options while their selection is valid; null otherwise.
    [[nodiscard]] SimulationOptions* liveOptions() const;

    std::shared_ptr<ObjectSelection> m_options;
    QCheckBox*                       m_checkBox{nullptr};
    QLineEdit*                       m_field{nullptr};
    FieldValidator*                  m_validator{nullptr};
    /// Java's updatingRandomSeedControls: set while the controls write the widgets or the
    /// options themselves.
    bool                                          m_updating{false};
    BindingContext::PhaseSignal::ScopedConnection m_optionsConnection;
};

}  // namespace QtRocket::Gui
