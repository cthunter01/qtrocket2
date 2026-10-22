#pragma once

// The test images of the freeform fin editor's "Import from image" and the report of what
// OpenRocket makes of them (tests/data/fin-images, written by tools/openrocket-fin-import; its
// README describes the blocks), for the tests of CustomFinImporter and FreeformFinSetConfig.

#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "QtRocket/util/Coordinate.h"

namespace QtRocket::Gui::Test
{

/// tests/data/fin-images/report.txt, as blocks: a line "== <header>" and the lines up to the
/// next one.
class FinImportReport
{
public:
    /// The report, read at the first call (a test failure when it cannot be read).
    [[nodiscard]] static const FinImportReport& get();

    /// The headers of the blocks, in order.
    [[nodiscard]] std::vector<std::string> headers() const;

    /// The rest of the line of the block "== @p header" that starts with "@p key ": nullopt when
    /// the block or the line is missing.
    [[nodiscard]] std::optional<std::string> line(std::string_view header,
                                                  std::string_view key) const;

private:
    explicit FinImportReport(std::string_view text);

    std::vector<std::pair<std::string, std::vector<std::string>>> m_blocks;
};

/// tests/data/fin-images/@p name.
[[nodiscard]] std::filesystem::path finImage(std::string_view name);

/// @p points as the report writes them: "N: (x,y) (x,y) ...", every coordinate as Java writes a
/// double.
[[nodiscard]] std::string outlineText(std::span<const Coordinate> points);

/// The points of @p text in the report's notation ("N: (x,y) (x,y) ..."); a test failure, and no
/// points, when it is malformed.
[[nodiscard]] std::vector<Coordinate> parseOutline(std::string_view text);

/// @p name ("one-pixel-tall.png") as a part of a test's name ("one_pixel_tall").
[[nodiscard]] std::string finImageTestName(std::string_view name);

}  // namespace QtRocket::Gui::Test
