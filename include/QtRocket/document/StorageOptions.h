#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

namespace QtRocket
{

/// How a document is to be saved (OpenRocket's document/StorageOptions): the file type, whether
/// the simulated data goes into the file, whether the user chose these options, and the preview
/// image an .ork archive carries. A document holds one as its default storage options.
///
/// A value type: a copy is Java's clone(), which copies the preview image's bytes.
///
/// After a load OpenRocket leaves the document's options at OPENROCKET, with the simulation data
/// saved when a simulation of the file has data, not explicitly set and without a preview image
/// (the preview.png of an archive is not read back).
///
/// Deviations from OpenRocket:
/// - The preview image is std::optional, nullopt for Java's null. An empty image is not "no
///   image" here either: Java's saver writes a preview only when the array is not empty, and
///   that test stays with the saver.
/// - getPreviewImage() returns a reference to the stored bytes; Java hands out its array, which a
///   caller may write to.
/// - operator== is an addition (Java compares StorageOptions by identity).
class StorageOptions
{
public:
    /// The file formats OpenRocket saves or exports (Java: StorageOptions.FileType), in Java's
    /// order. All four are kept; a saver refuses the formats it does not write.
    enum class FileType
    {
        OPENROCKET,
        ROCKSIM,
        RASAERO,
        WAVEFRONT_OBJ,
    };

    /// FileType.values(), in declaration order.
    static constexpr std::array<FileType, 4> kAllFileTypes{
        FileType::OPENROCKET, FileType::ROCKSIM, FileType::RASAERO, FileType::WAVEFRONT_OBJ};

    [[nodiscard]] FileType getFileType() const noexcept { return m_fileType; }
    void                   setFileType(FileType fileType) noexcept { m_fileType = fileType; }

    /// Whether the simulated data (the time series) is saved with the simulations.
    [[nodiscard]] bool getSaveSimulationData() const noexcept { return m_saveSimulationData; }
    void               setSaveSimulationData(bool saveSimulationData) noexcept
    {
        m_saveSimulationData = saveSimulationData;
    }

    /// Whether the user chose these options (in a save dialog) rather than a loader or a default.
    [[nodiscard]] bool isExplicitlySet() const noexcept { return m_explicitlySet; }
    void setExplicitlySet(bool explicitlySet) noexcept { m_explicitlySet = explicitlySet; }

    /// The bytes of the file preview image, or nullopt when none is set.
    [[nodiscard]] const std::optional<std::vector<std::byte>>& getPreviewImage() const noexcept
    {
        return m_previewImage;
    }
    /// Sets the bytes of the file preview image; nullopt removes it (Java: setPreviewImage(null)).
    void setPreviewImage(std::optional<std::vector<std::byte>> previewImage) noexcept
    {
        m_previewImage = std::move(previewImage);
    }
    /// Removes the file preview image.
    void clearPreviewImage() noexcept { m_previewImage.reset(); }

    /// The same file type, flags and preview image (an addition, see the class comment).
    [[nodiscard]] bool operator==(const StorageOptions&) const = default;

private:
    FileType                              m_fileType{FileType::OPENROCKET};
    bool                                  m_saveSimulationData{false};
    bool                                  m_explicitlySet{false};
    std::optional<std::vector<std::byte>> m_previewImage;
};

/// The constant's name, e.g. "WAVEFRONT_OBJ" (Java: FileType.name()).
[[nodiscard]] std::string_view name(StorageOptions::FileType fileType) noexcept;

}  // namespace QtRocket
