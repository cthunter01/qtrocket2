#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>

/// The metadata of the bundled motor database (OpenRocket's MotorDatabaseMetadataIO and
/// MotorDatabaseMetadata): the JSON object of motors/metadata.json (DataPaths::motorMetadata(),
/// OpenRocket's datafiles/thrustcurves/metadata.json byte for byte), of which only
/// "database_version" is read, the version the motor chooser shows ("Motor DB version:",
/// ThrustCurveMotorSelectionPanel). Its "sha256" is not the hash of the bundled database: it
/// belongs to the network updater's download, which is not ported.
///
/// Deviations from OpenRocket: OpenRocket reads the copy of the file its MotorDatabaseInitializer
/// writes into the user's motor library directory (<user application directory>/libraries/motors,
/// for the updater); QtRocket reads the bundled file, which holds the same version. The file is
/// read with Qt's JSON parser (QJsonDocument), strict where Gson is lenient, and the version is a
/// JSON number with a 64-bit integer value: Gson's getAsLong() also reads a string of digits and
/// cuts a fraction off. A file that is not JSON, whose root is not an object or whose version is of
/// another kind gives no version, as one that is missing or holds no version does; OpenRocket's
/// panel catches only the IOException of a missing version, so that Gson's JsonSyntaxException,
/// IllegalStateException or NumberFormatException ends the motor chooser's construction. A file
/// larger than kMaxFileSize is not read (a bound of QtRocket's, as for every file it reads).
namespace QtRocket::Gui::MotorDatabaseMetadataIO
{

/// The most bytes of a metadata file that are read: some two hundred times the bundled one's 363.
inline constexpr std::size_t kMaxFileSize = 65536;

/// readDatabaseVersion(): the "database_version" of the metadata file @p file, or nullopt when it
/// gives none (see the namespace comment). The caller shows a version that is not above 0 as
/// unknown, as OpenRocket does.
[[nodiscard]] std::optional<std::int64_t> readDatabaseVersion(const std::filesystem::path& file);

}  // namespace QtRocket::Gui::MotorDatabaseMetadataIO
