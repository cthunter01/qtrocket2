// The simulations table's warnings cell (Java: SimulationPanel's WarningsBox in its
// WarningsBoxRenderer). OpenRocket has no JUnit test of it; the texts of its labels for every row
// of the sixteen examples are pinned against OpenRocket's own boxes in
// SimulationTableModelTests.cpp (the examples have normal and informational warnings only). These
// tests pin the layout (WarningsBox's order, the separators, the padding, the widths and the
// vertical centring), the colours and what is painted, for every combination of the three levels,
// without pixels.

#include "QtRocket/gui/simulation/SimulationWarningsDelegate.h"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include <QFont>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QModelIndex>
#include <QPainter>
#include <QPalette>
#include <QRect>
#include <QSize>
#include <QStandardItemModel>
#include <QString>
#include <QStyle>
#include <QStyleOptionViewItem>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/gui/simulation/SimulationColumns.h"
#include "QtRocket/gui/simulation/SimulationTableModel.h"
#include "QtRocket/gui/util/Icons.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "figure/RecordingPaintDevice.h"

namespace QtRocket::Gui
{

namespace
{

using SimulationWarningsDelegateTest = Test::GuiTest;
using Counts                         = SimulationColumns::WarningCounts;
using Part                           = SimulationWarningsDelegate::Part;

/// The counts of @p critical, @p normal and @p informational warnings.
[[nodiscard]] constexpr Counts levels(std::size_t critical, std::size_t normal,
                                      std::size_t informational) noexcept
{
    return Counts{.critical = critical, .normal = normal, .informational = informational};
}

/// The cell's rectangle of the tests: 300 x 24 pixels at (5, 10).
constexpr QRect kCell(5, 10, 300, 24);

/// A one-cell model whose cell has @p counts in the model's roles (none: no data).
class CountsModel
{
public:
    explicit CountsModel(std::optional<Counts> counts) : m_model(1, 1)
    {
        if (counts.has_value())
        {
            const QModelIndex cell = m_model.index(0, 0);
            m_model.setData(cell, static_cast<int>(counts->critical),
                            SimulationTableModel::kCriticalWarningsRole);
            m_model.setData(cell, static_cast<int>(counts->normal),
                            SimulationTableModel::kNormalWarningsRole);
            m_model.setData(cell, static_cast<int>(counts->informational),
                            SimulationTableModel::kInformationalWarningsRole);
        }
    }

    [[nodiscard]] QModelIndex cell() const { return m_model.index(0, 0); }

private:
    QStandardItemModel m_model;
};

/// The option of a cell of kCell in the application's font and palette, @p selected or not.
[[nodiscard]] QStyleOptionViewItem optionOf(bool selected)
{
    QStyleOptionViewItem option;
    option.rect    = kCell;
    option.font    = QFont();
    option.palette = QGuiApplication::palette();
    option.state   = QStyle::State_Enabled | QStyle::State_Active;
    if (selected)
    {
        option.state |= QStyle::State_Selected;
    }
    return option;
}

/// The parts as text: a text as it is, an icon as [HIGH], [NORMAL] or [LOW].
[[nodiscard]] std::string textOf(const std::vector<Part>& parts)
{
    std::string text;
    for (const Part& part : parts)
    {
        if (!part.icon.has_value())
        {
            text += QtStrings::toUtf8(part.text);
        }
        else if (*part.icon == Icons::Id::WARNING_HIGH)
        {
            text += "[HIGH]";
        }
        else
        {
            text += *part.icon == Icons::Id::WARNING_NORMAL ? "[NORMAL]" : "[LOW]";
        }
    }
    return text;
}

/// The parts of @p counts in kCell, in the application's font.
[[nodiscard]] std::vector<Part> partsOf(const Counts& counts)
{
    return SimulationWarningsDelegate::layOut(counts, kCell, QFont());
}

/// @p part is a text @p text where @p x says, as wide as the font writes it, centred in kCell.
void expectText(const Part& part, const QString& text, int x)
{
    const QFontMetrics metrics{QFont()};
    EXPECT_EQ(part.text, text);
    EXPECT_FALSE(part.icon.has_value());
    EXPECT_EQ(part.rect.left(), x);
    EXPECT_EQ(part.rect.width(), metrics.horizontalAdvance(text));
    EXPECT_EQ(part.rect.height(), metrics.height());
    EXPECT_EQ(part.rect.top(), kCell.top() + ((kCell.height() - metrics.height()) / 2));
}

/// @p part is the icon @p icon where @p x says, a square as high as next to the font, centred.
void expectIcon(const Part& part, Icons::Id icon, int x)
{
    const int side = Icons::iconHeight(icon, QFont());
    EXPECT_TRUE(part.text.isEmpty());
    EXPECT_EQ(part.icon, icon);
    EXPECT_EQ(part.rect, QRect(x, kCell.top() + ((kCell.height() - side) / 2), side, side));
}

// ------------------------------------------------------------------------------ the layout

TEST_F(SimulationWarningsDelegateTest, LaysOutEachLevelAsWarningsBoxDoes)
{
    const std::vector<Part> parts = partsOf(levels(1, 2, 3));
    EXPECT_EQ(textOf(parts), "1 [HIGH], 2 [NORMAL], 3 [LOW]");
    ASSERT_EQ(parts.size(), 8U);
    // From the padding on, each part right after the one before.
    int x = kCell.left() + SimulationWarningsDelegate::kLeftPadding;
    expectText(parts.at(0), QStringLiteral("1 "), x);
    x += parts.at(0).rect.width();
    expectIcon(parts.at(1), Icons::Id::WARNING_HIGH, x);
    x += parts.at(1).rect.width();
    expectText(parts.at(2), QStringLiteral(", "), x);
    x += parts.at(2).rect.width();
    expectText(parts.at(3), QStringLiteral("2 "), x);
    x += parts.at(3).rect.width();
    expectIcon(parts.at(4), Icons::Id::WARNING_NORMAL, x);
    x += parts.at(4).rect.width();
    expectText(parts.at(5), QStringLiteral(", "), x);
    x += parts.at(5).rect.width();
    expectText(parts.at(6), QStringLiteral("3 "), x);
    x += parts.at(6).rect.width();
    expectIcon(parts.at(7), Icons::Id::WARNING_LOW, x);
}

TEST_F(SimulationWarningsDelegateTest, LeavesOutALevelWithoutWarnings)
{
    EXPECT_EQ(textOf(partsOf(levels(0, 1, 0))), "1 [NORMAL]");
    EXPECT_EQ(textOf(partsOf(levels(0, 0, 4))), "4 [LOW]");
    EXPECT_EQ(textOf(partsOf(levels(2, 0, 1))), "2 [HIGH], 1 [LOW]");
    EXPECT_EQ(textOf(partsOf(levels(12, 3, 0))), "12 [HIGH], 3 [NORMAL]");
    EXPECT_EQ(textOf(partsOf(levels(0, 2, 1))), "2 [NORMAL], 1 [LOW]");
    EXPECT_TRUE(partsOf(levels(0, 0, 0)).empty());
}

TEST_F(SimulationWarningsDelegateTest, ReadsTheCountsFromTheModelsRoles)
{
    const CountsModel warned(levels(1, 0, 2));
    EXPECT_EQ(SimulationWarningsDelegate::counts(warned.cell()), levels(1, 0, 2));
    // No data: no counts, and nothing drawn but the padding.
    const CountsModel withoutData(std::nullopt);
    EXPECT_EQ(SimulationWarningsDelegate::counts(withoutData.cell()), std::nullopt);
    const SimulationWarningsDelegate::Cell cell =
        SimulationWarningsDelegate::cell(optionOf(false), withoutData.cell());
    EXPECT_TRUE(cell.parts.empty());
    EXPECT_EQ(cell.width, SimulationWarningsDelegate::kLeftPadding);
    EXPECT_EQ(SimulationWarningsDelegate::counts(QModelIndex()), std::nullopt);
}

TEST_F(SimulationWarningsDelegateTest, WritesInTheTextOrTheSelectionsColour)
{
    const CountsModel                      model(levels(0, 1, 0));
    const SimulationWarningsDelegate::Cell plain =
        SimulationWarningsDelegate::cell(optionOf(false), model.cell());
    EXPECT_EQ(plain.textColor, QGuiApplication::palette().color(QPalette::Active, QPalette::Text));
    EXPECT_EQ(plain.font, QFont());
    const SimulationWarningsDelegate::Cell selected =
        SimulationWarningsDelegate::cell(optionOf(true), model.cell());
    EXPECT_EQ(selected.textColor,
              QGuiApplication::palette().color(QPalette::Active, QPalette::HighlightedText));
    EXPECT_EQ(textOf(selected.parts), "1 [NORMAL]");
}

TEST_F(SimulationWarningsDelegateTest, IsAsWideAsThePaddingAndItsParts)
{
    const CountsModel                model(levels(3, 0, 1));
    const SimulationWarningsDelegate delegate(nullptr);
    const QStyleOptionViewItem       option = optionOf(false);
    const std::vector<Part>          parts  = partsOf(levels(3, 0, 1));
    int                              width  = SimulationWarningsDelegate::kLeftPadding;
    int                              height = 0;
    for (const Part& part : parts)
    {
        width += part.rect.width();
        height = std::max(height, part.rect.height());
    }
    EXPECT_EQ(SimulationWarningsDelegate::cell(option, model.cell()).width, width);
    const QSize hint = delegate.sizeHint(option, model.cell());
    EXPECT_EQ(hint.width(), width);
    EXPECT_GE(hint.height(), height);
}

TEST_F(SimulationWarningsDelegateTest, PaintsTheTextsInTheirPlaces)
{
    const CountsModel                model(levels(1, 2, 0));
    const SimulationWarningsDelegate delegate(nullptr);
    Gui::Test::RecordingPaintDevice  device(QSize(400, 60));
    {
        QPainter painter(&device);
        delegate.paint(&painter, optionOf(false), model.cell());
    }
    std::vector<std::string> texts;
    for (const Gui::Test::RecordedPaint& record : device.records())
    {
        if (record.kind == Gui::Test::RecordedPaint::Kind::TEXT)
        {
            texts.push_back(QtStrings::toUtf8(record.text));
            EXPECT_EQ(record.pen.color(),
                      QGuiApplication::palette().color(QPalette::Active, QPalette::Text));
        }
    }
    EXPECT_EQ(texts, (std::vector<std::string>{"1 ", ", ", "2 "}));
}

TEST_F(SimulationWarningsDelegateTest, PaintsNoTextWithoutWarnings)
{
    const CountsModel                model(levels(0, 0, 0));
    const SimulationWarningsDelegate delegate(nullptr);
    Gui::Test::RecordingPaintDevice  device(QSize(400, 60));
    {
        QPainter painter(&device);
        delegate.paint(&painter, optionOf(true), model.cell());
    }
    std::size_t texts = 0;
    for (const Gui::Test::RecordedPaint& record : device.records())
    {
        texts += record.kind == Gui::Test::RecordedPaint::Kind::TEXT ? 1U : 0U;
    }
    EXPECT_EQ(texts, 0U);
}

}  // namespace

}  // namespace QtRocket::Gui
