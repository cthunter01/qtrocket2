#include "QtRocket/gui/app/MotorDatabaseMetadataIO.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <vector>

#include <QByteArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QString>
#include <QtTypes>

#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"

namespace QtRocket::Gui::MotorDatabaseMetadataIO
{

std::optional<std::int64_t> readDatabaseVersion(const std::filesystem::path& file)
{
    const Result<std::vector<std::byte>> bytes = readFile(file, kMaxFileSize);
    if (!bytes)
    {
        return std::nullopt;
    }
    QByteArray text;
    text.reserve(static_cast<qsizetype>(bytes->size()));
    for (const std::byte byte : *bytes)
    {
        text.append(static_cast<char>(std::to_integer<unsigned char>(byte)));
    }
    QJsonParseError     error{};
    const QJsonDocument document = QJsonDocument::fromJson(text, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject())
    {
        return std::nullopt;
    }
    // MotorDatabaseMetadata.KEY_DATABASE_VERSION.
    const QJsonValue version = document.object().value(QStringLiteral("database_version"));
    if (!version.isDouble())
    {
        return std::nullopt;
    }
    // toInteger() gives the default for a number without a 64-bit integer value.
    constexpr qint64 kNoInteger = 0;
    const qint64     value      = version.toInteger(kNoInteger);
    if (value == kNoInteger && version.toDouble() != 0.0)
    {
        return std::nullopt;
    }
    return static_cast<std::int64_t>(value);
}

}  // namespace QtRocket::Gui::MotorDatabaseMetadataIO
