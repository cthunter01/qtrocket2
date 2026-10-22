#include "QtRocket/gui/configdialog/ComponentAssemblyConfig.h"

#include <memory>
#include <numbers>
#include <span>
#include <utility>
#include <vector>

#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QString>
#include <QVBoxLayout>
#include <QWidget>

#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/gui/binding/ComponentSelection.h"
#include "QtRocket/gui/binding/DoubleBinding.h"
#include "QtRocket/gui/binding/EnumBinding.h"
#include "QtRocket/gui/binding/IntBinding.h"
#include "QtRocket/gui/binding/PropertySource.h"
#include "QtRocket/gui/binding/SliderMapping.h"
#include "QtRocket/gui/configdialog/ComponentConfigDialog.h"
#include "QtRocket/gui/configdialog/ConfigFields.h"
#include "QtRocket/gui/configdialog/InstancesPanel.h"
#include "QtRocket/gui/configdialog/PlacementPanel.h"
#include "QtRocket/gui/configdialog/RocketComponentConfig.h"
#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/widgets/BindingScope.h"
#include "QtRocket/gui/widgets/EnumComboBox.h"
#include "QtRocket/gui/widgets/IntSpinBox.h"
#include "QtRocket/gui/widgets/ValueSlider.h"
#include "QtRocket/rocket/Coaxial.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/position/AnglePositionable.h"
#include "QtRocket/rocket/position/RadiusMethod.h"
#include "QtRocket/rocket/position/RadiusPositionable.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Uuid.h"

namespace QtRocket::Gui
{

namespace
{

/// The selection of @p panel, which must be valid and edit a pod set or a stage.
[[nodiscard]] const std::shared_ptr<ComponentSelection>& assemblySelection(
    const RocketComponentConfig& panel)
{
    const std::shared_ptr<ComponentSelection>& selection = panel.selection();
    if (selection == nullptr || !selection->isValid())
    {
        bug("The configuration of an assembly needs a valid selection");
    }
    const ComponentKind kind = selection->getMain().kind();
    if (kind != ComponentKind::POD_SET && kind != ComponentKind::AXIAL_STAGE &&
        kind != ComponentKind::PARALLEL_STAGE)
    {
        bug("The configuration of an assembly needs a pod set or a stage");
    }
    return selection;
}

/// @p component as a RadiusPositionable.
/// @throws BugError when it is none
[[nodiscard]] const RadiusPositionable& radiusPositionable(const RocketComponent& component)
{
    const auto* positionable = dynamic_cast<const RadiusPositionable*>(&component);
    if (positionable == nullptr)
    {
        bug("The radius of an assembly needs a component placed at a radius");
    }
    return *positionable;
}

/// @p component as an AnglePositionable.
/// @throws BugError when it is none
[[nodiscard]] const AnglePositionable& anglePositionable(const RocketComponent& component)
{
    const auto* positionable = dynamic_cast<const AnglePositionable*>(&component);
    if (positionable == nullptr)
    {
        bug("The angle of an assembly needs a component placed at an angle");
    }
    return *positionable;
}

/// Splits each component of @p ids that is still in @p rocket, in order (see
/// ComponentAssemblyConfig): inside one freeze of the rocket when @p freeze is true and the rocket
/// is not frozen already. The removed originals go only after the last split has been told.
void splitEach(Rocket& rocket, const std::vector<Uuid>& ids, bool freeze)
{
    // Declared before the freeze: the originals outlive the thaw that tells of their removal.
    std::vector<RocketComponent::SplitResult> results;
    const bool                                freezing = freeze && !rocket.isFrozen();
    if (freezing)
    {
        rocket.freeze();
    }
    try
    {
        for (const Uuid& id : ids)
        {
            // An assembly inside one split before is in the removed original: not in the rocket.
            if (RocketComponent* component = rocket.findComponent(id))
            {
                results.push_back(component->splitInstances(false));
            }
        }
    }
    catch (...)
    {
        if (freezing)
        {
            rocket.thaw();
        }
        throw;
    }
    if (freezing)
    {
        rocket.thaw();
    }
}

}  // namespace

bool ComponentAssemblyConfig::isOffCenterline(ComponentKind kind) noexcept
{
    return kind == ComponentKind::POD_SET || kind == ComponentKind::PARALLEL_STAGE;
}

EnumSource<RadiusMethod> ComponentAssemblyConfig::radiusMethodSource()
{
    return EnumSource<RadiusMethod>::of<RocketComponent>({
        .get =
            [](const RocketComponent& component) {
                return radiusPositionable(component).getRadiusMethod();
            },
        .set =
            [](RocketComponent& component, const RadiusMethod& method) {
                // Java forwards the setter to the selected components that have it.
                if (auto* positionable = dynamic_cast<RadiusPositionable*>(&component))
                {
                    positionable->setRadiusMethod(method);
                }
            },
    });
}

DoubleSource ComponentAssemblyConfig::radiusOffsetSource()
{
    return DoubleSource::of<RocketComponent>({
        .get =
            [](const RocketComponent& component) {
                return radiusPositionable(component).getRadiusOffset();
            },
        .set =
            [](RocketComponent& component, const double& radius) {
                if (auto* positionable = dynamic_cast<RadiusPositionable*>(&component))
                {
                    positionable->setRadiusOffset(radius);
                }
            },
    });
}

DoubleSource ComponentAssemblyConfig::angleOffsetSource()
{
    return DoubleSource::of<RocketComponent>({
        .get =
            [](const RocketComponent& component) {
                return anglePositionable(component).getAngleOffset();
            },
        .set =
            [](RocketComponent& component, const double& angle) {
                if (auto* positionable = dynamic_cast<AnglePositionable*>(&component))
                {
                    positionable->setAngleOffset(angle);
                }
            },
    });
}

DoubleSource ComponentAssemblyConfig::parentOuterRadiusSource()
{
    return DoubleSource::of<RocketComponent>({
        .get =
            [](const RocketComponent& component) {
                const auto* parent = dynamic_cast<const Coaxial*>(component.getParent());
                return parent != nullptr ? parent->getOuterRadius() : 0.0;
            },
    });
}

ComponentAssemblyConfig::ComponentAssemblyConfig(OpenRocketDocument&                 document,
                                                 const GuiServices&                  services,
                                                 std::shared_ptr<ComponentSelection> selection,
                                                 QWidget*                            parent)
  : RocketComponentConfig(document, services, std::move(selection), parent)
{
    // Only the assemblies off the centerline get the tab and the button (Java).
    if (isOffCenterline(assemblySelection(*this)->getMain().kind()))
    {
        addAssemblyTab();
        setSelectedTabIndex(0);
        addSplitButton();
    }
}

void ComponentAssemblyConfig::addAssemblyTab()
{
    const std::shared_ptr<ComponentSelection>& selection = assemblySelection(*this);
    BindingScope&                              bindings  = scope();

    m_assembly.page = makeChild<QWidget>(this);
    auto* layout    = makeChild<QVBoxLayout>(m_assembly.page);
    auto* fields    = makeChild<QWidget>(m_assembly.page);
    auto* grid      = makeChild<QGridLayout>(fields);
    grid->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(fields);

    // The radius method; a change converts the offset, which its field shows at the next event.
    //: RocketComponent.Position.Method.Radius.Label
    grid->addWidget(makeChild<QLabel>(fields, tr("Radius relative to:")), 0, 0);
    m_assembly.radiusMethod = makeChild<EnumComboBox>(
        fields,
        bindings.make<EnumBinding<RadiusMethod>>(
            selection, radiusMethodSource(), std::span<const RadiusMethod>(kRadiusMethodChoices)));
    m_assembly.radiusMethod->setObjectName(QStringLiteral("RadiusMethod"));
    grid->addWidget(m_assembly.radiusMethod, 0, 1, 1, 3);

    // The radial distance, at least 0, its slider up to four times the parent's outer radius.
    const auto radius    = bindings.make<DoubleBinding>(selection, radiusOffsetSource(),
                                                        unitGroup(UnitGroupId::LENGTH),
                                                        DoubleBinding::Options{.min = 0.0});
    const auto maxRadius = bindings.make<DoubleBinding>(selection, parentOuterRadiusSource(),
                                                        unitGroup(UnitGroupId::LENGTH),
                                                        DoubleBinding::Options{.multiplier = 4.0});
    m_assembly.radius    = ConfigFields::addNumberRow(
        *grid, 1,
        //: ComponentAssemblyConfig.parallel.radius
        tr("Radial Distance:"), radius, QStringLiteral("RadiusOffset"),
        {.slider = SliderMapping::linear(0, maxRadius->value()), .bounds = {.max = maxRadius}});

    // The angle, -pi to pi.
    const auto angle = bindings.make<DoubleBinding>(
        selection, angleOffsetSource(), unitGroup(UnitGroupId::ANGLE),
        DoubleBinding::Options{.min = -std::numbers::pi, .max = std::numbers::pi});
    m_assembly.angle = ConfigFields::addNumberRow(
        *grid, 2,
        //: ComponentAssemblyConfig.parallel.angle
        tr("Angle:"), angle, QStringLiteral("AngleOffset"),
        {.slider = SliderMapping::linear(-std::numbers::pi, std::numbers::pi)});

    // The number of copies, at least 1.
    //: ComponentAssemblyConfig.parallel.count
    grid->addWidget(makeChild<QLabel>(fields, tr("Number of Copies:")), 3, 0);
    m_assembly.count = makeChild<IntSpinBox>(
        fields, bindings.make<IntBinding>(selection, InstancesPanel::instanceCountSource(), 1));
    m_assembly.count->setObjectName(QStringLiteral("InstanceCount"));
    grid->addWidget(m_assembly.count, 3, 1);
    grid->setColumnStretch(3, 1);

    // The placement, below a gap (Java: "wrap 30lp").
    layout->addSpacing(30);
    m_assembly.placement = makeChild<PlacementPanel>(m_assembly.page, bindings);
    layout->addWidget(m_assembly.placement);
    layout->addStretch(1);

    insertTab(0, *m_assembly.page,
              //: RocketCompCfg.tab.Assembly
              tr("General"),
              //: RocketCompCfg.tab.AssemblyComment
              tr("Options for locating stages parallel to other stages"));
}

void ComponentAssemblyConfig::addSplitButton()
{
    const RocketComponent& main = assemblySelection(*this)->getMain();
    QString                text;
    QString                toolTip;
    if (main.kind() == ComponentKind::POD_SET)
    {
        //: ComponentAssemblyConfig.but.splitPods
        text = tr("Split Pods");
        //: ComponentAssemblyConfig.but.splitPods.ttip
        toolTip = tr("Split the pod set into separate pod components.");
    }
    else
    {
        //: ComponentAssemblyConfig.but.splitBoosters
        text = tr("Split Boosters");
        //: ComponentAssemblyConfig.but.splitBoosters.ttip
        toolTip = tr("Split the booster into separate booster components.");
    }
    m_split = makeChild<QPushButton>(this, text);
    m_split->setObjectName(QStringLiteral("splitButton"));
    m_split->setToolTip(toolTip);
    m_split->setEnabled(main.getInstanceCount() > 1);
    connect(m_split, &QPushButton::clicked, this,
            guarded([this] { splitInstances(); }, QStringLiteral("splitButton")));
    addButton(*m_split);
}

void ComponentAssemblyConfig::updateFields()
{
    RocketComponentConfig::updateFields();
    const std::shared_ptr<ComponentSelection>& valid = selection();
    if (m_split != nullptr && valid != nullptr && valid->isValid())
    {
        m_split->setEnabled(valid->getMain().getInstanceCount() > 1);
    }
}

void ComponentAssemblyConfig::splitInstances()
{
    // The pending edits first: the copies take the assemblies as the fields show them.
    static_cast<void>(commitAll());
    const std::shared_ptr<ComponentSelection> selection = this->selection();
    if (selection == nullptr || !selection->isValid())
    {
        return;
    }
    // The main component first, then the others (Java: the config listeners), by id: the dialog
    // closes before the split, and with it the selection.
    const RocketComponent& main = selection->getMain();
    std::vector<Uuid>      ids{main.getId()};
    for (const RocketComponent* other : selection->getOthers())
    {
        ids.push_back(other->getId());
    }
    const bool          pods     = main.kind() == ComponentKind::POD_SET;
    OpenRocketDocument& document = this->document();
    Rocket&             rocket   = document.getRocket();

    // Java's disposeDialog(): the dialog that shows this panel closes first.
    if (const ComponentConfigDialog* dialog = ComponentConfigDialog::dialog();
        dialog != nullptr && &dialog->configurator() == this)
    {
        ComponentConfigDialog::disposeDialog();
    }

    //: Not translated in OpenRocket (ComponentAssemblyConfig.java): the undo step of the split
    document.startUndo(QtStrings::toUtf8(tr("Split assembly")));
    try
    {
        // Java: splitInstances(true) for a pod set, splitInstances(false) for a booster set.
        splitEach(rocket, ids, pods);
    }
    catch (...)
    {
        document.stopUndo();
        throw;
    }
    document.stopUndo();
}

}  // namespace QtRocket::Gui
