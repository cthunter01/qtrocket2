#pragma once

// OpenRocket's design of the add buttons' 25 clicks (tests/data/component-factory: the rocket.ork
// that OpenRocket saves for it) and the comparison of a design saved by QtRocket with it: the
// core's ComponentFactory (file/openrocket/add_buttons_save_tests.cpp) and the GUI's add buttons
// (tests/gui/main/ComponentAddButtonsTests.cpp) build the same design. Test-only, header-only.
//
// The comparison is line for line after two normalisations of both texts (normalisedDesignText()):
// - the text of every <id> element, a random UUID, becomes "#<n>", n numbering the distinct ids
//   from 1 in the order of their first appearance, so that a reference between ids would still be
//   compared;
// - the value of the creator attribute becomes "*": QtRocket writes its own name and version.
// Nothing else is normalised: rocket.ork holds no time stamp (the archive's entry times are not
// compared), and every number of the design is written as Java writes it.

#include <cstddef>
#include <filesystem>
#include <format>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/file/ZipArchive.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"
#include "TestPaths.h"

namespace QtRocket::Test
{

/// @p text with the text of each <id> element replaced by "#<n>" (n numbering the distinct ids by
/// their first appearance), and the value of each creator attribute by "*".
[[nodiscard]] inline std::string normalisedDesignText(std::string_view text)
{
    constexpr std::string_view kIdOpen       = "<id>";
    constexpr std::string_view kIdClose      = "</id>";
    constexpr std::string_view kCreatorOpen  = "creator=\"";
    constexpr std::string_view kCreatorClose = "\"";
    constexpr std::size_t      kNone         = std::string_view::npos;
    std::vector<std::string>   seen;
    const auto                 numberOf = [&seen](std::string_view id) {
        for (std::size_t i = 0; i < seen.size(); ++i)
        {
            if (seen[i] == id)
            {
                return i + 1;
            }
        }
        seen.emplace_back(id);
        return seen.size();
    };
    std::string result;
    std::size_t at = 0;
    while (at < text.size())
    {
        const std::size_t id      = text.find(kIdOpen, at);
        const std::size_t creator = text.find(kCreatorOpen, at);
        const bool        isId    = id != kNone && id <= creator;
        const std::size_t open    = isId ? id : creator;
        const std::size_t valueAt =
            open == kNone ? kNone : open + (isId ? kIdOpen.size() : kCreatorOpen.size());
        const std::size_t close =
            valueAt == kNone ? kNone : text.find(isId ? kIdClose : kCreatorClose, valueAt);
        if (close == kNone)
        {
            result += text.substr(at);
            break;
        }
        result += text.substr(at, valueAt - at);
        result += isId ? std::format("#{}", numberOf(text.substr(valueAt, close - valueAt)))
                       : std::string("*");
        at = close;
    }
    return result;
}

/// OpenRocket's rocket.ork of the add buttons' design (tests/data/component-factory).
[[nodiscard]] inline std::string openRocketsAddButtonsDesign()
{
    const std::filesystem::path path =
        testDataDir() / "component-factory" / "add-buttons-rocket.ork.xml";
    const Result<std::vector<std::byte>> bytes = readFile(path);
    EXPECT_TRUE(bytes.has_value()) << bytes.error().toString();
    return bytes.has_value() ? bytesToString(*bytes) : std::string();
}

/// The rocket.ork of @p bytes, a design archive, which must be its only entry; empty (and a
/// failure) otherwise.
[[nodiscard]] inline std::string theOnlyDesignOf(const std::vector<std::byte>& bytes)
{
    const Result<ZipArchive> archive = ZipArchive::fromBytes(bytes);
    if (!archive.has_value() || archive->entries().size() != 1 ||
        archive->find("rocket.ork") == nullptr)
    {
        ADD_FAILURE() << "not an archive of rocket.ork alone";
        return {};
    }
    return bytesToString(*archive->find("rocket.ork"));
}

}  // namespace QtRocket::Test
