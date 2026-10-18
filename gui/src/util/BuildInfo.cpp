#include "QtRocket/gui/util/BuildInfo.h"

#include <string_view>

#include <QByteArrayView>
#include <QString>

#include "QtRocket/util/Version.h"

namespace QtRocket::Gui::BuildInfo
{

namespace
{

QString fromUtf8(std::string_view text)
{
    return QString::fromUtf8(QByteArrayView(text));
}

}  // namespace

QString applicationName()
{
    return QStringLiteral("QtRocket");
}

QString organizationName()
{
    return QStringLiteral("QtRocket");
}

QString applicationVersion()
{
    return fromUtf8(version());
}

QString versionText()
{
    return fromUtf8(creatorString());
}

}  // namespace QtRocket::Gui::BuildInfo
