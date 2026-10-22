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
#include <QLabel>
#include <QLayout>
#include <QLayoutItem>
#include <QObject>
#include <QPushButton>
#include <QString>
#include <QStringList>
#include <QWidget>
#include <QtContainerFwd>

#include "QtRocket/gui/util/QtStrings.h"

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

void chooseItem(QComboBox& comboBox, int index)
{
    comboBox.setCurrentIndex(index);
    Q_EMIT comboBox.activated(index);
}

}  // namespace QtRocket::Gui::Test
