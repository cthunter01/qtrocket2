#pragma once

// A test element handler that records every call SimpleSax and DelegatorHandler make.

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "QtRocket/file/simplesax/ElementHandler.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/util/Error.h"

namespace QtRocket::Test
{

/// Handles every element itself, except those named "ignore" (ignored) and the one named
/// failOn (its openElement() fails with the message "fail <name>"); records the calls in log.
class RecordingHandler final : public ElementHandler
{
public:
    std::vector<std::string> log;
    std::string              failOn;

    [[nodiscard]] Result<ElementHandler*> openElement(std::string_view  element,
                                                      const Attributes& attributes,
                                                      WarningSet& /*warnings*/) override
    {
        log.push_back("open " + std::string(element) + describe(attributes));
        if (element == failOn)
        {
            return fail(ErrorCode::PARSE, "fail " + std::string(element));
        }
        if (element == "ignore")
        {
            return nullptr;
        }
        return this;
    }

    [[nodiscard]] Result<void> closeElement(std::string_view element, const Attributes& attributes,
                                            std::string_view content,
                                            WarningSet& /*warnings*/) override
    {
        std::string entry = "close " + std::string(element);
        entry += describe(attributes);
        entry += " [";
        entry += content;
        entry += "]";
        log.push_back(std::move(entry));
        return {};
    }

    [[nodiscard]] Result<void> endHandler(std::string_view element,
                                          const Attributes& /*attributes*/,
                                          std::string_view /*content*/,
                                          WarningSet& /*warnings*/) override
    {
        log.push_back("end " + std::string(element));
        return {};
    }

private:
    [[nodiscard]] static std::string describe(const Attributes& attributes)
    {
        std::string text;
        for (const auto& [name, value] : attributes)
        {
            text += " ";
            text += name;
            text += "=";
            text += value;
        }
        return text;
    }
};

}  // namespace QtRocket::Test
