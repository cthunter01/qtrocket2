#pragma once

#include <array>
#include <filesystem>
#include <optional>
#include <string_view>
#include <vector>

#include <QString>

namespace QtRocket::Gui
{

/// The example designs that File > Open example offers (ExampleDesignFile, and the order of
/// ExampleDesignFileAction's menu): the design files of a directory (the data directory's
/// examples/, DataPaths::examplesDirectory(), which holds OpenRocket's 16 examples), each named
/// after its file without the extension.
///
/// - The files (ExampleDesignFile.getExampleDesigns()): the regular files of the directory whose
///   name matches ".*\.[oO][rR][kK]$" (isDesignFileName()), named by their file name without its
///   last four characters (designName()), sorted by name as Java's String.compareTo() sorts (the
///   UTF-16 code units; two files of the same name, which only a file system that tells case can
///   hold, by their file names). A directory that is missing or cannot be read has none.
/// - The menu (menuEntries(), ExampleDesignFileAction.updateMenu()): the names of kMenuOrder in
///   its order, its separators included (a separator stands for each of them, also where the
///   designs around it are missing), then, after a separator, the designs it does not name, in
///   the sorted order.
///
/// Deviations: a name of kMenuOrder that no design has exactly takes the first design of that
/// name ignoring case (decision U4: "3D Printable Nose Cone and Fins" of the order list is the
/// file "3D printable nose cone and fins.ork", which OpenRocket's exact match puts last, after a
/// separator of its own); the files are the regular ones (OpenRocket reads its examples from its
/// jar, which lists no directory, but from a directory, as in a development build, it lists a
/// directory named "x.ork" too).
class ExampleDesigns final
{
public:
    /// An example design.
    struct Design
    {
        /// The name: the file's name without ".ork" (the menu's text, ExampleDesignFile's name).
        QString name;
        /// The design file.
        std::filesystem::path file;

        [[nodiscard]] bool operator==(const Design& other) const = default;
    };

    /// ExampleDesignFileAction.exampleFileOrder: the examples' names in the order of the menu,
    /// an empty name for a separator (Java's null).
    static constexpr std::array<std::string_view, 18> kMenuOrder{
        // Examples of basic rockets
        "A simple model rocket",
        "Two stage high power rocket",
        "Three stage low power rocket",
        "ARC payload rocket",
        "Tube fin rocket",
        "3D Printable Nose Cone and Fins",
        "",
        // Examples demonstrating complex rocket features
        "Deployable payload",
        "Airstart timing",
        "Chute release",
        "Dual parachute deployment",
        "Clustered motors",
        "Parallel booster staging",
        "Pods--airframes and winglets",
        "Pods--powered with recovery deployment",
        "",
        // Examples demonstrating customized functionality
        "Simulation extensions",
        "Simulation scripting",
    };

    /// The example designs of @p directory (see the class comment).
    explicit ExampleDesigns(const std::filesystem::path& directory);

    /// The designs, sorted by name.
    [[nodiscard]] const std::vector<Design>& designs() const noexcept { return m_designs; }

    /// The entries of the menu in order (see the class comment): a design, or nullopt for a
    /// separator.
    [[nodiscard]] std::vector<std::optional<Design>> menuEntries() const;

    /// Whether @p fileName is the name of an example design file: ExampleDesignFile.PATTERN
    /// ".*\.[oO][rR][kK]$" matched by String.matches(), so the name ends with a dot and "ork" in
    /// any case of these ASCII letters (".ork" alone too), and holds no line terminator before
    /// it ('.' takes none: '\n', '\r', U+0085, U+2028, U+2029).
    [[nodiscard]] static bool isDesignFileName(const QString& fileName);

    /// The name of the design of the file named @p fileName: all but its last four characters.
    [[nodiscard]] static QString designName(const QString& fileName);

private:
    std::vector<Design> m_designs;
};

}  // namespace QtRocket::Gui
