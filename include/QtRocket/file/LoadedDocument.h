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
    /// the file, then those of the simulation extensions (documentLoaded()).
    WarningSet warnings;
};

}  // namespace QtRocket
