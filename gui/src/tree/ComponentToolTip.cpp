#include "QtRocket/gui/tree/ComponentToolTip.h"

#include <algorithm>
#include <cstddef>
#include <span>
#include <string_view>
#include <vector>

#include <QChar>
#include <QString>

#include "QtRocket/aero/ComponentDrag.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket::Gui
{

namespace
{

/// @p component's name as tooltip HTML: escaped (decision U4; Java writes it as it is).
QString escapedName(const RocketComponent& component)
{
    return QtStrings::fromUtf8(Strings::escapeXml(component.getName()));
}

/// UnitGroup.UNITS_MASS.toStringUnit(@p mass).
QString massText(double mass)
{
    return QtStrings::defaultUnitText(UnitGroupId::MASS, mass);
}

/// UnitGroup.UNITS_LENGTH.toStringUnit(@p length).
QString lengthText(double length)
{
    return QtStrings::defaultUnitText(UnitGroupId::LENGTH, length);
}

/// What Java appends for the override CD of @p overrider: Double.toString(getOverrideCD()); the
/// text of the Error where the CD cannot be calculated (Java: a NullPointerException).
QString overrideCdText(const RocketComponent& overrider, double defaultMach)
{
    const Result<double> cd = ComponentDrag::getOverrideCD(overrider, defaultMach);
    return cd.has_value() ? QtStrings::javaDoubleText(*cd)
                          : QtStrings::fromUtf8(cd.error().message);
}

/// The comment of @p component as Java appends it: trimmed, escaped as XML and every newline a
/// <br>; empty when the comment trims to nothing.
QString commentHtml(const RocketComponent& component)
{
    const std::string_view comment = Strings::trim(component.getComment());
    if (comment.empty())
    {
        return {};
    }
    QString html = QtStrings::fromUtf8(Strings::escapeXml(comment));
    html.replace(QChar(u'\n'), QStringLiteral("<br>"));
    return html;
}

/// " <of> <mass> <total>": what Java appends after a mass for the total of a
/// section.
QString ofTotal(double sectionMass)
{
    //: (no key: ComponentTreeRenderer writes " of " itself)
    return QStringLiteral(" ") + ComponentToolTip::tr("of") + QStringLiteral(" ") +
           massText(sectionMass) + QStringLiteral(" ") +
           //: ComponentTreeRenderer.total
           ComponentToolTip::tr("total");
}

/// The mass of @p component in the title, when no ancestor overrides it: "
/// (<mass>[ of <section mass> total])" for a massive component or one whose
/// mass is overridden, " (<section mass> total)" for another that holds some
/// mass, else nothing.
QString massHtml(const RocketComponent& component)
{
    if (component.getMassOverriddenBy() != nullptr)
    {
        return {};
    }
    const bool hasChildren = component.getChildCount() > 0;
    if (component.isMassive() || component.isMassOverridden())
    {
        return QStringLiteral(" (") + massText(component.getMass()) +
               (hasChildren ? ofTotal(component.getSectionMass()) : QString()) +
               QStringLiteral(")");
    }
    if (hasChildren && component.getSectionMass() > 0)
    {
        return QStringLiteral(" (") + massText(component.getSectionMass()) + QStringLiteral(" ") +
               //: ComponentTreeRenderer.total
               ComponentToolTip::tr("total") + QStringLiteral(")");
    }
    return {};
}

/// The overrides of @p component itself that no ancestor's override hides, in
/// the title.
QString ownOverridesHtml(const RocketComponent& component)
{
    QString text;
    if (component.isMassOverridden() && component.getMassOverriddenBy() == nullptr)
    {
        //: ComponentTree.ttip.massoverride
        text += QStringLiteral(", ") + ComponentToolTip::tr("mass overriden");
    }
    if (component.isCGOverridden() && component.getCGOverriddenBy() == nullptr)
    {
        //: ComponentTree.ttip.cgoverride
        text += QStringLiteral(", ") + ComponentToolTip::tr("CG overriden");
    }
    if (component.isCDOverridden() && component.getCDOverriddenBy() == nullptr)
    {
        //: ComponentTree.ttip.cdoverride
        text += QStringLiteral(", ") + ComponentToolTip::tr("<html>C<sub>D</sub> overriden</html>");
    }
    return text;
}

/// The overrides of the ancestors of @p component, a line each.
QString overriddenByHtml(const RocketComponent& component, double defaultMach)
{
    QString text;
    if (const RocketComponent* overrider = component.getMassOverriddenBy())
    {
        //: RocketCompCfg.lbl.MassOverriddenBy
        text += QStringLiteral("<br>") +
                ComponentToolTip::tr("Mass overridden by %1").arg(escapedName(*overrider)) +
                QStringLiteral(" (") + massText(overrider->getMass()) + QStringLiteral(")");
    }
    if (const RocketComponent* overrider = component.getCGOverriddenBy())
    {
        //: RocketCompCfg.lbl.CGOverriddenBy
        text += QStringLiteral("<br>") +
                ComponentToolTip::tr("CG overridden by %1").arg(escapedName(*overrider)) +
                QStringLiteral(" (") + lengthText(overrider->getOverrideCGX()) +
                QStringLiteral(")");
    }
    if (const RocketComponent* overrider = component.getCDOverriddenBy())
    {
        //: RocketCompCfg.lbl.CDOverriddenBy
        text += QStringLiteral("<br>") +
                ComponentToolTip::tr("<html>C<sub>D</sub> overridden by %1")
                    .arg(escapedName(*overrider)) +
                QStringLiteral(" (") + overrideCdText(*overrider, defaultMach) +
                QStringLiteral(")");
    }
    return text;
}

/// The total mass of selected components (getToolTipMultipleComponents()).
struct SelectionMass
{
    double total   = 0;
    double section = 0;
    /// Whether a selected component's section mass is in section.
    bool containsSection = false;
};

/// What getToolTipMultipleComponents() adds up of @p components: the mass of
/// each (that of its overrider instead, once, when an ancestor that is not
/// selected overrides it), and of each without a selected ancestor its section
/// mass; but the section mass of the first component that is neither massive
/// nor overridden and holds some mass alone, when it comes first.
SelectionMass selectionMass(std::span<const RocketComponent* const> components)
{
    SelectionMass                       mass;
    std::vector<const RocketComponent*> overriders;
    for (const RocketComponent* component : components)
    {
        const RocketComponent* overrider = component->getMassOverriddenBy();
        if (overrider != nullptr)
        {
            if (std::ranges::find(components, overrider) == components.end() &&
                std::ranges::find(overriders, overrider) == overriders.end())
            {
                mass.total += overrider->getMass();
                overriders.push_back(overrider);
            }
            continue;
        }
        const bool holdsMass = component->getChildCount() > 0 && component->getSectionMass() > 0;
        if (component->isMassive() || component->isMassOverridden())
        {
            mass.total += component->getMass();
            // Not in the section mass when an ancestor is selected: its section has
            // it already.
            if (!RocketComponent::listContainsParent(components, *component))
            {
                mass.section += holdsMass ? component->getSectionMass() : component->getMass();
                mass.containsSection = mass.containsSection || holdsMass;
            }
        }
        else if (holdsMass)
        {
            mass.total           = component->getSectionMass();
            mass.containsSection = false;
            break;
        }
    }
    return mass;
}

}  // namespace

QString ComponentToolTip::singleComponent(const RocketComponent& component, double defaultMach)
{
    QString text = QStringLiteral("<html><b>") + escapedName(component) + QStringLiteral("</b>") +
                   massHtml(component) + ownOverridesHtml(component) +
                   overriddenByHtml(component, defaultMach);
    const QString comment = commentHtml(component);
    if (!comment.isEmpty())
    {
        text += QStringLiteral("<br>") + comment;
    }
    return text;
}

QString ComponentToolTip::multipleComponents(std::span<const RocketComponent* const> components)
{
    if (components.empty())
    {
        return {};
    }
    const SelectionMass mass = selectionMass(components);
    //: (no key: ComponentTreeRenderer writes "Components" itself)
    QString text = QStringLiteral("<html><b>") + tr("Components") + QStringLiteral("</b> (") +
                   massText(mass.total) +
                   (mass.containsSection ? ofTotal(mass.section) : QString()) +
                   QStringLiteral(")<br>");
    // The names of the components.
    for (std::size_t i = 0; i < components.size(); ++i)
    {
        text += (i > 0 ? QStringLiteral(", ") : QString()) + escapedName(*components[i]);
    }
    return text;
}
QString ComponentToolTip::forComponent(const RocketComponent&                  hovered,
                                       std::span<const RocketComponent* const> selection,
                                       double                                  defaultMach)
{
    if (selection.size() > 1 && std::ranges::find(selection, &hovered) != selection.end())
    {
        return multipleComponents(selection);
    }
    return singleComponent(hovered, defaultMach);
}

}  // namespace QtRocket::Gui
