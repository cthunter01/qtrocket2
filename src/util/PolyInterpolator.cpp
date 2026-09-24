#include "QtRocket/util/PolyInterpolator.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <format>
#include <initializer_list>
#include <ranges>
#include <span>
#include <utility>
#include <vector>

#include "QtRocket/util/BugError.h"

namespace QtRocket
{

namespace
{

using Matrix = std::vector<std::vector<double>>;

/// Gaussian elimination with scaled partial pivoting. On return @p a holds the upper triangle
/// with the pivoting ratios stored below the diagonal, and @p index the row order.
void gaussian(Matrix& a, std::vector<std::size_t>& index)
{
    const std::size_t   n = index.size();
    std::vector<double> c(n, 0.0);

    // Initialize the index
    for (std::size_t i = 0; i < n; ++i)
    {
        index[i] = i;
    }

    // Find the rescaling factors, one from each row
    for (std::size_t i = 0; i < n; ++i)
    {
        double c1 = 0;
        for (std::size_t j = 0; j < n; ++j)
        {
            c1 = std::max(c1, std::abs(a[i][j]));
        }
        c[i] = c1;
    }

    // Search the pivoting element from each column. As in OpenRocket, k keeps its previous value
    // when no element of the column beats zero.
    std::size_t k = 0;
    for (std::size_t j = 0; j + 1 < n; ++j)
    {
        double pi1 = 0;
        for (std::size_t i = j; i < n; ++i)
        {
            double pi0 = std::abs(a[index[i]][j]);
            pi0 /= c[index[i]];
            if (pi0 > pi1)
            {
                pi1 = pi0;
                k   = i;
            }
        }

        // Interchange rows according to the pivoting order
        std::swap(index[j], index[k]);
        for (std::size_t i = j + 1; i < n; ++i)
        {
            const double pj = a[index[i]][j] / a[index[j]][j];

            // Record pivoting ratios below the diagonal
            a[index[i]][j] = pj;

            // Modify other elements accordingly
            for (std::size_t l = j + 1; l < n; ++l)
            {
                a[index[i]][l] -= pj * a[index[j]][l];
            }
        }
    }
}

/// The inverse of @p matrix, which is used as scratch space.
Matrix inverse(Matrix matrix)
{
    const std::size_t        n = matrix.size();
    Matrix                   x(n, std::vector<double>(n, 0.0));
    Matrix                   b(n, std::vector<double>(n, 0.0));
    std::vector<std::size_t> index(n);
    for (std::size_t i = 0; i < n; ++i)
    {
        b[i][i] = 1;
    }

    // Transform the matrix into an upper triangle
    gaussian(matrix, index);

    // Update the matrix b[i][j] with the ratios stored
    for (std::size_t i = 0; i + 1 < n; ++i)
    {
        for (std::size_t j = i + 1; j < n; ++j)
        {
            for (std::size_t k = 0; k < n; ++k)
            {
                b[index[j]][k] -= matrix[index[j]][i] * b[index[i]][k];
            }
        }
    }

    // Perform backward substitutions
    for (std::size_t i = 0; i < n; ++i)
    {
        x[n - 1][i] = b[index[n - 1]][i] / matrix[index[n - 1]][n - 1];
        for (std::size_t j = n - 1; j-- > 0;)
        {
            x[j][i] = b[index[j]][i];
            for (std::size_t k = j + 1; k < n; ++k)
            {
                x[j][i] -= matrix[index[j]][k] * x[k][i];
            }
            x[j][i] /= matrix[index[j]][j];
        }
    }
    return x;
}

}  // namespace

PolyInterpolator::PolyInterpolator(std::span<const std::vector<double>> points)
{
    std::size_t count = 0;
    for (const auto& constraintXs : points)
    {
        count += constraintXs.size();
    }
    if (count == 0)
    {
        bug("No interpolation points defined.");
    }
    m_count = count;

    // mul[col] is the factor the j'th derivative puts on the column's power of x, the falling
    // factorial (n - 1 - col)! / (n - 1 - col - j)!. OpenRocket keeps it in an int; a double holds
    // the same exact integers without the risk of signed overflow.
    std::vector<double> mul(count, 1.0);

    Matrix      matrix(count, std::vector<double>(count, 0.0));
    std::size_t row = 0;
    std::size_t j   = 0;  // the derivative order of the constraints in constraintXs
    for (const auto& constraintXs : points)
    {
        for (const double point : constraintXs)
        {
            double x = 1;
            // Columns count - 1 - j down to 0; none when j >= count.
            for (std::size_t col = count > j ? count - j : 0; col-- > 0;)
            {
                matrix[row][col] = x * mul[col];
                x *= point;
            }
            ++row;
        }

        for (std::size_t i = 0; i < count; ++i)
        {
            mul[i] *=
                static_cast<double>(count) - static_cast<double>(i) - static_cast<double>(j) - 1;
        }
        ++j;
    }

    m_interpolationMatrix = inverse(std::move(matrix));
}

PolyInterpolator::PolyInterpolator(std::initializer_list<std::vector<double>> points)
  : PolyInterpolator(std::span<const std::vector<double>>{points.begin(), points.size()})
{
}

std::vector<double> PolyInterpolator::interpolator(std::span<const double> values) const
{
    if (values.size() != m_count)
    {
        bug(std::format("Wrong number of arguments {} expected {}", values.size(), m_count));
    }

    std::vector<double> ret(m_count, 0.0);
    for (std::size_t j = 0; j < m_count; ++j)
    {
        for (std::size_t i = 0; i < m_count; ++i)
        {
            ret[j] += m_interpolationMatrix[j][i] * values[i];
        }
    }
    return ret;
}

std::vector<double> PolyInterpolator::interpolator(std::initializer_list<double> values) const
{
    return interpolator(std::span<const double>{values.begin(), values.size()});
}

double PolyInterpolator::interpolate(double x, std::span<const double> values) const
{
    return eval(x, interpolator(values));
}

double PolyInterpolator::interpolate(double x, std::initializer_list<double> values) const
{
    return interpolate(x, std::span<const double>{values.begin(), values.size()});
}

double PolyInterpolator::eval(double x, std::span<const double> coefficients) noexcept
{
    double v      = 1;
    double result = 0;
    for (const double coefficient : std::views::reverse(coefficients))
    {
        result += coefficient * v;
        v *= x;
    }
    return result;
}

}  // namespace QtRocket
