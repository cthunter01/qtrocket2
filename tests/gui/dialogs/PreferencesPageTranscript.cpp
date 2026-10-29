#include "dialogs/PreferencesPageTranscript.h"

#include <algorithm>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <QAbstractSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QFrame>
#include <QLabel>
#include <QLayout>
#include <QLayoutItem>
#include <QLineEdit>
#include <QObject>
#include <QPushButton>
#include <QString>
#include <QStringList>
#include <QWidget>
#include <QtContainerFwd>

#include <gtest/gtest.h>

#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/widgets/DescriptionArea.h"

namespace QtRocket::Gui::Test
{

namespace
{

/// " disabled" and " tip=<tooltip>", as the probe writes them.
[[nodiscard]] QString stateOf(const QWidget& widget)
{
    QString state;
    if (!widget.isEnabled())
    {
        state += QStringLiteral(" disabled");
    }
    if (!widget.toolTip().isEmpty())
    {
        state += QStringLiteral(" tip=") + widget.toolTip();
    }
    return state;
}

/// The item of @p widget; nullopt for a panel (a widget of no kind the probe writes).
[[nodiscard]] std::optional<QString> describe(const QWidget& widget)
{
    if (const auto* const box = qobject_cast<const QCheckBox*>(&widget))
    {
        return QStringLiteral("CHECK ") + box->text() + QStringLiteral(" selected=") +
               (box->isChecked() ? QStringLiteral("true") : QStringLiteral("false")) +
               stateOf(widget);
    }
    if (const auto* const button = qobject_cast<const QPushButton*>(&widget))
    {
        return QStringLiteral("BUTTON ") + button->text() + stateOf(widget);
    }
    if (const auto* const label = qobject_cast<const QLabel*>(&widget))
    {
        return QStringLiteral("LABEL ") + label->text() + stateOf(widget);
    }
    if (const auto* const combo = qobject_cast<const QComboBox*>(&widget))
    {
        QStringList items;
        for (int index = 0; index < combo->count(); ++index)
        {
            items.append(combo->itemText(index));
        }
        return QStringLiteral("COMBO selected=") + combo->currentText() +
               QStringLiteral(" items=") + items.join(u'|') + stateOf(widget);
    }
    if (const auto* const spinner = qobject_cast<const QAbstractSpinBox*>(&widget))
    {
        return QStringLiteral("SPINNER ") + spinner->text() + QStringLiteral(" value=") +
               spinner->text() + stateOf(widget);
    }
    if (const auto* const field = qobject_cast<const QLineEdit*>(&widget))
    {
        return QStringLiteral("FIELD ") + field->text() + stateOf(widget);
    }
    // Java's JTextComponent: its text, without its state.
    if (const auto* const area = qobject_cast<const DescriptionArea*>(&widget))
    {
        return QStringLiteral("TEXT ") + area->description();
    }
    // After the label and the description, which are frames too.
    if (const auto* const frame = qobject_cast<const QFrame*>(&widget);
        frame != nullptr && frame->frameShape() == QFrame::HLine)
    {
        return QStringLiteral("SEPARATOR");
    }
    return std::nullopt;
}

[[nodiscard]] bool startsWithOneOf(std::string_view                  line,
                                   std::span<const std::string_view> prefixes)
{
    return std::ranges::any_of(
        prefixes, [line](std::string_view prefix) { return line.starts_with(prefix); });
}

void walk(const QLayout& layout, std::vector<std::string>& items)
{
    for (int index = 0; index < layout.count(); ++index)
    {
        QLayoutItem* const item = layout.itemAt(index);
        if (const QLayout* const inner = item->layout(); inner != nullptr)
        {
            walk(*inner, items);
            continue;
        }
        const QWidget* const widget = item->widget();
        if (widget == nullptr)
        {
            continue;  // a spacer
        }
        if (const std::optional<QString> text = describe(*widget); text.has_value())
        {
            items.push_back(QtStrings::toUtf8(*text));
        }
        else if (const QLayout* const panel = widget->layout(); panel != nullptr)
        {
            walk(*panel, items);
        }
    }
}

}  // namespace

std::vector<std::string> pageTranscript(const QWidget& page)
{
    std::vector<std::string> items;
    if (const QLayout* const layout = page.layout(); layout != nullptr)
    {
        walk(*layout, items);
    }
    return items;
}

std::vector<std::string> transcriptOf(std::span<const std::string_view> lines)
{
    return {lines.begin(), lines.end()};
}

std::vector<std::string> transcriptWithout(std::span<const std::string_view> lines,
                                           std::span<const std::string_view> prefixes)
{
    std::vector<std::string> kept;
    for (const std::string_view line : lines)
    {
        if (!startsWithOneOf(line, prefixes))
        {
            kept.emplace_back(line);
        }
    }
    return kept;
}

std::vector<std::string> transcriptKeeping(std::span<const std::string_view> lines,
                                           std::span<const std::string_view> prefixes)
{
    std::vector<std::string> kept;
    for (const std::string_view line : lines)
    {
        if (startsWithOneOf(line, prefixes))
        {
            kept.emplace_back(line);
        }
    }
    return kept;
}

std::vector<std::string> transcriptEdited(std::vector<std::string>        lines,
                                          std::span<const TranscriptEdit> edits)
{
    for (const TranscriptEdit& edit : edits)
    {
        const auto found = std::ranges::count(lines, edit.openRocket);
        if (found != 1)
        {
            ADD_FAILURE() << "the edited line is in the transcript " << found
                          << " times, not once: " << edit.openRocket;
            continue;
        }
        *std::ranges::find(lines, edit.openRocket) = edit.qtRocket;
    }
    return lines;
}

void chooseItem(QComboBox& comboBox, int index)
{
    comboBox.setCurrentIndex(index);
    Q_EMIT comboBox.activated(index);
}

}  // namespace QtRocket::Gui::Test
