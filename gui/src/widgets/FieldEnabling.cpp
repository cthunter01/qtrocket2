#include "QtRocket/gui/widgets/FieldEnabling.h"

#include <QObject>
#include <QWidget>
#include <Qt>

#include "QtRocket/gui/widgets/BoolCheckBox.h"
#include "QtRocket/gui/widgets/EnumComboBox.h"
#include "QtRocket/gui/widgets/IntSpinBox.h"
#include "QtRocket/gui/widgets/MaterialComboBox.h"
#include "QtRocket/gui/widgets/UnitSelector.h"
#include "QtRocket/gui/widgets/UnitSpinBox.h"
#include "QtRocket/gui/widgets/ValueSlider.h"

namespace QtRocket::Gui::FieldEnabling
{

namespace
{

/// Calls @p f with @p widget as the field widget it is, or with nothing when it is none; the
/// result of @p f. qobject_cast is safe on a widget that is being destroyed: its type is then a
/// base's (a QWidget's), and no field widget is found.
template <class Widget, class F>
auto withField(Widget& widget, const F& f)
{
    if (auto* spin = qobject_cast<UnitSpinBox*>(&widget))
    {
        return f(*spin);
    }
    if (auto* count = qobject_cast<IntSpinBox*>(&widget))
    {
        return f(*count);
    }
    if (auto* selector = qobject_cast<UnitSelector*>(&widget))
    {
        return f(*selector);
    }
    if (auto* slider = qobject_cast<ValueSlider*>(&widget))
    {
        return f(*slider);
    }
    if (auto* check = qobject_cast<BoolCheckBox*>(&widget))
    {
        return f(*check);
    }
    if (auto* combo = qobject_cast<EnumComboBox*>(&widget))
    {
        return f(*combo);
    }
    if (auto* materials = qobject_cast<MaterialComboBox*>(&widget))
    {
        return f(*materials);
    }
    return f(widget);
}

}  // namespace

void setFieldEnabled(QWidget& widget, bool enabled)
{
    withField(widget, [enabled](auto& field) {
        if constexpr (requires { field.setFieldEnabled(enabled); })
        {
            field.setFieldEnabled(enabled);
        }
        else
        {
            field.setEnabled(enabled);
        }
    });
}

bool isFieldEnabled(const QWidget& widget)
{
    // qobject_cast of a pointer to const gives a pointer to const.
    if (const auto* spin = qobject_cast<const UnitSpinBox*>(&widget))
    {
        return spin->isFieldEnabled();
    }
    if (const auto* count = qobject_cast<const IntSpinBox*>(&widget))
    {
        return count->isFieldEnabled();
    }
    if (const auto* selector = qobject_cast<const UnitSelector*>(&widget))
    {
        return selector->isFieldEnabled();
    }
    if (const auto* slider = qobject_cast<const ValueSlider*>(&widget))
    {
        return slider->isFieldEnabled();
    }
    if (const auto* check = qobject_cast<const BoolCheckBox*>(&widget))
    {
        return check->isFieldEnabled();
    }
    if (const auto* combo = qobject_cast<const EnumComboBox*>(&widget))
    {
        return combo->isFieldEnabled();
    }
    if (const auto* materials = qobject_cast<const MaterialComboBox*>(&widget))
    {
        return materials->isFieldEnabled();
    }
    return !widget.testAttribute(Qt::WA_ForceDisabled);
}

}  // namespace QtRocket::Gui::FieldEnabling
