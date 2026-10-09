#pragma once

#include <memory>
#include <string_view>

#include "QtRocket/file/simplesax/AbstractElementHandler.h"
#include "QtRocket/file/simplesax/ElementHandler.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/util/Error.h"

namespace QtRocket
{

class DocumentLoadingContext;
class OpenRocketDocument;

/// Handles the content of the <openrocket> element of a design file: the rocket, the data
/// types, the simulations, the photo studio settings and the document preferences
/// (OpenRocket's file/openrocket/importt/OpenRocketContentHandler). Each element is read into
/// the document of the loading context by a handler of its own:
/// - <rocket>, once: a ComponentParameterHandler for the document's rocket. A second one gives
///   "Multiple rocket designs within one document, ignoring later ones." and is ignored.
/// - <datatypes>, once: a DatatypeHandler. A second one gives "Multiple datatype blocks.
///   Ignoring later ones.".
/// - <simulations>, once: a SimulationsHandler. A second one gives "Multiple simulation
///   definitions within one document, ignoring later ones.".
/// - <photostudio>, any number of times: a PhotoStudioHandler for the document's photo
///   settings; a later element adds to and replaces what an earlier one set.
/// - <docprefs>, any number of times: a DocumentPreferencesHandler; a later element adds to
///   the preferences and materials of the earlier ones.
/// - anything else: "Unknown element <name>, ignoring.", and the element is ignored with
///   everything in it. The names are compared exactly, so <Rocket> is unknown.
///
/// When one of the five elements closes, AbstractElementHandler's closeElement() warns of text
/// in it ("Unknown text in element 'rocket', ignoring.") and of attributes ("Unknown
/// attributes in element 'rocket', ignoring."): none of them has either in a file OpenRocket
/// writes. The text and the attributes are those of the layer SimpleSax's DelegatorHandler
/// pops, so after an element that was ignored inside one of the five (an unknown child of
/// <simulations>, say) they are the ignored element's (OpenRocket's bookkeeping slip, kept on
/// purpose).
///
/// The elements are applied in the order of the document. OpenRocket writes <rocket> first,
/// <datatypes> before <simulations> (a simulation's stored columns find the document's custom
/// expressions among the types then) and the others last.
///
/// Deviations from OpenRocket:
/// - The document exists from the start. Java's getDocument() answers null until <rocket> was
///   opened, so there a <photostudio> or a <docprefs> before <rocket> ends the load with a
///   NullPointerException, and so does a <simulation> in a <simulations> that comes before
///   <rocket> (a <datatypes> or an empty <simulations> before it loads). Here every element is
///   applied wherever it stands: simulations before the rocket are simulations of the empty
///   rocket's configurations, which the rocket that follows keeps. A document without
///   <rocket> is the empty rocket in both, without a warning. getDocument() itself is not
///   ported.
/// - The constructor takes the loading context only, like the handlers it makes; a context
///   without a document is a BugError.
/// - The handler of a child element is owned here and replaced by the next child's.
class OpenRocketContentHandler final : public AbstractElementHandler
{
public:
    /// The handler of the content of <openrocket>, read into the document of @p context. The
    /// context and what it points at must outlive the handler. For a <simulations> element
    /// the context must have a preference store (SimulationsHandler throws a BugError
    /// without), and for a <motor> element a motor finder (MotorHandler): OpenRocketLoader
    /// checks both before it reads a file.
    /// @throws BugError when @p context has no document
    explicit OpenRocketContentHandler(const DocumentLoadingContext& context);
    /// A temporary context would dangle.
    explicit OpenRocketContentHandler(const DocumentLoadingContext&& context) = delete;

    [[nodiscard]] Result<ElementHandler*> openElement(std::string_view  element,
                                                      const Attributes& attributes,
                                                      WarningSet&       warnings) override;

private:
    const DocumentLoadingContext* m_context;
    OpenRocketDocument*           m_document;
    bool                          m_rocketDefined{false};
    bool                          m_simulationsDefined{false};
    bool                          m_datatypesDefined{false};
    /// The handler of the child element read last; the next one replaces it.
    std::unique_ptr<ElementHandler> m_childHandler;
};

}  // namespace QtRocket
