#pragma once

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "QtRocket/gui/binding/Binding.h"
#include "QtRocket/gui/binding/PropertySource.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/NoseCone.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/util/Signal.h"

namespace QtRocket::Gui::Test
{

/// The rocket of the bindings scout's multi-edit probes (multiedit_main.cpp, MultiEditProbe.java):
/// one stage with a nose cone and the body tubes "A" (outer radius 0.02 m, wall 0.001 m, length
/// 0.3 m) and "B" (0.03 m, 0.002 m, 0.5 m), both radii set by hand, events enabled. A test that
/// needs a document moves `owner` into it; the pointers stay valid.
struct TwoTubeRocket
{
    std::unique_ptr<Rocket> owner = std::make_unique<Rocket>();
    Rocket*                 rocket{owner.get()};
    AxialStage*             stage{nullptr};
    NoseCone*               nose{nullptr};
    BodyTube*               tubeA{nullptr};
    BodyTube*               tubeB{nullptr};

    TwoTubeRocket();
};

/// Records the change events a rocket emits while it lives: their types and the names of their
/// sources (read during the event: a source may be gone afterwards).
class RocketEventRecorder
{
public:
    explicit RocketEventRecorder(Rocket& rocket);

    [[nodiscard]] std::size_t                     count() const noexcept { return m_types.size(); }
    [[nodiscard]] const std::vector<int>&         types() const noexcept { return m_types; }
    [[nodiscard]] const std::vector<std::string>& sources() const noexcept { return m_sources; }
    void                                          clear();

private:
    std::vector<int>                        m_types;
    std::vector<std::string>                m_sources;
    ComponentChangeSignal::ScopedConnection m_connection;
};

/// Counts what a binding tells its listeners (Binding::addChangeListener()) while it lives.
class ChangeCounter
{
public:
    explicit ChangeCounter(Binding& binding);

    [[nodiscard]] int count() const noexcept { return m_count; }

private:
    int                        m_count{0};
    Signal<>::ScopedConnection m_connection;
};

/// A property that is not a component's, as the Java probes' Bean: a value and a count of the
/// setter's calls. The setter fires a NONFUNCTIONAL change of the component it is called for, as
/// the Bean tells its listeners.
struct BeanProperty
{
    double value{0};
    bool   automatic{false};
    int    sets{0};
    int    gets{0};
    /// Read by checkedSource() only: whether the field can be edited, and the problem shown in
    /// place of the value.
    bool                       enabled{true};
    std::optional<std::string> problem = std::nullopt;

    /// The source of the property (get, set, and the automatic pair), for any component.
    [[nodiscard]] PropertySource<double> source();

    /// The source of the property with getChecked (problem as its Error while there is one), set
    /// and isEnabled, for any component.
    [[nodiscard]] PropertySource<double> checkedSource();
};

/// An integer that is not a component's, with a count of the setter's calls; the setter fires a
/// NONFUNCTIONAL change of the component it is called for.
struct IntBeanProperty
{
    int  value{0};
    bool enabled{true};
    int  sets{0};

    /// The source of the integer (get, set and isEnabled), for any component.
    [[nodiscard]] PropertySource<int> source();
};

/// A text that is not a component's (TextFieldListenerTest's RecordingListener: the last text
/// set), with a count of the setter's calls; the setter fires a NONFUNCTIONAL change of the
/// component it is called for.
struct StringBeanProperty
{
    std::string value;
    bool        enabled{true};
    int         sets{0};

    /// The source of the text (get, set and isEnabled), for any component.
    [[nodiscard]] PropertySource<std::string> source();
};

/// The source of a body tube's length.
[[nodiscard]] PropertySource<double> bodyTubeLength();

/// The source of a body tube's outer radius with its automatic state.
[[nodiscard]] PropertySource<double> bodyTubeOuterRadius();

/// The source of a body tube's inner radius (a diameter with the multiplier 2).
[[nodiscard]] PropertySource<double> bodyTubeInnerRadius();

}  // namespace QtRocket::Gui::Test
