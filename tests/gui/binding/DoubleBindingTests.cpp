// The model of a number field (Java: DoubleModel with its spinner, slider and automatic models).
//
// Expectations marked "probe" are OpenRocket's own results, printed by the Java probes of the
// bindings scout (handoff/m2-scout/evidence/m2-scout-bindings/java/: SpinnerProbe.java ->
// spinner-probe.out, run on OpenRocket 5f164fd0e with JDK 17). Its model was
// DoubleModel(bean, "Value", 2, UNITS_LENGTH, 0, 1.0): a diameter of a radius, whose minimum is
// infinite (the int literal 0 is an Integer, which DoubleModel takes for no bound) and whose
// maximum is 1 m. The values are + - * / of the unit's multiplier, so they are pinned exactly.

#include "QtRocket/gui/binding/DoubleBinding.h"

#include <array>
#include <format>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>

#include <gtest/gtest.h>

#include "BindingTestSupport.h"
#include "GuiTestSupport.h"
#include "QtRocket/aero/ComponentDrag.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/gui/binding/ComponentSelection.h"
#include "QtRocket/gui/binding/PropertySource.h"
#include "QtRocket/gui/binding/SliderMapping.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/unit/GeneralUnit.h"
#include "QtRocket/unit/Unit.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/Signal.h"
#include "aero/BarrowmanTestRockets.h"
#include "unit/DefaultUnitsGuard.h"

namespace QtRocket::Gui
{

namespace
{

using DoubleBindingTest = Test::GuiTest;
using Test::BeanProperty;
using Test::bodyTubeInnerRadius;
using Test::bodyTubeLength;
using Test::bodyTubeOuterRadius;
using Test::ChangeCounter;
using Test::TwoTubeRocket;

constexpr double kInfinity = std::numeric_limits<double>::infinity();

[[nodiscard]] const UnitGroup& lengthUnits()
{
    return unitGroup(UnitGroupId::LENGTH);
}

[[nodiscard]] const Unit& lengthUnit(std::string_view name)
{
    return *lengthUnits().getUnit(name);
}

/// The value of @p value, or NaN with a failure when there is none.
[[nodiscard]] double valueOf(const std::optional<double>& value)
{
    if (!value.has_value())
    {
        ADD_FAILURE() << "no value";
        return std::numeric_limits<double>::quiet_NaN();
    }
    return *value;
}

/// The probe's model over @p bean on the component @p component: a diameter (multiplier 2) of
/// the length units with no minimum and a maximum of 1 m.
[[nodiscard]] std::shared_ptr<DoubleBinding> probeModel(BeanProperty&    bean,
                                                        RocketComponent& component)
{
    return std::make_shared<DoubleBinding>(std::make_shared<ComponentSelection>(component),
                                           bean.source(), lengthUnits(),
                                           DoubleBinding::Options{.multiplier = 2, .max = 1.0});
}

/// A text the probe typed and what OpenRocket made of it: the bean's value (the radius) and the
/// text shown afterwards.
struct CommitRow
{
    std::string_view text;
    double           radius;
    std::string_view shown;
};

/// The probe's spinner in one unit: the text of the radius 0.0125 m, the next and previous values,
/// and the five texts it took.
struct SpinnerUnitRow
{
    std::string_view         unit;
    std::string_view         text;
    double                   next;
    double                   previous;
    std::array<CommitRow, 5> commits;
};

// NOLINTBEGIN(modernize-use-designated-initializers): a table printed by the Java probe
const std::array<SpinnerUnitRow, 5> kSpinnerTable{{
    {"mm",
     "25",
     26.0,
     24.0,
     {{{"30", 0.015, "30"},
       {"2*3", 0.003, "6"},
       {"1 1/2", 7.5E-4, "1.5"},
       {"-4", -0.002, "-4"},
       {"2000", 0.5, "1000"}}}},
    {"cm",
     "2.5",
     3.5,
     1.5,
     {{{"30", 0.15, "30"},
       {"2*3", 0.03, "6"},
       {"1 1/2", 0.0075, "1.5"},
       {"-4", -0.02, "-4"},
       {"2000", 0.5, "100"}}}},
    {"in",
     "0.984",
     1.984251968503937,
     -0.015748031496062853,
     {{{"30", 0.381, "30"},
       {"2*3", 0.07619999999999999, "6"},
       {"1 1/2", 0.019049999999999997, "1.5"},
       {"-4", -0.0508, "-4"},
       {"2000", 0.5, "39.37"}}}},
    {"in/64",
     "\u2076\u00b3\u2044\u2086\u2084",
     1.0,
     0.9375,
     {{{"30", 0.381, "30"},
       {"2*3", 0.07619999999999999, "6"},
       {"1 1/2", 0.019049999999999997, "1 \u00b9\u2044\u2082"},
       {"-4", -0.0508, "-4"},
       {"2000", 0.5, "39 \u00b3\u2044\u2088"}}}},
    {"ft",
     "0.082",
     1.082020997375328,
     -0.9179790026246719,
     {{{"30", 0.5, "3.28"},
       {"2*3", 0.5, "3.28"},
       {"1 1/2", 0.22860000000000003, "1.5"},
       {"-4", -0.6096, "-4"},
       {"2000", 0.5, "3.28"}}}},
}};
// NOLINTEND(modernize-use-designated-initializers)

/// One sample of the probe's table of the units' steps: a value in the unit, the unit's next and
/// previous values of it (the spinner's step) and its rounding (the slider's).
struct UnitStep
{
    double value;
    double next;
    double previous;
    double rounded;
};

/// The samples of one length unit.
struct UnitStepRow
{
    std::string_view         unit;
    std::array<UnitStep, 15> steps;
};

// NOLINTBEGIN(modernize-use-designated-initializers): a table printed by the Java probe
const std::array<UnitStepRow, 6> kUnitStepTable{{
    {"mm",
     {{{0.0, 1.0, -1.0, 0.0},
       {1.0E-4, 1.0001, -0.9999, 0.0},
       {0.001, 1.001, -0.999, 0.0},
       {0.0123, 1.0123, -0.9877, 0.0},
       {0.1, 1.1, -0.9, 0.1},
       {0.5, 1.5, -0.5, 0.5},
       {1.0, 2.0, 0.0, 1.0},
       {1.23, 2.23, 0.22999999999999998, 1.2},
       {9.99, 10.99, 8.99, 10.0},
       {12.3, 13.3, 11.3, 12.0},
       {99.0, 100.0, 98.0, 99.0},
       {123.0, 124.0, 122.0, 120.0},
       {1234.0, 1235.0, 1233.0, 1200.0},
       {-0.5, 0.5, -1.5, -0.5},
       {-12.3, -11.3, -13.3, -12.3}}}},
    {"cm",
     {{{0.0, 1.0, -1.0, 0.0},
       {1.0E-4, 1.0001, -0.9999, 0.0},
       {0.001, 1.001, -0.999, 0.0},
       {0.0123, 1.0123, -0.9877, 0.0},
       {0.1, 1.1, -0.9, 0.1},
       {0.5, 1.5, -0.5, 0.5},
       {1.0, 2.0, 0.0, 1.0},
       {1.23, 2.23, 0.22999999999999998, 1.2},
       {9.99, 10.99, 8.99, 10.0},
       {12.3, 13.3, 11.3, 12.0},
       {99.0, 100.0, 98.0, 99.0},
       {123.0, 124.0, 122.0, 120.0},
       {1234.0, 1235.0, 1233.0, 1200.0},
       {-0.5, 0.5, -1.5, -0.5},
       {-12.3, -11.3, -13.3, -12.3}}}},
    {"m",
     {{{0.0, 1.0, -1.0, 0.0},
       {1.0E-4, 1.0001, -0.9999, 0.0},
       {0.001, 1.001, -0.999, 0.0},
       {0.0123, 1.0123, -0.9877, 0.0},
       {0.1, 1.1, -0.9, 0.1},
       {0.5, 1.5, -0.5, 0.5},
       {1.0, 2.0, 0.0, 1.0},
       {1.23, 2.23, 0.22999999999999998, 1.2},
       {9.99, 10.99, 8.99, 10.0},
       {12.3, 13.3, 11.3, 12.0},
       {99.0, 100.0, 98.0, 99.0},
       {123.0, 124.0, 122.0, 120.0},
       {1234.0, 1235.0, 1233.0, 1200.0},
       {-0.5, 0.5, -1.5, -0.5},
       {-12.3, -11.3, -13.3, -12.3}}}},
    {"in",
     {{{0.0, 1.0, -1.0, 0.0},
       {1.0E-4, 1.0001, -0.9999, 0.0},
       {0.001, 1.001, -0.999, 0.0},
       {0.0123, 1.0123, -0.9877, 0.0},
       {0.1, 1.1, -0.9, 0.1},
       {0.5, 1.5, -0.5, 0.5},
       {1.0, 2.0, 0.0, 1.0},
       {1.23, 2.23, 0.22999999999999998, 1.2},
       {9.99, 10.99, 8.99, 10.0},
       {12.3, 13.3, 11.3, 12.0},
       {99.0, 100.0, 98.0, 99.0},
       {123.0, 124.0, 122.0, 120.0},
       {1234.0, 1235.0, 1233.0, 1200.0},
       {-0.5, 0.5, -1.5, -0.5},
       {-12.3, -11.3, -13.3, -12.3}}}},
    {"in/64",
     {{{0.0, 0.0625, -0.0625, 0.0},
       {1.0E-4, 0.0625, -0.0625, 0.0},
       {0.001, 0.0625, -0.0625, 0.0},
       {0.0123, 0.0625, 0.0, 0.015625},
       {0.1, 0.125, 0.0625, 0.09375},
       {0.5, 0.5625, 0.4375, 0.5},
       {1.0, 1.0625, 0.9375, 1.0},
       {1.23, 1.25, 1.1875, 1.234375},
       {9.99, 10.0, 9.9375, 9.984375},
       {12.3, 12.3125, 12.25, 12.296875},
       {99.0, 99.0625, 98.9375, 99.0},
       {123.0, 123.0625, 122.9375, 123.0},
       {1234.0, 1234.0625, 1233.9375, 1234.0},
       {-0.5, -0.4375, -0.5625, -0.5},
       {-12.3, -12.25, -12.3125, -12.296875}}}},
    {"ft",
     {{{0.0, 1.0, -1.0, 0.0},
       {1.0E-4, 1.0001, -0.9999, 0.0},
       {0.001, 1.001, -0.999, 0.0},
       {0.0123, 1.0123, -0.9877, 0.0},
       {0.1, 1.1, -0.9, 0.1},
       {0.5, 1.5, -0.5, 0.5},
       {1.0, 2.0, 0.0, 1.0},
       {1.23, 2.23, 0.22999999999999998, 1.2},
       {9.99, 10.99, 8.99, 10.0},
       {12.3, 13.3, 11.3, 12.0},
       {99.0, 100.0, 98.0, 99.0},
       {123.0, 124.0, 122.0, 120.0},
       {1234.0, 1235.0, 1233.0, 1200.0},
       {-0.5, 0.5, -1.5, -0.5},
       {-12.3, -11.3, -13.3, -12.3}}}},
}};
// NOLINTEND(modernize-use-designated-initializers)

/// The differences between the probe's table of the units' steps and the units', one line each.
[[nodiscard]] std::string unitStepDifferences()
{
    std::string differences;
    for (const UnitStepRow& row : kUnitStepTable)
    {
        const Unit& unit = lengthUnit(row.unit);
        for (const UnitStep& step : row.steps)
        {
            const double next     = unit.getNextValue(step.value);
            const double previous = unit.getPreviousValue(step.value);
            const double rounded  = unit.round(step.value);
            if (next != step.next || previous != step.previous || rounded != step.rounded)
            {
                differences +=
                    std::format("{} {}: {}/{}/{}, OpenRocket {}/{}/{}\n", row.unit, step.value,
                                next, previous, rounded, step.next, step.previous, step.rounded);
            }
        }
    }
    return differences;
}

/// Puts the bean back to the probe's radius 0.0125 m, as the probe did before each text, and has
/// the selection read it again.
void resetBean(BeanProperty& bean, DoubleBinding& model)
{
    bean.value = 0.0125;
    bean.sets  = 0;
    model.getSelection()->refresh();
}

/// The texts of @p texts that @p model did not refuse as it should (it accepted them, called the
/// setter, or changed the value or the text shown), one line each; the bean is reset before each.
[[nodiscard]] std::string notRefused(BeanProperty& bean, DoubleBinding& model,
                                     std::span<const std::string_view> texts)
{
    std::string problems;
    for (const std::string_view text : texts)
    {
        resetBean(bean, model);
        const std::string shown    = model.text();
        const bool        accepted = model.commitText(text);
        if (accepted || bean.sets != 0 || bean.value != 0.0125 || model.text() != shown)
        {
            problems += std::format("[{}]: accepted {}, setter calls {}, radius {}, text {}\n",
                                    text, accepted, bean.sets, bean.value, model.text());
        }
    }
    return problems;
}

/// A widget that writes back to its binding whenever the binding tells it of a change, as a
/// widget refreshing itself would: each of its writes must be dropped.
class EchoingWidget
{
public:
    explicit EchoingWidget(DoubleBinding& model)
      : m_model(&model), m_connection(model.addChangeListener([this] { echo(); }))
    {
    }

    [[nodiscard]] int  calls() const noexcept { return m_calls; }
    [[nodiscard]] bool anyWriteAccepted() const noexcept { return m_accepted; }
    [[nodiscard]] bool alwaysFiring() const noexcept { return m_alwaysFiring; }

private:
    void echo()
    {
        ++m_calls;
        m_alwaysFiring = m_alwaysFiring && m_model->isFiring();
        const bool accepted =
            m_model->commitText("40") || m_model->setUnitValue(41) || m_model->stepUp();
        m_accepted = m_accepted || accepted;
        m_model->setSliderPosition(SliderMapping::linear(0, 1), 900);
        m_model->setAutomatic(true);
    }

    DoubleBinding*             m_model;
    int                        m_calls{0};
    bool                       m_accepted{false};
    bool                       m_alwaysFiring{true};
    Signal<>::ScopedConnection m_connection;
};

/// A body tube's length without a setter.
[[nodiscard]] DoubleSource readOnlyLength()
{
    return DoubleSource::of<BodyTube>(
        {.get = [](const BodyTube& tube) { return tube.getLength(); }});
}

/// A switch a test flips.
struct Switch
{
    bool on{false};
};

/// A body tube's length, enabled while @p enabled is on.
[[nodiscard]] DoubleSource lengthEnabledWhile(const Switch& enabled)
{
    return DoubleSource::of<BodyTube>({
        .get       = [](const BodyTube& tube) { return tube.getLength(); },
        .set       = [](BodyTube& tube, double length) { tube.setLength(length); },
        .isEnabled = [&enabled](const BodyTube&) { return enabled.on; },
    });
}

/// The override CD of any component as the Override tab shows it (Java:
/// component.getOverrideCD()), at the Mach number @p mach.
[[nodiscard]] DoubleSource overrideCD(double mach)
{
    return DoubleSource::of<RocketComponent>({
        .getChecked = [mach](const RocketComponent& component) -> Result<double> {
            return ComponentDrag::getOverrideCD(component, mach);
        },
        .set = [](RocketComponent& component, double cd) { component.setOverrideCD(cd); },
    });
}

/// The differences between the probe's spinner table and @p model's, one line each.
[[nodiscard]] std::string spinnerTableDifferences(BeanProperty& bean, DoubleBinding& model)
{
    std::string differences;
    const auto  differ = [&differences](std::string_view what, const auto& got,
                                        const auto& expected) {
        if (got != expected)
        {
            differences += std::format("{}: {}, OpenRocket {}\n", what, got, expected);
        }
    };
    for (const SpinnerUnitRow& row : kSpinnerTable)
    {
        model.setCurrentUnit(lengthUnit(row.unit));
        resetBean(bean, model);
        const std::string unit(row.unit);
        differ(unit + " text", model.text(), std::string(row.text));
        differ(unit + " next", valueOf(model.nextValue()), row.next);
        differ(unit + " previous", valueOf(model.previousValue()), row.previous);
        for (const CommitRow& commit : row.commits)
        {
            resetBean(bean, model);
            const std::string what = unit + " [" + std::string(commit.text) + "]";
            differ(what + " accepted", model.commitText(commit.text), true);
            differ(what + " radius", bean.value, commit.radius);
            differ(what + " setter calls", bean.sets, 1);
            differ(what + " text", model.text(), std::string(commit.shown));
        }
    }
    return differences;
}

}  // namespace

// Port of DoubleModelTest.externalChangeSourceUpdatesModelListeners: a change made outside the
// binding (here a setter of the component) reaches its listeners once.
TEST_F(DoubleBindingTest, ExternalChangeSourceUpdatesModelListeners)
{
    const TwoTubeRocket design;
    design.tubeA->setLength(1.0);
    DoubleBinding       model(std::make_shared<ComponentSelection>(*design.tubeA), bodyTubeLength(),
                              unitGroup(UnitGroupId::NONE), {});
    const ChangeCounter eventCount(model);

    design.tubeA->setLength(4.2);

    EXPECT_EQ(model.value(), 4.2);
    EXPECT_EQ(eventCount.count(), 1);
}

// Port of DoubleModelTest.spinnerUsesCustomIncrementInSelectedUnit: getSpinnerModel(0.1) steps by
// 0.1 of the current unit.
TEST_F(DoubleBindingTest, SpinnerUsesCustomIncrementInSelectedUnit)
{
    constexpr double kEpsilon = 1.0e-12;
    DoubleBinding    model(0.01, lengthUnits(), 0);
    const Unit&      centimeters = lengthUnit("cm");
    const Unit&      inches      = lengthUnit("in");

    model.setCurrentUnit(centimeters);
    EXPECT_NEAR(1.1, valueOf(model.nextValue(0.1)), kEpsilon);
    EXPECT_NEAR(0.9, valueOf(model.previousValue(0.1)), kEpsilon);
    model.setUnitValue(valueOf(model.nextValue(0.1)));
    EXPECT_NEAR(0.011, model.value(), kEpsilon);

    model.setCurrentUnit(inches);
    const double valueInInches = inches.toUnit(model.value());
    EXPECT_NEAR(valueInInches + 0.1, valueOf(model.nextValue(0.1)), kEpsilon);
    EXPECT_NEAR(valueInInches - 0.1, valueOf(model.previousValue(0.1)), kEpsilon);
    model.setUnitValue(valueOf(model.nextValue(0.1)));
    EXPECT_NEAR(0.011 + inches.fromUnit(0.1), model.value(), kEpsilon);
}

TEST_F(DoubleBindingTest, TheSpinnerIsOpenRocketsInEveryLengthUnit)
{
    // Probe: the text, the steps and the five valid texts in mm, cm, in, in/64 and ft.
    const TwoTubeRocket                  design;
    BeanProperty                         bean;
    const std::shared_ptr<DoubleBinding> model = probeModel(bean, *design.tubeA);
    EXPECT_EQ(spinnerTableDifferences(bean, *model), "");
}

TEST_F(DoubleBindingTest, TheSpinnerStepsByTheUnitsOwnStepAsOpenRocket)
{
    // Probe: next, previous and round() of 15 values in mm, cm, m, in, in/64 and ft. The spin box
    // steps by the current unit's getNextValue() and getPreviousValue() (+-1 in the unit, the next
    // 1/16 in in/64), the slider rounds by its round().
    EXPECT_EQ(unitStepDifferences(), "");
}

TEST_F(DoubleBindingTest, AUnitWithANegativeMultiplierStepsTheOtherWay)
{
    // No unit of OpenRocket's groups has a negative multiplier; Java's spinner model handles one:
    // the bound a step goes towards is the minimum, and the other way round.
    UnitGroup negative;
    negative.addUnit(std::make_unique<GeneralUnit>(-1, "neg"));
    DoubleBinding binding(10.0, negative, 0, 10);
    EXPECT_EQ(binding.text(), "-10");
    EXPECT_EQ(binding.previousValue(), std::nullopt);
    EXPECT_EQ(valueOf(binding.nextValue()), -9.0);
    EXPECT_TRUE(binding.stepUp());
    EXPECT_EQ(binding.value(), 9.0);
    binding.setValue(0.5);
    EXPECT_EQ(valueOf(binding.nextValue()), 0.0);
    binding.setValue(0);
    EXPECT_EQ(binding.nextValue(), std::nullopt);
    EXPECT_EQ(valueOf(binding.previousValue()), -1.0);
}

TEST_F(DoubleBindingTest, TheSpinnerStopsAtTheMaximumAndIsClampedToIt)
{
    // Probe, in mm: at the minimum (none: -1 is fine), at the maximum (no next value) and just
    // below it (clamped to it).
    const TwoTubeRocket                  design;
    BeanProperty                         bean;
    const std::shared_ptr<DoubleBinding> model = probeModel(bean, *design.tubeA);
    model->setCurrentUnit(lengthUnit("mm"));
    bean.value = 0.0;
    model->getSelection()->refresh();
    EXPECT_EQ(valueOf(model->previousValue()), -1.0);
    EXPECT_EQ(valueOf(model->nextValue()), 1.0);
    bean.value = 0.5;
    model->getSelection()->refresh();
    EXPECT_EQ(valueOf(model->previousValue()), 999.0);
    EXPECT_EQ(model->nextValue(), std::nullopt);
    EXPECT_FALSE(model->stepUp());
    bean.value = 0.49999;
    model->getSelection()->refresh();
    EXPECT_EQ(valueOf(model->nextValue()), 1000.0);
    EXPECT_TRUE(model->stepUp());
    EXPECT_EQ(bean.value, 0.5);
}

TEST_F(DoubleBindingTest, TextThatIsNoNumberSetsNothing)
{
    // OpenRocket sets the last value again (probe: sets=1 for "abc", "5 cm", "1e2" and ""), which
    // a multi-edit forwards to the other components; the user's decision U3: nothing is set.
    const TwoTubeRocket design;
    BeanProperty        bean;
    bean.value                                  = 0.0125;
    const std::shared_ptr<DoubleBinding>  model = probeModel(bean, *design.tubeA);
    const ChangeCounter                   changes(*model);
    const std::array<std::string_view, 6> texts{"abc", "5 cm", "1e2", "", "3-", "pi"};
    EXPECT_EQ(notRefused(bean, *model, texts), "");
    EXPECT_EQ(model->text(), "2.5");
    EXPECT_EQ(changes.count(), 0);
}

TEST_F(DoubleBindingTest, ANaNOrInfiniteResultIsInvalid)
{
    // "0/0" is NaN, invalid in Java too; "1/0" is Infinity, which Java clamps to the maximum (or
    // stores in an unbounded field): the user's decision U4 makes it invalid.
    const TwoTubeRocket                   design;
    BeanProperty                          bean;
    const std::shared_ptr<DoubleBinding>  model = probeModel(bean, *design.tubeA);
    const std::array<std::string_view, 4> texts{"0/0", "1/0", "-1/0", "2^2000"};
    EXPECT_EQ(notRefused(bean, *model, texts), "");

    // A number that the unit makes infinite.
    UnitGroup huge;
    huge.addUnit(std::make_unique<GeneralUnit>(1e300, "huge"));
    DoubleBinding big(1.0, huge);
    EXPECT_FALSE(big.commitText("10^300"));
    EXPECT_EQ(big.value(), 1.0);
    EXPECT_TRUE(big.commitText("2"));
    EXPECT_EQ(big.value(), 2e300);
    EXPECT_FALSE(big.setUnitValue(1e300));
    EXPECT_FALSE(big.setUnitValue(kInfinity));
    EXPECT_FALSE(big.setUnitValue(std::numeric_limits<double>::quiet_NaN()));
}

TEST_F(DoubleBindingTest, TheValueIsReadOncePerEvent)
{
    // Qt asks a spin box for its state on every paint; the override CD's getter is a force
    // analysis.
    const TwoTubeRocket                  design;
    BeanProperty                         bean;
    const std::shared_ptr<DoubleBinding> model = probeModel(bean, *design.tubeA);
    EXPECT_EQ(bean.gets, 1);
    for (int i = 0; i < 10; ++i)
    {
        std::ignore = model->value();
        std::ignore = model->text();
        std::ignore = model->nextValue();
        std::ignore = model->previousValue();
        std::ignore = model->isAutomatic();
    }
    EXPECT_EQ(bean.gets, 1);
    design.tubeB->setLength(0.7);
    EXPECT_EQ(bean.gets, 2);
}

TEST_F(DoubleBindingTest, OnlyAChangeOfWhatIsShownIsTold)
{
    const TwoTubeRocket                  design;
    BeanProperty                         bean;
    const std::shared_ptr<DoubleBinding> model = probeModel(bean, *design.tubeA);
    const ChangeCounter                  changes(*model);
    design.tubeB->setLength(0.7);
    EXPECT_EQ(changes.count(), 0);
    EXPECT_TRUE(model->commitText("30"));
    EXPECT_EQ(changes.count(), 1);
    // The same value written differently changes nothing and is not told: the widget shows text()
    // again itself.
    EXPECT_TRUE(model->commitText("3*10"));
    EXPECT_EQ(bean.sets, 2);
    EXPECT_EQ(changes.count(), 1);
    bean.automatic = true;
    model->getSelection()->refresh();
    EXPECT_EQ(changes.count(), 2);
}

TEST_F(DoubleBindingTest, ValuesAreClampedToTheBoundsOfTheMultipliedValue)
{
    // Probe: "2000" mm on a diameter with a maximum of 1 m sets the radius 0.5 m.
    const TwoTubeRocket design;
    DoubleBinding       diameter(std::make_shared<ComponentSelection>(*design.tubeA),
                                 bodyTubeOuterRadius(), lengthUnits(),
                                 {.multiplier = 2, .min = 0.01, .max = 0.1});
    EXPECT_EQ(diameter.value(), 0.04);
    EXPECT_EQ(diameter.minimum(), 0.01);
    EXPECT_EQ(diameter.maximum(), 0.1);
    diameter.setValue(0.5);
    EXPECT_EQ(design.tubeA->getOuterRadius(), 0.05);
    EXPECT_EQ(diameter.value(), 0.1);
    diameter.setValue(0.0);
    EXPECT_EQ(design.tubeA->getOuterRadius(), 0.005);
    EXPECT_EQ(diameter.getMultiplier(), 2.0);
}

TEST_F(DoubleBindingTest, ABoundBindingIsADynamicBound)
{
    // Java: a DoubleModel given as the bound (the inner diameter up to the outer diameter).
    const TwoTubeRocket design;
    const auto          selection = std::make_shared<ComponentSelection>(*design.tubeA);
    const auto          outer     = std::make_shared<DoubleBinding>(
        selection, bodyTubeOuterRadius(), lengthUnits(), DoubleBinding::Options{.multiplier = 2});
    DoubleBinding       inner(selection, bodyTubeInnerRadius(), lengthUnits(),
                              {.multiplier = 2, .min = 0, .maxBinding = outer});
    const ChangeCounter changes(inner);
    EXPECT_EQ(inner.maximum(), 0.04);
    EXPECT_EQ(inner.value(), design.tubeA->getInnerRadius() * 2);
    EXPECT_TRUE(inner.setUnitValue(10));  // cm, beyond the outer diameter
    EXPECT_EQ(design.tubeA->getInnerRadius(), 0.02);
    EXPECT_EQ(inner.nextValue(), std::nullopt);
    EXPECT_TRUE(inner.setUnitValue(2));
    EXPECT_EQ(design.tubeA->getInnerRadius(), 0.01);

    // The outer diameter grows: the inner one's bound follows and its listeners hear of it.
    const int before = changes.count();
    outer->setValue(0.08);
    EXPECT_EQ(inner.maximum(), 0.08);
    EXPECT_GT(changes.count(), before);
    EXPECT_TRUE(inner.nextValue().has_value());
}

TEST_F(DoubleBindingTest, TheAutomaticStateIsTheSourcesAndTypingSwitchesItOff)
{
    // Probe: the automatic action of a bean with isValueAutomatic/setValueAutomatic is enabled,
    // and putValue(SELECTED_KEY, true) sets it.
    const TwoTubeRocket design;
    DoubleBinding radius(std::make_shared<ComponentSelection>(*design.tubeA), bodyTubeOuterRadius(),
                         lengthUnits(), {.multiplier = 2});
    EXPECT_TRUE(radius.isAutomaticAvailable());
    EXPECT_FALSE(radius.isAutomatic());
    radius.setAutomatic(true);
    EXPECT_TRUE(design.tubeA->isOuterRadiusAutomatic());
    EXPECT_TRUE(radius.isAutomatic());
    // BodyTube::setOuterRadius() switches the automatic radius off.
    EXPECT_TRUE(radius.commitText("5"));
    EXPECT_FALSE(design.tubeA->isOuterRadiusAutomatic());
    EXPECT_FALSE(radius.isAutomatic());

    DoubleBinding length(std::make_shared<ComponentSelection>(*design.tubeA), bodyTubeLength(),
                         lengthUnits(), {});
    EXPECT_FALSE(length.isAutomaticAvailable());
    EXPECT_FALSE(length.isAutomatic());
    length.setAutomatic(true);
    EXPECT_FALSE(length.isAutomatic());
}

TEST_F(DoubleBindingTest, TheCurrentUnitIsTheDefaultAtFirstAndThenTheBindingsOwn)
{
    const QtRocket::Test::DefaultUnitsGuard units;
    UnitGroup&                              length = unitGroup(UnitGroupId::LENGTH);
    ASSERT_TRUE(length.setDefaultUnit("mm"));
    DoubleBinding       first(0.025, length);
    const DoubleBinding second(0.025, length);
    EXPECT_EQ(first.getCurrentUnit().getUnit(), "mm");
    EXPECT_EQ(first.text(), "25");

    const ChangeCounter changes(first);
    first.setCurrentUnit(lengthUnit("in"));
    EXPECT_EQ(changes.count(), 1);
    EXPECT_EQ(first.text(), "0.984");
    first.setCurrentUnit(*length.getUnit("in"));
    EXPECT_EQ(changes.count(), 1);
    EXPECT_EQ(second.getCurrentUnit().getUnit(), "mm");
    EXPECT_EQ(length.getDefaultUnit().getUnit(), "mm");

    ASSERT_TRUE(length.setDefaultUnit("m"));
    EXPECT_EQ(second.getCurrentUnit().getUnit(), "mm");

    const GeneralUnit foreign(0.5, "half");
    EXPECT_THROW(first.setCurrentUnit(foreign), BugError);
}

TEST_F(DoubleBindingTest, WidgetWritesAreDroppedWhileTheBindingTellsItsListeners)
{
    const TwoTubeRocket design;
    BeanProperty        bean;
    const auto          model = probeModel(bean, *design.tubeA);
    const EchoingWidget widget(*model);
    EXPECT_TRUE(model->commitText("30"));
    EXPECT_EQ(widget.calls(), 1);
    EXPECT_TRUE(widget.alwaysFiring());
    EXPECT_FALSE(widget.anyWriteAccepted());
    EXPECT_FALSE(model->isFiring());
    EXPECT_EQ(bean.value, 0.15);  // 30 cm, a diameter
    EXPECT_FALSE(bean.automatic);
    EXPECT_EQ(bean.sets, 1);
}

TEST_F(DoubleBindingTest, AReadOnlySourceCannotBeSet)
{
    // Java: BugException "setMethod not available".
    const TwoTubeRocket design;
    DoubleBinding length(std::make_shared<ComponentSelection>(*design.tubeA), readOnlyLength(),
                         lengthUnits(), {});
    EXPECT_FALSE(length.isWritable());
    EXPECT_EQ(length.value(), 0.3);
    EXPECT_THROW(length.setValue(0.4), BugError);
    EXPECT_THROW(std::ignore = length.commitText("40"), BugError);
    EXPECT_TRUE(DoubleBinding(1.0).isWritable());
}

TEST_F(DoubleBindingTest, TheEnabledStateIsTheSources)
{
    // Java's BooleanModel reads is<Name>Enabled (Transition.isClippedEnabled()).
    const TwoTubeRocket design;
    Switch              enabled;
    DoubleBinding       length(std::make_shared<ComponentSelection>(*design.tubeA),
                               lengthEnabledWhile(enabled), lengthUnits(), {});
    const ChangeCounter changes(length);
    EXPECT_FALSE(length.isEnabled());
    enabled.on = true;
    length.getSelection()->refresh();
    EXPECT_TRUE(length.isEnabled());
    EXPECT_EQ(changes.count(), 1);
}

TEST_F(DoubleBindingTest, AProblemTextTakesThePlaceOfTheValue)
{
    // The override CD of a stage selection that has no force analysis (an active stage below two
    // inactive ones): ComponentDrag::getOverrideCD() gives an Error whose text the field shows (M1
    // leftover 2).
    const QtRocket::Test::TestBoostersOnBoostersRocket nested;
    const DoubleBinding cd(std::make_shared<ComponentSelection>(*nested.innerFins), overrideCD(0.3),
                           unitGroup(UnitGroupId::COEFFICIENT), {});
    EXPECT_FALSE(cd.getProblem().has_value());
    EXPECT_GT(cd.value(), 0.0);

    nested.rocket->getSelectedConfiguration().setOnlyStage(
        QtRocket::Test::TestBoostersOnBoostersRocket::kInnerStageNumber);
    cd.getSelection()->refresh();
    EXPECT_EQ(cd.getProblem().value_or(""),
              "The force analysis cannot be made for this selection of stages: stage 'Inner "
              "Boosters' is active below two inactive stages ('Booster Stage' and 'Core Stage').");
    EXPECT_EQ(cd.text(), cd.getProblem().value_or(""));
    EXPECT_EQ(cd.nextValue(), std::nullopt);
    EXPECT_EQ(cd.previousValue(), std::nullopt);

    // Overridden, the CD is the stored one: no analysis, no problem.
    nested.innerFins->setOverrideCD(0.2);
    nested.innerFins->setCDOverridden(true);
    EXPECT_FALSE(cd.getProblem().has_value());
    EXPECT_EQ(cd.value(), 0.2);
    EXPECT_EQ(cd.text(), "0.200");
}

TEST_F(DoubleBindingTest, AnInvalidSelectionShowsAndAcceptsNothing)
{
    // A dialog's last focus-out commit can come after the TREE or UNDO change that destroyed the
    // components: it reaches none of them (proved under ASan).
    TwoTubeRocket      design;
    OpenRocketDocument document(std::move(design.owner));
    BeanProperty       bean;
    bean.value                = 0.0125;
    const auto          model = probeModel(bean, *design.tubeA);
    const ChangeCounter changes(*model);
    document.addUndoPosition("Modify body tube");
    EXPECT_TRUE(model->commitText("30"));
    document.undo();
    EXPECT_FALSE(model->isValid());
    EXPECT_EQ(changes.count(), 2);
    EXPECT_EQ(model->text(), "");
    EXPECT_FALSE(model->isEnabled());
    EXPECT_EQ(model->value(), 0.3);  // the last value read: 30 cm
    EXPECT_EQ(model->nextValue(), std::nullopt);
    EXPECT_EQ(model->previousValue(), std::nullopt);
    const int sets = bean.sets;
    EXPECT_FALSE(model->commitText("40"));
    EXPECT_FALSE(model->setUnitValue(41));
    EXPECT_FALSE(model->stepDown());
    model->setValue(0.05);
    model->setSliderPosition(SliderMapping::linear(0, 1), 500);
    model->setAutomatic(true);
    EXPECT_EQ(bean.sets, sets);
    EXPECT_FALSE(bean.automatic);
    EXPECT_EQ(changes.count(), 2);
}

TEST_F(DoubleBindingTest, AListenerMayDropTheLastOwnerOfTheBindingAndItsSelection)
{
    // A dialog that closes when it is told of a change, or of the end of its selection, destroys
    // its bindings: each keeps itself, and the selection itself, alive until it has told every
    // listener (proved under ASan).
    TwoTubeRocket                  design;
    OpenRocketDocument             document(std::move(design.owner));
    std::shared_ptr<DoubleBinding> binding =
        std::make_shared<DoubleBinding>(std::make_shared<ComponentSelection>(*design.tubeA),
                                        bodyTubeLength(), lengthUnits(), DoubleBinding::Options{});
    int drops   = 0;
    std::ignore = binding->addChangeListener([&binding, &drops] {
        ++drops;
        binding.reset();
    });
    EXPECT_TRUE(binding->commitText("40"));
    EXPECT_EQ(binding, nullptr);
    EXPECT_EQ(drops, 1);
    EXPECT_EQ(design.tubeA->getLength(), 0.4);

    binding =
        std::make_shared<DoubleBinding>(std::make_shared<ComponentSelection>(*design.tubeB),
                                        bodyTubeLength(), lengthUnits(), DoubleBinding::Options{});
    std::ignore = binding->addChangeListener([&binding, &drops] {
        ++drops;
        binding.reset();
    });
    document.undo();
    EXPECT_EQ(binding, nullptr);
    EXPECT_EQ(drops, 2);
}

TEST_F(DoubleBindingTest, ABindingNeedsASelectionAndAUsableMultiplier)
{
    const TwoTubeRocket design;
    EXPECT_THROW(DoubleBinding(nullptr, bodyTubeLength(), lengthUnits(), {}), BugError);
    const auto selection = std::make_shared<ComponentSelection>(*design.tubeA);
    EXPECT_THROW(DoubleBinding(selection, bodyTubeLength(), lengthUnits(), {.multiplier = 0}),
                 BugError);
    EXPECT_THROW(
        DoubleBinding(selection, bodyTubeLength(), lengthUnits(), {.multiplier = kInfinity}),
        BugError);
    // A body tube's source on a nose cone.
    EXPECT_THROW(DoubleBinding(std::make_shared<ComponentSelection>(*design.nose), bodyTubeLength(),
                               lengthUnits(), {}),
                 BugError);
    // Java: IllegalArgumentException for a step that is not finite and positive.
    const DoubleBinding length(selection, bodyTubeLength(), lengthUnits(), {});
    EXPECT_THROW(std::ignore = length.nextValue(0.0), BugError);
    EXPECT_THROW(std::ignore = length.previousValue(-0.1), BugError);
}

TEST_F(DoubleBindingTest, TheSliderSetsTheRoundedValue)
{
    // Probe: the diameter model's slider (0, 0.04, 0.2) in mm: position 750 gives 0.09 m and
    // shows 750; a diameter of 0.05 m shows 583, 0.1 m 779.
    const TwoTubeRocket design;
    BeanProperty        bean;
    bean.value                  = 0.0125;
    const auto          model   = probeModel(bean, *design.tubeA);
    const SliderMapping mapping = SliderMapping::fromRange(0, 0.04, 0.2);
    model->setCurrentUnit(lengthUnit("mm"));
    model->setSliderPosition(mapping, 750);
    EXPECT_EQ(bean.value * 2, 0.09);
    EXPECT_EQ(model->sliderPosition(mapping), 750);
    model->setSliderPosition(mapping, 501);
    EXPECT_EQ(bean.value * 2, 0.04);
    EXPECT_EQ(model->sliderPosition(mapping), 500);
    bean.value = 0.05 / 2;
    model->getSelection()->refresh();
    EXPECT_EQ(model->sliderPosition(mapping), 583);
    bean.value = 0.1 / 2;
    model->getSelection()->refresh();
    EXPECT_EQ(model->sliderPosition(mapping), 779);
    bean.value = 0.3 / 2;
    model->getSelection()->refresh();
    EXPECT_EQ(model->sliderPosition(mapping), SliderMapping::kMaximum);
}

}  // namespace QtRocket::Gui
