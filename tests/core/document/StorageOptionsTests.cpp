#include "QtRocket/document/StorageOptions.h"

#include <array>
#include <cstddef>
#include <initializer_list>
#include <optional>
#include <type_traits>
#include <vector>

#include <gtest/gtest.h>

// OpenRocket has no test of StorageOptions. The Java values are those of the probe
// CustomExpressionProbe.java, section F (probes/tier9a-document-d2).

namespace
{

using QtRocket::StorageOptions;

using FileType = StorageOptions::FileType;

// A value type: Java's clone() is the copy.
static_assert(std::is_copy_constructible_v<StorageOptions>);
static_assert(std::is_copy_assignable_v<StorageOptions>);
static_assert(std::is_nothrow_move_constructible_v<StorageOptions>);

using Image = std::optional<std::vector<std::byte>>;

/// A preview image of the bytes @p values.
[[nodiscard]] Image image(std::initializer_list<int> values)
{
    std::vector<std::byte> result;
    for (const int value : values)
    {
        result.push_back(static_cast<std::byte>(value));
    }
    return result;
}

TEST(StorageOptions, Defaults)
{
    // Java: type=OPENROCKET saveSimulationData=false explicitlySet=false preview=null
    const StorageOptions options;
    EXPECT_EQ(options.getFileType(), FileType::OPENROCKET);
    EXPECT_FALSE(options.getSaveSimulationData());
    EXPECT_FALSE(options.isExplicitlySet());
    EXPECT_EQ(options.getPreviewImage(), std::nullopt);
}

TEST(StorageOptions, FileTypesAreJavasInJavasOrder)
{
    // Java: FileType.values() = [OPENROCKET, ROCKSIM, RASAERO, WAVEFRONT_OBJ]
    EXPECT_EQ(StorageOptions::kAllFileTypes,
              (std::array<FileType, 4>{FileType::OPENROCKET, FileType::ROCKSIM, FileType::RASAERO,
                                       FileType::WAVEFRONT_OBJ}));
    EXPECT_EQ(static_cast<int>(FileType::OPENROCKET), 0);
    EXPECT_EQ(static_cast<int>(FileType::ROCKSIM), 1);
    EXPECT_EQ(static_cast<int>(FileType::RASAERO), 2);
    EXPECT_EQ(static_cast<int>(FileType::WAVEFRONT_OBJ), 3);
    EXPECT_EQ(name(FileType::OPENROCKET), "OPENROCKET");
    EXPECT_EQ(name(FileType::ROCKSIM), "ROCKSIM");
    EXPECT_EQ(name(FileType::RASAERO), "RASAERO");
    EXPECT_EQ(name(FileType::WAVEFRONT_OBJ), "WAVEFRONT_OBJ");
}

TEST(StorageOptions, SettersStoreWhatTheyAreGiven)
{
    StorageOptions options;
    options.setFileType(FileType::ROCKSIM);
    EXPECT_EQ(options.getFileType(), FileType::ROCKSIM);
    options.setFileType(FileType::RASAERO);
    EXPECT_EQ(options.getFileType(), FileType::RASAERO);
    options.setFileType(FileType::WAVEFRONT_OBJ);
    EXPECT_EQ(options.getFileType(), FileType::WAVEFRONT_OBJ);
    options.setFileType(FileType::OPENROCKET);
    EXPECT_EQ(options.getFileType(), FileType::OPENROCKET);
    options.setSaveSimulationData(true);
    EXPECT_TRUE(options.getSaveSimulationData());
    options.setSaveSimulationData(false);
    EXPECT_FALSE(options.getSaveSimulationData());
    options.setExplicitlySet(true);
    EXPECT_TRUE(options.isExplicitlySet());
    options.setExplicitlySet(false);
    EXPECT_FALSE(options.isExplicitlySet());

    // No setter touches another field.
    options.setFileType(FileType::RASAERO);
    options.setSaveSimulationData(true);
    EXPECT_FALSE(options.isExplicitlySet());
    EXPECT_EQ(options.getPreviewImage(), std::nullopt);
    options.setExplicitlySet(true);
    EXPECT_EQ(options.getFileType(), FileType::RASAERO);
    EXPECT_TRUE(options.getSaveSimulationData());
}

TEST(StorageOptions, PreviewImage)
{
    StorageOptions options;
    options.setPreviewImage(image({1, 2, 3}));
    EXPECT_EQ(options.getPreviewImage(), image({1, 2, 3}));
    // The other fields stay.
    EXPECT_EQ(options.getFileType(), FileType::OPENROCKET);
    EXPECT_FALSE(options.getSaveSimulationData());
    EXPECT_FALSE(options.isExplicitlySet());

    // An empty image is an image, not "none" (Java: "empty preview: length 0"); the saver is the
    // one that writes no preview for it.
    options.setPreviewImage(std::vector<std::byte>{});
    EXPECT_TRUE(options.getPreviewImage().has_value());
    EXPECT_EQ(options.getPreviewImage(), image({}));
    EXPECT_NE(options.getPreviewImage(), std::nullopt);

    options.clearPreviewImage();
    EXPECT_EQ(options.getPreviewImage(), std::nullopt);
    // Clearing twice, and Java's setPreviewImage(null).
    options.clearPreviewImage();
    EXPECT_EQ(options.getPreviewImage(), std::nullopt);
    options.setPreviewImage(image({7}));
    options.setPreviewImage(std::nullopt);
    EXPECT_EQ(options.getPreviewImage(), std::nullopt);
}

TEST(StorageOptions, ACopyIsJavasClone)
{
    // Java: clone: type=ROCKSIM save=true explicit=true preview=[9, 2, 3] original
    // preview=[1, 2, 3]: the clone has the fields and bytes of its own.
    StorageOptions options;
    options.setFileType(FileType::ROCKSIM);
    options.setSaveSimulationData(true);
    options.setExplicitlySet(true);
    options.setPreviewImage(image({1, 2, 3}));

    StorageOptions clone = options;
    EXPECT_EQ(clone.getFileType(), FileType::ROCKSIM);
    EXPECT_TRUE(clone.getSaveSimulationData());
    EXPECT_TRUE(clone.isExplicitlySet());
    EXPECT_EQ(clone.getPreviewImage(), image({1, 2, 3}));

    clone.setPreviewImage(image({9, 2, 3}));
    EXPECT_EQ(options.getPreviewImage(), image({1, 2, 3}));

    // Java: "cleared: null; the clone keeps [9, 2, 3]".
    options.clearPreviewImage();
    EXPECT_EQ(clone.getPreviewImage(), image({9, 2, 3}));

    // A clone of options without a preview has none.
    const StorageOptions without = options;
    EXPECT_EQ(without.getPreviewImage(), std::nullopt);
    EXPECT_EQ(without.getFileType(), FileType::ROCKSIM);
}

TEST(StorageOptions, EqualityComparesEveryField)
{
    // An addition: Java compares by identity (its clone "equals=false").
    const StorageOptions defaults;
    EXPECT_TRUE(defaults == StorageOptions());

    StorageOptions other;
    other.setFileType(FileType::WAVEFRONT_OBJ);
    EXPECT_FALSE(defaults == other);
    other = defaults;
    other.setSaveSimulationData(true);
    EXPECT_FALSE(defaults == other);
    other = defaults;
    other.setExplicitlySet(true);
    EXPECT_FALSE(defaults == other);
    other = defaults;
    other.setPreviewImage(std::vector<std::byte>{});
    EXPECT_FALSE(defaults == other);
    other.setPreviewImage(image({1}));
    StorageOptions same = other;
    EXPECT_TRUE(same == other);
    same.setPreviewImage(image({2}));
    EXPECT_FALSE(same == other);
    other = defaults;
    EXPECT_TRUE(defaults == other);
}

}  // namespace
