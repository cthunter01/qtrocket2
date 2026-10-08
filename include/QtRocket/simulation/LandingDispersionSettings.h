#pragma once

#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace QtRocket
{

/// The landing dispersion settings of a simulation as a design file states them: what the
/// <landingdispersion> element of a simulation holds, kept as text and not interpreted.
///
///     <landingdispersion runs="500" seed="12345">
///       <uncertainty parameter="windspeed" distribution="normal" spread="0.5"/>
///       <uncertainty parameter="totalmass" distribution="lognormal" spread="0.02"/>
///     </landingdispersion>
///
/// It stands where OpenRocket's Simulation holds a MonteCarloSettings (the number of runs, the
/// seed, the number of threads and, per MonteCarloParameter, an UncertaintySpec of a
/// distribution and a spread). Landing dispersion is a Monte Carlo analysis, which is not part
/// of QtRocket yet, so nothing here says what the texts mean: the class exists so that a
/// design that has the settings keeps them through loading, copying, undo and a later save,
/// where OpenRocket keeps the settings it understood.
///
/// A value type: a copy is a copy of the texts, and two settings are equal when their
/// attributes are equal and their uncertainties are equal one by one, in order.
///
/// Deviations from OpenRocket (all of them until the Monte Carlo milestone,
/// "HOOK(monte-carlo)" in the source file of the .ork reader's handler, which fills this):
/// - Nothing is validated. OpenRocket's reader refuses the settings as a whole when the number
///   of runs or the seed is missing or no integer or when the number of runs is not within 2
///   to 100000, and refuses an uncertainty whose parameter or distribution it does not know,
///   whose spread is no number, not finite or negative, or whose distribution does not fit
///   the parameter. Here all of that is kept as it was read.
/// - The uncertainties are a list in the order of the file. OpenRocket keeps one per
///   parameter, the last one read, drops one whose spread is 0, and writes them in the order
///   of its parameters.
/// - Every attribute of the element and of an uncertainty is kept, also one OpenRocket does
///   not read.
/// - Equality compares the texts, so "500" and " 500 " are different numbers of runs here and
///   the same in OpenRocket. OpenRocket's settings also hold a number of threads, which a file
///   does not store: settings read from a file take the number of processors of the machine.
class LandingDispersionSettings
{
public:
    /// The attributes of an element by name (ElementHandler::Attributes is the same type).
    using Attributes = std::map<std::string, std::string, std::less<>>;

    /// Settings without attributes and without uncertainties: an empty <landingdispersion/>.
    LandingDispersionSettings() = default;

    /// Settings with the attributes @p attributes of the element and the attributes of its
    /// <uncertainty> children, @p uncertainties, in the order of the file.
    explicit LandingDispersionSettings(Attributes              attributes,
                                       std::vector<Attributes> uncertainties = {});

    /// The attributes of the element. OpenRocket reads "runs" (the number of simulation runs)
    /// and "seed" (the seed of the random numbers).
    [[nodiscard]] const Attributes& getAttributes() const noexcept { return m_attributes; }

    /// The attribute @p name of the element, or nullopt when it has none of that name.
    [[nodiscard]] std::optional<std::string_view> getAttribute(std::string_view name) const;

    /// The attributes of the <uncertainty> children, one map per child, in the order of the
    /// file. OpenRocket reads "parameter" (what varies from run to run: "windspeed",
    /// "totalmass", ...), "distribution" ("normal", "uniform" or "lognormal") and "spread".
    [[nodiscard]] const std::vector<Attributes>& getUncertainties() const noexcept
    {
        return m_uncertainties;
    }

    /// Adds an uncertainty with the attributes @p attributes after the ones there are.
    void addUncertainty(Attributes attributes);

    /// The settings as one line of text, for a log or a test: the attributes and then the
    /// uncertainties, each as Java prints a sorted map: "{runs=500, seed=12345}
    /// [{distribution=normal, parameter=windspeed, spread=0.5}]".
    [[nodiscard]] std::string toString() const;

    /// Whether the attributes and the uncertainties are the same texts (see the class comment).
    /// (Defined in the source file: inlined into a caller that compares with settings it has
    /// just made, the comparison of the lists trips GCC's -Wnull-dereference.)
    [[nodiscard]] bool operator==(const LandingDispersionSettings& other) const;

private:
    Attributes              m_attributes;
    std::vector<Attributes> m_uncertainties;
};

}  // namespace QtRocket
