#pragma once

#include <string_view>

#include "QtRocket/file/openrocket/DoubleSetter.h"
#include "QtRocket/file/openrocket/Setter.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/util/Error.h"

namespace QtRocket
{

class DocumentLoadingContext;
class RocketComponent;

/// Sets where the tab of a fin set sits along the fin's root: the method the offset is measured
/// by and the offset, from `<tabposition relativeto="top">0.01</tabposition>` (OpenRocket's
/// file/openrocket/importt/FinTabPositionSetter).
///
/// set() does, in this order:
/// - No attribute "relativeto": the warning "Required attribute 'relativeto' not found for fin
///   tab position.", and nothing is set.
/// - The names of files up to format 1.4 are translated: a value that contains "front" becomes
///   "top", else one that contains "center" becomes "middle", else one that contains "end"
///   becomes "bottom" (String.contains(): "frontend" is "top", "the end" is "bottom", "FRONT" is
///   none of them).
/// - The method is the AxialMethod the value then names, read as DocumentConfig.findEnum() reads
///   (trimmed, nothing else), so "absolute" and "after" are methods too. A value that names
///   none adds the warning "Illegal attribute value '<value>' encountered." with the value
///   after the translation, and nothing is set.
/// - Else the method is set (FinSet::setTabOffsetMethod()), and then the text is handed to a
///   DoubleSetter for the offset (FinSet::setTabOffset()): a text that is no number, a NaN or an
///   infinity adds that setter's warning ("Invalid parameter encountered, ignoring. data:
///   '<text>' - <name of the component>") and leaves the offset, the method being set already.
///
/// OpenRocket's saver writes the element twice, with the old name of the method first
/// (relativeto="end", then "bottom"): the second one sets the same again.
///
/// Deviations from OpenRocket:
/// - Java's class extends DoubleSetter; here it has one (DoubleSetter is final).
/// - A component that is no fin set is a mistake of whoever chose the setter and throws BugError
///   (Java: an IllegalStateException). The walk of DocumentConfig::findSetter() cannot bring it
///   about: the key is "FinSet:tabposition".
class FinTabPositionSetter final : public Setter
{
public:
    FinTabPositionSetter();

    [[nodiscard]] Result<void> set(RocketComponent& component, std::string_view value,
                                   const Attributes& attributes, WarningSet& warnings,
                                   const DocumentLoadingContext& context) const override;

private:
    /// Sets the offset of the tab (Java: the DoubleSetter this class extends).
    DoubleSetter m_offset;
};

}  // namespace QtRocket
