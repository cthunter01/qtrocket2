#include "QtRocket/motor/chooser/ThrustCurveMotorSelection.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <deque>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "QtRocket/motor/Manufacturer.h"
#include "QtRocket/motor/Motor.h"
#include "QtRocket/motor/MotorPreferences.h"
#include "QtRocket/motor/ThrustCurveMotor.h"
#include "QtRocket/motor/ThrustCurveMotorSet.h"
#include "QtRocket/motor/ThrustCurveMotorSetDatabase.h"
#include "QtRocket/motor/chooser/MotorHolder.h"
#include "QtRocket/motor/chooser/ThrustCurveMotorComparator.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/MathUtil.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket
{

namespace
{

/// The sets of @p database, in its order.
[[nodiscard]] std::vector<const ThrustCurveMotorSet*> setsOf(
    const ThrustCurveMotorSetDatabase& database)
{
    std::vector<const ThrustCurveMotorSet*> sets;
    sets.reserve(database.getMotorSets().size());
    for (const ThrustCurveMotorSet& set : database.getMotorSets())
    {
        sets.push_back(&set);
    }
    return sets;
}

/// Every manufacturer of @p sets once, sorted by simple name (MotorFilterPanel.java:149-157).
[[nodiscard]] std::vector<const Manufacturer*> manufacturersOf(
    const std::vector<const ThrustCurveMotorSet*>& sets)
{
    std::vector<const Manufacturer*> manufacturers;
    for (const ThrustCurveMotorSet* set : sets)
    {
        const Manufacturer* manufacturer = set->getManufacturer();
        if (manufacturer != nullptr &&
            std::ranges::find(manufacturers, manufacturer) == manufacturers.end())
        {
            manufacturers.push_back(manufacturer);
        }
    }
    std::ranges::stable_sort(manufacturers, [](const Manufacturer* a, const Manufacturer* b) {
        return Strings::javaCompareTo(a->getSimpleName(), b->getSimpleName()) < 0;
    });
    return manufacturers;
}

/// Whether @p set holds @p motor (that very object).
[[nodiscard]] bool holds(const ThrustCurveMotorSet& set, const ThrustCurveMotor& motor)
{
    return std::ranges::any_of(set.getMotors(),
                               [&motor](const std::shared_ptr<const ThrustCurveMotor>& candidate) {
                                   return candidate.get() == &motor;
                               });
}

}  // namespace

ThrustCurveMotorSelection::ThrustCurveMotorSelection(const ThrustCurveMotorSetDatabase& database,
                                                     Preferences&                       preferences)
  : m_preferences(preferences),
    m_model(setsOf(database)),
    m_manufacturers(manufacturersOf(setsOf(database))),
    m_hideSimilar(MotorPreferences::isHideSimilar(preferences))
{
    const std::vector<const Manufacturer*> excluded =
        MotorPreferences::getExcludedMotorManufacturers(preferences);
    m_rowFilter.setExcludedManufacturers(excluded);
    m_rowFilter.setHideUnavailable(MotorPreferences::isHideUnavailable(preferences));
}

ThrustCurveMotorSelection::~ThrustCurveMotorSelection() = default;

std::vector<std::size_t> ThrustCurveMotorSelection::getViewRows() const
{
    std::vector<std::size_t> rows;
    for (std::size_t row = 0; row < m_model.getRowCount(); row++)
    {
        if (m_rowFilter.include(m_model.getMotorSet(row), m_preferences))
        {
            rows.push_back(row);
        }
    }
    return m_sorter.sort(rows, m_model, m_preferences);
}

void ThrustCurveMotorSelection::setMotorMountAndConfig(const MountConfiguration& configuration)
{
    // motorFilterPanel.setMotorMount(mount), which OpenRocket calls again at the end: the second
    // call changes nothing now that the used motors are replaced (MotorRowFilter).
    m_rowFilter.setUsedMotors(configuration.mountMotors);

    m_selectedMotor                                              = nullptr;
    m_selectedMotorSet                                           = nullptr;
    m_selectedDelay                                              = 0;
    m_maximumNozzleExitDiameter                                  = 0;
    m_nozzleExitDiameter                                         = 0;
    const std::shared_ptr<const ThrustCurveMotor>& motorToSelect = configuration.motor;
    if (motorToSelect == nullptr)
    {
        return;
    }
    m_selectedDelay = configuration.ejectionDelay;

    // If current motor is not found in db, add a new ThrustCurveMotorSet containing it
    if (findMotorSet(*motorToSelect) == nullptr)
    {
        addExtraSet(motorToSelect);
    }
    select(motorToSelect);
    // M3's bug 8: OpenRocket's select() has just replaced the configuration's delay with the
    // closest standard one; QtRocket keeps the configured delay (a NaN one excepted, which has no
    // closest delay but the last).
    if (!std::isnan(configuration.ejectionDelay))
    {
        m_selectedDelay = configuration.ejectionDelay;
    }
    setNozzleExitDiameter(configuration.nozzleExitDiameter);
}

void ThrustCurveMotorSelection::select(const std::shared_ptr<const ThrustCurveMotor>& motor)
{
    if (motor == nullptr || motor == m_selectedMotor)
    {
        return;
    }
    const ThrustCurveMotorSet* set = findMotorSet(*motor);
    if (set == nullptr)
    {
        bug("Could not find motor from database, motor=" + motor->toString());
    }
    const bool updateDelays = set != m_selectedMotorSet;

    m_selectedMotor             = motor;
    m_selectedMotorSet          = set;
    m_maximumNozzleExitDiameter = motor->getDiameter();
    setNozzleExitDiameter(0);
    if (updateDelays)
    {
        resetDelays();
    }
}

void ThrustCurveMotorSelection::selectMotorFromTable(std::size_t modelRow)
{
    const ThrustCurveMotorSet& set = m_model.getMotorSet(modelRow);
    if (&set != m_selectedMotorSet)
    {
        select(selectMotor(set));
    }
}

std::shared_ptr<const ThrustCurveMotor> ThrustCurveMotorSelection::selectMotor(
    const ThrustCurveMotorSet& set) const
{
    if (set.getMotorCount() == 0)
    {
        bug("Attempting to select motor from empty ThrustCurveMotorSet: " + set.toString());
    }
    if (set.getMotorCount() == 1)
    {
        return set.getMotors().front();
    }
    // Find which motor has been used the most recently
    if (std::shared_ptr<const ThrustCurveMotor> preferred =
            MotorPreferences::findPreferredThrustCurve(m_preferences, set))
    {
        return preferred;
    }
    // No motor has been used: the first of the best (Java sorts a copy of the list, stably, and
    // takes its first).
    const std::shared_ptr<const ThrustCurveMotor>* best = nullptr;
    for (const std::shared_ptr<const ThrustCurveMotor>& motor : set.getMotors())
    {
        if (best == nullptr || ThrustCurveMotorComparator::compare(*motor, **best) < 0)
        {
            best = &motor;
        }
    }
    QTROCKET_ASSERT(best != nullptr);
    return *best;
}

const ThrustCurveMotorSet* ThrustCurveMotorSelection::findMotorSet(
    const ThrustCurveMotor& motor) const
{
    for (const ThrustCurveMotorSet* set : m_model.getMotorSets())
    {
        if (holds(*set, motor))
        {
            return set;
        }
    }
    return nullptr;
}

void ThrustCurveMotorSelection::storeSelectedMotor(const ThrustCurveMotor& motor)
{
    const ThrustCurveMotorSet* set = findMotorSet(motor);
    if (set == nullptr)
    {
        return;
    }
    MotorPreferences::storePreferredThrustCurve(m_preferences, *set, motor);
}

std::vector<std::shared_ptr<const ThrustCurveMotor>> ThrustCurveMotorSelection::getFilteredCurves()
{
    if (m_selectedMotorSet == nullptr)
    {
        return {};
    }
    std::vector<std::shared_ptr<const ThrustCurveMotor>> motors;
    for (const std::shared_ptr<const ThrustCurveMotor>& motor : m_selectedMotorSet->getMotors())
    {
        if (!m_hideSimilar || m_selectedMotor == nullptr || motor == m_selectedMotor ||
            m_similarities.similarity(m_selectedMotor, motor) < kMotorSimilarityThreshold)
        {
            motors.push_back(motor);
        }
    }
    std::ranges::stable_sort(motors, ThrustCurveMotorComparator{});
    return motors;
}

std::vector<MotorHolder> ThrustCurveMotorSelection::getCurveItems()
{
    std::vector<MotorHolder>                                   items;
    const std::vector<std::shared_ptr<const ThrustCurveMotor>> curves = getFilteredCurves();
    items.reserve(curves.size());
    for (std::size_t index = 0; index < curves.size(); index++)
    {
        items.emplace_back(curves[index], index);
    }
    return items;
}

void ThrustCurveMotorSelection::setHideSimilar(bool hideSimilar)
{
    m_hideSimilar = hideSimilar;
    MotorPreferences::setHideSimilar(m_preferences, hideSimilar);
}

void ThrustCurveMotorSelection::setHideUnavailable(bool hideUnavailable)
{
    MotorPreferences::setHideUnavailable(m_preferences, hideUnavailable);
    m_rowFilter.setHideUnavailable(hideUnavailable);
}

void ThrustCurveMotorSelection::setDelayTexts(std::string plugged, std::string pluggedNone)
{
    m_pluggedText     = std::move(plugged);
    m_pluggedNoneText = std::move(pluggedNone);
}

std::vector<std::string> ThrustCurveMotorSelection::getDelayItems() const
{
    if (m_selectedMotor == nullptr)
    {
        //// Display nothing
        return {};
    }
    const std::vector<double>& delays = m_selectedMotorSet->getDelays();
    std::vector<std::string>   items;
    items.reserve(delays.size() + 1);
    for (const double delay : delays)
    {
        items.push_back(ThrustCurveMotor::getDelayString(delay, m_pluggedText));
    }
    // We always want the plugged option in the combobox, even if the motor doesn't have it
    if (std::ranges::find(delays, Motor::kPluggedDelay) == delays.end())
    {
        items.push_back(m_pluggedText);
    }
    return items;
}

std::string ThrustCurveMotorSelection::getDelayText() const
{
    if (m_selectedMotor == nullptr)
    {
        return {};
    }
    return ThrustCurveMotor::getDelayString(m_selectedDelay, m_pluggedText);
}

void ThrustCurveMotorSelection::setDelayText(std::string_view text)
{
    m_selectedDelay = parseDelay(text, m_selectedDelay);
}

void ThrustCurveMotorSelection::setNozzleExitDiameter(double diameter) noexcept
{
    m_nozzleExitDiameter = MathUtil::clamp(diameter, 0, m_maximumNozzleExitDiameter);
}

void ThrustCurveMotorSelection::resetDelays()
{
    // Find and set the closest value: the condition is true for every delay while closest is NaN,
    // so the first delay replaces it, and a tie goes to the later delay.
    const double current = m_selectedDelay;
    double       closest = std::numeric_limits<double>::quiet_NaN();
    for (const double delay : m_selectedMotorSet->getDelays())
    {
        if (!(std::abs(delay - current) > std::abs(closest - current)))
        {
            closest = delay;
        }
    }
    // OpenRocket selects the closest delay's text in its combo box (or "Plugged" when there is
    // none), and the box fires its action listener, which reads that text back: the delay is the
    // text's (ThrustCurveMotor::getDelayString() rounds to tenths; "Plugged" is plugged).
    const std::string shown = std::isnan(closest)
                                  ? m_pluggedText
                                  : ThrustCurveMotor::getDelayString(closest, m_pluggedText);
    m_selectedDelay         = parseDelay(shown, closest);
}

double ThrustCurveMotorSelection::parseDelay(std::string_view text, double current) const
{
    //// Plugged (or None)
    if (Strings::javaEqualsIgnoreCase(text, m_pluggedText) ||
        Strings::javaEqualsIgnoreCase(text, m_pluggedNoneText))
    {
        return Motor::kPluggedDelay;
    }
    const std::optional<double> parsed = Strings::javaParseDouble(text);
    // M3's bug 11: OpenRocket takes NaN as a delay; QtRocket keeps the delay.
    if (!parsed.has_value() || std::isnan(*parsed))
    {
        return current;
    }
    return *parsed;
}

void ThrustCurveMotorSelection::addExtraSet(const std::shared_ptr<const ThrustCurveMotor>& motor)
{
    auto extra = std::make_unique<ThrustCurveMotorSet>();
    extra->addMotor(motor);
    std::vector<const ThrustCurveMotorSet*> database(m_model.getMotorSets().begin(),
                                                     m_model.getMotorSets().end());
    database.push_back(extra.get());
    m_extraSets.push_back(std::move(extra));
    // Collections.sort(database): stable, by ThrustCurveMotorSet.compareTo.
    std::ranges::stable_sort(database,
                             [](const ThrustCurveMotorSet* a, const ThrustCurveMotorSet* b) {
                                 return a->compareTo(*b) < 0;
                             });
    m_model.setDatabase(std::move(database));
}

}  // namespace QtRocket
