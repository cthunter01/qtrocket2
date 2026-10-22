#pragma once

#include <QString>

#include "QtRocket/gui/dialogs/OptionDialog.h"

class QPushButton;
class QWidget;

namespace QtRocket::Gui
{

/// The question of OpenRocket's DecalNotFoundDialog.showDialog(), asked when a save fails
/// because the source file of a decal image is missing: "No decal source file"
/// (ExportDecalDialog.source.title), a warning with Yes and No, whose message is the save's
/// error text (ExportDecalDialog.source.exception: "Could not find decal source file '<file>'.
/// <br> <br>Would you like to look for this file?", rich text) wrapped at 400 pixels
/// (MessageWidthUtil.setMessageWidth(message, 400)). Yes asks for the file next (a file
/// chooser: WidgetUserPrompts::askDecalFile(), which shows this question); No, or closing the
/// dialog, leaves the image out of the next save (BasicFrame.saveAsOpenRocket()).
///
/// Deviation: the message is the error's text as rich text in a label 400 pixels wide (Java
/// wraps it in an HTML paragraph of that width).
class DecalNotFoundDialog final : public OptionDialog
{
    Q_OBJECT

public:
    /// The width of the message, in pixels (MessageWidthUtil.setMessageWidth(message, 400)).
    static constexpr int kMessageWidth = 400;

    /// The question with @p message, not shown yet.
    explicit DecalNotFoundDialog(const QString& message, QWidget* parent = nullptr);
    ~DecalNotFoundDialog() override;
    DecalNotFoundDialog(const DecalNotFoundDialog&)            = delete;
    DecalNotFoundDialog& operator=(const DecalNotFoundDialog&) = delete;
    DecalNotFoundDialog(DecalNotFoundDialog&&)                 = delete;
    DecalNotFoundDialog& operator=(DecalNotFoundDialog&&)      = delete;

    /// Asks the question with @p message, modal, parented to @p parent (or none): whether the
    /// user wants to look for the file (Yes).
    [[nodiscard]] static bool askToLookForFile(QWidget* parent, const QString& message);

    /// Whether Yes closed the dialog.
    [[nodiscard]] bool isYes() const noexcept;

    [[nodiscard]] QPushButton* yesButton() const noexcept { return m_yes; }
    [[nodiscard]] QPushButton* noButton() const noexcept { return m_no; }

private:
    QPushButton* m_yes;
    QPushButton* m_no;
};

}  // namespace QtRocket::Gui
