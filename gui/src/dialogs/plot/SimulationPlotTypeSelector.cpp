#include "QtRocket/gui/dialogs/plot/SimulationPlotTypeSelector.h"

#include <cstddef>
#include <vector>

#include <QColor>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QString>
#include <QWidget>
#include <Qt>

#include "QtRocket/gui/dialogs/plot/PlotTypeSelector.h"
#include "QtRocket/gui/util/Icons.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/widgets/ColorChooserButton.h"
#include "QtRocket/gui/widgets/LineStyleComboBox.h"
#include "QtRocket/util/LineStyle.h"

namespace QtRocket::Gui
{

namespace
{

/// An icon button of the second line (JButton(Icon)), named @p name, with @p toolTip; never a
/// dialog's default button.
[[nodiscard]] QPushButton* iconButton(QWidget* parent, Icons::Id icon, const QString& name,
                                      const QString& toolTip)
{
    auto* button = makeChild<QPushButton>(parent);
    button->setObjectName(name);
    button->setIcon(Icons::icon(icon));
    button->setToolTip(toolTip);
    button->setAutoDefault(false);
    return button;
}

}  // namespace

SimulationPlotTypeSelector::SimulationPlotTypeSelector(
    std::size_t plotIndex, const FlightDataType& type, const Unit* unit, int position,
    const std::vector<const FlightDataType*>& availableTypes, const QColor& initialColor,
    LineStyle initialLineStyle, UserPrompts& prompts, QWidget* parent)
  : PlotTypeSelector(plotIndex, type, unit, position, availableTypes, parent),
    m_colorButton(makeChild<ColorChooserButton>(this, initialColor, prompts)),
    m_lineStyleSelector(makeChild<LineStyleComboBox>(this, initialLineStyle)),
    m_setDefaultButton(iconButton(
        this, Icons::Id::SET_AS_DEFAULT, QStringLiteral("setDefaultButton"),
        //: simplotpanel.but.SetAsDefault.ttip
        tr("<html>Set as default<br>Save the current color and line style as the application "
           "default for this data type</html>"))),
    m_resetToFactoryButton(iconButton(
        this, Icons::Id::RESET, QStringLiteral("resetToFactoryButton"),
        //: simplotpanel.but.ResetToFactory.ttip
        tr("<html>Reset to factory default<br>Revert the color and line style to their built-in "
           "defaults for this plot</html>")))
{
    m_colorButton->setObjectName(QStringLiteral("colorButton"));
    m_colorButton->setAutoDefault(false);
    m_lineStyleSelector->setObjectName(QStringLiteral("lineStyleSelector"));
    addLine(makeAppearancePanel());
}

SimulationPlotTypeSelector::~SimulationPlotTypeSelector() = default;

const QColor& SimulationPlotTypeSelector::selectedColor() const noexcept
{
    return m_colorButton->selectedColor();
}

void SimulationPlotTypeSelector::setSelectedColor(const QColor& color)
{
    m_colorButton->setSelectedColor(color);
}

LineStyle SimulationPlotTypeSelector::selectedLineStyle() const
{
    return m_lineStyleSelector->selectedLineStyle();
}

void SimulationPlotTypeSelector::setSelectedLineStyle(LineStyle style)
{
    m_lineStyleSelector->setSelectedLineStyle(style);
}

void SimulationPlotTypeSelector::updateButtonStates(bool isAtFactoryDefault, bool isAtAppDefault)
{
    m_resetToFactoryButton->setEnabled(!isAtFactoryDefault);
    m_setDefaultButton->setEnabled(!isAtAppDefault);
}

QWidget* SimulationPlotTypeSelector::makeAppearancePanel()
{
    auto* panel = makeChild<QWidget>(this);
    panel->setObjectName(QStringLiteral("appearancePanel"));
    auto* layout = makeChild<QHBoxLayout>(panel);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(kRelatedGap);
    //: simplotpanel.lbl.Color
    layout->addWidget(makeChild<QLabel>(panel, tr("Color:")), 0, Qt::AlignTop);
    layout->addWidget(m_colorButton, 0, Qt::AlignTop);
    // "gapleft para, top".
    layout->addSpacing(kParagraphGap);
    //: simplotpanel.lbl.LineStyle
    layout->addWidget(makeChild<QLabel>(panel, tr("Line style:")), 0, Qt::AlignTop);
    layout->addWidget(m_lineStyleSelector, 0, Qt::AlignTop);
    layout->addSpacing(kParagraphGap);
    layout->addWidget(m_setDefaultButton, 0, Qt::AlignTop);
    layout->addWidget(m_resetToFactoryButton, 0, Qt::AlignTop);
    return panel;
}

}  // namespace QtRocket::Gui
