#pragma once

#include <cstddef>
#include <deque>
#include <filesystem>
#include <functional>
#include <optional>
#include <string_view>
#include <variant>
#include <vector>

#include <QString>
#include <QStringList>
#include <QtContainerFwd>

#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/gui/util/UserPrompts.h"

class QWidget;

namespace QtRocket
{
class OpenRocketDocument;
}

namespace QtRocket::Gui::Test
{

/// The UserPrompts of the tests: every question is answered from a script the test writes
/// beforehand, in the order the questions come, and every question and message is recorded.
/// A question the script has no answer for, or an answer of another kind than the question,
/// fails the test and gives the cancelling answer (Cancel, No, no file), so that no test can hang
/// on a dialog or go on with an answer it did not mean; an answer left unused when the double
/// is destroyed fails the test too. Messages need no answer.
class ScriptedUserPrompts final : public UserPrompts
{
public:
    /// What was asked or shown.
    enum class Kind
    {
        INSERT_POSITION,
        DISCARD_CHANGES,
        SAVE_CHANGES,
        OVERWRITE,
        STORAGE_OPTIONS,
        YES_NO,
        DECAL_FILE,
        OPEN_FILES,
        SAVE_FILE,
        DESIGN_INFORMATION,
        MESSAGE,
        WARNINGS,
        INTERNAL_ERROR,
    };

    /// One question or message, as the code under test asked it.
    struct Prompt
    {
        Kind    kind;
        QString title;
        /// The question's texts: the design's or the file's name, the message lines and
        /// warnings, the request's name filters, ...; the name of the question's enum constant
        /// for the fixed questions.
        QStringList texts;
    };

    /// What editDesignInformation() does with the document (the user's edits); nothing by
    /// default.
    using DesignInformationEdit = std::function<void(OpenRocketDocument&)>;

    ScriptedUserPrompts() = default;
    ~ScriptedUserPrompts() override;
    ScriptedUserPrompts(const ScriptedUserPrompts&)            = delete;
    ScriptedUserPrompts& operator=(const ScriptedUserPrompts&) = delete;
    ScriptedUserPrompts(ScriptedUserPrompts&&)                 = delete;
    ScriptedUserPrompts& operator=(ScriptedUserPrompts&&)      = delete;

    // The script: the answer of the next question of that kind, in order.
    void answerInsertPosition(InsertPositionAnswer answer);
    void answerDiscardChanges(DiscardAnswer answer);
    void answerSaveChanges(SaveChangesAnswer answer);
    void answerOverwrite(bool overwrite);
    void answerStorageOptions(std::optional<bool> saveSimulationData);
    void answerYesNo(bool yes);
    void answerDecalFile(std::optional<OpenFileAnswer> answer);
    void answerOpenFiles(std::optional<OpenFileAnswer> answer);
    void answerSaveFile(std::optional<SaveFileAnswer> answer);
    void answerDesignInformation(DesignInformationEdit edit = {});
    void answerInternalError(InternalErrorAnswer answer);

    /// Every question and message so far, in order.
    [[nodiscard]] const std::vector<Prompt>& prompts() const noexcept { return m_prompts; }
    /// The requests of askOpenFiles() and askSaveFile() so far, in order.
    [[nodiscard]] const std::vector<OpenFileRequest>& openFileRequests() const noexcept
    {
        return m_openFileRequests;
    }
    [[nodiscard]] const std::vector<SaveFileRequest>& saveFileRequests() const noexcept
    {
        return m_saveFileRequests;
    }
    /// The answers of the script not asked for yet.
    [[nodiscard]] std::size_t unusedAnswers() const noexcept { return m_script.size(); }

    /// The name of @p kind ("SAVE_CHANGES").
    [[nodiscard]] static std::string_view kindName(Kind kind);

    [[nodiscard]] InsertPositionAnswer askInsertPosition(QWidget*               parent,
                                                         InsertPositionQuestion question) override;
    [[nodiscard]] DiscardAnswer        askDiscardChanges(QWidget*        parent,
                                                         DiscardQuestion question) override;
    [[nodiscard]] SaveChangesAnswer    askSaveChanges(QWidget*       parent,
                                                      const QString& designName) override;
    [[nodiscard]] bool askOverwrite(QWidget* parent, const QString& fileName) override;
    [[nodiscard]] std::optional<bool> askStorageOptions(QWidget* parent,
                                                        bool     saveSimulationData) override;
    [[nodiscard]] bool                askYesNo(QWidget* parent, const Question& question) override;
    [[nodiscard]] std::optional<OpenFileAnswer> askDecalFile(
        QWidget* parent, const QString& message, const std::filesystem::path& directory) override;
    [[nodiscard]] std::optional<OpenFileAnswer> askOpenFiles(
        QWidget* parent, const OpenFileRequest& request) override;
    [[nodiscard]] std::optional<SaveFileAnswer> askSaveFile(
        QWidget* parent, const SaveFileRequest& request) override;
    void editDesignInformation(QWidget* parent, OpenRocketDocument& document) override;
    void showMessage(QWidget* parent, const Message& message) override;
    void showWarnings(QWidget* parent, const WarningsMessage& message) override;
    /// Recorded with the failure's kind (BUG, EXCEPTION, UNKNOWN, ERROR_CONDITION), its message
    /// and its context as the texts; the cancelling answer is CONTINUE.
    [[nodiscard]] InternalErrorAnswer showInternalError(QWidget*              parent,
                                                        const Guard::Failure& failure) override;

private:
    using Answer =
        std::variant<InsertPositionAnswer, DiscardAnswer, SaveChangesAnswer, bool,
                     std::optional<bool>, std::optional<OpenFileAnswer>,
                     std::optional<SaveFileAnswer>, DesignInformationEdit, InternalErrorAnswer>;

    struct Scripted
    {
        Kind   kind;
        Answer answer;
    };

    [[nodiscard]] static Prompt makePrompt(Kind kind, QString title, QStringList texts);
    void                        script(Kind kind, Answer answer);
    /// Records the question and returns the script's next answer, or @p cancel when the script
    /// has none of this kind (a test failure).
    template <class T>
    [[nodiscard]] T next(Prompt prompt, T cancel);

    std::deque<Scripted>         m_script;
    std::vector<Prompt>          m_prompts;
    std::vector<OpenFileRequest> m_openFileRequests;
    std::vector<SaveFileRequest> m_saveFileRequests;
};

}  // namespace QtRocket::Gui::Test
