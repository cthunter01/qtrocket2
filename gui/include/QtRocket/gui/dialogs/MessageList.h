#pragma once

#include <QColor>
#include <QListWidget>
#include <QStringList>
#include <QtContainerFwd>

class QMouseEvent;
class QWidget;

namespace QtRocket::Gui
{

/// The list of warnings or errors of WarningDialog and ErrorWarningDialog: a JList of the
/// messages' texts with OpenRocket's BetterListCellRenderer (rows of alternating background,
/// the texts in the list's colour or in one given for them) in its scroll pane. A press below
/// the last row clears the selection (ErrorWarningDialog's mouse listener).
///
/// Deviations: the alternating rows are the palette's Base and AlternateBase (the platform
/// style's; OpenRocket's ROW_LIGHTER and ROW_DARKER theme colours), and a selected row has the
/// palette's highlighted text; Java's lists of WarningDialog select the nearest row on a press
/// below the last one, these clear the selection as ErrorWarningDialog's do.
class MessageList final : public QListWidget
{
    Q_OBJECT

public:
    /// The list of @p messages, in order.
    explicit MessageList(const QStringList& messages, QWidget* parent = nullptr);

    /// The list of @p messages with their texts in @p textColor (BetterListCellRenderer(Color)).
    MessageList(const QStringList& messages, const QColor& textColor, QWidget* parent = nullptr);

    /// The texts of the rows, in order.
    [[nodiscard]] QStringList messages() const;

protected:
    void mousePressEvent(QMouseEvent* event) override;
};

}  // namespace QtRocket::Gui
