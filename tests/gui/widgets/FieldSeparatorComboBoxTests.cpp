// The choice of a CSV field separator (OpenRocket's FieldSeparatorComboBox), with the export
// scout's Swing probe of it (swing-probe.txt, its "SEPARATOR stored=... shown=... string=...
// char=..." lines) and the fix of OpenRocket's bug 50.

#include "QtRocket/gui/widgets/FieldSeparatorComboBox.h"

#include <string_view>

#include <QChar>
#include <QComboBox>
#include <QLineEdit>
#include <QString>
#include <QStringList>
#include <QTest>
#include <QWidget>
#include <Qt>
#include <QtContainerFwd>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "widgets/WidgetTestSupport.h"

namespace QtRocket::Gui
{

namespace
{

using FieldSeparatorComboBoxTest = Test::GuiTest;

/// The items of @p box.
[[nodiscard]] QStringList itemsOf(const QComboBox& box)
{
    QStringList items;
    for (int index = 0; index < box.count(); ++index)
    {
        items.append(box.itemText(index));
    }
    return items;
}

/// What a box shows, gives as its string and as its character after loading @p stored from the
/// export preferences (the probe's "shown", "string" and "char").
struct Loaded
{
    QString shown;
    QString string;
    QChar   character;
};

/// A box's Loaded after loadFromExportPreferences() of a store whose separator is @p stored.
[[nodiscard]] Loaded loadedFrom(std::string_view stored)
{
    InMemoryPreferences preferences;
    preferences.put("ExportFieldSeparator", stored);
    FieldSeparatorComboBox box(nullptr);
    box.loadFromExportPreferences(preferences);
    return {.shown     = box.currentText(),
            .string    = box.separatorString(),
            .character = box.separatorChar()};
}

/// A box showing @p text as the user typed it over the current text, in a window shown active
/// (the focus needs one): the keys go to the box's line edit.
class TypedBox
{
public:
    explicit TypedBox(const QString& text) : m_box(makeChild<FieldSeparatorComboBox>(&m_window))
    {
        if (!Test::showActiveWith(m_window, {m_box}))
        {
            ADD_FAILURE() << "the window did not become active";
            return;
        }
        m_box->setFocus();
        m_box->lineEdit()->selectAll();
        QTest::keyClicks(m_box->lineEdit(), text);
    }

    [[nodiscard]] FieldSeparatorComboBox& box() const noexcept { return *m_box; }

private:
    QWidget                 m_window;
    FieldSeparatorComboBox* m_box;
};

}  // namespace

TEST_F(FieldSeparatorComboBoxTest, HasJavasItemsAndStartsAtTheComma)
{
    // The probe: editable=true items=[,|;|SPACE|TAB] selected=','.
    const FieldSeparatorComboBox box(nullptr);
    EXPECT_EQ(itemsOf(box), (QStringList{QStringLiteral(","), QStringLiteral(";"),
                                         QStringLiteral("SPACE"), QStringLiteral("TAB")}));
    EXPECT_TRUE(box.isEditable());
    EXPECT_EQ(box.currentIndex(), 0);
    EXPECT_EQ(box.separatorString(), QStringLiteral(","));
    EXPECT_EQ(box.separatorChar(), QChar(u','));
    EXPECT_EQ(FieldSeparatorComboBox::spaceText(), QStringLiteral("SPACE"));
    EXPECT_EQ(FieldSeparatorComboBox::tabText(), QStringLiteral("TAB"));
    // Swing's editable box neither adds a typed text to its items nor completes it.
    EXPECT_EQ(box.insertPolicy(), QComboBox::NoInsert);
    EXPECT_EQ(box.completer(), nullptr);
}

TEST_F(FieldSeparatorComboBoxTest, LoadsTheStoredSeparatorsAsTheProbeShows)
{
    // The probe's eight stored separators, with the fix of bug 50 for "<->" and "SPACE" (Java:
    // shown and given as "<" and "S").
    const Loaded comma = loadedFrom(",");
    EXPECT_EQ(comma.shown, QStringLiteral(","));
    EXPECT_EQ(comma.string, QStringLiteral(","));
    EXPECT_EQ(comma.character, QChar(u','));
    const Loaded semicolon = loadedFrom(";");
    EXPECT_EQ(semicolon.shown, QStringLiteral(";"));
    EXPECT_EQ(semicolon.string, QStringLiteral(";"));
    EXPECT_EQ(semicolon.character, QChar(u';'));
    const Loaded space = loadedFrom(" ");
    EXPECT_EQ(space.shown, QStringLiteral("SPACE"));
    EXPECT_EQ(space.string, QStringLiteral(" "));
    EXPECT_EQ(space.character, QChar(u' '));
    const Loaded tab = loadedFrom("\t");
    EXPECT_EQ(tab.shown, QStringLiteral("TAB"));
    EXPECT_EQ(tab.string, QStringLiteral("\t"));
    EXPECT_EQ(tab.character, QChar(u'\t'));
    const Loaded several = loadedFrom("<->");
    EXPECT_EQ(several.shown, QStringLiteral("<->"));
    EXPECT_EQ(several.string, QStringLiteral("<->"));
    EXPECT_EQ(several.character, QChar(u'<'));
    const Loaded empty = loadedFrom("");
    EXPECT_EQ(empty.shown, QStringLiteral(","));
    EXPECT_EQ(empty.string, QStringLiteral(","));
    EXPECT_EQ(empty.character, QChar(u','));
    // OpenRocket's wind import stores "SPACE" (bug 34): whole, it is the SPACE item.
    const Loaded spaceText = loadedFrom("SPACE");
    EXPECT_EQ(spaceText.shown, QStringLiteral("SPACE"));
    EXPECT_EQ(spaceText.string, QStringLiteral(" "));
    EXPECT_EQ(spaceText.character, QChar(u' '));
    const Loaded other = loadedFrom("x");
    EXPECT_EQ(other.shown, QStringLiteral("x"));
    EXPECT_EQ(other.string, QStringLiteral("x"));
    EXPECT_EQ(other.character, QChar(u'x'));
}

TEST_F(FieldSeparatorComboBoxTest, LoadsAnyKeyWithItsDefault)
{
    // loadFromPreferences(key, default): the default when the key is absent; an empty stored
    // text changes nothing.
    InMemoryPreferences    preferences;
    FieldSeparatorComboBox box(nullptr);
    box.loadFromPreferences(preferences, "LookupSeparator", ";");
    EXPECT_EQ(box.currentText(), QStringLiteral(";"));
    preferences.put("LookupSeparator", "\t");
    box.loadFromPreferences(preferences, "LookupSeparator", ";");
    EXPECT_EQ(box.currentText(), QStringLiteral("TAB"));
    preferences.put("LookupSeparator", "");
    box.loadFromPreferences(preferences, "LookupSeparator", ";");
    EXPECT_EQ(box.currentText(), QStringLiteral("TAB"));
    // The export separator's default is ",".
    FieldSeparatorComboBox exportBox(nullptr);
    exportBox.setSeparatorChar(QChar(u';'));
    exportBox.loadFromExportPreferences(InMemoryPreferences());
    EXPECT_EQ(exportBox.currentText(), QStringLiteral(","));
}

TEST_F(FieldSeparatorComboBoxTest, SetsTheSeparatorCharacterAsJavaDoes)
{
    // setSeparatorChar(): the items of ',', ';', ' ' and '\t', any other character as the text.
    FieldSeparatorComboBox box(nullptr);
    box.setSeparatorChar(QChar(u';'));
    EXPECT_EQ(box.currentIndex(), 1);
    box.setSeparatorChar(QChar(u' '));
    EXPECT_EQ(box.currentIndex(), 2);
    EXPECT_EQ(box.currentText(), QStringLiteral("SPACE"));
    box.setSeparatorChar(QChar(u'\t'));
    EXPECT_EQ(box.currentIndex(), 3);
    EXPECT_EQ(box.currentText(), QStringLiteral("TAB"));
    box.setSeparatorChar(QChar(u'|'));
    EXPECT_EQ(box.currentIndex(), -1);
    EXPECT_EQ(box.currentText(), QStringLiteral("|"));
    EXPECT_EQ(box.separatorString(), QStringLiteral("|"));
    EXPECT_EQ(box.count(), 4);
    box.setSeparatorChar(QChar(u','));
    EXPECT_EQ(box.currentIndex(), 0);
    EXPECT_EQ(box.currentText(), QStringLiteral(","));
}

TEST_F(FieldSeparatorComboBoxTest, SetsTheSeparatorStringWhole)
{
    FieldSeparatorComboBox box(nullptr);
    box.setSeparatorString(QStringLiteral(" "));
    EXPECT_EQ(box.currentText(), QStringLiteral("SPACE"));
    box.setSeparatorString(QStringLiteral("\t"));
    EXPECT_EQ(box.currentText(), QStringLiteral("TAB"));
    box.setSeparatorString(QStringLiteral(";"));
    EXPECT_EQ(box.currentIndex(), 1);
    box.setSeparatorString(QStringLiteral(" | "));
    EXPECT_EQ(box.currentIndex(), -1);
    EXPECT_EQ(box.separatorString(), QStringLiteral(" | "));
    EXPECT_EQ(box.separatorChar(), QChar(u' '));
    box.setSeparatorString(QString());
    EXPECT_EQ(box.separatorString(), QString());
    // Java's getSeparatorChar() of an empty text.
    EXPECT_EQ(box.separatorChar(), QChar(u','));
    box.setSeparatorString(QStringLiteral("TAB"));
    EXPECT_EQ(box.currentIndex(), 3);
    EXPECT_EQ(box.separatorString(), QStringLiteral("\t"));
}

TEST_F(FieldSeparatorComboBoxTest, ATypedSeparatorCountsAsItIs)
{
    // getSeparatorString() of what the user typed: "SPACE" and "TAB" are the space and the tab,
    // any other text is itself (compared exactly: "space" is "space").
    EXPECT_EQ(TypedBox(QStringLiteral("SPACE")).box().separatorString(), QStringLiteral(" "));
    EXPECT_EQ(TypedBox(QStringLiteral("TAB")).box().separatorString(), QStringLiteral("\t"));
    EXPECT_EQ(TypedBox(QStringLiteral("space")).box().separatorString(), QStringLiteral("space"));
    EXPECT_EQ(TypedBox(QStringLiteral("::")).box().separatorString(), QStringLiteral("::"));
    EXPECT_EQ(TypedBox(QStringLiteral("::")).box().separatorChar(), QChar(u':'));
}

TEST_F(FieldSeparatorComboBoxTest, CompletesAndInsertsNothingTyped)
{
    // An "s" stays an "s" (a QComboBox's completer would make it "SPACE"), and Return adds no
    // item (QComboBox's default insert policy would).
    const TypedBox typed(QStringLiteral("s"));
    EXPECT_EQ(typed.box().currentText(), QStringLiteral("s"));
    EXPECT_EQ(typed.box().separatorString(), QStringLiteral("s"));
    QTest::keyClick(typed.box().lineEdit(), Qt::Key_Return);
    EXPECT_EQ(typed.box().count(), 4);
    EXPECT_EQ(typed.box().currentText(), QStringLiteral("s"));
    // Typing an item's text and Return chooses that item.
    const TypedBox item(QStringLiteral(";"));
    QTest::keyClick(item.box().lineEdit(), Qt::Key_Return);
    EXPECT_EQ(item.box().count(), 4);
    EXPECT_EQ(item.box().currentIndex(), 1);
}

}  // namespace QtRocket::Gui
