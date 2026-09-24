#pragma once

#include <cstddef>
#include <initializer_list>
#include <span>
#include <vector>

namespace QtRocket
{

/// Polynomial interpolation through function value and derivative constraints, as OpenRocket's
/// PolyInterpolator (the nose cone pressure drag interpolation depends on it).
///
/// 1. Construct with the x coordinates of the constraints: the first list holds the x of the
///    function value constraints, the second those of the first derivative constraints, the third
///    those of the second derivatives, and so on. The number of coefficients is the total number
///    of constraints. Construction is O(n^3) (it inverts an n x n matrix).
/// 2. interpolator() turns the constraint values, in the same order, into the coefficients of
///    the polynomial that meets them, in O(n^2).
/// 3. eval() evaluates such a polynomial at a point in O(n).
class PolyInterpolator
{
public:
    /// Throws std::invalid_argument when there is no constraint at all
    /// (Java: IllegalArgumentException).
    explicit PolyInterpolator(std::span<const std::vector<double>> points);
    PolyInterpolator(std::initializer_list<std::vector<double>> points);

    /// The number of constraints, which is the number of polynomial coefficients.
    [[nodiscard]] std::size_t count() const noexcept { return m_count; }

    /// The coefficients of the polynomial that meets the constraints with these values (function
    /// values first, then first derivatives, ...), highest order term first and the constant
    /// last. Throws std::invalid_argument when there are not count() values.
    [[nodiscard]] std::vector<double> interpolator(std::span<const double> values) const;
    [[nodiscard]] std::vector<double> interpolator(std::initializer_list<double> values) const;

    /// The polynomial interpolator(values) evaluated at @p x.
    [[nodiscard]] double interpolate(double x, std::span<const double> values) const;
    [[nodiscard]] double interpolate(double x, std::initializer_list<double> values) const;

    /// Evaluates a polynomial from its coefficients, highest order term first and the constant
    /// last. No coefficients evaluate to 0.
    [[nodiscard]] static double eval(double x, std::span<const double> coefficients) noexcept;

private:
    std::size_t                      m_count{0};
    std::vector<std::vector<double>> m_interpolationMatrix;  ///< the inverse constraint matrix
};

}  // namespace QtRocket
