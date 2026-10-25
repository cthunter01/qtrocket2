#pragma once

#include <cstddef>
#include <filesystem>
#include <span>
#include <string_view>
#include <vector>

#include "QtRocket/file/DirectoryIterator.h"
#include "QtRocket/file/preset/OpenRocketComponentLoader.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/rocket/preset/ComponentPresetDatabase.h"
#include "QtRocket/util/Error.h"

namespace QtRocket
{

class MaterialStorage;

/// Fills a ComponentPresetDatabase with the bundled component presets and the user's preset files:
/// the Qt-free core of OpenRocket's ComponentPresetDatabaseLoader (loadDatabase() =
/// loadPresetComponents(), then loadUserComponents()). Each .orc file is read with an
/// OpenRocketComponentLoader and its presets go into the database with addAll() (Database.add():
/// sorted by manufacturer and part number, a preset whose digest the database holds dropped), file
/// after file in the order they are met: the bundled directory first, then the user's files in
/// the order given. The bundled set (data/components, share/QtRocket/components when installed:
/// kBundledDirectory) is 31 files: 5231 presets read, 5228 kept.
///
/// Which files: a file whose name ends with ".orc", ignoring ASCII case, as OpenRocket's
/// SimpleFileFilter("", false, "orc") takes it (isPresetFile()). A directory, the bundled one or
/// one of the user's list, is searched by DirectoryIterator: depth first, the entries of each
/// directory in the order of their UTF-8 names, names that start with '.' skipped, a directory
/// reached twice through links searched once. A user's file is read when it is a regular file
/// with that extension, whatever its name starts with, as in OpenRocket.
///
/// What goes wrong is recorded, never thrown (getProblems()), and the load goes on with the next
/// file: a file that cannot be read or that OpenRocketComponentLoader refuses, a path of the
/// user's list that is no .orc file or no directory, a directory that cannot be listed, a bound
/// of a walk, a bundled directory without preset files. The warnings of the files that were read
/// (the components OpenRocketComponentLoader skipped) are gathered in getWarnings().
///
/// Deviations from OpenRocket:
/// - It loads in the caller's thread; OpenRocket's AsynchronousDatabaseLoader loads on a thread
///   of its own, which getDatabase() waits for. Nothing here shows a dialog: OpenRocket shows one
///   for each user file that fails ("Error upon component preset import", ComponentDbLoaderDlg),
///   and logs the other problems. Problem::origin and Problem::refusedFile say which those are.
/// - Bug 18 of Milestone 3 (the user's decision U-M3-5): a file that OpenRocket's DTOs throw a
///   RuntimeException on is refused alone, and the files after it, the user's included, are
///   read. In OpenRocket the exception escapes the loader (which catches IOException only) and
///   ends the whole load, so that no later file is read. (OpenRocketComponentLoader says which
///   files these are.)
/// - The order of a directory's files (DirectoryIterator): Java's File.listFiles() order is
///   unspecified, and its bundled files come in the order of the entries of its jar.
/// - Bounds (decision D7 of the scouting, the policy of the loaders of Milestone 1): a directory
///   walk takes at most kMaxDirectoryFiles files and enters directories at most
///   kMaxDirectoryDepth deep, each bound reached a problem; a file is read within
///   OpenRocketComponentLoader's limits; at most kMaxWarnings warnings are kept, then one that
///   says the others were dropped. OpenRocket has none of these bounds.
/// - A user's path that is a regular file without the extension is a problem of its own
///   (ErrorCode::UNSUPPORTED_FORMAT); OpenRocket logs it as "... is neither file nor directory".
///
/// The materials of the presets are looked up in the MaterialStorage the loader is made with,
/// which is only read (OpenRocketComponentLoader): it must outlive the loader, and the loader runs
/// on the thread that owns it.
class ComponentPresetDatabaseLoader final
{
public:
    /// The directory of the bundled presets in QtRocket's data directory (share/QtRocket when
    /// installed, the build tree's copy of data/; OpenRocket's datafiles/components).
    static constexpr std::string_view kBundledDirectory = "components";
    /// The extension of the preset files, compared ignoring ASCII case.
    static constexpr std::string_view kExtension = "orc";
    /// The most files one directory walk takes (D7), some three hundred times the bundled
    /// directory's 31.
    static constexpr std::size_t kMaxDirectoryFiles = 10000;
    /// The deepest directory a walk enters, counted from the one it starts from (D7); the
    /// bundled files are one directory deep.
    static constexpr std::size_t kMaxDirectoryDepth = 100;
    /// The most warnings kept (DocumentConfig::kMaxWarnings of the .ork loader). The bundled
    /// files give none.
    static constexpr std::size_t kMaxWarnings = 1000;

    /// Where a problem was met.
    enum class Origin
    {
        /// The bundled directory (loadPresetComponents()). OpenRocket logs these.
        BUNDLED,
        /// The user's files and directories (loadUserComponents()).
        USER,
    };

    /// A file or directory that could not be loaded, and why.
    struct Problem
    {
        /// The file or directory, as the walk or the user's list names it (Java's
        /// File.getPath()).
        std::filesystem::path file;
        Error                 error;
        Origin                origin{Origin::USER};
        /// Whether the problem is an .orc file that was found and could not be read or was
        /// refused (OpenRocketComponentLoader's Error). For such a user file OpenRocket shows
        /// its dialog: the error message in italics, then "Component preset '<b>'{0}'</b>' will
        /// be ignored during import." with this file (ComponentDbLoaderDlg.message1, .message2).
        /// The other problems OpenRocket logs, or does not know (the bounds).
        bool refusedFile{false};
    };

    /// The bounds of a load (see the class comment). An application keeps the defaults; the
    /// tests of the bounds lower them.
    struct Limits
    {
        /// The bounds of each file.
        OpenRocketComponentLoader::Limits file{};
        /// The bounds of each directory walk, the bundled one's included.
        DirectoryIterator::Limits directory{.files = kMaxDirectoryFiles,
                                            .depth = kMaxDirectoryDepth};
        /// The most warnings getWarnings() keeps.
        std::size_t warnings{kMaxWarnings};
    };

    /// A loader that looks the presets' materials up in @p materials, with the default limits.
    explicit ComponentPresetDatabaseLoader(const MaterialStorage& materials);
    /// A loader with the limits @p limits.
    ComponentPresetDatabaseLoader(const MaterialStorage& materials, const Limits& limits);

    /// loadDatabase(): loadPresetComponents(@p bundledDirectory), then
    /// loadUserComponents(@p userFiles).
    void loadDatabase(const std::filesystem::path&           bundledDirectory,
                      std::span<const std::filesystem::path> userFiles);

    /// Loads the bundled presets (loadPresetComponents()): every preset file under
    /// @p bundledDirectory, found as the class comment says. A file that fails is a problem
    /// (OpenRocket logs it) and the load goes on; a directory without preset files is one
    /// (ErrorCode::NOT_FOUND, "No component preset file found in <directory>"), after the
    /// problem of a directory that cannot be listed.
    void loadPresetComponents(const std::filesystem::path& bundledDirectory);

    /// Loads the user's preset files (loadUserComponents(), with the files OpenRocket's
    /// preferences list, getUserComponentPresetFiles()), in their order: a regular file with the
    /// extension is read, a directory is searched; any other path is a problem: a regular file
    /// without the extension (ErrorCode::UNSUPPORTED_FORMAT, "User-defined component preset file
    /// <path> does not have the extension .orc") or a path that is neither (ErrorCode::NOT_FOUND,
    /// Java's "User-defined component preset file <path> is neither file nor directory").
    void loadUserComponents(std::span<const std::filesystem::path> userFiles);

    /// The database loaded so far (getDatabase()).
    [[nodiscard]] const ComponentPresetDatabase& getDatabase() const noexcept { return m_database; }

    /// Moves the database out, leaving an empty one.
    [[nodiscard]] ComponentPresetDatabase takeDatabase();

    /// The number of files read (fileCount): the refused ones are not counted.
    [[nodiscard]] std::size_t getFileCount() const noexcept { return m_fileCount; }

    /// The number of presets the files gave (presetCount), those the database dropped as
    /// duplicates included: 5231 for the bundled files, whose database holds 5228.
    [[nodiscard]] std::size_t getPresetCount() const noexcept { return m_presetCount; }

    /// The problems, in the order they were met.
    [[nodiscard]] const std::vector<Problem>& getProblems() const noexcept { return m_problems; }

    /// The warnings of the files that were read, at most Limits::warnings of them and then the
    /// one that says the others were dropped.
    [[nodiscard]] const WarningSet& getWarnings() const noexcept { return m_warnings; }

    /// SimpleFileFilter("", false, "orc").accept() for a file: whether the name of @p file ends
    /// with ".orc", ignoring ASCII case ("BMS.ORC" is one).
    [[nodiscard]] static bool isPresetFile(const std::filesystem::path& file);

private:
    /// Reads @p file and adds its presets (loadFile() and addAll()), or records the problem.
    void loadFile(const std::filesystem::path& file, Origin origin);

    /// Every preset file under @p directory (DirectoryIterator), whose problems are recorded.
    [[nodiscard]] std::vector<std::filesystem::path> findFiles(
        const std::filesystem::path& directory, Origin origin);

    void addProblem(const std::filesystem::path& file, Error error, Origin origin,
                    bool refusedFile);

    OpenRocketComponentLoader m_loader;
    Limits                    m_limits;
    ComponentPresetDatabase   m_database;
    std::size_t               m_fileCount{0};
    std::size_t               m_presetCount{0};
    std::vector<Problem>      m_problems;
    WarningSet                m_warnings;
};

}  // namespace QtRocket
