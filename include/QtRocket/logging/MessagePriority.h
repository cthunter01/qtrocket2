#pragma once

#include <array>
#include <string_view>

namespace QtRocket
{

/// How important a user-visible message is (OpenRocket: logging/MessagePriority).
enum class MessagePriority
{
    LOW,
    NORMAL,
    HIGH,
};

/// Every priority, in declaration order (Java: values()).
inline constexpr std::array<MessagePriority, 3> kAllPriorities{
    MessagePriority::LOW, MessagePriority::NORMAL, MessagePriority::HIGH};

/// The label written into .ork files: "LOW", "NORMAL" or "HIGH" (Java: getExportLabel()).
[[nodiscard]] std::string_view exportLabel(MessagePriority priority) noexcept;

/// The priority with that export label; NORMAL when the label is unknown (Java: fromExportLabel()).
[[nodiscard]] MessagePriority priorityFromExportLabel(std::string_view label) noexcept;

}  // namespace QtRocket
