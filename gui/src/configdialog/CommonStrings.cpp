#include "QtRocket/gui/configdialog/CommonStrings.h"

#include <QChar>
#include <QCoreApplication>
#include <QLatin1Char>
#include <QString>

namespace QtRocket::Gui::CommonStrings
{

QString dagger()
{
    return {QChar(0x2020)};
}

QString overrideDescription()
{
    return dagger() + QLatin1Char(' ') +
           //: Configuration.lbl.override
           QCoreApplication::translate("QtRocket::Gui::CommonStrings",
                                       "This parameter can be overridden in each flight "
                                       "configuration.");
}

}  // namespace QtRocket::Gui::CommonStrings
