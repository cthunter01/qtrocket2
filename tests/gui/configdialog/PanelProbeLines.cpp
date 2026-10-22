#include "PanelProbeLines.h"

#include <algorithm>
#include <cstddef>
#include <format>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <QLabel>
#include <QList>
#include <QObject>
#include <QString>
#include <QWidget>

#include "QtRocket/gui/configdialog/MaterialPanel.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/widgets/BoolCheckBox.h"
#include "QtRocket/gui/widgets/EnumComboBox.h"
#include "QtRocket/gui/widgets/IntSpinBox.h"
#include "QtRocket/gui/widgets/UnitSelector.h"
#include "QtRocket/gui/widgets/UnitSpinBox.h"
#include "QtRocket/gui/widgets/ValueSlider.h"
#include "tree/TreeEditingSupport.h"

namespace QtRocket::Gui::Test
{

namespace
{

/// Whether @p widget lies in a MaterialPanel under @p root (the probes leave it out).
[[nodiscard]] bool inMaterialPanel(const QWidget& widget, const QWidget& root)
{
    for (const QWidget* ancestor = &widget; ancestor != nullptr && ancestor != &root;
         ancestor                = ancestor->parentWidget())
    {
        if (qobject_cast<const MaterialPanel*>(ancestor) != nullptr)
        {
            return true;
        }
    }
    return false;
}

/// The line of a number widget (a spin box, a unit selector, a slider); empty for another widget.
[[nodiscard]] std::string numberLine(const QWidget& widget, const std::string& name)
{
    if (const auto* spin = qobject_cast<const UnitSpinBox*>(&widget))
    {
        return std::format("spinner {} {} enabled={}", name, quoted(spin->text()),
                           javaBool(spin->isEnabled()));
    }
    if (const auto* count = qobject_cast<const IntSpinBox*>(&widget))
    {
        return std::format("spinner {} {} enabled={}", name, quoted(count->text()),
                           javaBool(count->isEnabled()));
    }
    if (const auto* unit = qobject_cast<const UnitSelector*>(&widget))
    {
        return std::format("unit {} {}", name, quoted(unit->text()));
    }
    if (const auto* slider = qobject_cast<const ValueSlider*>(&widget))
    {
        return std::format("slider {} {} enabled={}", name, slider->value(),
                           javaBool(slider->isEnabled()));
    }
    return {};
}

}  // namespace

std::string quoted(const QString& text)
{
    return '"' + QtStrings::toUtf8(text) + '"';
}

std::string quotedOrNull(const QString& text)
{
    return text.isEmpty() ? std::string("null") : quoted(text);
}

std::string checkLine(const BoolCheckBox& box)
{
    return std::format("check {} {} checked={} enabled={} tip={}",
                       QtStrings::toUtf8(box.objectName()), quoted(box.text()),
                       javaBool(box.isChecked()), javaBool(box.isEnabled()),
                       quotedOrNull(box.toolTip()));
}

std::string widgetLine(const QWidget& widget, ProbeLineOptions options)
{
    if (std::string line = numberLine(widget, QtStrings::toUtf8(widget.objectName()));
        !line.empty())
    {
        return line;
    }
    if (const auto* box = qobject_cast<const BoolCheckBox*>(&widget))
    {
        return checkLine(*box);
    }
    if (const auto* combo = qobject_cast<const EnumComboBox*>(&widget))
    {
        return std::format("combo {} enabled={}", quoted(combo->shownText()),
                           javaBool(combo->isEnabled()));
    }
    const auto* label = qobject_cast<const QLabel*>(&widget);
    if (label == nullptr || !options.labels || label->text().isEmpty())
    {
        return {};
    }
    std::string line =
        std::format("label {} enabled={}", quoted(label->text()), javaBool(label->isEnabled()));
    if (options.labelTips)
    {
        line += " tip=" + quotedOrNull(label->toolTip());
    }
    return line;
}

void addTabWidgetLines(const QWidget& tab, ProbeLineOptions options,
                       std::vector<std::string>& lines)
{
    for (const QWidget* widget : tab.findChildren<QWidget*>())
    {
        if (inMaterialPanel(*widget, tab))
        {
            continue;
        }
        std::string line = widgetLine(*widget, options);
        if (!line.empty())
        {
            lines.push_back(std::move(line));
        }
    }
}

std::string pinnedLineDifferences(const std::vector<std::string>&   actual,
                                  std::span<const std::string_view> expected)
{
    std::string       text;
    const std::size_t count = std::max(actual.size(), expected.size());
    for (std::size_t i = 0; i < count; ++i)
    {
        const std::string_view pinned = i < expected.size() ? expected[i] : "(none)";
        const std::string_view ours   = i < actual.size() ? std::string_view(actual[i]) : "(none)";
        if (pinned != ours)
        {
            text += std::format("line {}:\n- {}\n+ {}\n", i, pinned, ours);
        }
    }
    return text;
}

}  // namespace QtRocket::Gui::Test
