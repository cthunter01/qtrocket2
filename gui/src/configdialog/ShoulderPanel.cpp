#include "QtRocket/gui/configdialog/ShoulderPanel.h"

#include <memory>

#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QString>
#include <QWidget>
#include <Qt>

#include "QtRocket/gui/binding/BoolBinding.h"
#include "QtRocket/gui/binding/ComponentSelection.h"
#include "QtRocket/gui/binding/DoubleBinding.h"
#include "QtRocket/gui/binding/PropertySource.h"
#include "QtRocket/gui/binding/SliderMapping.h"
#include "QtRocket/gui/configdialog/ConfigFields.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/widgets/BindingScope.h"
#include "QtRocket/gui/widgets/BoolCheckBox.h"
#include "QtRocket/rocket/NoseCone.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/Transition.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/BugError.h"

namespace QtRocket::Gui
{

namespace
{

/// The source of a number property of the class C from its getter and its one-argument setter
/// (the overload Java's reflection finds).
template <class C>
[[nodiscard]] DoubleSource numberSource(double (C::*get)() const, void (C::*set)(double))
{
    return DoubleSource::of<C>({
        .get = [get](const C& component) { return (component.*get)(); },
        .set = [set](C& component, const double& value) { (component.*set)(value); },
    });
}

/// A read-only number property of the class C (a slider's bound).
template <class C>
[[nodiscard]] DoubleSource boundSource(double (C::*get)() const)
{
    return DoubleSource::of<C>({.get = [get](const C& component) { return (component.*get)(); }});
}

/// A check box's property of the class C.
template <class C>
[[nodiscard]] BoolSource flagSource(bool (C::*is)() const, void (C::*set)(bool))
{
    return BoolSource::of<C>({
        .get = [is](const C& component) { return (component.*is)(); },
        .set = [set](C& component, const bool& value) { (component.*set)(value); },
    });
}

/// Which shoulder: a transition's fore or aft shoulder, or a nose cone's.
enum class Shoulder
{
    FORE,
    AFT,
    NOSE_CONE,
};

/// The start of the property names of @p shoulder ("ForeShoulder", "AftShoulder", "Shoulder").
[[nodiscard]] QString prefixOf(Shoulder shoulder)
{
    switch (shoulder)
    {
        case Shoulder::FORE:
            return QStringLiteral("ForeShoulder");
        case Shoulder::AFT:
            return QStringLiteral("AftShoulder");
        case Shoulder::NOSE_CONE:
            return QStringLiteral("Shoulder");
    }
    QTROCKET_UNREACHABLE();
}

/// The shoulder's radius (ForeShoulderRadius, AftShoulderRadius, NoseCone's ShoulderRadius).
[[nodiscard]] DoubleSource radiusSource(Shoulder shoulder)
{
    switch (shoulder)
    {
        case Shoulder::FORE:
            return numberSource<Transition>(&Transition::getForeShoulderRadius,
                                            &Transition::setForeShoulderRadius);
        case Shoulder::AFT:
            return numberSource<Transition>(&Transition::getAftShoulderRadius,
                                            &Transition::setAftShoulderRadius);
        case Shoulder::NOSE_CONE:
            return numberSource<NoseCone>(&NoseCone::getShoulderRadius,
                                          &NoseCone::setShoulderRadius);
    }
    QTROCKET_UNREACHABLE();
}

/// The radius of the shoulder's end (ForeRadius, AftRadius, NoseCone's BaseRadius): read-only.
[[nodiscard]] DoubleSource endRadiusSource(Shoulder shoulder)
{
    switch (shoulder)
    {
        case Shoulder::FORE:
            return boundSource<Transition>(&Transition::getForeRadius);
        case Shoulder::AFT:
            return boundSource<Transition>(&Transition::getAftRadius);
        case Shoulder::NOSE_CONE:
            return boundSource<NoseCone>(&NoseCone::getBaseRadius);
    }
    QTROCKET_UNREACHABLE();
}

/// The shoulder's length.
[[nodiscard]] DoubleSource lengthSource(Shoulder shoulder)
{
    switch (shoulder)
    {
        case Shoulder::FORE:
            return numberSource<Transition>(&Transition::getForeShoulderLength,
                                            &Transition::setForeShoulderLength);
        case Shoulder::AFT:
            return numberSource<Transition>(&Transition::getAftShoulderLength,
                                            &Transition::setAftShoulderLength);
        case Shoulder::NOSE_CONE:
            return numberSource<NoseCone>(&NoseCone::getShoulderLength,
                                          &NoseCone::setShoulderLength);
    }
    QTROCKET_UNREACHABLE();
}

/// The shoulder's thickness.
[[nodiscard]] DoubleSource thicknessSource(Shoulder shoulder)
{
    switch (shoulder)
    {
        case Shoulder::FORE:
            return numberSource<Transition>(&Transition::getForeShoulderThickness,
                                            &Transition::setForeShoulderThickness);
        case Shoulder::AFT:
            return numberSource<Transition>(&Transition::getAftShoulderThickness,
                                            &Transition::setAftShoulderThickness);
        case Shoulder::NOSE_CONE:
            return numberSource<NoseCone>(&NoseCone::getShoulderThickness,
                                          &NoseCone::setShoulderThickness);
    }
    QTROCKET_UNREACHABLE();
}

/// Whether the shoulder is capped.
[[nodiscard]] BoolSource cappedSource(Shoulder shoulder)
{
    switch (shoulder)
    {
        case Shoulder::FORE:
            return flagSource<Transition>(&Transition::isForeShoulderCapped,
                                          &Transition::setForeShoulderCapped);
        case Shoulder::AFT:
            return flagSource<Transition>(&Transition::isAftShoulderCapped,
                                          &Transition::setAftShoulderCapped);
        case Shoulder::NOSE_CONE:
            return flagSource<NoseCone>(&NoseCone::isShoulderCapped, &NoseCone::setShoulderCapped);
    }
    QTROCKET_UNREACHABLE();
}

/// The rows of @p shoulder in @p box (RocketComponentConfig.addForeShoulderSection() and
/// addAftShoulderSection()).
void addShoulderRows(BindingScope& scope, QWidget& box, Shoulder shoulder)
{
    const std::shared_ptr<ComponentSelection>& selection = scope.selection();
    const UnitGroup&                           length    = unitGroup(UnitGroupId::LENGTH);
    auto*                                      grid      = makeChild<QGridLayout>(&box);

    // The diameter (the radius times 2), its slider from 0 to the end's diameter.
    const auto radius =
        scope.make<DoubleBinding>(selection, radiusSource(shoulder), length,
                                  DoubleBinding::Options{.multiplier = 2.0, .min = 0.0});
    const auto endRadius = scope.make<DoubleBinding>(selection, endRadiusSource(shoulder), length,
                                                     DoubleBinding::Options{.multiplier = 2.0});
    ConfigFields::addNumberRow(*grid, 0,
                               //: RocketCompCfg.lbl.Diameter
                               ShoulderPanel::tr("Diameter:"), radius,
                               prefixOf(shoulder) + QStringLiteral("Radius"),
                               {.slider = SliderMapping::linear(0.0, endRadius->value()),
                                .bounds = {.min = nullptr, .max = endRadius}});

    const auto shoulderLength = scope.make<DoubleBinding>(selection, lengthSource(shoulder), length,
                                                          DoubleBinding::Options{.min = 0.0});
    ConfigFields::addNumberRow(*grid, 1,
                               //: RocketCompCfg.lbl.Length
                               ShoulderPanel::tr("Length:"), shoulderLength,
                               prefixOf(shoulder) + QStringLiteral("Length"),
                               {.slider = SliderMapping::fromRange(0.0, 0.02, 0.2)});

    // The thickness, its slider from 0 to the shoulder's radius.
    const auto thickness = scope.make<DoubleBinding>(selection, thicknessSource(shoulder), length,
                                                     DoubleBinding::Options{.min = 0.0});
    const auto shoulderRadius = scope.make<DoubleBinding>(selection, radiusSource(shoulder), length,
                                                          DoubleBinding::Options{});
    ConfigFields::addNumberRow(*grid, 2,
                               //: RocketCompCfg.lbl.Thickness
                               ShoulderPanel::tr("Thickness:"), thickness,
                               prefixOf(shoulder) + QStringLiteral("Thickness"),
                               {.slider = SliderMapping::linear(0.0, shoulderRadius->value()),
                                .bounds = {.min = nullptr, .max = shoulderRadius}});

    auto* capped =
        makeChild<BoolCheckBox>(&box, scope.make<BoolBinding>(selection, cappedSource(shoulder)),
                                //: RocketCompCfg.checkbox.Endcapped
                                ShoulderPanel::tr("End capped"));
    capped->setObjectName(prefixOf(shoulder) + QStringLiteral("Capped"));
    //: RocketCompCfg.checkbox.Endcapped.ttip
    capped->setToolTip(ShoulderPanel::tr("Caps (closes) the end of the shoulder."));
    grid->addWidget(capped, 3, 0, 1, 4);
    grid->setColumnStretch(3, 1);
}

}  // namespace

ShoulderPanel::ShoulderPanel(BindingScope& scope, QWidget* parent) : QWidget(parent)
{
    const std::shared_ptr<ComponentSelection>& selection = scope.selection();
    if (selection == nullptr || !selection->isValid())
    {
        bug("The shoulder tab needs a valid selection");
    }
    const RocketComponent& main = selection->getMain();
    if (dynamic_cast<const Transition*>(&main) == nullptr)
    {
        bug("The shoulder tab needs a transition or a nose cone");
    }
    auto* layout = makeChild<QHBoxLayout>(this);

    // The fore shoulder: not for a nose cone.
    const bool noseCone = dynamic_cast<const NoseCone*>(&main) != nullptr;
    if (!noseCone)
    {
        //: RocketCompCfg.border.Foreshoulder
        m_fore = makeChild<QGroupBox>(this, tr("Fore shoulder"));
        addShoulderRows(scope, *m_fore, Shoulder::FORE);
        layout->addWidget(m_fore, 0, Qt::AlignTop);
    }

    // The aft shoulder: a nose cone's own shoulder, without a border.
    if (noseCone)
    {
        m_aft = makeChild<QWidget>(this);
        addShoulderRows(scope, *m_aft, Shoulder::NOSE_CONE);
    }
    else
    {
        //: RocketCompCfg.border.Aftshoulder
        auto* aft = makeChild<QGroupBox>(this, tr("Aft shoulder"));
        addShoulderRows(scope, *aft, Shoulder::AFT);
        m_aft = aft;
    }
    layout->addWidget(m_aft, 0, Qt::AlignTop);
    layout->addStretch(1);
}

}  // namespace QtRocket::Gui
