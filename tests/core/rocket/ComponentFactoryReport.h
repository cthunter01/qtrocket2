#pragma once

// The report of OpenRocket's add buttons, tests/data/component-factory/report.txt, which the dumper
// of tools/openrocket-component-factory (ComponentFactoryDumper.java) prints with OpenRocket's own
// code: the expectations of the tests of ComponentFactory and of the design its components build
// (rocket/ComponentFactoryTests.cpp, file/openrocket/add_buttons_save_tests.cpp). Test-only.

#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"
#include "TestPaths.h"

namespace QtRocket::Test
{

/// The lines of tests/data/component-factory/report.txt below its line @p header ("-- BodyTube",
/// "== clicks (...)"), up to the next line that starts a block ("-- " or "== ") or the end, each
/// with its '\n'. Empty, with a failure, when the report cannot be read or has no such line.
[[nodiscard]] inline std::string componentFactoryReportBlock(std::string_view header)
{
    const std::filesystem::path          path  = testDataDir() / "component-factory" / "report.txt";
    const Result<std::vector<std::byte>> bytes = readFile(path);
    if (!bytes.has_value())
    {
        ADD_FAILURE() << bytes.error().toString();
        return {};
    }
    const std::string text = bytesToString(*bytes);
    std::string       block;
    bool              inBlock = false;
    std::size_t       at      = 0;
    while (at < text.size())
    {
        const std::size_t      end  = text.find('\n', at);
        const std::size_t      next = end == std::string::npos ? text.size() : end + 1;
        const std::string_view line = std::string_view(text).substr(at, next - at);
        const std::string_view bare = line.substr(0, line.find('\n'));
        if (inBlock && (bare.starts_with("-- ") || bare.starts_with("== ")))
        {
            break;
        }
        if (inBlock)
        {
            block += line;
        }
        inBlock = inBlock || bare == header;
        at      = next;
    }
    if (!inBlock)
    {
        ADD_FAILURE() << "report.txt has no line '" << header << "'";
    }
    return block;
}

}  // namespace QtRocket::Test
