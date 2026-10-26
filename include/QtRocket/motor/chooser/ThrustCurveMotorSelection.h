#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "QtRocket/motor/chooser/MotorHolder.h"
#include "QtRocket/motor/chooser/MotorRowFilter.h"
#include "QtRocket/motor/chooser/MotorSimilarityCache.h"
#include "QtRocket/motor/chooser/MotorTableSorter.h"
#include "QtRocket/motor/chooser/ThrustCurveMotorDatabaseModel.h"

namespace QtRocket
{

class Manufacturer;
class Preferences;
class ThrustCurveMotor;
class ThrustCurveMotorSet;
class ThrustCurveMotorSetDatabase;

/// What the motor chooser's selection panel decides, without the widgets (OpenRocket's swing
/// ThrustCurveMotorSelectionPanel, gui/dialogs/motor/thrustcurve/ThrustCurveMotorSelectionPanel
/// .java, its logic of lines 146-170, 221-241, 262-290, 353-363, 468-471 and 484-867): the table's
/// rows (the model, the filter and the sort), the selected motor set and thrust curve, the curves
/// the curve list offers, the ejection delay and its texts, and the nozzle exit diameter. The
/// chooser dialog of the GUI shows this state and passes the user's actions on to it; it lives as
/// long as the dialog, so that the search, the filters, the sort and the selection survive between
/// openings, as in OpenRocket.
///
/// The selection, step by step:
/// - setMotorMountAndConfig() opens the chooser for a mount's motor configuration (given as data,
///   MountConfiguration: motor/ comes before rocket/ in the core's layers, so the chooser cannot
///   read a mount; MotorChooserInput::fromMount() in document/ makes one): the mount's
///   motors become the "used" ones of the filter, and the configuration's motor (when it has one)
///   is selected with its delay and nozzle exit diameter. A motor that is in no set of the list
///   (one loaded with a design) gets a set of its own, added to the list, which is then sorted
///   by ThrustCurveMotorSet::compareTo() and stays so (also for the model order of the table).
/// - selectMotorFromTable() follows the table's selection: a row of another set selects that set's
///   default curve (selectMotor(): the only one, else the one last chosen in the set,
///   MotorPreferences::findPreferredThrustCurve(), else the best by ThrustCurveMotorComparator).
/// - select() follows the curve list. A curve of another set resets the delay to the set's
///   standard delay closest to the current one (ties going to the later delay; with no standard
///   delays at all, plugged), as the delay box shows it and reads it back (OpenRocket's combo box
///   fires its listener for the item it selects: a delay is its getDelayString() text read again,
///   which changes only one beyond the int range, shown as "2147483647"); a curve change resets the
///   nozzle exit diameter to 0, its maximum becoming the motor's diameter.
/// - setDelayText() follows the delay box: "Plugged" and "None" (the texts given to
///   setDelayTexts(), any case) mean plugged, other text is read as Java's Double.parseDouble
///   reads it (Strings::javaParseDouble()); text it cannot read keeps the delay. The box then shows
///   getDelayText(), which the caller must not read back in (OpenRocket's combo box suppresses that
///   event): "5.04" keeps 5.04 and shows "5".
/// - storeSelectedMotor() on OK remembers the chosen curve of its set (selectedMotor()).
///
/// "Hide very similar thrust curves" (setHideSimilar()) leaves out of getFilteredCurves() every
/// curve whose MotorCorrelation::similarity() with the selected one is not below
/// kMotorSimilarityThreshold; the similarities are kept per pair (MotorSimilarityCache).
///
/// The object refers to the motor database's sets, to sets of its own, and to the Preferences;
/// the database and the Preferences must outlive it. It keeps no motor mount or component, only
/// the motors (shared). Not copyable (the model refers to its own sets).
///
/// Deviations from OpenRocket:
/// - Reopening for a configuration keeps its delay (QtRocket fixes M3's bug 8: OpenRocket replaces
///   it with the set's closest standard delay, so a C6 configured with a 6 s delay opens with 7 s,
///   and OK writes 7). A NaN delay is not kept: it gives what OpenRocket gives (the last standard
///   delay, read back from its text).
/// - A typed "NaN" (or any text Java reads as NaN) keeps the delay (QtRocket fixes M3's bug 11:
///   OpenRocket takes NaN as the delay, shows "NaN" and writes it).
/// - "Hide motors already used in the mount" hides the motors of the current mount only (bug 9,
///   MotorRowFilter::setUsedMotors()).
/// - The constructor stores nothing: OpenRocket's runs the listeners of "Hide very similar thrust
///   curves" and "Hide motors which are not in regular production" once, which writes their
///   preferences with the values it read (SwingPreferences also writes a default it reads;
///   QtRocket's stores do not).
/// - The table's sort and filter run when the caller asks (getViewRows()); a sort that fails is
///   not caught (OpenRocket logs "Motor table sort failed": DesignationComparator's deviation means
///   it cannot fail here).
class ThrustCurveMotorSelection
{
public:
    /// MOTOR_SIMILARITY_THRESHOLD: with "Hide very similar thrust curves" on, a curve stays in the
    /// list when its similarity with the selected curve is below this.
    static constexpr double kMotorSimilarityThreshold = 0.95;

    /// What OpenRocket's setMotorMountAndConfig(fcid, mount) reads of the mount: the motor
    /// configuration of the mount in the flight configuration (its override, else the mount's
    /// default) and the motors of all the mount's configurations. MotorChooserInput::fromMount()
    /// (document/) reads them off a MotorMount.
    struct MountConfiguration
    {
        /// The motor of every motor configuration of the mount that has one, the default
        /// configuration first, then the overrides in their order (Java's getMotorIterator()):
        /// what "Hide motors already used in the mount" hides.
        std::vector<std::shared_ptr<const ThrustCurveMotor>> mountMotors;
        /// The configuration's motor, or null when it has none.
        std::shared_ptr<const ThrustCurveMotor> motor;
        /// The configuration's ejection delay in s.
        double ejectionDelay{0};
        /// The configuration's nozzle exit diameter in m.
        double nozzleExitDiameter{0};
    };

    /// The chooser over every set of @p database, in the database's order (the table's model
    /// order), in its first state: no motor selected, the delay 0, the filter with the excluded
    /// manufacturers (MotorPreferences::getExcludedMotorManufacturers()) and "Hide motors which
    /// are not in regular production" (MotorPreferences::isHideUnavailable()) of @p preferences,
    /// "Hide very similar thrust curves" as stored (MotorPreferences::isHideSimilar()), the
    /// table's initial sort keys, and the texts "Plugged" and "None".
    ThrustCurveMotorSelection(const ThrustCurveMotorSetDatabase& database,
                              Preferences&                       preferences);

    ThrustCurveMotorSelection(const ThrustCurveMotorSelection&)            = delete;
    ThrustCurveMotorSelection& operator=(const ThrustCurveMotorSelection&) = delete;
    ThrustCurveMotorSelection(ThrustCurveMotorSelection&&)                 = delete;
    ThrustCurveMotorSelection& operator=(ThrustCurveMotorSelection&&)      = delete;
    ~ThrustCurveMotorSelection();

    // ---- the table

    /// The table's rows in model order.
    [[nodiscard]] const ThrustCurveMotorDatabaseModel& getModel() const noexcept { return m_model; }

    /// The filter, whose criteria the filter panel and the search field set.
    [[nodiscard]] MotorRowFilter&       getRowFilter() noexcept { return m_rowFilter; }
    [[nodiscard]] const MotorRowFilter& getRowFilter() const noexcept { return m_rowFilter; }

    /// The sort keys, which a click on a column header toggles.
    [[nodiscard]] MotorTableSorter&       getSorter() noexcept { return m_sorter; }
    [[nodiscard]] const MotorTableSorter& getSorter() const noexcept { return m_sorter; }

    /// The rows the table shows, in its order: the model rows the filter includes, sorted (the
    /// view-to-model index of the TableRowSorter after sort()). Computed at every call; the texts
    /// follow the current name column and default units.
    [[nodiscard]] std::vector<std::size_t> getViewRows() const;

    /// The manufacturers of the filter panel's list: the manufacturer of every set of the database
    /// given to the constructor, each once, sorted by simple name (String.compareTo,
    /// Strings::javaCompareTo()).
    [[nodiscard]] const std::vector<const Manufacturer*>& getManufacturers() const noexcept
    {
        return m_manufacturers;
    }

    // ---- the selection

    /// setMotorMountAndConfig(): opens the chooser for the motor configuration @p configuration,
    /// see the class comment. The motors are kept (shared).
    /// @throws BugError when a motor of the mount is null.
    void setMotorMountAndConfig(const MountConfiguration& configuration);

    /// The selected thrust curve (getSelectedMotor()), or null.
    [[nodiscard]] const std::shared_ptr<const ThrustCurveMotor>& getSelectedMotor() const noexcept
    {
        return m_selectedMotor;
    }

    /// The set of the selected curve, or null.
    [[nodiscard]] const ThrustCurveMotorSet* getSelectedMotorSet() const noexcept
    {
        return m_selectedMotorSet;
    }

    /// select(): selects @p motor, a curve of a set of the model (from the curve list, or
    /// selectMotor() of a set); nothing happens for null or the selected curve. See the class
    /// comment for the delay and the nozzle.
    /// @throws BugError when no set of the model holds @p motor ("Could not find motor from
    ///         database, motor=<ThrustCurveMotor::toString()>").
    void select(const std::shared_ptr<const ThrustCurveMotor>& motor);

    /// selectMotorFromTable(): the table selected the model row @p modelRow; when its set is not
    /// the selected one, selects the set's default curve (select(selectMotor(set))).
    /// @throws BugError when @p modelRow is not a row of the model.
    void selectMotorFromTable(std::size_t modelRow);

    /// selectMotor(): the default curve of @p set, see the class comment.
    /// @throws BugError when @p set holds no motor ("Attempting to select motor from empty
    ///         ThrustCurveMotorSet: <set>").
    [[nodiscard]] std::shared_ptr<const ThrustCurveMotor> selectMotor(
        const ThrustCurveMotorSet& set) const;

    /// findMotorSet(): the first set of the model that holds @p motor (that very object), or null.
    [[nodiscard]] const ThrustCurveMotorSet* findMotorSet(const ThrustCurveMotor& motor) const;

    /// selectedMotor(), on OK: stores @p motor as the preferred curve of its set
    /// (MotorPreferences::storePreferredThrustCurve()); nothing when no set holds it (OpenRocket
    /// logs an error).
    void storeSelectedMotor(const ThrustCurveMotor& motor);

    // ---- the thrust curves

    /// getFilteredCurves(): the curves of the selected set, with "Hide very similar thrust curves"
    /// on only the selected one and those whose similarity with it is below
    /// kMotorSimilarityThreshold, sorted by ThrustCurveMotorComparator (stable: the set's order
    /// among equals); empty when no set is selected.
    [[nodiscard]] std::vector<std::shared_ptr<const ThrustCurveMotor>> getFilteredCurves();

    /// The items of the curve list (updateData()): MotorHolder(getFilteredCurves()[i], i).
    [[nodiscard]] std::vector<MotorHolder> getCurveItems();

    /// "Hide very similar thrust curves".
    [[nodiscard]] bool isHideSimilar() const noexcept { return m_hideSimilar; }
    /// Sets "Hide very similar thrust curves" and stores it (MotorPreferences::setHideSimilar()).
    void setHideSimilar(bool hideSimilar);

    /// "Hide motors which are not in regular production" (the filter's).
    [[nodiscard]] bool isHideUnavailable() const noexcept
    {
        return m_rowFilter.isHideUnavailable();
    }
    /// Sets "Hide motors which are not in regular production" in the filter and stores it
    /// (MotorPreferences::setHideUnavailable()).
    void setHideUnavailable(bool hideUnavailable);

    // ---- the ejection delay

    /// The texts of the plugged option: @p plugged ("Plugged", TCMotorSelPan.delayBox.Plugged),
    /// which the delay box shows and reads, and @p pluggedNone ("None",
    /// TCMotorSelPan.delayBox.PluggedNone), which it also reads; the GUI passes its translations.
    void setDelayTexts(std::string plugged, std::string pluggedNone);

    /// The selected delay in s (getSelectedDelay()), Motor::kPluggedDelay for plugged.
    [[nodiscard]] double getSelectedDelay() const noexcept { return m_selectedDelay; }

    /// The delay box's items (setDelays()): the selected set's standard delays as
    /// ThrustCurveMotor::getDelayString() writes them with the plugged text, and the plugged text
    /// appended when the set has no plugged delay; none when no curve is selected.
    [[nodiscard]] std::vector<std::string> getDelayItems() const;

    /// The text the delay box shows: getDelayString(getSelectedDelay()) with the plugged text;
    /// empty when no curve is selected.
    [[nodiscard]] std::string getDelayText() const;

    /// The delay box's action listener: @p text typed or chosen, see the class comment.
    void setDelayText(std::string_view text);

    // ---- the nozzle exit diameter

    /// The nozzle exit diameter in m (getSelectedNozzleExitDiameter()).
    [[nodiscard]] double getNozzleExitDiameter() const noexcept { return m_nozzleExitDiameter; }

    /// Sets the nozzle exit diameter, limited to [0, getMaximumNozzleExitDiameter()] as the
    /// spinner's DoubleModel limits it (MathUtil::clamp()).
    void setNozzleExitDiameter(double diameter) noexcept;

    /// The largest nozzle exit diameter, the selected curve's diameter (0 with none).
    [[nodiscard]] double getMaximumNozzleExitDiameter() const noexcept
    {
        return m_maximumNozzleExitDiameter;
    }

private:
    /// setDelays(true) and the action event OpenRocket's combo box then fires.
    void resetDelays();
    /// The delay @p text means, or @p current when it means none.
    [[nodiscard]] double parseDelay(std::string_view text, double current) const;
    /// Adds a set of its own for @p motor and sorts the model's list.
    void addExtraSet(const std::shared_ptr<const ThrustCurveMotor>& motor);

    Preferences&                                      m_preferences;
    std::vector<std::unique_ptr<ThrustCurveMotorSet>> m_extraSets;
    ThrustCurveMotorDatabaseModel                     m_model;
    MotorRowFilter                                    m_rowFilter;
    MotorTableSorter                                  m_sorter;
    MotorSimilarityCache                              m_similarities;
    std::vector<const Manufacturer*>                  m_manufacturers;

    std::shared_ptr<const ThrustCurveMotor> m_selectedMotor;
    const ThrustCurveMotorSet*              m_selectedMotorSet{nullptr};
    double                                  m_selectedDelay{0};
    double                                  m_nozzleExitDiameter{0};
    double                                  m_maximumNozzleExitDiameter{0};
    bool                                    m_hideSimilar{true};
    std::string                             m_pluggedText{"Plugged"};
    std::string                             m_pluggedNoneText{"None"};
};

}  // namespace QtRocket
