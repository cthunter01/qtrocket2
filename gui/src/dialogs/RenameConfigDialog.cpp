#include "QtRocket/gui/dialogs/RenameConfigDialog.h"

#include <functional>

#include <QDialog>
#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QLatin1Char>
#include <QLineEdit>
#include <QPushButton>
#include <QString>
#include <QVBoxLayout>
#include <QWidget>
#include <Qt>

#include "QtRocket/gui/configdialog/CommonStrings.h"
#include "QtRocket/gui/dialogs/ModalDialog.h"
#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/util/ThemeColors.h"
#include "QtRocket/gui/util/UserPrompts.h"
#include "QtRocket/gui/widgets/StyledLabel.h"
#include "QtRocket/util/BugError.h"

namespace QtRocket::Gui
{

RenameConfigDialog::RenameConfigDialog(const UserPrompts::RenameConfigurationRequest& request,
                                       QWidget*                                       parent)
  : QDialog(parent),
    m_saveAsDefaultAction(request.saveAsDefault),
    //: RenameConfigDialog.lbl.name
    m_nameLabel(makeChild<QLabel>(
        this, tr("Name for flight configuration:") + QLatin1Char(' ') + CommonStrings::dagger())),
    m_textbox(makeChild<QLineEdit>(this, request.name)),
    //: button.ok
    m_ok(makeChild<QPushButton>(this, tr("OK"))),
    //: RenameConfigDialog.but.saveDefault
    m_saveAsDefault(makeChild<QPushButton>(this, tr("Save as default"))),
    //: RenameConfigDialog.but.reset
    m_resetToDefault(makeChild<QPushButton>(this, tr("Reset to default"))),
    //: button.cancel
    m_cancel(makeChild<QPushButton>(this, tr("Cancel"))),
    m_info(makeChild<StyledLabel>(this, infoText(), kInfoSize))
{
    if (!m_saveAsDefaultAction)
    {
        bug("The rename dialog's \"Save as default\" needs the caller's action");
    }
    setObjectName(QStringLiteral("RenameConfigDialog"));
    //: RenameConfigDialog.title
    setWindowTitle(tr("Rename Configuration"));
    // Dialog.ModalityType.APPLICATION_MODAL.
    setWindowModality(Qt::ApplicationModal);
    auto* layout = makeChild<QVBoxLayout>(this);

    // "span, wrap rel", then the text field ("span, w 200lp, growx, wrap para").
    m_nameLabel->setObjectName(QStringLiteral("nameLabel"));
    layout->addWidget(m_nameLabel);
    m_textbox->setObjectName(QStringLiteral("textbox"));
    m_textbox->setMinimumWidth(kTextFieldWidth);
    layout->addWidget(m_textbox);
    layout->addSpacing(kParagraphGap);

    // An empty panel ("growx") pushes the four buttons to the right ("wrap para" after Cancel).
    auto* row       = makeChild<QWidget>(this);
    auto* rowLayout = makeChild<QHBoxLayout>(row);
    rowLayout->setContentsMargins(0, 0, 0, 0);
    rowLayout->addStretch(1);
    m_ok->setObjectName(QStringLiteral("okButton"));
    m_saveAsDefault->setObjectName(QStringLiteral("saveAsDefaultButton"));
    m_resetToDefault->setObjectName(QStringLiteral("resetToDefaultButton"));
    m_cancel->setObjectName(QStringLiteral("cancel"));
    for (QPushButton* const button : {m_ok, m_saveAsDefault, m_resetToDefault, m_cancel})
    {
        rowLayout->addWidget(button);
    }
    // GUIUtil.setDisposableDialogOptions(this, okButton).
    m_ok->setDefault(true);
    layout->addWidget(row);
    layout->addSpacing(kParagraphGap);

    // "spanx, growx, wrap": the information in the dim text colour.
    m_info->setObjectName(QStringLiteral("info"));
    m_info->setTextFormat(Qt::RichText);
    layout->addWidget(m_info);
    updateColors();

    connect(m_ok, &QPushButton::clicked, this, guarded([this] { ok(); }, objectName()));
    connect(m_saveAsDefault, &QPushButton::clicked, this,
            guarded([this] { saveAsDefault(); }, objectName()));
    connect(m_resetToDefault, &QPushButton::clicked, this,
            guarded([this] { resetToDefault(); }, objectName()));
    connect(m_cancel, &QPushButton::clicked, this, &QDialog::reject);
    // pack().
    adjustSize();
}

RenameConfigDialog::~RenameConfigDialog() = default;

UserPrompts::RenameConfigurationAnswer RenameConfigDialog::ask(
    QWidget* parent, const UserPrompts::RenameConfigurationRequest& request)
{
    ModalDialog<RenameConfigDialog> dialog(parent, request);
    const int                       result   = dialog.exec();
    const RenameConfigDialog* const answered = dialog.get();
    if (result != QDialog::Accepted || answered == nullptr)
    {
        return {.action = UserPrompts::RenameAction::CANCEL, .name = QString()};
    }
    return answered->answer();
}

QString RenameConfigDialog::infoText()
{
    // "<html>" + CommonStrings.dagger + " " + infoMotors + infoManufacturers + infoCases +
    // infoCombination.
    return QStringLiteral("<html>") + CommonStrings::dagger() + QLatin1Char(' ') +
           //: RenameConfigDialog.lbl.infoMotors
           tr("The text '<b>{motors}</b>' will be replaced with the <b>motor designation(s).</b> "
              "<br><pre>\te.g. '{motors} → 'M1350-0'</pre>") +
           //: RenameConfigDialog.lbl.infoManufacturers
           tr("The text '<b>{manufacturers}</b>' will be replaced with the <b>motor "
              "manufacturer(s).</b> <br><pre>\te.g. '{manufacturers}' → 'AeroTech'</pre>") +
           //: RenameConfigDialog.lbl.infoCases
           tr("The text '<b>{cases}</b>' will be replaced with the <b>motor case(s).</b> "
              "<br><pre>\te.g. '{cases}' → 'SU 18.0x70.0'</pre>") +
           //: RenameConfigDialog.lbl.infoCombination
           tr("A <b>combination</b> of the above can be used (you may choose the separator). "
              "<br><pre>\te.g. '{manufacturers motors}' → 'AeroTech M1350-0'</pre>");
}

UserPrompts::RenameConfigurationAnswer RenameConfigDialog::answer() const
{
    return {.action = m_action,
            .name = m_action == UserPrompts::RenameAction::RENAME ? m_textbox->text() : QString()};
}

void RenameConfigDialog::ok()
{
    m_action = UserPrompts::RenameAction::RENAME;
    accept();
}

void RenameConfigDialog::saveAsDefault()
{
    // The caller's action shows a message over the dialog, whose event loop may take the dialog
    // along with its parent: the action runs from a copy, and nothing of the dialog is used
    // afterwards.
    const std::function<void(QWidget*, const QString&)> action = m_saveAsDefaultAction;
    action(this, m_textbox->text());
}

void RenameConfigDialog::resetToDefault()
{
    m_action = UserPrompts::RenameAction::RESET;
    accept();
}

void RenameConfigDialog::changeEvent(QEvent* event)
{
    QDialog::changeEvent(event);
    if (event->type() == QEvent::PaletteChange)
    {
        updateColors();
    }
}

void RenameConfigDialog::updateColors()
{
    m_info->setFontColor(ThemeColors::color(ThemeColors::Key::TEXT_DIM, palette()));
}

}  // namespace QtRocket::Gui
