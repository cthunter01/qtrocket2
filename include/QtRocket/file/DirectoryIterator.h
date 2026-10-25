#pragma once

#include <filesystem>
#include <functional>
#include <vector>

#include "QtRocket/util/Error.h"

namespace QtRocket
{

/// The files under a directory as OpenRocket's DirectoryIterator finds them, the way its database
/// loaders use it (new DirectoryIterator(directory, filter, true), whose DirSelectionFileFilter
/// wraps the filter): a recursive walk that skips every name starting with '.', file or
/// directory, enters every other directory and keeps the files the filter accepts. The bundled
/// and the user's thrust curves (MotorDatabaseLoader) and component presets
/// (ComponentPresetDatabaseLoader) are found this way, and so the order of the files is the order
/// in which they are loaded.
///
/// Deviations, so that the order is the same on every machine and a walk always ends:
/// - Java's File.listFiles() order is unspecified. Here the entries of each directory are taken
///   in the order of their UTF-8 names compared byte by byte ("BMS.ORC" before "apogee.orc"),
///   depth first: a directory's files and subdirectories in that one order, each subdirectory
///   walked where its name falls.
/// - A directory reached a second time, through a symbolic link (or a junction) to one walked
///   before, is skipped: Java follows such a loop until its paths grow too long.
/// The walk follows links as Java's File.isDirectory() does: a link to a directory is walked as
/// a directory, a link to a file is a file.
class DirectoryIterator final
{
public:
    /// The test for a file (not a directory): true to keep it. OpenRocket's SimpleFileFilter
    /// accepts the names with one of its extensions.
    using FileFilter = std::function<bool(const std::filesystem::path&)>;

    /// A directory whose entries could not be listed, and why. Java throws an IOException for
    /// the directory a walk starts from and logs a warning ("Error opening file/directory ...")
    /// for one below it; here both are recorded, and the walk goes on with the other entries.
    struct Problem
    {
        std::filesystem::path directory;
        /// ErrorCode::IO and Java's message: "not a directory or IOException occurred when
        /// listing files from <directory>".
        Error error;
    };

    /// What listFiles() found.
    struct Listing
    {
        /// The files accepted, in the order of the walk: their paths as @p directory and the
        /// names below it make them (not made canonical, so a file reached through a link has
        /// the link's path).
        std::vector<std::filesystem::path> files;
        /// The directories that could not be listed, in the order they were met.
        std::vector<Problem> problems;
    };

    DirectoryIterator() = delete;

    /// Every file under @p directory that @p accept keeps, found as the class documentation
    /// says. A @p directory that does not exist, is not a directory or cannot be listed gives no
    /// files and one problem; so does each directory below it that cannot be listed.
    [[nodiscard]] static Listing listFiles(const std::filesystem::path& directory,
                                           const FileFilter&            accept);
};

}  // namespace QtRocket
