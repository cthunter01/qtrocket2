#pragma once

class QWidget;

/// The dialog's part of a field widget's enabled state: what a check box's enable follower
/// (BoolCheckBox::addEnableFollower(), Java's BooleanModel.addEnableComponent()) or a dialog's own
/// rule (Java: checkAutoOuterRadius.setEnabled(false)) decides. The field widgets of this area
/// (UnitSpinBox, IntSpinBox, UnitSelector, ValueSlider, BoolCheckBox, EnumComboBox,
/// MaterialComboBox) are enabled when their binding allows it AND the dialog does: setEnabled()
/// on them would last only until their binding's next change, so their setFieldEnabled() keeps
/// the dialog's part apart. Any other widget (a label, a button) takes setEnabled().
namespace QtRocket::Gui::FieldEnabling
{

/// Sets the dialog's part of @p widget's enabled state (see the namespace comment).
void setFieldEnabled(QWidget& widget, bool enabled);

/// The dialog's part of @p widget's enabled state: isFieldEnabled() of a field widget; for any
/// other widget whether it was not disabled itself (setEnabled(false)), whatever its parent is.
[[nodiscard]] bool isFieldEnabled(const QWidget& widget);

}  // namespace QtRocket::Gui::FieldEnabling
