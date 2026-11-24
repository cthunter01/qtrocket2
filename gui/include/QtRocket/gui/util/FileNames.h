#pragma once

#include <filesystem>

#include <QCoreApplication>
#include <QString>

namespace QtRocket::Gui
{

/// The file names of OpenRocket's file questions (its gui/util/FileHelper.java): the extension a
/// saved file is given, the absolute path Java's File makes, and the name filters every CSV
/// question offers. One copy for every caller: the design files (main/DocumentFileOperations),
/// the fin points' CSV export (configdialog/FreeformFinSetConfig) and, from Milestone 4, the
/// simulation data's and the simulation table's CSV exports and the wind levels' and the
/// aerodynamic lookup's CSV files.
class FileNames final
{
    Q_DECLARE_TR_FUNCTIONS(FileNames)

public:
    FileNames() = delete;

    /// @p file made absolute as Java's File.getAbsolutePath() makes it: resolved against the
    /// current directory as text, nothing of its name changed (std::filesystem::absolute() on
    /// Windows drops the dots and spaces that end a name), with the platform's separators (Java's
    /// File turns '/' into '\\' on Windows: a file chosen in a file dialog, which Qt names with
    /// '/', and the same file from the command line are one recent file); @p file with the
    /// platform's separators when the current directory is unknown.
    [[nodiscard]] static std::filesystem::path absoluteFile(const std::filesystem::path& file);

    /// FileHelper.forceExtension(@p file, @p extension): @p file as it is when the text after the
    /// last dot of its name (a dot that is not its first character) is @p extension, ignoring
    /// case (Java's toLowerCase(Locale.ENGLISH): QString::toLower(), so that the Kelvin sign is a
    /// 'k'); otherwise absoluteFile(@p file) with "." and @p extension added ("x.ork.gz" ->
    /// "x.ork.gz.ork", "a.txt" -> "a.txt.csv", ".ork" -> ".ork.ork", "x." -> "x..ork"). An
    /// @p extension that starts with a dot is added as it is (and then never matches: Java
    /// compares it with the text after the dot).
    [[nodiscard]] static std::filesystem::path forceExtension(const std::filesystem::path& file,
                                                              const QString& extension);

    /// FileHelper.CSV_FILTER as a Qt name filter: "Comma Separated Files (*.csv)".
    [[nodiscard]] static QString csvFilter();

    /// The file chooser's accept-all filter (Swing's FileChooser.acceptAllFileFilterText) as a Qt
    /// name filter: "All Files (*)".
    [[nodiscard]] static QString allFilesFilter();
};

}  // namespace QtRocket::Gui
