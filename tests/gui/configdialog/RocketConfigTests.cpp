#include "QtRocket/gui/configdialog/RocketConfig.h"

#include <memory>
#include <optional>

#include <QCoreApplication>
#include <QEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QString>
#include <QStringList>
#include <QTabWidget>
#include <QTest>
#include <QWidget>
#include <Qt>
#include <QtContainerFwd>

#include <gtest/gtest.h>

#include "AssemblyPanelTestSupport.h"
#include "ConfigDialogTestSupport.h"
#include "MassPanelTestSupport.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/gui/binding/ComponentSelection.h"
#include "QtRocket/gui/binding/EnumBinding.h"
#include "QtRocket/gui/configdialog/ComponentConfigDialog.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/util/UserPrompts.h"
#include "QtRocket/gui/widgets/DescriptionArea.h"
#include "QtRocket/gui/widgets/EnumComboBox.h"
#include "QtRocket/gui/widgets/IconToggleButton.h"
#include "QtRocket/gui/widgets/TextCommitter.h"
#include "QtRocket/rocket/DesignType.h"
#include "QtRocket/rocket/MassComponent.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/BugError.h"
#include "RingPanelTestSupport.h"
#include "ScriptedUserPrompts.h"

namespace QtRocket::Gui
{

namespace
{

using RocketConfigTest = Test::ConfigDialogTest;
using Test::comboItemTexts;
using Test::ConfigDialogServices;
using Test::FactoryPanel;
using Test::labelTextsUnder;
using Test::MassDesign;
using Test::ScriptedUserPrompts;
using Test::showActiveDialog;

/// The panel of @p created as a RocketConfig.
[[nodiscard]] RocketConfig& rocketPanel(const FactoryPanel& created)
{
    auto* panel = created.as<RocketConfig>();
    if (panel == nullptr)
    {
        bug("not the rocket's panel");
    }
    return *panel;
}

/// Chooses @p type in the design type combo box of @p panel as a user does.
void chooseDesignType(const RocketConfig& panel, DesignType type)
{
    const std::shared_ptr<EnumBinding<DesignType>> binding =
        panel.designTypeComboBox().enumBinding<DesignType>();
    Q_EMIT panel.designTypeComboBox().activated(binding->indexOf(type));
}

/// The messages @p prompts showed: each its title and its lines.
[[nodiscard]] QStringList shownMessages(const ScriptedUserPrompts& prompts)
{
    QStringList messages;
    for (const ScriptedUserPrompts::Prompt& prompt : prompts.prompts())
    {
        if (prompt.kind == ScriptedUserPrompts::Kind::MESSAGE)
        {
            messages.append(prompt.title + QStringLiteral(": ") +
                            prompt.texts.join(QStringLiteral(" | ")));
        }
    }
    return messages;
}

TEST_F(RocketConfigTest, ShowsJavasRowsInPlaceOfTheTabs)
{
    // D2cGuiProbe: RocketConfig's rows; its tabbed pane (Override, Comment) is no longer shown.
    const ConfigDialogServices services;
    const MassDesign           design;
    Rocket&                    rocket = design.document->getRocket();
    const FactoryPanel         created(*design.document, services.services(), rocket);
    const RocketConfig&        panel = rocketPanel(created);
    EXPECT_EQ(labelTextsUnder(panel.rowsPage()),
              (QStringList{QStringLiteral("Design name:"), QStringLiteral("Designer:"),
                           QStringLiteral("Design type:"), QStringLiteral("Kit name:"),
                           QStringLiteral("Comments:"), QStringLiteral("Revision history:")}));
    EXPECT_TRUE(panel.tabs().isHidden());
    EXPECT_EQ(panel.tabs().count(), 2);
    EXPECT_EQ(panel.selectedTabName(), QStringLiteral("Override"));
    EXPECT_TRUE(panel.nameField().isVisibleTo(&panel));
    EXPECT_EQ(panel.nameField().parentWidget(), &panel.rowsPage());
    EXPECT_EQ(panel.commentArea().parentWidget(), &panel.rowsPage());
    EXPECT_EQ(panel.nameField().text(), QStringLiteral("Rocket"));
    EXPECT_EQ(panel.designerArea().objectName(), QStringLiteral("Designer"));
    EXPECT_EQ(panel.kitNameArea().objectName(), QStringLiteral("KitName"));
    EXPECT_EQ(panel.revisionArea().objectName(), QStringLiteral("Revision"));
    EXPECT_EQ(panel.designTypeComboBox().objectName(), QStringLiteral("DesignType"));
    // The rocket's description and its button stay below the rows.
    ASSERT_NE(panel.componentInfoButton(), nullptr);
    EXPECT_TRUE(panel.componentInfoButton()->isVisibleTo(&panel));
    EXPECT_TRUE(panel.okButton().isVisibleTo(&panel));
}

TEST_F(RocketConfigTest, TheDesignTypesAreJavas)
{
    // D2cProbe: DesignType.getName() of the seven values; a new rocket is ORIGINAL, without the
    // kit name.
    const ConfigDialogServices services;
    const MassDesign           design;
    const FactoryPanel created(*design.document, services.services(), design.document->getRocket());
    const RocketConfig& panel = rocketPanel(created);
    EXPECT_EQ(
        comboItemTexts(panel.designTypeComboBox()),
        (QStringList{QStringLiteral("Original Design/Other"), QStringLiteral("Commercial Kit"),
                     QStringLiteral("Clone of Commercial Kit"),
                     QStringLiteral("Upscale of Commercial Kit"),
                     QStringLiteral("Downscale of Commercial Kit"),
                     QStringLiteral("Modification of a Commercial Kit"),
                     QStringLiteral("Kit Bash of Commercial Kits")}));
    EXPECT_EQ(panel.designTypeComboBox().shownText(), QStringLiteral("Original Design/Other"));
    EXPECT_FALSE(panel.isKitNameShown());
    EXPECT_TRUE(panel.kitNameLabel().isHidden());
}

TEST_F(RocketConfigTest, AKitShowsTheKitName)
{
    const ConfigDialogServices services;
    const MassDesign           design;
    Rocket&                    rocket = design.document->getRocket();
    const FactoryPanel         created(*design.document, services.services(), rocket);
    const RocketConfig&        panel = rocketPanel(created);
    chooseDesignType(panel, DesignType::COMMERCIAL_KIT);
    EXPECT_EQ(rocket.getDesignType(), DesignType::COMMERCIAL_KIT);
    EXPECT_TRUE(panel.isKitNameShown());
    EXPECT_FALSE(panel.kitNameLabel().isHidden());
    chooseDesignType(panel, DesignType::ORIGINAL);
    EXPECT_FALSE(panel.isKitNameShown());
}

TEST_F(RocketConfigTest, TheRowsShowTheRocketsTexts)
{
    const ConfigDialogServices services;
    const MassDesign           design;
    Rocket&                    rocket = design.document->getRocket();
    rocket.setDesigner("Me");
    rocket.setDesignType(DesignType::CLONE_KIT);
    rocket.setKitName("Kit");
    rocket.setRevision("First");
    rocket.setComment("A comment");
    design.document->clearUndo();
    const FactoryPanel  created(*design.document, services.services(), rocket);
    const RocketConfig& panel = rocketPanel(created);
    EXPECT_EQ(panel.designerArea().toPlainText(), QStringLiteral("Me"));
    EXPECT_EQ(panel.designTypeComboBox().shownText(), QStringLiteral("Clone of Commercial Kit"));
    EXPECT_TRUE(panel.isKitNameShown());
    EXPECT_EQ(panel.kitNameArea().toPlainText(), QStringLiteral("Kit"));
    EXPECT_EQ(panel.revisionArea().toPlainText(), QStringLiteral("First"));
    EXPECT_EQ(panel.commentArea().toPlainText(), QStringLiteral("A comment"));
}

TEST_F(RocketConfigTest, OneCommitCommitsTheDesignerKitNameAndRevision)
{
    // Java's TextFieldListener of RocketConfig sets all three on the focus loss of any of them.
    const ConfigDialogServices services;
    const MassDesign           design;
    Rocket&                    rocket = design.document->getRocket();
    rocket.setDesignType(DesignType::COMMERCIAL_KIT);
    design.document->clearUndo();
    const FactoryPanel  created(*design.document, services.services(), rocket);
    const RocketConfig& panel = rocketPanel(created);
    QTest::keyClicks(&panel.designerArea(), QStringLiteral("Ann"));
    QTest::keyClicks(&panel.kitNameArea(), QStringLiteral("Alpha"));
    QTest::keyClicks(&panel.revisionArea(), QStringLiteral("v2"));
    EXPECT_EQ(rocket.getDesigner(), "");
    EXPECT_TRUE(panel.designTextCommitter().commit());
    EXPECT_EQ(rocket.getDesigner(), "Ann");
    EXPECT_EQ(rocket.getKitName(), "Alpha");
    EXPECT_EQ(rocket.getRevision(), "v2");
}

TEST_F(RocketConfigTest, TheNameAndTheCommentKeepTheBasesCommitter)
{
    const ConfigDialogServices services;
    const MassDesign           design;
    Rocket&                    rocket = design.document->getRocket();
    const FactoryPanel         created(*design.document, services.services(), rocket);
    const RocketConfig&        panel = rocketPanel(created);
    panel.nameField().selectAll();
    QTest::keyClicks(&panel.nameField(), QStringLiteral("Alpha III"));
    QTest::keyClicks(&panel.commentArea(), QStringLiteral("Classic"));
    EXPECT_TRUE(panel.textCommitter().commit());
    EXPECT_EQ(rocket.getName(), "Alpha III");
    EXPECT_EQ(rocket.getComment(), "Classic");
    EXPECT_FALSE(panel.designTextCommitter().hasField(panel.nameField()));
}

/// The easter egg's message box for the name @p name as one text: its severity, its title and
/// its lines; empty for none.
[[nodiscard]] QString easterEggText(const QString& name)
{
    const std::optional<UserPrompts::Message> message = RocketConfig::easterEggMessage(name);
    if (!message.has_value())
    {
        return {};
    }
    const UserPrompts::Message& shown = *message;
    return (shown.severity == UserPrompts::Severity::INFORMATION ? QStringLiteral("i ")
                                                                 : QStringLiteral("? ")) +
           shown.title + QStringLiteral(": ") + shown.lines.join(QStringLiteral(" | "));
}

TEST_F(RocketConfigTest, TheEasterEggsAreJavas)
{
    EXPECT_EQ(easterEggText(QStringLiteral("SA-508")),
              QStringLiteral("i Oh oh...: Houston, we have a problem. |  | Just kidding, have fun "
                             "building your 'Apollo 13' rocket!"));
    EXPECT_EQ(easterEggText(QStringLiteral("SA-506")),
              QStringLiteral("i Or was that not the quote?: One small step for a rocket, one "
                             "giant leap for rocketkind."));
    EXPECT_EQ(easterEggText(QStringLiteral("Vega")),
              QStringLiteral("i Vega, Ready for Launch and Laughs!: Viva las Vega!"));
    EXPECT_EQ(easterEggText(QStringLiteral("Ariane 5")),
              QStringLiteral("i Happens to the best of us: Non, je ne regrette rien\u2026 except "
                             "for that one overflow error\u2026"));
    EXPECT_EQ(easterEggText(QStringLiteral("Vega ")), QString());
    EXPECT_EQ(easterEggText(QStringLiteral("vega")), QString());
}

TEST_F(RocketConfigTest, TypingAnEasterEggShowsItsMessage)
{
    const ConfigDialogServices services;
    const MassDesign           design;
    const FactoryPanel created(*design.document, services.services(), design.document->getRocket());
    const RocketConfig& panel = rocketPanel(created);
    panel.nameField().clear();
    // Typed into "Veg" the "a" makes "Vega" (Java: the text and the typed character).
    QTest::keyClicks(&panel.nameField(), QStringLiteral("Veg"));
    EXPECT_EQ(shownMessages(services.prompts()), QStringList());
    QTest::keyClicks(&panel.nameField(), QStringLiteral("a"));
    QCoreApplication::processEvents();
    EXPECT_EQ(shownMessages(services.prompts()),
              (QStringList{QStringLiteral(
                  "Vega, Ready for Launch and Laughs!: INFORMATION | Viva las Vega!")}));
    // A text that becomes "Vega" otherwise (a deleted character) shows nothing.
    QTest::keyClicks(&panel.nameField(), QStringLiteral("s"));
    QTest::keyClick(&panel.nameField(), Qt::Key_Backspace);
    QCoreApplication::processEvents();
    EXPECT_EQ(panel.nameField().text(), QStringLiteral("Vega"));
    EXPECT_EQ(shownMessages(services.prompts()).size(), 1);
}

TEST_F(RocketConfigTest, TheDialogOfTheRocketShowsTheRows)
{
    const ConfigDialogServices   services;
    const MassDesign             design;
    QWidget                      window;
    Rocket&                      rocket = design.document->getRocket();
    const ComponentConfigDialog* dialog =
        showActiveDialog(window, services.services(), *design.document, rocket);
    ASSERT_NE(dialog, nullptr);
    EXPECT_EQ(dialog->windowTitle(), QStringLiteral("Rocket configuration"));
    const auto* panel = dynamic_cast<const RocketConfig*>(&dialog->configurator());
    ASSERT_NE(panel, nullptr);
    EXPECT_TRUE(panel->rowsPage().isVisible());
    EXPECT_FALSE(panel->tabs().isVisible());
    EXPECT_FALSE(panel->kitNameArea().isVisible());
    chooseDesignType(*panel, DesignType::KIT_BASH);
    EXPECT_TRUE(panel->kitNameArea().isVisible());
    // A change of the rocket's name fires no TREE change: the dialog stays.
    rocket.setName("Renamed");
    EXPECT_EQ(ComponentConfigDialog::dialog(), dialog);
    EXPECT_TRUE(dialog->isModified());
    ComponentConfigDialog::disposeDialog();
}

TEST_F(RocketConfigTest, CancelUndoesTheNameAndTheDesignInformation)
{
    // Cancel undoes the session (Rocket::loadFrom()), which restores the components' names and
    // comments and the rocket's designer, revision, design type and kit name: the user's
    // decision R3-2, a U4 addition (Java's Rocket.loadFrom() copies none of those four, so
    // there the design type stays UPSCALE_KIT).
    const ConfigDialogServices services;
    const MassDesign           design;
    QWidget                    window;
    services.services().preferences().setShowDiscardConfirmation(false);
    const ComponentConfigDialog* dialog = showActiveDialog(
        window, services.services(), *design.document, design.document->getRocket());
    ASSERT_NE(dialog, nullptr);
    const auto* panel = dynamic_cast<const RocketConfig*>(&dialog->configurator());
    ASSERT_NE(panel, nullptr);
    panel->nameField().selectAll();
    QTest::keyClicks(&panel->nameField(), QStringLiteral("Alpha"));
    QTest::keyClick(&panel->nameField(), Qt::Key_Return);
    chooseDesignType(*panel, DesignType::UPSCALE_KIT);
    QTest::keyClicks(&panel->designerArea(), QStringLiteral("Ann"));
    EXPECT_EQ(design.document->getRocket().getName(), "Alpha");
    dialog->configurator().cancelButton().click();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    EXPECT_EQ(design.document->getRocket().getName(), "Rocket");
    EXPECT_EQ(design.document->getRocket().getDesignType(), DesignType::ORIGINAL);
    // The pending designer was dropped.
    EXPECT_EQ(design.document->getRocket().getDesigner(), "");
}

TEST_F(RocketConfigTest, AnUndoWhileOpenClosesTheDialogAndDropsThePendingDesigner)
{
    // Proved under the asan preset. The rocket outlives the undo (Rocket::loadFrom() keeps it,
    // restoring its designer), so a pending designer that the focus loss of the hidden dialog
    // committed after the undo would stay: the dialog's release drops it first.
    const ConfigDialogServices services;
    const MassDesign           design;
    QWidget                    window;
    design.document->addUndoPosition("Change");
    design.mass->setName("Payload");
    const ComponentConfigDialog* dialog = showActiveDialog(
        window, services.services(), *design.document, design.document->getRocket());
    ASSERT_NE(dialog, nullptr);
    const auto* panel = dynamic_cast<const RocketConfig*>(&dialog->configurator());
    ASSERT_NE(panel, nullptr);
    panel->designerArea().setFocus();
    QTest::keyClicks(&panel->designerArea(), QStringLiteral("Ann"));
    EXPECT_TRUE(panel->designerArea().hasFocus());

    design.document->undo();
    EXPECT_EQ(ComponentConfigDialog::dialog(), nullptr);
    EXPECT_TRUE(dialog->isDisposed());
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    EXPECT_EQ(design.document->getRocket().getDesigner(), "");
    const RocketComponent& body =
        *design.document->getRocket().getChildren().front()->getChildren().front();
    EXPECT_EQ(body.getChildren().at(0)->getName(), "Mass Component");
}

TEST_F(RocketConfigTest, ATreeChangeEndsTheDesignTypeQuietly)
{
    // The TREE change ends the selection: the design type's binding tells its end, and the kit
    // name keeps its row without reading the rocket.
    const ConfigDialogServices services;
    const MassDesign           design;
    const FactoryPanel created(*design.document, services.services(), design.document->getRocket());
    const RocketConfig& panel = rocketPanel(created);
    design.body->addChild(std::make_unique<MassComponent>());
    EXPECT_FALSE(panel.selection()->isValid());
    EXPECT_FALSE(panel.isKitNameShown());
}

TEST_F(RocketConfigTest, AComponentThatIsNoRocketIsABug)
{
    const ConfigDialogServices services;
    const MassDesign           design;
    QWidget                    host;
    EXPECT_THROW(static_cast<void>(
                     makeChild<RocketConfig>(&host, *design.document, services.services(),
                                             std::make_shared<ComponentSelection>(*design.body))),
                 BugError);
}

}  // namespace

}  // namespace QtRocket::Gui
