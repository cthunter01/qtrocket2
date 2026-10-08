#pragma once

#include <string>

namespace QtRocket
{

class FlightDataType;

/// A flight data type the user defined by a formula over the others (OpenRocket's
/// simulation/customexpression/CustomExpression), carried opaquely: its name ("Kinetic energy"),
/// its symbol, its unit and the expression text, as the `<datatypes>` element of an .ork file
/// holds them. A document keeps the list, so that a file that has custom expressions can be
/// loaded, shows its stored columns under their types and is saved back unchanged.
///
/// A value type: a copy is Java's clone(), and operator== is Java's equals(), the four strings.
///
/// Deviations from OpenRocket:
/// - Nothing is parsed or evaluated. Not ported: the expression builder (exp4j) with the index
///   and range sub-expressions ("m[1.2]", "h[1:2]"), checkSymbol(), checkName(), checkUnit(),
///   checkExpression() and checkAll(), evaluate() and evaluateDouble(), hash() and hashCode(),
///   and CustomExpressionSimulationListener, which would store the values during a flight. So a
///   simulation run here has no columns for the custom expressions (in OpenRocket's core nothing
///   attaches that listener either; its GUI does). The places are marked HOOK(custom-expression).
/// - No document pointer, and so no addToDocument() and no overwrite(): the document's list is
///   edited through the document. In Java every constructor and every setExpression() reads the
///   document's flight data types to build its parser, which has two effects this class does not
///   have: getType() of the expressions already in the document is called, so their types are
///   registered then and not only when someone asks, and an expression in the document whose
///   symbol is not a valid regular expression (such as "(") makes the making of every later
///   expression of that document throw a PatternSyntaxException.
/// - toString() never fails (Java: a NullPointerException for a null name; a name here is never
///   null).
class CustomExpression
{
public:
    /// An expression of four empty strings (Java: CustomExpression(doc)).
    CustomExpression() = default;

    /// Java: CustomExpression(doc, name, symbol, unit, expression).
    CustomExpression(std::string name, std::string symbol, std::string unit,
                     std::string expression);

    /// The long name, e.g. "Kinetic energy".
    [[nodiscard]] const std::string& getName() const noexcept { return m_name; }
    void                             setName(std::string name);

    /// The short, locale-independent symbol the expression is referred to by in other
    /// expressions.
    [[nodiscard]] const std::string& getSymbol() const noexcept { return m_symbol; }
    void                             setSymbol(std::string symbol);

    /// The unit of the result, as text: an SI unit symbol ("m/s") or anything else.
    [[nodiscard]] const std::string& getUnit() const noexcept { return m_unit; }
    void                             setUnit(std::string unit);

    /// The expression as the user wrote it.
    [[nodiscard]] const std::string& getExpressionString() const noexcept { return m_expression; }
    void                             setExpression(std::string expression);

    /// The flight data type of the values of this expression, which is also the type a stored
    /// column of this name is read as (Java: getType()): FlightDataType::getType(name, symbol,
    /// group) with the unit group the unit names as an SI unit (unitGroupFromSiUnit(): "m" is
    /// ALL_LENGTHS, "J" ENERGY, ...), else FlightDataType::getTypeWithFixedUnit(name, symbol,
    /// unit). Not a plain getter: as in Java the type is made, or an existing type of that symbol
    /// replaced for later lookups, in the process-wide registry of FlightDataType (see
    /// FlightDataType::getType()). An expression named and unit-ed like a built-in type still
    /// gets a type of its own when the unit groups differ ("Altitude", "h", "m": the built-in
    /// altitude is a DISTANCE, "m" is ALL_LENGTHS). The type outlives the expression (see
    /// FlightDataType); all the same, bind the reference from a named expression, since GCC's
    /// -Wdangling-reference suspects a reference that a temporary's member function returns.
    [[nodiscard]] const FlightDataType& getType() const;

    /// "[Expression name=<name> expression=<expression> unit=<unit>]" (the symbol is not part of
    /// it, as in Java).
    [[nodiscard]] std::string toString() const;

    /// Java's equals(): the same name, symbol, expression and unit.
    [[nodiscard]] bool operator==(const CustomExpression&) const = default;

    // HOOK(custom-expression): checkSymbol(), checkName(), checkUnit(), checkExpression(),
    // checkAll(), evaluate(), evaluateDouble() and hash() come with the milestone that evaluates
    // custom expressions.

private:
    std::string m_name;
    std::string m_symbol;
    std::string m_unit;
    std::string m_expression;
};

}  // namespace QtRocket
