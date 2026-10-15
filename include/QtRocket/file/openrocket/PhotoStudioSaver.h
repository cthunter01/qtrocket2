#pragma once

#include <array>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "QtRocket/file/openrocket/OrkWriter.h"

namespace QtRocket
{

/// One setting of the photo studio that PhotoStudioSaver writes.
struct PhotoStudioSetting
{
    std::string_view key;
    /// A colour ("R G B A", written as attributes) rather than a text.
    bool color{false};
};

/// Writes the settings of the photo studio (OpenRocketDocument::getPhotoSettings(), the keys and
/// texts a file's <photostudio> element held) as the lines of that element (OpenRocket's
/// file/openrocket/savers/PhotoStudioSaver).
///
/// getElements() goes through kSettings in their fixed order and writes a line for each key
/// the map holds; a key the map lacks writes nothing (OpenRocket writes no "null" for it either:
/// PhotoStudioSaverTest), and a key that is not in kSettings is not written at all. A text
/// setting is `<key>text</key>`. A colour setting holds "R G B A": the text is split at every
/// space (String.split(" "), trailing empty fields dropped); with fewer than four fields the line
/// is `<key></key>`, otherwise the first four fields are read as Integer.parseInt reads them and
/// written as `<key red="R" green="G" blue="B" alpha="A"/>` (ORColor.toXMLAttributes(), which
/// takes any int: "007 -1 300 4" gives red="7" green="-1" blue="300" alpha="4").
///
/// kSettings holds 29 keys, 23 texts and 6 colours: smokeOpacity, which the loader keeps
/// (PhotoStudioHandler::kTextSettings), is not among them, so a save loses it, as OpenRocket's
/// does.
///
/// Deviations from OpenRocket:
/// - The texts go through OrkWriter::escape() (decision E3 of tier 10); Java writes them as they
///   are, so a '&' or '<' in one makes a file neither program can read.
/// - A colour field that Integer.parseInt refuses (a word, an empty field between two spaces, a
///   number beyond the int range) gives `<key></key>`, as too few fields do. Java's saver dies of
///   the NumberFormatException there; the loader never stores such a text, only a caller of
///   OpenRocketDocument::setPhotoSettings() can. Strings::parseInt() reads ASCII digits only
///   (Java also takes the decimal digits of other scripts).
/// - getElements() takes the writer for its escape() (Java: a static TextUtil call).
class PhotoStudioSaver final
{
public:
    PhotoStudioSaver() = delete;

    /// The settings, in the order Java writes them.
    static constexpr std::array<PhotoStudioSetting, 29> kSettings{{
        {.key = "roll"},
        {.key = "yaw"},
        {.key = "pitch"},
        {.key = "advance"},
        {.key = "viewAlt"},
        {.key = "viewAz"},
        {.key = "viewDistance"},
        {.key = "fov"},
        {.key = "lightAlt"},
        {.key = "lightAz"},
        {.key = "lightStrength"},
        {.key = "sunlight", .color = true},
        {.key = "ambiance"},
        {.key = "skyColor", .color = true},
        {.key = "backgroundType"},
        {.key = "gradientTopColor", .color = true},
        {.key = "gradientBottomColor", .color = true},
        {.key = "motionBlurred"},
        {.key = "motionBlurAmount"},
        {.key = "flame"},
        {.key = "flameColor", .color = true},
        {.key = "smoke"},
        {.key = "smokeColor", .color = true},
        {.key = "sparks"},
        {.key = "exhaustScale"},
        {.key = "flameAspectRatio"},
        {.key = "sparkConcentration"},
        {.key = "sparkWeight"},
        {.key = "sky"},
    }};

    /// The lines of the <photostudio> element for @p photoSettings (Java: getElements()), without
    /// the element's own tags and indentation; none for an empty map. Texts are escaped through
    /// @p writer.
    [[nodiscard]] static std::vector<std::string> getElements(
        const std::map<std::string, std::string>& photoSettings, OrkWriter& writer);
};

}  // namespace QtRocket
