#pragma once

#include <array>
#include <map>
#include <optional>
#include <string>
#include <string_view>

#include "QtRocket/file/simplesax/AbstractElementHandler.h"
#include "QtRocket/file/simplesax/ElementHandler.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/util/Error.h"

namespace QtRocket
{

/// Reads the <photostudio> element of a design file: the settings of the photo studio, kept as
/// the keys and texts the file holds, in the map of the document
/// (OpenRocketDocument::getPhotoSettings()), so that a file is saved with the settings it was
/// loaded with (OpenRocket's file/openrocket/importt/PhotoStudioHandler). The core does not
/// interpret them; they are the GUI's.
///
/// Every child is plain text. When one closes:
/// - one of the 24 names of kTextSettings ("roll", "sky", "backgroundType", ...): the text of
///   the element, as it is, neither trimmed nor read as a number, is stored under the name; its
///   attributes are not looked at;
/// - one of the 6 names of kColorSettings ("sunlight", "skyColor", ...): the attributes red,
///   green and blue, each Integer.parseInt, and alpha, which is 255 when the element has none,
///   are stored under the name as the text "R G B A" ("255 0 128 255"). The numbers are not
///   limited to 0 to 255. When red, green or blue is missing nothing is stored, without a
///   warning. The text of the element is not looked at;
/// - any other child gives AbstractElementHandler's warnings for its text and attributes.
/// A later element of a name replaces what an earlier one stored.
///
/// A colour value that is no int ends the load, with ErrorCode::INVALID_ARGUMENT and the
/// message of Java's NumberFormatException (`For input string: "x"`): red, green and blue are
/// read in this order before it is asked whether one is missing, and alpha after that, so a
/// bad red fails the load also when the element has no blue.
///
/// Deviation from OpenRocket: Integer.parseInt also takes the decimal digits of other scripts;
/// DocumentConfig::parseInt() reads ASCII digits only, so such a colour value fails the load
/// here. (Java logs a missing colour component; nothing is logged here.)
class PhotoStudioHandler final : public AbstractElementHandler
{
public:
    /// The settings stored as the text of their element (Java: params).
    static constexpr std::array<std::string_view, 24> kTextSettings{"roll",
                                                                    "yaw",
                                                                    "pitch",
                                                                    "advance",
                                                                    "viewAlt",
                                                                    "viewAz",
                                                                    "viewDistance",
                                                                    "fov",
                                                                    "lightAlt",
                                                                    "lightAz",
                                                                    "lightStrength",
                                                                    "ambiance",
                                                                    "motionBlurred",
                                                                    "motionBlurAmount",
                                                                    "flame",
                                                                    "smoke",
                                                                    "smokeOpacity",
                                                                    "sparks",
                                                                    "exhaustScale",
                                                                    "flameAspectRatio",
                                                                    "sparkConcentration",
                                                                    "sparkWeight",
                                                                    "sky",
                                                                    "backgroundType"};

    /// The settings stored as the four numbers of a colour (Java: colors).
    static constexpr std::array<std::string_view, 6> kColorSettings{
        "sunlight",   "skyColor",         "flameColor",
        "smokeColor", "gradientTopColor", "gradientBottomColor"};

    /// The handler of a <photostudio> element that stores into @p settings, which must outlive
    /// it: the map of the document being loaded.
    explicit PhotoStudioHandler(std::map<std::string, std::string>& settings) noexcept;

    [[nodiscard]] Result<ElementHandler*> openElement(std::string_view  element,
                                                      const Attributes& attributes,
                                                      WarningSet&       warnings) override;

    [[nodiscard]] Result<void> closeElement(std::string_view element, const Attributes& attributes,
                                            std::string_view content,
                                            WarningSet&      warnings) override;

private:
    /// The text "R G B A" of the colour the attributes give (getColor()); nullopt when red,
    /// green or blue is missing, and a failure for a value that is no int.
    [[nodiscard]] static Result<std::optional<std::string>> getColor(const Attributes& attributes);

    std::map<std::string, std::string>* m_settings;
};

}  // namespace QtRocket
