#include "QtRocket/gui/configdialog/InstancesPanel.h"

#include <memory>

#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QString>
#include <QWidget>

#include "QtRocket/gui/binding/ComponentSelection.h"
#include "QtRocket/gui/binding/DoubleBinding.h"
#include "QtRocket/gui/binding/IntBinding.h"
#include "QtRocket/gui/binding/PropertySource.h"
#include "QtRocket/gui/binding/SliderMapping.h"
#include "QtRocket/gui/configdialog/ConfigFields.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/widgets/BindingScope.h"
#include "QtRocket/gui/widgets/IntSpinBox.h"
#include "QtRocket/rocket/LineInstanceable.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/BugError.h"

namespace QtRocket::Gui
{

namespace
{

/// @p component as a LineInstanceable.
/// @throws BugError when it is none
[[nodiscard]] const LineInstanceable& lineInstanceable(const RocketComponent& component)
{
    const auto* instanceable = dynamic_cast<const LineInstanceable*>(&component);
    if (instanceable == nullptr)
    {
        bug("The instances panel needs a component whose instances are spaced along the axis");
    }
    return *instanceable;
}

}  // namespace

IntSource InstancesPanel::instanceCountSource()
{
    return IntSource::of<RocketComponent>({
        .get = [](const RocketComponent& component) { return component.getInstanceCount(); },
        .set = [](RocketComponent& component,
                  const int&       count) { component.setInstanceCount(count); },
    });
}

DoubleSource InstancesPanel::instanceSeparationSource()
{
    return DoubleSource::of<RocketComponent>({
        .get =
            [](const RocketComponent& component) {
                return lineInstanceable(component).getInstanceSeparation();
            },
        .set =
            [](RocketComponent& component, const double& separation) {
                // Java forwards the setter to the selected components whose class has it.
                if (auto* instanceable = dynamic_cast<LineInstanceable*>(&component))
                {
                    instanceable->setInstanceSeparation(separation);
                }
            },
    });
}

InstancesPanel::InstancesPanel(BindingScope& scope, QWidget* parent)
  //: InstancesPanel.title.Instances
  : QGroupBox(tr("Instances"), parent), m_grid(makeChild<QGridLayout>(this))
{
    const std::shared_ptr<ComponentSelection>& selection = scope.selection();
    if (selection == nullptr || !selection->isValid())
    {
        bug("The instances panel needs a valid selection");
    }
    const RocketComponent& main = selection->getMain();
    static_cast<void>(lineInstanceable(main));

    //: InstancesPanel.lbl.InstanceCount
    m_grid->addWidget(makeChild<QLabel>(this, tr("Instance Count:")), 0, 0);
    m_count =
        makeChild<IntSpinBox>(this, scope.make<IntBinding>(selection, instanceCountSource(), 1));
    m_count->setObjectName(QStringLiteral("InstanceCount"));
    m_grid->addWidget(m_count, 0, 1);

    // The slider's range is the parent's length, fixed when the panel is made (Java).
    double maxSeparation = 0.1;
    if (main.getParent() != nullptr && main.getParent()->getLength() > 0)
    {
        maxSeparation = main.getParent()->getLength();
    }
    const auto separation =
        scope.make<DoubleBinding>(selection, instanceSeparationSource(),
                                  unitGroup(UnitGroupId::LENGTH), DoubleBinding::Options{});
    m_separation = ConfigFields::addNumberRow(
        *m_grid, 1,
        //: InstancesPanel.lbl.InstanceSeparation
        tr("Instance Separation:"), separation, QStringLiteral("InstanceSeparation"),
        {.slider = SliderMapping::linear(-maxSeparation, maxSeparation)});
    m_grid->setColumnStretch(3, 1);
}

}  // namespace QtRocket::Gui
