#include "dialogs/simulation/SimulationDialogWalk.h"

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <QAbstractButton>
#include <QAbstractSpinBox>
#include <QCheckBox>
#include <QFrame>
#include <QGroupBox>
#include <QLabel>
#include <QLayout>
#include <QLayoutItem>
#include <QObject>
#include <QRadioButton>
#include <QSlider>
#include <QString>
#include <QWidget>

#include "QtRocket/gui/configdialog/ConfigFields.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/widgets/FieldValidator.h"
#include "QtRocket/gui/widgets/UnitSelector.h"
#include "QtRocket/gui/widgets/UnitSpinBox.h"
#include "QtRocket/gui/widgets/ValueSlider.h"

namespace QtRocket::Gui::Test
{

namespace
{

/// @p text with its line breaks written as "\n" (the probe's clean()).
[[nodiscard]] QString clean(QString text)
{
    return text.replace(u'\n', QStringLiteral("\\n"));
}

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
        state += QStringLiteral(" tip=") + clean(widget.toolTip());
    }
    return state;
}

[[nodiscard]] QString selected(bool on)
{
    return on ? QStringLiteral(" selected=true") : QStringLiteral(" selected=false");
}

/// The line of @p widget without its state; nullopt for a container the walk enters.
[[nodiscard]] std::optional<QString> describe(const QWidget& widget)
{
    if (const auto* const spinner = qobject_cast<const QAbstractSpinBox*>(&widget))
    {
        return QStringLiteral("SPINNER ") + clean(spinner->text());
    }
    if (const auto* const radio = qobject_cast<const QRadioButton*>(&widget))
    {
        return QStringLiteral("RADIO ") + radio->text() + selected(radio->isChecked());
    }
    if (const auto* const check = qobject_cast<const QCheckBox*>(&widget))
    {
        return QStringLiteral("CHECK ") + check->text() + selected(check->isChecked());
    }
    // Java's UnitSelector is a label.
    if (const auto* const unit = qobject_cast<const UnitSelector*>(&widget))
    {
        return QStringLiteral("LABEL ") + QtStrings::unitSymbol(unit->getSelectedUnit());
    }
    if (const auto* const button = qobject_cast<const QAbstractButton*>(&widget))
    {
        return QStringLiteral("BUTTON ") + clean(button->text()) +
               (button->icon().isNull() ? QString() : QStringLiteral(" icon"));
    }
    if (const auto* const slider = qobject_cast<const QSlider*>(&widget))
    {
        return QStringLiteral("SLIDER %1..%2 at %3")
            .arg(slider->minimum())
            .arg(slider->maximum())
            .arg(slider->value());
    }
    if (const auto* const label = qobject_cast<const QLabel*>(&widget))
    {
        return QStringLiteral("LABEL ") + clean(label->text());
    }
    // After the label, which is a frame too.
    if (const auto* const frame = qobject_cast<const QFrame*>(&widget);
        frame != nullptr && frame->frameShape() == QFrame::HLine)
    {
        return QStringLiteral("SEPARATOR");
    }
    return std::nullopt;
}

void walkWidget(const QWidget& widget, const QWidget& root, const QString& indent,
                std::vector<std::string>& lines);

void walkLayout(const QLayout& layout, const QWidget& root, const QString& indent,
                std::vector<std::string>& lines)
{
    for (int index = 0; index < layout.count(); ++index)
    {
        QLayoutItem* const item = layout.itemAt(index);
        if (const QLayout* const inner = item->layout(); inner != nullptr)
        {
            walkLayout(*inner, root, indent, lines);
        }
        else if (const QWidget* const widget = item->widget(); widget != nullptr)
        {
            walkWidget(*widget, root, indent, lines);
        }
    }
}

void walkWidget(const QWidget& widget, const QWidget& root, const QString& indent,
                std::vector<std::string>& lines)
{
    if (&widget != &root && !widget.isVisibleTo(&root))
    {
        return;
    }
    if (const std::optional<QString> line = describe(widget); line.has_value())
    {
        const bool plain = *line == QStringLiteral("SEPARATOR");
        lines.push_back(QtStrings::toUtf8(indent + *line + (plain ? QString() : stateOf(widget))));
        return;
    }
    QString inner = indent;
    if (const auto* const box = qobject_cast<const QGroupBox*>(&widget);
        box != nullptr && !box->title().isEmpty())
    {
        lines.push_back(QtStrings::toUtf8(indent + QStringLiteral("BORDER ") + box->title()));
        inner += QStringLiteral("  ");
    }
    if (const QLayout* const layout = widget.layout(); layout != nullptr)
    {
        walkLayout(*layout, root, inner, lines);
    }
}

}  // namespace

std::vector<std::string> dialogWalk(const QWidget& root)
{
    std::vector<std::string> lines;
    walkWidget(root, root, QString(), lines);
    return lines;
}

std::string numberRowState(std::string_view name, const ConfigFields::NumberWidgets& row)
{
    const UnitSpinBox& spinBox = *row.spinBox;
    QString            state =
        QtStrings::fromUtf8(name) + QStringLiteral("=[") + spinBox.text() + QStringLiteral("]");
    if (row.slider != nullptr)
    {
        state += QStringLiteral(" at %1").arg(row.slider->value());
    }
    if (!spinBox.isEnabled())
    {
        state += QStringLiteral(" disabled");
    }
    switch (FieldValidator::outlineOf(spinBox))
    {
        case FieldValidator::Outline::NONE:
            break;
        case FieldValidator::Outline::WARNING:
            state += QStringLiteral(" outline=WARNING");
            break;
        case FieldValidator::Outline::ERROR:
            state += QStringLiteral(" outline=ERROR");
            break;
    }
    if (!spinBox.toolTip().isEmpty())
    {
        state += QStringLiteral(" tip=") + clean(spinBox.toolTip());
    }
    return QtStrings::toUtf8(state);
}

std::vector<std::string> linesWithPrefix(std::span<const std::string_view> lines,
                                         std::string_view                  prefix)
{
    std::vector<std::string> kept;
    for (const std::string_view line : lines)
    {
        if (line.starts_with(prefix))
        {
            kept.emplace_back(line.substr(prefix.size()));
        }
    }
    return kept;
}

}  // namespace QtRocket::Gui::Test
