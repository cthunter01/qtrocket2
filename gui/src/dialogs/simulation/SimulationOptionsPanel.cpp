#include "QtRocket/gui/dialogs/simulation/SimulationOptionsPanel.h"

#include <array>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <utility>

#include <QCheckBox>
#include <QEvent>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QObject>
#include <QPointer>
#include <QPushButton>
#include <QSize>
#include <QString>
#include <QVBoxLayout>
#include <QWidget>
#include <Qt>

#include "QtRocket/gui/binding/BindingContext.h"
#include "QtRocket/gui/binding/DoubleBinding.h"
#include "QtRocket/gui/binding/EnumBinding.h"
#include "QtRocket/gui/binding/ObjectSelection.h"
#include "QtRocket/gui/binding/PropertySource.h"
#include "QtRocket/gui/binding/SliderMapping.h"
#include "QtRocket/gui/configdialog/ConfigFields.h"
#include "QtRocket/gui/dialogs/ModalDialog.h"
#include "QtRocket/gui/dialogs/simulation/AerodynamicLookupDialog.h"
#include "QtRocket/gui/dialogs/simulation/RandomSeedControls.h"
#include "QtRocket/gui/dialogs/simulation/SimulationExtensionList.h"
#include "QtRocket/gui/util/EnumText.h"
#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/gui/util/GuiServices.h"
#include "QtRocket/gui/util/Icons.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/widgets/BindingScope.h"
#include "QtRocket/gui/widgets/DescriptionArea.h"
#include "QtRocket/gui/widgets/EnumComboBox.h"
#include "QtRocket/gui/widgets/FieldValidator.h"
#include "QtRocket/gui/widgets/UnitSelector.h"
#include "QtRocket/gui/widgets/UnitSpinBox.h"
#include "QtRocket/gui/widgets/ValueSlider.h"
#include "QtRocket/models/GravityModelType.h"
#include "QtRocket/preferences/PreferenceKeys.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/simulation/AbstractRkSimulationStepper.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/simulation/SimulationInputRanges.h"
#include "QtRocket/simulation/SimulationOptions.h"
#include "QtRocket/simulation/SimulationStepperMethod.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/GeodeticComputationStrategy.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket::Gui
{

namespace
{

/// The context of the panel's reports (Guard).
QString context()
{
    return QStringLiteral("Simulation options");
}

/// MigLayout's related gap ("gap rel"), between the boxes.
constexpr int kRelatedGap = 6;
/// MigLayout's paragraph gap ("wrap para", "gaptop para").
constexpr int kParagraphGap = 12;

// The grid's columns (Java: "[grow][min!][]"): the labels, the fields, the unit selectors (and
// the lookup summary's icon), the sliders (ConfigFields::addNumberRow() puts a number row's
// widgets into the last three).
constexpr int kLabelColumn  = 0;
constexpr int kFieldColumn  = 1;
constexpr int kUnitColumn   = 2;
constexpr int kSliderColumn = 3;
constexpr int kColumnCount  = 4;

// The grid's rows; the empty ones are Java's paragraph gaps ("wrap para", "gaptop para").
constexpr int kCalculationRow  = 0;
constexpr int kStepperRow      = 1;
constexpr int kLookupRow       = 2;
constexpr int kLookupGapRow    = 3;
constexpr int kGeodeticRow     = 4;
constexpr int kGravityModelRow = 5;
constexpr int kGravityValueRow = 6;
constexpr int kTimeStepGapRow  = 7;
constexpr int kTimeStepRow     = 8;
constexpr int kMaximumTimeRow  = 9;
constexpr int kSeedGapRow      = 10;
constexpr int kSeedRow         = 11;

/// A source of the double property of the options read by @p get and written by @p set (Java:
/// DoubleModel(conditions, "<Name>")).
[[nodiscard]] DoubleSource doubleSource(double (SimulationOptions::*get)() const,
                                        void (SimulationOptions::*set)(double))
{
    return DoubleSource::ofObject<SimulationOptions>({
        .get = [get](const SimulationOptions& options) { return std::invoke(get, options); },
        .set = [set](SimulationOptions& options,
                     const double&      value) { std::invoke(set, options, value); },
    });
}

/// A source of the enum property of the options read by @p get and written by @p set (Java:
/// EnumModel(conditions, "<Name>")).
template <class E>
[[nodiscard]] EnumSource<E> enumSource(E (SimulationOptions::*get)() const,
                                       void (SimulationOptions::*set)(E))
{
    return EnumSource<E>::template ofObject<SimulationOptions>({
        .get = [get](const SimulationOptions& options) { return std::invoke(get, options); },
        .set = [set](SimulationOptions& options,
                     const E&           value) { std::invoke(set, options, value); },
    });
}

/// @p row's widgets get the tooltip @p tip.
void setRowToolTip(const ConfigFields::NumberWidgets& row, const QString& tip)
{
    for (QWidget* const widget :
         std::array<QWidget*, 4>{row.label, row.spinBox, row.unitSelector, row.slider})
    {
        if (widget != nullptr)
        {
            widget->setToolTip(tip);
        }
    }
}

/// A label of the grid with the tooltip @p tip.
[[nodiscard]] QLabel* makeLabel(QWidget& parent, const QString& text, const QString& tip)
{
    auto* label = makeChild<QLabel>(&parent, text);
    label->setToolTip(tip);
    return label;
}

}  // namespace

SimulationOptionsPanel::SimulationOptionsPanel(std::shared_ptr<Simulation> simulation,
                                               const GuiServices& services, BindingScope& scope,
                                               QWidget* parent)
  : QWidget(parent), m_simulation(std::move(simulation)), m_services(services)
{
    if (m_simulation == nullptr)
    {
        bug("The simulation options tab needs a simulation");
    }
    setObjectName(QStringLiteral("simulationOptionsPanel"));
    m_selection =
        scope.keepContext(ObjectSelection::make<SimulationOptions>(m_simulation->getOptions()));

    //: simedtdlg.border.Simopt
    m_optionsBox = makeChild<QGroupBox>(this, tr("Simulator options"));
    m_optionsBox->setObjectName(QStringLiteral("simulatorOptionsPanel"));
    m_grid = makeChild<QWidget>(m_optionsBox);
    // The grid of Java's subsub panel ("fillx, ins 0, gap rel rel", "[grow][min!][]").
    m_gridLayout = makeChild<QGridLayout>(m_grid);
    m_gridLayout->setContentsMargins(0, 0, 0, 0);
    m_gridLayout->setColumnStretch(kLabelColumn, 1);
    buildMethods(scope);
    buildGravity(scope);
    buildTimes(scope);
    m_randomSeed = makeChild<RandomSeedControls>(m_grid, m_selection);
    buildDefaultButtons();
    buildExtensions();
    layOut();

    m_optionsConnection = m_selection->addListener([this](BindingContext::Phase phase) {
        if (phase == BindingContext::Phase::NOTIFY)
        {
            updateLookupSummary();
        }
    });
    updateLookupSummary();
}

SimulationOptionsPanel::~SimulationOptionsPanel() = default;

// -------------------------------------------------------------------------- the methods

void SimulationOptionsPanel::buildMethods(BindingScope& scope)
{
    // Calculation method:
    //: simedtdlg.lbl.ttip.Calcmethod
    const QString calculationTip = tr(
        "<html>The Extended Barrowman method calculates aerodynamic forces according  <br>to the "
        "Barrowman equations extended to accommodate more components.");
    //: simedtdlg.lbl.Calcmethod
    m_calculationLabel = makeLabel(*m_grid, tr("Calculation method:"), calculationTip);
    //: simedtdlg.lbl.ExtBarrowman
    m_barrowmanLabel = makeLabel(*m_grid, tr("Extended Barrowman"), calculationTip);

    // Simulation method
    const QString stepperTip =
        //: simedtdlg.lbl.ttip.Simmethod1
        tr("<html>The six degree-of-freedom simulator allows the rocket total freedom during "
           "flight. <br>") +
        //: simedtdlg.lbl.ttip.Simmethod2
        tr("Integration is performed using a 4<sup>th</sup> order Runge-Kutta 4 numerical "
           "integration.");
    //: simedtdlg.lbl.Simmethod
    m_stepperLabel     = makeLabel(*m_grid, tr("Simulation method:"), stepperTip);
    const auto stepper = scope.make<EnumBinding<SimulationStepperMethod>>(
        m_selection,
        enumSource<SimulationStepperMethod>(&SimulationOptions::getSimulationStepperMethodChoice,
                                            &SimulationOptions::setSimulationStepperMethodChoice),
        std::span<const SimulationStepperMethod>(kAllSimulationStepperMethods));
    m_stepperCombo = makeChild<EnumComboBox>(m_grid, stepper);
    m_stepperCombo->setObjectName(QStringLiteral("SimulationStepperMethodChoice"));
    const auto stepperTooltip = [this, stepper] {
        const SimulationStepperMethod method = stepper->value();
        m_stepperCombo->setToolTip(
            translatedEnumText(descriptionKey(method), getDescription(method)));
    };
    stepperTooltip();
    m_stepperConnection = stepper->addChangeListener(stepperTooltip);

    // Aerodynamic data
    //: AerodynamicLookupDialog.lbl.summary
    m_lookupLabel = makeChild<QLabel>(m_grid, tr("Aerodynamic data:"));
    //: AerodynamicLookupDialog.btn.configure
    m_configureButton = makeChild<QPushButton>(m_grid, tr("Configure..."));
    m_configureButton->setObjectName(QStringLiteral("configureLookupButton"));
    m_configureButton->setAutoDefault(false);
    connect(m_configureButton, &QPushButton::clicked, this,
            guarded([this] { openLookupDialog(); }, context()));
    m_lookupSummaryIcon = makeChild<QLabel>(m_grid);
    m_lookupSummaryIcon->setObjectName(QStringLiteral("aerodynamicLookupSummaryIconLabel"));
    updateHelpIcon();

    // Geodetic calculation method:
    //: simedtdlg.lbl.GeodeticMethod
    m_geodeticLabel = makeLabel(*m_grid, tr("Geodetic calculations:"),
                                //: simedtdlg.lbl.ttip.GeodeticMethodTip
                                tr("Relate to the calculation of coordinates on the earth.  This "
                                   "also enables coriolis effect computations."));
    const auto geodetic = scope.make<EnumBinding<GeodeticComputationStrategy>>(
        m_selection,
        enumSource<GeodeticComputationStrategy>(&SimulationOptions::getGeodeticComputation,
                                                &SimulationOptions::setGeodeticComputation),
        std::span<const GeodeticComputationStrategy>(kAllGeodeticComputationStrategies));
    m_geodeticCombo = makeChild<EnumComboBox>(m_grid, geodetic);
    m_geodeticCombo->setObjectName(QStringLiteral("GeodeticComputation"));
    const auto geodeticTooltip = [this, geodetic] {
        const GeodeticComputationStrategy strategy = geodetic->value();
        m_geodeticCombo->setToolTip(
            translatedEnumText(descriptionKey(strategy), description(strategy)));
    };
    geodeticTooltip();
    m_geodeticConnection = geodetic->addChangeListener(geodeticTooltip);
}

// -------------------------------------------------------------------------- the gravity

void SimulationOptionsPanel::buildGravity(BindingScope& scope)
{
    // Gravity model:
    //: simedtdlg.lbl.GravityModel
    m_gravityModelLabel = makeLabel(*m_grid, tr("Gravity model:"),
                                    //: simedtdlg.lbl.ttip.GravityModel
                                    tr("Select the gravity model to use during the simulation."));
    const auto gravity  = scope.make<EnumBinding<GravityModelType>>(
        m_selection,
        enumSource<GravityModelType>(&SimulationOptions::getGravityModelType,
                                     &SimulationOptions::setGravityModelType),
        std::span<const GravityModelType>(kAllGravityModelTypes));
    m_gravityCombo = makeChild<EnumComboBox>(m_grid, gravity);
    m_gravityCombo->setObjectName(QStringLiteral("GravityModelType"));

    // Constant gravity value:
    const auto value = scope.make<DoubleBinding>(
        m_selection,
        doubleSource(&SimulationOptions::getConstantGravity,
                     &SimulationOptions::setConstantGravity),
        unitGroup(UnitGroupId::ACCELERATION),
        DoubleBinding::Options{.min = 0, .max = SimulationInputRanges::kConstantGravity.max});
    //: simedtdlg.lbl.GravityValue
    m_gravityValue = ConfigFields::addNumberRow(
        *m_gridLayout, kGravityValueRow, tr("Gravity value:"), value,
        QStringLiteral("ConstantGravity"), {.slider = SliderMapping::linear(0, 20)});
    //: simedtdlg.lbl.ttip.GravityValue
    setRowToolTip(m_gravityValue, tr("Constant gravitational acceleration value"));
    m_gravityValue.slider->setFixedWidth(kGravitySliderWidth);

    // Update the tooltip and the visibility of the constant gravity's row from the model
    const auto gravityChanged = [this, gravity] {
        m_gravityCombo->setToolTip(
            translatedEnumText(tooltipKey(gravity->value()), tooltip(gravity->value())));
        showGravityValue();
    };
    gravityChanged();
    m_gravityConnection = gravity->addChangeListener(gravityChanged);
}

void SimulationOptionsPanel::showGravityValue()
{
    const auto binding =
        m_gravityCombo != nullptr ? m_gravityCombo->enumBinding<GravityModelType>() : nullptr;
    const bool isConstant = binding != nullptr && binding->value() == GravityModelType::CONSTANT;
    for (QWidget* const widget :
         std::array<QWidget*, 4>{m_gravityValue.label, m_gravityValue.spinBox,
                                 m_gravityValue.unitSelector, m_gravityValue.slider})
    {
        if (widget != nullptr)
        {
            widget->setVisible(isConstant);
        }
    }
}

// ---------------------------------------------------------------------------- the times

void SimulationOptionsPanel::buildTimes(BindingScope& scope)
{
    // Time step:
    const QString timeStepTip =
        //: simedtdlg.lbl.ttip.Timestep1
        tr("<html>The time between simulation steps. <br>A smaller time step results in a more "
           "accurate but slower simulation. <br>") +
        //: simedtdlg.lbl.ttip.Timestep2
        tr("The 4<sup>th</sup> order simulation method is quite accurate with a time step of") +
        QStringLiteral(" ") +
        QtStrings::defaultUnitText(UnitGroupId::TIME_STEP,
                                   AbstractRkSimulationStepper::kRecommendedTimeStep) +
        QStringLiteral(".");
    const auto timeStep = scope.make<DoubleBinding>(
        m_selection, doubleSource(&SimulationOptions::getTimeStep, &SimulationOptions::setTimeStep),
        unitGroup(UnitGroupId::TIME_STEP), DoubleBinding::Options{.min = 0.01, .max = 1});
    //: simedtdlg.lbl.Timestep
    m_timeStep = ConfigFields::addNumberRow(*m_gridLayout, kTimeStepRow, tr("Time step:"), timeStep,
                                            QStringLiteral("TimeStep"),
                                            {.slider = SliderMapping::linear(0.01, 0.2)});
    setRowToolTip(m_timeStep, timeStepTip);
    m_timeStep.slider->setFixedWidth(kTimeStepSliderWidth);

    // Maximum simulation time:
    const auto maximumTime = scope.make<DoubleBinding>(
        m_selection,
        doubleSource(&SimulationOptions::getMaxSimulationTime,
                     &SimulationOptions::setMaxSimulationTime),
        unitGroup(UnitGroupId::LONG_TIME),
        DoubleBinding::Options{.min = 1, .max = SimulationInputRanges::kMaxSimulationTime.max});
    //: simedtdlg.lbl.MaxSimTime
    m_maximumTime =
        ConfigFields::addNumberRow(*m_gridLayout, kMaximumTimeRow, tr("Maximum simulation time:"),
                                   maximumTime, QStringLiteral("MaxSimulationTime"), {});
    //: simedtdlg.lbl.ttip.MaxSimTime
    setRowToolTip(m_maximumTime, tr("<html>The maximum time the simulation will run."));
}

// -------------------------------------------------------------------------- the buttons

void SimulationOptionsPanel::buildDefaultButtons()
{
    // Reset to default button
    //: simedtdlg.but.resettodefault
    m_resetButton = makeChild<QPushButton>(m_optionsBox, tr("Reset to default"));
    m_resetButton->setObjectName(QStringLiteral("resetButton"));
    m_resetButton->setAutoDefault(false);
    // Reset the time step to its default value (
    m_resetButton->setToolTip(
        //: simedtdlg.but.ttip.resettodefault
        tr("Reset the time step to its default value (") +
        QtStrings::defaultUnitText(UnitGroupId::SHORT_TIME,
                                   AbstractRkSimulationStepper::kRecommendedTimeStep) +
        QStringLiteral(")."));
    connect(m_resetButton, &QPushButton::clicked, this,
            guarded([this] { resetToDefault(); }, context()));

    // Save as default button
    //: simedtdlg.but.savedefault
    m_saveButton = makeChild<QPushButton>(m_optionsBox, tr("Save as default"));
    m_saveButton->setObjectName(QStringLiteral("saveButton"));
    m_saveButton->setAutoDefault(false);
    connect(m_saveButton, &QPushButton::clicked, this,
            guarded([this] { saveAsDefault(); }, context()));
}

void SimulationOptionsPanel::commitEdits()
{
    for (UnitSpinBox* spinBox : findChildren<UnitSpinBox*>())
    {
        spinBox->commitEdit();
    }
}

void SimulationOptionsPanel::resetToDefault()
{
    commitEdits();
    SimulationOptions* const options = liveOptions();
    if (options == nullptr)
    {
        return;
    }
    const Preferences& preferences = m_services.preferences();
    options->setTimeStep(preferences.getDouble(PreferenceKeys::kSimulationTimeStep,
                                               AbstractRkSimulationStepper::kRecommendedTimeStep));
    options->setMaxSimulationTime(preferences.getDouble(
        PreferenceKeys::kSimulationMaxTime, AbstractRkSimulationStepper::kRecommendedMaxTime));
    options->setGeodeticComputation(
        geodeticComputationStrategyFromString(preferences.getGeodeticComputationName())
            .value_or(GeodeticComputationStrategy::SPHERICAL));
    if (preferences.isRandomSeedFixed())
    {
        options->setRandomSeed(preferences.getRandomSeed());
        options->setRandomSeedFixed(true);
    }
    else
    {
        options->setRandomSeedFixed(false);
    }
    m_randomSeed->showOptions();
    m_randomSeed->validator().update();
}

void SimulationOptionsPanel::saveAsDefault()
{
    commitEdits();
    const SimulationOptions* const options = liveOptions();
    if (options == nullptr)
    {
        return;
    }
    Preferences& preferences = m_services.preferences();
    preferences.setTimeStep(options->getTimeStep());
    preferences.setMaxSimulationTime(options->getMaxSimulationTime());
    preferences.setGeodeticComputationName(name(options->getGeodeticComputation()));
    prepareForSimulation();
    if (options->isRandomSeedFixed())
    {
        preferences.setRandomSeed(options->getRandomSeed());
    }
    preferences.setRandomSeedFixed(options->isRandomSeedFixed());
}

void SimulationOptionsPanel::prepareForSimulation()
{
    m_randomSeed->prepareForSimulation();
}

// ----------------------------------------------------------------------- the lookup tables

void SimulationOptionsPanel::openLookupDialog()
{
    SimulationOptions* const options = liveOptions();
    if (options == nullptr)
    {
        return;
    }
    // The panel may go while the dialog is open (its window closed in the dialog's event loop).
    const QPointer<SimulationOptionsPanel> alive(this);
    {
        ModalDialog<AerodynamicLookupDialog> dialog(window(), *options, m_services);
        dialog.exec();
    }
    if (!alive.isNull())
    {
        updateLookupSummary();
    }
}

void SimulationOptionsPanel::updateLookupSummary()
{
    if (const SimulationOptions* const options = liveOptions(); options != nullptr)
    {
        m_lookupSummaryIcon->setToolTip(lookupSummaryTooltip(*options));
    }
}

QString SimulationOptionsPanel::lookupSummaryTooltip(const SimulationOptions& options)
{
    const auto line = [](const QString& text) {
        return QtStrings::fromUtf8(Strings::escapeHtml(QtStrings::toUtf8(text)));
    };
    const QString drag = AerodynamicLookupDialog::summaryText(options.getDragLookupCsvPath(),
                                                              options.getDragLookupTable().get());
    const QString stability = AerodynamicLookupDialog::summaryText(
        options.getStabilityLookupCsvPath(), options.getStabilityLookupTable().get());
    return QStringLiteral("<html>") +
           //: AerodynamicLookupDialog.lbl.summaryDrag
           line(tr("Drag: %1").arg(drag)) + QStringLiteral("<br>") +
           //: AerodynamicLookupDialog.lbl.summaryStability
           line(tr("Stability: %1").arg(stability)) + QStringLiteral("</html>");
}

// ---------------------------------------------------------------------------- extensions

void SimulationOptionsPanel::buildExtensions()
{
    //// Simulation extensions
    //: simedtdlg.border.SimExt
    m_extensionsBox = makeChild<QGroupBox>(this, tr("Simulation extensions"));
    m_extensionsBox->setObjectName(QStringLiteral("simulationExtensionsPanel"));
    m_extensionsBox->setMinimumWidth(kExtensionsMinimumWidth);
    auto* column            = makeChild<QVBoxLayout>(m_extensionsBox);
    m_extensionsDescription = makeChild<DescriptionArea>(
        m_extensionsBox,
        //: simedtdlg.SimExt.desc
        tr("<html><i>Simulation extensions</i> enable advanced features and custom functionality "
           "during flight simulations.  You can for example do hardware-in-the-loop testing with "
           "them."),
        kExtensionDescriptionRows);
    m_extensionsDescription->setObjectName(QStringLiteral("extensionsDescription"));
    m_extensionsDescription->setMinimumHeight(kExtensionDescriptionMinHeight);
    column->addWidget(m_extensionsDescription);
    // Java's "Add extension" button with its menu (and "Copy extension") comes with Milestone 7.
    m_extensionList = makeChild<SimulationExtensionList>(m_extensionsBox, m_simulation);
    column->addWidget(m_extensionList, 1);
}

// ----------------------------------------------------------------------------- the layout

void SimulationOptionsPanel::layOut()
{
    // The grid's widgets in the order of Java's container (which the walk of the panel reads):
    // the number rows that ConfigFields put into it first are taken out and put back in their
    // place.
    QGridLayout* const grid = m_gridLayout;
    const std::array   rows{&m_gravityValue, &m_timeStep, &m_maximumTime};
    for (const ConfigFields::NumberWidgets* widgets : rows)
    {
        for (QWidget* const widget : std::array<QWidget*, 4>{
                 widgets->label, widgets->spinBox, widgets->unitSelector, widgets->slider})
        {
            if (widget != nullptr)
            {
                grid->removeWidget(widget);
            }
        }
    }
    const auto addNumberRow = [grid](const ConfigFields::NumberWidgets& widgets, int row) {
        grid->addWidget(widgets.label, row, kLabelColumn);
        grid->addWidget(widgets.spinBox, row, kFieldColumn);
        grid->addWidget(widgets.unitSelector, row, kUnitColumn);
        if (widgets.slider != nullptr)
        {
            grid->addWidget(widgets.slider, row, kSliderColumn);
        }
    };
    grid->addWidget(m_calculationLabel, kCalculationRow, kLabelColumn);
    grid->addWidget(m_barrowmanLabel, kCalculationRow, kFieldColumn, 1,
                    kColumnCount - kFieldColumn);
    grid->addWidget(m_stepperLabel, kStepperRow, kLabelColumn);
    grid->addWidget(m_stepperCombo, kStepperRow, kFieldColumn, 1, kColumnCount - kFieldColumn,
                    Qt::AlignLeft);
    grid->addWidget(m_lookupLabel, kLookupRow, kLabelColumn);
    grid->addWidget(m_configureButton, kLookupRow, kFieldColumn, Qt::AlignLeft);
    grid->addWidget(m_lookupSummaryIcon, kLookupRow, kUnitColumn, Qt::AlignLeft);
    grid->setRowMinimumHeight(kLookupGapRow, kParagraphGap);
    grid->addWidget(m_geodeticLabel, kGeodeticRow, kLabelColumn);
    grid->addWidget(m_geodeticCombo, kGeodeticRow, kFieldColumn, 1, kColumnCount - kFieldColumn,
                    Qt::AlignLeft);
    grid->addWidget(m_gravityModelLabel, kGravityModelRow, kLabelColumn);
    grid->addWidget(m_gravityCombo, kGravityModelRow, kFieldColumn, 1, kColumnCount - kFieldColumn,
                    Qt::AlignLeft);
    addNumberRow(m_gravityValue, kGravityValueRow);
    grid->setRowMinimumHeight(kTimeStepGapRow, kParagraphGap);
    addNumberRow(m_timeStep, kTimeStepRow);
    addNumberRow(m_maximumTime, kMaximumTimeRow);
    grid->setRowMinimumHeight(kSeedGapRow, kParagraphGap);
    grid->addWidget(&m_randomSeed->checkBox(), kSeedRow, kLabelColumn);
    grid->addWidget(&m_randomSeed->field(), kSeedRow, kFieldColumn, 1, kColumnCount - kFieldColumn,
                    Qt::AlignLeft);

    // Java's sub panel: the grid, a paragraph, the buttons on the left ("align left, split 2").
    auto* box = makeChild<QVBoxLayout>(m_optionsBox);
    box->addWidget(m_grid);
    box->addSpacing(kParagraphGap);
    auto* buttonRow = makeChild<QWidget>(m_optionsBox);
    auto* buttons   = makeChild<QHBoxLayout>(buttonRow);
    buttons->setContentsMargins(0, 0, 0, 0);
    buttons->addWidget(m_resetButton);
    buttons->addWidget(m_saveButton);
    buttons->addStretch(1);
    box->addWidget(buttonRow);
    box->addStretch(1);

    // Java: MigLayout("fillx, gap rel rel"): the two boxes side by side, each as tall as the row.
    auto* panel = makeChild<QHBoxLayout>(this);
    panel->setSpacing(kRelatedGap);
    panel->addWidget(m_optionsBox, 1);
    panel->addWidget(m_extensionsBox, 1);
}

void SimulationOptionsPanel::changeEvent(QEvent* event)
{
    QWidget::changeEvent(event);
    if (m_lookupSummaryIcon != nullptr &&
        (event->type() == QEvent::PaletteChange || event->type() == QEvent::FontChange ||
         event->type() == QEvent::DevicePixelRatioChange))
    {
        updateHelpIcon();
    }
}

void SimulationOptionsPanel::updateHelpIcon()
{
    // Java: JLabel(Icons.HELP), the icon in the palette's colours at the font's icon size.
    const int size = Icons::iconHeight(Icons::Id::HELP, m_lookupSummaryIcon->font());
    m_lookupSummaryIcon->setPixmap(
        Icons::icon(Icons::Id::HELP).pixmap(QSize(size, size), devicePixelRatio()));
}

SimulationOptions* SimulationOptionsPanel::liveOptions() const
{
    return m_selection->isValid() ? &m_selection->getObject<SimulationOptions>() : nullptr;
}

}  // namespace QtRocket::Gui
