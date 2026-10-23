#include "QtRocket/gui/configdialog/FinSetConfig.h"

#include <array>
#include <cstddef>
#include <memory>
#include <numbers>
#include <optional>
#include <string>

#include <QCoreApplication>
#include <QEvent>
#include <QObject>
#include <QPushButton>
#include <QString>
#include <QTest>
#include <QWidget>
#include <QtWidgets/qtestsupport_widgets.h>

#include <gtest/gtest.h>

#include "BodyPanelTestSupport.h"
#include "ConfigDialogTestSupport.h"
#include "PanelReplay.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/gui/binding/ComponentSelection.h"
#include "QtRocket/gui/binding/DoubleBinding.h"
#include "QtRocket/gui/configdialog/ComponentConfigDialog.h"
#include "QtRocket/gui/configdialog/RocketComponentConfig.h"
#include "QtRocket/gui/configdialog/TrapezoidFinSetConfig.h"
#include "QtRocket/gui/util/GuiServices.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/widgets/UnitSpinBox.h"
#include "QtRocket/material/Material.h"
#include "QtRocket/material/MaterialPreferences.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/EllipticalFinSet.h"
#include "QtRocket/rocket/FinSet.h"
#include "QtRocket/rocket/FreeformFinSet.h"
#include "QtRocket/rocket/InnerTube.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/TrapezoidFinSet.h"
#include "QtRocket/unit/Unit.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/Uuid.h"
#include "widgets/WidgetTestSupport.h"

namespace QtRocket::Gui
{

namespace
{

using FinSetConfigTest = Test::ConfigDialogTest;
using Test::ConfigDialogServices;
using Test::EveryKindDesign;
using Test::FactoryPanel;
using Test::onBodyTube;
using Test::typeInto;

/// The trapezoidal fin set of @p design.
[[nodiscard]] TrapezoidFinSet& trapezoid(const EveryKindDesign& design)
{
    return dynamic_cast<TrapezoidFinSet&>(design.component(ComponentKind::TRAPEZOID_FIN_SET));
}

/// Adds a trapezoidal fin set named @p name to the body tube of @p design.
TrapezoidFinSet& addTrapezoid(const EveryKindDesign& design, const std::string& name)
{
    auto& fins =
        design.component(ComponentKind::BODY_TUBE).addChild(std::make_unique<TrapezoidFinSet>());
    fins.setName(name);
    return fins;
}

/// The fin set with the id @p id in @p design's rocket; null when there is none.
[[nodiscard]] const FinSet* finSetWithId(const EveryKindDesign& design, const Uuid& id)
{
    return dynamic_cast<const FinSet*>(design.document->getRocket().findComponent(id));
}

TEST_F(FinSetConfigTest, CalculateAutomaticallyIsAnUndoStepOfItsOwn)
{
    // Java's startUndo("Compute fin tabs") and stopUndo(): the tab between the motor mount's
    // centering rings, undone in one step.
    const ConfigDialogServices services;
    const EveryKindDesign      design;
    const auto& body = dynamic_cast<const BodyTube&>(design.component(ComponentKind::BODY_TUBE));
    TrapezoidFinSet& fins      = trapezoid(design);
    const Uuid       id        = fins.getId();
    const double     tabLength = fins.getTabLength();
    const double     tabHeight = fins.getTabHeight();
    {
        const FactoryPanel<TrapezoidFinSetConfig> panel(*design.document, services, fins);
        ASSERT_NE(panel.get(), nullptr);
        panel->autoCalcButton().click();
        EXPECT_EQ(design.document->getUndoDescription(),
                  std::optional<std::string>("Compute fin tabs"));
        // Some tab, down to the largest inner tube under the fin or to the axis.
        EXPECT_GT(fins.getTabHeight(), 0.0);
        EXPECT_LE(fins.getTabHeight(), body.getOuterRadius());
    }
    design.document->undo();
    const FinSet* restored = finSetWithId(design, id);
    ASSERT_NE(restored, nullptr);
    EXPECT_EQ(restored->getTabLength(), tabLength);
    EXPECT_EQ(restored->getTabHeight(), tabHeight);
}

TEST_F(FinSetConfigTest, CalculateAutomaticallyGivesEverySelectedFinSetTheMainOnesTab)
{
    // The main fin set's tab, through the fields, reaches every selected one (Java: the models
    // forward to the config listeners); each clamps the height to what its parent allows.
    const ConfigDialogServices services;
    const EveryKindDesign      design;
    TrapezoidFinSet&           fins   = trapezoid(design);
    TrapezoidFinSet&           second = addTrapezoid(design, "Second");
    second.setRootChord(0.2);
    const std::array<RocketComponent*, 1>     others{&second};
    const FactoryPanel<TrapezoidFinSetConfig> panel(*design.document, services, fins, others);
    ASSERT_NE(panel.get(), nullptr);
    panel->calculateAutoTab();
    EXPECT_EQ(second.getTabLength(), fins.getTabLength());
    EXPECT_EQ(second.getTabOffsetMethod(), fins.getTabOffsetMethod());
    EXPECT_EQ(second.getTabHeight(), fins.getTabHeight());
}

TEST_F(FinSetConfigTest, CalculateAutomaticallyShowsTheTabHeightInTheDefaultUnit)
{
    // Java: tabHeightModel.setCurrentUnit(UnitGroup.UNITS_LENGTH.getDefaultUnit()).
    const ConfigDialogServices                services;
    const EveryKindDesign                     design;
    const FactoryPanel<TrapezoidFinSetConfig> panel(*design.document, services, trapezoid(design));
    ASSERT_NE(panel.get(), nullptr);
    DoubleBinding&   height = *panel->tabHeightWidgets().spinBox->binding();
    const UnitGroup& units  = unitGroup(UnitGroupId::LENGTH);
    const Unit*      mm     = units.getUnit("mm");
    ASSERT_NE(mm, nullptr);
    ASSERT_NE(mm, &units.getDefaultUnit());
    height.setCurrentUnit(*mm);
    panel->calculateAutoTab();
    EXPECT_EQ(&height.getCurrentUnit(), &units.getDefaultUnit());
}

/// A trapezoidal fin set on the probes' body tube (radius 2.1 cm, 30 cm long) beside an inner
/// tube as long as the body and wider than it: no tab height fits.
struct FinsOverAWideTube
{
    TrapezoidFinSet*                    fins{nullptr};
    std::unique_ptr<OpenRocketDocument> document = onBodyTube([this](BodyTube& body) {
        fins       = &body.addChild(std::make_unique<TrapezoidFinSet>());
        auto& tube = body.addChild(std::make_unique<InnerTube>());
        tube.setOuterRadius(0.03);
        tube.setLength(0.3);
    });
};

TEST_F(FinSetConfigTest, CalculateAutomaticallyKeepsTheHeightAndItsUnitWithoutAHeight)
{
    // Java sets the tab height, and its unit to the default one, only for a height of 0 or more
    // (FinSetConfig.java, "if (height >= 0)"): here the tube under the fin is wider than the body.
    const ConfigDialogServices                services;
    const FinsOverAWideTube                   design;
    const FactoryPanel<TrapezoidFinSetConfig> panel(*design.document, services, *design.fins);
    ASSERT_NE(panel.get(), nullptr);
    DoubleBinding&   height = *panel->tabHeightWidgets().spinBox->binding();
    const UnitGroup& units  = unitGroup(UnitGroupId::LENGTH);
    const Unit*      inch   = units.getUnit("in");
    ASSERT_NE(inch, nullptr);
    ASSERT_NE(inch, &units.getDefaultUnit());
    height.setCurrentUnit(*inch);
    const double before = design.fins->getTabHeight();
    panel->calculateAutoTab();
    EXPECT_EQ(design.fins->getTabHeight(), before);
    EXPECT_EQ(&height.getCurrentUnit(), inch);
}

TEST_F(FinSetConfigTest, ConvertToFreeformReopensTheDialogOnEverySelectedFinSet)
{
    // Three fin sets selected: all three are converted, keeping their ids, and the dialog opens
    // again on them (OpenRocket converts only the first two, see
    // FinSetConfig::convertToFreeform()).
    const ConfigDialogServices            services;
    EveryKindDesign                       design;
    QWidget                               window;
    TrapezoidFinSet&                      fins   = trapezoid(design);
    TrapezoidFinSet&                      second = addTrapezoid(design, "Second");
    TrapezoidFinSet&                      third  = addTrapezoid(design, "Third");
    const std::array<Uuid, 3>             ids{fins.getId(), second.getId(), third.getId()};
    const std::array<RocketComponent*, 2> others{&second, &third};
    ComponentConfigDialog::showDialog(window, services.services(), *design.document, fins, others);
    const ComponentConfigDialog* dialog = ComponentConfigDialog::dialog();
    ASSERT_NE(dialog, nullptr);
    const auto* panel = qobject_cast<const TrapezoidFinSetConfig*>(&dialog->configurator());
    ASSERT_NE(panel, nullptr);
    ASSERT_NE(panel->convertButton(), nullptr);

    panel->convertButton()->click();
    EXPECT_TRUE(dialog->isDisposed());
    // The old dialog goes (deleted later); the new one is the window's.
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    const ComponentConfigDialog* reopened = ComponentConfigDialog::dialog();
    ASSERT_NE(reopened, nullptr);
    EXPECT_EQ(reopened->parentWidget(), &window);
    EXPECT_EQ(reopened->windowTitle(),
              QStringLiteral("(Multi-component) Freeform Fin Set configuration"));
    const ComponentSelection& selection = *reopened->configurator().selection();
    EXPECT_EQ(selection.getMain().getId(), ids[0]);
    ASSERT_EQ(selection.getOthers().size(), 2U);
    EXPECT_EQ(selection.getOthers()[0]->getId(), ids[1]);
    EXPECT_EQ(selection.getOthers()[1]->getId(), ids[2]);
    for (const Uuid& id : ids)
    {
        EXPECT_NE(dynamic_cast<const FreeformFinSet*>(finSetWithId(design, id)), nullptr);
    }
    EXPECT_EQ(design.document->getUndoDescription(), std::optional<std::string>("Convert fin set"));
    ComponentConfigDialog::disposeDialog();

    // One undo brings back the three trapezoidal fin sets.
    design.document->undo();
    for (const Uuid& id : ids)
    {
        EXPECT_NE(dynamic_cast<const TrapezoidFinSet*>(finSetWithId(design, id)), nullptr);
    }
}

TEST_F(FinSetConfigTest, ConvertToFreeformCommitsWhatWasTypedFirst)
{
    // The dialog's closing commits a typed root chord (Java: its focus loss) before the fin set is
    // converted, so the freeform fin set has the root typed.
    const ConfigDialogServices services;
    EveryKindDesign            design;
    QWidget                    window;
    TrapezoidFinSet&           fins = trapezoid(design);
    const Uuid                 id   = fins.getId();
    ComponentConfigDialog::showDialog(window, services.services(), *design.document, fins);
    ComponentConfigDialog* dialog = ComponentConfigDialog::dialog();
    ASSERT_NE(dialog, nullptr);
    const auto* panel = qobject_cast<const TrapezoidFinSetConfig*>(&dialog->configurator());
    ASSERT_NE(panel, nullptr);
    dialog->activateWindow();
    ASSERT_TRUE(QTest::qWaitForWindowActive(dialog));
    typeInto(*panel->rootChordWidgets().spinBox, QStringLiteral("7"));
    panel->convertButton()->click();
    const auto* freeform = dynamic_cast<const FreeformFinSet*>(finSetWithId(design, id));
    ASSERT_NE(freeform, nullptr);
    EXPECT_DOUBLE_EQ(freeform->getLength(), 0.07);
    ComponentConfigDialog::disposeDialog();
    // The typed root chord is the dialog's change, before the step "Convert fin set" (Java: the
    // field's focus loss, then addUndoPosition()): one undo keeps it.
    design.document->undo();
    const auto* restored = dynamic_cast<const TrapezoidFinSet*>(finSetWithId(design, id));
    ASSERT_NE(restored, nullptr);
    EXPECT_DOUBLE_EQ(restored->getRootChord(), 0.07);
}

TEST_F(FinSetConfigTest, ConvertToFreeformGivesTheNewFinSetTheDefaultMaterials)
{
    // Java's `new FreeformFinSet()` takes the default materials of the application's preferences
    // ("Balsa" for fin sets after the start-up); the converted fin set's material is copied, its
    // fillets' is not (FreeformFinSet::convertFinSet() with the preferences). Found by the exit
    // suite FromScratchExitGolden: the fillets were the built-in "Cardboard".
    ConfigDialogServices services;
    loadDefaultComponentMaterials(services.preferences(), services.services().materials());
    EveryKindDesign  design;
    QWidget          window;
    TrapezoidFinSet& fins = trapezoid(design);
    const Uuid       id   = fins.getId();
    ASSERT_EQ(fins.getFilletMaterial().getName(), "Cardboard");  // made with the built-in ones
    const std::string material = fins.getMaterial().getName();
    ComponentConfigDialog::showDialog(window, services.services(), *design.document, fins);
    const ComponentConfigDialog* const dialog = ComponentConfigDialog::dialog();
    ASSERT_NE(dialog, nullptr);
    const auto* panel = qobject_cast<const TrapezoidFinSetConfig*>(&dialog->configurator());
    ASSERT_NE(panel, nullptr);
    panel->convertButton()->click();
    const auto* freeform = dynamic_cast<const FreeformFinSet*>(finSetWithId(design, id));
    ASSERT_NE(freeform, nullptr);
    EXPECT_EQ(freeform->getMaterial().getName(), material);
    EXPECT_EQ(freeform->getFilletMaterial().getName(), "Balsa");
    ComponentConfigDialog::disposeDialog();
}

TEST_F(FinSetConfigTest, SplitFinsKeepsWhatWasTypedInTheDialogsStep)
{
    // Java disposes the dialog (its field committed) before addUndoPosition("Split fin set"): one
    // undo brings the fin set back with the cant typed.
    const ConfigDialogServices services;
    EveryKindDesign            design;
    QWidget                    window;
    TrapezoidFinSet&           fins = trapezoid(design);
    const Uuid                 id   = fins.getId();
    ComponentConfigDialog::showDialog(window, services.services(), *design.document, fins);
    ComponentConfigDialog* dialog = ComponentConfigDialog::dialog();
    ASSERT_NE(dialog, nullptr);
    const auto* panel = qobject_cast<const TrapezoidFinSetConfig*>(&dialog->configurator());
    ASSERT_NE(panel, nullptr);
    dialog->activateWindow();
    ASSERT_TRUE(QTest::qWaitForWindowActive(dialog));
    typeInto(*panel->cantWidgets().spinBox, QStringLiteral("5"));
    panel->splitButton()->click();
    EXPECT_EQ(ComponentConfigDialog::dialog(), nullptr);
    EXPECT_EQ(design.document->getUndoDescription(), std::optional<std::string>("Split fin set"));
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    design.document->undo();
    const FinSet* restored = finSetWithId(design, id);
    ASSERT_NE(restored, nullptr);
    EXPECT_NEAR(restored->getCantAngle(), 5 * std::numbers::pi / 180, 1e-12);
}

TEST_F(FinSetConfigTest, ConvertToFreeformOutsideADialogOpensNone)
{
    const ConfigDialogServices                services;
    const EveryKindDesign                     design;
    TrapezoidFinSet&                          fins = trapezoid(design);
    const Uuid                                id   = fins.getId();
    const FactoryPanel<TrapezoidFinSetConfig> panel(*design.document, services, fins);
    ASSERT_NE(panel.get(), nullptr);
    panel->convertToFreeform();
    EXPECT_EQ(ComponentConfigDialog::dialog(), nullptr);
    EXPECT_FALSE(panel->selection()->isValid());
    EXPECT_NE(dynamic_cast<const FreeformFinSet*>(finSetWithId(design, id)), nullptr);
    EXPECT_EQ(design.document->getUndoDescription(), std::optional<std::string>("Convert fin set"));
}

TEST_F(FinSetConfigTest, SplitAndConvertTakeTheFinSetsJavaTakes)
{
    // Java: splitInstances() splits a listener of the main one's class, convertFinSet() converts
    // any FinSet listener. A trapezoidal panel given an elliptical fin set too shows both rules.
    const ConfigDialogServices services;
    const EveryKindDesign      design;
    TrapezoidFinSet&           fins = trapezoid(design);
    auto&                      elliptic =
        dynamic_cast<EllipticalFinSet&>(design.component(ComponentKind::ELLIPTICAL_FIN_SET));
    const Uuid        finsId     = fins.getId();
    const Uuid        ellipticId = elliptic.getId();
    const Uuid        bodyId     = fins.getParent()->getId();
    const std::size_t children   = fins.getParent()->getChildCount();
    const int         count      = fins.getFinCount();
    {
        QWidget                               host;
        const std::array<RocketComponent*, 1> others{&elliptic};
        const auto selection = std::make_shared<ComponentSelection>(fins, others);
        auto* panel = makeChild<TrapezoidFinSetConfig>(&host, *design.document, services.services(),
                                                       selection);
        panel->splitFins();
    }
    const RocketComponent* body = design.document->getRocket().findComponent(bodyId);
    ASSERT_NE(body, nullptr);
    EXPECT_EQ(body->getChildCount(), children + static_cast<std::size_t>(count) - 1);
    EXPECT_NE(dynamic_cast<const EllipticalFinSet*>(finSetWithId(design, ellipticId)), nullptr);

    // Undone, then converted: both fin sets become freeform ones.
    design.document->undo();
    Rocket& rocket   = design.document->getRocket();
    auto*   restored = dynamic_cast<TrapezoidFinSet*>(rocket.findComponent(finsId));
    auto*   ellipse  = dynamic_cast<EllipticalFinSet*>(rocket.findComponent(ellipticId));
    ASSERT_NE(restored, nullptr);
    ASSERT_NE(ellipse, nullptr);
    const std::array<RocketComponent*, 1> again{ellipse};
    QWidget                               host;
    const auto selection = std::make_shared<ComponentSelection>(*restored, again);
    auto*      panel =
        makeChild<TrapezoidFinSetConfig>(&host, *design.document, services.services(), selection);
    panel->convertToFreeform();
    EXPECT_NE(dynamic_cast<const FreeformFinSet*>(finSetWithId(design, finsId)), nullptr);
    EXPECT_NE(dynamic_cast<const FreeformFinSet*>(finSetWithId(design, ellipticId)), nullptr);
}

TEST_F(FinSetConfigTest, SplitFinsSplitsEverySelectedFinSetInOneUndoStep)
{
    const ConfigDialogServices services;
    EveryKindDesign            design;
    QWidget                    window;
    TrapezoidFinSet&           fins   = trapezoid(design);
    TrapezoidFinSet&           second = addTrapezoid(design, "Second");
    TrapezoidFinSet&           third  = addTrapezoid(design, "Third");
    second.setFinCount(2);
    third.setFinCount(4);
    const Uuid                            bodyId   = fins.getParent()->getId();
    const std::size_t                     children = fins.getParent()->getChildCount();
    const std::array<RocketComponent*, 2> others{&second, &third};
    ComponentConfigDialog::showDialog(window, services.services(), *design.document, fins, others);
    const ComponentConfigDialog* dialog = ComponentConfigDialog::dialog();
    ASSERT_NE(dialog, nullptr);
    const auto* panel = qobject_cast<const TrapezoidFinSetConfig*>(&dialog->configurator());
    ASSERT_NE(panel, nullptr);
    panel->splitButton()->click();
    EXPECT_EQ(ComponentConfigDialog::dialog(), nullptr);
    // 3 + 2 + 4 single fins in place of the three fin sets.
    const RocketComponent* body = design.document->getRocket().findComponent(bodyId);
    ASSERT_NE(body, nullptr);
    EXPECT_EQ(body->getChildCount(), children + 9 - 3);
    EXPECT_EQ(design.document->getUndoDescription(), std::optional<std::string>("Split fin set"));
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    design.document->undo();
    const RocketComponent* restored = design.document->getRocket().findComponent(bodyId);
    ASSERT_NE(restored, nullptr);
    EXPECT_EQ(restored->getChildCount(), children);
}

}  // namespace

}  // namespace QtRocket::Gui
