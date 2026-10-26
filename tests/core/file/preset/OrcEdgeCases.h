#pragma once

// The edge-case documents of the probe tools/openrocket-probes/orc-loader (OrcEdgeProbe), which
// fed them to OpenRocket's OpenRocketComponentLoader: what it answered for each is in the probe's
// expected.txt, and the tests pin those answers by hand. The documents are built here as the probe
// builds them, so that they are the same bytes (orc_edge_case_tests.cpp checks each one's size and
// SHA-256 against the probe's).

#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace QtRocket::Test
{

/// One case of the probe.
struct OrcEdgeCase
{
    /// What the probe's case name starts with: "01" to "82" (batch 1, the scouting's
    /// OrcEdgeProbe), "N00", "I00", "B00", "S00", "T01", ... (batch 2, its OrcEdgeProbe2),
    /// "XN00", "XI00", "XB00", "XS00", "XT01", "XH00", ... (batch 3, part O1's).
    std::string id;
    /// The probe's case name, as its transcript prints it after "== ".
    std::string name;
    /// The document as the probe hands it to the loader: UTF-8, but for case 81 (ISO-8859-1)
    /// and case 82 (UTF-16 with a byte order mark).
    std::string bytes;
};

/// The 337 cases in the probe's order: 82 of batch 1, 96 of batch 2 (the scouting's 178) and 159
/// of batch 3.
[[nodiscard]] const std::vector<OrcEdgeCase>& orcEdgeCases();

/// The case @p id; a test failure, and an empty case, for an unknown id.
[[nodiscard]] const OrcEdgeCase& orcEdgeCase(std::string_view id);

/// The bytes of the case @p id.
[[nodiscard]] std::span<const std::byte> orcEdgeCaseBytes(std::string_view id);

}  // namespace QtRocket::Test
