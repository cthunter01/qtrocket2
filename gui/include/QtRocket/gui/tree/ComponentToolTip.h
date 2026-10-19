#pragma once

#include <span>

#include <QCoreApplication>
#include <QString>

namespace QtRocket
{
class RocketComponent;
}

namespace QtRocket::Gui
{

/// The tooltips of the component tree (OpenRocket's ComponentTreeRenderer: applyToolTipText(),
/// getToolTipSingleComponent() and getToolTipMultipleComponents()): HTML that names the
/// component, its mass (and the total of what it holds), its own overrides, the overrides it
/// is subject to and its comment; for a hovered component among several selected ones, the
/// selection's names and total mass. Masses and lengths are in the default units
/// (unitGroup(MASS / LENGTH).toStringUnit()), the CD as Java's Double.toString().
///
/// The texts are messages.properties' ("mass overriden" [sic], "CG overriden",
/// "<html>C<sub>D</sub> overriden</html>" with its nested <html>, "Mass overridden by %s", ...),
/// and the hard-coded English of the renderer ("Components", " of "), all through tr().
///
/// Deviation from OpenRocket (decision U4, an OpenRocket bug fixed): every component name (the
/// component's, an overrider's, the selected ones') is escaped as XML (Strings::escapeXml(),
/// TextUtil.escapeXML), as the comment is; OpenRocket writes names as they are, so that a name
/// like "My <nose>" shows as "My ".
class ComponentToolTip
{
    Q_DECLARE_TR_FUNCTIONS(ComponentToolTip)

public:
    ComponentToolTip() = delete;

    /// getToolTipSingleComponent(@p component). The CD of an overrider is its
    /// ComponentDrag::getOverrideCD() at @p defaultMach (Preferences::getDefaultMach()), as
    /// Java's getOverrideCD() gives it; when that cannot be calculated, the Error's text stands
    /// in for it.
    [[nodiscard]] static QString singleComponent(const RocketComponent& component,
                                                 double                 defaultMach);

    /// getToolTipMultipleComponents(@p components): "<html><b>Components</b> (<mass>[ of <total>
    /// total])<br><name>, <name>, ...", in the order of @p components; a null string for an
    /// empty list (Java: null). @p components must not hold null.
    [[nodiscard]] static QString multipleComponents(
        std::span<const RocketComponent* const> components);

    /// applyToolTipText(): the multi-component tooltip of @p selection when it holds more than
    /// one component and @p hovered is one of them, else the tooltip of @p hovered alone.
    [[nodiscard]] static QString forComponent(const RocketComponent&                  hovered,
                                              std::span<const RocketComponent* const> selection,
                                              double                                  defaultMach);
};

}  // namespace QtRocket::Gui
