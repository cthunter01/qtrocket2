// The design view (RocketPanel) on its own, over a document with a component tree and its
// selection made here: the ribbon, the footer and the layout of OpenRocket's createPanel(), what
// the ribbon's controls do to the figure, the rotation and the pan by the mouse, and the clicks'
// requests. The click transcript of OpenRocket and the snapping are in
// rocket_panel_golden_tests.cpp, the panel in a design window in
// tests/gui/main/rocket_panel_window_tests.cpp.

#include "QtRocket/gui/figure/RocketPanel.h"

#include <cmath>
#include <cstddef>
#include <numbers>
#include <optional>
#include <string>
#include <vector>

#include <QAbstractScrollArea>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QEvent>
#include <QFont>
#include <QFrame>
#include <QLabel>
#include <QList>
#include <QObject>
#include <QPoint>
#include <QPointF>
#include <QRect>
#include <QScrollBar>
#include <QSize>
#include <QString>
#include <QStringList>
#include <QToolButton>
#include <QWheelEvent>
#include <QWidget>
#include <Qt>
#include <QtContainerFwd>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/figure/FigureView.h"
#include "QtRocket/gui/binding/DoubleBinding.h"
#include "QtRocket/gui/figure/DesignPhysics.h"
#include "QtRocket/gui/figure/RocketFigure.h"
#include "QtRocket/gui/figure/RulerUnitSelector.h"
#include "QtRocket/gui/figure/ScaleScrollArea.h"
#include "QtRocket/gui/figure/ScaleSelector.h"
#include "QtRocket/gui/figure/ViewRotationControl.h"
#include "QtRocket/gui/tree/ComponentTreeModel.h"
#include "QtRocket/gui/tree/ComponentTreeView.h"
#include "QtRocket/gui/tree/DocumentSelectionModel.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/widgets/ConfigurationComboBox.h"
#include "QtRocket/gui/widgets/SingleRowScrollArea.h"
#include "QtRocket/gui/widgets/StageSelector.h"
#include "QtRocket/preferences/DocumentPreferences.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/unit/Unit.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/Uuid.h"
#include "figure/FigureMouse.h"
#include "tree/ProbeDesign.h"
#include "widgets/RibbonTestSupport.h"

namespace QtRocket::Gui
{

namespace
{

using RocketPanelTest = Gui::Test::GuiTest;
using Gui::Test::ProbeDesign;
using Booster = ProbeDesign::Booster;

/// Delivers the posted events until the layouts and the figure are done (a scroll bar shows one
/// pass after its range changed, the figure answers the rocket's events once per pass).
void settle()
{
    constexpr int kPasses = 4;
    for (int pass = 0; pass < kPasses; ++pass)
    {
        QApplication::processEvents();
    }
}

/// The texts of the labels among @p widget's children, in the order they were made.
[[nodiscard]] QStringList labelTexts(const QWidget& widget)
{
    QStringList texts;
    for (const QLabel* label : widget.findChildren<QLabel*>(Qt::FindDirectChildrenOnly))
    {
        texts.push_back(label->text());
    }
    return texts;
}

/// The left edge of @p widget in @p ancestor's coordinates.
[[nodiscard]] int leftIn(const QWidget& widget, const QWidget& ancestor)
{
    return widget.mapTo(&ancestor, QPoint(0, 0)).x();
}

/// The probes' design (ProbeDesign) in a document, its component tree and the tree's selection,
/// and the design view of the document using that selection, 1000 x 600 pixels and shown; the
/// configuration dialog's and the popup's requests recorded.
struct Panel
{
    explicit Panel(Booster booster = Booster::WITH_SIDE_BOOSTERS)
      : design(booster),
        model(design.document(), preferences),
        view(model),
        selection(makeChild<DocumentSelectionModel>(&view))
    {
        // The first stage selected, as a design window starts.
        selection->setSelectedComponent(design.find("Sustainer"));
        panel.setSelectionModel(selection);
        QObject::connect(
            &panel, &RocketPanel::editRequested, &panel,
            [this](RocketComponent* component, const std::vector<RocketComponent*>& others) {
                std::string text = "dialog " + component->getName() + " [";
                for (const RocketComponent* other : others)
                {
                    text += (text.back() == '[' ? "" : ", ") + other->getName();
                }
                requests.push_back(text + "]");
            });
        QObject::connect(&panel, &RocketPanel::componentPopupRequested, &panel,
                         [this](const QPoint& position) { popups.push_back(position); });
        panel.resize(1000, 600);
        panel.show();
        settle();
    }

    [[nodiscard]] RocketFigure& figure() const { return panel.figure(); }
    [[nodiscard]] Rocket&       rocket() const { return design.rocket(); }
    /// The names of the selected components, "[A, B]".
    [[nodiscard]] std::string selected() const
    {
        return design.names(selection->selectedComponents());
    }
    /// A pixel of the figure where @p name is the topmost component (a test failure without one).
    [[nodiscard]] QPoint pixelOf(std::string_view name) const
    {
        const std::optional<QPoint> pixel = Gui::Test::figurePixelOf(figure(), design.find(name));
        if (!pixel.has_value())
        {
            ADD_FAILURE() << "no pixel of " << name;
            return {};
        }
        return *pixel;
    }
    /// A pixel of the figure where there is no component: its top left corner.
    [[nodiscard]] static QPoint emptyPixel() { return {2, 2}; }
    /// @p pixel of the figure in the viewport's coordinates.
    [[nodiscard]] QPoint inViewport(QPoint pixel) const { return figure().mapToParent(pixel); }

    InMemoryPreferences      preferences;
    ProbeDesign              design;
    ComponentTreeModel       model;
    ComponentTreeView        view;
    DocumentSelectionModel*  selection = nullptr;
    std::vector<std::string> requests;
    std::vector<QPoint>      popups;
    RocketPanel              panel{design.document(), preferences, nullptr};
};

// =============================================================================== the layout

TEST_F(RocketPanelTest, MakesTheRibbonOfOpenRocket)
{
    const Panel        shown;
    const RocketPanel& panel  = shown.panel;
    const QWidget&     ribbon = *panel.ribbonScrollArea().widget();
    EXPECT_EQ(ribbon.objectName(), QStringLiteral("ribbon"));
    EXPECT_EQ(labelTexts(ribbon),
              (QStringList{QStringLiteral("View Type:"), QStringLiteral("Zoom:"),
                           QStringLiteral("Stages:"), QStringLiteral("Flight configuration:")}));

    // The views, the side view at first.
    const QComboBox& views = panel.viewSelector();
    ASSERT_EQ(views.count(), 3);
    EXPECT_EQ(views.itemText(0), QStringLiteral("Top view"));
    EXPECT_EQ(views.itemText(1), QStringLiteral("Side view"));
    EXPECT_EQ(views.itemText(2), QStringLiteral("Back view"));
    EXPECT_EQ(views.currentIndex(), 1);
    EXPECT_EQ(panel.viewType(), FigureView::SIDE);

    // The zoom's controls in the ribbon, the zoom relative to the fit.
    const ScaleSelector& zoom = panel.scaleSelector();
    EXPECT_EQ(zoom.zoomOutButton()->parentWidget(), &ribbon);
    EXPECT_EQ(zoom.scaleCombo()->parentWidget(), &ribbon);
    EXPECT_EQ(zoom.zoomInButton()->parentWidget(), &ribbon);
    EXPECT_EQ(zoom.zoomFitButton()->parentWidget(), &ribbon);
    EXPECT_EQ(zoom.scaleCombo()->currentText(), QStringLiteral("100%(fit)"));

    // Show CG/CP, checked.
    const QCheckBox& carets = panel.showCgCpCheckBox();
    EXPECT_EQ(carets.text(), QStringLiteral("Show CG/CP"));
    EXPECT_TRUE(carets.isChecked());
    EXPECT_EQ(
        carets.toolTip(),
        QStringLiteral("Disabling this checkbox hides the CG and CP markings in the rocket view."));

    // The stages of the design and its configurations.
    EXPECT_EQ(panel.stageSelector().buttonCount(), 3U);
    EXPECT_EQ(panel.configurationSelector().count(), 1);
    EXPECT_EQ(panel.configurationSelector().minimumWidth(),
              RocketPanel::kConfigurationMinimumWidth);

    // In OpenRocket's order, from left to right, the separator between Show CG/CP and the stages.
    const auto* separator = ribbon.findChild<const QFrame*>(QStringLiteral("ribbonSeparator"));
    ASSERT_NE(separator, nullptr);
    EXPECT_EQ(separator->frameShape(), QFrame::VLine);
    const std::vector<const QWidget*> order{&views,
                                            zoom.zoomOutButton(),
                                            zoom.scaleCombo(),
                                            zoom.zoomInButton(),
                                            zoom.zoomFitButton(),
                                            &carets,
                                            separator,
                                            &panel.stageSelector(),
                                            &panel.configurationSelector()};
    for (std::size_t i = 1; i < order.size(); ++i)
    {
        EXPECT_LT(order[i - 1]->geometry().left(), order[i]->geometry().left()) << i;
    }
}

TEST_F(RocketPanelTest, MakesTheFooterAndTheLayoutOfOpenRocket)
{
    const Panel        shown;
    const RocketPanel& panel = shown.panel;
    EXPECT_EQ(panel.sizeHint(), QSize(800, 300));
    EXPECT_EQ(panel.objectName(), QStringLiteral("rocketPanel"));

    // The footer: the hint, 3 points smaller, and Show warnings.
    const QLabel& hint = panel.infoMessage();
    EXPECT_EQ(hint.text(), QStringLiteral("<html>Click to select &nbsp;&nbsp; Shift+click to "
                                          "select other &nbsp;&nbsp; Double-click to edit "
                                          "&nbsp;&nbsp; Click+drag to move"));
    EXPECT_EQ(hint.font().pointSizeF(), QApplication::font().pointSizeF() - 3);
    const QCheckBox& warnings = panel.showWarningsCheckBox();
    EXPECT_EQ(warnings.text(), QStringLiteral("Show warnings"));
    EXPECT_EQ(warnings.toolTip(),
              QStringLiteral("Show/hide geometry warnings in the rocket design view."));
    EXPECT_EQ(panel.footerScrollArea().widget(), hint.parentWidget());

    // The figure in its scroll area, fitted, at least 300 x 100, the rulers' unit selector in
    // the corner.
    const ScaleScrollArea& area = panel.scrollArea();
    EXPECT_EQ(area.figure(), &panel.figure());
    EXPECT_TRUE(area.isFitting());
    EXPECT_EQ(area.minimumSize(), QSize(300, 100));
    EXPECT_EQ(area.rulerCornerWidget(), &panel.rulerUnitSelector());

    // The ribbon across the top, the rotation control left of the figure, the footer below the
    // figure, 25 pixels in from its left edge.
    const QRect ribbon   = panel.ribbonScrollArea().geometry();
    const QRect figure   = area.geometry();
    const QRect rotation = panel.rotationControl().geometry();
    EXPECT_LE(ribbon.bottom(), figure.top());
    EXPECT_LE(ribbon.left(), rotation.left());
    EXPECT_LE(rotation.right(), figure.left());
    EXPECT_GE(rotation.height(), figure.height() - 1);
    const SingleRowScrollArea& footer = panel.footerScrollArea();
    EXPECT_GE(footer.mapTo(&panel, QPoint(0, 0)).y(), figure.bottom());
    EXPECT_EQ(leftIn(footer, panel), figure.left() + RocketPanel::kFooterIndent);
}

// ========================================================================== the ribbon

TEST_F(RocketPanelTest, ShowsTheViewChosen)
{
    Panel        shown;
    RocketPanel& panel = shown.panel;
    Gui::Test::pickComboItem(panel.viewSelector(), 0);
    EXPECT_EQ(shown.figure().view(), FigureView::TOP);
    EXPECT_EQ(panel.viewType(), FigureView::TOP);
    EXPECT_TRUE(std::isfinite(panel.physics().cgCaret().x()));

    // The back view has no carets.
    Gui::Test::pickComboItem(panel.viewSelector(), 2);
    EXPECT_EQ(shown.figure().view(), FigureView::BACK);
    EXPECT_TRUE(std::isnan(panel.physics().cgCaret().x()));
    EXPECT_TRUE(std::isnan(panel.physics().cpCaret().x()));

    // setViewType() goes through the combo box.
    panel.setViewType(FigureView::SIDE);
    EXPECT_EQ(panel.viewSelector().currentIndex(), 1);
    EXPECT_EQ(shown.figure().view(), FigureView::SIDE);
    EXPECT_TRUE(std::isfinite(panel.physics().cpCaret().x()));
}

TEST_F(RocketPanelTest, AnotherViewPutsTheRulersBackToTheDefaultUnit)
{
    Panel            shown;
    RocketPanel&     panel  = shown.panel;
    const UnitGroup& length = unitGroup(UnitGroupId::LENGTH);
    const Unit*      found  = length.getUnit("m");
    ASSERT_NE(found, nullptr);
    const Unit& metres = *found;
    panel.scrollArea().setRulerUnit(metres);
    EXPECT_EQ(&panel.rulerUnitSelector().binding()->getCurrentUnit(), &metres);

    // applyViewTypeSelection() calls updateRulers().
    Gui::Test::pickComboItem(panel.viewSelector(), 2);
    EXPECT_EQ(&panel.scrollArea().rulerUnit(), &length.getDefaultUnit());
    EXPECT_EQ(&panel.rulerUnitSelector().binding()->getCurrentUnit(), &length.getDefaultUnit());

    // The rulers' unit selector sets the rulers' unit.
    panel.rulerUnitSelector().binding()->setCurrentUnit(metres);
    EXPECT_EQ(&panel.scrollArea().rulerUnit(), &metres);
    panel.updateRulers();
    EXPECT_EQ(&panel.scrollArea().rulerUnit(), &length.getDefaultUnit());
}

TEST_F(RocketPanelTest, ShowCgCpShowsAndHidesTheCarets)
{
    const Panel        shown;
    const RocketPanel& panel = shown.panel;
    EXPECT_TRUE(shown.figure().isDrawCarets());
    panel.showCgCpCheckBox().click();
    EXPECT_FALSE(shown.figure().isDrawCarets());
    panel.showCgCpCheckBox().click();
    EXPECT_TRUE(shown.figure().isDrawCarets());
}

TEST_F(RocketPanelTest, TheZoomControlsZoomRelativeToTheFit)
{
    const Panel        shown;
    const RocketPanel& panel = shown.panel;
    ScaleScrollArea&   area  = panel.scrollArea();
    const double       fit   = area.fitScale();
    panel.scaleSelector().zoomInButton()->click();
    EXPECT_FALSE(area.isFitting());
    EXPECT_NEAR(area.relativeScale(), 1.5, 1e-9);
    EXPECT_NEAR(area.userScale(), 1.5 * fit, 1e-9);
    EXPECT_EQ(panel.scaleSelector().scaleCombo()->currentText(), QStringLiteral("150%"));

    panel.scaleSelector().zoomFitButton()->click();
    EXPECT_TRUE(area.isFitting());
    EXPECT_EQ(panel.scaleSelector().scaleCombo()->currentText(), QStringLiteral("100%(fit)"));
}

TEST_F(RocketPanelTest, ShowWarningsIsTheDocumentsPreference)
{
    Panel              shown;
    const RocketPanel& panel = shown.panel;
    // Unset: true.
    EXPECT_TRUE(panel.showWarningsCheckBox().isChecked());
    EXPECT_TRUE(panel.physics().isShowWarnings());

    panel.showWarningsCheckBox().click();
    EXPECT_FALSE(panel.physics().isShowWarnings());
    EXPECT_FALSE(shown.design.document().getDocumentPreferences().getBoolean(
        DocumentPreferences::kPrefShowWarnings, true));

    // Another view of the document starts with it.
    const RocketPanel other(shown.design.document(), shown.preferences, nullptr);
    EXPECT_FALSE(other.showWarningsCheckBox().isChecked());
    EXPECT_FALSE(other.physics().isShowWarnings());
}

TEST_F(RocketPanelTest, TheStageButtonsToggleTheStagesOfTheFigure)
{
    const Panel                shown;
    const StageSelector&       stages        = shown.panel.stageSelector();
    const FlightConfiguration& configuration = shown.rocket().getSelectedConfiguration();
    const std::size_t          shapes        = shown.figure().figureGeometry().shapes().size();
    ASSERT_TRUE(configuration.isStageActive(1));

    stages.button(1)->click();
    settle();
    EXPECT_FALSE(configuration.isStageActive(1));
    EXPECT_LT(shown.figure().figureGeometry().shapes().size(), shapes);

    stages.button(1)->click();
    settle();
    EXPECT_TRUE(configuration.isStageActive(1));
    EXPECT_EQ(shown.figure().figureGeometry().shapes().size(), shapes);
}

TEST_F(RocketPanelTest, TheConfigurationComboSelectsTheRocketsConfiguration)
{
    const Panel                 shown;
    const FlightConfigurationId second = shown.rocket().createFlightConfiguration().getId();
    settle();
    ConfigurationComboBox& configurations = shown.panel.configurationSelector();
    ASSERT_EQ(configurations.count(), 2);
    Gui::Test::pickComboItem(configurations, 1);
    EXPECT_EQ(shown.rocket().getSelectedConfiguration().getId(), second);
    EXPECT_EQ(configurations.currentIndex(), 1);
    Gui::Test::pickComboItem(configurations, 0);
    EXPECT_EQ(shown.rocket().getSelectedConfiguration().getId(),
              FlightConfigurationId::defaultValueId());
}

TEST_F(RocketPanelTest, TheWheelOverTheRibbonsComboBoxesChoosesNothing)
{
    const Panel        shown;
    const RocketPanel& panel = shown.panel;
    for (QComboBox* combo : {&panel.viewSelector(), panel.scaleSelector().scaleCombo()})
    {
        const int     index = combo->currentIndex();
        const QPointF centre(combo->width() / 2.0, combo->height() / 2.0);
        QWheelEvent   wheel(centre, combo->mapToGlobal(centre), QPoint(), QPoint(0, -120),
                            Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
        QApplication::sendEvent(combo, &wheel);
        EXPECT_EQ(combo->currentIndex(), index) << combo->objectName().toStdString();
    }
}

// =========================================================================== the mouse

TEST_F(RocketPanelTest, ALeftDragRotatesTheFigure)
{
    const Panel        shown;
    const RocketPanel& panel  = shown.panel;
    const int          height = panel.scrollArea().height();
    const QPoint       from   = shown.inViewport(Panel::emptyPixel()) + QPoint(100, 50);
    Gui::Test::dragFrom(shown.figure(), from, from + QPoint(3, 40));
    EXPECT_EQ(shown.figure().rotation(), RocketPanel::dragRotation(0.0, 40, height, false));
    EXPECT_NEAR(shown.figure().rotation(),
                (2 * std::numbers::pi) - (40.0 / height * std::numbers::pi), 1e-12);
    // A drag is no click: nothing was selected or cleared.
    EXPECT_EQ(shown.selected(), "[Sustainer]");

    // With Shift the rotation snaps; from where the figure is.
    const double start = shown.figure().rotation();
    Gui::Test::dragFrom(shown.figure(), from, from + QPoint(0, -25), Qt::LeftButton,
                        Qt::ShiftModifier);
    EXPECT_EQ(shown.figure().rotation(), RocketPanel::dragRotation(start, -25, height, true));
    EXPECT_EQ(shown.figure().rotation(),
              ViewRotationControl::snapRotation(shown.figure().rotation()));

    // The rotation control shows it.
    EXPECT_EQ(panel.rotationControl().binding()->value(), shown.figure().rotation());
}

TEST_F(RocketPanelTest, TheRotationLockMakesALeftDragPan)
{
    const Panel            shown;
    const RocketPanel&     panel = shown.panel;
    const ScaleScrollArea& area  = panel.scrollArea();
    // Zoomed in so that the view can move.
    panel.scaleSelector().zoomInButton()->click();
    panel.scaleSelector().zoomInButton()->click();
    settle();
    const QScrollBar& horizontal = *area.horizontalScrollBar();
    ASSERT_GT(horizontal.maximum(), 100);
    area.horizontalScrollBar()->setValue(horizontal.maximum() / 2);
    area.verticalScrollBar()->setValue(area.verticalScrollBar()->maximum() / 2);

    panel.rotationControl().setDragRotationLocked(true);
    EXPECT_TRUE(area.panButtons().testFlag(Qt::LeftButton));
    const int    x    = horizontal.value();
    const QPoint from = QPoint(200, 100);
    Gui::Test::dragFrom(shown.figure(), from, from + QPoint(-40, 0));
    EXPECT_EQ(horizontal.value(), x + 40);
    EXPECT_EQ(shown.figure().rotation(), 0.0);

    panel.rotationControl().setDragRotationLocked(false);
    EXPECT_FALSE(area.panButtons().testFlag(Qt::LeftButton));
}

TEST_F(RocketPanelTest, AMiddleOrARightDragPans)
{
    const Panel            shown;
    const RocketPanel&     panel = shown.panel;
    const ScaleScrollArea& area  = panel.scrollArea();
    panel.scaleSelector().zoomInButton()->click();
    panel.scaleSelector().zoomInButton()->click();
    settle();
    const QScrollBar& horizontal = *area.horizontalScrollBar();
    area.horizontalScrollBar()->setValue(horizontal.maximum() / 2);
    for (const Qt::MouseButton button : {Qt::MiddleButton, Qt::RightButton})
    {
        const int    x    = horizontal.value();
        const QPoint from = QPoint(200, 100);
        Gui::Test::dragFrom(shown.figure(), from, from + QPoint(30, 0), button);
        EXPECT_EQ(horizontal.value(), x - 30) << button;
        EXPECT_EQ(shown.figure().rotation(), 0.0);
    }
    // No popup: a drag is no click.
    EXPECT_TRUE(shown.popups.empty());
    EXPECT_EQ(shown.selected(), "[Sustainer]");
}

TEST_F(RocketPanelTest, AClickSelectsTheTopmostComponent)
{
    const Panel shown;
    Gui::Test::clickAt(shown.figure(), shown.pixelOf("BT2"));
    EXPECT_EQ(shown.selected(), "[BT2]");
    Gui::Test::clickAt(shown.figure(), shown.pixelOf("Chute"));
    EXPECT_EQ(shown.selected(), "[Chute]");
    Gui::Test::clickAt(shown.figure(), shown.pixelOf("NC"), Qt::LeftButton, Qt::ShiftModifier);
    EXPECT_EQ(shown.selected(), "[Chute, NC]");
    Gui::Test::clickAt(shown.figure(), Panel::emptyPixel());
    EXPECT_EQ(shown.selected(), "[]");
    EXPECT_TRUE(shown.requests.empty());
    EXPECT_TRUE(shown.popups.empty());
}

TEST_F(RocketPanelTest, ADoubleClickAsksForTheDialog)
{
    Panel        shown;
    const QPoint pixel = shown.pixelOf("BT1");
    Gui::Test::clickAt(shown.figure(), pixel);
    Gui::Test::secondClickAt(shown.figure(), pixel);
    EXPECT_EQ(shown.requests, (std::vector<std::string>{"dialog BT1 []"}));
    EXPECT_EQ(shown.selected(), "[BT1]");

    // With Ctrl: a multi-edit of the selection, the last selected one the main component.
    shown.requests.clear();
    const QPoint nose = shown.pixelOf("NC");
    Gui::Test::clickAt(shown.figure(), nose, Qt::LeftButton, Qt::ControlModifier);
    EXPECT_EQ(shown.selected(), "[BT1, NC]");
    Gui::Test::secondClickAt(shown.figure(), nose, Qt::LeftButton, Qt::ControlModifier);
    EXPECT_EQ(shown.selected(), "[BT1, NC]");
    EXPECT_EQ(shown.requests, (std::vector<std::string>{"dialog NC [BT1]"}));

    // A clicked component that is not selected joins the selection as the main component.
    shown.requests.clear();
    Gui::Test::secondClickAt(shown.figure(), shown.pixelOf("BT2"), Qt::LeftButton,
                             Qt::ControlModifier);
    EXPECT_EQ(shown.selected(), "[BT1, NC, BT2]");
    EXPECT_EQ(shown.requests, (std::vector<std::string>{"dialog BT2 [BT1, NC]"}));
}

TEST_F(RocketPanelTest, ARightClickAsksForThePopupWhereTheMouseIs)
{
    Panel        shown;
    const QPoint pixel = shown.pixelOf("IT");
    Gui::Test::clickAt(shown.figure(), pixel, Qt::RightButton);
    EXPECT_EQ(shown.selected(), "[IT]");
    ASSERT_EQ(shown.popups.size(), 1U);
    EXPECT_EQ(shown.popups.front(), shown.figure().mapToGlobal(pixel));

    // On nothing: the selection is cleared, and no popup.
    Gui::Test::clickAt(shown.figure(), Panel::emptyPixel(), Qt::RightButton);
    EXPECT_EQ(shown.selected(), "[]");
    EXPECT_EQ(shown.popups.size(), 1U);
}

TEST_F(RocketPanelTest, WithoutASelectionAClickSelectsNothing)
{
    Panel shown;
    shown.panel.setSelectionModel(nullptr);
    EXPECT_EQ(shown.panel.selectionModel(), nullptr);
    const QPoint pixel = shown.pixelOf("BT1");
    Gui::Test::clickAt(shown.figure(), pixel);
    Gui::Test::secondClickAt(shown.figure(), pixel);
    Gui::Test::clickAt(shown.figure(), pixel, Qt::RightButton);
    Gui::Test::clickAt(shown.figure(), Panel::emptyPixel());
    EXPECT_EQ(shown.selected(), "[Sustainer]");
    EXPECT_TRUE(shown.requests.empty());
    EXPECT_TRUE(shown.popups.empty());
}

TEST_F(RocketPanelTest, TheFigureDrawsTheHighlightedComponents)
{
    Panel                               shown;
    const std::vector<RocketComponent*> highlighted = shown.design.find({"BT1", "Fins"});
    shown.panel.setHighlightedComponents(highlighted);
    EXPECT_EQ(shown.figure().selection(),
              (std::vector<Uuid>{highlighted[0]->getId(), highlighted[1]->getId()}));
    // The selection model is not the figure's: a click changes the selection only.
    Gui::Test::clickAt(shown.figure(), shown.pixelOf("NC"));
    EXPECT_EQ(shown.selected(), "[NC]");
    EXPECT_EQ(shown.figure().selection().size(), 2U);
}

TEST_F(RocketPanelTest, OutlivesItsSelectionModel)
{
    Panel        shown;
    const QPoint pixel = shown.pixelOf("BT1");
    shown.selection->deleteLater();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    EXPECT_EQ(shown.panel.selectionModel(), nullptr);
    Gui::Test::clickAt(shown.figure(), pixel);
    EXPECT_TRUE(shown.requests.empty());
}

}  // namespace

}  // namespace QtRocket::Gui
