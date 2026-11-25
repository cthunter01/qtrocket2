#include "QtRocket/gui/dialogs/plot/PlotTypeSelector.h"

#include <cstddef>
#include <vector>

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QString>
#include <QVBoxLayout>
#include <QWidget>
#include <Qt>

#include "QtRocket/gui/util/Icons.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/widgets/DataTypeComboBox.h"
#include "QtRocket/gui/widgets/UnitSelector.h"
#include "QtRocket/simulation/FlightDataType.h"
#include "QtRocket/unit/Unit.h"
#include "QtRocket/unit/UnitGroup.h"

namespace QtRocket::Gui
{

PlotTypeSelector::PlotTypeSelector(std::size_t plotIndex, const FlightDataType& type,
                                   const Unit* unit, int position,
                                   const std::vector<const FlightDataType*>& availableTypes,
                                   bool addRemoveButton, QWidget* parent)
  : QWidget(parent),
    m_index(plotIndex),
    m_lines(makeChild<QVBoxLayout>(this)),
    m_firstLine(makeChild<QWidget>(this)),
    m_firstLineLayout(makeChild<QHBoxLayout>(m_firstLine)),
    m_typeSelector(makeChild<DataTypeComboBox>(this, availableTypes)),
    m_unitSelector(makeChild<UnitSelector>(this, type.getUnitGroup())),
    m_axisSelector(makeChild<QComboBox>(this)),
    m_removeButton(makeChild<QPushButton>(this))
{
    // MigLayout("ins 0"): no margins; the lines kRelatedGap apart.
    m_lines->setContentsMargins(0, 0, 0, 0);
    m_lines->setSpacing(kRelatedGap);
    m_firstLine->setObjectName(QStringLiteral("firstLine"));
    m_firstLineLayout->setContentsMargins(0, 0, 0, 0);
    m_firstLineLayout->setSpacing(kRelatedGap);
    m_lines->addWidget(m_firstLine);

    m_typeSelector->setObjectName(QStringLiteral("typeSelector"));
    m_typeSelector->setSelectedType(&type);
    // Keep long localized data type names from hiding the controls at the end of the row:
    // "width 200lp!, gapright unrel, top" (DataTypeComboBox keeps the width itself).
    m_firstLineLayout->addWidget(m_typeSelector, 0, Qt::AlignTop);
    m_firstLineLayout->addSpacing(kUnrelatedGap);

    //: simplotpanel.lbl.Unit (PlotTypeSelector's literal "Unit:", not translated in OpenRocket)
    m_firstLineLayout->addWidget(makeChild<QLabel>(this, tr("Unit:")), 0, Qt::AlignTop);
    m_unitSelector->setObjectName(QStringLiteral("unitSelector"));
    if (unit != nullptr)
    {
        m_unitSelector->setSelectedUnit(*unit);
    }
    // "width 40lp, gapright unrel, top".
    m_unitSelector->setMinimumWidth(kUnitSelectorWidth);
    m_firstLineLayout->addWidget(m_unitSelector, 0, Qt::AlignTop);
    m_firstLineLayout->addSpacing(kUnrelatedGap);

    //: simplotpanel.lbl.Axis (PlotTypeSelector's literal "Axis:", not translated in OpenRocket)
    m_firstLineLayout->addWidget(makeChild<QLabel>(this, tr("Axis:")), 0, Qt::AlignTop);
    m_axisSelector->setObjectName(QStringLiteral("axisSelector"));
    for (const QString& name : axisNames())
    {
        m_axisSelector->addItem(name);
    }
    // The index is the axis + 1 (any negative axis is the automatic one).
    m_axisSelector->setCurrentIndex(position < 0 ? 0 : position + 1);
    m_firstLineLayout->addWidget(m_axisSelector, 0, Qt::AlignTop);

    m_removeButton->setObjectName(QStringLiteral("removeButton"));
    m_removeButton->setIcon(Icons::icon(Icons::Id::EDIT_DELETE));
    //: Not translated in OpenRocket (PlotTypeSelector's literal tooltip)
    m_removeButton->setToolTip(tr("Remove this plot"));
    // setBorderPainted(false); never a dialog's default button.
    m_removeButton->setFlat(true);
    m_removeButton->setAutoDefault(false);
    m_removeButton->setVisible(false);
    if (addRemoveButton)
    {
        this->addRemoveButton();
    }
    // The row is as wide as its contents: the space beyond stays empty.
    m_firstLineLayout->addStretch(1);
}

PlotTypeSelector::PlotTypeSelector(std::size_t plotIndex, const FlightDataType& type,
                                   const Unit* unit, int position,
                                   const std::vector<const FlightDataType*>& availableTypes,
                                   QWidget*                                  parent)
  : PlotTypeSelector(plotIndex, type, unit, position, availableTypes, true, parent)
{
}

PlotTypeSelector::~PlotTypeSelector() = default;

const FlightDataType* PlotTypeSelector::selectedType() const noexcept
{
    return m_typeSelector->selectedType();
}

const Unit& PlotTypeSelector::selectedUnit() const noexcept
{
    return m_unitSelector->getSelectedUnit();
}

int PlotTypeSelector::selectedAxis() const noexcept
{
    return m_axisSelector->currentIndex() - 1;
}

void PlotTypeSelector::setUnitGroup(const UnitGroup& unitGroup)
{
    m_unitSelector->setUnitGroup(unitGroup);
}

std::vector<QString> PlotTypeSelector::axisNames()
{
    return {
        //: simplotpanel.AUTO_NAME
        tr("Auto"),
        //: simplotpanel.LEFT_NAME
        tr("Left"),
        //: simplotpanel.RIGHT_NAME
        tr("Right"),
    };
}

void PlotTypeSelector::addRemoveButton()
{
    // "gapright 0, top", after the axis box (before the trailing stretch, when there is one).
    const int  last      = m_firstLineLayout->count() - 1;
    const bool stretched = last >= 0 && m_firstLineLayout->itemAt(last)->spacerItem() != nullptr;
    m_firstLineLayout->insertWidget(stretched ? last : m_firstLineLayout->count(), m_removeButton,
                                    0, Qt::AlignTop);
    m_removeButton->setVisible(true);
}

void PlotTypeSelector::addLine(QWidget* line)
{
    // "newline, gapleft para, spanx": below the lines so far, indented, as wide as it is.
    auto* indented = makeChild<QWidget>(this);
    auto* layout   = makeChild<QHBoxLayout>(indented);
    layout->setContentsMargins(kParagraphGap, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(line, 0, Qt::AlignTop);
    layout->addStretch(1);
    m_lines->addWidget(indented);
}

}  // namespace QtRocket::Gui
