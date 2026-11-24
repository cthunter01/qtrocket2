// The bindings over an object that is no component (OpenRocket's DoubleModel and its kin over a
// ChangeSource: the simulation options, a wind model, a wind level, the preferences): read and
// written through PropertySource::ofObject(), refreshed on the object's change signal, ended when
// the object goes.

#include "QtRocket/gui/binding/ObjectSelection.h"

#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include <gtest/gtest.h>

#include "BindingTestSupport.h"
#include "GuiTestSupport.h"
#include "QtRocket/gui/binding/BindingContext.h"
#include "QtRocket/gui/binding/BoolBinding.h"
#include "QtRocket/gui/binding/ComponentSelection.h"
#include "QtRocket/gui/binding/DoubleBinding.h"
#include "QtRocket/gui/binding/EnumBinding.h"
#include "QtRocket/gui/binding/IntBinding.h"
#include "QtRocket/gui/binding/PropertySource.h"
#include "QtRocket/gui/binding/StringBinding.h"
#include "QtRocket/models/MultiLevelPinkNoiseWindModel.h"
#include "QtRocket/models/PinkNoiseWindModel.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/simulation/SimulationOptions.h"
#include "QtRocket/simulation/SimulationOptionsInterface.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/GeodeticComputationStrategy.h"
#include "QtRocket/util/Signal.h"

namespace QtRocket::Gui
{

namespace
{

using ObjectSelectionTest = Test::GuiTest;
using Test::bodyTubeLength;
using Test::ChangeCounter;
using Test::TwoTubeRocket;
using Phase     = BindingContext::Phase;
using LevelWind = MultiLevelPinkNoiseWindModel::LevelWindModel;

/// An object that is no component (the Java probes' Bean): values of every binding's type, each
/// setter emitting changed() when it changes its value, one value whose setter announces
/// nothing, and the count of the setters' calls.
struct Bean
{
    double                      value{1.5};
    bool                        automatic{false};
    bool                        enabled{true};
    bool                        flag{false};
    int                         count{3};
    GeodeticComputationStrategy choice{GeodeticComputationStrategy::SPHERICAL};
    std::string                 text{"bean"};
    double                      quiet{0.0};
    int                         sets{0};
    Signal<>                    signal;

    [[nodiscard]] Signal<>& changed() noexcept { return signal; }

    template <class T>
    void store(T& field, const T& newValue)
    {
        ++sets;
        if (field != newValue)
        {
            field = newValue;
            signal.emit();
        }
    }
};

/// The bean's value, with its automatic state and the enabled state.
[[nodiscard]] DoubleSource beanValue()
{
    return DoubleSource::ofObject<Bean>({
        .get          = [](const Bean& bean) { return bean.value; },
        .set          = [](Bean& bean, double value) { bean.store(bean.value, value); },
        .isAutomatic  = [](const Bean& bean) { return bean.automatic; },
        .setAutomatic = [](Bean& bean, bool automatic) { bean.store(bean.automatic, automatic); },
        .isEnabled    = [](const Bean& bean) { return bean.enabled; },
    });
}

/// The bean's value whose setter announces nothing.
[[nodiscard]] DoubleSource beanQuiet()
{
    return DoubleSource::ofObject<Bean>({
        .get = [](const Bean& bean) { return bean.quiet; },
        .set = [](Bean& bean, double value) { bean.quiet = value; },
    });
}

[[nodiscard]] BoolSource beanFlag()
{
    return BoolSource::ofObject<Bean>({
        .get = [](const Bean& bean) { return bean.flag; },
        .set = [](Bean& bean, bool flag) { bean.store(bean.flag, flag); },
    });
}

[[nodiscard]] IntSource beanCount()
{
    return IntSource::ofObject<Bean>({
        .get = [](const Bean& bean) { return bean.count; },
        .set = [](Bean& bean, int count) { bean.store(bean.count, count); },
    });
}

[[nodiscard]] EnumSource<GeodeticComputationStrategy> beanChoice()
{
    return EnumSource<GeodeticComputationStrategy>::ofObject<Bean>({
        .get = [](const Bean& bean) { return bean.choice; },
        .set = [](Bean&                       bean,
                  GeodeticComputationStrategy choice) { bean.store(bean.choice, choice); },
    });
}

[[nodiscard]] StringSource beanText()
{
    return StringSource::ofObject<Bean>({
        .get = [](const Bean& bean) { return bean.text; },
        .set = [](Bean& bean, const std::string& text) { bean.store(bean.text, text); },
    });
}

/// The launch rod's length through the interface the simulation options and the launch
/// preferences share.
[[nodiscard]] DoubleSource rodLength()
{
    return DoubleSource::ofObject<SimulationOptionsInterface>({
        .get =
            [](const SimulationOptionsInterface& options) { return options.getLaunchRodLength(); },
        .set = [](SimulationOptionsInterface& options,
                  double                      length) { options.setLaunchRodLength(length); },
    });
}

/// A recovery threshold of the simulation options (RecoveryWarningsPanel's).
[[nodiscard]] DoubleSource optionsRecoverySpeed()
{
    return DoubleSource::ofObject<SimulationOptions>({
        .get = [](const SimulationOptions& options) { return options.getRecoverySpeedWarning(); },
        .set = [](SimulationOptions& options,
                  double             speed) { options.setRecoverySpeedWarning(speed); },
    });
}

/// The same threshold of the preferences (the Warnings page's).
[[nodiscard]] DoubleSource preferencesRecoverySpeed()
{
    return DoubleSource::ofObject<Preferences>({
        .get = [](const Preferences& preferences) { return preferences.getRecoverySpeedWarning(); },
        .set = [](Preferences& preferences,
                  double       speed) { preferences.setRecoverySpeedWarning(speed); },
    });
}

/// The Design page's "Show confirmation dialog for discarding simulation changes", whose setter
/// stores without emitting changed().
[[nodiscard]] BoolSource discardSimulationConfirmation()
{
    return BoolSource::ofObject<Preferences>({
        .get =
            [](const Preferences& preferences) {
                return preferences.isShowDiscardSimulationConfirmation();
            },
        .set = [](Preferences& preferences,
                  bool         shown) { preferences.setShowDiscardSimulationConfirmation(shown); },
    });
}

/// The average wind speed and its standard deviation (SimulationConditionsPanel's, the deviation
/// bounded by the average).
[[nodiscard]] DoubleSource windAverage()
{
    return DoubleSource::ofObject<PinkNoiseWindModel>({
        .get = [](const PinkNoiseWindModel& wind) { return wind.getAverage(); },
        .set = [](PinkNoiseWindModel& wind, double average) { wind.setAverage(average); },
    });
}

[[nodiscard]] DoubleSource windDeviation()
{
    return DoubleSource::ofObject<PinkNoiseWindModel>({
        .get = [](const PinkNoiseWindModel& wind) { return wind.getStandardDeviation(); },
        .set = [](PinkNoiseWindModel& wind,
                  double              deviation) { wind.setStandardDeviation(deviation); },
    });
}

/// A wind level's speed (the wind editor's).
[[nodiscard]] DoubleSource levelSpeed()
{
    return DoubleSource::ofObject<LevelWind>({
        .get = [](const LevelWind& level) { return level.getSpeed(); },
        .set = [](LevelWind& level, double speed) { level.setSpeed(speed); },
    });
}

/// The bean's units: none.
[[nodiscard]] const UnitGroup& plainUnits()
{
    return unitGroup(UnitGroupId::NONE);
}

[[nodiscard]] const UnitGroup& lengthUnits()
{
    return unitGroup(UnitGroupId::LENGTH);
}

[[nodiscard]] const UnitGroup& speedUnits()
{
    return unitGroup(UnitGroupId::VELOCITY);
}

/// Records the phases a context sends.
class PhaseRecorder
{
public:
    explicit PhaseRecorder(BindingContext& context)
      : m_connection(context.addListener([this](Phase phase) { m_phases.push_back(phase); }))
    {
    }

    [[nodiscard]] const std::vector<Phase>& phases() const noexcept { return m_phases; }
    void                                    clear() { m_phases.clear(); }

private:
    std::vector<Phase>                            m_phases;
    BindingContext::PhaseSignal::ScopedConnection m_connection;
};

/// Records the bound of a binding each time it tells its listeners.
class MaximumRecorder
{
public:
    explicit MaximumRecorder(const std::shared_ptr<DoubleBinding>& binding)
      : m_connection(binding->addChangeListener(
            [this, raw = binding.get()] { m_maxima.push_back(raw->maximum()); }))
    {
    }

    [[nodiscard]] const std::vector<double>& maxima() const noexcept { return m_maxima; }

private:
    std::vector<double>        m_maxima;
    Signal<>::ScopedConnection m_connection;
};

/// A binding listener that drops the last owners of the binding and its selection (a dialog
/// that closes on a change).
struct DropOwners
{
    std::shared_ptr<DoubleBinding>*   binding;
    std::shared_ptr<ObjectSelection>* selection;

    void operator()() const
    {
        binding->reset();
        selection->reset();
    }
};

}  // namespace

TEST_F(ObjectSelectionTest, ABindingReadsAndWritesItsObject)
{
    Bean                                   bean;
    const std::shared_ptr<ObjectSelection> selection = ObjectSelection::make<Bean>(bean);
    const auto binding = std::make_shared<DoubleBinding>(selection, beanValue(), plainUnits(),
                                                         DoubleBinding::Options{});
    const ChangeCounter changes(*binding);
    EXPECT_TRUE(selection->isValid());
    EXPECT_TRUE(selection->holds<Bean>());
    EXPECT_FALSE(selection->holds<SimulationOptions>());
    EXPECT_EQ(&selection->getObject<Bean>(), &bean);
    EXPECT_EQ(binding->getContext(), selection);
    EXPECT_EQ(binding->getSelection(), nullptr);
    EXPECT_TRUE(binding->isValid());
    EXPECT_EQ(binding->value(), 1.5);
    EXPECT_TRUE(binding->isAutomaticAvailable());
    EXPECT_TRUE(binding->isEnabled());

    binding->setValue(2.5);
    EXPECT_EQ(bean.value, 2.5);
    EXPECT_EQ(bean.sets, 1);
    EXPECT_EQ(binding->value(), 2.5);
    EXPECT_EQ(changes.count(), 1);

    EXPECT_TRUE(binding->commitText("4"));
    EXPECT_EQ(bean.value, 4.0);
    binding->setAutomatic(true);
    EXPECT_TRUE(bean.automatic);
    EXPECT_TRUE(binding->isAutomatic());
    EXPECT_EQ(changes.count(), 3);
}

TEST_F(ObjectSelectionTest, AChangeOfTheObjectReachesItsBindings)
{
    // DoubleModel.java:764-766: "programmatic changes, such as resetting simulation conditions,
    // are propagated to the Swing widgets".
    SimulationOptions options;
    options.setLaunchRodLength(2.0);
    const std::shared_ptr<ObjectSelection> selection =
        ObjectSelection::make<SimulationOptionsInterface>(options);
    const auto length = std::make_shared<DoubleBinding>(selection, rodLength(), lengthUnits(),
                                                        DoubleBinding::Options{.min = 0});
    const ChangeCounter changes(*length);
    EXPECT_EQ(length->value(), 2.0);

    options.setLaunchRodLength(1.5);
    EXPECT_EQ(length->value(), 1.5);
    EXPECT_EQ(changes.count(), 1);

    // Reset to default: copyConditionsFrom() emits several changes; the binding tells of a change
    // of what it shows only.
    const SimulationOptions defaults;
    options.copyConditionsFrom(defaults);
    EXPECT_EQ(length->value(), defaults.getLaunchRodLength());
    EXPECT_EQ(changes.count(), 2);

    // A change of something else tells the widgets nothing.
    options.setLaunchAltitude(options.getLaunchAltitude() + 100);
    EXPECT_EQ(changes.count(), 2);
}

TEST_F(ObjectSelectionTest, EveryEmissionRefreshesInTwoPhases)
{
    Bean                                   bean;
    const std::shared_ptr<ObjectSelection> selection = ObjectSelection::make<Bean>(bean);
    PhaseRecorder                          phases(*selection);
    bean.store(bean.count, 4);
    EXPECT_EQ(phases.phases(), (std::vector<Phase>{Phase::READ, Phase::NOTIFY}));
    phases.clear();
    // A store of the same value emits nothing.
    bean.store(bean.count, 4);
    EXPECT_TRUE(phases.phases().empty());
    selection->refresh();
    EXPECT_EQ(phases.phases(), (std::vector<Phase>{Phase::READ, Phase::NOTIFY}));
}

TEST_F(ObjectSelectionTest, ADynamicBoundIsReadBeforeTheWidgetsAreTold)
{
    // SimulationConditionsPanel: the standard deviation is bounded by the average wind speed.
    PinkNoiseWindModel wind(1);
    wind.setAverage(2.0);
    wind.setStandardDeviation(0.5);
    const std::shared_ptr<ObjectSelection> selection =
        ObjectSelection::make<PinkNoiseWindModel>(wind);
    const auto average = std::make_shared<DoubleBinding>(selection, windAverage(), speedUnits(),
                                                         DoubleBinding::Options{.min = 0});
    const auto deviation =
        std::make_shared<DoubleBinding>(selection, windDeviation(), speedUnits(),
                                        DoubleBinding::Options{.min = 0, .maxBinding = average});
    const MaximumRecorder maxima(deviation);
    EXPECT_EQ(deviation->maximum(), 2.0);

    // setAverage() keeps the turbulence intensity: the deviation and its bound both change.
    average->setValue(4.0);
    EXPECT_EQ(wind.getAverage(), 4.0);
    EXPECT_EQ(deviation->value(), 1.0);
    ASSERT_FALSE(maxima.maxima().empty());
    for (const double maximum : maxima.maxima())
    {
        EXPECT_EQ(maximum, 4.0);
    }
    deviation->setValue(9.0);
    EXPECT_EQ(wind.getStandardDeviation(), 4.0);
}

TEST_F(ObjectSelectionTest, AWriteThatAnnouncesNothingStillRefreshes)
{
    InMemoryPreferences preferences;
    preferences.setShowDiscardSimulationConfirmation(true);
    const std::shared_ptr<ObjectSelection> selection =
        ObjectSelection::make<Preferences>(preferences);
    const auto shown = std::make_shared<BoolBinding>(selection, discardSimulationConfirmation());
    const ChangeCounter changes(*shown);
    const PhaseRecorder phases(*selection);
    EXPECT_TRUE(shown->value());

    // The setter emits no changed(): apply() refreshes the bindings itself.
    EXPECT_TRUE(shown->setChecked(false));
    EXPECT_FALSE(preferences.isShowDiscardSimulationConfirmation());
    EXPECT_FALSE(shown->value());
    EXPECT_EQ(changes.count(), 1);
    EXPECT_EQ(phases.phases(), (std::vector<Phase>{Phase::READ, Phase::NOTIFY}));

    // A change elsewhere that announces nothing shows after a refresh().
    preferences.setShowDiscardSimulationConfirmation(true);
    EXPECT_FALSE(shown->value());
    selection->refresh();
    EXPECT_TRUE(shown->value());
    EXPECT_EQ(changes.count(), 2);

    // A setter that emits refreshes once, through its emission.
    Bean                                   bean;
    const std::shared_ptr<ObjectSelection> beanSelection = ObjectSelection::make<Bean>(bean);
    const auto quiet = std::make_shared<DoubleBinding>(beanSelection, beanQuiet(), plainUnits(),
                                                       DoubleBinding::Options{});
    const auto value = std::make_shared<DoubleBinding>(beanSelection, beanValue(), plainUnits(),
                                                       DoubleBinding::Options{});
    const PhaseRecorder beanPhases(*beanSelection);
    quiet->setValue(0.25);
    EXPECT_EQ(quiet->value(), 0.25);
    value->setValue(7.0);
    EXPECT_EQ(beanPhases.phases(),
              (std::vector<Phase>{Phase::READ, Phase::NOTIFY, Phase::READ, Phase::NOTIFY}));
}

TEST_F(ObjectSelectionTest, TheSelectionEndsWithItsObject)
{
    auto                                   bean      = std::make_unique<Bean>();
    const std::shared_ptr<ObjectSelection> selection = ObjectSelection::make<Bean>(*bean);
    const auto binding = std::make_shared<DoubleBinding>(selection, beanValue(), plainUnits(),
                                                         DoubleBinding::Options{});
    const auto flag    = std::make_shared<BoolBinding>(selection, beanFlag());
    const PhaseRecorder phases(*selection);

    bean.reset();
    EXPECT_FALSE(selection->isValid());
    EXPECT_FALSE(binding->isValid());
    EXPECT_FALSE(binding->isEnabled());
    EXPECT_EQ(binding->text(), "");
    EXPECT_EQ(binding->value(), 1.5);
    EXPECT_FALSE(binding->commitText("3"));
    EXPECT_FALSE(flag->setChecked(true));
    binding->setValue(5.0);
    EXPECT_THROW(std::ignore = selection->getObject<Bean>(), BugError);
    EXPECT_THROW(selection->apply<Bean>([](Bean& gone) { gone.value = 0; }), BugError);
    selection->refresh();
    EXPECT_TRUE(phases.phases().empty());
    // Nobody is told when the object goes; invalidate() still tells the bindings.
    selection->invalidate();
    EXPECT_EQ(phases.phases(), (std::vector<Phase>{Phase::INVALIDATED}));
}

TEST_F(ObjectSelectionTest, InvalidateTellsTheBindingsOnce)
{
    Bean                                   bean;
    const std::shared_ptr<ObjectSelection> selection = ObjectSelection::make<Bean>(bean);
    const auto binding = std::make_shared<DoubleBinding>(selection, beanValue(), plainUnits(),
                                                         DoubleBinding::Options{});
    const ChangeCounter changes(*binding);
    const PhaseRecorder phases(*selection);

    selection->invalidate();
    EXPECT_FALSE(selection->isValid());
    EXPECT_EQ(changes.count(), 1);
    EXPECT_EQ(phases.phases(), (std::vector<Phase>{Phase::INVALIDATED}));
    selection->invalidate();
    selection->release();
    EXPECT_EQ(phases.phases().size(), 1U);

    // The object stays, untouched by the bindings, and its changes reach nobody.
    EXPECT_FALSE(binding->commitText("8"));
    bean.store(bean.value, 3.0);
    EXPECT_EQ(binding->value(), 1.5);
    EXPECT_EQ(phases.phases().size(), 1U);
    EXPECT_TRUE(bean.signal.empty());
}

TEST_F(ObjectSelectionTest, ReleaseEndsTheSelectionWithoutTellingAnybody)
{
    Bean                                   bean;
    const std::shared_ptr<ObjectSelection> selection = ObjectSelection::make<Bean>(bean);
    const auto          flag = std::make_shared<BoolBinding>(selection, beanFlag());
    const ChangeCounter changes(*flag);
    const PhaseRecorder phases(*selection);

    selection->release();
    EXPECT_FALSE(selection->isValid());
    EXPECT_FALSE(flag->isValid());
    EXPECT_FALSE(flag->setChecked(true));
    EXPECT_FALSE(bean.flag);
    EXPECT_EQ(changes.count(), 0);
    selection->invalidate();
    EXPECT_TRUE(phases.phases().empty());
    EXPECT_TRUE(bean.signal.empty());
}

TEST_F(ObjectSelectionTest, TheSignalLosingItsConnectionsEndsTheSelection)
{
    Bean                                   bean;
    const std::shared_ptr<ObjectSelection> selection = ObjectSelection::make<Bean>(bean);
    bean.signal.disconnectAll();
    EXPECT_FALSE(selection->isValid());
}

TEST_F(ObjectSelectionTest, ASelectionListensToTheSignalItIsGiven)
{
    Bean                                   bean;
    Signal<>                               other;
    const std::shared_ptr<ObjectSelection> selection = ObjectSelection::make<Bean>(bean, other);
    const PhaseRecorder                    phases(*selection);
    bean.store(bean.count, 7);
    EXPECT_TRUE(phases.phases().empty());
    other.emit();
    EXPECT_EQ(phases.phases(), (std::vector<Phase>{Phase::READ, Phase::NOTIFY}));
}

TEST_F(ObjectSelectionTest, AWindLevelIsInvalidatedBeforeItIsRemoved)
{
    // The wind editor's rows: a level removed from the model goes with its signal.
    const InMemoryPreferences    preferences;
    MultiLevelPinkNoiseWindModel model(preferences);
    ASSERT_TRUE(model.addWindLevel(100, 3, 0, 0.3).has_value());
    ASSERT_TRUE(model.addWindLevel(200, 4, 0, 0.4).has_value());
    LevelWind*                             first  = model.getLevels().at(1);
    LevelWind*                             second = model.getLevels().at(2);
    const std::shared_ptr<ObjectSelection> firstSelection =
        ObjectSelection::make<LevelWind>(*first);
    const std::shared_ptr<ObjectSelection> secondSelection =
        ObjectSelection::make<LevelWind>(*second);
    const auto firstSpeed = std::make_shared<DoubleBinding>(firstSelection, levelSpeed(),
                                                            speedUnits(), DoubleBinding::Options{});
    const auto secondSpeed = std::make_shared<DoubleBinding>(
        secondSelection, levelSpeed(), speedUnits(), DoubleBinding::Options{});
    EXPECT_EQ(firstSpeed->value(), 3.0);
    EXPECT_EQ(secondSpeed->value(), 4.0);
    firstSpeed->setValue(3.5);
    EXPECT_EQ(model.getLevels().at(1)->getSpeed(), 3.5);

    // The owner tells the widgets before it removes the level.
    const ChangeCounter firstChanges(*firstSpeed);
    firstSelection->invalidate();
    model.removeWindLevel(100);
    EXPECT_EQ(firstChanges.count(), 1);
    EXPECT_FALSE(firstSpeed->isValid());

    // Removed without a word, the level's selection learns it from the signal's end.
    model.removeWindLevel(200);
    EXPECT_FALSE(secondSelection->isValid());
    EXPECT_FALSE(secondSpeed->isValid());
    EXPECT_FALSE(secondSpeed->commitText("1"));
}

TEST_F(ObjectSelectionTest, ASourceReadsOnlyObjectsOfItsOwnClass)
{
    SimulationOptions options;
    // The options' own class: a source of the interface does not read it.
    const std::shared_ptr<ObjectSelection> own = ObjectSelection::make<SimulationOptions>(options);
    EXPECT_THROW(std::ignore = std::make_shared<DoubleBinding>(own, rodLength(), lengthUnits(),
                                                               DoubleBinding::Options{}),
                 BugError);
    EXPECT_THROW(std::ignore = own->getObject<SimulationOptionsInterface>(), BugError);
    const auto speed = std::make_shared<DoubleBinding>(own, optionsRecoverySpeed(), speedUnits(),
                                                       DoubleBinding::Options{.min = 0});
    EXPECT_EQ(speed->value(), options.getRecoverySpeedWarning());

    // Through the interface the conditions panel edits.
    const std::shared_ptr<ObjectSelection> shared =
        ObjectSelection::make<SimulationOptionsInterface>(options);
    EXPECT_TRUE(shared->holds<SimulationOptionsInterface>());
    EXPECT_FALSE(shared->holds<SimulationOptions>());
    const auto length = std::make_shared<DoubleBinding>(shared, rodLength(), lengthUnits(),
                                                        DoubleBinding::Options{});
    length->setValue(0.75);
    EXPECT_EQ(options.getLaunchRodLength(), 0.75);
    EXPECT_THROW(std::ignore = std::make_shared<DoubleBinding>(
                     shared, optionsRecoverySpeed(), speedUnits(), DoubleBinding::Options{}),
                 BugError);
}

TEST_F(ObjectSelectionTest, OneThresholdEditorHasASourceForEachClass)
{
    // RecoveryWarningsPanel edits the simulation options or the preferences (Java's DoubleModel
    // finds getRecoverySpeedWarning() on either by reflection).
    SimulationOptions   options;
    InMemoryPreferences preferences;
    const auto          simulation = std::make_shared<DoubleBinding>(
        ObjectSelection::make<SimulationOptions>(options), optionsRecoverySpeed(), speedUnits(),
        DoubleBinding::Options{.min = 0});
    const auto page = std::make_shared<DoubleBinding>(
        ObjectSelection::make<Preferences>(preferences), preferencesRecoverySpeed(), speedUnits(),
        DoubleBinding::Options{.min = 0});
    EXPECT_TRUE(simulation->commitText("37"));
    EXPECT_EQ(options.getRecoverySpeedWarning(), 37.0);
    EXPECT_TRUE(page->commitText("12"));
    EXPECT_EQ(preferences.getRecoverySpeedWarning(), 12.0);
    page->setValue(-3.0);
    EXPECT_EQ(preferences.getRecoverySpeedWarning(), 0.0);
}

TEST_F(ObjectSelectionTest, ComponentAndObjectSourcesNeedTheirOwnKindOfContext)
{
    const TwoTubeRocket design;
    const auto          components = std::make_shared<ComponentSelection>(*design.tubeA);
    Bean                bean;
    const std::shared_ptr<ObjectSelection> object = ObjectSelection::make<Bean>(bean);
    EXPECT_THROW(std::ignore = std::make_shared<DoubleBinding>(
                     components, beanValue(), plainUnits(), DoubleBinding::Options{}),
                 BugError);
    EXPECT_THROW(std::ignore = std::make_shared<DoubleBinding>(
                     object, bodyTubeLength(), lengthUnits(), DoubleBinding::Options{}),
                 BugError);
    EXPECT_THROW(beanValue().set(*components, 1.0), BugError);
    EXPECT_THROW(bodyTubeLength().set(*object, 1.0), BugError);
    EXPECT_EQ(design.tubeA->getLength(), 0.3);
    EXPECT_EQ(bean.value, 1.5);
}

TEST_F(ObjectSelectionTest, TheAccessorsOfAnObjectMustBeComplete)
{
    using Accessors = DoubleSource::Accessors<Bean>;
    EXPECT_THROW(std::ignore = DoubleSource::ofObject<Bean>(Accessors{}), BugError);
    EXPECT_THROW(std::ignore = DoubleSource::ofObject<Bean>(
                     {.get        = [](const Bean& bean) { return bean.value; },
                      .getChecked = [](const Bean& bean) { return Result<double>(bean.value); }}),
                 BugError);
    EXPECT_THROW(std::ignore = DoubleSource::ofObject<Bean>(
                     {.get         = [](const Bean& bean) { return bean.value; },
                      .isAutomatic = [](const Bean& bean) { return bean.automatic; }}),
                 BugError);
    const DoubleSource readOnly =
        DoubleSource::ofObject<Bean>({.get = [](const Bean& bean) { return bean.value; }});
    Bean                                   bean;
    const std::shared_ptr<ObjectSelection> selection = ObjectSelection::make<Bean>(bean);
    EXPECT_FALSE(readOnly.isWritable());
    EXPECT_THROW(readOnly.set(*selection, 2.0), BugError);
    EXPECT_THROW(readOnly.setAutomatic(*selection, true), BugError);
    EXPECT_EQ(readOnly.getValue(*selection), 1.5);
}

TEST_F(ObjectSelectionTest, EveryBindingWorksOverAnObject)
{
    Bean                                   bean;
    const std::shared_ptr<ObjectSelection> selection = ObjectSelection::make<Bean>(bean);
    const auto flag   = std::make_shared<BoolBinding>(selection, beanFlag());
    const auto count  = std::make_shared<IntBinding>(selection, beanCount(), 0, 10);
    const auto choice = std::make_shared<EnumBinding<GeodeticComputationStrategy>>(
        selection, beanChoice(), kAllGeodeticComputationStrategies);
    const auto text = std::make_shared<StringBinding>(selection, beanText());

    EXPECT_TRUE(flag->setChecked(true));
    EXPECT_TRUE(bean.flag);
    EXPECT_TRUE(count->stepUp());
    EXPECT_EQ(bean.count, 4);
    count->setValue(20);
    EXPECT_EQ(bean.count, 10);
    EXPECT_TRUE(choice->select(GeodeticComputationStrategy::WGS84));
    EXPECT_EQ(bean.choice, GeodeticComputationStrategy::WGS84);
    EXPECT_TRUE(text->setText("renamed"));
    EXPECT_EQ(bean.text, "renamed");

    // Each re-reads on the object's change.
    bean.store(bean.flag, false);
    bean.store(bean.count, 1);
    bean.store(bean.choice, GeodeticComputationStrategy::FLAT);
    bean.store(bean.text, std::string("again"));
    EXPECT_FALSE(flag->value());
    EXPECT_EQ(count->value(), 1);
    EXPECT_EQ(choice->value(), GeodeticComputationStrategy::FLAT);
    EXPECT_EQ(text->value(), "again");
}

TEST_F(ObjectSelectionTest, AListenerMayDropTheLastOwners)
{
    Bean                             bean;
    std::shared_ptr<ObjectSelection> selection = ObjectSelection::make<Bean>(bean);
    auto binding = std::make_shared<DoubleBinding>(selection, beanValue(), plainUnits(),
                                                   DoubleBinding::Options{});
    const Signal<>::ScopedConnection drop(
        binding->addChangeListener(DropOwners{.binding = &binding, .selection = &selection}));
    const std::weak_ptr<ObjectSelection> watched = selection;
    bean.store(bean.value, 6.0);
    EXPECT_EQ(binding, nullptr);
    EXPECT_EQ(selection, nullptr);
    EXPECT_TRUE(watched.expired());
    EXPECT_TRUE(bean.signal.empty());
}

}  // namespace QtRocket::Gui
