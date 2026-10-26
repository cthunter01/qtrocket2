#pragma once

#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "QtRocket/motor/chooser/ImpulseClass.h"

namespace QtRocket
{

class Manufacturer;
class Preferences;
class ThrustCurveMotor;
class ThrustCurveMotorSet;

/// Which motor sets the motor chooser's table shows (OpenRocket's swing MotorRowFilter,
/// gui/dialogs/motor/thrustcurve/MotorRowFilter.java, a Swing RowFilter; here without Qt): a set
/// is included when it passes every test, in this order (include()):
///
/// 1. its manufacturer is not one of the excluded manufacturers (by identity);
/// 2. when "Hide motors already used in the mount" is on, it matches none of the mount's motors
///    (ThrustCurveMotorSet::matches());
/// 3. its size: a diameter above the minimum diameter less 1.5 mm (kMinimumDiameterSlack) and
///    below the maximum diameter plus 0.4 mm (kMaximumDiameterSlack), each when set, and a length
///    neither above the maximum length (the largest double unless set) nor below the minimum one
///    (0 unless set);
/// 4. every search term is contained in the text of one of the table's columns
///    (ThrustCurveMotorColumns, so also in the total impulse, diameter and length as the table
///    shows them, in the user's units) or in the set's common name or designation (both, whatever
///    the name column shows), compared case-folded;
/// 5. its total impulse (the rounded ThrustCurveMotorSet::getTotalImpulse()) lies above the low
///    end of the minimum impulse class and not above the high end of the maximum one, each when
///    set;
/// 6. when "Hide motors which are not in regular production" is on, the set is available.
///
/// The filter panel and the search field of the chooser set the criteria; the selection panel
/// re-sorts the table after every change (ThrustCurveMotorSelection::getViewRows()).
///
/// Deviations from OpenRocket:
/// - setUsedMotors(), OpenRocket's setMotorMount(mount), replaces the motors that "Hide motors
///   already used in the mount" hides with those of the new mount (QtRocket fixes OpenRocket's bug,
///   M3's bug 9: OpenRocket adds them to the motors of every mount the chooser was opened for
///   before, and so hides those too). It takes the motors, not the mount: motor/ comes before
///   rocket/ in the core's layers (MotorChooserInput::fromMount() reads them off a mount).
/// - The search folds case with Strings::javaCaseFold(), Java's toLowerCase(Locale.getDefault())
///   with en_US's rules except for the few letters whose upper case lowers to another letter (the
///   micro sign U+00B5, the long s U+017F, the final sigma U+03C2, ...), U+0130 and a final
///   capital sigma: a search for U+03BC (mu) finds "U+00B5 m" (the micro sign) here and not in
///   OpenRocket. In a Turkish or Azerbaijani locale OpenRocket also folds "I" to a dotless i.
/// - OpenRocket fires a change event when setMinimumLength() or setMaximumLength() changes the
///   value (its count label listens); here the caller updates what depends on the filter after
///   each change.
class MotorRowFilter
{
public:
    /// The slack filterBySize() gives the minimum diameter: a set whose diameter is at most the
    /// minimum less this is left out.
    static constexpr double kMinimumDiameterSlack = 0.0015;
    /// The slack it gives the maximum diameter: a set whose diameter is at least the maximum plus
    /// this is left out.
    static constexpr double kMaximumDiameterSlack = 0.0004;

    /// setMotorMount(): the motors "Hide motors already used in the mount" hides become
    /// @p usedMotors, the motors of every motor configuration of the mount the chooser is opened
    /// for, the default one and the overrides, the configuration being edited included
    /// (MotorChooserInput::fromMount()); none for no mount.
    /// @throws BugError when a motor is null.
    void setUsedMotors(std::vector<std::shared_ptr<const ThrustCurveMotor>> usedMotors);

    /// The motors setUsedMotors() keeps.
    [[nodiscard]] std::span<const std::shared_ptr<const ThrustCurveMotor>> getUsedMotors()
        const noexcept
    {
        return m_usedMotors;
    }

    /// setSearchTerms(): every term trimmed (String.trim, Strings::trim()) and case-folded
    /// (Strings::javaCaseFold()), the empty ones left out.
    void setSearchTerms(std::span<const std::string> searchTerms);

    /// The search field's text @p text as the chooser hands it to setSearchTerms():
    /// text.trim().split("\\s+"), so @p text trimmed and split at the runs of Java regex
    /// whitespace (space, tab, line feed, vertical tab, form feed, carriage return).
    void setSearchText(std::string_view text);

    /// The terms in use, trimmed and case-folded.
    [[nodiscard]] const std::vector<std::string>& getSearchTerms() const noexcept
    {
        return m_searchTerms;
    }

    /// The terms setSearchText(@p text) puts in use: @p text trimmed (characters at or below
    /// U+0020 removed at both ends), split at runs of Java regex whitespace, each part trimmed and
    /// case-folded, the empty ones left out. "  Estes\tC6 " gives {"estes", "c6"}.
    [[nodiscard]] static std::vector<std::string> searchTerms(std::string_view text);

    /// The minimum length in m (getMinimumLength()), 0 at first.
    [[nodiscard]] double getMinimumLength() const noexcept { return m_minimumLength; }
    void                 setMinimumLength(double minimumLength) noexcept;

    /// The maximum length in m (getMaximumLength()), the largest double at first.
    [[nodiscard]] double getMaximumLength() const noexcept { return m_maximumLength; }
    void                 setMaximumLength(double maximumLength) noexcept;

    /// The minimum diameter in m (getMinimumDiameter()), none at first.
    [[nodiscard]] std::optional<double> getMinimumDiameter() const noexcept
    {
        return m_minimumDiameter;
    }
    void setMinimumDiameter(std::optional<double> minimumDiameter) noexcept;

    /// The maximum diameter in m (getMaximumDiameter()), none at first.
    [[nodiscard]] std::optional<double> getMaximumDiameter() const noexcept
    {
        return m_maximumDiameter;
    }
    void setMaximumDiameter(std::optional<double> maximumDiameter) noexcept;

    /// "Hide motors already used in the mount" (setHideUsedMotors()), off at first.
    [[nodiscard]] bool isHideUsedMotors() const noexcept { return m_hideUsedMotors; }
    void               setHideUsedMotors(bool hideUsedMotors) noexcept;

    /// The excluded manufacturers (getExcludedManufacturers()), none at first, in the order set.
    [[nodiscard]] const std::vector<const Manufacturer*>& getExcludedManufacturers() const noexcept
    {
        return m_excludedManufacturers;
    }
    /// setExcludedManufacturers(): replaces the excluded manufacturers by @p excludedManufacturers.
    /// @throws BugError when one is null.
    void setExcludedManufacturers(std::span<const Manufacturer* const> excludedManufacturers);

    /// The minimum impulse class (getMinimumImpulse()), none at first.
    [[nodiscard]] std::optional<ImpulseClass> getMinimumImpulse() const noexcept
    {
        return m_minimumImpulse;
    }
    void setMinimumImpulse(std::optional<ImpulseClass> minimumImpulse) noexcept;

    /// The maximum impulse class (getMaximumImpulse()), none at first.
    [[nodiscard]] std::optional<ImpulseClass> getMaximumImpulse() const noexcept
    {
        return m_maximumImpulse;
    }
    void setMaximumImpulse(std::optional<ImpulseClass> maximumImpulse) noexcept;

    /// "Hide motors which are not in regular production" (isHideUnavailable()), off at first (the
    /// chooser sets it from the preference, MotorPreferences::isHideUnavailable()).
    [[nodiscard]] bool isHideUnavailable() const noexcept { return m_hideUnavailable; }
    void               setHideUnavailable(bool hideUnavailable) noexcept;

    /// include(): whether the table shows @p set, see the class comment. @p preferences chooses the
    /// name column's text (Preferences::getMotorNameColumn()).
    /// @throws BugError when @p set holds no motor.
    [[nodiscard]] bool include(const ThrustCurveMotorSet& set,
                               const Preferences&         preferences) const;

private:
    [[nodiscard]] bool filterManufacturers(const ThrustCurveMotorSet& set) const;
    [[nodiscard]] bool filterUsed(const ThrustCurveMotorSet& set) const;
    [[nodiscard]] bool filterBySize(const ThrustCurveMotorSet& set) const;
    [[nodiscard]] bool filterByString(const ThrustCurveMotorSet& set,
                                      const Preferences&         preferences) const;
    [[nodiscard]] bool filterByImpulseClass(const ThrustCurveMotorSet& set) const;
    [[nodiscard]] bool filterUnavailable(const ThrustCurveMotorSet& set) const;

    std::vector<std::shared_ptr<const ThrustCurveMotor>> m_usedMotors;
    double                                               m_minimumLength{0};
    double                           m_maximumLength{std::numeric_limits<double>::max()};
    std::optional<double>            m_minimumDiameter;
    std::optional<double>            m_maximumDiameter;
    std::vector<std::string>         m_searchTerms;
    bool                             m_hideUsedMotors{false};
    std::vector<const Manufacturer*> m_excludedManufacturers;
    std::optional<ImpulseClass>      m_minimumImpulse;
    std::optional<ImpulseClass>      m_maximumImpulse;
    bool                             m_hideUnavailable{false};
};

}  // namespace QtRocket
