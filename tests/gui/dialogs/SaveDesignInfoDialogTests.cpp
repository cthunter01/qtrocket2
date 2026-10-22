#include "QtRocket/gui/dialogs/SaveDesignInfoDialog.h"

#include <cstddef>
#include <memory>
#include <optional>
#include <string>

#include <QCheckBox>
#include <QLineEdit>
#include <QObject>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QString>
#include <QStringList>
#include <QTest>
#include <QWidget>
#include <Qt>
#include <QtContainerFwd>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "NextModalDialog.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/OpenRocketDocumentFactory.h"
#include "QtRocket/gui/configdialog/RocketConfig.h"
#include "QtRocket/gui/widgets/IconToggleButton.h"
#include "QtRocket/gui/widgets/StyledLabel.h"
#include "QtRocket/rocket/Rocket.h"
#include "ScriptedUserPrompts.h"
#include "configdialog/ConfigDialogTestSupport.h"

namespace QtRocket::Gui
{

namespace
{

using SaveDesignInfoDialogTest = Test::GuiTest;
using Test::ConfigDialogServices;
using Test::NextModalDialog;
using Test::ScriptedUserPrompts;

/// @p widget, the modal dialog, as a SaveDesignInfoDialog; null, with a test failure, when it is
/// another.
[[nodiscard]] SaveDesignInfoDialog* designInfoDialog(QWidget& widget)
{
    auto* dialog = qobject_cast<SaveDesignInfoDialog*>(&widget);
    EXPECT_NE(dialog, nullptr) << "the modal dialog is no SaveDesignInfoDialog";
    return dialog;
}

/// Types "Ann" into the designer area of @p dialog, an edit still pending.
void typeDesigner(const SaveDesignInfoDialog& dialog)
{
    QTest::keyClicks(&dialog.panel().designerArea(), QStringLiteral("Ann"));
}

/// Types the designer and clicks OK.
void typeDesignerThenOk(QWidget& widget)
{
    if (const SaveDesignInfoDialog* dialog = designInfoDialog(widget))
    {
        typeDesigner(*dialog);
        dialog->okButton().click();
    }
}

/// Types the designer and clicks Cancel.
void typeDesignerThenCancel(QWidget& widget)
{
    if (const SaveDesignInfoDialog* dialog = designInfoDialog(widget))
    {
        typeDesigner(*dialog);
        dialog->cancelButton().click();
    }
}

/// Clicks Cancel (the question answered no: the dialog stays), then OK.
void cancelThenOk(QWidget& widget)
{
    if (const SaveDesignInfoDialog* dialog = designInfoDialog(widget))
    {
        dialog->cancelButton().click();
        EXPECT_TRUE(dialog->isVisible());
        dialog->okButton().click();
    }
}

/// Types the designer and presses Esc.
void typeDesignerThenEscape(QWidget& widget)
{
    if (SaveDesignInfoDialog* dialog = designInfoDialog(widget))
    {
        typeDesigner(*dialog);
        QTest::keyClick(dialog, Qt::Key_Escape);
    }
}

/// Ticks "Don't show this dialog again", then clicks OK (or Cancel).
struct TickThen
{
    bool ok = true;

    void operator()(QWidget& widget) const
    {
        if (const SaveDesignInfoDialog* dialog = designInfoDialog(widget))
        {
            dialog->dontShowAgainBox().click();
            (ok ? dialog->okButton() : dialog->cancelButton()).click();
        }
    }
};

/// The questions @p prompts was asked, each its kind and texts.
[[nodiscard]] QStringList questions(const ScriptedUserPrompts& prompts)
{
    QStringList asked;
    for (const ScriptedUserPrompts::Prompt& prompt : prompts.prompts())
    {
        asked.append(QString::fromUtf8(ScriptedUserPrompts::kindName(prompt.kind)) +
                     QStringLiteral(": ") + prompt.texts.join(QStringLiteral(" | ")));
    }
    return asked;
}

TEST_F(SaveDesignInfoDialogTest, ShowsTheRocketsRowsBelowJavasNote)
{
    // D2cGuiProbe: SaveDesignInfoPanel's note, rows and buttons.
    const ConfigDialogServices                services;
    const std::unique_ptr<OpenRocketDocument> document =
        OpenRocketDocumentFactory::createNewRocket();
    const SaveDesignInfoDialog dialog(services.services(), *document);
    EXPECT_EQ(dialog.windowTitle(), QStringLiteral("Save Design Info"));
    EXPECT_TRUE(dialog.isModal());
    EXPECT_EQ(dialog.headingLabel().text(),
              QStringLiteral("(Optional) Fill in the design information for this file"));
    EXPECT_TRUE(dialog.headingLabel().font().bold());
    EXPECT_EQ(dialog.dontShowAgainBox().text(), QStringLiteral("Don't show this dialog again"));
    EXPECT_FALSE(dialog.dontShowAgainBox().isChecked());
    EXPECT_EQ(dialog.cancelButton().text(), QStringLiteral("Cancel"));
    EXPECT_EQ(dialog.cancelButton().toolTip(),
              QStringLiteral("Discard changes and close the dialog"));
    EXPECT_EQ(dialog.okButton().text(), QStringLiteral("OK"));
    EXPECT_EQ(dialog.okButton().toolTip(), QStringLiteral("Keep changes and close the dialog"));
    // No info button, no mass label, no second Cancel and OK.
    const RocketConfig& panel = dialog.panel();
    EXPECT_FALSE(panel.okButton().isVisibleTo(&dialog));
    ASSERT_NE(panel.componentInfoButton(), nullptr);
    EXPECT_FALSE(panel.componentInfoButton()->isVisibleTo(&dialog));
    EXPECT_FALSE(panel.infoLabel().isVisibleTo(&dialog));
    EXPECT_TRUE(panel.rowsPage().isVisibleTo(&dialog));
    // Java's design name field starts empty (see the class comment): here the rocket's name.
    EXPECT_EQ(panel.nameField().text(), QStringLiteral("Rocket"));
}

TEST_F(SaveDesignInfoDialogTest, OKKeepsTheEditsAndTheSaveGoesOn)
{
    const ConfigDialogServices                services;
    const std::unique_ptr<OpenRocketDocument> document =
        OpenRocketDocumentFactory::createNewRocket();
    const std::size_t     states = document->getUndoDetail().descriptions.size();
    const NextModalDialog answer(typeDesignerThenOk);
    EXPECT_TRUE(SaveDesignInfoDialog::edit(nullptr, services.services(), *document));
    EXPECT_TRUE(answer.acted());
    EXPECT_EQ(document->getRocket().getDesigner(), "Ann");
    // No undo position: the edit joins the current step.
    EXPECT_EQ(document->getUndoDetail().descriptions.size(), states);
    EXPECT_EQ(document->getUndoDetail().nextDescription, std::nullopt);
    EXPECT_TRUE(services.services().preferences().isShowSaveRocketInfo());
}

TEST_F(SaveDesignInfoDialogTest, EscGoesOnAsOK)
{
    const ConfigDialogServices                services;
    const std::unique_ptr<OpenRocketDocument> document =
        OpenRocketDocumentFactory::createNewRocket();
    const NextModalDialog answer(typeDesignerThenEscape);
    EXPECT_TRUE(SaveDesignInfoDialog::edit(nullptr, services.services(), *document));
    EXPECT_EQ(document->getRocket().getDesigner(), "Ann");
}

TEST_F(SaveDesignInfoDialogTest, ACancelTheUserConfirmsStopsTheSave)
{
    ConfigDialogServices services;
    services.prompts().answerDiscardChanges({.discard = true, .doNotAskAgain = false});
    const std::unique_ptr<OpenRocketDocument> document =
        OpenRocketDocumentFactory::createNewRocket();
    const NextModalDialog answer(typeDesignerThenCancel);
    EXPECT_FALSE(SaveDesignInfoDialog::edit(nullptr, services.services(), *document));
    EXPECT_EQ(questions(services.prompts()),
              (QStringList{QStringLiteral("DISCARD_CHANGES: COMPONENT_CHANGES")}));
    // The pending edit was dropped.
    EXPECT_EQ(document->getRocket().getDesigner(), "");
    EXPECT_TRUE(services.services().preferences().isShowDiscardConfirmation());
}

TEST_F(SaveDesignInfoDialogTest, ACancelAnsweredNoKeepsTheDialogOpen)
{
    ConfigDialogServices services;
    services.prompts().answerDiscardChanges({.discard = false, .doNotAskAgain = true});
    const std::unique_ptr<OpenRocketDocument> document =
        OpenRocketDocumentFactory::createNewRocket();
    const NextModalDialog answer(cancelThenOk);
    EXPECT_TRUE(SaveDesignInfoDialog::edit(nullptr, services.services(), *document));
    // "Don't ask me again" turns the confirmation off whatever the answer.
    EXPECT_FALSE(services.services().preferences().isShowDiscardConfirmation());
}

TEST_F(SaveDesignInfoDialogTest, CancelAsksEvenWhenTheConfirmationIsOff)
{
    // Java's SaveDesignInfoPanel asks without looking at the preference.
    ConfigDialogServices services;
    services.services().preferences().setShowDiscardConfirmation(false);
    services.prompts().answerDiscardChanges({.discard = true, .doNotAskAgain = false});
    const std::unique_ptr<OpenRocketDocument> document =
        OpenRocketDocumentFactory::createNewRocket();
    const NextModalDialog answer(typeDesignerThenCancel);
    EXPECT_FALSE(SaveDesignInfoDialog::edit(nullptr, services.services(), *document));
    EXPECT_EQ(services.prompts().unusedAnswers(), 0U);
}

TEST_F(SaveDesignInfoDialogTest, DontShowAgainIsStoredWhenTicked)
{
    const ConfigDialogServices                services;
    const std::unique_ptr<OpenRocketDocument> document =
        OpenRocketDocumentFactory::createNewRocket();
    const NextModalDialog answer(TickThen{.ok = true});
    EXPECT_TRUE(SaveDesignInfoDialog::edit(nullptr, services.services(), *document));
    EXPECT_FALSE(services.services().preferences().isShowSaveRocketInfo());

    // Unticked again, the dialog shows next time.
    const SaveDesignInfoDialog dialog(services.services(), *document);
    dialog.dontShowAgainBox().click();
    dialog.dontShowAgainBox().click();
    EXPECT_TRUE(services.services().preferences().isShowSaveRocketInfo());
}

TEST_F(SaveDesignInfoDialogTest, DontShowAgainIsStoredAlsoWhenCancelled)
{
    ConfigDialogServices services;
    services.prompts().answerDiscardChanges({.discard = true, .doNotAskAgain = false});
    const std::unique_ptr<OpenRocketDocument> document =
        OpenRocketDocumentFactory::createNewRocket();
    const NextModalDialog answer(TickThen{.ok = false});
    EXPECT_FALSE(SaveDesignInfoDialog::edit(nullptr, services.services(), *document));
    EXPECT_FALSE(services.services().preferences().isShowSaveRocketInfo());
}

}  // namespace

}  // namespace QtRocket::Gui
