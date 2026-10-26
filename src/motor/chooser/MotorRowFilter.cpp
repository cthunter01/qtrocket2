#include "QtRocket/motor/chooser/MotorRowFilter.h"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "QtRocket/motor/Manufacturer.h"
#include "QtRocket/motor/ThrustCurveMotor.h"
#include "QtRocket/motor/ThrustCurveMotorSet.h"
#include "QtRocket/motor/chooser/ImpulseClass.h"
#include "QtRocket/motor/chooser/ThrustCurveMotorColumns.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket
{

namespace
{

/// Whether @p c is whitespace to Java's regex \s (without UNICODE_CHARACTER_CLASS).
[[nodiscard]] bool isJavaWhitespace(char c) noexcept
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\x0B' || c == '\f' || c == '\r';
}

/// @p term as setSearchTerms() keeps it: trimmed and case-folded; empty when nothing is left.
[[nodiscard]] std::string searchTerm(std::string_view term)
{
    return Strings::javaCaseFold(Strings::trim(term));
}

}  // namespace

void MotorRowFilter::setUsedMotors(std::vector<std::shared_ptr<const ThrustCurveMotor>> usedMotors)
{
    // OpenRocket adds to the list and never clears it (M3's bug 9): here the list is the mount's.
    QTROCKET_ASSERT(std::ranges::none_of(
        usedMotors,
        [](const std::shared_ptr<const ThrustCurveMotor>& motor) { return motor == nullptr; }));
    m_usedMotors = std::move(usedMotors);
}

void MotorRowFilter::setSearchTerms(std::span<const std::string> searchTerms)
{
    m_searchTerms.clear();
    for (const std::string& term : searchTerms)
    {
        std::string kept = searchTerm(term);
        if (!kept.empty())
        {
            m_searchTerms.push_back(std::move(kept));
        }
    }
}

void MotorRowFilter::setSearchText(std::string_view text)
{
    m_searchTerms = searchTerms(text);
}

std::vector<std::string> MotorRowFilter::searchTerms(std::string_view text)
{
    // text.trim().split("\\s+"), then setSearchTerms(): String.trim removes every character at or
    // below U+0020, a superset of \s, so the trimmed text starts with no separator and split()
    // gives no leading empty part; empty parts are left out anyway.
    const std::string_view   trimmed = Strings::trim(text);
    std::vector<std::string> terms;
    std::size_t              start = 0;
    while (start < trimmed.size())
    {
        std::size_t end = start;
        while (end < trimmed.size() && !isJavaWhitespace(trimmed[end]))
        {
            end++;
        }
        std::string term = searchTerm(trimmed.substr(start, end - start));
        if (!term.empty())
        {
            terms.push_back(std::move(term));
        }
        start = end;
        while (start < trimmed.size() && isJavaWhitespace(trimmed[start]))
        {
            start++;
        }
    }
    return terms;
}

void MotorRowFilter::setMinimumLength(double minimumLength) noexcept
{
    m_minimumLength = minimumLength;
}

void MotorRowFilter::setMaximumLength(double maximumLength) noexcept
{
    m_maximumLength = maximumLength;
}

void MotorRowFilter::setMinimumDiameter(std::optional<double> minimumDiameter) noexcept
{
    m_minimumDiameter = minimumDiameter;
}

void MotorRowFilter::setMaximumDiameter(std::optional<double> maximumDiameter) noexcept
{
    m_maximumDiameter = maximumDiameter;
}

void MotorRowFilter::setHideUsedMotors(bool hideUsedMotors) noexcept
{
    m_hideUsedMotors = hideUsedMotors;
}

void MotorRowFilter::setExcludedManufacturers(
    std::span<const Manufacturer* const> excludedManufacturers)
{
    QTROCKET_ASSERT(std::ranges::none_of(
        excludedManufacturers,
        [](const Manufacturer* manufacturer) { return manufacturer == nullptr; }));
    m_excludedManufacturers.assign(excludedManufacturers.begin(), excludedManufacturers.end());
}

void MotorRowFilter::setMinimumImpulse(std::optional<ImpulseClass> minimumImpulse) noexcept
{
    m_minimumImpulse = minimumImpulse;
}

void MotorRowFilter::setMaximumImpulse(std::optional<ImpulseClass> maximumImpulse) noexcept
{
    m_maximumImpulse = maximumImpulse;
}

void MotorRowFilter::setHideUnavailable(bool hideUnavailable) noexcept
{
    m_hideUnavailable = hideUnavailable;
}

bool MotorRowFilter::include(const ThrustCurveMotorSet& set, const Preferences& preferences) const
{
    QTROCKET_ASSERT(set.getManufacturer() != nullptr);
    return filterManufacturers(set) && filterUsed(set) && filterBySize(set) &&
           filterByString(set, preferences) && filterByImpulseClass(set) && filterUnavailable(set);
}

bool MotorRowFilter::filterManufacturers(const ThrustCurveMotorSet& set) const
{
    return std::ranges::find(m_excludedManufacturers, set.getManufacturer()) ==
           m_excludedManufacturers.end();
}

bool MotorRowFilter::filterUsed(const ThrustCurveMotorSet& set) const
{
    if (!m_hideUsedMotors)
    {
        return true;
    }
    return std::ranges::none_of(m_usedMotors,
                                [&set](const std::shared_ptr<const ThrustCurveMotor>& motor) {
                                    return set.matches(*motor);
                                });
}

bool MotorRowFilter::filterBySize(const ThrustCurveMotorSet& set) const
{
    if (m_minimumDiameter.has_value() &&
        set.getDiameter() <= *m_minimumDiameter - kMinimumDiameterSlack)
    {
        return false;
    }
    if (m_maximumDiameter.has_value() &&
        set.getDiameter() >= *m_maximumDiameter + kMaximumDiameterSlack)
    {
        return false;
    }
    if (set.getLength() > m_maximumLength)
    {
        return false;
    }
    return !(set.getLength() < m_minimumLength);
}

bool MotorRowFilter::filterByString(const ThrustCurveMotorSet& set,
                                    const Preferences&         preferences) const
{
    if (m_searchTerms.empty())
    {
        return true;
    }
    // Each text folded once for all the terms.
    std::vector<std::string> texts;
    texts.reserve(kAllThrustCurveMotorColumns.size() + 2);
    for (const ThrustCurveMotorColumns column : kAllThrustCurveMotorColumns)
    {
        texts.push_back(Strings::javaCaseFold(getValueText(column, set, preferences)));
    }
    // Make sure that you can search on both the common name, and designation
    texts.push_back(Strings::javaCaseFold(set.getCommonName()));
    texts.push_back(Strings::javaCaseFold(set.getDesignation()));
    return std::ranges::all_of(m_searchTerms, [&texts](const std::string& term) {
        return std::ranges::any_of(
            texts, [&term](const std::string& text) { return text.contains(term); });
    });
}

bool MotorRowFilter::filterByImpulseClass(const ThrustCurveMotorSet& set) const
{
    const auto totalImpulse = static_cast<double>(set.getTotalImpulse());
    if (m_minimumImpulse.has_value() && totalImpulse <= getLow(*m_minimumImpulse))
    {
        return false;
    }
    return !m_maximumImpulse.has_value() || totalImpulse <= getHigh(*m_maximumImpulse);
}

bool MotorRowFilter::filterUnavailable(const ThrustCurveMotorSet& set) const
{
    return !m_hideUnavailable || set.isAvailable();
}

}  // namespace QtRocket
