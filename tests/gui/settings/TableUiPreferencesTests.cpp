// The column widths, order and visibility of a table in the preferences (Java:
// gui/util/TableUIPreferences.java and GUIUtil.computeOptimalColumnWidths()). OpenRocket has no
// JUnit test of either.
//
// Expectations marked "probe" are OpenRocket's own results, printed by
// tools/openrocket-gui-pins/preset-chooser/probe/TableUiProbe.java (run on OpenRocket 5f164fd0e
// with JDK 17; tools/openrocket-gui-pins/preset-chooser/hand-pinned.txt keeps its transcript): a
// JTable of the eight columns A to H in the parts library's XTableColumnModel over three rows,
// whose renderers' preferred widths are 10 pixels per character (the header's plus 5). ProbeTable
// gives a QTableView the same sizes through Qt::SizeHintRole, Swing's default column width of 75
// (QHeaderView::setDefaultSectionSize()) and its minimum of 15 (setMinimumSectionSize()), so the
// probe's widths are Qt's too. Each step prints as the probe does: the shown columns in view
// order, every column of the table in its order with the hidden ones in brackets, and the width
// of each shown column (Java: its preferred width, which loadTableUIPreferences() sets).

#include "QtRocket/gui/settings/TableUiPreferences.h"

#include <array>
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

#include <QHeaderView>
#include <QSize>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QString>
#include <QTableView>
#include <Qt>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/gui/settings/GuiPreferenceKeys.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/util/BugError.h"

namespace QtRocket::Gui
{

namespace
{

using TableUiPreferencesTest = Test::GuiTest;
using Identifiers            = std::vector<std::optional<std::string>>;

constexpr std::string_view kTable = "CmpPrst.Probe";

constexpr std::size_t                            kColumns = 8;
constexpr std::array<std::string_view, kColumns> kHeaders{"A",    "Bb", "Ccc", "D",
                                                          "Eeee", "F",  "G",   "H"};
using Row = std::array<std::string_view, kColumns>;
constexpr std::array<Row, 3> kRows{
    Row{"x", "y", "zz", "", "wwwwwwww", "v", "u", "t"},
    Row{"xx", "", "z", "dddddddd", "w", "", "u", "t"},
    Row{"x", "yyyyy", "", "d", "w", "v", "", ""},
};

/// The probe's table: its model, a view that is never shown, and the identifiers of its columns
/// (the headers; nullopt for a column removed with remove()).
class ProbeTable
{
public:
    /// The probe's three rows; @p wideG gives the first row a G cell of 250 characters.
    explicit ProbeTable(bool wideG = false)
      : m_model(static_cast<int>(kRows.size()), static_cast<int>(kColumns))
    {
        for (std::size_t column = 0; column < kColumns; ++column)
        {
            const auto header = kHeaders.at(column);
            m_model.setHeaderData(static_cast<int>(column), Qt::Horizontal,
                                  QtStrings::fromUtf8(header));
            m_model.setHeaderData(static_cast<int>(column), Qt::Horizontal,
                                  QSize((10 * static_cast<int>(header.size())) + 5, 16),
                                  Qt::SizeHintRole);
            m_identifiers.emplace_back(header);
        }
        for (std::size_t row = 0; row < kRows.size(); ++row)
        {
            for (std::size_t column = 0; column < kColumns; ++column)
            {
                std::string text{kRows.at(row).at(column)};
                if (wideG && row == 0 && column == 6)
                {
                    text = std::string(250, 'u');
                }
                auto item = std::make_unique<QStandardItem>(QtStrings::fromUtf8(text));
                item->setData(QSize(10 * static_cast<int>(text.size()), 16), Qt::SizeHintRole);
                m_model.setItem(static_cast<int>(row), static_cast<int>(column), item.release());
            }
        }
        QHeaderView& header = *m_view.horizontalHeader();
        header.setMinimumSectionSize(15);
        header.setDefaultSectionSize(75);
        m_view.setShowGrid(false);
        m_view.setModel(&m_model);
    }

    [[nodiscard]] QTableView&        view() noexcept { return m_view; }
    [[nodiscard]] QHeaderView&       header() noexcept { return *m_view.horizontalHeader(); }
    [[nodiscard]] const Identifiers& identifiers() const noexcept { return m_identifiers; }

    /// The logical index of the column @p name.
    [[nodiscard]] static int column(std::string_view name)
    {
        for (std::size_t i = 0; i < kColumns; ++i)
        {
            if (kHeaders.at(i) == name)
            {
                return static_cast<int>(i);
            }
        }
        return -1;
    }

    /// Takes the column @p name out of the table (Java: XTableColumnModel.removeColumn()).
    void remove(std::string_view name)
    {
        m_identifiers[static_cast<std::size_t>(column(name))] = std::nullopt;
    }

    void setVisible(std::string_view name, bool visible)
    {
        header().setSectionHidden(column(name), !visible);
    }

    /// Swing's moveColumn(from, to) among the shown columns.
    void moveShown(int from, int to)
    {
        const std::vector<int> shown = TableUiPreferences::visibleColumns(header(), m_identifiers);
        header().moveSection(header().visualIndex(shown[static_cast<std::size_t>(from)]),
                             header().visualIndex(shown[static_cast<std::size_t>(to)]));
    }

    void load(const Preferences& preferences)
    {
        TableUiPreferences::load(m_view, m_identifiers, kTable, preferences);
    }

    void store(Preferences& preferences) const
    {
        TableUiPreferences::store(m_view, m_identifiers, kTable, preferences);
    }

    /// The identifier of the logical column @p column, "?" for none.
    [[nodiscard]] std::string nameOf(int column) const
    {
        const std::optional<std::string>& identifier =
            m_identifiers.at(static_cast<std::size_t>(column));
        return identifier.has_value() ? *identifier : std::string{"?"};
    }

    /// The probe's "shown:" line.
    [[nodiscard]] std::string shown()
    {
        std::string line;
        for (const int column : TableUiPreferences::visibleColumns(header(), m_identifiers))
        {
            line += " " + nameOf(column);
        }
        return line;
    }

    /// The probe's "all:" line.
    [[nodiscard]] std::string all()
    {
        std::string line;
        for (int visual = 0; visual < header().count(); ++visual)
        {
            const int                         logical = header().logicalIndex(visual);
            const std::optional<std::string>& identifier =
                m_identifiers[static_cast<std::size_t>(logical)];
            if (identifier.has_value())
            {
                line += header().isSectionHidden(logical) ? " [" + *identifier + "]"
                                                          : " " + *identifier;
            }
        }
        return line;
    }

    /// The probe's "preferred:" line: the shown columns' widths.
    [[nodiscard]] std::string widths()
    {
        std::string line;
        for (const int column : TableUiPreferences::visibleColumns(header(), m_identifiers))
        {
            line += " " + nameOf(column) + "=" + std::to_string(header().sectionSize(column));
        }
        return line;
    }

private:
    QStandardItemModel m_model;
    QTableView         m_view;
    Identifiers        m_identifiers;
};

/// Stores @p value under "CmpPrst.Probe.<kind>.<column>" in the node "tables".
void put(Preferences& preferences, std::string_view kind, std::string_view column,
         std::string_view value)
{
    preferences.getNode(GuiPreferenceKeys::kNodeTables)
        .put(std::string{kTable} + "." + std::string{kind} + "." + std::string{column}, value);
}

/// The keys of the node "tables" with their values, as the probe prints them, sorted.
[[nodiscard]] std::vector<std::string> storedKeys(const Preferences& preferences)
{
    std::vector<std::string> lines;
    const Preferences* const node = preferences.findNode(GuiPreferenceKeys::kNodeTables);
    if (node == nullptr)
    {
        return lines;
    }
    for (const std::string& key : node->keys())
    {
        lines.push_back(key + " = " + node->get(key).value_or(""));
    }
    return lines;
}

}  // namespace

TEST_F(TableUiPreferencesTest, NothingStoredShowsEveryColumnAtItsOptimalWidth)
{
    // probe: "== fresh": the widest of the header (10 per character + 5) and the cells, at least
    // 15.
    const InMemoryPreferences preferences;
    ProbeTable                table;
    table.load(preferences);
    EXPECT_EQ(table.shown(), " A Bb Ccc D Eeee F G H");
    EXPECT_EQ(table.all(), " A Bb Ccc D Eeee F G H");
    EXPECT_EQ(table.widths(), " A=20 Bb=50 Ccc=35 D=80 Eeee=80 F=15 G=15 H=15");
    // Reading writes nothing.
    EXPECT_EQ(preferences.findNode(GuiPreferenceKeys::kNodeTables), nullptr);
}

TEST_F(TableUiPreferencesTest, StoreWritesTheShownColumnsAndEveryColumnsVisibility)
{
    InMemoryPreferences preferences;
    ProbeTable          table;
    table.load(preferences);
    // probe: "== fresh, moved, G hidden, widths set": two columns moved, G hidden, the widths 30,
    // 37, 44, ... in view order.
    table.moveShown(0, 3);
    table.moveShown(5, 1);
    table.setVisible("G", false);
    EXPECT_EQ(table.shown(), " Bb F Ccc D A Eeee H");
    EXPECT_EQ(table.all(), " Bb F Ccc D A Eeee [G] H");
    int width = 30;
    for (const int column : TableUiPreferences::visibleColumns(table.header(), table.identifiers()))
    {
        table.header().resizeSection(column, width);
        width += 7;
    }
    table.store(preferences);
    // probe: "== store: 22 keys".
    EXPECT_EQ(storedKeys(preferences),
              (std::vector<std::string>{
                  "CmpPrst.Probe.co.A = 4",     "CmpPrst.Probe.co.Bb = 0",
                  "CmpPrst.Probe.co.Ccc = 2",   "CmpPrst.Probe.co.D = 3",
                  "CmpPrst.Probe.co.Eeee = 5",  "CmpPrst.Probe.co.F = 1",
                  "CmpPrst.Probe.co.H = 6",     "CmpPrst.Probe.cv.A = true",
                  "CmpPrst.Probe.cv.Bb = true", "CmpPrst.Probe.cv.Ccc = true",
                  "CmpPrst.Probe.cv.D = true",  "CmpPrst.Probe.cv.Eeee = true",
                  "CmpPrst.Probe.cv.F = true",  "CmpPrst.Probe.cv.G = false",
                  "CmpPrst.Probe.cv.H = true",  "CmpPrst.Probe.cw.A = 58",
                  "CmpPrst.Probe.cw.Bb = 30",   "CmpPrst.Probe.cw.Ccc = 44",
                  "CmpPrst.Probe.cw.D = 51",    "CmpPrst.Probe.cw.Eeee = 65",
                  "CmpPrst.Probe.cw.F = 37",    "CmpPrst.Probe.cw.H = 72",
              }));

    // probe: "== load what was stored": the one pass of the order does not restore this one
    // exactly (Ccc and D come back swapped), as in Java.
    ProbeTable again;
    again.load(preferences);
    EXPECT_EQ(again.shown(), " Bb F D Ccc A Eeee H");
    EXPECT_EQ(again.all(), " Bb F D Ccc A Eeee [G] H");
    EXPECT_EQ(again.widths(), " Bb=30 F=37 D=51 Ccc=44 A=58 Eeee=65 H=72");
}

TEST_F(TableUiPreferencesTest, TheOrderIsRestoredInJavasOnePass)
{
    struct Case
    {
        std::array<std::string_view, kColumns> order;
        std::string_view                       shown;
    };
    // probe: "== order <the stored indices of A to H>".
    const std::array<Case, 5> cases{
        Case{.order = {"5", "0", "1", "2", "3", "4", "6", "7"}, .shown = " Bb Ccc D Eeee F A G H"},
        Case{.order = {"1", "2", "3", "4", "5", "6", "7", "0"}, .shown = " Bb A D Ccc F Eeee H G"},
        Case{.order = {"7", "6", "5", "4", "3", "2", "1", "0"}, .shown = " H Bb G D F Eeee Ccc A"},
        Case{.order = {"2", "0", "1", "5", "3", "4", "7", "6"}, .shown = " Bb Ccc A Eeee F D H G"},
        Case{.order = {"3", "7", "0", "6", "1", "5", "2", "4"}, .shown = " Ccc Eeee Bb A H G F D"},
    };
    for (const Case& c : cases)
    {
        InMemoryPreferences preferences;
        for (std::size_t i = 0; i < kColumns; ++i)
        {
            put(preferences, "co", kHeaders.at(i), c.order.at(i));
        }
        ProbeTable table;
        table.load(preferences);
        EXPECT_EQ(table.shown(), c.shown) << c.order[0];
        EXPECT_EQ(table.all(), c.shown) << c.order[0];
    }
}

TEST_F(TableUiPreferencesTest, AnIndexTwiceOrOutsideTheColumnsMovesWhatJavasMoves)
{
    // probe: "== order with duplicates and out of range": two columns at 5, one at 9, Eeee's not
    // an integer, D's absent, G at its own index, H at 8 (one past the end).
    InMemoryPreferences preferences;
    put(preferences, "co", "A", "5");
    put(preferences, "co", "Bb", "5");
    put(preferences, "co", "Ccc", "9");
    put(preferences, "co", "Eeee", "x");
    put(preferences, "co", "F", "0");
    put(preferences, "co", "G", "6");
    put(preferences, "co", "H", "8");
    ProbeTable table;
    table.load(preferences);
    EXPECT_EQ(table.shown(), " F Bb Ccc D Eeee A G H");
}

TEST_F(TableUiPreferencesTest, HiddenColumnsComeBackWhereSwingPutsThem)
{
    // probe: "== hidden Bb and D, moved" ("FALSE" is false, "yes" the default true), then each
    // shown again, A hidden while the first column moves to 4, and shown again.
    InMemoryPreferences preferences;
    put(preferences, "cv", "Bb", "false");
    put(preferences, "cv", "D", "FALSE");
    put(preferences, "cv", "F", "yes");
    put(preferences, "co", "A", "2");
    put(preferences, "co", "Ccc", "0");
    put(preferences, "co", "Eeee", "5");
    put(preferences, "co", "G", "1");
    ProbeTable table;
    table.load(preferences);
    EXPECT_EQ(table.shown(), " Ccc G A F H Eeee");
    EXPECT_EQ(table.all(), " [Bb] Ccc [D] G A F H Eeee");
    EXPECT_EQ(table.widths(), " Ccc=35 G=15 A=20 F=15 H=15 Eeee=80");

    table.setVisible("Bb", true);
    EXPECT_EQ(table.all(), " Bb Ccc [D] G A F H Eeee");
    table.setVisible("D", true);
    EXPECT_EQ(table.all(), " Bb Ccc D G A F H Eeee");
    // probe: "Bb=75 ... D=75": a column hidden at the load got no width.
    EXPECT_EQ(table.widths(), " Bb=75 Ccc=35 D=75 G=15 A=20 F=15 H=15 Eeee=80");

    table.setVisible("A", false);
    table.moveShown(0, 4);
    EXPECT_EQ(table.shown(), " Ccc D G F Bb H Eeee");
    EXPECT_EQ(table.all(), " Ccc D G [A] F Bb H Eeee");
    table.setVisible("A", true);
    EXPECT_EQ(table.all(), " Ccc D G A F Bb H Eeee");
}

TEST_F(TableUiPreferencesTest, AMissingWidthGivesTheOthersWithoutOneTheirOptimalWidths)
{
    // probe: "== widths, some missing": A 40, Bb not an integer (its optimal 50), Ccc 5 and F -3
    // (raised to the minimum of 15), D 99999, Eeee and G missing (optimal: G's cell of 250
    // characters is cut to 2000), H a fixed column of 24 with 60 stored.
    InMemoryPreferences preferences;
    put(preferences, "cw", "A", "40");
    put(preferences, "cw", "Bb", "abc");
    put(preferences, "cw", "Ccc", "5");
    put(preferences, "cw", "D", "99999");
    put(preferences, "cw", "F", "-3");
    put(preferences, "cw", "H", "60");
    ProbeTable table(true);
    const int  fixed = ProbeTable::column("H");
    table.header().setSectionResizeMode(fixed, QHeaderView::Fixed);
    table.header().resizeSection(fixed, 24);
    table.load(preferences);
    EXPECT_EQ(table.widths(), " A=40 Bb=50 Ccc=15 D=99999 Eeee=80 F=15 G=2000 H=24");
}

TEST_F(TableUiPreferencesTest, EveryWidthStoredNeedsNoOptimalWidths)
{
    // probe: "== widths, all stored": Eeee's text is not an integer, so it keeps its width (75).
    InMemoryPreferences preferences;
    for (const std::string_view header : kHeaders)
    {
        put(preferences, "cw", header, std::to_string(20 + header.size()));
    }
    put(preferences, "cw", "Eeee", "wide");
    ProbeTable table;
    table.load(preferences);
    EXPECT_EQ(table.widths(), " A=21 Bb=22 Ccc=23 D=21 Eeee=75 F=21 G=21 H=21");

    // probe: "== widths of the shown columns stored, Ccc hidden": a hidden column needs none.
    InMemoryPreferences hidden;
    for (const std::string_view header : kHeaders)
    {
        if (header != "Ccc")
        {
            put(hidden, "cw", header, std::to_string(30 + header.size()));
        }
    }
    put(hidden, "cv", "Ccc", "false");
    ProbeTable withHidden;
    withHidden.load(hidden);
    EXPECT_EQ(withHidden.all(), " A Bb [Ccc] D Eeee F G H");
    EXPECT_EQ(withHidden.widths(), " A=31 Bb=32 D=31 Eeee=34 F=31 G=31 H=31");
}

TEST_F(TableUiPreferencesTest, ANegativeIndexIsIgnored)
{
    // probe: "== order -1 threw java.lang.IllegalArgumentException: moveColumn() - Index out of
    // range", after which Java sets no width (all 75). Deviation: QtRocket ignores the index and
    // goes on (the optimal widths, as with nothing stored).
    InMemoryPreferences preferences;
    put(preferences, "co", "Ccc", "-1");
    ProbeTable table;
    table.load(preferences);
    EXPECT_EQ(table.shown(), " A Bb Ccc D Eeee F G H");
    EXPECT_EQ(table.widths(), " A=20 Bb=50 Ccc=35 D=80 Eeee=80 F=15 G=15 H=15");
}

TEST_F(TableUiPreferencesTest, AColumnOutsideTheTableIsHiddenAndNeverStored)
{
    // probe: "== D removed" (the parts library removes the columns its type does not display):
    // D's keys neither show nor move it, and a store leaves them as they were.
    InMemoryPreferences preferences;
    put(preferences, "cv", "D", "true");
    put(preferences, "co", "D", "0");
    ProbeTable table;
    table.remove("D");
    table.load(preferences);
    EXPECT_EQ(table.shown(), " A Bb Ccc Eeee F G H");
    EXPECT_TRUE(table.header().isSectionHidden(ProbeTable::column("D")));
    EXPECT_EQ(table.widths(), " A=20 Bb=50 Ccc=35 Eeee=80 F=15 G=15 H=15");

    // The probe's table was never laid out: its columns are as wide as Swing's default, 75.
    for (const int column : TableUiPreferences::visibleColumns(table.header(), table.identifiers()))
    {
        table.header().resizeSection(column, 75);
    }
    table.store(preferences);
    // probe: "== store with D removed: 23 keys".
    EXPECT_EQ(storedKeys(preferences),
              (std::vector<std::string>{
                  "CmpPrst.Probe.co.A = 0",       "CmpPrst.Probe.co.Bb = 1",
                  "CmpPrst.Probe.co.Ccc = 2",     "CmpPrst.Probe.co.D = 0",
                  "CmpPrst.Probe.co.Eeee = 3",    "CmpPrst.Probe.co.F = 4",
                  "CmpPrst.Probe.co.G = 5",       "CmpPrst.Probe.co.H = 6",
                  "CmpPrst.Probe.cv.A = true",    "CmpPrst.Probe.cv.Bb = true",
                  "CmpPrst.Probe.cv.Ccc = true",  "CmpPrst.Probe.cv.D = true",
                  "CmpPrst.Probe.cv.Eeee = true", "CmpPrst.Probe.cv.F = true",
                  "CmpPrst.Probe.cv.G = true",    "CmpPrst.Probe.cv.H = true",
                  "CmpPrst.Probe.cw.A = 75",      "CmpPrst.Probe.cw.Bb = 75",
                  "CmpPrst.Probe.cw.Ccc = 75",    "CmpPrst.Probe.cw.Eeee = 75",
                  "CmpPrst.Probe.cw.F = 75",      "CmpPrst.Probe.cw.G = 75",
                  "CmpPrst.Probe.cw.H = 75",
              }));
}

TEST_F(TableUiPreferencesTest, TheKeysAreThoseOfGuiPreferenceKeys)
{
    // A column whose identifier is not ASCII (the parts library's "\u2026 Favorite") and a table
    // named by a component.
    InMemoryPreferences preferences;
    ProbeTable          table;
    Identifiers         identifiers = table.identifiers();
    identifiers[0]                  = "\u2026 Favorite";
    TableUiPreferences::store(table.view(), identifiers,
                              GuiPreferenceKeys::componentPresetTableName(ComponentKind::BODY_TUBE),
                              preferences);
    const Preferences& node = preferences.getNode(GuiPreferenceKeys::kNodeTables);
    EXPECT_EQ(node.get("CmpPrst.Body Tube.cw.\u2026 Favorite"), "75");
    EXPECT_EQ(node.get("CmpPrst.Body Tube.co.\u2026 Favorite"), "0");
    EXPECT_EQ(node.get("CmpPrst.Body Tube.cv.\u2026 Favorite"), "true");
    EXPECT_EQ(node.get("CmpPrst.Body Tube.co.H"), "7");
}

TEST_F(TableUiPreferencesTest, OneIdentifierPerColumnIsNeeded)
{
    InMemoryPreferences preferences;
    ProbeTable          table;
    const Identifiers   tooFew(kColumns - 1, std::string{"x"});
    EXPECT_THROW(TableUiPreferences::load(table.view(), tooFew, kTable, preferences), BugError);
    EXPECT_THROW(TableUiPreferences::store(table.view(), tooFew, kTable, preferences), BugError);
    EXPECT_THROW(std::ignore = TableUiPreferences::visibleColumns(table.header(), tooFew),
                 BugError);
    EXPECT_EQ(preferences.findNode(GuiPreferenceKeys::kNodeTables), nullptr);
}

TEST_F(TableUiPreferencesTest, TheOptimalWidthsReadEveryRow)
{
    // QTableView::sizeHintForColumn() reads 1000 rows by default (the header's
    // resizeContentsPrecision()); Java reads every row: a wide cell in row 1200 counts, and the
    // precision is as it was afterwards.
    QStandardItemModel model(1200, 1);
    for (int row = 0; row < model.rowCount(); ++row)
    {
        auto item = std::make_unique<QStandardItem>(QStringLiteral("x"));
        item->setData(QSize(row == 1199 ? 300 : 10, 16), Qt::SizeHintRole);
        model.setItem(row, 0, item.release());
    }
    model.setHeaderData(0, Qt::Horizontal, QSize(25, 16), Qt::SizeHintRole);
    QTableView view;
    view.setShowGrid(false);
    view.setModel(&model);
    view.horizontalHeader()->setResizeContentsPrecision(1000);
    const std::array<int, 1> columns{0};
    EXPECT_EQ(TableUiPreferences::computeOptimalColumnWidths(view, columns, 20, true),
              std::vector<int>{300});
    EXPECT_EQ(TableUiPreferences::computeOptimalColumnWidths(view, columns, 2, true),
              std::vector<int>{200});
    EXPECT_EQ(view.horizontalHeader()->resizeContentsPrecision(), 1000);
}

}  // namespace QtRocket::Gui
