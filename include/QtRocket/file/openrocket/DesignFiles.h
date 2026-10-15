#pragma once

#include <expected>
#include <filesystem>
#include <optional>
#include <string_view>

namespace QtRocket
{

/// Which file a design may read: the rule of QtRocket's own (not in OpenRocket) that the
/// loader's handlers ask before they read a file that a plain or gzip design names, a lookup
/// table (the user's decision U5 of 2026-10-08; CsvLookupHandler and the legacy elements of
/// SimulationConditionsHandler), a decal image (decision D8 of 2026-10-14; AppearanceHandler)
/// and a thrust curve "thrustcurves/<digest>.rse" (MotorHandler). The names of an archive's
/// entries are no files and are free.
///
/// OpenRocket reads whatever path a design names, a relative one against the current directory
/// of the process: a design that someone was sent can so make the application open any file of
/// the machine it is opened on. Here a design reads a file only from inside its own directory,
/// the one DocumentLoadingContext::getDesignDirectory() names (that of the design's file, or the
/// base directory of a design read from memory), and the current directory of the process plays
/// no part (fileOfDesign()):
/// - a relative name is resolved against the design's directory;
/// - a name that is not relative is refused: an absolute path, and whatever else has a root as
///   std::filesystem::path of the platform tells it ("/x" everywhere; on Windows also a drive
///   "C:\x" and "C:x", a path of the current drive "\x", a server "\\server\share\x" and a
///   device path "\\.\x" or "\\?\x"). That holds also for a path that points into the design's
///   directory;
/// - a name that leaves the directory through ".." is refused. That is decided on the name
///   alone, in its lexically normal form (std::filesystem::path::lexically_normal(): "a/../b" is
///   "b" and stays inside, "a/../../b" is "../b" and leaves), and nothing is asked of the file
///   system; the normal form is also what is opened. A name with an element of nothing but dots
///   and blanks other than "." and ".." ("...", ".. ") is refused with these, because Windows
///   reads such an element in ways of its own;
/// - a design without a directory (read from memory, with no base directory given) reads no
///   file at all;
/// - ON WINDOWS ONLY (kRefusesDeviceNames), a name with an element that Windows reserves for a
///   device is refused (isDeviceName(): "nul", "COM1", "aux.csv"), because Windows opens the
///   device for such a name in any directory: "con" would read from the console and "COM1" wait
///   at a serial port. The decisions do not name this refusal; it is there for the one platform
///   on which a relative name inside the directory is not a file of the directory. Elsewhere
///   such a name is a file like any other, which OpenRocket reads, and so it is read: a design
///   of a table "aux.csv" loads on Linux and macOS and is refused on Windows.
/// Each handler gives a warning of its own for a refusal (LoaderWarnings) and reads nothing.
///
/// What the rule does not do: a symbolic link inside the design's directory is followed wherever
/// it points, a link being part of the directory, which is the user's own (a design file cannot
/// make one); and the design's directory is taken as it is given, so with a ".." behind a
/// symbolic link in the directory's own path the lexically normal path that is opened is not the
/// directory the system would reach.
class DesignFiles final
{
public:
    DesignFiles() = delete;

    /// Why a design may not read a file it names (fileOfDesign()).
    enum class Refusal
    {
        /// The design has no directory.
        NO_DIRECTORY,
        /// The name is not relative: it has a root directory or a root name.
        NOT_RELATIVE,
        /// The name leaves the design's directory through "..", or has an element of dots and
        /// blanks that Windows may read as that.
        LEAVES_DIRECTORY,
        /// An element of the name is the name of a device of Windows (given on Windows only:
        /// kRefusesDeviceNames).
        DEVICE_NAME,
    };

    /// Whether fileOfDesign() refuses a name with an element that Windows reserves for a
    /// device: on Windows, where such a name opens the device, and nowhere else.
#ifdef _WIN32
    static constexpr bool kRefusesDeviceNames = true;
#else
    static constexpr bool kRefusesDeviceNames = false;
#endif

    /// The file that a design whose directory is @p designDirectory names as @p file: the
    /// directory with the lexically normal form of the name behind it, which is what the caller
    /// opens, or why the design may not read such a file (see the class comment). Nothing is
    /// asked of the file system. An empty @p designDirectory stands for the current directory,
    /// as "." does. The reasons are looked for in the order of Refusal; the last of them is
    /// given only where kRefusesDeviceNames says so.
    [[nodiscard]] static std::expected<std::filesystem::path, Refusal> fileOfDesign(
        const std::optional<std::filesystem::path>& designDirectory,
        const std::filesystem::path&                file);

    /// Whether @p file has no root: neither a root directory nor, on Windows, a drive or a
    /// server ('/x' is not is_absolute() on Windows, but it has a root directory).
    [[nodiscard]] static bool isRelativeName(const std::filesystem::path& file);

    /// Whether Windows reserves @p name, one element of a path as UTF-8 text, for a device
    /// whatever directory it stands in: "CON", "PRN", "AUX", "NUL", "COM0" to "COM9", "LPT0"
    /// to "LPT9" (the digit also as a superscript one, two or three), "CONIN$" and "CONOUT$",
    /// the ASCII letters in any case, with or without blanks at the end and whatever follows
    /// a first dot or colon ("nul", "Aux.csv", "com1:", "LPT1 .txt"). The answer is the same
    /// on every platform; what fileOfDesign() does with it is not (kRefusesDeviceNames).
    [[nodiscard]] static bool isDeviceName(std::string_view name);
};

}  // namespace QtRocket
