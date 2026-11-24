#pragma once

#include <array>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <format>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "QtRocket/plot/PlotAxisBounds.h"
#include "QtRocket/simulation/DataBranch.h"
#include "QtRocket/unit/Unit.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Color.h"
#include "QtRocket/util/LineStyle.h"
#include "QtRocket/util/MathUtil.h"

namespace QtRocket
{

/// A branch type a PlotConfiguration of the data type @p T plots: a DataBranch of @p T (Java:
/// B extends DataBranch<T>).
template <class B, class T>
concept PlotDataBranch = std::derived_from<B, DataBranch<T>>;

/// What one plot shows (OpenRocket's swing gui/plot/PlotConfiguration): the data type of the x
/// axis (the domain) with its unit, and rows for the y axes, each a data type with its unit, its
/// axis and how it is drawn. @p T is the data type (FlightDataType for the simulation plots) and
/// @p B the branch of data it comes from (FlightDataBranch); SimulationPlotConfiguration is the
/// configuration of the simulation plots, with its presets. The configuration also chooses the
/// rows' automatic axes (fillAutoAxes()) and the ranges of the two axes (fitAxes(),
/// getAllAxes()), which is what OpenRocket's plots show before the user zooms.
///
/// Rows: a type, its unit (the unit group's default when the row is added), its axis (0 the left
/// one, 1 the right one, kAutoAxis or any other negative value: chosen by fillAutoAxes()), and,
/// for the plot panel, a colour (nullopt: the plot's own) and a line style (SOLID).
///
/// The automatic axes (fillAutoAxes()) are found by brute force: every assignment of the
/// automatic rows to the two axes is scored by getGoodnessValue() on the main branch, the first
/// automatic row the most significant and axis 0 before axis 1 (with rows 1 and 2 automatic:
/// 00, 01, 10, 11), and the first with the largest finite score is chosen (a non-finite score
/// never replaces one, and when every score is non-finite the first assignment is chosen). The
/// score (getGoodnessValue()): fitAxes() on the main branch, then 100 * sqrt(data range / axis
/// range) for every row whose range on the main branch is neither NaN nor flat, + 1 when row 0 is
/// on the left axis, + 2 when the left axis holds zero, + 40 when both axes hold zero, + 50 when
/// only one axis is used (the other's range is NaN).
///
/// The ranges (fitAxes()): every row's extremes over the branches, in the row's unit, on its axis;
/// then on each axis a flat range (MathUtil::equals) is widened by 1 on both sides, 3 % of the
/// range is added on both sides, and zero is included when the end nearer to it is within 30 % of
/// the range; then, when both axes hold zero, the right axis is widened so that its zero is at the
/// height of the left one's (the common zero). Only + - * / and sqrt are involved, so the results
/// are Java's to the last bit on every platform (the build turns floating-point contraction off),
/// and tests pin them exactly.
///
/// Lifetime: the types and units are held by pointer and must outlive the configuration. The
/// FlightDataTypes, their unit groups and the process-wide unit groups' units live for the whole
/// process.
///
/// Copying: a value type, copied with its rows, ranges and name (Java: clone()).
/// cloneConfiguration() is the copy that keeps the dynamic type (Java's clone() keeps the class):
/// fillAutoAxes() makes its candidates with it, so that a subclass's getGoodnessValue() and
/// calculateAxisBounds() score and fit them. A subclass overrides it.
///
/// For subclasses (Java's protected members): calculateAxisBounds() (Milestone 7's component
/// analysis plots override it) and getGoodnessValue() are virtual, and axisBounds() gives an axis
/// to widen.
///
/// Deviations from OpenRocket:
/// - Bug 45 of the plan's section 11.4, fixed: when the left range starts or ends at exactly zero
///   (where the include-zero rule puts it) and the right one spans zero, Java's common zero divides
///   by zero (-1 / zeroLoc1, zeroLoc1 / (1 - zeroLoc1)) and gives the right axis an infinite
///   bound. Here the common zero is left out when it would give a bound that is not finite, so the
///   right axis keeps its range. The candidates of fillAutoAxes() are scored on that finite range,
///   so the automatic axes can differ from Java's there (Java scores the right rows 0): of the 486
///   plots of the examples' stored simulations, "Dual parachute deployment"'s first simulation
///   with the time step preset (Java: the time step on the left, QtRocket: on the right).
/// - Bug 46, fixed: a branch without a row's type (or whose values of it are all NaN) has NaN
///   extremes, which make Java's Math.min and Math.max NaN and so drop the row from its axis (an
///   axis whose only row it is stays NaN). Here a NaN extreme of a branch is ignored: the row's
///   range is that of the branches that have values of its type, the main branch included (an
///   empty main branch too). Where no extreme is NaN, Java's Math.min and Math.max decide (signed
///   zeros included).
/// - A configuration without rows (an unlisted OpenRocket bug, fixed): Java's getGoodnessValue()
///   reads row 0 for its bonus and throws IndexOutOfBoundsException, so fillAutoAxes() fails for
///   it, which OpenRocket's plot panel asks for when its last y row is removed (an internal error).
///   Here the bonus is left out and the ranges stay NaN.
/// - Indices are std::size_t. A programming error throws BugError where Java throws: an index
///   beyond the last row (Java: IndexOutOfBoundsException), an axis above 1 (Java:
///   IllegalArgumentException "Axis index too large"), a unit outside the row's or the domain's
///   unit group (IllegalArgumentException), fitAxes() with an automatic row
///   (IllegalStateException "fitAxes called with auto-selected axis") and fitAxes() of a
///   configuration with rows over no branch (IndexOutOfBoundsException). As in Java, a unit given
///   with the row's type (the four-argument setPlotDataType()) is not checked.
/// - getAllAxes() is the two ranges themselves, read-only (Java: a new list of the Axis objects
///   themselves, which the caller could change).
/// - fitAxes() takes the branches as pointers; fitAxes() of one branch is protected, as in Java.
/// - The name has a translation key beside it (getNameKey(): the key of messages.properties a
///   preset's English name comes from, empty for any other name), so that the GUI shows the name
///   in the user's language; Java keeps the translated name only.
/// - Colours are QtRocket::Color, nullopt for Java's null.
/// - Java's fillAutoAxes() falls back to a clone when its search returns no configuration; the
///   search always keeps its first candidate, so the fallback is kept but never taken.
/// - Not ported: roundScale(), roundScaleUp() and roundScaleDown() (private and unused) and
///   toString() (the name).
template <BranchDataType T, PlotDataBranch<T> B>
class PlotConfiguration
{
public:
    /// The number of axes, the left (0) and the right (1) one (Java: axesCount).
    static constexpr int kAxesCount = 2;
    /// The axis of a row that fillAutoAxes() assigns (any negative axis is one).
    static constexpr int kAutoAxis = -1;

    /// A configuration named @p name over @p domainType (in its unit group's default unit), with
    /// no rows and NaN ranges.
    PlotConfiguration(std::string name, const T& domainType)
      : PlotConfiguration(std::move(name), std::string{}, domainType)
    {
    }

    /// The same with the translation key @p nameKey of the name (see getNameKey()).
    PlotConfiguration(std::string name, std::string nameKey, const T& domainType)
      : m_domainAxisType(&domainType),
        m_domainAxisUnit(&domainType.getUnitGroup().getDefaultUnit()),
        m_name(std::move(name)),
        m_nameKey(std::move(nameKey))
    {
    }

    virtual ~PlotConfiguration()                           = default;
    PlotConfiguration(const PlotConfiguration&)            = default;
    PlotConfiguration& operator=(const PlotConfiguration&) = default;
    PlotConfiguration(PlotConfiguration&&)                 = default;
    PlotConfiguration& operator=(PlotConfiguration&&)      = default;

    //// The rows

    /// Adds a row for @p type in its unit group's default unit on an automatic axis, with no
    /// colour and a solid line.
    void addPlotDataType(const T& type);

    /// The same on @p axis (0 or 1, or negative for an automatic one).
    /// @throws BugError when @p axis is above 1 ("Axis index too large")
    void addPlotDataType(const T& type, int axis);

    /// Changes row @p index's type to @p type; its unit becomes the default unit of @p type's unit
    /// group unless both types have the same unit group (the same object).
    /// @throws BugError when there is no row @p index
    void setPlotDataType(std::size_t index, const T& type);

    /// Shows row @p index in @p unit.
    /// @throws BugError when there is no row @p index, or when @p unit is not in the unit group of
    ///         the row's type (UnitGroup::contains)
    void setPlotDataUnit(std::size_t index, const Unit& unit);

    /// Puts row @p index on @p axis (0 or 1, or negative for an automatic one).
    /// @throws BugError when there is no row @p index or @p axis is above 1
    void setPlotDataAxis(std::size_t index, int axis);

    /// Changes row @p index's type, unit and axis at once; @p unit is not checked (as in Java).
    /// @throws BugError when there is no row @p index or @p axis is above 1
    void setPlotDataType(std::size_t index, const T& type, const Unit& unit, int axis);

    /// Removes row @p index; the rows after it move up.
    /// @throws BugError when there is no row @p index
    void removePlotDataType(std::size_t index);

    /// Row @p index's type, unit, axis, colour and line style.
    /// @throws BugError when there is no row @p index
    [[nodiscard]] const T&                    getType(std::size_t index) const;
    [[nodiscard]] const Unit&                 getUnit(std::size_t index) const;
    [[nodiscard]] int                         getAxis(std::size_t index) const;
    [[nodiscard]] const std::optional<Color>& getPlotDataColor(std::size_t index) const;
    [[nodiscard]] LineStyle                   getPlotDataLineStyle(std::size_t index) const;

    /// Draws row @p index in @p color (nullopt: the plot's own colour).
    /// @throws BugError when there is no row @p index
    void setPlotDataColor(std::size_t index, std::optional<Color> color);

    /// Draws row @p index with @p style.
    /// @throws BugError when there is no row @p index
    void setPlotDataLineStyle(std::size_t index, LineStyle style);

    /// Gives every row no colour and a solid line (Java: clearPlotAppearances()).
    void clearPlotAppearances() noexcept;

    /// The number of rows (Java: getDataCount()).
    [[nodiscard]] std::size_t getDataCount() const noexcept { return m_rows.size(); }

    //// The domain (x) axis

    [[nodiscard]] const T& getDomainAxisType() const noexcept { return *m_domainAxisType; }

    /// Plots the rows over @p type; the domain unit becomes the default unit of @p type's unit
    /// group unless both types have the same unit group (the same object).
    void setDomainAxisType(const T& type);

    [[nodiscard]] const Unit& getDomainAxisUnit() const noexcept { return *m_domainAxisUnit; }

    /// Shows the domain in @p unit.
    /// @throws BugError when @p unit is not in the domain type's unit group (UnitGroup::contains)
    void setDomainAxisUnit(const Unit& unit);

    //// The axes and their ranges

    /// The ranges of the left (0) and the right (1) axis, as the last fitAxes() left them (NaN
    /// before it, and on an axis without rows).
    [[nodiscard]] const std::array<PlotAxisBounds, kAxesCount>& getAllAxes() const noexcept
    {
        return m_allAxes;
    }

    /// A copy of this configuration (made by cloneConfiguration(), so of its dynamic type) with
    /// the best assignment of the automatic rows to the axes for @p data (the main branch: the
    /// first of a simulation) and the ranges fitted to @p data (see the class comment). This
    /// configuration does not change.
    [[nodiscard]] std::unique_ptr<PlotConfiguration> fillAutoAxes(const B& data) const;

    /// Fits the ranges of the axes to the rows' data in @p data, every branch's, the first being
    /// the main branch (see the class comment and calculateAxisBounds()).
    /// @throws BugError when a row is on an automatic axis, when @p data holds a null pointer, or
    ///         when the configuration has rows and @p data is empty
    void fitAxes(std::span<const B* const> data);

    /// Gives every row the default unit of its type's unit group; the domain keeps its unit.
    /// Returns this configuration (Java: resetUnits()).
    PlotConfiguration& resetUnits();

    //// The name

    /// The name the user sees: a preset's English name (see getNameKey()), "Custom", or what the
    /// caller named it.
    [[nodiscard]] const std::string& getName() const noexcept { return m_name; }

    /// The translation key the name comes from ("PlotConfiguration.Verticalmotion", the key of
    /// messages.properties), or empty when the name is not a translated text.
    [[nodiscard]] const std::string& getNameKey() const noexcept { return m_nameKey; }

    /// Renames the configuration to @p name, the English text of @p nameKey (empty: no key).
    void setName(std::string name, std::string nameKey = {});

    /// A copy of this configuration of the same dynamic type (Java: cloneConfiguration(), whose
    /// super.clone() keeps the class). A subclass overrides it.
    // MSVC instantiates a class template's virtual member functions with the class, GCC and Clang
    // only when they are used (SimulationPlotConfiguration.cpp instantiates them all for the
    // simulation plots); the bodies of the three are valid for every T and B.
    // NOLINTNEXTLINE(portability-template-virtual-member-function): valid for every T and B
    [[nodiscard]] virtual std::unique_ptr<PlotConfiguration> cloneConfiguration() const;

protected:
    /// fitAxes() over @p data alone.
    /// @throws BugError when a row is on an automatic axis
    void fitAxes(const B& data);

    /// Widens the axes (cleared by fitAxes()) to every row's extremes over @p data, in the row's
    /// unit: the extremes of the first branch, then of every later branch that is not empty, a
    /// NaN extreme of a branch ignored (bug 46, see the class comment). Milestone 7's component
    /// analysis plots override it.
    /// @throws BugError when a row is on an automatic axis, when @p data holds a null pointer, or
    ///         when the configuration has rows and @p data is empty
    // NOLINTNEXTLINE(portability-template-virtual-member-function): valid for every T and B
    virtual void calculateAxisBounds(std::span<const B* const> data);

    /// Fits the ranges to @p data alone (fitAxes()) and scores how well they suit the rows, the
    /// larger the better (see the class comment).
    /// @throws BugError when a row is on an automatic axis
    // NOLINTNEXTLINE(portability-template-virtual-member-function): valid for every T and B
    [[nodiscard]] virtual double getGoodnessValue(const B& data);

    /// The range of axis @p axis (0 or 1), to widen (Java's protected allAxes).
    /// @throws BugError when @p axis is not 0 or 1
    [[nodiscard]] PlotAxisBounds& axisBounds(int axis);

private:
    /// BONUS_FIRST_TYPE_ON_FIRST_AXIS: the bonus for row 0 on the left axis.
    static constexpr double kBonusFirstTypeOnFirstAxis = 1.0;
    /// BONUS_FIRST_AXIS_HAS_ZERO: the bonus for a left axis that holds zero.
    static constexpr double kBonusFirstAxisHasZero = 2.0;
    /// BONUS_COMMON_ZERO: the bonus for two axes that hold zero (a common zero).
    static constexpr double kBonusCommonZero = 40.0;
    /// BONUS_ONLY_ONE_AXIS: the bonus for a single axis in use.
    static constexpr double kBonusOnlyOneAxis = 50.0;
    /// INCLUDE_ZERO_DISTANCE: zero is included when it is within 30 % of the range.
    static constexpr double kIncludeZeroDistance = 0.3;

    struct Row
    {
        const T*             type{nullptr};
        const Unit*          unit{nullptr};
        int                  axis{kAutoAxis};
        std::optional<Color> color;
        LineStyle            lineStyle{LineStyle::SOLID};
    };

    /// Row @p index.
    /// @throws BugError when there is no row @p index
    [[nodiscard]] Row& row(std::size_t index)
    {
        checkRow(index);
        return m_rows[index];
    }
    [[nodiscard]] const Row& row(std::size_t index) const
    {
        checkRow(index);
        return m_rows[index];
    }

    /// @throws BugError when there is no row @p index
    void checkRow(std::size_t index) const;

    /// The best configuration of the automatic rows from the first automatic one on, with its
    /// score (Java: recursiveFillAutoAxes()).
    [[nodiscard]] std::pair<std::unique_ptr<PlotConfiguration>, double> recursiveFillAutoAxes(
        const B& data) const;

    /// Whether @p candidateValue replaces @p bestValue: only a finite value does, and it replaces
    /// a non-finite one or a smaller one (Java: isBetterGoodness()).
    [[nodiscard]] static bool isBetterGoodness(double candidateValue, double bestValue) noexcept;

    /// The smaller of @p current and @p value as Java's Math.min, a NaN of either ignored (bug 46).
    [[nodiscard]] static double combinedMinimum(double current, double value) noexcept;
    /// The larger of @p current and @p value as Java's Math.max, a NaN of either ignored (bug 46).
    [[nodiscard]] static double combinedMaximum(double current, double value) noexcept;

    /// @throws BugError when @p axis is above 1
    static void checkAxis(int axis);

    std::vector<Row>                       m_rows;
    const T*                               m_domainAxisType;
    const Unit*                            m_domainAxisUnit;
    std::array<PlotAxisBounds, kAxesCount> m_allAxes{};
    std::string                            m_name;
    std::string                            m_nameKey;
};

template <BranchDataType T, PlotDataBranch<T> B>
void PlotConfiguration<T, B>::addPlotDataType(const T& type)
{
    m_rows.push_back(Row{.type      = &type,
                         .unit      = &type.getUnitGroup().getDefaultUnit(),
                         .axis      = kAutoAxis,
                         .color     = std::nullopt,
                         .lineStyle = LineStyle::SOLID});
}

template <BranchDataType T, PlotDataBranch<T> B>
void PlotConfiguration<T, B>::addPlotDataType(const T& type, int axis)
{
    checkAxis(axis);
    m_rows.push_back(Row{.type      = &type,
                         .unit      = &type.getUnitGroup().getDefaultUnit(),
                         .axis      = axis,
                         .color     = std::nullopt,
                         .lineStyle = LineStyle::SOLID});
}

template <BranchDataType T, PlotDataBranch<T> B>
void PlotConfiguration<T, B>::setPlotDataType(std::size_t index, const T& type)
{
    Row&     changed  = row(index);
    const T* origType = changed.type;
    changed.type      = &type;

    if (&origType->getUnitGroup() != &type.getUnitGroup())
    {
        changed.unit = &type.getUnitGroup().getDefaultUnit();
    }
}

template <BranchDataType T, PlotDataBranch<T> B>
void PlotConfiguration<T, B>::setPlotDataUnit(std::size_t index, const Unit& unit)
{
    Row& changed = row(index);
    if (!changed.type->getUnitGroup().contains(unit))
    {
        bug(std::format("Attempting to set unit {} to group {}", unit.getUnit(),
                        changed.type->getUnitGroup().toString()));
    }
    changed.unit = &unit;
}

template <BranchDataType T, PlotDataBranch<T> B>
void PlotConfiguration<T, B>::setPlotDataAxis(std::size_t index, int axis)
{
    Row& changed = row(index);
    checkAxis(axis);
    changed.axis = axis;
}

template <BranchDataType T, PlotDataBranch<T> B>
void PlotConfiguration<T, B>::setPlotDataType(std::size_t index, const T& type, const Unit& unit,
                                              int axis)
{
    Row& changed = row(index);
    checkAxis(axis);
    changed.type = &type;
    changed.unit = &unit;
    changed.axis = axis;
}

template <BranchDataType T, PlotDataBranch<T> B>
void PlotConfiguration<T, B>::removePlotDataType(std::size_t index)
{
    static_cast<void>(row(index));
    m_rows.erase(m_rows.begin() + static_cast<std::ptrdiff_t>(index));
}

template <BranchDataType T, PlotDataBranch<T> B>
const T& PlotConfiguration<T, B>::getType(std::size_t index) const
{
    return *row(index).type;
}

template <BranchDataType T, PlotDataBranch<T> B>
const Unit& PlotConfiguration<T, B>::getUnit(std::size_t index) const
{
    return *row(index).unit;
}

template <BranchDataType T, PlotDataBranch<T> B>
int PlotConfiguration<T, B>::getAxis(std::size_t index) const
{
    return row(index).axis;
}

template <BranchDataType T, PlotDataBranch<T> B>
const std::optional<Color>& PlotConfiguration<T, B>::getPlotDataColor(std::size_t index) const
{
    return row(index).color;
}

template <BranchDataType T, PlotDataBranch<T> B>
LineStyle PlotConfiguration<T, B>::getPlotDataLineStyle(std::size_t index) const
{
    return row(index).lineStyle;
}

template <BranchDataType T, PlotDataBranch<T> B>
void PlotConfiguration<T, B>::setPlotDataColor(std::size_t index, std::optional<Color> color)
{
    row(index).color = color;
}

template <BranchDataType T, PlotDataBranch<T> B>
void PlotConfiguration<T, B>::setPlotDataLineStyle(std::size_t index, LineStyle style)
{
    row(index).lineStyle = style;
}

template <BranchDataType T, PlotDataBranch<T> B>
void PlotConfiguration<T, B>::clearPlotAppearances() noexcept
{
    for (Row& each : m_rows)
    {
        each.color     = std::nullopt;
        each.lineStyle = LineStyle::SOLID;
    }
}

template <BranchDataType T, PlotDataBranch<T> B>
void PlotConfiguration<T, B>::setDomainAxisType(const T& type)
{
    const bool setUnit = &m_domainAxisType->getUnitGroup() != &type.getUnitGroup();

    m_domainAxisType = &type;
    if (setUnit)
    {
        m_domainAxisUnit = &m_domainAxisType->getUnitGroup().getDefaultUnit();
    }
}

template <BranchDataType T, PlotDataBranch<T> B>
void PlotConfiguration<T, B>::setDomainAxisUnit(const Unit& unit)
{
    if (!m_domainAxisType->getUnitGroup().contains(unit))
    {
        bug(std::format("Setting unit {} to type {}", unit.getUnit(), m_domainAxisType->getName()));
    }
    m_domainAxisUnit = &unit;
}

template <BranchDataType T, PlotDataBranch<T> B>
std::unique_ptr<PlotConfiguration<T, B>> PlotConfiguration<T, B>::fillAutoAxes(const B& data) const
{
    std::unique_ptr<PlotConfiguration> config = recursiveFillAutoAxes(data).first;
    if (config == nullptr)
    {
        // Keep plotting functional even if every axis combination scores as NaN/Infinity.
        config = cloneConfiguration();
    }
    config->fitAxes(data);
    return config;
}

template <BranchDataType T, PlotDataBranch<T> B>
std::pair<std::unique_ptr<PlotConfiguration<T, B>>, double>
PlotConfiguration<T, B>::recursiveFillAutoAxes(const B& data) const
{
    // Create copy to fill in
    std::unique_ptr<PlotConfiguration> copy = cloneConfiguration();

    std::size_t autoindex = 0;
    for (; autoindex < m_rows.size(); ++autoindex)
    {
        if (m_rows[autoindex].axis < 0)
        {
            break;
        }
    }

    if (autoindex >= m_rows.size())
    {
        // All axes have been assigned, just return since we are already the best
        const double goodness = copy->getGoodnessValue(data);
        return {std::move(copy), goodness};
    }

    // Set the auto-selected index one at a time and choose the best one
    std::unique_ptr<PlotConfiguration> best;
    double                             bestValue = -std::numeric_limits<double>::infinity();
    for (int i = 0; i < kAxesCount; ++i)
    {
        copy->m_rows[autoindex].axis     = i;
        auto [candidate, candidateValue] = copy->recursiveFillAutoAxes(data);
        if (best == nullptr || isBetterGoodness(candidateValue, bestValue))
        {
            best      = std::move(candidate);
            bestValue = candidateValue;
        }
    }

    return {std::move(best), bestValue};
}

template <BranchDataType T, PlotDataBranch<T> B>
bool PlotConfiguration<T, B>::isBetterGoodness(double candidateValue, double bestValue) noexcept
{
    if (!std::isfinite(candidateValue))
    {
        return false;
    }
    return !std::isfinite(bestValue) || candidateValue > bestValue;
}

template <BranchDataType T, PlotDataBranch<T> B>
void PlotConfiguration<T, B>::fitAxes(const B& data)
{
    const std::array<const B*, 1> branches{&data};
    fitAxes(branches);
}

template <BranchDataType T, PlotDataBranch<T> B>
void PlotConfiguration<T, B>::fitAxes(std::span<const B* const> data)
{
    // Reset axes
    for (PlotAxisBounds& a : m_allAxes)
    {
        a.reset();
    }

    // Add full range to the axes
    calculateAxisBounds(data);

    // Ensure non-zero (or NaN) range, add a few percent range, include zero if it is close
    for (PlotAxisBounds& a : m_allAxes)
    {
        if (MathUtil::equals(a.getMinValue(), a.getMaxValue()))
        {
            a.addBound(a.getMinValue() - 1);
            a.addBound(a.getMaxValue() + 1);
        }

        const double addition = a.getRangeLength() * 0.03;
        a.addBound(a.getMinValue() - addition);
        a.addBound(a.getMaxValue() + addition);

        const double dist = MathUtil::javaMin(std::abs(a.getMinValue()), std::abs(a.getMaxValue()));
        if (dist <= a.getRangeLength() * kIncludeZeroDistance)
        {
            a.addBound(0);
        }
    }

    // Check whether to use a common zero
    PlotAxisBounds& left  = m_allAxes[0];
    PlotAxisBounds& right = m_allAxes[1];

    if (left.getMinValue() > 0 || left.getMaxValue() < 0 || right.getMinValue() > 0 ||
        right.getMaxValue() < 0 || std::isnan(left.getMinValue()) ||
        std::isnan(right.getMinValue()))
    {
        return;
    }

    //// Compute common zero
    const double min1 = left.getMinValue();
    const double max1 = left.getMaxValue();
    double       min2 = right.getMinValue();
    double       max2 = right.getMaxValue();

    // Get the zero locations, scaled down to a value from 0 to 1 (0 = bottom of y axis, 1 = top)
    const double zeroLoc1 = -min1 / left.getRangeLength();
    const double zeroLoc2 = -min2 / right.getRangeLength();

    // Scale the right axis
    if (zeroLoc1 > zeroLoc2)
    {
        min2 = -max2 * (zeroLoc1 / (1 - zeroLoc1));
    }
    else
    {
        max2 = min2 * ((-1 / zeroLoc1) + 1);
    }

    // Bug 45 (see the class comment): a left range that starts or ends at exactly zero makes the
    // scale infinite; the right axis then keeps its range.
    if (!std::isfinite(min2) || !std::isfinite(max2))
    {
        return;
    }

    // Apply scale
    left.addBound(min1);
    left.addBound(max1);
    right.addBound(min2);
    right.addBound(max2);
}

template <BranchDataType T, PlotDataBranch<T> B>
void PlotConfiguration<T, B>::calculateAxisBounds(std::span<const B* const> data)
{
    for (const B* branch : data)
    {
        QTROCKET_ASSERT(branch != nullptr);
    }
    for (const Row& each : m_rows)
    {
        if (each.axis < 0)
        {
            bug("fitAxes called with auto-selected axis");
        }
        if (data.empty())
        {
            bug("fitAxes called without data");
        }
        PlotAxisBounds& axis = axisBounds(each.axis);

        double min = each.unit->toUnit(data.front()->getMinimum(*each.type));
        double max = each.unit->toUnit(data.front()->getMaximum(*each.type));

        for (std::size_t j = 1; j < data.size(); ++j)
        {
            // Ignore empty data
            if (data[j]->getLength() == 0)
            {
                continue;
            }
            min = combinedMinimum(min, each.unit->toUnit(data[j]->getMinimum(*each.type)));
            max = combinedMaximum(max, each.unit->toUnit(data[j]->getMaximum(*each.type)));
        }

        axis.addBound(min);
        axis.addBound(max);
    }
}

template <BranchDataType T, PlotDataBranch<T> B>
double PlotConfiguration<T, B>::combinedMinimum(double current, double value) noexcept
{
    if (std::isnan(value))
    {
        return current;
    }
    if (std::isnan(current))
    {
        return value;
    }
    return MathUtil::javaMin(current, value);
}

template <BranchDataType T, PlotDataBranch<T> B>
double PlotConfiguration<T, B>::combinedMaximum(double current, double value) noexcept
{
    if (std::isnan(value))
    {
        return current;
    }
    if (std::isnan(current))
    {
        return value;
    }
    return MathUtil::javaMax(current, value);
}

template <BranchDataType T, PlotDataBranch<T> B>
double PlotConfiguration<T, B>::getGoodnessValue(const B& data)
{
    double goodness = 0;

    // Fit the axes ranges to the data
    fitAxes(data);

    /*
     * Calculate goodness of ranges.  100 points is given if the values fill the
     * entire range, 0 if they fill none of it.
     */
    for (const Row& each : m_rows)
    {
        if (each.axis < 0)
        {
            bug("getGoodnessValue called with auto-selected axis");
        }
        const PlotAxisBounds& axis = axisBounds(each.axis);

        const double min = each.unit->toUnit(data.getMinimum(*each.type));
        const double max = each.unit->toUnit(data.getMaximum(*each.type));
        if (std::isnan(min) || std::isnan(max))
        {
            continue;
        }
        if (MathUtil::equals(min, max))
        {
            continue;
        }

        double d = (max - min) / axis.getRangeLength();
        d        = MathUtil::safeSqrt(d);  // Prioritize small ranges
        goodness += d * 100.0;
    }

    /*
     * Add extra points for specific things.
     */

    // A little for the first type being on the first axis (no first type: no bonus, where Java
    // fails; see the class comment)
    if (!m_rows.empty() && m_rows.front().axis == 0)
    {
        goodness += kBonusFirstTypeOnFirstAxis;
    }

    // A little bonus if the first axis contains zero
    const PlotAxisBounds& left = m_allAxes[0];
    if (left.getMinValue() <= 0 && left.getMaxValue() >= 0)
    {
        goodness += kBonusFirstAxisHasZero;
    }

    // A boost if a common zero was used in the ranging
    const PlotAxisBounds& right = m_allAxes[1];
    if (left.getMinValue() <= 0 && left.getMaxValue() >= 0 && right.getMinValue() <= 0 &&
        right.getMaxValue() >= 0)
    {
        goodness += kBonusCommonZero;
    }

    // A boost if only one axis is used
    if (std::isnan(left.getMinValue()) || std::isnan(right.getMinValue()))
    {
        goodness += kBonusOnlyOneAxis;
    }

    return goodness;
}

template <BranchDataType T, PlotDataBranch<T> B>
PlotConfiguration<T, B>& PlotConfiguration<T, B>::resetUnits()
{
    for (Row& each : m_rows)
    {
        each.unit = &each.type->getUnitGroup().getDefaultUnit();
    }
    return *this;
}

template <BranchDataType T, PlotDataBranch<T> B>
void PlotConfiguration<T, B>::setName(std::string name, std::string nameKey)
{
    m_name    = std::move(name);
    m_nameKey = std::move(nameKey);
}

template <BranchDataType T, PlotDataBranch<T> B>
std::unique_ptr<PlotConfiguration<T, B>> PlotConfiguration<T, B>::cloneConfiguration() const
{
    return std::make_unique<PlotConfiguration>(*this);
}

template <BranchDataType T, PlotDataBranch<T> B>
PlotAxisBounds& PlotConfiguration<T, B>::axisBounds(int axis)
{
    if (axis < 0 || axis >= kAxesCount)
    {
        bug(std::format("No axis {}", axis));
    }
    return m_allAxes.at(static_cast<std::size_t>(axis));
}

template <BranchDataType T, PlotDataBranch<T> B>
void PlotConfiguration<T, B>::checkRow(std::size_t index) const
{
    if (index >= m_rows.size())
    {
        bug(std::format("No row {} (the configuration has {})", index, m_rows.size()));
    }
}

template <BranchDataType T, PlotDataBranch<T> B>
void PlotConfiguration<T, B>::checkAxis(int axis)
{
    if (axis >= kAxesCount)
    {
        bug("Axis index too large");
    }
}

}  // namespace QtRocket
