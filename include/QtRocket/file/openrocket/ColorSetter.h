#pragma once

#include <functional>
#include <string_view>

#include "QtRocket/file/openrocket/Setter.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/util/Color.h"
#include "QtRocket/util/Error.h"

namespace QtRocket
{

class DocumentLoadingContext;
class RocketComponent;

/// Sets a colour from the attributes of its element, `<color red="1" green="2" blue="3"/>`
/// (OpenRocket's file/openrocket/importt/ColorSetter).
///
/// The colour is what ORColor.fromXMLAttributes() makes of the attributes
/// (Color::fromXmlAttributes()): red, green and blue must be there, alpha may be (255 without
/// it), and each must be a whole number from 0 to 255 as Integer.parseInt reads one, so "+1" is
/// a channel and " 1", "1.0", "300" and "" are not. Attributes that make no colour add
/// Warning::kFileInvalidParameter and set nothing. Otherwise the colour is set, and a text in
/// the element that is not blank (String.trim()) then adds the same warning: the colour stays
/// set.
///
/// Deviation from OpenRocket: Integer.parseInt also takes the decimal digits of other scripts
/// (red="&#1636;", the Arabic-Indic digit four, is 4 there); here only ASCII digits are digits
/// (Strings::parseInt()), so such attributes make no colour: the warning, and nothing is set.
class ColorSetter final : public Setter
{
public:
    /// What sets the colour on the component (Java: the setter method).
    using SetFunction = std::function<void(RocketComponent&, const Color&)>;

    /// @throws BugError when @p set is empty.
    explicit ColorSetter(SetFunction set);

    [[nodiscard]] Result<void> set(RocketComponent& component, std::string_view value,
                                   const Attributes& attributes, WarningSet& warnings,
                                   const DocumentLoadingContext& context) const override;

private:
    SetFunction m_set;
};

}  // namespace QtRocket
