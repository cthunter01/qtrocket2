#include "QtRocket/gui/configdialog/EllipticalFinSetConfig.h"

#include <memory>
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
#include "QtRocket/rocket/EllipticalFinSet.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/BugError.h"

namespace QtRocket::Gui
{

namespace
{

/// The sliders of the root chord and the height: 0, 5 cm, 20 cm (Java: getSliderModel(0, 0.05,
/// 0.2)).
constexpr double kOutlineSliderMid = 0.05;
constexpr double kOutlineSliderMax = 0.2;
/// The gap before the right column (Java: "gapright 40lp").
constexpr int kColumnGap = 40;

/// The rows of the General tab, and the row below them, which takes the extra height.
constexpr int kFinCountRow     = 0;
constexpr int kCantRow         = 1;
constexpr int kRootChordRow    = 2;
constexpr int kHeightRow       = 3;
constexpr int kCrossSectionRow = 4;
constexpr int kThicknessRow    = 5;
constexpr int kRowCount        = 6;

/// The columns after ConfigFields' four: a gap, the right column.
constexpr int kSliderColumn = 3;
constexpr int kGapColumn    = 4;
constexpr int kRightColumn  = 5;

/// @p selection, whose main component must be an EllipticalFinSet.
/// @throws BugError when it is not one (or the selection is null or invalid)
[[nodiscard]] std::shared_ptr<ComponentSelection> ellipticalSelection(
    std::shared_ptr<ComponentSelection> selection)
{
    if (selection != nullptr && selection->isValid() &&
        dynamic_cast<const EllipticalFinSet*>(&selection->getMain()) == nullptr)
    {
        bug("The elliptical fin set configuration needs an elliptical fin set");
    }
    return selection;  // a null or invalid one is RocketComponentConfig's BugError
}

/// The root chord, the fin set's length (Java: DoubleModel(component, "Length", UNITS_LENGTH,
/// 0)).
[[nodiscard]] DoubleSource lengthSource()
{
    return DoubleSource::of<EllipticalFinSet>({
        .get = [](const EllipticalFinSet& fins) { return fins.getLength(); },
        .set = [](EllipticalFinSet& fins, const double& length) { fins.setLength(length); },
    });
}

/// The height (Java: DoubleModel(component, "Height", UNITS_LENGTH, 0)).
[[nodiscard]] DoubleSource heightSource()
{
    return DoubleSource::of<EllipticalFinSet>({
        .get = [](const EllipticalFinSet& fins) { return fins.getHeight(); },
        .set = [](EllipticalFinSet& fins, const double& height) { fins.setHeight(height); },
    });
}

}  // namespace

EllipticalFinSetConfig::EllipticalFinSetConfig(OpenRocketDocument&                 document,
                                               const GuiServices&                  services,
                                               std::shared_ptr<ComponentSelection> selection,
                                               QWidget*                            parent)
  : FinSetConfig(document, services, ellipticalSelection(std::move(selection)), parent)
{
    QWidget& general = makeGeneralTab();

    addFinSetButtons();

    //: EllipticalFinSetCfg.General
    insertTab(0, general, tr("General"),
              //: EllipticalFinSetCfg.Generalproperties
              tr("General properties"));
    setSelectedTabIndex(0);
}

QWidget& EllipticalFinSetConfig::makeGeneralTab()
{
    auto*                                      page        = makeChild<QWidget>(this);
    auto*                                      grid        = makeChild<QGridLayout>(page);
    BindingScope&                              bindings    = scope();
    const std::shared_ptr<ComponentSelection>& selected    = selection();
    const UnitGroup&                           lengthUnits = unitGroup(UnitGroupId::LENGTH);
    const SliderMapping                        slider =
        SliderMapping::fromRange(0.0, kOutlineSliderMid, kOutlineSliderMax);

    // Number of fins (no tooltip in OpenRocket's panel)
    //: EllipticalFinSetCfg.Nbroffins
    addFinCountRow(*page, *grid, kFinCountRow, tr("Number of fins:"), QString());
    // Fin cant
    //: EllipticalFinSetCfg.Fincant
    addCantRow(*grid, kCantRow, tr("Fin cant:"),
               //: EllipticalFinSetCfg.ttip.Fincant
               tr("The angle that the fins are canted with respect to the rocket body."));
    // Root chord
    m_rootChord = ConfigFields::addNumberRow(
        *grid, kRootChordRow,
        //: EllipticalFinSetCfg.Rootchord
        tr("Root chord:"),
        bindings.make<DoubleBinding>(selected, lengthSource(), lengthUnits,
                                     DoubleBinding::Options{.min = 0.0}),
        QStringLiteral("Length"), {.slider = slider});
    // Height
    m_height = ConfigFields::addNumberRow(
        *grid, kHeightRow,
        //: EllipticalFinSetCfg.Height
        tr("Height:"),
        bindings.make<DoubleBinding>(selected, heightSource(), lengthUnits,
                                     DoubleBinding::Options{.min = 0.0}),
        QStringLiteral("Height"), {.slider = slider});
    // Fin cross section:
    //: EllipticalFinSetCfg.FincrossSection
    addCrossSectionRow(*page, *grid, kCrossSectionRow, tr("Fin cross section:"));
    // Thickness:
    //: EllipticalFinSetCfg.Thickness
    addThicknessRow(*grid, kThicknessRow, tr("Thickness:"));

    // The right side: the placement with the fin rotation (unbounded), the material and the root
    // fillets.
    grid->setColumnMinimumWidth(kGapColumn, kColumnGap);
    grid->addWidget(&makeRightColumn(*page, false), 0, kRightColumn, kRowCount + 1, 1,
                    Qt::AlignTop);
    grid->setColumnStretch(kSliderColumn, 1);
    grid->setRowStretch(kRowCount, 1);
    return *page;
}

}  // namespace QtRocket::Gui
