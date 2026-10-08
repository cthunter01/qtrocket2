#include "QtRocket/simulation/customexpression/CustomExpression.h"

#include <optional>
#include <string>
#include <utility>

#include "QtRocket/simulation/FlightDataType.h"
#include "QtRocket/unit/UnitGroup.h"

namespace QtRocket
{

CustomExpression::CustomExpression(std::string name, std::string symbol, std::string unit,
                                   std::string expression)
  : m_name(std::move(name)),
    m_symbol(std::move(symbol)),
    m_unit(std::move(unit)),
    m_expression(std::move(expression))
{
}

void CustomExpression::setName(std::string name)
{
    m_name = std::move(name);
}

void CustomExpression::setSymbol(std::string symbol)
{
    m_symbol = std::move(symbol);
}

void CustomExpression::setUnit(std::string unit)
{
    m_unit = std::move(unit);
}

void CustomExpression::setExpression(std::string expression)
{
    // This is the expression as supplied
    m_expression = std::move(expression);
    // HOOK(custom-expression): Java replaces the index and range sub-expressions and builds the
    // parser here, with every symbol of the document's flight data types as a variable.
}

const FlightDataType& CustomExpression::getType() const
{
    // Java: UnitGroup.SIUNITS.get(unit), else a new FixedUnitGroup(unit).
    if (const std::optional<UnitGroupId> units = unitGroupFromSiUnit(m_unit))
    {
        return FlightDataType::getType(m_name, m_symbol, *units);
    }
    return FlightDataType::getTypeWithFixedUnit(m_name, m_symbol, m_unit);
}

std::string CustomExpression::toString() const
{
    return "[Expression name=" + m_name + " expression=" + m_expression + " unit=" + m_unit + "]";
}

}  // namespace QtRocket
