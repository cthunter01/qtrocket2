#pragma once

#include <string_view>

#include "QtRocket/file/openrocket/Setter.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/util/Error.h"

namespace QtRocket
{

class DocumentLoadingContext;
class RocketComponent;

/// Sets how far from its parent's axis a component sits: the method the offset is measured by
/// and the offset, from `<radiusoffset method="surface">0.0</radiusoffset>` (OpenRocket's
/// file/openrocket/importt/RadiusPositionSetter). The setter table has it for a pod set, a
/// booster, a fin set and a tube fin set.
///
/// set() does, in this order:
/// - The method is the RadiusMethod the attribute "method" names, read as
///   DocumentConfig.findEnum() reads ("coaxial", "free", "relative", "surface"; trimmed, nothing
///   else). A missing attribute and one that names no method are both SURFACE, without a
///   warning.
/// - The offset is the text as Double.parseDouble reads it. A text that is no number adds the
///   warning "Warning: invalid value radius position. value=<text>    class: <class>" (the
///   text untrimmed, four blanks, <class> as DocumentConfig::javaClassName() gives it) and
///   nothing is set.
/// - A component that is no RadiusPositionable gets the warning "Warning: radiusPositionable is
///   not valid for this class: <class>" and nothing is set. With the keys the setter table has,
///   no file brings this about.
/// - Else the method is set and then the offset (setRadiusMethod(), then setRadiusOffset()).
///   What the component makes of them is its own business: a fin set and a tube fin set ignore
///   both (they stand on their parent's surface), and SURFACE and COAXIAL make the offset zero.
///
/// Deviation from OpenRocket (decision L3 of the loader): an offset that is a NaN or an
/// infinity is refused with the warning of a text that is no number, and neither the method nor
/// the offset is set. OpenRocket stores it (`<radiusoffset method="free">NaN</radiusoffset>`
/// gives a pod set at a radius of NaN, which it writes back).
class RadiusPositionSetter final : public Setter
{
public:
    RadiusPositionSetter() = default;

    [[nodiscard]] Result<void> set(RocketComponent& component, std::string_view value,
                                   const Attributes& attributes, WarningSet& warnings,
                                   const DocumentLoadingContext& context) const override;
};

}  // namespace QtRocket
