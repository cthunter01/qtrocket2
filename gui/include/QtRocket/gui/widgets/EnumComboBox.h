#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>

#include <QComboBox>
#include <QObject>
#include <QString>

#include "QtRocket/gui/binding/EnumBinding.h"
#include "QtRocket/gui/util/EnumText.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Signal.h"

class QStyleOptionComboBox;
class QWheelEvent;
class QWidget;

namespace QtRocket::Gui
{

class Binding;

/// The combo box of an enum property (OpenRocket: a JComboBox on an EnumModel or a
/// TransitionShapeModel): the placement's axial method, a surface finish, a fin cross section, a
/// mass component's type, a deployment, separation or ignition event, a transition's shape.
///
/// Its items are the binding's displayed values (EnumBinding::displayedValues(): the values given,
/// without the hidden ones), each shown as its text: enumText() (the core's displayName() through
/// the translators, util/EnumText; Java: the enum's toString()) or the text function given (the
/// surface finish shows its roughness in the current unit, Java's Finish.toString()). The current
/// item is the binding's value; a value that is not among the items (hidden, or not offered) is
/// shown all the same, with no item current (Java's JComboBox shows a selected item that its list
/// lacks; shownText() gives it). A choice of the user's (QComboBox::activated: the popup, the
/// keys, the wheel) goes to EnumBinding::select(); afterwards, whatever came of it, the combo box
/// shows the binding's state again (a value the binding skips, drops or a setter refuses fires
/// nothing). It also shows it whenever the binding tells of a change, rebuilding its items when
/// their texts changed.
///
/// Enabled: the binding's isEnabled() AND setFieldEnabled() (the dialog's part, FieldEnabling);
/// once the binding's selection is invalid the combo box keeps the last value and is disabled.
///
/// Deviations from OpenRocket: the keys are the platform's (Qt's QComboBox: Up and Down choose
/// the previous or the next value without opening the popup; Java's Down opens it); the wheel
/// chooses only while the combo box has the focus, which the wheel does not give it (Java's combo
/// boxes take no wheel; UnitSpinBox follows the same rule).
class EnumComboBox : public QComboBox
{
    Q_OBJECT

public:
    /// A combo box of @p binding, its values shown as enumText().
    /// @throws BugError when @p binding is null
    template <DisplayedEnum E>
    EnumComboBox(std::shared_ptr<EnumBinding<E>> binding, QWidget* parent)
      : EnumComboBox(std::move(binding), [](E value) { return enumText(value); }, parent)
    {
    }

    /// A combo box of @p binding, its values shown as @p text gives them.
    /// @throws BugError when @p binding or @p text is null
    template <class E>
    EnumComboBox(std::shared_ptr<EnumBinding<E>>                        binding,
                 const std::type_identity_t<std::function<QString(E)>>& text, QWidget* parent)
      : EnumComboBox(modelOf(std::move(binding), text), parent)
    {
    }

    ~EnumComboBox() override;
    EnumComboBox(const EnumComboBox&)            = delete;
    EnumComboBox& operator=(const EnumComboBox&) = delete;
    EnumComboBox(EnumComboBox&&)                 = delete;
    EnumComboBox& operator=(EnumComboBox&&)      = delete;

    /// The binding, as the base class.
    [[nodiscard]] const std::shared_ptr<Binding>& binding() const noexcept
    {
        return m_model.binding;
    }

    /// The binding as an EnumBinding<E>; null when it is of another enum.
    template <class E>
    [[nodiscard]] std::shared_ptr<EnumBinding<E>> enumBinding() const
    {
        return std::dynamic_pointer_cast<EnumBinding<E>>(m_model.binding);
    }

    /// The text the combo box shows: the current item's, or that of a value no item holds.
    [[nodiscard]] QString shownText() const;

    /// Whether the combo box may be used as far as the dialog is concerned (FieldEnabling).
    [[nodiscard]] bool isFieldEnabled() const noexcept { return m_fieldEnabled; }
    void               setFieldEnabled(bool enabled);

    /// Shows the binding's state again, the texts read anew: for a text that changes without an
    /// event of the rocket (a unit in the text, after the default units changed).
    void refresh();

protected:
    void initStyleOption(QStyleOptionComboBox* option) const override;
    void wheelEvent(QWheelEvent* event) override;

private:
    /// What the combo box needs of an EnumBinding<E>, without E.
    struct Model
    {
        std::shared_ptr<Binding>    binding;
        std::function<int()>        size;
        std::function<QString(int)> textAt;
        std::function<int()>        currentIndex;
        std::function<QString()>    valueText;
        std::function<bool(int)>    selectAt;
        std::function<bool()>       isEnabled;
    };

    template <class E>
    [[nodiscard]] static Model modelOf(std::shared_ptr<EnumBinding<E>>  binding,
                                       const std::function<QString(E)>& text)
    {
        if (binding == nullptr || text == nullptr)
        {
            bug("An enum combo box needs a binding and a text for its values");
        }
        const std::shared_ptr<EnumBinding<E>> typed = binding;
        return Model{
            .binding = std::move(binding),
            .size    = [typed] { return typed->size(); },
            .textAt =
                [typed, text](int index) {
                    const std::optional<E> value = typed->elementAt(index);
                    return value.has_value() ? text(*value) : QString();
                },
            .currentIndex = [typed] { return typed->indexOf(typed->value()); },
            .valueText    = [typed, text] { return text(typed->value()); },
            .selectAt =
                [typed](int index) {
                    const std::optional<E> value = typed->elementAt(index);
                    return value.has_value() && typed->select(*value);
                },
            .isEnabled = [typed] { return typed->isEnabled(); },
        };
    }

    EnumComboBox(Model model, QWidget* parent);

    /// A choice of the user's, to the binding.
    void activatedByUser(int index);

    Model                      m_model;
    Signal<>::ScopedConnection m_connection;
    bool                       m_fieldEnabled{true};
    /// The text of the binding's value while no item holds it, else empty.
    QString m_apartText;
};

}  // namespace QtRocket::Gui
