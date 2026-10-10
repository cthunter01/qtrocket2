#pragma once

#include <memory>

#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/logging/WarningSet.h"

namespace QtRocket
{

/// What a successful load of a design file gives (GeneralRocketLoader::load()): the document
/// and the warnings of the load, which in Java are the loader's return value and its
/// getWarnings().
struct LoadedDocument
{
    /// The document that was read; never null. It belongs to whoever holds this, and outlives
    /// the loader and everything the loader was given but the preference store (see
    /// GeneralRocketLoader).
    std::unique_ptr<OpenRocketDocument> document;
    /// The warnings of the load, in the order they were given: those of the handlers that read
    /// the file, then those of the simulation extensions (documentLoaded()). At most
    /// DocumentConfig::kMaxWarnings of them, and then one more that says that further ones were
    /// dropped (see GeneralRocketLoader). The set KEEPS THAT CAPACITY, and so does every copy
    /// of it: who adds other warnings to it, or to a copy (those of a second load, of a
    /// simulation run), has them counted against the same thousand and dropped under the text
    /// "The file gave more than 1000 warnings ...". Lift the capacity first
    /// (WarningSet::clearCapacity()), or add into a set of your own (addAll() of a set without
    /// a capacity takes everything of this one).
    WarningSet warnings;
};

}  // namespace QtRocket
