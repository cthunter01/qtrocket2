#include "QtRocket/gui/dialogs/StorageOptionsDialog.h"

#include <optional>

#include <QAbstractButton>
#include <QButtonGroup>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QString>
#include <QVBoxLayout>
#include <QWidget>

#include "QtRocket/gui/dialogs/ModalDialog.h"
#include "QtRocket/gui/dialogs/OptionDialog.h"
#include "QtRocket/gui/util/MakeChild.h"

namespace QtRocket::Gui
{

StorageOptionsDialog::StorageOptionsDialog(bool saveSimulationData, QWidget* parent)
  : OptionDialog(MessageType::QUESTION,
                 //: StorageOptChooser.lbl.Saveopt
                 tr("Save options"), parent),
    m_indent(makeChild<QWidget>(messageArea())),
    //: StorageOptChooser.ttip.Saveopt
    m_chooser(makeChild<QGroupBox>(m_indent, tr("Save options"))),
    //: StorageOptChooser.lbl.Simdatatostore
    m_dataLabel(makeChild<QLabel>(m_chooser, tr("Simulated data to store:"))),
    //: StorageOptChooser.rdbut.Allsimdata
    m_all(makeChild<QRadioButton>(m_chooser, tr("All simulated data"))),
    //: StorageOptChooser.rdbut.Onlysummarydata
    m_summary(makeChild<QRadioButton>(m_chooser, tr("Only summary data"))),
    m_info(makeChild<QLabel>(m_chooser)),
    // JOptionPane.OK_CANCEL_OPTION.
    m_ok(addOption(QDialogButtonBox::Ok)),
    m_cancel(addOption(QDialogButtonBox::Cancel))
{
    setObjectName(QStringLiteral("storageOptionsDialog"));

    // The empty border of 10 pixels on the left, then the titled border.
    auto* indentLayout = makeChild<QVBoxLayout>(m_indent);
    indentLayout->setContentsMargins(kIndent, 0, 0, 0);
    m_chooser->setObjectName(QStringLiteral("storageOptionChooser"));
    indentLayout->addWidget(m_chooser);
    auto* layout = makeChild<QVBoxLayout>(m_chooser);

    layout->addWidget(m_dataLabel);
    // MigLayout "wrap unrel": the unrelated gap.
    layout->addSpacing(gapHeight());

    auto* group = makeChild<QButtonGroup>(m_chooser);
    m_all->setObjectName(QStringLiteral("allButton"));
    m_all->setToolTip(
        //: StorageOptChooser.lbl.longA1
        tr("<html>Store all simulated data. <br>") +
        //: StorageOptChooser.lbl.longA2
        tr("This can result in very large files!"));
    group->addButton(m_all);
    layout->addWidget(m_all);

    m_summary->setObjectName(QStringLiteral("noneButton"));
    m_summary->setToolTip(
        //: StorageOptChooser.lbl.longC1
        tr("<html>Store only the values shown in the summary table. <br>") +
        //: StorageOptChooser.lbl.longC2
        tr("This results in the smallest files."));
    group->addButton(m_summary);
    layout->addWidget(m_summary);
    layout->addSpacing(kInfoGap);

    m_info->setObjectName(QStringLiteral("infoLabel"));
    //: StorageOptChooser.lbl.longD1
    m_info->setToolTip(
        tr("An estimate on how large the resulting file would be with the present options."));
    layout->addWidget(m_info);
    addMessageWidget(m_indent);

    // loadOptions().
    (saveSimulationData ? m_all : m_summary)->setChecked(true);
    updateInfoLabel();
    connect(group, &QButtonGroup::buttonToggled, this, [this] { updateInfoLabel(); });
    setInitialOption(m_ok);
}

StorageOptionsDialog::~StorageOptionsDialog() = default;

std::optional<bool> StorageOptionsDialog::ask(QWidget* parent, bool saveSimulationData)
{
    ModalDialog<StorageOptionsDialog> dialog(parent, saveSimulationData);
    dialog.exec();
    const StorageOptionsDialog* const question = dialog.get();
    if (question == nullptr || !question->isOk())
    {
        return std::nullopt;
    }
    return question->saveSimulationData();
}

bool StorageOptionsDialog::saveSimulationData() const noexcept
{
    return m_all->isChecked();
}

bool StorageOptionsDialog::isOk() const noexcept
{
    return chosenOption() == m_ok;
}

void StorageOptionsDialog::updateInfoLabel()
{
    if (m_all->isChecked())
    {
        //: StorageOptChooser.lbl.info1
        m_info->setText(tr("Largest file size"));
    }
    else
    {
        //: StorageOptChooser.lbl.info3
        m_info->setText(tr("Smallest file size"));
    }
}

}  // namespace QtRocket::Gui
