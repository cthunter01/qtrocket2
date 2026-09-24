#include "QtRocket/rocket/ComponentChangeEvent.h"

#include <optional>
#include <string>
#include <string_view>

#include "QtRocket/util/BugError.h"

namespace QtRocket
{

std::optional<ComponentChangeEvent::Type> ComponentChangeEvent::typeFromValue(
    int typeNumber) noexcept
{
    for (const Type type : kAllTypes)
    {
        if (value(type) == typeNumber)
        {
            return type;
        }
    }
    return std::nullopt;
}

std::string_view ComponentChangeEvent::typeName(Type type) noexcept
{
    switch (type)
    {
        case Type::ERROR:
            return "Error";
        case Type::NON_FUNCTIONAL:
            return "nonFunctional";
        case Type::MASS:
            return "Mass";
        case Type::AERODYNAMIC:
            return "Aerodynamic";
        case Type::TREE:
            return "TREE";
        case Type::UNDO:
            return "UNDO";
        case Type::MOTOR:
            return "Motor";
        case Type::EVENT:
            return "Event";
        case Type::TEXTURE:
            return "Texture";
        case Type::GRAPHIC:
            return "Configuration";
        case Type::TREE_CHILDREN:
            return "TREE_CHILDREN";
    }
    return "Error";
}

ComponentChangeEvent::ComponentChangeEvent(RocketComponent* source, int type)
  : m_source(source), m_type(type)
{
    QTROCKET_ASSERT(source != nullptr);
}

ComponentChangeEvent::ComponentChangeEvent(RocketComponent* source, Type type)
  : m_source(source), m_type(value(type))
{
    QTROCKET_ASSERT(source != nullptr);
    if (type == Type::ERROR)
    {
        bug("no event type provided");
    }
}

bool ComponentChangeEvent::isNonFunctionalChange() const noexcept
{
    // Texture and graphic changes are also non-functional (visual only, no simulation impact).
    constexpr int kNonFunctionalMask =
        value(Type::NON_FUNCTIONAL) | value(Type::TEXTURE) | value(Type::GRAPHIC);
    return (m_type & ~kNonFunctionalMask) == 0;
}

std::string ComponentChangeEvent::toString() const
{
    std::string s;
    if (isNonFunctionalChange())
    {
        s += ",nonfunc";
    }
    if (isMassChange())
    {
        s += ",mass";
    }
    if (isAerodynamicChange())
    {
        s += ",aero";
    }
    if (isTreeChange())
    {
        s += ",tree";
    }
    if (isTreeChildrenChange())
    {
        s += ",treechild";
    }
    if (isUndoChange())
    {
        s += ",undo";
    }
    if (isMotorChange())
    {
        s += ",motor";
    }
    if (isEventChange())
    {
        s += ",event";
    }
    if (!s.empty())
    {
        s.erase(0, 1);
    }
    return "ComponentChangeEvent[" + s + "]";
}

}  // namespace QtRocket
