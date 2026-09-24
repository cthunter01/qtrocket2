#pragma once

#include <cstddef>
#include <vector>

#include "QtRocket/logging/MessagePriority.h"
#include "QtRocket/logging/MessageSet.h"
#include "QtRocket/logging/Warning.h"

namespace QtRocket
{

/// A MessageSet of warnings (OpenRocket: logging/WarningSet). add(std::string_view) builds a
/// Warning::Other with the default priority; the helpers group the warnings by priority: HIGH is
/// "critical", NORMAL "normal" and LOW "informational".
///
/// It adds no state and is final: MessageSet's destructor is not virtual, so a WarningSet must
/// never be deleted through a MessageSet<Warning> pointer.
class WarningSet final : public MessageSet<Warning>
{
public:
    /// Java: getNrOfCriticalWarnings().
    [[nodiscard]] std::size_t countCritical() const noexcept
    {
        return countWithPriority(MessagePriority::HIGH);
    }
    /// Java: getNrOfNormalWarnings().
    [[nodiscard]] std::size_t countNormal() const noexcept
    {
        return countWithPriority(MessagePriority::NORMAL);
    }
    /// Java: getNrOfInformationalWarnings().
    [[nodiscard]] std::size_t countInformational() const noexcept
    {
        return countWithPriority(MessagePriority::LOW);
    }

    /// Java: getCriticalWarnings().
    [[nodiscard]] std::vector<const Warning*> criticalWarnings() const
    {
        return messagesWithPriority(MessagePriority::HIGH);
    }
    /// Java: getNormalWarnings().
    [[nodiscard]] std::vector<const Warning*> normalWarnings() const
    {
        return messagesWithPriority(MessagePriority::NORMAL);
    }
    /// Java: getInformationalWarnings().
    [[nodiscard]] std::vector<const Warning*> informationalWarnings() const
    {
        return messagesWithPriority(MessagePriority::LOW);
    }
};

}  // namespace QtRocket
