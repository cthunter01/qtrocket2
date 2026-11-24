#include "QtRocket/gui/widgets/CsvOptionPanel.h"

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include <QBoxLayout>
#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QSpinBox>
#include <QString>
#include <QStringList>
#include <QWidget>

#include "QtRocket/gui/settings/CsvOptionPreferences.h"
#include "QtRocket/gui/settings/GuiPreferenceKeys.h"
#include "QtRocket/gui/util/CsvOptionValues.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/widgets/FieldSeparatorComboBox.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/util/BugError.h"

namespace QtRocket::Gui
{

namespace
{

/// The values of a panel with @p optionCount comment options when nothing is stored.
[[nodiscard]] CsvOptionValues defaultValues(std::size_t optionCount)
{
    CsvOptionValues values;
    values.commentOptions.assign(optionCount, GuiPreferenceKeys::kCsvOptionDefault);
    return values;
}

}  // namespace

CsvOptionPanel::CsvOptionPanel(std::string                       baseClassName,
                               const std::vector<CommentOption>& includeComments,
                               Arrangement arrangement, QWidget* parent)
  : QWidget(parent),
    m_baseClassName(std::move(baseClassName)),
    m_format(makeFormatSettings()),
    m_comments(makeComments(includeComments))
{
    // Java: MigLayout("fill, insets 0"), the groups "growx, wrap unrel" or "growx, gapright unrel".
    auto* layout = makeChild<QBoxLayout>(this, arrangement == Arrangement::ON_NEW_ROWS
                                                   ? QBoxLayout::TopToBottom
                                                   : QBoxLayout::LeftToRight);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_format);
    layout->addWidget(m_comments);
    setValues(defaultValues(m_options.size()));
}

QGroupBox* CsvOptionPanel::makeFormatSettings()
{
    //: SimExpPan.border.FormatSettings
    auto* group  = makeChild<QGroupBox>(this, tr("Format settings"));
    auto* layout = makeChild<QGridLayout>(group);
    layout->setColumnStretch(1, 1);

    //// Field separation
    //: SimExpPan.lbl.longA1
    const QString separatorTip = tr("<html>The string used to separate the fields in the exported "
                                    "file. <br>") +
                                 //: SimExpPan.lbl.longA2
                                 tr("Use ',' for a Comma Separated Values (CSV) file.");
    //: SimExpPan.lbl.Fieldsepstr
    auto* separatorLabel = makeChild<QLabel>(group, tr("Field separator string:"));
    separatorLabel->setToolTip(separatorTip);
    layout->addWidget(separatorLabel, 0, 0);
    m_fieldSeparator = makeChild<FieldSeparatorComboBox>(group);
    m_fieldSeparator->setObjectName(QStringLiteral("fieldSeparator"));
    m_fieldSeparator->setToolTip(separatorTip);
    separatorLabel->setBuddy(m_fieldSeparator);
    layout->addWidget(m_fieldSeparator, 0, 1);

    //// Decimal places
    //: SimExpPan.lbl.DecimalPlaces.ttip
    const QString decimalsTip =
        tr("The number of decimal places to use for the variables in the exported file.");
    //: SimExpPan.lbl.DecimalPlaces
    auto* decimalsLabel = makeChild<QLabel>(group, tr("Decimal places:"));
    decimalsLabel->setToolTip(decimalsTip);
    layout->addWidget(decimalsLabel, 1, 0);
    m_decimalPlaces = makeChild<QSpinBox>(group);
    m_decimalPlaces->setObjectName(QStringLiteral("decimalPlacesSpinner"));
    m_decimalPlaces->setRange(CsvOptionPreferences::kMinDecimalPlaces,
                              CsvOptionPreferences::kMaxDecimalPlaces);
    m_decimalPlaces->setSingleStep(1);
    m_decimalPlaces->setToolTip(decimalsTip);
    decimalsLabel->setBuddy(m_decimalPlaces);
    layout->addWidget(m_decimalPlaces, 1, 1);

    //// Exponential notation
    //: SimExpPan.lbl.ExponentialNotation
    m_exponentialNotation = makeChild<QCheckBox>(group, tr("Use exponential notation"));
    m_exponentialNotation->setObjectName(QStringLiteral("exponentialNotationCheckbox"));
    //: SimExpPan.lbl.ExponentialNotation.ttip
    m_exponentialNotation->setToolTip(
        tr("Whether to use exponential notation for the variables "
           "(e.g. 1.5e-4) or not (e.g. 0.00015)."));
    layout->addWidget(m_exponentialNotation, 2, 0);
    return group;
}

QGroupBox* CsvOptionPanel::makeComments(const std::vector<CommentOption>& includeComments)
{
    //: SimExpPan.border.Comments
    auto* group  = makeChild<QGroupBox>(this, tr("Comments"));
    auto* layout = makeChild<QGridLayout>(group);
    layout->setColumnStretch(1, 1);

    // List of include comments options
    m_options.reserve(includeComments.size());
    int row = 0;
    for (const CommentOption& option : includeComments)
    {
        auto* checkBox = makeChild<QCheckBox>(group, option.text);
        checkBox->setObjectName(QStringLiteral("options"));
        checkBox->setToolTip(option.toolTip);
        layout->addWidget(checkBox, row++, 0, 1, 2);
        m_options.push_back(checkBox);
    }

    // Java: the label and the box in one cell ("split 2"), the box growing.
    //: SimExpPan.lbl.ttip.Commentchar
    const QString characterTip = tr("The character(s) that mark a comment line.");
    //: SimExpPan.lbl.Commentchar
    auto* characterLabel = makeChild<QLabel>(group, tr("Comment character:"));
    characterLabel->setToolTip(characterTip);
    layout->addWidget(characterLabel, row, 0);
    m_commentCharacter = makeChild<QComboBox>(group);
    m_commentCharacter->setObjectName(QStringLiteral("commentCharacter"));
    m_commentCharacter->addItems(
        QStringList{QStringLiteral("#"), QStringLiteral("%"), QStringLiteral(";")});
    m_commentCharacter->setEditable(true);
    // As FieldSeparatorComboBox: nothing typed is added to the items or completed.
    m_commentCharacter->setInsertPolicy(QComboBox::NoInsert);
    m_commentCharacter->setCompleter(nullptr);
    m_commentCharacter->setToolTip(characterTip);
    characterLabel->setBuddy(m_commentCharacter);
    layout->addWidget(m_commentCharacter, row, 1);
    return group;
}

QString CsvOptionPanel::fieldSeparator() const
{
    return m_fieldSeparator->separatorString();
}

int CsvOptionPanel::decimalPlaces() const
{
    return m_decimalPlaces->value();
}

bool CsvOptionPanel::isExponentialNotation() const
{
    return m_exponentialNotation->isChecked();
}

QString CsvOptionPanel::commentCharacter() const
{
    return m_commentCharacter->currentText();
}

bool CsvOptionPanel::selectionOption(std::size_t index) const
{
    return optionCheckBox(index)->isChecked();
}

QCheckBox* CsvOptionPanel::optionCheckBox(std::size_t index) const
{
    if (index >= m_options.size())
    {
        bug("The CSV option panel has no such comment option");
    }
    return m_options[index];
}

CsvOptionValues CsvOptionPanel::values() const
{
    CsvOptionValues values;
    values.fieldSeparator      = fieldSeparator();
    values.decimalPlaces       = decimalPlaces();
    values.exponentialNotation = isExponentialNotation();
    values.commentCharacter    = commentCharacter();
    values.commentOptions.reserve(m_options.size());
    for (const QCheckBox* const option : m_options)
    {
        values.commentOptions.push_back(option->isChecked());
    }
    return values;
}

void CsvOptionPanel::setValues(const CsvOptionValues& values)
{
    if (values.commentOptions.size() != m_options.size())
    {
        bug("CSV option values need one comment option for each of the panel's");
    }
    m_fieldSeparator->setSeparatorString(values.fieldSeparator);
    m_decimalPlaces->setValue(values.decimalPlaces);
    m_exponentialNotation->setChecked(values.exponentialNotation);
    // Java: setSelectedItem() of the editable box, the item or else the text.
    if (const int index = m_commentCharacter->findText(values.commentCharacter); index >= 0)
    {
        m_commentCharacter->setCurrentIndex(index);
    }
    else
    {
        m_commentCharacter->setCurrentIndex(-1);
        m_commentCharacter->setEditText(values.commentCharacter);
    }
    for (std::size_t index = 0; index < m_options.size(); ++index)
    {
        m_options[index]->setChecked(values.commentOptions[index]);
    }
}

void CsvOptionPanel::loadPreferences(const Preferences& preferences)
{
    setValues(CsvOptionPreferences::load(preferences, m_baseClassName, m_options.size()));
}

void CsvOptionPanel::storePreferences(Preferences& preferences) const
{
    CsvOptionPreferences::store(preferences, m_baseClassName, values());
}

}  // namespace QtRocket::Gui
