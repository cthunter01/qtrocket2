#include "QtRocket/gui/dialogs/StorageOptionsDialog.h"

#include <optional>

#include <QGroupBox>
#include <QLabel>
#include <QLayout>
#include <QMargins>
#include <QObject>
#include <QPushButton>
#include <QRadioButton>
#include <QString>
#include <QTest>
#include <QWidget>
#include <Qt>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "NextModalDialog.h"

namespace QtRocket::Gui
{

namespace
{

using StorageOptionsDialogTest = Test::GuiTest;

/// Chooses "Only summary data" when @p summary, else "All simulated data", then OK, or Cancel
/// when @p cancel, or Esc when @p escape.
struct Choose
{
    bool summary = false;
    bool cancel  = false;
    bool escape  = false;

    void operator()(QWidget& widget) const
    {
        auto* const dialog = qobject_cast<StorageOptionsDialog*>(&widget);
        ASSERT_NE(dialog, nullptr);
        (summary ? dialog->summaryButton() : dialog->allButton()).click();
        if (escape)
        {
            QTest::keyClick(dialog, Qt::Key_Escape);
            return;
        }
        (cancel ? dialog->cancelButton() : dialog->okButton())->click();
    }
};

TEST_F(StorageOptionsDialogTest, ShowsTheStorageOptionChooser)
{
    // StorageOptionChooser with the texts of messages.properties.
    const StorageOptionsDialog dialog(true);
    EXPECT_EQ(dialog.objectName(), QStringLiteral("storageOptionsDialog"));
    EXPECT_EQ(dialog.windowTitle(), QStringLiteral("Save options"));
    EXPECT_FALSE(dialog.iconLabel()->pixmap().isNull());
    EXPECT_EQ(dialog.chooser().title(), QStringLiteral("Save options"));
    EXPECT_EQ(dialog.chooser().objectName(), QStringLiteral("storageOptionChooser"));
    EXPECT_EQ(dialog.dataLabel().text(), QStringLiteral("Simulated data to store:"));
    EXPECT_EQ(dialog.allButton().text(), QStringLiteral("All simulated data"));
    EXPECT_EQ(dialog.allButton().objectName(), QStringLiteral("allButton"));
    EXPECT_EQ(dialog.allButton().toolTip(),
              QStringLiteral("<html>Store all simulated data. <br>This can result in very large "
                             "files!"));
    EXPECT_EQ(dialog.summaryButton().text(), QStringLiteral("Only summary data"));
    EXPECT_EQ(dialog.summaryButton().objectName(), QStringLiteral("noneButton"));
    EXPECT_EQ(dialog.summaryButton().toolTip(),
              QStringLiteral("<html>Store only the values shown in the summary table. <br>This "
                             "results in the smallest files."));
    EXPECT_EQ(dialog.infoLabel().toolTip(),
              QStringLiteral("An estimate on how large the resulting file would be with the "
                             "present options."));
    // BorderFactory.createEmptyBorder(0, 10, 0, 0) around the titled border.
    const QWidget* const indent = dialog.chooser().parentWidget();
    ASSERT_NE(indent, nullptr);
    ASSERT_NE(indent->layout(), nullptr);
    EXPECT_EQ(indent->layout()->contentsMargins(),
              QMargins(StorageOptionsDialog::kIndent, 0, 0, 0));
    EXPECT_EQ(StorageOptionsDialog::kIndent, 10);
    EXPECT_TRUE(dialog.okButton()->isDefault());
}

TEST_F(StorageOptionsDialogTest, TheChoiceStartsFromTheOptionsAndGivesTheEstimate)
{
    // loadOptions() and updateInfoLabel().
    const StorageOptionsDialog all(true);
    EXPECT_TRUE(all.allButton().isChecked());
    EXPECT_TRUE(all.saveSimulationData());
    EXPECT_EQ(all.infoLabel().text(), QStringLiteral("Largest file size"));

    const StorageOptionsDialog summary(false);
    EXPECT_TRUE(summary.summaryButton().isChecked());
    EXPECT_FALSE(summary.saveSimulationData());
    EXPECT_EQ(summary.infoLabel().text(), QStringLiteral("Smallest file size"));
    summary.allButton().click();
    EXPECT_TRUE(summary.saveSimulationData());
    EXPECT_EQ(summary.infoLabel().text(), QStringLiteral("Largest file size"));
}

TEST_F(StorageOptionsDialogTest, OkGivesTheChoiceAndCancelOrEscGiveNone)
{
    {
        const Gui::Test::NextModalDialog choose(
            Choose{.summary = true, .cancel = false, .escape = false});
        EXPECT_EQ(StorageOptionsDialog::ask(nullptr, true), std::optional<bool>(false));
    }
    {
        const Gui::Test::NextModalDialog choose(
            Choose{.summary = false, .cancel = false, .escape = false});
        EXPECT_EQ(StorageOptionsDialog::ask(nullptr, false), std::optional<bool>(true));
    }
    {
        const Gui::Test::NextModalDialog choose(
            Choose{.summary = false, .cancel = true, .escape = false});
        EXPECT_EQ(StorageOptionsDialog::ask(nullptr, true), std::nullopt);
    }
    const Gui::Test::NextModalDialog choose(
        Choose{.summary = false, .cancel = false, .escape = true});
    EXPECT_EQ(StorageOptionsDialog::ask(nullptr, true), std::nullopt);
}

}  // namespace

}  // namespace QtRocket::Gui
