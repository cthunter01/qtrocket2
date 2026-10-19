#include "QtRocket/gui/util/MenuText.h"

#include <QChar>
#include <QString>
#include <QtTypes>

namespace QtRocket::Gui::MenuText
{

QString escaped(const QString& text)
{
    QString result = text;
    result.replace(QChar(u'&'), QStringLiteral("&&"));
    return result;
}

QString withMnemonic(const QString& text, QChar mnemonic)
{
    const qsizetype upper = text.indexOf(mnemonic.toUpper());
    const qsizetype lower = text.indexOf(mnemonic.toLower());
    qsizetype       index = upper;
    if (index < 0 || (lower >= 0 && lower < index))
    {
        index = lower;
    }
    if (index < 0)
    {
        return escaped(text);
    }
    return escaped(text.left(index)) + QChar(u'&') + escaped(text.mid(index));
}

}  // namespace QtRocket::Gui::MenuText
