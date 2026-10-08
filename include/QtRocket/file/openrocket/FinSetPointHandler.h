#pragma once

#include <string_view>
#include <vector>

#include "QtRocket/file/simplesax/AbstractElementHandler.h"
#include "QtRocket/file/simplesax/ElementHandler.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Error.h"

namespace QtRocket
{

class DocumentLoadingContext;
class FreeformFinSet;

/// Reads the <finpoints> element of a freeform fin set: the points of its outline, each
/// `<point x="..." y="..."/>` (OpenRocket's file/openrocket/importt/FinSetPointHandler).
///
/// Every child is a point, whatever its name: its attributes x and y, each Double.parseDouble,
/// are the coordinates. A child without one of the two, or with one that is no number, gives
/// "Illegal fin points specification, ignoring." and is passed over. A point that was read
/// also gets AbstractElementHandler's warnings for its text and for attributes other than x
/// and y, and counts all the same.
///
/// When the element closes, the points become the fin set's outline
/// (FreeformFinSet::setPoints(), which moves the first point to the origin, clamps the points
/// onto the parent and takes back an outline that crosses itself, all without a word), and
/// then the position of the fin tab is computed again (FinSet::updateTabPosition()): the
/// <tabposition> of a file stands before the <finpoints>, so it was applied to the length the
/// fin set had before. Neither the text nor the attributes of <finpoints> itself are warned of.
///
/// Deviations from OpenRocket:
/// - A <finpoints> without a single point that could be read gives "Illegal fin points
///   specification, ignoring." (once more, when its children already gave it: a WarningSet
///   keeps one of equal warnings) and leaves the fin set its outline. OpenRocket dies there of
///   an IndexOutOfBoundsException, and FreeformFinSet::setPoints() throws BugError.
/// - A coordinate that is a NaN or an infinity makes the point one that cannot be read
///   (OpenRocket stores it).
/// - Java removes x and y from the attribute map before it warns of the others; the attributes
///   being const here, the warning is decided on a copy without the two.
/// - The constructor takes the loading context, as every handler of the loader does, and does
///   not need it; neither does Java's.
class FinSetPointHandler final : public AbstractElementHandler
{
public:
    /// The handler of the <finpoints> element of @p finset, which must outlive it.
    FinSetPointHandler(FreeformFinSet& finset, const DocumentLoadingContext& context) noexcept;

    [[nodiscard]] Result<ElementHandler*> openElement(std::string_view  element,
                                                      const Attributes& attributes,
                                                      WarningSet&       warnings) override;

    [[nodiscard]] Result<void> closeElement(std::string_view element, const Attributes& attributes,
                                            std::string_view content,
                                            WarningSet&      warnings) override;

    [[nodiscard]] Result<void> endHandler(std::string_view element, const Attributes& attributes,
                                          std::string_view content, WarningSet& warnings) override;

private:
    FreeformFinSet*         m_finset;
    std::vector<Coordinate> m_coordinates;
};

}  // namespace QtRocket
