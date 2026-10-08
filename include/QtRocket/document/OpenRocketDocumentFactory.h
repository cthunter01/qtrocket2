#pragma once

#include <memory>
#include <string_view>

namespace QtRocket
{

class OpenRocketDocument;
class Rocket;

/// Makes documents (OpenRocket's document/OpenRocketDocumentFactory).
///
/// Whatever the function, the document's constructor enables the events of its rocket and starts
/// the undo history with the rocket as it is, and the document has no file, no simulation and
/// is saved until something changes (see OpenRocketDocument).
///
/// Deviations from OpenRocket:
/// - The name of the stage of createNewRocket() is the English text of
///   "BasicFrame.StageName.Sustainer"; Java asks the application's translator.
/// - createDocumentFromRocket() takes the rocket over (Java: a reference); null is a BugError
///   (Java: a NullPointerException).
class OpenRocketDocumentFactory
{
public:
    OpenRocketDocumentFactory() = delete;

    /// The name of the stage of a new rocket: the text of "BasicFrame.StageName.Sustainer".
    static constexpr std::string_view kSustainerName = "Sustainer";

    /// The document of a new design: a rocket named "Rocket" with one stage named "Sustainer"
    /// and no flight configuration but the default, in which every stage is set active. It is
    /// marked saved (setSaved(true)), which, nothing having changed yet, it was already.
    [[nodiscard]] static std::unique_ptr<OpenRocketDocument> createNewRocket();

    /// A document of @p rocket.
    /// @throws BugError when @p rocket is null
    [[nodiscard]] static std::unique_ptr<OpenRocketDocument> createDocumentFromRocket(
        std::unique_ptr<Rocket> rocket);

    /// A document of a rocket without a stage: what a loader fills.
    [[nodiscard]] static std::unique_ptr<OpenRocketDocument> createEmptyRocket();
};

}  // namespace QtRocket
