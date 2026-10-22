#include "QtRocket/gui/dialogs/DecalNotFoundDialog.h"

#include <QAbstractButton>
#include <QDialogButtonBox>
#include <QLabel>
#include <QObject>
#include <QPushButton>
#include <QString>
#include <QTest>
#include <QWidget>
#include <Qt>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "NextModalDialog.h"
#include "QtRocket/document/Attachment.h"
#include "QtRocket/gui/util/QtStrings.h"

namespace QtRocket::Gui
{

namespace
{

using DecalNotFoundDialogTest = Test::GuiTest;

/// The message of a save whose decal file /d/decal.png is missing (the Java probe
/// FileOperationsProbe: DECAL-MSG; the text of decalNotFound()).
[[nodiscard]] QString decalMessage()
{
    return QtStrings::fromUtf8(QtRocket::decalNotFound("/d/decal.png").error().message);
}

/// Answers the question with the button @p button (Yes or No), or Esc for NoButton.
struct Answer
{
    QDialogButtonBox::StandardButton button;

    void operator()(QWidget& widget) const
    {
        auto* const dialog = qobject_cast<DecalNotFoundDialog*>(&widget);
        ASSERT_NE(dialog, nullptr);
        if (button == QDialogButtonBox::NoButton)
        {
            QTest::keyClick(dialog, Qt::Key_Escape);
            return;
        }
        dialog->buttonBox()->button(button)->click();
    }
};

TEST_F(DecalNotFoundDialogTest, AsksWhetherToLookForTheFile)
{
    EXPECT_EQ(decalMessage(), QStringLiteral("Could not find decal source file '/d/decal.png'. "
                                             "<br> <br>Would you like to look for this file?"));
    const DecalNotFoundDialog dialog(decalMessage());
    EXPECT_EQ(dialog.objectName(), QStringLiteral("decalNotFoundDialog"));
    EXPECT_EQ(dialog.windowTitle(), QStringLiteral("No decal source file"));
    ASSERT_EQ(dialog.messageLabels().size(), 1);
    const QLabel& message = *dialog.messageLabels().front();
    EXPECT_EQ(message.text(), decalMessage());
    EXPECT_EQ(message.textFormat(), Qt::RichText);
    EXPECT_TRUE(message.wordWrap());
    // MessageWidthUtil.setMessageWidth(message, 400).
    EXPECT_EQ(message.minimumWidth(), DecalNotFoundDialog::kMessageWidth);
    EXPECT_EQ(message.maximumWidth(), DecalNotFoundDialog::kMessageWidth);
    EXPECT_EQ(DecalNotFoundDialog::kMessageWidth, 400);
    EXPECT_FALSE(dialog.iconLabel()->pixmap().isNull());
    EXPECT_EQ(dialog.buttonBox()->buttons().size(), 2);
    EXPECT_TRUE(dialog.yesButton()->isDefault());
    EXPECT_FALSE(dialog.isYes());
}

TEST_F(DecalNotFoundDialogTest, YesLooksForTheFileAndNoOrEscDoNot)
{
    {
        const Gui::Test::NextModalDialog answer(Answer{.button = QDialogButtonBox::Yes});
        EXPECT_TRUE(DecalNotFoundDialog::askToLookForFile(nullptr, decalMessage()));
    }
    {
        const Gui::Test::NextModalDialog answer(Answer{.button = QDialogButtonBox::No});
        EXPECT_FALSE(DecalNotFoundDialog::askToLookForFile(nullptr, decalMessage()));
    }
    const Gui::Test::NextModalDialog answer(Answer{.button = QDialogButtonBox::NoButton});
    EXPECT_FALSE(DecalNotFoundDialog::askToLookForFile(nullptr, decalMessage()));
}

}  // namespace

}  // namespace QtRocket::Gui
