// The wind editor's table view (OpenRocket's MultiLevelWindTable as the dialog shows it): the
// header's unit selectors, the editors opened on demand (the user's decision U-M4-6), the
// selection that stays when the focus leaves (OpenRocket bug 33), and the deletions by the Delete
// column, the popup menu and the Delete key.

#include "QtRocket/gui/dialogs/simulation/MultiLevelWindTableView.h"

#include <array>
#include <cstddef>
#include <memory>
#include <numbers>
#include <string_view>

#include <QAbstractItemModelTester>
#include <QAction>
#include <QApplication>
#include <QContextMenuEvent>
#include <QHeaderView>
#include <QLineEdit>
#include <QMenu>
#include <QModelIndex>
#include <QObject>
#include <QPoint>
#include <QRect>
#include <QSignalSpy>
#include <QString>
#include <QTest>
#include <QWidget>
#include <Qt>
#include <QtCore/qtestsupport_core.h>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/gui/dialogs/simulation/MultiLevelWindTableModel.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/widgets/UnitSelector.h"
#include "QtRocket/gui/widgets/UnitSpinBox.h"
#include "QtRocket/models/MultiLevelPinkNoiseWindModel.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/unit/Unit.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/BugError.h"
#include "dialogs/simulation/WindEditorTranscript.h"
#include "widgets/WidgetTestSupport.h"

namespace QtRocket::Gui
{

namespace
{

using MultiLevelWindTableViewTest = Gui::Test::GuiTest;
using Model                       = MultiLevelWindTableModel;
using View                        = Gui::MultiLevelWindTableView;

/// The levels 0, 500 and 1250.5 m of the probe tools/openrocket-probes/wind-editor.
void threeLevels(MultiLevelPinkNoiseWindModel& model)
{
    model.clearLevels();
    EXPECT_TRUE(model.addWindLevel(0, 3, std::numbers::pi / 2, 0.3).has_value());
    EXPECT_TRUE(model.addWindLevel(500, 7.5, std::numbers::pi, 1.0).has_value());
    EXPECT_TRUE(model.addWindLevel(1250.5, 12.25, 0.0, 3.0).has_value());
}

/// A view of the table of three levels, a window of its own, under QAbstractItemModelTester.
class WindView
{
public:
    WindView() { threeLevels(m_wind); }

    [[nodiscard]] MultiLevelPinkNoiseWindModel& wind() noexcept { return m_wind; }
    [[nodiscard]] Model&                        model() noexcept { return m_model; }
    [[nodiscard]] View&                         view() noexcept { return m_view; }

    /// The middle of the cell @p row, @p column, in the viewport.
    [[nodiscard]] QPoint cell(int row, int column) const
    {
        return m_view.visualRect(m_model.index(row, column)).center();
    }

    /// A click of the left button on the cell.
    void click(int row, int column)
    {
        QTest::mouseClick(m_view.viewport(), Qt::LeftButton, {}, cell(row, column));
    }

    /// The open editor as the spin box it is; null with a test failure when there is none.
    [[nodiscard]] UnitSpinBox* editor() const
    {
        auto* const spinBox = qobject_cast<UnitSpinBox*>(m_view.openEditor());
        if (spinBox == nullptr)
        {
            ADD_FAILURE() << "no cell editor is open";
        }
        return spinBox;
    }

    /// Waits until no editor is open (the delegate closes it in the next turn of the event
    /// loop after Enter): whether none is.
    [[nodiscard]] bool editorClosed() const
    {
        return QTest::qWaitFor([this] { return m_view.openEditor() == nullptr; });
    }

    /// The altitude of the level of row @p row, m.
    [[nodiscard]] double altitude(int row) const
    {
        const Model::Level* const level = m_model.levelAt(row);
        return level == nullptr ? -1.0 : level->getAltitude();
    }

private:
    InMemoryPreferences          m_preferences;
    MultiLevelPinkNoiseWindModel m_wind{m_preferences};
    Model                        m_model{m_wind, m_preferences, nullptr};
    QAbstractItemModelTester     m_tester{&m_model,
                                          QAbstractItemModelTester::FailureReportingMode::Warning};
    View                         m_view{m_model, nullptr};
};

/// The unit of @p group named @p name.
[[nodiscard]] const Unit& unitNamed(UnitGroupId group, std::string_view name)
{
    const Unit* const unit = unitGroup(group).getUnit(name);
    if (unit == nullptr)
    {
        ADD_FAILURE() << "no unit " << name;
        return unitGroup(group).getDefaultUnit();
    }
    return *unit;
}

/// Java's column widths.
void expectJavasColumnWidths(const View& view)
{
    for (int column = 0; column < Model::kColumnCount; ++column)
    {
        EXPECT_EQ(view.columnWidth(column),
                  View::kColumnWidths.at(static_cast<std::size_t>(column)))
            << column;
    }
}

/// The unit selector of @p column lies under the column's name, inside its section.
void expectUnderColumnName(const View& view, int column)
{
    const QHeaderView& header   = *view.horizontalHeader();
    const QRect        geometry = view.unitSelector(column).geometry();
    const int          left     = header.sectionViewportPosition(column);
    EXPECT_GT(geometry.top(), header.fontMetrics().height()) << column;
    EXPECT_LE(geometry.bottom(), header.height()) << column;
    EXPECT_GE(geometry.left(), left) << column;
    EXPECT_LE(geometry.right(), left + header.sectionSize(column)) << column;
}

/// The unit selector of @p column: named @p name, showing @p unit, a child of the header.
void expectUnitSelector(const View& view, int column, const QString& name, const QString& unit)
{
    const UnitSelector& selector = view.unitSelector(column);
    EXPECT_EQ(selector.objectName(), name);
    EXPECT_EQ(QtStrings::fromUtf8(selector.getSelectedUnit().getUnit()), unit);
    EXPECT_EQ(selector.parentWidget(), view.horizontalHeader());
    expectUnderColumnName(view, column);
}

/// "Add new level" @p count times.
void addLevels(View& view, int count)
{
    for (int added = 0; added < count; ++added)
    {
        view.addLevel();
    }
}

}  // namespace

TEST_F(MultiLevelWindTableViewTest, TheColumnsHaveJavasWidths)
{
    WindView table;
    expectJavasColumnWidths(table.view());
    EXPECT_TRUE(table.view().verticalHeader()->isHidden());
    EXPECT_EQ(table.view().objectName(), QStringLiteral("windTable"));
}

TEST_F(MultiLevelWindTableViewTest, EachNumberColumnHasAUnitSelectorUnderItsName)
{
    WindView table;
    ASSERT_TRUE(Gui::Test::showActive(table.view()));
    expectUnitSelector(table.view(), Model::kAltitudeColumn, QStringLiteral("altitudeUnitSelector"),
                       QStringLiteral("m"));
    expectUnitSelector(table.view(), Model::kSpeedColumn, QStringLiteral("speedUnitSelector"),
                       QStringLiteral("m/s"));
    expectUnitSelector(table.view(), Model::kDirectionColumn,
                       QStringLiteral("directionUnitSelector"), QStringLiteral("\u00B0"));
    expectUnitSelector(table.view(), Model::kDeviationColumn,
                       QStringLiteral("stdDeviationUnitSelector"), QStringLiteral("m/s"));
    expectUnitSelector(table.view(), Model::kTurbulenceColumn,
                       QStringLiteral("turbulenceUnitSelector"), QStringLiteral("%"));
    EXPECT_THROW(static_cast<void>(table.view().unitSelector(Model::kIntensityColumn)), BugError);
}

TEST_F(MultiLevelWindTableViewTest, ChoosingAUnitShowsTheColumnInIt)
{
    WindView         table;
    const QSignalSpy units(&table.model(), &Model::columnUnitChanged);
    table.view()
        .unitSelector(Model::kAltitudeColumn)
        .setSelectedUnit(unitNamed(UnitGroupId::DISTANCE, "ft"));
    EXPECT_EQ(units.count(), 1);
    EXPECT_EQ(table.model().columnUnit(Model::kAltitudeColumn).getUnit(), "ft");
    EXPECT_EQ(table.model().cellText(1, Model::kAltitudeColumn), QStringLiteral("1640"));
}

TEST_F(MultiLevelWindTableViewTest, APressOnANumberCellEditsIt)
{
    WindView table;
    ASSERT_TRUE(Gui::Test::showActive(table.view()));
    table.click(1, Model::kAltitudeColumn);
    UnitSpinBox* const editor = table.editor();
    ASSERT_NE(editor, nullptr);
    EXPECT_EQ(editor->text(), QStringLiteral("500"));
    EXPECT_EQ(table.view().selectedRow(), 1);
    Gui::Test::typeInto(*editor, QStringLiteral("600"));
    QTest::keyClick(editor, Qt::Key_Return);
    EXPECT_TRUE(table.editorClosed());
    EXPECT_EQ(table.altitude(1), 600.0);
    // A press on a cell that is no number opens nothing.
    table.click(1, Model::kIntensityColumn);
    EXPECT_EQ(table.view().openEditor(), nullptr);
}

TEST_F(MultiLevelWindTableViewTest, EscapeDropsTheEdit)
{
    WindView table;
    ASSERT_TRUE(Gui::Test::showActive(table.view()));
    table.click(1, Model::kSpeedColumn);
    UnitSpinBox* const editor = table.editor();
    ASSERT_NE(editor, nullptr);
    Gui::Test::typeInto(*editor, QStringLiteral("9"));
    QTest::keyClick(editor, Qt::Key_Escape);
    EXPECT_TRUE(table.editorClosed());
    EXPECT_EQ(table.model().cellText(1, Model::kSpeedColumn), QStringLiteral("7.5"));
}

TEST_F(MultiLevelWindTableViewTest, TabMovesToTheNextCellsEditor)
{
    WindView table;
    ASSERT_TRUE(Gui::Test::showActive(table.view()));
    table.click(0, Model::kAltitudeColumn);
    UnitSpinBox* const altitude = table.editor();
    ASSERT_NE(altitude, nullptr);
    Gui::Test::typeInto(*altitude, QStringLiteral("-10"));
    QTest::keyClick(altitude, Qt::Key_Tab);
    EXPECT_TRUE(QTest::qWaitFor([&table] {
        return table.view().openEditor() != nullptr &&
               table.view().currentIndex().column() == Model::kSpeedColumn;
    }));
    EXPECT_EQ(table.altitude(0), -10.0);
    const UnitSpinBox* const speed = table.editor();
    ASSERT_NE(speed, nullptr);
    EXPECT_EQ(speed->text(), QStringLiteral("3"));
    EXPECT_TRUE(table.view().commitEditing());
    EXPECT_EQ(table.view().openEditor(), nullptr);
}

TEST_F(MultiLevelWindTableViewTest, AnAltitudeEditMovesTheSelectedRow)
{
    WindView table;
    ASSERT_TRUE(Gui::Test::showActive(table.view()));
    table.click(0, Model::kAltitudeColumn);
    UnitSpinBox* const editor = table.editor();
    ASSERT_NE(editor, nullptr);
    Gui::Test::typeInto(*editor, QStringLiteral("800"));
    QTest::keyClick(editor, Qt::Key_Return);
    EXPECT_TRUE(table.editorClosed());
    // OpenRocket bug 32: the levels sorted; the edited level's row is still the selected one.
    EXPECT_EQ(Gui::Test::windModelLine(table.wind()), "MODEL 500.0 800.0 1250.5");
    EXPECT_EQ(table.view().selectedRow(), 1);
    EXPECT_EQ(table.view().selectedLevel(), table.wind().getLevels().at(1));
}

TEST_F(MultiLevelWindTableViewTest, CommitEditingCommitsAPendingEdit)
{
    WindView table;
    ASSERT_TRUE(Gui::Test::showActive(table.view()));
    EXPECT_FALSE(table.view().commitEditing());
    table.click(2, Model::kDirectionColumn);
    UnitSpinBox* const editor = table.editor();
    ASSERT_NE(editor, nullptr);
    Gui::Test::typeInto(*editor, QStringLiteral("45"));
    EXPECT_TRUE(table.view().commitEditing());
    EXPECT_EQ(table.view().openEditor(), nullptr);
    EXPECT_EQ(table.model().cellText(2, Model::kDirectionColumn), QStringLiteral("45"));
}

TEST_F(MultiLevelWindTableViewTest, TheSelectionStaysWhenTheFocusLeaves)
{
    // OpenRocket bug 33: Java drops the selection when its spinner loses the focus.
    // The window first: the view, a member of the table, goes before it.
    QWidget  window;
    QWidget  other(&window);
    WindView table;
    other.setFocusPolicy(Qt::StrongFocus);
    table.view().setParent(&window);
    ASSERT_TRUE(Gui::Test::showActiveWith(window, {&table.view(), &other}));
    table.click(2, Model::kSpeedColumn);
    ASSERT_NE(table.view().openEditor(), nullptr);
    const QSignalSpy selections(&table.view(), &View::selectedLevelChanged);
    other.setFocus();
    EXPECT_TRUE(table.editorClosed());
    EXPECT_EQ(table.view().selectedRow(), 2);
    EXPECT_EQ(selections.count(), 0);
}

TEST_F(MultiLevelWindTableViewTest, ClickingTheDeleteColumnDeletesItsRow)
{
    WindView         table;
    const QSignalSpy selections(&table.view(), &View::selectedLevelChanged);
    ASSERT_TRUE(Gui::Test::showActive(table.view()));
    table.click(1, Model::kDeleteColumn);
    EXPECT_EQ(Gui::Test::windModelLine(table.wind()), "MODEL 0.0 1250.5");
    // Java: selectRow(null).
    EXPECT_EQ(table.view().selectedRow(), -1);
    EXPECT_GE(selections.count(), 1);
    table.click(1, Model::kDeleteColumn);
    EXPECT_EQ(Gui::Test::windModelLine(table.wind()), "MODEL 0.0");
    // The last level stays.
    table.click(0, Model::kDeleteColumn);
    EXPECT_EQ(Gui::Test::windModelLine(table.wind()), "MODEL 0.0");
}

TEST_F(MultiLevelWindTableViewTest, TheDeleteKeyDeletesTheSelectedRow)
{
    WindView table;
    ASSERT_TRUE(Gui::Test::showActive(table.view()));
    table.view().selectRow(2);
    table.view().setFocus();
    QTest::keyClick(&table.view(), Qt::Key_Delete);
    EXPECT_EQ(Gui::Test::windModelLine(table.wind()), "MODEL 0.0 500.0");
    EXPECT_EQ(table.view().selectedRow(), -1);
    // Without a selection it deletes nothing.
    QTest::keyClick(&table.view(), Qt::Key_Delete);
    EXPECT_EQ(Gui::Test::windModelLine(table.wind()), "MODEL 0.0 500.0");
    EXPECT_FALSE(table.view().deleteSelectedLevel());
}

TEST_F(MultiLevelWindTableViewTest, ThePopupMenuDeletesItsRow)
{
    WindView table;
    ASSERT_TRUE(Gui::Test::showActive(table.view()));
    {
        const Gui::Test::OffscreenPopupWarnings warnings;
        QContextMenuEvent                       event(
            QContextMenuEvent::Mouse, table.cell(1, Model::kSpeedColumn),
            table.view().viewport()->mapToGlobal(table.cell(1, Model::kSpeedColumn)));
        QApplication::sendEvent(table.view().viewport(), &event);
        EXPECT_TRUE(table.view().popupMenu().isVisible());
        EXPECT_EQ(table.view().selectedRow(), 1);
        EXPECT_TRUE(table.view().deleteAction().isEnabled());
        EXPECT_EQ(table.view().deleteAction().text(), QStringLiteral("Delete level"));
        table.view().deleteAction().trigger();
        table.view().popupMenu().hide();
    }
    EXPECT_EQ(Gui::Test::windModelLine(table.wind()), "MODEL 0.0 1250.5");
    EXPECT_EQ(table.view().selectedRow(), -1);
}

TEST_F(MultiLevelWindTableViewTest, ThePopupMenuOfTheLastLevelDeletesNothing)
{
    WindView table;
    ASSERT_TRUE(table.model().deleteLevel(0));
    ASSERT_TRUE(table.model().deleteLevel(0));
    ASSERT_TRUE(Gui::Test::showActive(table.view()));
    {
        const Gui::Test::OffscreenPopupWarnings warnings;
        table.view().showPopupMenu(0, table.view().mapToGlobal(QPoint(10, 10)));
        EXPECT_FALSE(table.view().deleteAction().isEnabled());
        table.view().deleteAction().trigger();
        table.view().popupMenu().hide();
    }
    EXPECT_EQ(Gui::Test::windModelLine(table.wind()), "MODEL 1250.5");
}

TEST_F(MultiLevelWindTableViewTest, AddLevelScrollsToTheNewRow)
{
    WindView table;
    table.view().resize(400, 120);
    ASSERT_TRUE(Gui::Test::showActive(table.view()));
    addLevels(table.view(), 10);
    const int last = table.model().rowCount() - 1;
    EXPECT_EQ(last, 12);
    EXPECT_TRUE(table.view().viewport()->rect().intersects(
        table.view().visualRect(table.model().index(last, 0))));
}

TEST_F(MultiLevelWindTableViewTest, TheViewMayGoWithAnEditOpen)
{
    // Under ASan: the editor's binding and the delegate go with the view.
    auto table = std::make_unique<WindView>();
    ASSERT_TRUE(Gui::Test::showActive(table->view()));
    table->click(0, Model::kTurbulenceColumn);
    UnitSpinBox* const editor = table->editor();
    ASSERT_NE(editor, nullptr);
    Gui::Test::typeInto(*editor, QStringLiteral("20"));
    table.reset();
}

}  // namespace QtRocket::Gui
