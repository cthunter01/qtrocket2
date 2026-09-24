#include "QtRocket/logging/MessagePriority.h"

#include <string_view>

namespace QtRocket
{

std::string_view exportLabel(MessagePriority priority) noexcept
{
    switch (priority)
    {
        case MessagePriority::LOW:
            return "LOW";
        case MessagePriority::NORMAL:
            return "NORMAL";
        case MessagePriority::HIGH:
            return "HIGH";
    }
    return "NORMAL";
}

MessagePriority priorityFromExportLabel(std::string_view label) noexcept
{
    for (const MessagePriority priority : kAllPriorities)
    {
        if (exportLabel(priority) == label)
        {
            return priority;
        }
    }
    return MessagePriority::NORMAL;
}

}  // namespace QtRocket
