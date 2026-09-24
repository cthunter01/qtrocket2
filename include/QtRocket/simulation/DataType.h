#pragma once

#include <string>

namespace QtRocket
{

class UnitGroup;

/// A type of data that can be stored in a DataBranch (OpenRocket's simulation/DataType, together
/// with the getUnitGroup() of util/UnitValue it extends): a named quantity with a short symbol,
/// shown in the units of a unit group. FlightDataType is the one the simulation uses.
///
/// Java's DataType declares getName() only; getSymbol() is here as well because every
/// implementation (FlightDataType, and later CADataType) has one and the plots label series with
/// it. Types are identity objects handed around by reference or pointer; the copy and move
/// operations are protected so that a DataType cannot be sliced.
class DataType
{
public:
    virtual ~DataType() = default;

    /// The name shown to the user.
    [[nodiscard]] virtual const std::string& getName() const = 0;
    /// The short symbol, e.g. "h" for altitude.
    [[nodiscard]] virtual const std::string& getSymbol() const = 0;
    /// The units the values are shown in; the values themselves are always in SI.
    [[nodiscard]] virtual const UnitGroup& getUnitGroup() const = 0;

protected:
    DataType()                           = default;
    DataType(const DataType&)            = default;
    DataType& operator=(const DataType&) = default;
    DataType(DataType&&)                 = default;
    DataType& operator=(DataType&&)      = default;
};

}  // namespace QtRocket
