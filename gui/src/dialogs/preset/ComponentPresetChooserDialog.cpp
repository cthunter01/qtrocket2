#include "QtRocket/gui/dialogs/preset/ComponentPresetChooserDialog.h"

#include <array>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include <QAbstractItemView>
#include <QCheckBox>
#include <QDialog>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QModelIndex>
#include <QObject>
#include <QPushButton>
#include <QString>
#include <QVBoxLayout>
#include <QWidget>
#include <Qt>

#include "QtRocket/gui/dialogs/ModalDialog.h"
#include "QtRocket/gui/dialogs/preset/ComponentPresetFilterModel.h"
#include "QtRocket/gui/dialogs/preset/ComponentPresetTable.h"
#include "QtRocket/gui/settings/GuiPreferenceKeys.h"
#include "QtRocket/gui/settings/WindowGeometry.h"
#include "QtRocket/gui/util/EnumText.h"
#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/gui/util/GuiServices.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/util/UserPrompts.h"
#include "QtRocket/gui/widgets/StyledLabel.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/preset/ComponentPreset.h"
#include "QtRocket/rocket/preset/ComponentPresetDatabase.h"
#include "QtRocket/rocket/preset/ComponentPresetType.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Chars.h"

namespace QtRocket::Gui
{

namespace
{

/// MigLayout's paragraph gap ("ins para", "wrap para", "gapright para").
constexpr int kParagraphGap = 12;
/// The note's indent ("gapleft 5px").
constexpr int kNoteIndent = 5;

/// The preset type of @p kind's components.
/// @throws BugError for a kind that takes no presets
[[nodiscard]] ComponentPresetType presetTypeOrBug(ComponentKind kind)
{
    const std::optional<ComponentPresetType> type = presetTypeOf(kind);
    if (!type.has_value())
    {
        bug("ComponentPresetChooserDialog: a " + std::string(displayName(kind)) +
            " takes no presets");
    }
    return *type;
}

[[nodiscard]] QString context()
{
    return QStringLiteral("ComponentPresetChooserDialog");
}

}  // namespace

ComponentPresetChooserDialog::ComponentPresetChooserDialog(const GuiServices&         services,
                                                           UserPrompts::PresetRequest request,
                                                           QWidget*                   parent)
  : QDialog(parent),
    m_preferences(&services.preferences()),
    m_request(request),
    m_tableName(GuiPreferenceKeys::componentPresetTableName(request.componentKind)),
    m_table(makeChild<ComponentPresetTable>(
        this, presetTypeOrBug(request.componentKind),
        services.presets().listForType(presetTypeOrBug(request.componentKind)),
        services.preferences())),
    //: ComponentPresetChooserDialog.filter.label
    m_filterLabel(makeChild<QLabel>(this, tr("Filter by text:"))),
    m_filterText(makeChild<QLineEdit>(this)),
    //: ComponentPresetChooserDialog.checkbox.showLegacyCheckBox
    m_showLegacy(makeChild<QCheckBox>(this, tr("Show Legacy Database"))),
    // String.format("<html>%s %s</html>", Chars.UP_ARROW, trans.get("lbl.favorites")).
    m_favoritesLabel(makeChild<StyledLabel>(
        this,
        QStringLiteral("<html>%1 %2</html>")
            .arg(QtStrings::fromUtf8(Chars::kUpArrow),
                 //: ComponentPresetChooserDialog.lbl.favorites
                 tr("Check to add preset to the preset drop-down menu in the component edit "
                    "dialog <br>Directly apply a preset by double-clicking it or by selecting it "
                    "and closing this window.")),
        kFavoritesLabelSize)),
    //: ComponentPresetChooserDialog.checkbox.alwaysOpenPreset
    m_alwaysOpen(makeChild<QCheckBox>(
        this,
        tr("Always open this dialog when creating a new %1").arg(enumText(request.componentKind)))),
    //: dlg.but.close
    m_close(makeChild<QPushButton>(this, tr("Close")))
{
    setObjectName(QString::fromLatin1(kGeometryName));
    //: ComponentPresetChooserDialog.title
    setWindowTitle(tr("Choose component preset"));
    // Dialog.ModalityType.APPLICATION_MODAL.
    setWindowModality(Qt::ApplicationModal);

    m_table->setObjectName(QStringLiteral("componentSelectionTable"));
    m_filterLabel->setObjectName(QStringLiteral("filterLabel"));
    m_filterText->setObjectName(QStringLiteral("filterText"));
    m_filterText->setMinimumWidth(kFilterTextMinimumWidth);
    connect(m_filterText, &QLineEdit::textChanged, this,
            guarded([this] { updateFilters(); }, context()));

    m_showLegacy->setObjectName(QStringLiteral("showLegacyCheckBox"));
    makeDiameterBoxes();

    // Load the table UI settings from the preferences; the table puts the Legacy state back as it
    // was before (OpenRocket: "Restore legacy state (may change during UI preference loading)"),
    // so the box starts unticked.
    m_table->loadUiPreferences(m_tableName);
    m_showLegacy->setChecked(m_table->isLegacyShown());
    connect(m_showLegacy, &QCheckBox::toggled, this,
            guarded([this](bool shown) { m_table->setLegacyShown(shown); }, context()));
    // When the legacy column changes visibility (by right-clicking on the column header and
    // toggling the legacy header checkbox), update the main legacy checkbox (OpenRocket's
    // comment).
    connect(m_table, &ComponentPresetTable::legacyShownChanged, m_showLegacy,
            &QCheckBox::setChecked);

    m_favoritesLabel->setObjectName(QStringLiteral("favoritesLabel"));
    m_favoritesLabel->setTextFormat(Qt::RichText);

    // When double-clicking a preset row, apply the preset and close this dialog (OpenRocket's
    // comment).
    connect(m_table, &QAbstractItemView::doubleClicked, this,
            guarded([this](const QModelIndex& index) { applyDoubleClicked(index); }, context()));

    // Always open this window when creating a new component (OpenRocket's comment).
    m_alwaysOpen->setObjectName(QStringLiteral("alwaysOpenPreset"));
    m_alwaysOpen->setChecked(
        m_preferences->getBoolean(GuiPreferenceKeys::alwaysOpenPresetKey(m_request.componentKind),
                                  GuiPreferenceKeys::kAlwaysOpenPresetDefault));
    connect(m_alwaysOpen, &QCheckBox::clicked, this,
            guarded(
                [this](bool checked) {
                    m_preferences->putBoolean(
                        GuiPreferenceKeys::alwaysOpenPresetKey(m_request.componentKind), checked);
                },
                context()));

    m_close->setObjectName(QStringLiteral("closeButton"));
    // GUIUtil.setDisposableDialogOptions(this, closeButton): the default button.
    m_close->setDefault(true);
    connect(m_close, &QPushButton::clicked, this,
            guarded([this] { closeWithSelection(); }, context()));

    layOut();
    applyInitialGeometry(parent);
    updateFilters();
}

ComponentPresetChooserDialog::~ComponentPresetChooserDialog()
{
    // QWidget's destructor deletes the parts after this object's members are gone: none of their
    // signals may reach this dialog's slots then.
    disconnect(m_table, nullptr, this, nullptr);
    disconnect(m_filterText, nullptr, this, nullptr);
    disconnect(m_showLegacy, nullptr, this, nullptr);
    disconnect(m_alwaysOpen, nullptr, this, nullptr);
    disconnect(m_close, nullptr, this, nullptr);
    if (m_foreBox != nullptr)
    {
        disconnect(m_foreBox, nullptr, this, nullptr);
    }
    if (m_aftBox != nullptr)
    {
        disconnect(m_aftBox, nullptr, this, nullptr);
    }
}

UserPrompts::PresetAnswer ComponentPresetChooserDialog::choose(
    QWidget* parent, const GuiServices& services, const UserPrompts::PresetRequest& request)
{
    ModalDialog<ComponentPresetChooserDialog> dialog(parent, services, request);
    static_cast<void>(dialog.exec());
    const ComponentPresetChooserDialog* const shown = dialog.get();
    if (shown == nullptr)
    {
        return {.outcome = UserPrompts::PresetOutcome::CLOSED, .preset = nullptr};
    }
    return shown->answer();
}

void ComponentPresetChooserDialog::makeDiameterBoxes()
{
    const ComponentPresetFilterModel& filter = m_table->filterModel();
    if (m_request.foreDiameter.has_value() && filter.foreDiameterColumn() >= 0)
    {
        //: ComponentPresetChooserDialog.checkbox.filterForeDiameter
        m_foreBox = makeChild<QCheckBox>(this, tr("Match fore diameter"));
        m_foreBox->setObjectName(QStringLiteral("foreDiameterFilterCheckBox"));
        //: ComponentPresetChooserDialog.checkbox.filterForeDiameter.ttip
        m_foreBox->setToolTip(
            tr("Search for components with the same fore diameter as the previous component"));
        m_foreBox->setChecked(m_preferences->isMatchForeDiameter());
        connect(m_foreBox, &QCheckBox::toggled, this,
                guarded(
                    [this](bool checked) {
                        updateFilters();
                        m_preferences->setMatchForeDiameter(checked);
                    },
                    context()));
    }
    if (m_request.aftDiameter.has_value() && filter.aftDiameterColumn() >= 0)
    {
        if (m_request.flippedNoseCone)
        {
            //: ComponentPresetChooserDialog.checkbox.filterForeDiameter
            m_aftBox = makeChild<QCheckBox>(this, tr("Match fore diameter"));
            //: ComponentPresetChooserDialog.checkbox.filterForeDiameter.ttip
            m_aftBox->setToolTip(
                tr("Search for components with the same fore diameter as the previous component"));
        }
        else
        {
            //: ComponentPresetChooserDialog.checkbox.filterAftDiameter
            m_aftBox = makeChild<QCheckBox>(this, tr("Match aft diameter"));
            //: ComponentPresetChooserDialog.checkbox.filterAftDiameter.ttip
            m_aftBox->setToolTip(
                tr("Search for components with the same aft diameter as the next component"));
        }
        m_aftBox->setObjectName(QStringLiteral("aftDiameterFilterCheckBox"));
        m_aftBox->setChecked(m_preferences->isMatchAftDiameter());
        connect(m_aftBox, &QCheckBox::toggled, this,
                guarded(
                    [this](bool checked) {
                        updateFilters();
                        m_preferences->setMatchAftDiameter(checked);
                    },
                    context()));
    }
}

void ComponentPresetChooserDialog::layOut()
{
    // MigLayout "fill, ins para": two columns, the first one growing.
    auto* layout = makeChild<QGridLayout>(this);
    layout->setContentsMargins(kParagraphGap, kParagraphGap, kParagraphGap, kParagraphGap);
    layout->setVerticalSpacing(kParagraphGap);
    layout->setHorizontalSpacing(kParagraphGap);
    layout->setColumnStretch(0, 1);
    layout->setRowStretch(1, 1);

    // The text filter ("growx, pushx, ay 0, gapright para"): a panel of the label ("gapright
    // para") and the field ("width 50:320, pushx, growx").
    auto* text       = makeChild<QWidget>(this);
    auto* textLayout = makeChild<QHBoxLayout>(text);
    textLayout->setContentsMargins(0, 0, 0, 0);
    textLayout->setSpacing(kParagraphGap);
    textLayout->addWidget(m_filterLabel);
    textLayout->addWidget(m_filterText, 1);
    layout->addWidget(text, 0, 0, Qt::AlignTop);

    // The check boxes, one per row ("wrap"), in a panel ("ins 0") with the next row a paragraph
    // gap below ("wrap para").
    auto* boxes       = makeChild<QWidget>(this);
    auto* boxesLayout = makeChild<QVBoxLayout>(boxes);
    boxesLayout->setContentsMargins(0, 0, 0, 0);
    boxesLayout->addWidget(m_showLegacy);
    for (QCheckBox* const box : {m_foreBox, m_aftBox})
    {
        if (box != nullptr)
        {
            boxesLayout->addWidget(box);
        }
    }
    layout->addWidget(boxes, 0, 1, Qt::AlignTop);

    // "grow, pushy, spanx, wrap rel"
    layout->addWidget(m_table, 1, 0, 1, 2);
    // "spanx, gapleft 5px, wrap para"
    m_favoritesLabel->setContentsMargins(kNoteIndent, 0, 0, 0);
    layout->addWidget(m_favoritesLabel, 2, 0, 1, 2);
    // "spanx 2", then the button "spanx, right, tag close".
    layout->addWidget(m_alwaysOpen, 3, 0);
    layout->addWidget(m_close, 3, 1, Qt::AlignRight);

    // The focus order of the layout, as Swing's LayoutFocusTraversalPolicy gives it.
    const std::array<QWidget*, 6> order{m_showLegacy, m_foreBox,    m_aftBox,
                                        m_table,      m_alwaysOpen, m_close};
    QWidget*                      previous = m_filterText;
    for (QWidget* const next : order)
    {
        if (next != nullptr)
        {
            setTabOrder(previous, next);
            previous = next;
        }
    }
    m_filterText->setFocus(Qt::OtherFocusReason);
}

void ComponentPresetChooserDialog::applyInitialGeometry(QWidget* parent)
{
    m_geometry =
        makeChild<WindowGeometry>(this, *m_preferences, QString::fromLatin1(kGeometryName));
    if (m_geometry->restored())
    {
        return;
    }
    // owner.getParent(): the window of the window the parent belongs to.
    const QWidget* const owner       = parent == nullptr ? nullptr : parent->window();
    const QWidget* const ownerParent = owner == nullptr || owner->parentWidget() == nullptr
                                           ? nullptr
                                           : owner->parentWidget()->window();
    if (ownerParent == nullptr)
    {
        // pack(): the contents' size.
        adjustSize();
        return;
    }
    // (int) (0.7 * width), (int) (0.7 * height).
    resize(static_cast<int>(kParentShare * ownerParent->width()),
           static_cast<int>(kParentShare * ownerParent->height()));
}

void ComponentPresetChooserDialog::updateFilters()
{
    // The text and the diameters ANDed with the legacy filter, which the table keeps with its
    // Legacy column. Only what changed is set, so that each change filters the rows once, as
    // Java's one setRowFilter() does.
    ComponentPresetFilterModel& filter = m_table->filterModel();
    const QString               text   = m_filterText->text();
    if (text != filter.textFilter())
    {
        filter.setTextFilter(text);
    }
    const std::optional<double> aft =
        m_aftBox != nullptr && m_aftBox->isChecked() ? m_request.aftDiameter : std::nullopt;
    if (aft != filter.aftDiameterFilter())
    {
        filter.setAftDiameterFilter(aft);
    }
    const std::optional<double> fore =
        m_foreBox != nullptr && m_foreBox->isChecked() ? m_request.foreDiameter : std::nullopt;
    if (fore != filter.foreDiameterFilter())
    {
        filter.setForeDiameterFilter(fore);
    }
}

void ComponentPresetChooserDialog::closeWithSelection()
{
    m_table->storeUiPreferences(m_tableName);
    std::shared_ptr<const ComponentPreset> preset = m_table->selectedPreset();
    if (preset != nullptr)
    {
        m_answer = {.outcome = UserPrompts::PresetOutcome::CHOSEN, .preset = std::move(preset)};
    }
    else
    {
        m_answer = {.outcome = UserPrompts::PresetOutcome::NONE, .preset = nullptr};
    }
    accept();
}

void ComponentPresetChooserDialog::applyDoubleClicked(const QModelIndex& index)
{
    // Don't do anything when double-clicking the first column (OpenRocket's comment): the first
    // shown one.
    if (m_table->viewColumn(index.column()) <= 0)
    {
        return;
    }
    std::shared_ptr<const ComponentPreset> preset = m_table->selectedPreset();
    if (preset == nullptr)
    {
        return;
    }
    m_answer = {.outcome = UserPrompts::PresetOutcome::CHOSEN, .preset = std::move(preset)};
    accept();
}

}  // namespace QtRocket::Gui
