#include "QtRocket/gui/configdialog/TrapezoidFinSetConfig.h"

#include <memory>
#include <numbers>
#include <utility>

#include <QGridLayout>
#include <QString>
#include <QWidget>
#include <Qt>

#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/gui/binding/ComponentSelection.h"
#include "QtRocket/gui/binding/DoubleBinding.h"
#include "QtRocket/gui/binding/PropertySource.h"
#include "QtRocket/gui/binding/SliderMapping.h"
#include "QtRocket/gui/configdialog/ConfigFields.h"
#include "QtRocket/gui/configdialog/FinSetConfig.h"
#include "QtRocket/gui/util/GuiServices.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/widgets/BindingScope.h"
#include "QtRocket/rocket/TrapezoidFinSet.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/BugError.h"

namespace QtRocket::Gui
{

namespace
{

/// The sliders of the root chord, the tip chord and the height: 0, 5 cm, 20 cm (Java:
/// getSliderModel(0, 0.05, 0.2)).
constexpr double kOutlineSliderMid = 0.05;
constexpr double kOutlineSliderMax = 0.2;
/// The bounds of the sweep length's slider: -1.1 times the tip chord, 1.1 times the root chord
/// (Java: DoubleModel(component, "TipChord", -1.1, ...) and (component, "RootChord", 1.1, ...)).
constexpr double kSweepSliderFactor = 1.1;
/// The gap before the right column (Java: "gapright 40lp").
constexpr int kColumnGap = 40;

/// The rows of the General tab, and the row below them, which takes the extra height.
constexpr int kFinCountRow     = 0;
constexpr int kCantRow         = 1;
constexpr int kRootChordRow    = 2;
constexpr int kTipChordRow     = 3;
constexpr int kHeightRow       = 4;
constexpr int kSweepRow        = 5;
constexpr int kSweepAngleRow   = 6;
constexpr int kCrossSectionRow = 7;
constexpr int kThicknessRow    = 8;
constexpr int kRowCount        = 9;

/// The columns after ConfigFields' four: a gap, the right column.
constexpr int kSliderColumn = 3;
constexpr int kGapColumn    = 4;
constexpr int kRightColumn  = 5;

/// @p selection, whose main component must be a TrapezoidFinSet.
/// @throws BugError when it is not one (or the selection is null or invalid)
[[nodiscard]] std::shared_ptr<ComponentSelection> trapezoidSelection(
    std::shared_ptr<ComponentSelection> selection)
{
    if (selection != nullptr && selection->isValid() &&
        dynamic_cast<const TrapezoidFinSet*>(&selection->getMain()) == nullptr)
    {
        bug("The trapezoidal fin set configuration needs a trapezoidal fin set");
    }
    return selection;  // a null or invalid one is RocketComponentConfig's BugError
}

/// The root chord (Java: DoubleModel(component, "RootChord", UNITS_LENGTH, 0)).
[[nodiscard]] DoubleSource rootChordSource()
{
    return DoubleSource::of<TrapezoidFinSet>({
        .get = [](const TrapezoidFinSet& fins) { return fins.getRootChord(); },
        .set = [](TrapezoidFinSet& fins, const double& chord) { fins.setRootChord(chord); },
    });
}

/// The tip chord (Java: DoubleModel(component, "TipChord", UNITS_LENGTH, 0)).
[[nodiscard]] DoubleSource tipChordSource()
{
    return DoubleSource::of<TrapezoidFinSet>({
        .get = [](const TrapezoidFinSet& fins) { return fins.getTipChord(); },
        .set = [](TrapezoidFinSet& fins, const double& chord) { fins.setTipChord(chord); },
    });
}

/// The height (Java: DoubleModel(component, "Height", UNITS_LENGTH, 0)).
[[nodiscard]] DoubleSource heightSource()
{
    return DoubleSource::of<TrapezoidFinSet>({
        .get = [](const TrapezoidFinSet& fins) { return fins.getHeight(); },
        .set = [](TrapezoidFinSet& fins, const double& height) { fins.setHeight(height); },
    });
}

/// The sweep length (Java: DoubleModel(component, "Sweep", UNITS_LENGTH)).
[[nodiscard]] DoubleSource sweepSource()
{
    return DoubleSource::of<TrapezoidFinSet>({
        .get = [](const TrapezoidFinSet& fins) { return fins.getSweep(); },
        .set = [](TrapezoidFinSet& fins, const double& sweep) { fins.setSweep(sweep); },
    });
}

/// The sweep angle (Java: DoubleModel(component, "SweepAngle", UNITS_ANGLE, ...)).
[[nodiscard]] DoubleSource sweepAngleSource()
{
    return DoubleSource::of<TrapezoidFinSet>({
        .get = [](const TrapezoidFinSet& fins) { return fins.getSweepAngle(); },
        .set = [](TrapezoidFinSet& fins, const double& angle) { fins.setSweepAngle(angle); },
    });
}

}  // namespace

TrapezoidFinSetConfig::TrapezoidFinSetConfig(OpenRocketDocument&                 document,
                                             const GuiServices&                  services,
                                             std::shared_ptr<ComponentSelection> selection,
                                             QWidget*                            parent)
  : FinSetConfig(document, services, trapezoidSelection(std::move(selection)), parent)
{
    //: TrapezoidFinSetCfg.tab.General
    insertTab(0, makeGeneralTab(), tr("General"),
              //: TrapezoidFinSetCfg.tab.Generalproperties
              tr("General properties"));
    setSelectedTabIndex(0);

    addFinSetButtons();
}

QWidget& TrapezoidFinSetConfig::makeGeneralTab()
{
    auto* page = makeChild<QWidget>(this);
    auto* grid = makeChild<QGridLayout>(page);

    // Number of fins:
    //: TrapezoidFinSetCfg.lbl.Nbroffins
    addFinCountRow(*page, *grid, kFinCountRow, tr("Number of fins:"),
                   //: TrapezoidFinSetCfg.lbl.ttip.Nbroffins
                   tr("The number of fins in the fin set."));
    // Fin cant:
    //: TrapezoidFinSetCfg.lbl.Fincant
    addCantRow(*grid, kCantRow, tr("Fin cant:"),
               //: TrapezoidFinSetCfg.lbl.ttip.Fincant
               tr("The angle that the fins are canted with respect to the rocket body."));
    addOutlineRows(*grid);
    addSweepRows(*grid);
    // Fin cross section:
    //: TrapezoidFinSetCfg.lbl.FincrossSection
    addCrossSectionRow(*page, *grid, kCrossSectionRow, tr("Fin cross section:"));
    // Thickness:
    //: TrapezoidFinSetCfg.lbl.Thickness
    addThicknessRow(*grid, kThicknessRow, tr("Thickness:"));

    // The right side: the placement with the fin rotation (bounded, Java's -PI ... PI), the
    // material and the root fillets.
    grid->setColumnMinimumWidth(kGapColumn, kColumnGap);
    grid->addWidget(&makeRightColumn(*page, true), 0, kRightColumn, kRowCount + 1, 1, Qt::AlignTop);
    grid->setColumnStretch(kSliderColumn, 1);
    grid->setRowStretch(kRowCount, 1);
    return *page;
}

void TrapezoidFinSetConfig::addOutlineRows(QGridLayout& grid)
{
    BindingScope&                              bindings    = scope();
    const std::shared_ptr<ComponentSelection>& selected    = selection();
    const UnitGroup&                           lengthUnits = unitGroup(UnitGroupId::LENGTH);
    const SliderMapping                        slider =
        SliderMapping::fromRange(0.0, kOutlineSliderMid, kOutlineSliderMax);

    // Root chord:
    m_rootChord = ConfigFields::addNumberRow(
        grid, kRootChordRow,
        //: TrapezoidFinSetCfg.lbl.Rootchord
        tr("Root chord:"),
        bindings.make<DoubleBinding>(selected, rootChordSource(), lengthUnits,
                                     DoubleBinding::Options{.min = 0.0}),
        QStringLiteral("RootChord"), {.slider = slider});
    // Tip chord:
    m_tipChord = ConfigFields::addNumberRow(
        grid, kTipChordRow,
        //: TrapezoidFinSetCfg.lbl.Tipchord
        tr("Tip chord:"),
        bindings.make<DoubleBinding>(selected, tipChordSource(), lengthUnits,
                                     DoubleBinding::Options{.min = 0.0}),
        QStringLiteral("TipChord"), {.slider = slider});
    // Height:
    m_height = ConfigFields::addNumberRow(
        grid, kHeightRow,
        //: TrapezoidFinSetCfg.lbl.Height
        tr("Height:"),
        bindings.make<DoubleBinding>(selected, heightSource(), lengthUnits,
                                     DoubleBinding::Options{.min = 0.0}),
        QStringLiteral("Height"), {.slider = slider});
}

void TrapezoidFinSetConfig::addSweepRows(QGridLayout& grid)
{
    BindingScope&                              bindings    = scope();
    const std::shared_ptr<ComponentSelection>& selected    = selection();
    const UnitGroup&                           lengthUnits = unitGroup(UnitGroupId::LENGTH);

    // Sweep length: its slider from -1.1 times the tip chord to 1.1 times the root chord.
    const auto tipBound =
        bindings.make<DoubleBinding>(selected, tipChordSource(), lengthUnits,
                                     DoubleBinding::Options{.multiplier = -kSweepSliderFactor});
    const auto rootBound =
        bindings.make<DoubleBinding>(selected, rootChordSource(), lengthUnits,
                                     DoubleBinding::Options{.multiplier = kSweepSliderFactor});
    m_sweep = ConfigFields::addNumberRow(
        grid, kSweepRow,
        //: TrapezoidFinSetCfg.lbl.Sweeplength
        tr("Sweep length:"),
        bindings.make<DoubleBinding>(selected, sweepSource(), lengthUnits,
                                     DoubleBinding::Options{}),
        QStringLiteral("Sweep"),
        {.slider = SliderMapping::linear(tipBound->value(), rootBound->value()),
         .bounds = {.min = tipBound, .max = rootBound}});

    // Sweep angle:
    m_sweepAngle = ConfigFields::addNumberRow(
        grid, kSweepAngleRow,
        //: TrapezoidFinSetCfg.lbl.Sweepangle
        tr("Sweep angle:"),
        bindings.make<DoubleBinding>(
            selected, sweepAngleSource(), unitGroup(UnitGroupId::ANGLE),
            DoubleBinding::Options{.min = -TrapezoidFinSet::kMaxSweepAngle,
                                   .max = TrapezoidFinSet::kMaxSweepAngle}),
        QStringLiteral("SweepAngle"),
        {.slider = SliderMapping::linear(-std::numbers::pi / 4, std::numbers::pi / 4)});
}

}  // namespace QtRocket::Gui
