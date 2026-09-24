#pragma once

#include <string>
#include <string_view>

/// The parts of OpenRocket's L10N that the core needs although QtRocket has no translation layer
/// (it shows English only, and the GUI translates with tr()): OpenRocket looks translatable names
/// up by a normalized key, and some lookups, such as the material names, depend on that key.
namespace QtRocket::L10N
{

/// L10N.normalize: the form of @p text that OpenRocket's translation keys use ("Paper (office)"
/// gives "paper_office", "CRÊPE  paper" gives "crepe_paper"). Accented letters and a few symbols
/// are replaced by ASCII (L10N's normalization map: "É" by "E", "½" by "1/2", a no-break space by
/// a space), the text is lower-cased as in an English locale, runs of whitespace become one space
/// and the ends are trimmed; then ASCII letters and digits are kept, a space, '/' or the
/// fraction slash (U+2044) becomes '_', everything else is dropped, and '_' is stripped from
/// both ends.
[[nodiscard]] std::string normalize(std::string_view text);

}  // namespace QtRocket::L10N
