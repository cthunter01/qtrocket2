#pragma once

#include <string_view>

#include "QtRocket/file/openrocket/Setter.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/util/Error.h"

namespace QtRocket
{

class DocumentLoadingContext;
class RocketComponent;

/// Sets where a component sits along its parent: the method the offset is measured by and the
/// offset, from `<axialoffset method="bottom">-0.01</axialoffset>` or, in files up to format
/// 1.8, `<position type="bottom">-0.01</position>` (OpenRocket's
/// file/openrocket/importt/AxialPositionSetter). Both elements have this setter, and each takes
/// either attribute.
///
/// set() does, in this order:
/// - The method is the AxialMethod the attribute "method" names, read as DocumentConfig.findEnum()
///   reads ("absolute", "after", "top", "middle", "bottom"; trimmed, nothing else); when that
///   attribute is missing or names none, the one the attribute "type" names. No method: the
///   warning Warning::kFileInvalidParameter, and nothing is set.
/// - The offset is the text as Double.parseDouble reads it (white space around the number is
///   dropped, "1e-1d" and "0x1p-3" are numbers). A text that is no number adds the warning
///   "Warning: invalid value radius position. value=<text>    class: <class>" and nothing is set.
///   The text is the element's, untrimmed; the word "radius" and the four blanks are
///   OpenRocket's; <class> is the component's Java class
///   (DocumentConfig::javaClassName(): "info.openrocket.core.rocketcomponent.Bulkhead").
/// - A component that has no axial position of its own (it is no AxialPositionable: a body tube,
///   a transition, a nose cone) gets the warning "Warning:
///   info.openrocket.core.file.openrocket.importt.AxialPositionSetter is not valid for class:
///   <class>" and nothing is set.
/// - Else the method is set and then the offset, in that order (setAxialMethod(), then
///   setAxialOffset()): what the component makes of the offset depends on the method it then
///   has. The component may have another method than the file's: a stage always follows its
///   predecessor (AFTER), a pod set and a booster take TOP for AFTER, the rocket ignores both.
///
/// The component is in its tree when the setter is applied (a loader attaches a component to
/// its parent before it sets anything): a stage, a booster or a pod set without a parent cannot
/// be positioned (BugError; OpenRocket: a BugException).
///
/// Deviation from OpenRocket (decisions L3 and L4 of the loader): an offset that is a NaN or an
/// infinity is refused with the warning of a text that is no number, and neither the method nor
/// the offset is set. OpenRocket sets the method and then dies of a BugException for a NaN
/// ("setAxialOffset is broken -- attempted to update as NaN", which RocketComponent::
/// setAxialOffset() has as a BugError), and it stores an infinity. A component that is no
/// AxialPositionable gets its own warning for such a text too, as in OpenRocket. The rocket and
/// a stage, which ignore the offset, get the warning here and none in OpenRocket.
class AxialPositionSetter final : public Setter
{
public:
    AxialPositionSetter() = default;

    [[nodiscard]] Result<void> set(RocketComponent& component, std::string_view value,
                                   const Attributes& attributes, WarningSet& warnings,
                                   const DocumentLoadingContext& context) const override;
};

}  // namespace QtRocket
