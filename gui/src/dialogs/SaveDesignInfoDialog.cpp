#include "QtRocket/gui/dialogs/SaveDesignInfoDialog.h"

#include <memory>

#include <QCheckBox>
#include <QDialog>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QKeySequence>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QString>
#include <QVBoxLayout>
#include <QWidget>

#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/gui/binding/ComponentSelection.h"
#include "QtRocket/gui/configdialog/RocketConfig.h"
#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/gui/util/GuiServices.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/util/UserPrompts.h"
#include "QtRocket/gui/widgets/DescriptionArea.h"
#include "QtRocket/gui/widgets/StyledLabel.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/rocket/Rocket.h"

namespace QtRocket::Gui
{

SaveDesignInfoDialog::SaveDesignInfoDialog(const GuiServices&  services,
                                           OpenRocketDocument& document, QWidget* parent)
  : QDialog(parent), m_services(services)
{
    setObjectName(QStringLiteral("SaveDesignInfoDialog"));
    //: BasicFrame.lbl.SaveRocketInfo
    setWindowTitle(tr("Save Design Info"));
    setModal(true);
    auto* layout = makeChild<QVBoxLayout>(this);

    //: SaveDesignInfoPanel.lbl.FillInInfo
    m_heading =
        makeChild<StyledLabel>(this, tr("(Optional) Fill in the design information for this file"),
                               StyledLabel::Style::BOLD);
    m_heading->setObjectName(QStringLiteral("fillInInfoLabel"));
    layout->addWidget(m_heading);

    m_panel = makeChild<RocketConfig>(this, document, services,
                                      std::make_shared<ComponentSelection>(document.getRocket()));
    layout->addWidget(m_panel, 1);
    // The panel's button row (the info button, the mass label, its Cancel and OK) and the
    // description give way to the dialog's row (Java: SaveDesignInfoPanel.addButtons()).
    m_panel->okButton().parentWidget()->hide();
    if (DescriptionArea* info = m_panel->componentInfo())
    {
        info->hide();
    }

    auto* row       = makeChild<QWidget>(this);
    auto* rowLayout = makeChild<QHBoxLayout>(row);
    rowLayout->setContentsMargins(0, 0, 0, 0);
    //: welcome.dlg.checkbox.dontShowAgain
    m_dontShowAgain = makeChild<QCheckBox>(row, tr("Don't show this dialog again"));
    m_dontShowAgain->setObjectName(QStringLiteral("dontShowAgain"));
    rowLayout->addWidget(m_dontShowAgain, 1);
    // Java: "gapleft 30lp" before Cancel.
    rowLayout->addSpacing(30);
    //: dlg.but.cancel
    m_cancel = makeChild<QPushButton>(row, tr("Cancel"));
    m_cancel->setObjectName(QStringLiteral("cancelButton"));
    //: RocketCompCfg.btn.Cancel.ttip
    m_cancel->setToolTip(tr("Discard changes and close the dialog"));
    m_cancel->setAutoDefault(false);
    rowLayout->addWidget(m_cancel);
    //: dlg.but.ok
    m_ok = makeChild<QPushButton>(row, tr("OK"));
    m_ok->setObjectName(QStringLiteral("okButton"));
    //: RocketCompCfg.btn.OK.ttip
    m_ok->setToolTip(tr("Keep changes and close the dialog"));
    m_ok->setAutoDefault(false);
    rowLayout->addWidget(m_ok);
    layout->addWidget(row);

    connect(
        m_dontShowAgain, &QCheckBox::toggled, this,
        guarded([this](bool checked) { m_services.preferences().setShowSaveRocketInfo(!checked); },
                QStringLiteral("dontShowAgain")));
    connect(m_cancel, &QPushButton::clicked, this,
            guarded([this] { cancel(); }, QStringLiteral("cancelButton")));
    connect(m_ok, &QPushButton::clicked, this,
            guarded([this] { ok(); }, QStringLiteral("okButton")));

    // The rows, then the check box, Cancel and OK.
    QWidget::setTabOrder(&m_panel->revisionArea(), m_dontShowAgain);
    QWidget::setTabOrder(m_dontShowAgain, m_cancel);
    QWidget::setTabOrder(m_cancel, m_ok);
}

SaveDesignInfoDialog::~SaveDesignInfoDialog()
{
    // Before the dialog hides: a field that loses the focus then commits nothing.
    m_panel->release();
}

bool SaveDesignInfoDialog::edit(QWidget* parent, const GuiServices& services,
                                OpenRocketDocument& document)
{
    SaveDesignInfoDialog dialog(services, document, parent);
    return dialog.exec() == QDialog::Accepted;
}

void SaveDesignInfoDialog::ok()
{
    // A button that takes no focus (macOS) leaves an edit pending.
    static_cast<void>(m_panel->commitAll());
    m_panel->release();
    accept();
}

void SaveDesignInfoDialog::cancel()
{
    // Java: the Cancel button takes the focus, whose loss commits the edit in progress, before
    // the question (a button that takes no focus, on macOS, leaves it pending).
    static_cast<void>(m_panel->commitAll());
    const UserPrompts::DiscardAnswer answer = m_services.prompts().askDiscardChanges(
        this, UserPrompts::DiscardQuestion::COMPONENT_CHANGES);
    if (answer.doNotAskAgain)
    {
        m_services.preferences().setShowDiscardConfirmation(false);
    }
    if (!answer.discard)
    {
        return;
    }
    // Nothing is undone: Java's Cancel only closes the dialog.
    m_panel->release();
    done(QDialog::Rejected);
}

void SaveDesignInfoDialog::reject()
{
    ok();
}

void SaveDesignInfoDialog::keyPressEvent(QKeyEvent* event)
{
    // Java's plain JDialog binds nothing to Esc (BasicFrame.showSaveRocketInfoDialog() does not
    // call GUIUtil.setDisposableDialogOptions()), so Esc does nothing here either.
    if (event->matches(QKeySequence::Cancel))
    {
        event->accept();
        return;
    }
    QDialog::keyPressEvent(event);
}

}  // namespace QtRocket::Gui
