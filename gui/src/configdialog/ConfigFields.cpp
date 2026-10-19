#include "QtRocket/gui/configdialog/ConfigFields.h"

#include <memory>

#include <QGridLayout>
#include <QLabel>
#include <QString>
#include <QWidget>

#include "QtRocket/gui/binding/DoubleBinding.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/widgets/UnitSelector.h"
#include "QtRocket/gui/widgets/UnitSpinBox.h"
#include "QtRocket/gui/widgets/ValueSlider.h"
#include "QtRocket/util/BugError.h"

namespace QtRocket::Gui::ConfigFields
{

NumberWidgets addNumberRow(QGridLayout& grid, int row, const QString& label,
                           const std::shared_ptr<DoubleBinding>& binding, const QString& property,
                           const NumberRowOptions& options)
{
    if (binding == nullptr)
    {
        bug("A number row needs a binding");
    }
    QWidget* parent = grid.parentWidget();
    if (parent == nullptr)
    {
        bug("A number row needs a grid that is set on its widget");
    }
    NumberWidgets widgets;
    if (!label.isEmpty())
    {
        widgets.label = makeChild<QLabel>(parent, label);
        grid.addWidget(widgets.label, row, 0);
    }
    widgets.spinBox = makeChild<UnitSpinBox>(parent, binding);
    widgets.spinBox->setObjectName(property);
    grid.addWidget(widgets.spinBox, row, 1);
    if (options.unitSelector)
    {
        widgets.unitSelector = makeChild<UnitSelector>(parent, binding);
        widgets.unitSelector->setObjectName(property);
        grid.addWidget(widgets.unitSelector, row, 2);
    }
    if (options.slider.has_value())
    {
        const SliderMapping& mapping = *options.slider;
        widgets.slider = makeChild<ValueSlider>(parent, binding, mapping, options.bounds);
        widgets.slider->setObjectName(property);
        grid.addWidget(widgets.slider, row, 3);
    }
    return widgets;
}

}  // namespace QtRocket::Gui::ConfigFields
