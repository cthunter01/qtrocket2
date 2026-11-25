#include "QtRocket/gui/dialogs/SimulationPreferencesPanel.h"

#include <algorithm>
#include <array>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>

#include <QChar>
#include <QCheckBox>
#include <QEvent>
#include <QFocusEvent>
#include <QGridLayout>
#include <QGroupBox>
// FieldValidator.h's inline messagePopup() needs the class.
#include <QLabel>  // IWYU pragma: keep
#include <QLineEdit>
#include <QObject>
#include <QPushButton>
#include <QRandomGenerator>
#include <QString>
#include <QVBoxLayout>
#include <QWidget>
#include <Qt>

#include "QtRocket/gui/binding/DoubleBinding.h"
#include "QtRocket/gui/binding/EnumBinding.h"
#include "QtRocket/gui/binding/ObjectSelection.h"
#include "QtRocket/gui/binding/PropertySource.h"
#include "QtRocket/gui/binding/SliderMapping.h"
#include "QtRocket/gui/configdialog/ConfigFields.h"
#include "QtRocket/gui/dialogs/simulation/RandomSeedControls.h"
#include "QtRocket/gui/util/EnumText.h"
#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/util/ThemeColors.h"
#include "QtRocket/gui/widgets/BindingScope.h"
#include "QtRocket/gui/widgets/EnumComboBox.h"
#include "QtRocket/gui/widgets/FieldValidator.h"
#include "QtRocket/gui/widgets/StyledLabel.h"
#include "QtRocket/gui/widgets/UnitSelector.h"
#include "QtRocket/gui/widgets/UnitSpinBox.h"
#include "QtRocket/gui/widgets/ValueSlider.h"
#include "QtRocket/models/GravityModelType.h"
#include "QtRocket/preferences/PreferenceKeys.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/simulation/SimulationInputRanges.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/GeodeticComputationStrategy.h"

namespace QtRocket::Gui
{

namespace
{

/// The context of the page's reports (Guard).
QString context()
{
    return QStringLiteral("Simulation preferences");
}

/// MigLayout's paragraph gap ("wrap para", "gaptop para").
constexpr int kParagraphGap = 12;
/// Java: JTextField(12), the seed field's columns.
constexpr int kSeedFieldColumns = 12;
/// The constant gravity of "Reset to default" (Java's literal 9.807).
constexpr double kDefaultConstantGravity = 9.807;

// The grid's columns (Java: "[grow][min!][min!][]"): the labels, the fields, the unit selectors,
// the sliders (ConfigFields::addNumberRow() puts a number row's widgets there).
constexpr int kLabelColumn = 0;
constexpr int kFieldColumn = 1;
constexpr int kColumnCount = 4;

// The grid's rows; the empty ones are Java's paragraph gaps.
constexpr int kWarningRow      = 0;
constexpr int kWarningGapRow   = 1;
constexpr int kCalculationRow  = 2;
constexpr int kMethodRow       = 3;
constexpr int kGeodeticRow     = 4;
constexpr int kGravityModelRow = 5;
constexpr int kGravityValueRow = 6;
constexpr int kTimeStepRow     = 7;
constexpr int kMaximumTimeRow  = 8;
constexpr int kSeedGapRow      = 9;
constexpr int kSeedRow         = 10;

/// A source of the double property of the preferences read by @p get and written by @p set (Java:
/// DoubleModel(preferences, "<Name>")).
[[nodiscard]] DoubleSource doubleSource(double (Preferences::*get)() const,
                                        void (Preferences::*set)(double))
{
    return DoubleSource::ofObject<Preferences>({
        .get = [get](const Preferences& preferences) { return std::invoke(get, preferences); },
        .set = [set](Preferences&  preferences,
                     const double& value) { std::invoke(set, preferences, value); },
    });
}

/// The preferences' geodetic computation (Java: getGeodeticComputation(), SPHERICAL without it).
[[nodiscard]] GeodeticComputationStrategy geodeticOf(const Preferences& preferences)
{
    return geodeticComputationStrategyFromString(preferences.getGeodeticComputationName())
        .value_or(GeodeticComputationStrategy::SPHERICAL);
}

/// The preferences' gravity model (Java: getGravityModel(), WGS without it).
[[nodiscard]] GravityModelType gravityModelOf(const Preferences& preferences)
{
    const std::string stored = preferences.getGravityModelName();
    for (const GravityModelType type : kAllGravityModelTypes)
    {
        if (gravityModelTypeName(type) == stored)
        {
            return type;
        }
    }
    return GravityModelType::WGS;
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

/// Whether a focus loss for @p reason is a temporary one (Java: FocusEvent.isTemporary()): the
/// window lost its activation, or a popup or the menu bar took the focus for a while.
[[nodiscard]] bool isTemporary(Qt::FocusReason reason) noexcept
{
    return reason == Qt::ActiveWindowFocusReason || reason == Qt::PopupFocusReason ||
           reason == Qt::MenuBarFocusReason;
}

/// Java's new Random().nextInt(): a seed drawn at random (the 32 bits as an int).
[[nodiscard]] int drawRandomSeed()
{
    return static_cast<int>(QRandomGenerator::global()->generate());
}

}  // namespace

SimulationPreferencesPanel::SimulationPreferencesPanel(Preferences&  preferences,
                                                       BindingScope& scope, QWidget* parent)
  : QWidget(parent),
    m_preferences(&preferences),
    m_selection(scope.keepContext(ObjectSelection::make<Preferences>(preferences)))
{
    setObjectName(QStringLiteral("simulationPreferencesPanel"));
    auto* page = makeChild<QVBoxLayout>(this);

    // Confirm deletion of simulations:
    //: pref.dlg.lbl.Confirmdeletion
    m_confirmDelete = makeChild<QCheckBox>(this, tr("Confirm deletion of simulations."));
    m_confirmDelete->setObjectName(QtStrings::fromUtf8(PreferenceKeys::kConfirmDeleteSimulation));
    m_confirmDelete->setChecked(preferences.getConfirmSimDeletion());
    connect(m_confirmDelete, &QCheckBox::clicked, this,
            guarded([this](bool checked) { m_preferences->setConfirmSimDeletion(checked); },
                    context()));
    page->addWidget(m_confirmDelete);

    // Automatically run all simulation out-dated by design changes.
    m_autoRun = makeChild<QCheckBox>(
        //: pref.dlg.checkbox.Runsimulations
        this, tr("Run out-dated simulations when you open the simulation tab."));
    m_autoRun->setObjectName(QtStrings::fromUtf8(PreferenceKeys::kAutoRunSimulations));
    m_autoRun->setChecked(preferences.getAutoRunSimulations());
    connect(m_autoRun, &QCheckBox::clicked, this,
            guarded([this](bool checked) { m_preferences->setAutoRunSimulations(checked); },
                    context()));
    page->addWidget(m_autoRun);
    page->addSpacing(kOptionsGap);

    // // Simulator options
    //: simedtdlg.border.Simopt
    m_optionsBox = makeChild<QGroupBox>(this, tr("Simulator options"));
    m_optionsBox->setObjectName(QStringLiteral("simulatorOptionsPanel"));
    // Separate panel for computation methods, as they use a different layout
    auto* gridWidget = makeChild<QWidget>(m_optionsBox);
    auto* grid       = makeChild<QGridLayout>(gridWidget);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setColumnStretch(kLabelColumn, 1);
    buildMethods(*gridWidget, *grid);
    buildGeodeticAndGravity(*gridWidget, *grid, scope);
    buildTimes(*grid, scope);
    buildRandomSeed(*gridWidget, *grid);

    // Reset to default button
    //: simedtdlg.but.resettodefault
    m_resetButton = makeChild<QPushButton>(m_optionsBox, tr("Reset to default"));
    m_resetButton->setObjectName(QStringLiteral("resetButton"));
    m_resetButton->setAutoDefault(false);
    // Reset the time step to its default value (
    m_resetButton->setToolTip(
        //: simedtdlg.but.ttip.resettodefault
        tr("Reset the time step to its default value (") +
        QtStrings::defaultUnitText(UnitGroupId::SHORT_TIME, Preferences::kRecommendedTimeStep) +
        QStringLiteral(")."));
    connect(m_resetButton, &QPushButton::clicked, this,
            guarded([this] { resetToDefault(); }, context()));

    // Java's sub panel (MigLayout("fill")): the grid ("spanx, wrap para"), the button on the left.
    auto* box = makeChild<QVBoxLayout>(m_optionsBox);
    box->addWidget(gridWidget);
    box->addSpacing(kParagraphGap);
    box->addWidget(m_resetButton, 0, Qt::AlignLeft);
    page->addWidget(m_optionsBox);
    page->addStretch(1);
}

SimulationPreferencesPanel::~SimulationPreferencesPanel() = default;

// -------------------------------------------------------------------------------- the methods

void SimulationPreferencesPanel::buildMethods(QWidget& parent, QGridLayout& grid)
{
    // // Warning
    m_launchWarning = makeChild<StyledLabel>(
        &parent,
        QStringLiteral("<html>") +
            //: pref.dlg.lbl.launchWarning
            tr("NOTE: the settings below apply only to newly created simulations. <br>To change "
               "the settings for an existing simulation, select that simulation and edit it.") +
            QStringLiteral("</html>"),
        0, StyledLabel::Style::BOLD);
    m_launchWarning->setObjectName(QStringLiteral("launchWarning"));
    m_launchWarning->setFontColor(ThemeColors::color(ThemeColors::Key::DARK_ERROR, palette()));
    //: pref.dlg.lbl.launchWarning.ttip
    m_launchWarning->setToolTip(tr("You would not be the first one to be confused by this :)"));
    grid.addWidget(m_launchWarning, kWarningRow, kLabelColumn, 1, kColumnCount);
    grid.setRowMinimumHeight(kWarningGapRow, kParagraphGap);

    // // Calculation method:
    //: simedtdlg.lbl.ttip.Calcmethod
    const QString calculationTip = tr(
        "<html>The Extended Barrowman method calculates aerodynamic forces according  <br>to the "
        "Barrowman equations extended to accommodate more components.");
    //: simedtdlg.lbl.Calcmethod
    m_calculationLabel = makeLabel(parent, tr("Calculation method:"), calculationTip);
    grid.addWidget(m_calculationLabel, kCalculationRow, kLabelColumn);
    // // Extended Barrowman
    //: simedtdlg.lbl.ExtBarrowman
    m_barrowmanLabel = makeLabel(parent, tr("Extended Barrowman"), calculationTip);
    grid.addWidget(m_barrowmanLabel, kCalculationRow, kFieldColumn, 1, kColumnCount - kFieldColumn);

    // Simulation method
    const QString methodTip =
        //: simedtdlg.lbl.ttip.Simmethod1
        tr("<html>The six degree-of-freedom simulator allows the rocket total freedom during "
           "flight. <br>") +
        //: simedtdlg.lbl.ttip.Simmethod2
        tr("Integration is performed using a 4<sup>th</sup> order Runge-Kutta 4 numerical "
           "integration.");
    //: simedtdlg.lbl.Simmethod
    m_methodLabel = makeLabel(parent, tr("Simulation method:"), methodTip);
    grid.addWidget(m_methodLabel, kMethodRow, kLabelColumn);
    // Java's literal: the page offers no other stepper.
    m_methodValue = makeLabel(parent, QStringLiteral("6-DOF Runge-Kutta 4"), methodTip);
    grid.addWidget(m_methodValue, kMethodRow, kFieldColumn, 1, kColumnCount - kFieldColumn);
}

// ------------------------------------------------------------- the geodetic method and gravity

void SimulationPreferencesPanel::buildGeodeticAndGravity(QWidget& parent, QGridLayout& grid,
                                                         BindingScope& scope)
{
    // // Geodetic calculation method:
    //: simedtdlg.lbl.GeodeticMethod
    m_geodeticLabel = makeLabel(parent, tr("Geodetic calculations:"),
                                //: simedtdlg.lbl.ttip.GeodeticMethodTip
                                tr("Relate to the calculation of coordinates on the earth.  This "
                                   "also enables coriolis effect computations."));
    grid.addWidget(m_geodeticLabel, kGeodeticRow, kLabelColumn);
    const auto geodetic = scope.make<EnumBinding<GeodeticComputationStrategy>>(
        m_selection,
        EnumSource<GeodeticComputationStrategy>::ofObject<Preferences>({
            .get = [](const Preferences& preferences) { return geodeticOf(preferences); },
            .set =
                [](Preferences& preferences, const GeodeticComputationStrategy& strategy) {
                    preferences.setGeodeticComputationName(name(strategy));
                },
        }),
        std::span<const GeodeticComputationStrategy>(kAllGeodeticComputationStrategies));
    m_geodeticCombo = makeChild<EnumComboBox>(&parent, geodetic);
    m_geodeticCombo->setObjectName(QStringLiteral("GeodeticComputation"));
    grid.addWidget(m_geodeticCombo, kGeodeticRow, kFieldColumn, 1, kColumnCount - kFieldColumn,
                   Qt::AlignLeft);
    const auto geodeticTooltip = [this, geodetic] {
        const GeodeticComputationStrategy strategy = geodetic->value();
        m_geodeticCombo->setToolTip(
            translatedEnumText(descriptionKey(strategy), description(strategy)));
    };
    geodeticTooltip();
    m_geodeticConnection = geodetic->addChangeListener(geodeticTooltip);

    // // Gravity model:
    //: simedtdlg.lbl.GravityModel
    m_gravityModelLabel = makeLabel(parent, tr("Gravity model:"),
                                    //: simedtdlg.lbl.ttip.GravityModel
                                    tr("Select the gravity model to use during the simulation."));
    grid.addWidget(m_gravityModelLabel, kGravityModelRow, kLabelColumn);
    const auto gravity = scope.make<EnumBinding<GravityModelType>>(
        m_selection,
        EnumSource<GravityModelType>::ofObject<Preferences>({
            .get = [](const Preferences& preferences) { return gravityModelOf(preferences); },
            .set =
                [](Preferences& preferences, const GravityModelType& type) {
                    preferences.setGravityModelName(gravityModelTypeName(type));
                },
        }),
        std::span<const GravityModelType>(kAllGravityModelTypes));
    m_gravityCombo = makeChild<EnumComboBox>(&parent, gravity);
    m_gravityCombo->setObjectName(QStringLiteral("GravityModel"));
    grid.addWidget(m_gravityCombo, kGravityModelRow, kFieldColumn, 1, kColumnCount - kFieldColumn,
                   Qt::AlignLeft);

    // // Constant gravity value:
    const auto value = scope.make<DoubleBinding>(
        m_selection,
        doubleSource(&Preferences::getConstantGravityValue, &Preferences::setConstantGravityValue),
        unitGroup(UnitGroupId::ACCELERATION),
        DoubleBinding::Options{.min = 0, .max = SimulationInputRanges::kConstantGravity.max});
    //: simedtdlg.lbl.GravityValue
    m_gravityValue = ConfigFields::addNumberRow(grid, kGravityValueRow, tr("Gravity value:"), value,
                                                QStringLiteral("ConstantGravityValue"),
                                                {.slider = SliderMapping::linear(0, 20)});
    //: simedtdlg.lbl.ttip.GravityValue
    setRowToolTip(m_gravityValue, tr("Constant gravitational acceleration value"));
    m_gravityValue.slider->setFixedWidth(kSliderWidth);

    // The tooltip and the visibility of the constant gravity's row follow the model.
    const auto gravityChanged = [this, gravity] {
        m_gravityCombo->setToolTip(
            translatedEnumText(tooltipKey(gravity->value()), tooltip(gravity->value())));
        showGravityValue();
    };
    gravityChanged();
    m_gravityConnection = gravity->addChangeListener(gravityChanged);
}

void SimulationPreferencesPanel::showGravityValue()
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

// ---------------------------------------------------------------------------------- the times

void SimulationPreferencesPanel::buildTimes(QGridLayout& grid, BindingScope& scope)
{
    // // Time step:
    const QString timeStepTip =
        //: simedtdlg.lbl.ttip.Timestep1
        tr("<html>The time between simulation steps. <br>A smaller time step results in a more "
           "accurate but slower simulation. <br>") +
        //: simedtdlg.lbl.ttip.Timestep2
        tr("The 4<sup>th</sup> order simulation method is quite accurate with a time step of") +
        QStringLiteral(" ") +
        QtStrings::defaultUnitText(UnitGroupId::TIME_STEP, Preferences::kRecommendedTimeStep) +
        QStringLiteral(".");
    const auto timeStep = scope.make<DoubleBinding>(
        m_selection, doubleSource(&Preferences::getTimeStep, &Preferences::setTimeStep),
        unitGroup(UnitGroupId::TIME_STEP), DoubleBinding::Options{.min = 0.01, .max = 1});
    //: simedtdlg.lbl.Timestep
    m_timeStep = ConfigFields::addNumberRow(grid, kTimeStepRow, tr("Time step:"), timeStep,
                                            QStringLiteral("TimeStep"),
                                            {.slider = SliderMapping::linear(0.01, 0.2)});
    setRowToolTip(m_timeStep, timeStepTip);
    m_timeStep.slider->setFixedWidth(kSliderWidth);

    // // Maximum simulation time:
    const auto maximumTime = scope.make<DoubleBinding>(
        m_selection,
        doubleSource(&Preferences::getMaxSimulationTime, &Preferences::setMaxSimulationTime),
        unitGroup(UnitGroupId::LONG_TIME),
        DoubleBinding::Options{.min = 1, .max = SimulationInputRanges::kMaxSimulationTime.max});
    //: simedtdlg.lbl.MaxSimTime
    m_maximumTime =
        ConfigFields::addNumberRow(grid, kMaximumTimeRow, tr("Maximum simulation time:"),
                                   maximumTime, QStringLiteral("MaxSimulationTime"), {});
    //: simedtdlg.lbl.ttip.MaxSimTime
    setRowToolTip(m_maximumTime, tr("<html>The maximum time the simulation will run."));
}

// ---------------------------------------------------------------------------------- the seed

void SimulationPreferencesPanel::buildRandomSeed(QWidget& parent, QGridLayout& grid)
{
    grid.setRowMinimumHeight(kSeedGapRow, kParagraphGap);
    // // Default fixed random seed:
    //: simedtdlg.checkbox.FixedRandomSeed
    m_seedCheckBox = makeChild<QCheckBox>(&parent, tr("Use fixed random seed:"));
    m_seedCheckBox->setObjectName(QStringLiteral("fixedRandomSeedCheckBox"));
    //: simedtdlg.checkbox.ttip.FixedRandomSeed
    m_seedCheckBox->setToolTip(tr("Reuse the same random seed for reproducible simulation runs."));
    m_seedCheckBox->setChecked(m_preferences->isRandomSeedFixed());
    grid.addWidget(m_seedCheckBox, kSeedRow, kLabelColumn);

    m_seedField = makeChild<QLineEdit>(&parent);
    m_seedField->setObjectName(QStringLiteral("randomSeedField"));
    //: simedtdlg.lbl.ttip.RandomSeed
    m_seedField->setToolTip(
        tr("A signed 32-bit integer used to initialize simulation randomness."));
    m_seedField->setMinimumWidth(
        std::max(kSeedFieldWidth,
                 m_seedField->fontMetrics().horizontalAdvance(QChar(u'0')) * kSeedFieldColumns));
    showRandomSeed();
    grid.addWidget(m_seedField, kSeedRow, kFieldColumn, 1, kColumnCount - kFieldColumn,
                   Qt::AlignLeft);

    m_seedValidator =
        &FieldValidator::of(*m_seedField)
             .errorIf(
                 [this] {
                     return m_seedCheckBox->isChecked() &&
                            !RandomSeedControls::parseRandomSeed(m_seedField->text()).has_value();
                 },
                 //: simedtdlg.error.RandomSeed
                 tr("Enter a signed 32-bit integer."))
             .showMessagePopup(FieldValidator::kMessagePopupDuration);
    m_seedValidator->update();

    connect(m_seedCheckBox, &QCheckBox::clicked, this,
            guarded([this](bool checked) { seedCheckBoxClicked(checked); }, context()));
    connect(m_seedField, &QLineEdit::textChanged, this,
            guarded(
                [this] {
                    seedFromField();
                    m_seedValidator->update();
                },
                context()));
    m_seedField->installEventFilter(this);
}

void SimulationPreferencesPanel::seedCheckBoxClicked(bool checked)
{
    if (m_updatingSeed)
    {
        return;
    }
    if (checked)
    {
        m_preferences->setRandomSeed(drawRandomSeed());
        m_preferences->setRandomSeedFixed(true);
    }
    else
    {
        m_preferences->setRandomSeedFixed(false);
    }
    showRandomSeed();
    m_seedValidator->update();
}

void SimulationPreferencesPanel::seedFromField()
{
    if (m_updatingSeed || !m_seedCheckBox->isChecked())
    {
        return;
    }
    if (const std::optional<int> seed = RandomSeedControls::parseRandomSeed(m_seedField->text());
        seed.has_value())
    {
        m_preferences->setRandomSeed(*seed);
    }
}

void SimulationPreferencesPanel::replaceInvalidRandomSeed()
{
    if (!m_seedCheckBox->isChecked() ||
        RandomSeedControls::parseRandomSeed(m_seedField->text()).has_value())
    {
        return;
    }
    m_preferences->setRandomSeed(drawRandomSeed());
    showRandomSeed();
    m_seedValidator->update();
}

void SimulationPreferencesPanel::showRandomSeed()
{
    m_updatingSeed   = true;
    const bool fixed = m_preferences->isRandomSeedFixed();
    m_seedCheckBox->setChecked(fixed);
    m_seedField->setEnabled(fixed);
    m_seedField->setText(fixed ? QString::number(m_preferences->getRandomSeed()) : QString());
    m_updatingSeed = false;
}

void SimulationPreferencesPanel::prepareForSave()
{
    const std::optional<int> seed = m_seedCheckBox->isChecked()
                                        ? RandomSeedControls::parseRandomSeed(m_seedField->text())
                                        : std::optional<int>();
    if (!seed.has_value())
    {
        m_preferences->setRandomSeedFixed(false);
    }
    else
    {
        m_preferences->setRandomSeed(*seed);
        m_preferences->setRandomSeedFixed(true);
    }
    showRandomSeed();
    m_seedValidator->update();
}

bool SimulationPreferencesPanel::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_seedField && event->type() == QEvent::FocusOut)
    {
        const auto* focus = dynamic_cast<const QFocusEvent*>(event);
        if (focus != nullptr && !isTemporary(focus->reason()))
        {
            Guard::run([this] { replaceInvalidRandomSeed(); }, context());
        }
    }
    return QWidget::eventFilter(watched, event);
}

// ------------------------------------------------------------------------------ reset to default

void SimulationPreferencesPanel::commitEdits()
{
    for (UnitSpinBox* spinBox : findChildren<UnitSpinBox*>())
    {
        spinBox->commitEdit();
    }
}

void SimulationPreferencesPanel::resetToDefault()
{
    commitEdits();
    if (!m_selection->isValid())
    {
        return;
    }
    m_preferences->setTimeStep(Preferences::kRecommendedTimeStep);
    m_preferences->setMaxSimulationTime(Preferences::kRecommendedMaxTime);
    m_preferences->setGeodeticComputationName(name(GeodeticComputationStrategy::SPHERICAL));
    m_preferences->setGravityModelName(gravityModelTypeName(GravityModelType::WGS));
    m_preferences->setConstantGravityValue(kDefaultConstantGravity);
    m_preferences->setRandomSeedFixed(false);
    // The enum setters announce nothing (putEnum()): the fields read the preferences again.
    m_selection->refresh();
    showRandomSeed();
    m_seedValidator->update();
}

}  // namespace QtRocket::Gui
