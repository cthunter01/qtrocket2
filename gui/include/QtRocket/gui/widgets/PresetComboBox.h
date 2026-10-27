#pragma once

#include <memory>
#include <vector>

#include <QComboBox>
#include <QFont>
#include <QObject>
#include <QString>

#include "QtRocket/gui/binding/PresetBinding.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/util/Signal.h"

class QEvent;
class QPaintEvent;
class QStyleOptionComboBox;
class QWheelEvent;
class QWidget;

namespace QtRocket::Gui
{

/// The preset combo box of a component dialog's header row (OpenRocket: the JComboBox on a
/// PresetModel that RocketComponentConfig makes for the components that take presets,
/// RocketComponentConfig.java:160-169), beside the "Parts Library" button.
///
/// Its items are "Custom" (PresetModel.lbl.custompreset, in italics, Java's
/// "<html><i>Custom</i></html>"), then the binding's favourite presets
/// (PresetBinding::favorites(), the database's order), each shown as its part number
/// (ComponentPreset::toString()). The current item is the one that equals the main component's
/// preset (Java: JComboBox.getSelectedIndex(), the first item that equals() the model's selected
/// item, ComponentPreset's digest), "Custom" for none; a preset that is no favourite is shown all
/// the same, by its part number, with no item current (shownText(); Java's JComboBox paints a
/// selected item its list lacks: probe). Not editable, at most 25 rows in the list
/// (setMaximumRowCount(25), where the style lets the combo box decide: QComboBox's
/// maxVisibleItems), and the tooltip PresetModel.combo.ttip, as Java's lines 163-168 set them.
///
/// A choice of the user's (QComboBox::activated: the popup, the keys, the wheel) is Java's
/// JComboBox.setSelectedItem(): nothing when the chosen item equals the current one (probe:
/// JComboBox does not call the model then, also for a copy of the preset with the same digest),
/// otherwise "Custom" clears the presets (PresetBinding::clear(), no undo position) and a preset
/// is chosen (choosePreset()). Afterwards, whatever came of it, the combo box shows the binding's
/// state again. It also shows it whenever the binding tells of a change (an event of the rocket,
/// PresetBinding::reread()), rebuilding its items when the favourites changed.
///
/// choosePreset() is PresetModel.setSelectedItem(preset) itself, which the parts library calls on
/// Close (also for the current preset): PresetBinding::choose() with the undo text "Use Preset
/// <component name>" (undoDescription()), then presetChosen(), on which the dialog moves the focus
/// to its first field (Java: RocketComponentConfig.setFocusElement()). The parts library may have
/// ticked favourites: its caller then calls binding()->reread().
///
/// Once the binding's selection is invalid the combo box keeps what it showed and is disabled;
/// otherwise its enabled state is the caller's.
///
/// "probe": OpenRocket's results, printed by
/// tools/openrocket-gui-pins/preset-chooser/probe/PresetComboProbe.java (a JComboBox on a recording
/// model with PresetModel's items; the family's hand-pinned.txt keeps them, PresetComboBoxTests.cpp
/// pins them).
///
/// Deviations from OpenRocket:
/// - The wheel chooses only while the combo box has the focus, which the wheel does not give it
///   (EnumComboBox's rule); Java's combo box takes no wheel. In the open list the arrow keys move
///   the highlight and Enter or a click chooses, as in every Qt combo box; Java's list chooses at
///   each arrow key (ComboBox.noActionOnKeyNavigation is false in OpenRocket's FlatLaf Light:
///   probe), each choice a preset with its undo position. On the closed combo box Up and Down
///   choose the previous or the next item in both (FlatLaf's selectNext2 and selectPrevious2).
/// - The favourites follow the parts library's ticks when it closes, not while it is open (the
///   plan's decision O-M3-12; PresetBinding's deviations).
class PresetComboBox : public QComboBox
{
    Q_OBJECT

public:
    /// A combo box of @p binding, whose selection must be valid.
    /// @throws BugError when @p binding is null or its selection is invalid
    PresetComboBox(std::shared_ptr<PresetBinding> binding, QWidget* parent);
    ~PresetComboBox() override;
    PresetComboBox(const PresetComboBox&)            = delete;
    PresetComboBox& operator=(const PresetComboBox&) = delete;
    PresetComboBox(PresetComboBox&&)                 = delete;
    PresetComboBox& operator=(PresetComboBox&&)      = delete;

    /// The text of the "Custom" item (PresetModel.lbl.custompreset).
    [[nodiscard]] static QString customText();

    /// The undo text of a preset chosen for a component of @p kind (Java: "Use Preset " +
    /// getComponentName(), whose first part OpenRocket does not translate: "Use Preset Body
    /// Tube").
    [[nodiscard]] static QString undoDescription(ComponentKind kind);

    [[nodiscard]] const std::shared_ptr<PresetBinding>& binding() const noexcept
    {
        return m_binding;
    }

    /// The favourites the items after "Custom" show, in their order.
    [[nodiscard]] const std::vector<PresetBinding::PresetPtr>& shownFavorites() const noexcept
    {
        return m_shownFavorites;
    }

    /// The text the closed combo box shows: the current item's, or the part number of a preset
    /// no item holds.
    [[nodiscard]] QString shownText() const;

    /// The font the closed combo box writes shownText() in: the combo box's, in italics while
    /// "Custom" is current.
    [[nodiscard]] QFont shownFont() const;

    /// Java's PresetModel.setSelectedItem(@p preset): PresetBinding::choose() with
    /// undoDescription() of the main component, then presetChosen() and the binding's state
    /// shown again. False, with nothing done, when the binding refuses (its selection is invalid,
    /// or it is telling its listeners).
    /// @throws BugError when @p preset is null or of another type than the binding's
    bool choosePreset(const PresetBinding::PresetPtr& preset);

    /// Shows the binding's state again (see the class comment). It reads the main component's
    /// preset (its digest), so it runs outside a freeze of the rocket, as the binding's listeners
    /// do.
    void refresh();

Q_SIGNALS:
    /// A preset was chosen and loaded (choosePreset(), or an item of the user's): the dialog moves
    /// the focus to its first field (Java: setFocusElement()). Not for "Custom".
    void presetChosen();

protected:
    void changeEvent(QEvent* event) override;
    void initStyleOption(QStyleOptionComboBox* option) const override;
    void paintEvent(QPaintEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    /// The item that equals the binding's value (see the class comment): 0 for "Custom", -1 for
    /// a preset no item holds.
    [[nodiscard]] int itemOfValue() const;
    /// The choice of item @p index (Java: JComboBox.setSelectedItem() of that item).
    void chooseItem(int index);
    /// A choice of the user's on the combo box.
    void activatedByUser(int index);
    /// Puts the font of the "Custom" item: the combo box's, in italics.
    void setCustomFont();

    std::shared_ptr<PresetBinding>        m_binding;
    ComponentKind                         m_kind;
    Signal<>::ScopedConnection            m_connection;
    std::vector<PresetBinding::PresetPtr> m_shownFavorites;
    /// The text of the binding's preset while no item holds it, else empty.
    QString m_apartText;
};

}  // namespace QtRocket::Gui
