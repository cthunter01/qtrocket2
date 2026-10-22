#include "QtRocket/gui/widgets/BoolCheckBox.h"

#include <algorithm>
#include <memory>
#include <utility>

#include <QAbstractButton>
#include <QCheckBox>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QWidget>

#include "QtRocket/gui/binding/Binding.h"
#include "QtRocket/gui/binding/BoolBinding.h"
#include "QtRocket/gui/binding/DoubleBinding.h"
#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/gui/widgets/FieldEnabling.h"
#include "QtRocket/util/BugError.h"

namespace QtRocket::Gui
{

namespace
{

/// @p binding, which must not be null.
template <class B>
[[nodiscard]] std::shared_ptr<B> checkedBinding(std::shared_ptr<B> binding)
{
    if (binding == nullptr)
    {
        bug("A check box needs a binding");
    }
    return binding;
}

}  // namespace

BoolCheckBox::BoolCheckBox(std::shared_ptr<BoolBinding> binding, const QString& text,
                           QWidget* parent)
  : QCheckBox(text, parent), m_binding(checkedBinding(std::move(binding)))
{
    setUp(m_binding);
}

BoolCheckBox::BoolCheckBox(std::shared_ptr<DoubleBinding> numberBinding, const QString& text,
                           QWidget* parent)
  : QCheckBox(text, parent), m_numberBinding(checkedBinding(std::move(numberBinding)))
{
    setUp(m_numberBinding);
}

BoolCheckBox::~BoolCheckBox() = default;

void BoolCheckBox::setUp(const std::shared_ptr<Binding>& binding)
{
    m_connection =
        binding->addChangeListener([this] { Guard::run([this] { refresh(); }, objectName()); });
    connect(this, &QAbstractButton::clicked, this,
            [this](bool checked) { clickedByUser(checked); });
    refresh();
}

bool BoolCheckBox::bindingValue() const
{
    return m_binding != nullptr ? m_binding->value() : m_numberBinding->isAutomatic();
}

bool BoolCheckBox::bindingEnabled() const
{
    if (m_binding != nullptr)
    {
        return m_binding->isEnabled();
    }
    return m_numberBinding->isEnabled() && m_numberBinding->isAutomaticAvailable();
}

void BoolCheckBox::refresh()
{
    setChecked(bindingValue());
    setEnabled(m_fieldEnabled && bindingEnabled());
}

void BoolCheckBox::setFieldEnabled(bool enabled)
{
    m_fieldEnabled = enabled;
    setEnabled(m_fieldEnabled && bindingEnabled());
}

void BoolCheckBox::clickedByUser(bool checked)
{
    // The binding's listeners may delete the dialog (with deleteLater(), or at once).
    const QPointer<BoolCheckBox> self(this);
    Guard::run(
        [this, checked] {
            if (m_binding != nullptr)
            {
                const std::shared_ptr<BoolBinding> binding = m_binding;
                static_cast<void>(binding->setChecked(checked));
            }
            else
            {
                const std::shared_ptr<DoubleBinding> binding = m_numberBinding;
                binding->setAutomatic(checked);
            }
        },
        objectName());
    if (!self.isNull())
    {
        refresh();  // whatever came of it: a dropped or refused write fires nothing
    }
}

void BoolCheckBox::addEnableFollower(QWidget& follower, bool enableState)
{
    if (m_binding == nullptr)
    {
        bug("An automatic check box has no enable followers");
    }
    removeEnableFollower(follower);
    const QPointer<QWidget> target(&follower);
    Follower                entry;
    entry.widget     = &follower;
    entry.destroyed  = connect(&follower, &QObject::destroyed, this,
                               [this](QObject* gone) { forgetFollower(gone); });
    entry.connection = m_binding->addEnableFollower(
        [target](bool enabled) {
            if (!target.isNull())
            {
                FieldEnabling::setFieldEnabled(*target, enabled);
            }
        },
        enableState);
    m_followers.push_back(std::move(entry));
}

void BoolCheckBox::removeEnableFollower(const QWidget& follower)
{
    forgetFollower(&follower);
}

bool BoolCheckBox::hasEnableFollower(const QWidget& widget) const
{
    return std::ranges::any_of(
        m_followers, [&widget](const Follower& entry) { return entry.widget == &widget; });
}

void BoolCheckBox::forgetFollower(const QObject* follower)
{
    const auto found = std::ranges::find_if(
        m_followers, [follower](const Follower& entry) { return entry.widget == follower; });
    if (found == m_followers.end())
    {
        return;
    }
    disconnect(found->destroyed);
    m_followers.erase(found);  // its ScopedConnection disconnects from the binding
}

}  // namespace QtRocket::Gui
