#include "QtRocket/file/motor/AbstractMotorLoader.h"

#include <algorithm>
#include <cstddef>
#include <expected>
#include <format>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "QtRocket/motor/ThrustCurveMotor.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"
#include "QtRocket/util/MathUtil.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket
{

namespace
{

/// The characters of Java's regex class \s: space, \t, \n, \x0B, \f and \r.
constexpr std::string_view kJavaWhitespace = " \t\n\x0B\f\r";

/// The failure of Java's ArrayList.get(index) on a list of @p length elements.
[[nodiscard]] std::unexpected<Error> outOfBounds(std::ptrdiff_t index, std::size_t length)
{
    return fail(ErrorCode::PARSE,
                std::format("Index {} out of bounds for length {}", index, length));
}

/// Removes the element at @p index from @p list (List.remove(int)).
void removeAt(std::vector<double>& list, std::size_t index)
{
    QTROCKET_ASSERT(index < list.size());
    list.erase(std::next(list.begin(), static_cast<std::ptrdiff_t>(index)));
}

/// Removes the element at @p index from @p time, @p thrust and every list of @p lists.
void removeEverywhere(std::vector<double>& time, std::vector<double>& thrust,
                      std::initializer_list<std::reference_wrapper<std::vector<double>>> lists,
                      std::size_t                                                        index)
{
    removeAt(time, index);
    removeAt(thrust, index);
    for (const std::reference_wrapper<std::vector<double>> list : lists)
    {
        removeAt(list.get(), index);
    }
}

}  // namespace

Result<std::vector<ThrustCurveMotor::Builder>> AbstractMotorLoader::load(
    std::span<const std::byte> data, std::string_view filename) const
{
    return loadText(decode(data, getDefaultCharset()), filename);
}

std::string AbstractMotorLoader::removeDelay(std::string_view designation)
{
    return ThrustCurveMotor::removeDelay(designation);
}

std::string AbstractMotorLoader::decode(std::span<const std::byte> bytes, Charset charset)
{
    const std::string raw = bytesToString(bytes);
    switch (charset)
    {
        case Charset::ISO_8859_1:
            return Strings::latin1ToUtf8(raw);
        case Charset::UTF_8:
            return Strings::toValidUtf8(raw);
    }
    QTROCKET_UNREACHABLE();
}

std::vector<double> AbstractMotorLoader::calculateMass(std::span<const double> time,
                                                       std::span<const double> thrust, double total,
                                                       double prop)
{
    QTROCKET_ASSERT(!time.empty() && thrust.size() >= time.size());
    std::vector<double> mass;
    std::vector<double> deltam;
    mass.reserve(time.size());
    deltam.reserve(time.size());

    // First calculate mass change between points
    double t0              = time[0];
    double f0              = thrust[0];
    double totalMassChange = 0;
    for (std::size_t i = 1; i < time.size(); i++)
    {
        const double t1 = time[i];
        const double f1 = thrust[i];

        const double dm = 0.5 * (f0 + f1) * (t1 - t0);

        deltam.push_back(dm);
        totalMassChange += dm;
        t0 = t1;
        f0 = f1;
    }

    // Scale mass change and calculate mass
    mass.push_back(total);
    const double scale = prop / totalMassChange;
    for (const double dm : deltam)
    {
        total -= dm * scale;
        // to correct negative mass error condition: (caused by rounding errors in the above loop).
        // std::max keeps a NaN total, as OpenRocket's "if (total < 0)" does.
        total = std::max(total, 0.0);
        mass.push_back(total);
    }
    return mass;
}

std::vector<std::string> AbstractMotorLoader::split(std::string_view str)
{
    return split(str, kJavaWhitespace);
}

std::vector<std::string> AbstractMotorLoader::split(std::string_view str,
                                                    std::string_view delimiters)
{
    std::vector<std::string> pieces;
    std::size_t              position = 0;
    while (position < str.size())
    {
        const std::size_t start = str.find_first_not_of(delimiters, position);
        if (start == std::string_view::npos)
        {
            break;
        }
        std::size_t end = str.find_first_of(delimiters, start);
        if (end == std::string_view::npos)
        {
            end = str.size();
        }
        pieces.emplace_back(str.substr(start, end - start));
        position = end;
    }
    return pieces;
}

void AbstractMotorLoader::sortLists(
    std::vector<double>&                                               primary,
    std::initializer_list<std::reference_wrapper<std::vector<double>>> lists)
{
    for (const std::reference_wrapper<std::vector<double>> list : lists)
    {
        QTROCKET_ASSERT(list.get().size() >= primary.size());
    }
    // OpenRocket swaps the first adjacent pair out of order and starts over until none is left.
    // Everything before that pair is in order, so the swapped element keeps moving left while it is
    // smaller than its neighbour, and the scan then resumes where it was: an insertion sort making
    // the same swaps in the same order.
    for (std::size_t next = 1; next < primary.size(); next++)
    {
        for (std::size_t index = next; index > 0 && primary[index] < primary[index - 1]; index--)
        {
            std::swap(primary[index], primary[index - 1]);
            for (const std::reference_wrapper<std::vector<double>> list : lists)
            {
                std::swap(list.get()[index], list.get()[index - 1]);
            }
        }
    }
}

Result<void> AbstractMotorLoader::finalizeThrustCurve(
    std::vector<double>& time, std::vector<double>& thrust,
    std::initializer_list<std::reference_wrapper<std::vector<double>>> lists)
{
    if (time.empty())
    {
        return {};
    }
    QTROCKET_ASSERT(thrust.size() >= time.size());
    for (const std::reference_wrapper<std::vector<double>> list : lists)
    {
        QTROCKET_ASSERT(list.get().size() >= time.size());
    }

    // Start
    // If there is no datapoint at t=0, put one there (this is the normal case for a RASP file). If
    // there is a nonzero thrust at time 0 it's an error, but not one that calls for not using the
    // file. We *don't* want to also put a 0-thrust point at time 0 in that case, as that will
    // cause the simulation to throw an exception.
    if (!MathUtil::equals(time[0], 0))
    {
        time.insert(time.begin(), 0.0);
        thrust.insert(thrust.begin(), 0.0);
        for (const std::reference_wrapper<std::vector<double>> list : lists)
        {
            const double first = list.get().front();
            list.get().insert(list.get().begin(), first);
        }
    }

    // Not-uncommon issue at start of thrust curves: two points for t=0, one with thrust zero and
    // one non-zero. We'll throw out the 0-thrust point and go on. (OpenRocket removes it from the
    // time and thrust lists only.)
    if (time.size() < 2)
    {
        return outOfBounds(1, time.size());
    }
    if (MathUtil::equals(time[0], 0) && MathUtil::equals(time[1], 0))
    {
        removeAt(time, 0);
        removeAt(thrust, 0);
    }

    // Very rare but not unheard of issue: two data points with identical time and thrust (see KBA
    // K1750). We'll throw out the first of them, and hope the data in any other lists passed in is
    // also duplicated (it *can't* make a big difference in the simulations).
    for (std::size_t i = 0; i + 1 < time.size(); i++)
    {
        while (i + 1 < time.size() && MathUtil::equals(time[i], time[i + 1]) &&
               MathUtil::equals(thrust[i], thrust[i + 1]))
        {
            removeEverywhere(time, thrust, lists, i);
        }
    }

    // Occasional issue: two final data points at the same time, one zero and one not. We'll throw
    // the 0 point out.
    const std::size_t n = time.size() - 1;
    if (n == 0)
    {
        return outOfBounds(-1, time.size());
    }
    if (MathUtil::equals(time[n - 1], time[n]))
    {
        if (MathUtil::equals(thrust[n - 1], 0))
        {
            removeEverywhere(time, thrust, lists, n - 1);
        }
        else if (MathUtil::equals(thrust[n], 0))
        {
            removeEverywhere(time, thrust, lists, n);
        }
    }

    // End: OpenRocket deliberately does not append a zero-thrust point at the last time.
    return {};
}

std::vector<std::string_view> AbstractMotorLoader::readLines(std::string_view text)
{
    std::vector<std::string_view> lines;
    std::size_t                   position = 0;
    while (position < text.size())
    {
        const std::size_t end = text.find_first_of("\r\n", position);
        if (end == std::string_view::npos)
        {
            lines.push_back(text.substr(position));
            break;
        }
        lines.push_back(text.substr(position, end - position));
        position = end + 1;
        if (text[end] == '\r' && position < text.size() && text[position] == '\n')
        {
            position++;
        }
    }
    return lines;
}

}  // namespace QtRocket
