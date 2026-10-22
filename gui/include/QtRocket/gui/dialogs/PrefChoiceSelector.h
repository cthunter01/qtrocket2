#pragma once

#include <string>
#include <string_view>

#include <QComboBox>
#include <QObject>
#include <QStringList>
#include <QtContainerFwd>

class QWheelEvent;
class QWidget;

namespace QtRocket
{
class Preferences;
}

namespace QtRocket::Gui
{

/// The combo box of a choice preference, an int that is the index of one of the descriptions
/// (OpenRocket's PreferencesPanel.PrefChoiceSelector, a ComboBoxModel): the Design page's
/// insert positions and the size of the design view's text. Its current item is the stored
/// choice (Preferences::getChoice(), the first item when the key is absent or out of range);
/// choosing an item stores its index at once (putChoice()).
///
/// The object name is the preference's key ("BodyComponentInsertPosition").
///
/// Deviations: Java reads the choice with getChoice(key, descriptions.length, 0), one more than
/// the last index, so a stored choice of 3 (of three descriptions) throws
/// ArrayIndexOutOfBoundsException while the page is made (probe); here it is out of range and
/// shows the first item, as the code that reads the preference treats it (the add buttons, the
/// design view's text). The wheel chooses only while the combo box has the focus (EnumComboBox's
/// rule; Java's combo boxes take no wheel).
class PrefChoiceSelector final : public QComboBox
{
    Q_OBJECT

public:
    /// The combo box of the choice @p preference of @p preferences (which must outlive it), its
    /// items @p descriptions in the order of their indices.
    PrefChoiceSelector(Preferences& preferences, std::string_view preference,
                       const QStringList& descriptions, QWidget* parent);
    ~PrefChoiceSelector() override;
    PrefChoiceSelector(const PrefChoiceSelector&)            = delete;
    PrefChoiceSelector& operator=(const PrefChoiceSelector&) = delete;
    PrefChoiceSelector(PrefChoiceSelector&&)                 = delete;
    PrefChoiceSelector& operator=(PrefChoiceSelector&&)      = delete;

    [[nodiscard]] const std::string& preference() const noexcept { return m_preference; }

    /// The stored choice, as the combo box shows it.
    [[nodiscard]] int storedChoice() const;

    /// Shows the stored choice as the current item.
    void refresh();

protected:
    void wheelEvent(QWheelEvent* event) override;

private:
    /// The user chose the item @p index (Java: setSelectedItem()).
    void activatedByUser(int index);

    Preferences* m_preferences;
    std::string  m_preference;
};

}  // namespace QtRocket::Gui
