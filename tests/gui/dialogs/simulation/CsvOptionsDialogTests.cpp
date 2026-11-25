// CsvOptionsDialog: the CSV options of the simulation table's export, asked once its file is
// chosen (the user's decision U-M4-6), where OpenRocket shows its CsvOptionPanel(
// SimulationTableCSVExport.class) as the accessory of the file chooser
// (SimulationPanel.setUpSimExportCSVFileChooser(), which has no JUnit test). The panel itself, its
// texts and tooltips are CsvOptionPanelTest's; here the question around it: its texts, the values
// it starts with and answers, OK, Cancel, Esc and a parent that goes. WidgetUserPrompts asks it
// after its file dialog (WidgetFileDialogTest in main/WidgetUserPromptsTests.cpp).

#include "QtRocket/gui/dialogs/simulation/CsvOptionsDialog.h"

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include <QChar>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QObject>
#include <QPushButton>
#include <QSpinBox>
#include <QString>
#include <QTest>
#include <QWidget>
#include <Qt>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "NextModalDialog.h"
#include "QtRocket/gui/util/CsvOptionValues.h"
#include "QtRocket/gui/widgets/CsvOptionPanel.h"
#include "QtRocket/gui/widgets/FieldSeparatorComboBox.h"

namespace QtRocket::Gui
{

namespace
{

using CsvOptionsDialogTest = Gui::Test::GuiTest;

/// The title of the table export's file question (simpanel.pop.exportToCSV.save.dialog.title).
[[nodiscard]] QString tableExportTitle()
{
    return QStringLiteral("Save as CSV file");
}

/// Values other than the defaults, without comment options (the table export's).
[[nodiscard]] CsvOptionValues someValues()
{
    CsvOptionValues values;
    values.fieldSeparator      = QStringLiteral(";");
    values.decimalPlaces       = 7;
    values.exponentialNotation = false;
    values.commentCharacter    = QStringLiteral("%");
    return values;
}

/// How a test answers the question.
enum class Answer
{
    OK,
    CANCEL,
    ESCAPE,
};

/// Edits the panel of the modal CsvOptionsDialog (a tab, 2 decimal places, exponential notation,
/// "//"), then answers it with @p answer.
struct EditAndAnswer
{
    Answer answer = Answer::OK;

    void operator()(QWidget& widget) const
    {
        auto* const dialog = qobject_cast<CsvOptionsDialog*>(&widget);
        ASSERT_NE(dialog, nullptr);
        const CsvOptionPanel& panel = dialog->optionPanel();
        panel.fieldSeparatorComboBox()->setSeparatorChar(QChar(u'\t'));
        panel.decimalPlacesSpinBox()->setValue(2);
        panel.exponentialNotationCheckBox()->setChecked(true);
        panel.commentCharacterComboBox()->setEditText(QStringLiteral("//"));
        switch (answer)
        {
            case Answer::OK:
                dialog->okButton()->click();
                return;
            case Answer::CANCEL:
                dialog->cancelButton()->click();
                return;
            case Answer::ESCAPE:
                QTest::keyClick(dialog, Qt::Key_Escape);
                return;
        }
    }
};

/// Types @p text as the field separator of the modal CsvOptionsDialog, then presses Return.
struct TypeSeparatorAndReturn
{
    QString text;

    void operator()(QWidget& widget) const
    {
        auto* const dialog = qobject_cast<CsvOptionsDialog*>(&widget);
        ASSERT_NE(dialog, nullptr);
        QLineEdit* const field = dialog->optionPanel().fieldSeparatorComboBox()->lineEdit();
        ASSERT_NE(field, nullptr);
        field->selectAll();
        QTest::keyClicks(field, text);
        QTest::keyClick(field, Qt::Key_Return);
    }
};

/// Destroys @p parent, the question's parent, while the question is open.
struct DestroyTheParent
{
    std::unique_ptr<QWidget>* parent;

    void operator()(QWidget& /*dialog*/) const { parent->reset(); }
};

/// The values EditAndAnswer chooses, with @p commentOptions.
[[nodiscard]] CsvOptionValues editedValues(std::vector<bool> commentOptions)
{
    CsvOptionValues values;
    values.fieldSeparator      = QStringLiteral("\t");
    values.decimalPlaces       = 2;
    values.exponentialNotation = true;
    values.commentCharacter    = QStringLiteral("//");
    values.commentOptions      = std::move(commentOptions);
    return values;
}

}  // namespace

TEST_F(CsvOptionsDialogTest, IsTheTableExportsOptionPanelTitledAsItsFileQuestion)
{
    const CsvOptionsDialog dialog(tableExportTitle(), someValues());
    EXPECT_EQ(dialog.objectName(), QStringLiteral("csvOptionsDialog"));
    EXPECT_EQ(dialog.windowTitle(), tableExportTitle());
    // A file chooser's accessory has no icon (JOptionPane's PLAIN_MESSAGE).
    EXPECT_TRUE(dialog.iconLabel()->isHidden());
    EXPECT_TRUE(dialog.messageLines().isEmpty());

    // CsvOptionPanel(SimulationTableCSVExport.class): its groups, no comment option.
    const CsvOptionPanel& panel = dialog.optionPanel();
    EXPECT_EQ(panel.objectName(), QStringLiteral("csvOptions"));
    EXPECT_EQ(panel.parentWidget(), dialog.messageArea());
    EXPECT_EQ(panel.baseClassName(), "SimulationTableCSVExport");
    EXPECT_EQ(panel.optionCount(), 0U);
    EXPECT_EQ(panel.formatSettingsGroup()->title(), QStringLiteral("Format settings"));
    EXPECT_EQ(panel.commentsGroup()->title(), QStringLiteral("Comments"));
    EXPECT_TRUE(panel.commentsGroup()->findChildren<QCheckBox*>().isEmpty());

    // OK, the initial option, and Cancel.
    ASSERT_EQ(dialog.buttonBox()->buttons().size(), 2);
    EXPECT_EQ(dialog.buttonBox()->standardButton(dialog.okButton()), QDialogButtonBox::Ok);
    EXPECT_EQ(dialog.buttonBox()->standardButton(dialog.cancelButton()), QDialogButtonBox::Cancel);
    EXPECT_TRUE(dialog.okButton()->isDefault());
    EXPECT_FALSE(dialog.isOk());
}

TEST_F(CsvOptionsDialogTest, StartsWithTheValuesAskedAndGivesTheirCommentOptionsBack)
{
    const CsvOptionsDialog dialog(tableExportTitle(), someValues());
    const CsvOptionPanel&  panel = dialog.optionPanel();
    EXPECT_EQ(panel.fieldSeparatorComboBox()->currentText(), QStringLiteral(";"));
    EXPECT_EQ(panel.decimalPlacesSpinBox()->value(), 7);
    EXPECT_FALSE(panel.exponentialNotationCheckBox()->isChecked());
    EXPECT_EQ(panel.commentCharacterComboBox()->currentText(), QStringLiteral("%"));
    EXPECT_EQ(dialog.values(), someValues());

    // A tab shows as the item TAB and is answered as the tab itself.
    CsvOptionValues tab = someValues();
    tab.fieldSeparator  = QStringLiteral("\t");
    const CsvOptionsDialog tabbed(tableExportTitle(), tab);
    EXPECT_EQ(tabbed.optionPanel().fieldSeparatorComboBox()->currentText(), QStringLiteral("TAB"));
    EXPECT_EQ(tabbed.values(), tab);

    // Comment options no check box shows come back as they were given.
    CsvOptionValues withComments = someValues();
    withComments.commentOptions  = {true, false};
    const CsvOptionsDialog commented(tableExportTitle(), withComments);
    EXPECT_EQ(commented.optionPanel().optionCount(), 0U);
    EXPECT_EQ(commented.values(), withComments);
}

TEST_F(CsvOptionsDialogTest, OkAnswersTheValuesChosen)
{
    {
        const Gui::Test::NextModalDialog edit(EditAndAnswer{.answer = Answer::OK});
        EXPECT_EQ(CsvOptionsDialog::ask(nullptr, tableExportTitle(), someValues()),
                  std::optional(editedValues({})));
    }
    CsvOptionValues withComments = someValues();
    withComments.commentOptions  = {false};
    const Gui::Test::NextModalDialog edit(EditAndAnswer{.answer = Answer::OK});
    EXPECT_EQ(CsvOptionsDialog::ask(nullptr, tableExportTitle(), withComments),
              std::optional(editedValues({false})));
}

TEST_F(CsvOptionsDialogTest, ReturnInATypedSeparatorAnswersItWhole)
{
    // The separator box inserts nothing typed: Return reaches the dialog's default button, OK.
    // OpenRocket bug 50 (a stored "<->" read back as "<") is CsvOptionPreferences': the answer
    // is the whole text.
    const Gui::Test::NextModalDialog type(TypeSeparatorAndReturn{.text = QStringLiteral("<->")});
    const std::optional<CsvOptionValues> answer =
        CsvOptionsDialog::ask(nullptr, tableExportTitle(), someValues());
    ASSERT_TRUE(answer.has_value());
    EXPECT_EQ(answer.value_or(CsvOptionValues{}).fieldSeparator, QStringLiteral("<->"));
}

TEST_F(CsvOptionsDialogTest, CancelEscOrAParentThatGoesAnswerNone)
{
    {
        const Gui::Test::NextModalDialog cancel(EditAndAnswer{.answer = Answer::CANCEL});
        EXPECT_EQ(CsvOptionsDialog::ask(nullptr, tableExportTitle(), someValues()), std::nullopt);
    }
    {
        const Gui::Test::NextModalDialog escape(EditAndAnswer{.answer = Answer::ESCAPE});
        EXPECT_EQ(CsvOptionsDialog::ask(nullptr, tableExportTitle(), someValues()), std::nullopt);
    }
    // The question's parent (the asking window) destroyed while it is open: the dialog goes
    // with it (ModalDialog), and nothing is answered.
    auto                             parent = std::make_unique<QWidget>();
    const Gui::Test::NextModalDialog destroy(DestroyTheParent{.parent = &parent});
    EXPECT_EQ(CsvOptionsDialog::ask(parent.get(), tableExportTitle(), someValues()), std::nullopt);
    EXPECT_EQ(parent, nullptr);
}

}  // namespace QtRocket::Gui
