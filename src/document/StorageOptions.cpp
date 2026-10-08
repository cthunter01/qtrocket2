#include "QtRocket/document/StorageOptions.h"

#include <string_view>

namespace QtRocket
{

std::string_view name(StorageOptions::FileType fileType) noexcept
{
    switch (fileType)
    {
        case StorageOptions::FileType::OPENROCKET:
            return "OPENROCKET";
        case StorageOptions::FileType::ROCKSIM:
            return "ROCKSIM";
        case StorageOptions::FileType::RASAERO:
            return "RASAERO";
        case StorageOptions::FileType::WAVEFRONT_OBJ:
            return "WAVEFRONT_OBJ";
    }
    return "OPENROCKET";  // not reached: the switch covers every file type
}

}  // namespace QtRocket
