#pragma once

#include <string_view>

#include "QtRocket/file/openrocket/Setter.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/util/Error.h"

namespace QtRocket
{

class DocumentLoadingContext;
class RocketComponent;

/// Sets the angle about its parent's axis at which a component sits: the method the angle is
/// measured by and the angle, from `<angleoffset method="relative">90.0</angleoffset>`
/// (OpenRocket's file/openrocket/importt/AnglePositionSetter). The setter table has it for a pod
/// set, a booster, a fin set, a tube fin set, a launch lug and a rail button.
///
/// set() does, in this order:
/// - The method is the AngleMethod the attribute "method" names, read as
///   DocumentConfig.findEnum() reads ("relative", "fixed", "mirrorxy"; trimmed, nothing else).
///   A missing attribute and one that names no method are both RELATIVE, without a warning. So
///   is "mirror_xy", which is what OpenRocket's saver writes for MIRROR_XY: a design saved with
///   that method comes back as RELATIVE, in OpenRocket and here.
/// - The angle is the text as Double.parseDouble reads it, in degrees, made radians as
///   Math.toRadians does (MathUtil::javaToRadians()). A text that is no number adds the warning
///   "Warning: invalid angle position. value=<text>  (degrees)  class: <class>" (the text
///   untrimmed, two blanks on each side of "(degrees)", <class> as
///   DocumentConfig::javaClassName() gives it) and nothing is set.
/// - A component that is no AnglePositionable gets the warning "Warning:
///   info.openrocket.core.file.openrocket.importt.AnglePositionSetter is not valid for class:
///   <class>" and nothing is set. With the keys the setter table has, no file brings this about.
/// - Else the method is set and then the angle (setAngleMethod(), then setAngleOffset()). What
///   the component makes of them is its own business: a launch lug, a rail button and a pod set
///   keep RELATIVE whatever the method, and each class reduces the angle in its own way.
///
/// Deviation from OpenRocket (decision L3 of the loader): an angle that is a NaN or an infinity
/// is refused with the warning of a text that is no number, and neither the method nor the angle
/// is set. OpenRocket stores it (a fin set at an angle of NaN, which it writes back as
/// `<angleoffset method="relative">NaN</angleoffset>` and `<rotation>NaN</rotation>`).
class AnglePositionSetter final : public Setter
{
public:
    AnglePositionSetter() = default;

    [[nodiscard]] Result<void> set(RocketComponent& component, std::string_view value,
                                   const Attributes& attributes, WarningSet& warnings,
                                   const DocumentLoadingContext& context) const override;
};

}  // namespace QtRocket
