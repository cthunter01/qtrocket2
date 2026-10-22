#include "BodyPanelTestSupport.h"

#include <algorithm>
#include <initializer_list>
#include <limits>
#include <string>
#include <vector>

#include <QString>
#include <QWidget>
#include <Qt>

#include "ConfigDialogTestSupport.h"
#include "QtRocket/gui/binding/DoubleBinding.h"
#include "QtRocket/gui/binding/SliderMapping.h"
#include "QtRocket/gui/configdialog/ConfigFields.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/widgets/UnitSelector.h"
#include "QtRocket/gui/widgets/UnitSpinBox.h"
#include "QtRocket/gui/widgets/ValueSlider.h"

namespace QtRocket::Gui::Test
{

std::string missingNumberRows(const QWidget& root, std::initializer_list<const char*> properties)
{
    std::string missing;
    for (const char* property : properties)
    {
        const QString name = QString::fromLatin1(property);
        if (childNamed<UnitSpinBox>(root, name) == nullptr ||
            childNamed<UnitSelector>(root, name) == nullptr ||
            childNamed<ValueSlider>(root, name) == nullptr)
        {
            missing += std::string(property) + '\n';
        }
    }
    return missing;
}

bool takesFromZeroUp(const ConfigFields::NumberWidgets& widgets)
{
    const DoubleBinding& binding = *widgets.spinBox->binding();
    return binding.minimum() == 0.0 && binding.maximum() == std::numeric_limits<double>::infinity();
}

bool hasLinearSlider(const ConfigFields::NumberWidgets& widgets, double min, double max)
{
    const SliderMapping& mapping = widgets.slider->mapping();
    return mapping.isLinear() && mapping.getMin() == min && mapping.getMax() == max;
}

std::vector<std::string> focusOrder(const QWidget&                     root,
                                    std::initializer_list<const char*> properties)
{
    std::vector<std::string> order;
    for (const QWidget* widget = root.nextInFocusChain(); widget != &root;
         widget                = widget->nextInFocusChain())
    {
        if (!root.isAncestorOf(widget) || (widget->focusPolicy() & Qt::TabFocus) == 0)
        {
            continue;
        }
        const std::string name   = QtStrings::toUtf8(widget->objectName());
        const bool        listed = std::ranges::any_of(
            properties, [&name](const char* property) { return name == property; });
        if (listed && (order.empty() || order.back() != name))
        {
            order.push_back(name);
        }
    }
    return order;
}

}  // namespace QtRocket::Gui::Test
