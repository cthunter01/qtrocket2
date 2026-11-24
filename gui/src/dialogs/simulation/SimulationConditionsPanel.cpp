#include "QtRocket/gui/dialogs/simulation/SimulationConditionsPanel.h"

#include <array>
#include <cmath>
#include <functional>
#include <limits>
#include <memory>
#include <numbers>

#include <QEvent>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QObject>
#include <QPushButton>
#include <QRadioButton>
#include <QStackedWidget>
#include <QString>
#include <QVBoxLayout>
#include <QWidget>
#include <Qt>

#include "QtRocket/gui/binding/BindingContext.h"
#include "QtRocket/gui/binding/BoolBinding.h"
#include "QtRocket/gui/binding/DoubleBinding.h"
#include "QtRocket/gui/binding/ObjectSelection.h"
#include "QtRocket/gui/binding/PropertySource.h"
#include "QtRocket/gui/binding/SliderMapping.h"
#include "QtRocket/gui/configdialog/ConfigFields.h"
#include "QtRocket/gui/dialogs/simulation/TurbulenceIntensityText.h"
#include "QtRocket/gui/dialogs/simulation/WindLevelSummary.h"
#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/gui/util/GuiServices.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/util/UserPrompts.h"
#include "QtRocket/gui/widgets/BindingScope.h"
#include "QtRocket/gui/widgets/BoolCheckBox.h"
#include "QtRocket/gui/widgets/FieldValidator.h"
#include "QtRocket/gui/widgets/UnitSelector.h"
#include "QtRocket/gui/widgets/UnitSpinBox.h"
#include "QtRocket/gui/widgets/ValueSlider.h"
#include "QtRocket/models/ExtendedIsaModel.h"
#include "QtRocket/models/PinkNoiseWindModel.h"
#include "QtRocket/models/WindModelType.h"
#include "QtRocket/simulation/DefaultSimulationOptionFactory.h"
#include "QtRocket/simulation/SimulationInputRanges.h"
#include "QtRocket/simulation/SimulationOptions.h"
#include "QtRocket/simulation/SimulationOptionsInterface.h"
#include "QtRocket/unit/Unit.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/MathUtil.h"

namespace QtRocket::Gui
{

namespace
{

using Target = SimulationOptionsInterface;

/// The context of the panel's reports (Guard).
QString context()
{
    return QStringLiteral("Launch conditions");
}

/// MigLayout's related gap ("gap rel"), between the boxes.
constexpr int kRelatedGap = 6;
/// MigLayout's paragraph gap ("gapright para"), between the default buttons.
constexpr int kParagraphGap = 12;
/// The empty border Java gives the "Average" radio button on its right.
constexpr int kAverageButtonGap = 20;

// The validation's thresholds (Java's private statics of SimulationConditionsPanel).
constexpr double kHighWindSpeed            = 10;      // m/s
constexpr double kExtremeWindSpeed         = 20;      // m/s
constexpr double kLowTemperature           = 233.15;  // K, -40 degrees C
constexpr double kHighTemperature          = 323.15;  // K, 50 degrees C
constexpr double kExtremelyLowTemperature  = 193.15;  // K, -80 degrees C
constexpr double kExtremelyHighTemperature = 353.15;  // K, 80 degrees C
constexpr double kLowPressureRatio         = 0.75;
constexpr double kHighPressureRatio        = 1.25;
constexpr double kLargeRodAngle            = 45;  // degrees
constexpr double kMaxedRodAngle            = 60;  // degrees

/// A source of the double property of the class O read by @p get and written by @p set (Java:
/// DoubleModel(source, "<Name>") over get<Name>() and set<Name>()).
template <class O>
[[nodiscard]] DoubleSource doubleSource(double (O::*get)() const, void (O::*set)(double))
{
    return DoubleSource::ofObject<O>({
        .get = [get](const O& object) { return std::invoke(get, object); },
        .set = [set](O& object, const double& value) { std::invoke(set, object, value); },
    });
}

/// A source of the flag of the target read by @p get and written by @p set (Java:
/// BooleanModel(target, "<Name>")).
[[nodiscard]] BoolSource boolSource(bool (Target::*get)() const, void (Target::*set)(bool))
{
    return BoolSource::ofObject<Target>({
        .get = [get](const Target& target) { return std::invoke(get, target); },
        .set = [set](Target& target, const bool& value) { std::invoke(set, target, value); },
    });
}

/// The widgets of @p row: its label, spin box, unit selector and slider (null for a part it has
/// not).
[[nodiscard]] std::array<QWidget*, 4> rowWidgets(const ConfigFields::NumberWidgets& row)
{
    return {row.label, row.spinBox, row.unitSelector, row.slider};
}

/// @p row's widgets get the tooltip @p tip.
void setRowToolTip(const ConfigFields::NumberWidgets& row, const QString& tip)
{
    for (QWidget* const widget : rowWidgets(row))
    {
        if (widget != nullptr)
        {
            widget->setToolTip(tip);
        }
    }
}

/// @p row's widgets follow @p checkBox: enabled while its value is @p enableState (Java:
/// BooleanModel.addEnableComponent()).
void followCheckBox(BoolCheckBox& checkBox, const ConfigFields::NumberWidgets& row,
                    bool enableState)
{
    for (QWidget* const widget : rowWidgets(row))
    {
        if (widget != nullptr)
        {
            checkBox.addEnableFollower(*widget, enableState);
        }
    }
}

/// The pressure at the launch site over the standard atmosphere's at @p altitude (Java:
/// pressureRatio()).
[[nodiscard]] double pressureRatio(double pressure, double altitude,
                                   const ExtendedIsaModel& standardAtmosphere)
{
    return pressure / standardAtmosphere.getConditions(altitude).getPressure();
}

/// A grid of a box's number rows: the labels' column takes the width left (Java's columns
/// "[grow][80lp!][32lp!][72lp!]").
[[nodiscard]] QGridLayout* makeRowGrid(QWidget& owner)
{
    auto* grid = makeChild<QGridLayout>(&owner);
    grid->setColumnStretch(0, 1);
    return grid;
}

/// A titled box of the panel (Java: a JPanel with a TitledBorder).
[[nodiscard]] QGroupBox* makeBox(QWidget& panel, const QString& title, const QString& name)
{
    auto* box = makeChild<QGroupBox>(&panel, title);
    box->setObjectName(name);
    return box;
}

}  // namespace

SimulationConditionsPanel::SimulationConditionsPanel(SimulationOptionsInterface& target,
                                                     Variant variant, const GuiServices& services,
                                                     BindingScope& scope, QWidget* parent)
  : QWidget(parent), m_variant(variant), m_services(services)
{
    if (variant == Variant::SIMULATION)
    {
        m_options = dynamic_cast<SimulationOptions*>(&target);
        if (m_options == nullptr)
        {
            bug("The launch conditions of a simulation need its SimulationOptions");
        }
    }
    setObjectName(QStringLiteral("simulationConditionsPanel"));
    const Contexts contexts{
        .target      = scope.keepContext(ObjectSelection::make<SimulationOptionsInterface>(target)),
        .averageWind = scope.keepContext(
            ObjectSelection::make<PinkNoiseWindModel>(target.getAverageWindModel())),
    };
    m_targetSelection = contexts.target;

    buildWind(contexts, scope);
    buildAtmosphere(contexts, scope);
    buildLaunchSite(contexts, scope);
    buildLaunchRod(contexts, scope);
    buildValidators();
    if (m_variant == Variant::SIMULATION)
    {
        buildDefaultButtons();
    }
    layOut();
}

SimulationConditionsPanel::~SimulationConditionsPanel() = default;

// ------------------------------------------------------------------------------------- the wind

void SimulationConditionsPanel::buildWind(const Contexts& contexts, BindingScope& scope)
{
    //: simedtdlg.lbl.Wind
    m_windBox = makeBox(*this, tr("Wind"), QStringLiteral("windPanel"));
    if (m_variant == Variant::LAUNCH_PREFERENCES)
    {
        // Java: addAverageWindSettings() straight into the box.
        buildAverageWind(*m_windBox, contexts, scope);
        return;
    }
    // Java: addWindModelPanel(), a MigLayout("fillx") column: the choice, a line, the cards.
    auto* column = makeChild<QVBoxLayout>(m_windBox);
    // The label and the radio buttons on one row ("spanx, split 3, gapright para").
    auto* choiceRow = makeChild<QWidget>(m_windBox);
    auto* choice    = makeChild<QHBoxLayout>(choiceRow);
    choice->setContentsMargins(0, 0, 0, 0);
    //: simedtdlg.lbl.WindModelSelection
    choice->addWidget(makeChild<QLabel>(choiceRow, tr("Wind model to use:")));
    choice->addSpacing(kParagraphGap);
    buildWindModelChoice(*choiceRow);
    choice->addWidget(m_averageButton);
    choice->addSpacing(kAverageButtonGap);
    choice->addWidget(m_multiLevelButton);
    choice->addStretch(1);
    column->addWidget(choiceRow);

    auto* line = makeChild<QFrame>(m_windBox);
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    column->addWidget(line);

    m_windCards = makeChild<QStackedWidget>(m_windBox);
    m_windCards->setObjectName(QStringLiteral("windSettingsPanel"));
    auto* averageCard = makeChild<QWidget>(m_windCards);
    averageCard->setObjectName(QStringLiteral("averagePanel"));
    buildAverageWind(*averageCard, contexts, scope);
    m_windCards->addWidget(averageCard);
    m_windLevelSummary =
        makeChild<WindLevelSummary>(m_windCards, m_options->getMultiLevelWindModel());
    m_windLevelSummary->setObjectName(QStringLiteral("multiLevelPanel"));
    m_windCards->addWidget(m_windLevelSummary);
    connect(m_windLevelSummary, &WindLevelSummary::editLevelsRequested, this,
            &SimulationConditionsPanel::editWindLevelsRequested);
    column->addWidget(m_windCards);
    showWindModel();
}

void SimulationConditionsPanel::buildWindModelChoice(QWidget& parent)
{
    //: simedtdlg.radio.AverageWind
    m_averageButton = makeChild<QRadioButton>(&parent, tr("Average"));
    m_averageButton->setObjectName(QStringLiteral("averageButton"));
    //: simedtdlg.radio.AverageWind.ttip
    m_averageButton->setToolTip(
        tr("Model the wind as pink noise from the average wind speed and standard deviation."));
    //: simedtdlg.radio.MultiLevelWind
    m_multiLevelButton = makeChild<QRadioButton>(&parent, tr("Multi-level"));
    m_multiLevelButton->setObjectName(QStringLiteral("multiLevelButton"));
    //: simedtdlg.radio.MultiLevelWind.ttip
    m_multiLevelButton->setToolTip(
        tr("Model the wind using speed and direction entries at various altitude levels."));
    connect(m_averageButton, &QRadioButton::clicked, this,
            guarded([this] { chooseWindModel(false); }, context()));
    connect(m_multiLevelButton, &QRadioButton::clicked, this,
            guarded([this] { chooseWindModel(true); }, context()));
    // Java: the options' listener that keeps the choice in step with programmatic changes, such
    // as "Reset to default". The scope's release ends the selection, which then tells nothing.
    m_windModelConnection = m_targetSelection->addListener([this](BindingContext::Phase phase) {
        if (phase == BindingContext::Phase::NOTIFY)
        {
            showWindModel();
        }
    });
}

void SimulationConditionsPanel::buildAverageWind(QWidget& card, const Contexts& contexts,
                                                 BindingScope& scope)
{
    // Java: addAverageWindSettings(), its rows by addDoubleModelRow(): the tooltip on the spin
    // box alone.
    QGridLayout* grid = makeRowGrid(card);
    if (&card != m_windBox)
    {
        grid->setContentsMargins(0, 0, 0, 0);
    }
    const auto average = scope.make<DoubleBinding>(
        contexts.averageWind,
        doubleSource<PinkNoiseWindModel>(&PinkNoiseWindModel::getAverage,
                                         &PinkNoiseWindModel::setAverage),
        unitGroup(UnitGroupId::WINDSPEED),
        DoubleBinding::Options{.min = 0, .max = SimulationInputRanges::kWindSpeed.max});
    //: simedtdlg.lbl.Averwindspeed
    m_averageWind = ConfigFields::addNumberRow(*grid, 0, tr("Average windspeed:"), average,
                                               QStringLiteral("Average"),
                                               {.slider = SliderMapping::linear(0, 10)});
    //: simedtdlg.lbl.ttip.Averwindspeed
    m_averageWind.spinBox->setToolTip(tr("The average windspeed relative to the ground."));

    // At most the average (Java: the average's DoubleModel as the maximum, and as the slider's).
    const auto deviation = scope.make<DoubleBinding>(
        contexts.averageWind,
        doubleSource<PinkNoiseWindModel>(&PinkNoiseWindModel::getStandardDeviation,
                                         &PinkNoiseWindModel::setStandardDeviation),
        unitGroup(UnitGroupId::WINDSPEED), DoubleBinding::Options{.min = 0, .maxBinding = average});
    //: simedtdlg.lbl.Stddeviation
    m_standardDeviation = ConfigFields::addNumberRow(
        *grid, 1, tr("Standard deviation:"), deviation, QStringLiteral("StandardDeviation"),
        {.slider = SliderMapping::linear(0, average->value()),
         .bounds = ValueSlider::DynamicBounds{.min = nullptr, .max = average}});
    //: simedtdlg.lbl.ttip.Stddeviation
    m_standardDeviation.spinBox->setToolTip(
        tr("<html>The standard deviation of the windspeed. <br>The windspeed is within twice the "
           "standard deviation from the average for 95% of the time."));

    const auto turbulence = scope.make<DoubleBinding>(
        contexts.averageWind,
        doubleSource<PinkNoiseWindModel>(&PinkNoiseWindModel::getTurbulenceIntensity,
                                         &PinkNoiseWindModel::setTurbulenceIntensity),
        unitGroup(UnitGroupId::RELATIVE), DoubleBinding::Options{.min = 0, .max = 1});
    //: simedtdlg.lbl.Turbulenceintensity
    m_turbulenceIntensity = ConfigFields::addNumberRow(
        *grid, 2, tr("Turbulence intensity:"), turbulence, QStringLiteral("TurbulenceIntensity"),
        {.slider = SliderMapping::linear(0, 1)});
    const Unit&   percent = unitGroup(UnitGroupId::RELATIVE).getDefaultUnit();
    const QString turbulenceTip =
        //: simedtdlg.lbl.ttip.Turbulenceintensity1
        tr("<html>The turbulence intensity is the standard deviation divided by the average "
           "windspeed. <br>") +
        //: simedtdlg.lbl.ttip.Turbulenceintensity2
        tr("Typical values range from") + QStringLiteral(" ") +
        QtStrings::unitTextWithUnit(percent, 0.05) + QStringLiteral(" ") +
        //: simedtdlg.lbl.ttip.Turbulenceintensity3
        tr("to") + QStringLiteral(" ") + QtStrings::unitTextWithUnit(percent, 0.20) +
        QStringLiteral(".");
    m_turbulenceIntensity.spinBox->setToolTip(turbulenceTip);
    m_turbulenceIntensity.spinBox->installEventFilter(this);

    // The description, below the spin box (Java: "w 75lp, skip 1, wrap"), told of every change
    // of the model, as the binding is.
    m_intensityLabel = makeChild<QLabel>(&card);
    m_intensityLabel->setObjectName(QStringLiteral("intensityLabel"));
    m_intensityLabel->setToolTip(turbulenceTip);
    grid->addWidget(m_intensityLabel, 3, 1, 1, 2);
    const auto showIntensity = [this, selection = contexts.averageWind] {
        if (selection->isValid())
        {
            m_intensityLabel->setText(
                TurbulenceIntensityText::of(selection->getObject<PinkNoiseWindModel>()));
        }
    };
    showIntensity();
    m_intensityConnection = turbulence->addChangeListener(showIntensity);

    const auto direction = scope.make<DoubleBinding>(
        contexts.averageWind,
        doubleSource<PinkNoiseWindModel>(&PinkNoiseWindModel::getDirection,
                                         &PinkNoiseWindModel::setDirection),
        unitGroup(UnitGroupId::ANGLE),
        DoubleBinding::Options{.min = 0, .max = 2 * std::numbers::pi});
    //: simedtdlg.lbl.Winddirection
    m_windDirection = ConfigFields::addNumberRow(
        *grid, 4, tr("Wind direction:"), direction, QStringLiteral("Direction"),
        {.slider = SliderMapping::linear(0, 2 * std::numbers::pi)});
    //: simedtdlg.lbl.ttip.Winddirection
    m_windDirection.spinBox->setToolTip(
        tr("<html>Wind direction, 0-360 degrees.   <br>0 means "
           "from the north, <br> 90 is from the east"));
}

void SimulationConditionsPanel::showWindModel()
{
    const SimulationOptions* options = liveOptions();
    if (options == nullptr || m_windCards == nullptr)
    {
        return;
    }
    const bool multiLevel = options->getWindModelType() != WindModelType::AVERAGE;
    (multiLevel ? m_multiLevelButton : m_averageButton)->setChecked(true);
    m_windCards->setCurrentIndex(multiLevel ? 1 : 0);
}

void SimulationConditionsPanel::chooseWindModel(bool multiLevel)
{
    m_windCards->setCurrentIndex(multiLevel ? 1 : 0);
    if (SimulationOptions* options = liveOptions(); options != nullptr)
    {
        options->setWindModelType(multiLevel ? WindModelType::MULTI_LEVEL : WindModelType::AVERAGE);
    }
}

// ------------------------------------------------------------------------------ the atmosphere

void SimulationConditionsPanel::buildAtmosphere(const Contexts& contexts, BindingScope& scope)
{
    //: simedtdlg.border.Atmoscond
    m_atmosphereBox =
        makeBox(*this, tr("Atmospheric conditions"), QStringLiteral("atmospherePanel"));
    QGridLayout* grid = makeRowGrid(*m_atmosphereBox);

    const auto isa = scope.make<BoolBinding>(
        contexts.target, boolSource(&Target::isIsaAtmosphere, &Target::setIsaAtmosphere));
    //: simedtdlg.checkbox.InterStdAtmosphere
    m_isaCheckBox =
        makeChild<BoolCheckBox>(m_atmosphereBox, isa, tr("Use International Standard Atmosphere"));
    m_isaCheckBox->setObjectName(QStringLiteral("ISAAtmosphere"));
    m_isaCheckBox->setToolTip(
        //: simedtdlg.checkbox.ttip.InterStdAtmosphere1
        tr("<html>Select to use the International Standard Atmosphere model. <br>This model has a "
           "temperature of") +
        QStringLiteral(" ") +
        QtStrings::defaultUnitText(UnitGroupId::TEMPERATURE,
                                   ExtendedIsaModel::kStandardTemperature) +
        QStringLiteral(" ") +
        //: simedtdlg.checkbox.ttip.InterStdAtmosphere2
        tr("and a pressure of") + QStringLiteral(" ") +
        QtStrings::defaultUnitText(UnitGroupId::PRESSURE, ExtendedIsaModel::kStandardPressure) +
        QStringLiteral(" ") +
        //: simedtdlg.checkbox.ttip.InterStdAtmosphere3
        tr("at sea level."));
    grid->addWidget(m_isaCheckBox, 0, 0, 1, 4);
    // Java: "wrap unrel".
    grid->setRowMinimumHeight(1, kParagraphGap);

    const auto temperature = scope.make<DoubleBinding>(
        contexts.target,
        doubleSource<Target>(&Target::getLaunchTemperature, &Target::setLaunchTemperature),
        unitGroup(UnitGroupId::TEMPERATURE),
        DoubleBinding::Options{.min = 0, .max = SimulationInputRanges::kLaunchTemperature.max});
    //: simedtdlg.lbl.Temperature
    m_temperature = ConfigFields::addNumberRow(*grid, 2, tr("Temperature:"), temperature,
                                               QStringLiteral("LaunchTemperature"),
                                               {.slider = SliderMapping::linear(253.15, 308.15)});
    //: simedtdlg.lbl.ttip.Temperature
    setRowToolTip(m_temperature, tr("The temperature at the launch site."));

    const auto pressure = scope.make<DoubleBinding>(
        contexts.target,
        doubleSource<Target>(&Target::getLaunchPressure, &Target::setLaunchPressure),
        unitGroup(UnitGroupId::PRESSURE),
        DoubleBinding::Options{.min = 0.001, .max = SimulationInputRanges::kLaunchPressure.max});
    //: simedtdlg.lbl.Pressure
    m_pressure = ConfigFields::addNumberRow(*grid, 3, tr("Pressure:"), pressure,
                                            QStringLiteral("LaunchPressure"),
                                            {.slider = SliderMapping::linear(0.950e5, 1.050e5)});
    //: simedtdlg.lbl.ttip.Pressure
    setRowToolTip(m_pressure, tr("The atmospheric pressure at the launch site."));

    const auto humidity = scope.make<DoubleBinding>(
        contexts.target,
        doubleSource<Target>(&Target::getLaunchRelativeHumidity,
                             &Target::setLaunchRelativeHumidity),
        unitGroup(UnitGroupId::RELATIVE),
        DoubleBinding::Options{.min = 0,
                               .max = SimulationInputRanges::kLaunchRelativeHumidity.max});
    //: simedtdlg.lbl.RelativeHumidity
    m_relativeHumidity = ConfigFields::addNumberRow(*grid, 4, tr("Relative humidity:"), humidity,
                                                    QStringLiteral("LaunchRelativeHumidity"),
                                                    {.slider = SliderMapping::linear(0, 1)});
    //: simedtdlg.lbl.ttip.RelativeHumidity
    setRowToolTip(m_relativeHumidity, tr("The relative air humidity at the launch site."));

    // Java: isa.addEnableComponent(..., false) for each widget of the three rows.
    followCheckBox(*m_isaCheckBox, m_temperature, false);
    followCheckBox(*m_isaCheckBox, m_pressure, false);
    followCheckBox(*m_isaCheckBox, m_relativeHumidity, false);
}

// ----------------------------------------------------------------------------- the launch site

void SimulationConditionsPanel::buildLaunchSite(const Contexts& contexts, BindingScope& scope)
{
    //: simedtdlg.lbl.Launchsite
    m_launchSiteBox   = makeBox(*this, tr("Launch site"), QStringLiteral("launchSitePanel"));
    QGridLayout* grid = makeRowGrid(*m_launchSiteBox);

    const auto latitude = scope.make<DoubleBinding>(
        contexts.target,
        doubleSource<Target>(&Target::getLaunchLatitude, &Target::setLaunchLatitude),
        unitGroup(UnitGroupId::LATITUDE), DoubleBinding::Options{.min = -90, .max = 90});
    //: simedtdlg.lbl.Latitude
    m_latitude = ConfigFields::addNumberRow(*grid, 0, tr("Latitude:"), latitude,
                                            QStringLiteral("LaunchLatitude"),
                                            {.slider = SliderMapping::linear(-90, 90)});
    //: simedtdlg.lbl.ttip.Latitude
    setRowToolTip(m_latitude, tr("<html>The launch site latitude affects the gravitational pull of "
                                 "Earth. <br>Positive values are on the Northern hemisphere, "
                                 "negative values on the Southern hemisphere."));

    const auto longitude = scope.make<DoubleBinding>(
        contexts.target,
        doubleSource<Target>(&Target::getLaunchLongitude, &Target::setLaunchLongitude),
        unitGroup(UnitGroupId::LONGITUDE), DoubleBinding::Options{.min = -180, .max = 180});
    //: simedtdlg.lbl.Longitude
    m_longitude = ConfigFields::addNumberRow(*grid, 1, tr("Longitude:"), longitude,
                                             QStringLiteral("LaunchLongitude"),
                                             {.slider = SliderMapping::linear(-180, 180)});
    //: simedtdlg.lbl.ttip.Longitude
    setRowToolTip(m_longitude, tr("<html>Required for weather prediction and elevation models."));

    const auto altitude = scope.make<DoubleBinding>(
        contexts.target,
        doubleSource<Target>(&Target::getLaunchAltitude, &Target::setLaunchAltitude),
        unitGroup(UnitGroupId::DISTANCE),
        DoubleBinding::Options{.min = 0, .max = ExtendedIsaModel::getMaximumAllowedAltitude()});
    //: simedtdlg.lbl.Altitude
    m_altitude = ConfigFields::addNumberRow(*grid, 2, tr("Altitude:"), altitude,
                                            QStringLiteral("LaunchAltitude"),
                                            {.slider = SliderMapping::fromRange(0, 250, 1000)});
    //: simedtdlg.lbl.ttip.Altitude
    setRowToolTip(m_altitude, tr("<html>The launch altitude above mean sea level. <br>This affects "
                                 "the position of the rocket in the atmospheric model."));
}

// ------------------------------------------------------------------------------ the launch rod

void SimulationConditionsPanel::buildLaunchRod(const Contexts& contexts, BindingScope& scope)
{
    //: simedtdlg.border.Launchrod
    m_launchRodBox    = makeBox(*this, tr("Launch rod"), QStringLiteral("launchRodPanel"));
    QGridLayout* grid = makeRowGrid(*m_launchRodBox);

    const auto length = scope.make<DoubleBinding>(
        contexts.target,
        doubleSource<Target>(&Target::getLaunchRodLength, &Target::setLaunchRodLength),
        unitGroup(UnitGroupId::LENGTH),
        DoubleBinding::Options{.min = 0, .max = SimulationInputRanges::kLaunchRodLength.max});
    //: simedtdlg.lbl.Length
    m_rodLength = ConfigFields::addNumberRow(*grid, 0, tr("Length:"), length,
                                             QStringLiteral("LaunchRodLength"),
                                             {.slider = SliderMapping::fromRange(0, 1, 5)});
    //: simedtdlg.lbl.ttip.Length
    setRowToolTip(m_rodLength, tr("The length of the launch rod."));

    const auto intoWind = scope.make<BoolBinding>(
        contexts.target, boolSource(&Target::getLaunchIntoWind, &Target::setLaunchIntoWind));
    //: simedtdlg.checkbox.Intowind
    m_intoWindCheckBox = makeChild<BoolCheckBox>(m_launchRodBox, intoWind,
                                                 tr("Always launch directly up-wind or down-wind"));
    m_intoWindCheckBox->setObjectName(QStringLiteral("LaunchIntoWind"));
    // Intowind1 ends with a space, as Java's text.
    m_intoWindCheckBox->setToolTip(
        //: simedtdlg.checkbox.ttip.Intowind1
        tr("<html>Makes the launch rod point into the wind. <br> ") +
        //: simedtdlg.checkbox.ttip.Intowind2
        tr("A zero launch rod angle will point directly up (vertical).  <br>") +
        //: simedtdlg.checkbox.ttip.Intowind3
        tr("A negative launch rod angle will launch with the wind. <br>") +
        //: simedtdlg.checkbox.ttip.Intowind4
        tr("If you uncheck this box you can point the launchrod any direction you please."));
    grid->addWidget(m_intoWindCheckBox, 1, 0, 1, 4);
    // Java: "wrap unrel".
    grid->setRowMinimumHeight(2, kParagraphGap);

    const auto angle = scope.make<DoubleBinding>(
        contexts.target,
        doubleSource<Target>(&Target::getLaunchRodAngle, &Target::setLaunchRodAngle),
        unitGroup(UnitGroupId::ANGLE),
        DoubleBinding::Options{.min = -SimulationOptions::kMaxLaunchRodAngle,
                               .max = SimulationOptions::kMaxLaunchRodAngle});
    //: simedtdlg.lbl.Angle
    m_rodAngle = ConfigFields::addNumberRow(
        *grid, 3, tr("Angle:"), angle, QStringLiteral("LaunchRodAngle"),
        {.slider = SliderMapping::fromRange(-SimulationOptions::kMaxLaunchRodAngle, 0,
                                            SimulationOptions::kMaxLaunchRodAngle)});
    //: simedtdlg.lbl.ttip.Angle
    setRowToolTip(m_rodAngle,
                  tr("<html>The angle of the launch rod from vertical. <br> If set to always "
                     "launch up-wind or down-wind, positive angles are up-wind, negative angles "
                     "down-wind. <br> If set for a manual direction, positive angles towards the "
                     "direction axis. <br> E.g. if direction is set to 90\u00B0 (East of the "
                     "wind), positive angles will point the launch rod East. Negative angles will "
                     "point the rod West."));

    // No minimum: Java gives this DoubleModel the Integer 0, which is no Double and so no bound
    // (see the class comment).
    const auto direction = scope.make<DoubleBinding>(
        contexts.target,
        doubleSource<Target>(&Target::getLaunchRodDirection, &Target::setLaunchRodDirection),
        unitGroup(UnitGroupId::ANGLE),
        DoubleBinding::Options{.min = -std::numeric_limits<double>::infinity(),
                               .max = 2 * std::numbers::pi});
    //: simedtdlg.lbl.Direction
    m_rodDirection = ConfigFields::addNumberRow(
        *grid, 4, tr("Direction:"), direction, QStringLiteral("LaunchRodDirection"),
        {.slider = SliderMapping::linear(0, 2 * std::numbers::pi)});
    // Direction3 ends with a space, as Java's text.
    setRowToolTip(m_rodDirection,
                  //: simedtdlg.lbl.ttip.Direction1
                  tr("<html>Direction of the launch rod. <br>") +
                      QtStrings::defaultUnitText(UnitGroupId::ANGLE, 0) + QStringLiteral(" ") +
                      //: simedtdlg.lbl.ttip.Direction2
                      tr("-") + QStringLiteral(" ") +
                      QtStrings::defaultUnitText(UnitGroupId::ANGLE, 2 * std::numbers::pi) +
                      QStringLiteral(". ") +
                      //: simedtdlg.lbl.ttip.Direction3
                      tr("0 is North; ") + QStringLiteral(" ") +
                      QtStrings::defaultUnitText(UnitGroupId::ANGLE, std::numbers::pi / 2) +
                      QStringLiteral(" ") +
                      //: simedtdlg.lbl.ttip.Direction4
                      tr("is East."));
    // Java: intoWind.addEnableComponent(..., false) for the direction's widgets.
    followCheckBox(*m_intoWindCheckBox, m_rodDirection, false);
}

// ------------------------------------------------------------------------------- the validation

void SimulationConditionsPanel::buildValidators()
{
    const std::shared_ptr<DoubleBinding> average = m_averageWind.spinBox->binding();
    m_windValidator = &FieldValidator::of(*m_averageWind.spinBox)
                           .errorIf([average] { return average->value() >= kExtremeWindSpeed; },
                                    //: simedtdlg.error.windSpeedExtreme
                                    tr("Extremely high wind speed"))
                           .warnIf([average] { return average->value() >= kHighWindSpeed; },
                                   //: simedtdlg.warning.windSpeedHigh
                                   tr("High wind speed"))
                           .showMessagePopup(FieldValidator::kMessagePopupDuration)
                           .listenTo(*average);

    const std::shared_ptr<BoolBinding>   isa         = m_isaCheckBox->binding();
    const std::shared_ptr<DoubleBinding> temperature = m_temperature.spinBox->binding();
    // The validator's conditions are Java's, in Java's order: an error's message wins over a
    // warning's (FieldValidator::update()).
    m_temperatureValidator =
        &FieldValidator::of(*m_temperature.spinBox)
             .errorIf(
                 [isa, temperature] {
                     return !isa->value() && temperature->value() <= kExtremelyLowTemperature;
                 },
                 // OpenRocket bug 35: Java's key is missing from its messages.properties (see the
                 // class comment); the text is QtRocket's, after temperatureTooHigh's.
                 //: simedtdlg.error.temperatureTooLow (not in OpenRocket's messages.properties)
                 tr("Extremely low temperature"))
             .errorIf(
                 [isa, temperature] {
                     return !isa->value() && temperature->value() >= kExtremelyHighTemperature;
                 },
                 //: simedtdlg.error.temperatureTooHigh
                 tr("Extremely high temperature"))
             .warnIf(
                 [isa, temperature] {
                     return !isa->value() && temperature->value() <= kLowTemperature;
                 },
                 //: simedtdlg.warning.temperatureTooLow
                 tr("Low temperature"))
             .warnIf(
                 [isa, temperature] {
                     return !isa->value() && temperature->value() >= kHighTemperature;
                 },
                 //: simedtdlg.warning.temperatureTooHigh
                 tr("High temperature"))
             .showMessagePopup(FieldValidator::kMessagePopupDuration)
             .listenTo(*temperature)
             .listenTo(*isa);

    const std::shared_ptr<DoubleBinding> pressure = m_pressure.spinBox->binding();
    const std::shared_ptr<DoubleBinding> altitude = m_altitude.spinBox->binding();
    // Java's own standard atmosphere of the panel; the conditions only read it.
    const auto standardAtmosphere = std::make_shared<const ExtendedIsaModel>();
    m_pressureValidator =
        &FieldValidator::of(*m_pressure.spinBox)
             .warnIf(
                 [isa, pressure, altitude, standardAtmosphere] {
                     return !isa->value() && pressureRatio(pressure->value(), altitude->value(),
                                                           *standardAtmosphere) < kLowPressureRatio;
                 },
                 //: simedtdlg.warning.pressureTooLow
                 tr("Low pressure"))
             .warnIf(
                 [isa, pressure, altitude, standardAtmosphere] {
                     return !isa->value() &&
                            pressureRatio(pressure->value(), altitude->value(),
                                          *standardAtmosphere) > kHighPressureRatio;
                 },
                 //: simedtdlg.warning.pressureTooHigh
                 tr("High pressure"))
             .showMessagePopup(FieldValidator::kMessagePopupDuration)
             .listenTo(*pressure)
             .listenTo(*altitude)
             .listenTo(*isa);

    const std::shared_ptr<DoubleBinding> angle = m_rodAngle.spinBox->binding();
    m_rodAngleValidator =
        &FieldValidator::of(*m_rodAngle.spinBox)
             .warnIf(
                 [angle] {
                     return std::abs(angle->value()) >= MathUtil::javaToRadians(kLargeRodAngle);
                 },
                 //: simedtdlg.warning.launchRodAngleLarge
                 tr("Large launch rod angle"))
             .errorIf(
                 [angle] {
                     return std::abs(angle->value()) >= MathUtil::javaToRadians(kMaxedRodAngle);
                 },
                 //: simedtdlg.error.launchRodAngleMaxed
                 tr("Extremely large launch rod angle"))
             .showMessagePopup(FieldValidator::kMessagePopupDuration)
             .listenTo(*angle);
}

// ------------------------------------------------------------------------------- the buttons

void SimulationConditionsPanel::buildDefaultButtons()
{
    //: simedtdlg.but.resettodefault
    m_resetButton = makeChild<QPushButton>(this, tr("Reset to default"));
    m_resetButton->setObjectName(QStringLiteral("restoreDefaults"));
    connect(m_resetButton, &QPushButton::clicked, this,
            guarded([this] { resetToDefault(); }, context()));
    //: simedtdlg.but.savedefault
    m_saveButton = makeChild<QPushButton>(this, tr("Save as default"));
    m_saveButton->setObjectName(QStringLiteral("saveDefaults"));
    connect(m_saveButton, &QPushButton::clicked, this,
            guarded([this] { saveAsDefault(); }, context()));
}

void SimulationConditionsPanel::commitEdits()
{
    for (UnitSpinBox* spinBox : findChildren<UnitSpinBox*>())
    {
        spinBox->commitEdit();
    }
}

void SimulationConditionsPanel::resetToDefault()
{
    commitEdits();
    if (SimulationOptions* options = liveOptions(); options != nullptr)
    {
        // Java: everything copyConditionsFrom() copies, not only what the tab shows.
        options->copyConditionsFrom(
            DefaultSimulationOptionFactory(m_services.preferences()).getDefault());
    }
}

void SimulationConditionsPanel::saveAsDefault()
{
    commitEdits();
    if (const SimulationOptions* options = liveOptions(); options != nullptr)
    {
        DefaultSimulationOptionFactory(m_services.preferences()).saveDefault(*options);
    }
}

SimulationOptions* SimulationConditionsPanel::liveOptions() const noexcept
{
    return m_options != nullptr && m_targetSelection->isValid() ? m_options : nullptr;
}

// -------------------------------------------------------------------------------- the layout

void SimulationConditionsPanel::layOut()
{
    // Java: MigLayout("fillx, gap rel rel"): the wind above the atmosphere in the left column,
    // the launch site above the launch rod in the right one, each box at the top of its cell and
    // as wide as its column ("growx, split 2, aligny 0, flowy"); the buttons below, on the right.
    auto* grid = makeChild<QGridLayout>(this);
    grid->setHorizontalSpacing(kRelatedGap);
    grid->setVerticalSpacing(kRelatedGap);
    // Each column is a widget of its own, so that the boxes of one column do not share the
    // heights of the other's.
    const auto addColumn = [this, grid](int column, QGroupBox* top, QGroupBox* bottom) {
        auto* widget = makeChild<QWidget>(this);
        auto* boxes  = makeChild<QVBoxLayout>(widget);
        boxes->setContentsMargins(0, 0, 0, 0);
        boxes->setSpacing(kRelatedGap);
        boxes->addWidget(top);
        boxes->addWidget(bottom);
        boxes->addStretch(1);
        grid->addWidget(widget, 0, column);
        grid->setColumnStretch(column, 1);
    };
    addColumn(0, m_windBox, m_atmosphereBox);
    addColumn(1, m_launchSiteBox, m_launchRodBox);
    if (m_resetButton != nullptr)
    {
        auto* row     = makeChild<QWidget>(this);
        auto* buttons = makeChild<QHBoxLayout>(row);
        buttons->setContentsMargins(0, 0, 0, 0);
        buttons->addStretch(1);
        buttons->addWidget(m_resetButton);
        buttons->addSpacing(kParagraphGap);
        buttons->addWidget(m_saveButton);
        grid->addWidget(row, 1, 0, 1, 2);
    }
    grid->setRowStretch(2, 1);
}

// ----------------------------------------------------------------------------- the easter egg

bool SimulationConditionsPanel::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_turbulenceIntensity.spinBox && event->type() == QEvent::KeyPress)
    {
        // Java's keyTyped(): the text before the key and the key's character.
        const auto* key = dynamic_cast<const QKeyEvent*>(event);
        if (key != nullptr && !key->text().isEmpty() &&
            m_turbulenceIntensity.spinBox->text() + key->text() == QStringLiteral("42"))
        {
            Guard::run(
                [this] {
                    // Java's literals, not in messages.properties.
                    m_services.prompts().showMessage(
                        this, UserPrompts::Message{
                                  .severity = UserPrompts::Severity::INFORMATION,
                                  .title    = QStringLiteral("42"),
                                  .lines    = {QStringLiteral(
                                      "The answer to the ultimate question of life, the "
                                      "universe, and everything.")},
                              });
                },
                context());
        }
    }
    return QWidget::eventFilter(watched, event);
}

}  // namespace QtRocket::Gui
