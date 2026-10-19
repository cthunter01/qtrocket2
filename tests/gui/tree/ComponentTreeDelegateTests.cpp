#include "QtRocket/gui/tree/ComponentTreeDelegate.h"

#include <memory>
#include <vector>

#include <QColor>
#include <QFontMetrics>
#include <QIcon>
#include <QImage>
#include <QItemSelectionModel>
#include <QModelIndex>
#include <QPainter>
#include <QPalette>
#include <QPoint>
#include <QRect>
#include <QSize>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QString>
#include <QStyle>
#include <QStyleOptionViewItem>
#include <QStyledItemDelegate>
#include <QTreeView>
#include <Qt>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/OpenRocketDocumentFactory.h"
#include "QtRocket/gui/tree/ComponentToolTip.h"
#include "QtRocket/gui/tree/ComponentTreeModel.h"
#include "QtRocket/gui/util/ComponentIcons.h"
#include "QtRocket/gui/util/Icons.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/util/ThemeColors.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/InnerTube.h"
#include "QtRocket/rocket/NoseCone.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"

namespace QtRocket::Gui
{

namespace
{

using Id   = Icons::Id;
using Cell = ComponentTreeDelegate::Cell;

/// A design (a stage with a nose cone and a body tube holding an inner tube, and a second stage
/// with a body tube), its model, a view of it and the delegate, the view's.
class ComponentTreeDelegateTest : public Gui::Test::GuiTest
{
protected:
    void SetUp() override
    {
        Gui::Test::GuiTest::SetUp();
        m_document             = OpenRocketDocumentFactory::createNewRocket();
        Rocket&          r     = m_document->getRocket();
        RocketComponent& stage = r.getChild(0);
        m_noseCone             = &stage.addChild(std::make_unique<NoseCone>());
        m_bodyTube             = &stage.addChild(std::make_unique<BodyTube>());
        m_innerTube            = &m_bodyTube->addChild(std::make_unique<InnerTube>());
        m_booster              = &r.addChild(std::make_unique<AxialStage>());
        m_boosterTube          = &m_booster->addChild(std::make_unique<BodyTube>());
        m_model                = std::make_unique<ComponentTreeModel>(*m_document, m_preferences);
        m_view                 = std::make_unique<QTreeView>();
        m_view->setModel(m_model.get());
        m_delegate = makeChild<ComponentTreeDelegate>(m_view.get());
        m_view->setItemDelegate(m_delegate);
    }

    void TearDown() override
    {
        m_view = nullptr;
        m_model.reset();
        m_document.reset();
        Gui::Test::GuiTest::TearDown();
    }

    /// The option of a row of the view in @p rect, selected or not.
    [[nodiscard]] QStyleOptionViewItem option(const QRect& rect, bool selected = false) const
    {
        QStyleOptionViewItem option;
        option.initFrom(m_view.get());
        option.widget = m_view.get();
        option.rect   = rect;
        // As the view sets it up (QAbstractItemView::initViewItemOption()).
        option.textElideMode = m_view->textElideMode();
        option.state |= QStyle::State_Enabled;
        option.state.setFlag(QStyle::State_Selected, selected);
        return option;
    }

    [[nodiscard]] Cell cell(const RocketComponent& component, const QRect& rect,
                            bool selected = false) const
    {
        return m_delegate->cell(option(rect, selected), m_model->indexOf(component));
    }

    /// The style's margin left of a row's contents.
    [[nodiscard]] int margin() const
    {
        const QStyleOptionViewItem o = option(QRect(0, 0, 400, 24));
        return m_view->style()->pixelMetric(QStyle::PM_FocusFrameHMargin, &o, m_view.get()) + 1;
    }

    /// Makes the second stage inactive, as OpenRocket's stage toggling does.
    void deactivateBooster()
    {
        Rocket& r = m_document->getRocket();
        r.getSelectedConfiguration().setStageActive(m_booster->getStageNumber(), false);
        r.fireComponentChangeEvent(ComponentChangeEvent::kAeromassChange |
                                   ComponentChangeEvent::kMotorChange |
                                   ComponentChangeEvent::kTreeChangeChildren);
    }

    [[nodiscard]] QColor paletteColor(QPalette::ColorRole role) const
    {
        return option(QRect(0, 0, 400, 24)).palette.color(QPalette::Inactive, role);
    }

    std::unique_ptr<OpenRocketDocument> m_document;
    InMemoryPreferences                 m_preferences;
    RocketComponent*                    m_noseCone    = nullptr;
    RocketComponent*                    m_bodyTube    = nullptr;
    RocketComponent*                    m_innerTube   = nullptr;
    RocketComponent*                    m_booster     = nullptr;
    RocketComponent*                    m_boosterTube = nullptr;
    std::unique_ptr<ComponentTreeModel> m_model;
    std::unique_ptr<QTreeView>          m_view;
    ComponentTreeDelegate*              m_delegate = nullptr;
};

TEST_F(ComponentTreeDelegateTest, PutsTheIconTheNameAndTheMarkersSideBySide)
{
    m_bodyTube->setMassOverridden(true);
    m_bodyTube->setCGOverridden(true);
    const QRect rect(10, 20, 400, 24);

    const Cell parts = cell(*m_bodyTube, rect);

    // The small icon at its natural size, OpenRocket's PNG of 30 x 11 pixels, centred vertically.
    EXPECT_EQ(parts.icon, QRect(10 + margin(), 20 + ((24 - 11) / 2), 30, 11));
    EXPECT_EQ(ComponentIcons::smallIcon(ComponentKind::BODY_TUBE).actualSize(QSize(256, 256)),
              QSize(30, 11));
    // 4 px, the name, 4 px, the markers 3 px apart, each as high as OpenRocket sizes it.
    EXPECT_EQ(parts.text.left(), parts.icon.right() + 1 + ComponentTreeDelegate::kPartGap);
    EXPECT_EQ(parts.text.width(),
              QFontMetrics(parts.font).horizontalAdvance(QStringLiteral("Body Tube")));
    EXPECT_EQ(parts.text.top(), rect.top());
    EXPECT_EQ(parts.text.height(), rect.height());
    EXPECT_EQ(parts.displayText, QStringLiteral("Body Tube"));
    ASSERT_EQ(parts.markers.size(), 2U);
    EXPECT_EQ(parts.markers[0].first, Id::MASS_OVERRIDE);
    EXPECT_EQ(parts.markers[1].first, Id::CG_OVERRIDE);
    EXPECT_EQ(parts.markers[0].second.left(),
              parts.text.right() + 1 + ComponentTreeDelegate::kPartGap);
    EXPECT_EQ(parts.markers[1].second.left(),
              parts.markers[0].second.right() + 1 + ComponentTreeDelegate::kMarkerSpacing);
    EXPECT_EQ(parts.markers[0].second.size(),
              QSize(Icons::iconHeight(Id::MASS_OVERRIDE, parts.font),
                    Icons::iconHeight(Id::MASS_OVERRIDE, parts.font)));
    EXPECT_EQ(parts.markers[1].second.center().y(), parts.markers[0].second.center().y());
}

TEST_F(ComponentTreeDelegateTest, DrawsTheSubcomponentMarkersSmaller)
{
    m_bodyTube->setMassOverridden(true);
    m_bodyTube->setSubcomponentsOverriddenMass(true);

    const Cell parts = cell(*m_innerTube, QRect(0, 0, 400, 24));

    ASSERT_EQ(parts.markers.size(), 1U);
    EXPECT_EQ(parts.markers[0].first, Id::MASS_OVERRIDE_SUBCOMPONENT);
    // Three quarters of the size (Icons.java's scale multiplier).
    EXPECT_EQ(parts.markers[0].second.width(),
              Icons::iconHeight(Id::MASS_OVERRIDE_SUBCOMPONENT, parts.font));
    EXPECT_LT(parts.markers[0].second.width(), Icons::iconHeight(Id::MASS_OVERRIDE, parts.font));
}

TEST_F(ComponentTreeDelegateTest, LeavesNoRoomForTheIconOfTheRocket)
{
    const Cell parts = cell(m_document->getRocket(), QRect(0, 0, 400, 24));

    EXPECT_EQ(parts.icon.width(), 0);
    EXPECT_EQ(parts.text.left(), margin() + ComponentTreeDelegate::kPartGap);
    EXPECT_EQ(parts.displayText, QStringLiteral("Rocket"));
    EXPECT_TRUE(parts.markers.empty());
}

TEST_F(ComponentTreeDelegateTest, WritesAComponentPlainInTheTextColour)
{
    const Cell parts = cell(*m_bodyTube, QRect(0, 0, 400, 24));

    EXPECT_FALSE(parts.font.italic());
    EXPECT_EQ(parts.textColor, paletteColor(QPalette::Text));
}

TEST_F(ComponentTreeDelegateTest, WritesAComponentOfAnInactiveStageInMutedItalics)
{
    deactivateBooster();
    const QRect rect(0, 0, 400, 24);

    const Cell stage = cell(*m_booster, rect);
    const Cell tube  = cell(*m_boosterTube, rect);

    const QColor muted =
        ThemeColors::color(ThemeColors::Key::VISIBILITY_HIDDEN_FOREGROUND, option(rect).palette);
    EXPECT_TRUE(stage.font.italic());
    EXPECT_EQ(stage.textColor, muted);
    EXPECT_TRUE(tube.font.italic());
    EXPECT_EQ(tube.textColor, muted);
    ASSERT_EQ(stage.markers.size(), 1U);
    EXPECT_EQ(stage.markers[0].first, Id::COMPONENT_DISABLED);
    EXPECT_TRUE(tube.markers.empty());
    EXPECT_FALSE(cell(*m_bodyTube, rect).font.italic());
}

TEST_F(ComponentTreeDelegateTest, WritesASelectedComponentOfAnInactiveStageAsAnySelectedOne)
{
    deactivateBooster();

    const Cell parts = cell(*m_boosterTube, QRect(0, 0, 400, 24), true);

    EXPECT_FALSE(parts.font.italic());
    EXPECT_EQ(parts.textColor, paletteColor(QPalette::HighlightedText));
}

TEST_F(ComponentTreeDelegateTest, WritesAHiddenComponentMutedUnlessSelected)
{
    m_noseCone->setVisible(false);
    const QRect rect(0, 0, 400, 24);

    const Cell hidden   = cell(*m_noseCone, rect);
    const Cell selected = cell(*m_noseCone, rect, true);

    EXPECT_FALSE(hidden.font.italic());
    EXPECT_EQ(hidden.textColor, ThemeColors::color(ThemeColors::Key::VISIBILITY_HIDDEN_FOREGROUND,
                                                   option(rect).palette));
    ASSERT_EQ(hidden.markers.size(), 1U);
    EXPECT_EQ(hidden.markers[0].first, Id::COMPONENT_HIDDEN);
    EXPECT_EQ(selected.textColor, paletteColor(QPalette::HighlightedText));
}

TEST_F(ComponentTreeDelegateTest, ElidesANameThatDoesNotFitBeforeItsMarkers)
{
    m_bodyTube->setName("A body tube with a very long name indeed");
    m_bodyTube->setMassOverridden(true);
    const QRect rect(0, 0, 150, 24);

    const Cell parts = cell(*m_bodyTube, rect);

    EXPECT_NE(parts.displayText, QStringLiteral("A body tube with a very long name indeed"));
    EXPECT_TRUE(parts.displayText.endsWith(QChar(0x2026)));
    ASSERT_EQ(parts.markers.size(), 1U);
    EXPECT_LE(parts.markers[0].second.right(), rect.right() + 1 - margin());
}

TEST_F(ComponentTreeDelegateTest, AsksForRoomForEveryPart)
{
    m_bodyTube->setMassOverridden(true);
    m_bodyTube->setCDOverridden(true);
    const QStyleOptionViewItem o     = option(QRect(0, 0, 0, 0));
    const QModelIndex          index = m_model->indexOf(*m_bodyTube);

    const QSize size  = m_delegate->sizeHint(o, index);
    const Cell  parts = m_delegate->cell(option(QRect(QPoint(0, 0), size)), index);

    EXPECT_EQ(parts.displayText, QStringLiteral("Body Tube"));
    ASSERT_EQ(parts.markers.size(), 2U);
    EXPECT_EQ(size.width(), parts.markers[1].second.right() + 1 + margin());
    EXPECT_GE(size.height(), parts.icon.height());
    EXPECT_GE(size.height(), parts.markers[0].second.height());
    EXPECT_GE(size.height(), QFontMetrics(parts.font).height());
}

TEST_F(ComponentTreeDelegateTest, ShowsTheSelectionsToolTipOverOneOfItsRows)
{
    QItemSelectionModel& selection = *m_view->selectionModel();
    selection.select(m_model->indexOf(*m_noseCone), QItemSelectionModel::Select);
    selection.select(m_model->indexOf(*m_bodyTube), QItemSelectionModel::Select);

    EXPECT_EQ(ComponentTreeDelegate::toolTip(*m_view, m_model->indexOf(*m_bodyTube)),
              ComponentToolTip::multipleComponents(
                  std::vector<const RocketComponent*>{m_noseCone, m_bodyTube}));
    EXPECT_EQ(ComponentTreeDelegate::toolTip(*m_view, m_model->indexOf(*m_innerTube)),
              ComponentToolTip::singleComponent(*m_innerTube, m_preferences.getDefaultMach()));
}

TEST_F(ComponentTreeDelegateTest, PaintsEveryRow)
{
    m_bodyTube->setMassOverridden(true);
    m_noseCone->setVisible(false);
    deactivateBooster();
    QImage   image(400, 24, QImage::Format_ARGB32_Premultiplied);
    QPainter painter(&image);

    const std::vector<const RocketComponent*> rows{
        &m_document->getRocket(), m_noseCone, m_bodyTube, m_innerTube, m_booster, m_boosterTube};
    for (const RocketComponent* component : rows)
    {
        image.fill(Qt::white);
        m_delegate->paint(&painter, option(image.rect(), component == m_bodyTube),
                          m_model->indexOf(*component));
    }
    painter.end();
    // Nothing to compare (no pixel assertions): painting must neither warn nor fail.
    EXPECT_FALSE(image.isNull());
}

TEST_F(ComponentTreeDelegateTest, DrawsTheRowsOfAnotherModelAsItsBaseClassDoes)
{
    QStandardItemModel other;
    other.appendRow(makeQtOwned<QStandardItem>(QStringLiteral("Other")));
    const QStyledItemDelegate  plain;
    const QStyleOptionViewItem o = option(QRect(0, 0, 400, 24));

    EXPECT_EQ(m_delegate->sizeHint(o, other.index(0, 0)), plain.sizeHint(o, other.index(0, 0)));
    EXPECT_EQ(ComponentTreeDelegate::toolTip(*m_view, other.index(0, 0)), QString());
}

}  // namespace

}  // namespace QtRocket::Gui
