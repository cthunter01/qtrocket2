#include "QtRocket/gui/configdialog/InnerTubeConfig.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <format>
#include <memory>
#include <numbers>
#include <optional>
#include <utility>
#include <vector>

#include <QButtonGroup>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QString>
#include <QVBoxLayout>
#include <QWidget>
#include <Qt>

#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/gui/binding/ComponentSelection.h"
#include "QtRocket/gui/binding/DoubleBinding.h"
#include "QtRocket/gui/binding/PropertySource.h"
#include "QtRocket/gui/binding/SliderMapping.h"
#include "QtRocket/gui/configdialog/ClusterSelectionPanel.h"
#include "QtRocket/gui/configdialog/ConfigFields.h"
#include "QtRocket/gui/configdialog/MotorConfig.h"
#include "QtRocket/gui/configdialog/RingComponentConfig.h"
#include "QtRocket/gui/configdialog/RocketComponentConfig.h"
#include "QtRocket/gui/settings/GuiPreferenceKeys.h"
#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/gui/util/GuiServices.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/widgets/BindingScope.h"
#include "QtRocket/gui/widgets/DescriptionArea.h"
#include "QtRocket/gui/widgets/UnitSelector.h"
#include "QtRocket/gui/widgets/UnitSpinBox.h"
#include "QtRocket/gui/widgets/ValueSlider.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/rocket/InnerTube.h"
#include "QtRocket/rocket/RingComponent.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Coordinate.h"

namespace QtRocket::Gui
{

namespace
{

/// The property names of the separation field's two values.
constexpr const char* kRelativeProperty = "ClusterScale";
constexpr const char* kAbsoluteProperty = "ClusterScaleAbsolute";

/// The selection of @p panel, which must be valid and edit an inner tube.
[[nodiscard]] const std::shared_ptr<ComponentSelection>& tubeSelection(
    const RocketComponentConfig& panel)
{
    const std::shared_ptr<ComponentSelection>& selection = panel.selection();
    if (selection == nullptr || !selection->isValid())
    {
        bug("The configuration of an inner tube needs a valid selection");
    }
    if (dynamic_cast<const InnerTube*>(&selection->getMain()) == nullptr)
    {
        bug("The configuration of an inner tube needs an inner tube");
    }
    return selection;
}

/// @p panel, whose selection must be valid and edit an inner tube (tubeSelection()).
[[nodiscard]] RocketComponentConfig& checkedTubePanel(RocketComponentConfig& panel)
{
    static_cast<void>(tubeSelection(panel));
    return panel;
}

/// The relative separation's slider (Java: getSliderModel(0, 1, 4)).
[[nodiscard]] SliderMapping relativeSliderMapping()
{
    return SliderMapping::fromRange(0, 1, 4);
}

/// Sets @p toolTip on the label, the spin box and the slider of @p widgets (Java sets it on each
/// of them, not on the unit selector).
void setRowToolTip(const ConfigFields::NumberWidgets& widgets, const QString& toolTip)
{
    for (QWidget* widget : std::array<QWidget*, 3>{widgets.label, widgets.spinBox, widgets.slider})
    {
        if (widget != nullptr)
        {
            widget->setToolTip(toolTip);
        }
    }
}

/// Names the spin box, the unit selector and the slider of @p widgets @p property.
void nameRow(const ConfigFields::NumberWidgets& widgets, const QString& property)
{
    widgets.spinBox->setObjectName(property);
    widgets.unitSelector->setObjectName(property);
    widgets.slider->setObjectName(property);
}

}  // namespace

DoubleSource InnerTubeConfig::clusterScaleSource()
{
    return DoubleSource::of<InnerTube>({
        .get = [](const InnerTube& tube) { return tube.getClusterScale(); },
        .set = [](InnerTube& tube, const double& scale) { tube.setClusterScale(scale); },
    });
}

DoubleSource InnerTubeConfig::clusterScaleAbsoluteSource()
{
    return DoubleSource::of<InnerTube>({
        .get = [](const InnerTube& tube) { return tube.getClusterScaleAbsolute(); },
        .set = [](InnerTube& tube, const double& gap) { tube.setClusterScaleAbsolute(gap); },
    });
}

DoubleSource InnerTubeConfig::clusterRotationSource()
{
    return DoubleSource::of<InnerTube>({
        .get = [](const InnerTube& tube) { return tube.getClusterRotation(); },
        .set = [](InnerTube& tube, const double& rotation) { tube.setClusterRotation(rotation); },
    });
}

DoubleSource InnerTubeConfig::radialPositionSource()
{
    return DoubleSource::of<RingComponent>({
        .get = [](const RingComponent& ring) { return ring.getRadialPosition(); },
        .set = [](RingComponent& ring,
                  const double&  position) { ring.setRadialPosition(position); },
    });
}

DoubleSource InnerTubeConfig::radialDirectionSource()
{
    return DoubleSource::of<RingComponent>({
        .get = [](const RingComponent& ring) { return ring.getRadialDirection(); },
        .set = [](RingComponent& ring,
                  const double&  direction) { ring.setRadialDirection(direction); },
    });
}

InnerTubeConfig::InnerTubeConfig(OpenRocketDocument& document, const GuiServices& services,
                                 std::shared_ptr<ComponentSelection> selection, QWidget* parent)
  : RocketComponentConfig(document, services, std::move(selection), parent),
    // The General tab first, then the motor tab: their fields take the focus in that order.
    m_general(RingComponentConfig::makeGeneralTab(checkedTubePanel(*this),
                                                  {//: ThicknessRingCompCfg.tab.Length
                                                   .length = tr("Length:"),
                                                   //: ThicknessRingCompCfg.tab.Outerdiam
                                                   .outer = tr("Outer diameter:"),
                                                   //: ThicknessRingCompCfg.tab.Innerdiam
                                                   .inner = tr("Inner diameter:"),
                                                   //: ThicknessRingCompCfg.tab.Wallthickness
                                                   .thickness = tr("Wall thickness:")})),
    m_motor(makeChild<MotorConfig>(this, scope()))
{
    insertTab(0, *m_general.page,
              //: ThicknessRingCompCfg.tab.General
              tr("General"),
              //: ThicknessRingCompCfg.tab.Generalprop
              tr("General properties"));
    insertTab(1, *m_motor,
              //: InnerTubeCfg.tab.Motor
              tr("Motor"),
              //: InnerTubeCfg.tab.ttip.Motor
              tr("Motor mount configuration"));

    addClusterTab();
    addRadialPositionTab();
    setSelectedTabIndex(0);
}

void InnerTubeConfig::addClusterTab()
{
    const std::shared_ptr<ComponentSelection>& selection = tubeSelection(*this);
    BindingScope&                              bindings  = scope();
    auto*                                      page      = makeChild<QWidget>(this);
    auto*                                      layout    = makeChild<QHBoxLayout>(page);

    // The layouts.
    auto* choice       = makeChild<QWidget>(page);
    auto* choiceLayout = makeChild<QVBoxLayout>(choice);
    choiceLayout->setContentsMargins(0, 0, 0, 0);
    //: InnerTubeCfg.lbl.Selectclustercfg
    choiceLayout->addWidget(makeChild<QLabel>(choice, tr("Select cluster configuration:")));
    m_cluster = makeChild<ClusterSelectionPanel>(choice, bindings);
    choiceLayout->addWidget(m_cluster, 0, Qt::AlignLeft);
    choiceLayout->addStretch(1);
    layout->addWidget(choice, 0, Qt::AlignTop);

    auto* fields = makeChild<QWidget>(page);
    auto* grid   = makeChild<QGridLayout>(fields);
    layout->addWidget(fields, 1, Qt::AlignTop);

    // The separation: one field showing the relative separation or the absolute one, as the
    // preference says (Java: the radio buttons swap the field's models).
    m_relativeSeparation =
        bindings.make<DoubleBinding>(selection, clusterScaleSource(), unitGroup(UnitGroupId::NONE),
                                     DoubleBinding::Options{.min = 0.0});
    m_absoluteSeparation =
        bindings.make<DoubleBinding>(selection, clusterScaleAbsoluteSource(),
                                     unitGroup(UnitGroupId::LENGTH), DoubleBinding::Options{});
    const DoubleSource outerRadius = RingComponentConfig::outerRadiusSource(selection->getMain());
    m_absoluteMin =
        bindings.make<DoubleBinding>(selection, outerRadius, unitGroup(UnitGroupId::LENGTH),
                                     DoubleBinding::Options{.multiplier = -2.0});
    m_absoluteMax =
        bindings.make<DoubleBinding>(selection, outerRadius, unitGroup(UnitGroupId::LENGTH),
                                     DoubleBinding::Options{.multiplier = 6.0});
    //: InnerTubeCfg.lbl.ttip.TubeSep
    m_relativeToolTip = tr("The separation of the tubes, 1.0 = touching each other");
    //: InnerTubeCfg.lbl.ttip.TubeSepAbs
    m_absoluteToolTip = tr("The separation of the tubes, 0 = touching each other");
    m_separationRelative =
        services().preferences().getBoolean(GuiPreferenceKeys::kInnerTubeSeparationRelative,
                                            GuiPreferenceKeys::kInnerTubeSeparationRelativeDefault);
    m_separation = ConfigFields::addNumberRow(*grid, 0,
                                              //: InnerTubeCfg.lbl.TubeSep
                                              tr("Tube separation:"), m_relativeSeparation,
                                              QString::fromLatin1(kRelativeProperty),
                                              {.slider = relativeSliderMapping()});
    m_separation.label->setToolTip(m_relativeToolTip);
    showSeparation(m_separationRelative);

    // Relative or absolute.
    auto* choices       = makeChild<QWidget>(fields);
    auto* choicesLayout = makeChild<QHBoxLayout>(choices);
    choicesLayout->setContentsMargins(0, 0, 0, 0);
    //: InnerTubeCfg.radioBut.Relative
    m_relativeButton = makeChild<QRadioButton>(choices, tr("Relative"));
    m_relativeButton->setObjectName(QStringLiteral("relativeButton"));
    //: InnerTubeCfg.radioBut.Relative.ttip
    m_relativeButton->setToolTip(
        tr("The separation is measured relative to the outer diameter of the inner tube"));
    //: InnerTubeCfg.radioBut.Absolute
    m_absoluteButton = makeChild<QRadioButton>(choices, tr("Absolute"));
    m_absoluteButton->setObjectName(QStringLiteral("absoluteButton"));
    //: InnerTubeCfg.radioBut.Absolute.ttip
    m_absoluteButton->setToolTip(tr("The separation is measured in length units"));
    auto* group = makeChild<QButtonGroup>(choices);
    group->addButton(m_relativeButton);
    group->addButton(m_absoluteButton);
    choicesLayout->addWidget(m_relativeButton);
    choicesLayout->addWidget(m_absoluteButton);
    choicesLayout->addStretch(1);
    grid->addWidget(choices, 1, 1, 1, 3);
    (m_separationRelative ? m_relativeButton : m_absoluteButton)->setChecked(true);
    // Connected after the initial choice: only the user's choices are stored.
    connect(m_relativeButton, &QRadioButton::toggled, this,
            guarded(
                [this](bool checked) {
                    if (checked)
                    {
                        chooseSeparation(true);
                    }
                },
                QStringLiteral("relativeButton")));
    connect(m_absoluteButton, &QRadioButton::toggled, this,
            guarded(
                [this](bool checked) {
                    if (checked)
                    {
                        chooseSeparation(false);
                    }
                },
                QStringLiteral("absoluteButton")));

    // The rotation, -pi to pi (Java: DoubleModel(..., -Math.PI, Math.PI) and its slider over
    // that range).
    const auto rotation = bindings.make<DoubleBinding>(
        selection, clusterRotationSource(), unitGroup(UnitGroupId::ANGLE),
        DoubleBinding::Options{.min = -std::numbers::pi, .max = std::numbers::pi});
    m_rotation = ConfigFields::addNumberRow(
        *grid, 2,
        //: InnerTubeCfg.lbl.Rotation
        tr("Rotation:"), rotation, QStringLiteral("ClusterRotation"),
        {.slider = SliderMapping::linear(-std::numbers::pi, std::numbers::pi)});
    //: InnerTubeCfg.lbl.ttip.Rotation
    setRowToolTip(m_rotation, tr("Rotation angle of the cluster configuration"));

    // Split cluster and Reset settings, of one size, at the right.
    //: InnerTubeCfg.but.Splitcluster
    m_splitButton = makeChild<QPushButton>(fields, tr("Split cluster"));
    m_splitButton->setObjectName(QStringLiteral("splitClusterButton"));
    //: InnerTubeCfg.lbl.longA1
    m_splitButton->setToolTip(tr("<html>Split the cluster into separate components. <br>") +
                              //: InnerTubeCfg.lbl.longA2
                              tr("This also duplicates all components attached to this inner "
                                 "tube."));
    connect(m_splitButton, &QPushButton::clicked, this,
            guarded([this] { splitCluster(); }, QStringLiteral("splitClusterButton")));
    //: InnerTubeCfg.but.Resetsettings
    m_resetCluster = makeChild<QPushButton>(fields, tr("Reset settings"));
    m_resetCluster->setObjectName(QStringLiteral("resetClusterButton"));
    //: InnerTubeCfg.but.ttip.Resetsettings
    m_resetCluster->setToolTip(tr("Reset the separation and rotation to the default values"));
    connect(m_resetCluster, &QPushButton::clicked, this,
            guarded([this] { resetClusterSettings(); }, QStringLiteral("resetClusterButton")));
    const int width =
        std::max(m_splitButton->sizeHint().width(), m_resetCluster->sizeHint().width());
    m_splitButton->setMinimumWidth(width);
    m_resetCluster->setMinimumWidth(width);
    auto* buttons       = makeChild<QWidget>(fields);
    auto* buttonsLayout = makeChild<QHBoxLayout>(buttons);
    buttonsLayout->setContentsMargins(0, 0, 0, 0);
    buttonsLayout->addStretch(1);
    buttonsLayout->addWidget(m_splitButton);
    buttonsLayout->addWidget(m_resetCluster);
    grid->addWidget(buttons, 3, 0, 1, 4);
    grid->setColumnStretch(3, 1);
    grid->setRowStretch(4, 1);

    insertTab(2, *page,
              //: InnerTubeCfg.tab.Cluster
              tr("Cluster"),
              //: InnerTubeCfg.tab.ttip.Cluster
              tr("Cluster configuration"));
}

void InnerTubeConfig::addRadialPositionTab()
{
    const std::shared_ptr<ComponentSelection>& selection = tubeSelection(*this);
    BindingScope&                              bindings  = scope();
    auto*                                      page      = makeChild<QWidget>(this);
    auto*                                      grid      = makeChild<QGridLayout>(page);

    // The distance from the centerline, at least 0.
    const auto position = bindings.make<DoubleBinding>(selection, radialPositionSource(),
                                                       unitGroup(UnitGroupId::LENGTH),
                                                       DoubleBinding::Options{.min = 0.0});
    m_radialPosition    = ConfigFields::addNumberRow(
        *grid, 0,
        //: ringcompcfg.Radialdistance
        tr("Radial distance:"), position, QStringLiteral("RadialPosition"),
        {.slider = SliderMapping::fromRange(0, 0.1, 1.0)});
    //: ringcompcfg.Distancefrom
    setRowToolTip(m_radialPosition, tr("Distance from the rocket centerline"));

    // The direction, any angle (the core reduces it to -pi ... pi).
    const auto direction =
        bindings.make<DoubleBinding>(selection, radialDirectionSource(),
                                     unitGroup(UnitGroupId::ANGLE), DoubleBinding::Options{});
    m_radialDirection = ConfigFields::addNumberRow(
        *grid, 1,
        //: ringcompcfg.Radialdirection
        tr("Radial direction:"), direction, QStringLiteral("RadialDirection"),
        {.slider = SliderMapping::linear(-std::numbers::pi, std::numbers::pi)});
    //: ringcompcfg.radialdirectionfrom
    setRowToolTip(m_radialDirection, tr("The radial direction from the rocket centerline"));

    //: ringcompcfg.but.Reset
    m_resetRadial = makeChild<QPushButton>(page, tr("Reset"));
    m_resetRadial->setObjectName(QStringLiteral("resetRadialButton"));
    //: ringcompcfg.but.Resetcomponant
    m_resetRadial->setToolTip(tr("Resets the component to the rocket centerline."));
    connect(m_resetRadial, &QPushButton::clicked, this,
            guarded([this] { resetRadialPosition(); }, QStringLiteral("resetRadialButton")));
    grid->addWidget(m_resetRadial, 2, 0, 1, 4, Qt::AlignRight);

    m_radialNote = makeChild<DescriptionArea>(
        page,
        //: ringcompcfg.note.desc
        tr("Note: An inner tube will not affect the aerodynamics of the rocket even if it is "
           "located outside of the body tube."),
        4);
    m_radialNote->setObjectName(QStringLiteral("radialNote"));
    grid->addWidget(m_radialNote, 3, 0, 1, 4);
    grid->setColumnStretch(3, 1);
    grid->setRowStretch(4, 1);

    insertTab(3, *page,
              //: InnerTubeCfg.tab.Radialpos
              tr("Radial position"),
              //: InnerTubeCfg.tab.ttip.Radialpos
              tr("Radial position"));
}

void InnerTubeConfig::showSeparation(bool relative)
{
    m_separationRelative = relative;
    const std::shared_ptr<DoubleBinding>& binding =
        relative ? m_relativeSeparation : m_absoluteSeparation;
    m_separation.spinBox->setBinding(binding);
    m_separation.unitSelector->setBinding(binding);
    if (relative)
    {
        m_separation.slider->setBinding(binding, relativeSliderMapping());
    }
    else
    {
        // From -2 to 6 times the outer radius, following it (Java: getSliderModel(
        // DoubleModel(component, "OuterRadius", -2), DoubleModel(component, "OuterRadius", 6))).
        m_separation.slider->setBinding(
            binding, SliderMapping::linear(m_absoluteMin->value(), m_absoluteMax->value()),
            ValueSlider::DynamicBounds{.min = m_absoluteMin, .max = m_absoluteMax});
    }
    nameRow(m_separation, QString::fromLatin1(relative ? kRelativeProperty : kAbsoluteProperty));
    m_separation.spinBox->setToolTip(relative ? m_relativeToolTip : m_absoluteToolTip);
}

void InnerTubeConfig::chooseSeparation(bool relative)
{
    // The pending edit of the field first (a radio button takes no focus on macOS).
    static_cast<void>(m_separation.spinBox->commitEdit());
    showSeparation(relative);
    // The user's decision U4: "Relative" stores true (Java stores false for both).
    services().preferences().putBoolean(GuiPreferenceKeys::kInnerTubeSeparationRelative, relative);
}

void InnerTubeConfig::resetClusterSettings()
{
    const std::shared_ptr<ComponentSelection>& selection = this->selection();
    if (!selection->isValid())
    {
        return;
    }
    selection->apply<InnerTube>([](InnerTube& tube) {
        tube.setClusterScale(1.0);
        tube.setClusterRotation(0.0);
    });
}

void InnerTubeConfig::resetRadialPosition()
{
    const std::shared_ptr<ComponentSelection>& selection = this->selection();
    if (!selection->isValid())
    {
        return;
    }
    selection->apply<RingComponent>([](RingComponent& ring) {
        ring.setRadialDirection(0.0);
        ring.setRadialPosition(0.0);
    });
}

std::unique_ptr<RocketComponent> InnerTubeConfig::splitClusterOf(InnerTube& tube)
{
    RocketComponent* parent = tube.getParent();
    if (parent == nullptr)
    {
        bug("Split cluster: the inner tube has no parent");
    }
    const std::optional<std::size_t> index = parent->getChildPosition(&tube);
    if (!index.has_value())
    {
        bug("Split cluster: inconsistent state, the inner tube is not among its parent's "
            "children");
    }
    if (tube.getInstanceCount() <= 1)
    {
        return nullptr;
    }
    const std::vector<Coordinate>    locations = tube.getComponentLocations();
    std::unique_ptr<RocketComponent> removed   = parent->removeChild(*index);
    // The copies are made of the removed tube, which lives on in `removed`.
    for (std::size_t i = 0; i < locations.size(); ++i)
    {
        parent->addChild(InnerTube::makeIndividualClusterComponent(
                             locations[i], std::format("{} #{}", tube.getName(), i + 1), tube),
                         *index + i);
    }
    return removed;
}

void InnerTubeConfig::splitCluster()
{
    // The pending edits first: the copies take the tubes as the fields show them.
    static_cast<void>(commitAll());
    const std::shared_ptr<ComponentSelection>& selection = this->selection();
    if (!selection->isValid())
    {
        return;
    }
    // The main tube first, then the others (Java: the config listeners that are inner tubes).
    std::vector<InnerTube*> tubes;
    if (auto* main = dynamic_cast<InnerTube*>(&selection->getMain()))
    {
        tubes.push_back(main);
    }
    for (RocketComponent* other : selection->getOthers())
    {
        if (auto* tube = dynamic_cast<InnerTube*>(other))
        {
            tubes.push_back(tube);
        }
    }
    Rocket& rocket = selection->getRocket();
    //: Not translated in OpenRocket (InnerTubeConfig.java): the undo step of "Split cluster"
    document().addUndoPosition(QtStrings::toUtf8(tr("Split cluster")));

    // Declared before the freeze: the removed tubes go only after the thaw has told everybody, and
    // a selected tube inside one of them is split there (as in Java) before it goes.
    std::vector<std::unique_ptr<RocketComponent>> removed;
    const bool                                    freezing = !rocket.isFrozen();
    if (freezing)
    {
        rocket.freeze();
    }
    try
    {
        for (InnerTube* tube : tubes)
        {
            if (std::unique_ptr<RocketComponent> split = splitClusterOf(*tube))
            {
                removed.push_back(std::move(split));
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
        // The TREE change closes the dialog (the panel goes later, by deleteLater()).
        rocket.thaw();
    }
}

}  // namespace QtRocket::Gui
