#include "QtRocket/gui/dialogs/simulation/CsvOptionsDialog.h"

#include <optional>
#include <string>
#include <vector>

#include <QDialogButtonBox>
#include <QPushButton>
#include <QString>
#include <QWidget>

#include "QtRocket/gui/dialogs/ModalDialog.h"
#include "QtRocket/gui/dialogs/OptionDialog.h"
#include "QtRocket/gui/settings/CsvOptionPreferences.h"
#include "QtRocket/gui/util/CsvOptionValues.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/widgets/CsvOptionPanel.h"

namespace QtRocket::Gui
{

CsvOptionsDialog::CsvOptionsDialog(const QString& title, const CsvOptionValues& values,
                                   QWidget* parent)
  : OptionDialog(MessageType::PLAIN, title, parent),
    m_commentOptions(values.commentOptions),
    // CsvOptionPanel(SimulationTableCSVExport.class): no comment options.
    m_panel(makeChild<CsvOptionPanel>(
        messageArea(), std::string(CsvOptionPreferences::kSimulationTableCsvExport),
        std::vector<CsvOptionPanel::CommentOption>{}, CsvOptionPanel::Arrangement::ON_NEW_ROWS)),
    m_ok(addOption(QDialogButtonBox::Ok)),
    m_cancel(addOption(QDialogButtonBox::Cancel))
{
    setObjectName(QStringLiteral("csvOptionsDialog"));
    m_panel->setObjectName(QStringLiteral("csvOptions"));
    CsvOptionValues shown = values;
    shown.commentOptions.clear();
    m_panel->setValues(shown);
    addMessageWidget(m_panel);
    setInitialOption(m_ok);
}

CsvOptionsDialog::~CsvOptionsDialog() = default;

std::optional<CsvOptionValues> CsvOptionsDialog::ask(QWidget* parent, const QString& title,
                                                     const CsvOptionValues& values)
{
    ModalDialog<CsvOptionsDialog> dialog(parent, title, values);
    dialog.exec();
    const CsvOptionsDialog* const question = dialog.get();
    if (question == nullptr || !question->isOk())
    {
        return std::nullopt;
    }
    return question->values();
}

CsvOptionValues CsvOptionsDialog::values() const
{
    CsvOptionValues values = m_panel->values();
    values.commentOptions  = m_commentOptions;
    return values;
}

bool CsvOptionsDialog::isOk() const noexcept
{
    return chosenOption() == m_ok;
}

}  // namespace QtRocket::Gui
