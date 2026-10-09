#pragma once

#include <cstddef>
#include <span>

#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/util/Error.h"

namespace QtRocket
{

class DocumentLoadingContext;

/// Loads a rocket definition from an OpenRocket design document, the XML an .ork file holds
/// (OpenRocket's file/openrocket/importt/OpenRocketLoader with the load() of its base class
/// file/AbstractRocketLoader). GeneralRocketLoader is the entry of the file subsystem: it
/// finds the document in a file, chooses the attachments and calls this.
///
/// load() reads the bytes with SimpleSax, which finds their encoding as the JDK's parser does
/// (SimpleSax::readXml(bytes)), through an OpenRocketHandler, into the document of the
/// context. Then, in OpenRocket's order:
/// 1. the stage activeness the flight configurations of the file name is applied
///    (FlightConfiguration::applyPreloadedStageActiveness()) to every configuration of the
///    rocket, the default one included;
/// 2. for every simulation of the document, in order: Simulation::syncModId(), because the
///    configuration's modification id moved on while the rest of the file was read; and when
///    its status (Simulation::getStatus(), which works it out now) is neither EXTERNAL nor
///    NOT_SIMULATED and the first branch of its stored flight data has a time column, the
///    document's default storage options are set to save the simulated data. (As a file is
///    read, a simulation with stored data never comes out EXTERNAL: the test is Java's.)
/// 3. the storage options are marked as not chosen by the user and get the file type
///    OPENROCKET;
/// 4. SimulationExtension::documentLoaded() of every extension of every simulation, with the
///    load's warnings (the scripting extension disables an enabled script there and says so);
/// 5. the undo history of the document is cleared.
/// The document is filled with the events of its rocket enabled, as it was made
/// (OpenRocketDocumentFactory::createEmptyRocket()); nothing here switches them off.
///
/// Failures:
/// - a document that is not well-formed XML, or that the JDK's parser refuses for a byte
///   sequence its encoding does not have: ErrorCode::PARSE, "Malformed XML in input." (Java:
///   the SAXException);
/// - a document in an encoding SimpleSax does not read: ErrorCode::IO, "I/O error: <name of
///   the encoding>" (Java: the UnsupportedEncodingException, an IOException, as
///   AbstractRocketLoader words it);
/// - a failure of a handler or a setter, which is ErrorCode::INVALID_ARGUMENT with the message
///   of Java's IllegalArgumentException: returned as it is. The exception leaves Java's loader
///   too, and GeneralRocketLoader puts "Exception loading stream: " in front of it. The
///   message can be empty.
/// - a document type declaration XmlScanner does not read (declarations in an internal
///   subset): ErrorCode::UNSUPPORTED_FORMAT with XmlScanner's message, as it is. OpenRocket
///   reads such a document (an existing deviation of XmlScanner).
/// As in Java the handlers have filled the document with everything before the place of the
/// failure, and none of the steps above has run: a caller discards the document. What a
/// failed load leaves changed outside the document is listed at GeneralRocketLoader.
///
/// Deviations from OpenRocket:
/// - A static function; the context is not const, because the root handler sets the file
///   version in it. Java's fileName argument, which no loader of this format reads, is gone.
/// - The warnings go to the set the caller gives, which is not cleared first (Java: a set of
///   the loader, cleared at the start of a load and asked for afterwards).
/// - AbstractRocketLoader, RocketLoader and RocketLoadException are not classes here: the one
///   thing AbstractRocketLoader does, turning an IOException into "I/O error: <message>", is
///   in load(), there being one loader to share it; RocketLoader's two methods are load()
///   and the warning set; and a RocketLoadException is the Error of the Result.
/// - What the handlers need of the context is checked before the file is read, where Java
///   would fail in the middle of it with a NullPointerException: a document, a motor finder
///   and a preference store, each a BugError when it is missing (a file decides whether a
///   handler asks for the last two, so the check cannot be left to them).
class OpenRocketLoader final
{
public:
    OpenRocketLoader() = delete;

    /// Reads the design document @p source into the document of @p context and runs the steps
    /// that follow (loadFromStream()); the warnings of the handlers and of the extensions are
    /// added to @p warnings. The context, and everything it points at, is used during the
    /// call only, but for its preference store, which the simulations of the document keep
    /// (DocumentLoadingContext::getPreferences()).
    /// @throws BugError when @p context has no document, no motor finder or no preference
    ///         store
    [[nodiscard]] static Result<void> load(DocumentLoadingContext&    context,
                                           std::span<const std::byte> source, WarningSet& warnings);
};

}  // namespace QtRocket
