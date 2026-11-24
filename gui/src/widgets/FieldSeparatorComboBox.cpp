#include "QtRocket/gui/widgets/FieldSeparatorComboBox.h"

#include <string>
#include <string_view>

#include <QChar>
#include <QComboBox>
#include <QString>
#include <QStringList>
#include <QWidget>

#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/preferences/PreferenceKeys.h"
#include "QtRocket/preferences/Preferences.h"

namespace QtRocket::Gui
{

namespace
{

/// Java's loadFromExportPreferences() default.
constexpr std::string_view kDefaultSeparator = ",";

}  // namespace

FieldSeparatorComboBox::FieldSeparatorComboBox(QWidget* parent) : QComboBox(parent)
{
    addItems(QStringList{QStringLiteral(","), QStringLiteral(";"), spaceText(), tabText()});
    setEditable(true);
    // An editable QComboBox would add what the user types to its items and complete "s" to
    // "SPACE"; Swing's editable JComboBox does neither.
    setInsertPolicy(QComboBox::NoInsert);
    setCompleter(nullptr);
    setCurrentIndex(0);
}

QString FieldSeparatorComboBox::spaceText()
{
    //: CsvOptionPanel.separator.space
    return tr("SPACE");
}

QString FieldSeparatorComboBox::tabText()
{
    //: CsvOptionPanel.separator.tab
    return tr("TAB");
}

QString FieldSeparatorComboBox::separatorString() const
{
    QString separator = currentText();
    if (separator == spaceText())
    {
        return QStringLiteral(" ");
    }
    if (separator == tabText())
    {
        return QStringLiteral("\t");
    }
    return separator;
}

QChar FieldSeparatorComboBox::separatorChar() const
{
    const QString separator = currentText();
    if (separator == spaceText())
    {
        return {u' '};
    }
    if (separator == tabText())
    {
        return {u'\t'};
    }
    if (!separator.isEmpty())
    {
        return separator.front();
    }
    return {u','};
}

void FieldSeparatorComboBox::setSeparatorChar(QChar separator)
{
    if (separator == QChar(u' '))
    {
        selectText(spaceText());
    }
    else if (separator == QChar(u'\t'))
    {
        selectText(tabText());
    }
    else
    {
        // ',' and ';' are items; any other character is shown as the text.
        selectText(QString(separator));
    }
}

void FieldSeparatorComboBox::setSeparatorString(const QString& separator)
{
    if (separator == QStringLiteral(" "))
    {
        setSeparatorChar(QChar(u' '));
    }
    else if (separator == QStringLiteral("\t"))
    {
        setSeparatorChar(QChar(u'\t'));
    }
    else
    {
        selectText(separator);
    }
}

void FieldSeparatorComboBox::loadFromPreferences(const Preferences& preferences,
                                                 std::string_view   key,
                                                 std::string_view   defaultValue)
{
    const std::string saved = preferences.getString(key, defaultValue);
    // Bug 50: the whole separator, where Java's setSeparatorChar(savedSeparator.charAt(0)) keeps
    // its first character. Java's " " and "\t" cases are setSeparatorString()'s.
    if (!saved.empty())
    {
        setSeparatorString(QtStrings::fromUtf8(saved));
    }
}

void FieldSeparatorComboBox::loadFromExportPreferences(const Preferences& preferences)
{
    loadFromPreferences(preferences, PreferenceKeys::kExportFieldSeparator, kDefaultSeparator);
}

void FieldSeparatorComboBox::selectText(const QString& text)
{
    const int index = findText(text);
    if (index >= 0)
    {
        setCurrentIndex(index);
        return;
    }
    // Java's setSelectedItem() of an object that is no item: the editor shows it, no item is
    // selected.
    setCurrentIndex(-1);
    setEditText(text);
}

}  // namespace QtRocket::Gui
