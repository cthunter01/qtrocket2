#include "QtRocket/gui/tree/ComponentMimeData.h"

#include <utility>
#include <vector>

#include <QByteArray>
#include <QByteArrayView>
#include <QMimeData>
#include <QObject>
#include <QString>

#include "QtRocket/util/Uuid.h"

namespace QtRocket::Gui
{

namespace
{

/// The ids, one per line: the data of the format, for a program that looks at the drag.
QByteArray idLines(const std::vector<Uuid>& ids)
{
    QByteArray lines;
    for (const Uuid& id : ids)
    {
        lines.append(QByteArrayView(id.toString()));
        lines.append('\n');
    }
    return lines;
}

}  // namespace

ComponentMimeData::ComponentMimeData(const OpenRocketDocument& source, std::vector<Uuid> ids)
  : m_source(&source), m_ids(std::move(ids))
{
    setData(mimeType(), idLines(m_ids));
}

ComponentMimeData::~ComponentMimeData() = default;

QString ComponentMimeData::mimeType()
{
    return QStringLiteral("application/x-qtrocket-components");
}

const ComponentMimeData* ComponentMimeData::fromMimeData(const QMimeData* data)
{
    return qobject_cast<const ComponentMimeData*>(data);
}

}  // namespace QtRocket::Gui
