#include "QtRocket/gui/dialogs/motor/ThrustCurveMotorSelectionPanel.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <QAbstractItemView>
#include <QApplication>
#include <QBrush>
#include <QButtonGroup>
#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QEvent>
#include <QFont>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QList>
#include <QModelIndex>
#include <QMouseEvent>
#include <QObject>
#include <QPalette>
#include <QPoint>
#include <QRadioButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QString>
#include <QStyle>
#include <QStyleOptionViewItem>
#include <QStyledItemDelegate>
#include <QTabWidget>
#include <QTableView>
#include <QVariant>
#include <QWidget>
#include <Qt>

#include "QtRocket/document/MotorChooserInput.h"
#include "QtRocket/gui/binding/DoubleBinding.h"
#include "QtRocket/gui/dialogs/motor/MotorFilterPanel.h"
#include "QtRocket/gui/dialogs/motor/MotorInformationPanel.h"
#include "QtRocket/gui/dialogs/motor/MotorTableModel.h"
#include "QtRocket/gui/plot/PlotColors.h"
#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/gui/util/GuiServices.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/util/ThemeColors.h"
#include "QtRocket/gui/widgets/StyledLabel.h"
#include "QtRocket/gui/widgets/UnitSelector.h"
#include "QtRocket/gui/widgets/UnitSpinBox.h"
#include "QtRocket/motor/ThrustCurveMotor.h"
#include "QtRocket/motor/ThrustCurveMotorSet.h"
#include "QtRocket/motor/chooser/MotorHolder.h"
#include "QtRocket/motor/chooser/MotorRowFilter.h"
#include "QtRocket/motor/chooser/ThrustCurveMotorDatabaseModel.h"
#include "QtRocket/motor/chooser/ThrustCurveMotorSelection.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/MotorMount.h"
#include "QtRocket/unit/UnitGroup.h"

namespace QtRocket::Gui
{

namespace
{

/// MigLayout's paragraph gap ("gapleft para", "gap para para").
constexpr int kParagraphGap = 12;
/// The points the check boxes are smaller (GUIUtil.changeFontSize(box, -1)), the hints below the
/// delay and the nozzle (new StyledLabel(text, -3)) and the information row (StyledLabel(-2)).
constexpr double kCheckBoxFontChange = -1.0;
constexpr double kHintFontChange     = -3.0;
constexpr double kInfoFontChange     = -2.0;
/// The nozzle spin box's step in its unit (getSpinnerModel(0.1)).
constexpr double kNozzleStep = 0.1;
/// The table's least height ("height 80::").
constexpr int kTableMinimumHeight = 80;

/// The context of the panel's guarded slots.
[[nodiscard]] QString context()
{
    return QStringLiteral("Motor selection panel");
}

/// Changes @p widget's font by @p points points (GUIUtil.changeFontSize()).
void changeFontSize(QWidget& widget, double points)
{
    QFont font = widget.font();
    if (font.pointSizeF() > 0.0)
    {
        font.setPointSizeF(std::max(1.0, font.pointSizeF() + points));
    }
    else
    {
        // A font sized in pixels: as many pixels as points.
        font.setPixelSize(std::max(1, font.pixelSize() + static_cast<int>(points)));
    }
    widget.setFont(font);
}

/// @p widget in a row of its own, after the paragraph gap ("gapleft para"): the row's widget.
[[nodiscard]] QWidget& indented(QWidget& parent, QWidget& widget)
{
    auto* row    = makeChild<QWidget>(&parent);
    auto* layout = makeChild<QHBoxLayout>(row);
    layout->setContentsMargins(kParagraphGap, 0, 0, 0);
    widget.setParent(row);
    layout->addWidget(&widget, 1);
    return *row;
}

/// The curve list's item delegate (Java's CurveSelectionRenderer): an item is drawn in its colour
/// (its foreground role), a selected one on its colour, its text in the list's background.
class CurveItemDelegate final : public QStyledItemDelegate
{
public:
    explicit CurveItemDelegate(QObject* parent) : QStyledItemDelegate(parent) { }

protected:
    void initStyleOption(QStyleOptionViewItem* option, const QModelIndex& index) const override
    {
        QStyledItemDelegate::initStyleOption(option, index);
        const QVariant color = index.data(Qt::ForegroundRole);
        if (color.isValid() && option->state.testFlag(QStyle::State_Selected))
        {
            const QBrush background = option->palette.base();
            option->palette.setBrush(QPalette::Highlight, qvariant_cast<QBrush>(color));
            option->palette.setBrush(QPalette::HighlightedText, background);
        }
    }
};

}  // namespace

ThrustCurveMotorSelectionPanel::ThrustCurveMotorSelectionPanel(const GuiServices& services,
                                                               QWidget*           parent)
  : QWidget(parent),
    m_services(services),
    m_selection(
        std::make_unique<ThrustCurveMotorSelection>(services.motors(), services.preferences())),
    m_tableModel(
        makeChild<MotorTableModel>(this, *m_selection, std::as_const(services.preferences()))),
    //// MotorFilter: its onSelectionChanged() sorts the table and scrolls to the selection.
    m_motorFilterPanel(makeChild<MotorFilterPanel>(this, *m_selection, services.preferences())),
    m_motorInformationPanel(makeChild<MotorInformationPanel>(this, services)),
    m_nozzleExitDiameterModel(
        std::make_shared<DoubleBinding>(0.0, unitGroup(UnitGroupId::LENGTH), 0.0))
{
    setObjectName(QStringLiteral("selectionPanel"));
    m_selection->setDelayTexts(
        //: TCMotorSelPan.delayBox.Plugged
        QtStrings::toUtf8(tr("Plugged")),
        //: TCMotorSelPan.delayBox.PluggedNone
        QtStrings::toUtf8(tr("None")));
    connect(m_motorFilterPanel, &MotorFilterPanel::filterChanged, this,
            guarded([this] { filterChanged(); }, context()));

    ////  GUI: the left column in a scroll pane without a border.
    auto* scrollPane = makeChild<QScrollArea>(this);
    scrollPane->setObjectName(QStringLiteral("scrollPane"));
    scrollPane->setFrameShape(QFrame::NoFrame);
    scrollPane->setWidgetResizable(true);
    scrollPane->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scrollPane->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scrollPane->setMinimumSize(0, 0);
    auto* panel = makeChild<QWidget>(scrollPane);
    panel->setMinimumSize(0, 0);
    // MigLayout("fill", "[][grow]").
    auto* grid = makeChild<QGridLayout>(panel);
    grid->setColumnStretch(1, 1);
    makeCurveRow(*panel, *grid);
    makeDelayRows(*panel, *grid);
    makeNozzleRows(*panel, *grid);
    makeHideBoxes(*panel, *grid);
    makeNameColumnRow(*panel, *grid);
    makeTable(*panel, *grid);
    makeInfoRow(*panel, *grid);
    makeSearchRow(*panel, *grid);
    scrollPane->setWidget(panel);

    // Vertical split, then the tabs.
    auto* separator = makeChild<QFrame>(this);
    separator->setFrameShape(QFrame::VLine);
    separator->setFrameShadow(QFrame::Sunken);
    auto* rightSide = makeChild<QTabWidget>(this);
    rightSide->setObjectName(QStringLiteral("rightSide"));
    rightSide->setMinimumSize(0, 0);
    //: TCMotorSelPan.btn.filter
    rightSide->addTab(m_motorFilterPanel, tr("Filter Motors"));
    //: TCMotorSelPan.btn.details
    rightSide->addTab(m_motorInformationPanel, tr("Show Details"));

    auto* layout = makeChild<QHBoxLayout>(this);
    layout->addWidget(scrollPane, 1);
    layout->addSpacing(kParagraphGap);
    layout->addWidget(separator);
    layout->addSpacing(kParagraphGap);
    layout->addWidget(rightSide, 1);

    // Update the panel data
    updateData();
    showDelays();
    updateDimColors();
    // The listeners of the two boxes, once: each stores its box (as read) and applies it.
    m_motorFilterPanel->setHideUnavailable(m_hideUnavailableBox->isChecked());
    m_selection->setHideSimilar(m_hideSimilarBox->isChecked());
    updateData();
}

ThrustCurveMotorSelectionPanel::~ThrustCurveMotorSelectionPanel()
{
    // The children are deleted after this object's members (the selection among them): none of
    // their signals may reach the panel by then (the delay editor's focus loss, a held slider).
    const QList<QObject*> children = findChildren<QObject*>();
    for (const QObject* child : children)
    {
        QObject::disconnect(child, nullptr, this, nullptr);
    }
}

// ------------------------------------------------------------------------------ the rows

void ThrustCurveMotorSelectionPanel::makeCurveRow(QWidget& panel, QGridLayout& grid)
{
    //: TCMotorSelPan.lbl.Selectthrustcurve
    m_curveSelectionLabel = makeChild<QLabel>(&panel, tr("Select thrust curve:"));
    m_curveSelectionLabel->setObjectName(QStringLiteral("curveSelectionLabel"));
    m_curveSelectionBox = makeChild<QComboBox>(&panel);
    m_curveSelectionBox->setObjectName(QStringLiteral("curveSelectionBox"));
    // Not Qt::WheelFocus (QComboBox's): the wheel must not take the focus.
    m_curveSelectionBox->setFocusPolicy(Qt::StrongFocus);
    m_curveSelectionBox->setItemDelegate(makeChild<CurveItemDelegate>(m_curveSelectionBox));
    m_curveSelectionBox->installEventFilter(this);
    // The user's choice only: the list's own refill selects nothing (bug 20).
    connect(m_curveSelectionBox, &QComboBox::activated, this,
            guarded([this](int index) { curveChosen(index); }, context()));
    grid.addWidget(m_curveSelectionLabel, 0, 0);
    grid.addWidget(m_curveSelectionBox, 0, 1);
}

void ThrustCurveMotorSelectionPanel::makeDelayRows(QWidget& panel, QGridLayout& grid)
{
    //: TCMotorSelPan.lbl.Ejectionchargedelay
    m_ejectionChargeDelayLabel = makeChild<QLabel>(&panel, tr("Ejection charge delay:"));
    m_ejectionChargeDelayLabel->setObjectName(QStringLiteral("ejectionChargeDelayLabel"));
    m_delayBox = makeChild<QComboBox>(&panel);
    m_delayBox->setObjectName(QStringLiteral("delayBox"));
    m_delayBox->setEditable(true);
    m_delayBox->setInsertPolicy(QComboBox::NoInsert);
    // Java's editable combo box completes nothing.
    m_delayBox->setCompleter(nullptr);
    m_delayBox->setFocusPolicy(Qt::StrongFocus);
    m_delayBox->installEventFilter(this);
    connect(m_delayBox, &QComboBox::activated, this,
            guarded([this](int index) { delayItemChosen(index); }, context()));
    connect(m_delayBox->lineEdit(), &QLineEdit::returnPressed, this,
            guarded([this] { delayEntered(); }, context()));
    connect(m_delayBox->lineEdit(), &QLineEdit::editingFinished, this,
            guarded([this] { delayEditFinished(); }, context()));
    grid.addWidget(m_ejectionChargeDelayLabel, 1, 0);
    grid.addWidget(m_delayBox, 1, 1);
    //// (or type in desired delay in seconds)
    //: TCMotorSelPan.lbl.Numberofseconds
    grid.addWidget(makeChild<StyledLabel>(&panel, tr("(or type in desired delay in seconds)"),
                                          kHintFontChange),
                   2, 1);
}

void ThrustCurveMotorSelectionPanel::makeNozzleRows(QWidget& panel, QGridLayout& grid)
{
    //: TCMotorSelPan.lbl.Nozzleexitdiameter
    m_nozzleExitDiameterLabel = makeChild<QLabel>(&panel, tr("Nozzle exit diameter:"));
    m_nozzleExitDiameterLabel->setObjectName(QStringLiteral("nozzleExitDiameterLabel"));
    auto* fields = makeChild<QWidget>(&panel);
    auto* row    = makeChild<QHBoxLayout>(fields);
    row->setContentsMargins(0, 0, 0, 0);
    m_nozzleExitDiameterSpinner = makeChild<UnitSpinBox>(fields, m_nozzleExitDiameterModel);
    m_nozzleExitDiameterSpinner->setObjectName(QStringLiteral("nozzleExitDiameterSpinner"));
    m_nozzleExitDiameterSpinner->setFixedStep(kNozzleStep);
    m_nozzleExitDiameterUnitSelector = makeChild<UnitSelector>(fields, m_nozzleExitDiameterModel);
    m_nozzleExitDiameterUnitSelector->setObjectName(
        QStringLiteral("nozzleExitDiameterUnitSelector"));
    row->addWidget(m_nozzleExitDiameterSpinner, 1);
    row->addWidget(m_nozzleExitDiameterUnitSelector);
    m_nozzleConnection = m_nozzleExitDiameterModel->addChangeListener(
        [this] { Guard::run([this] { nozzleChanged(); }, context()); });
    grid.addWidget(m_nozzleExitDiameterLabel, 3, 0);
    grid.addWidget(fields, 3, 1);
    grid.addWidget(
        makeChild<StyledLabel>(
            &panel,
            //: TCMotorSelPan.lbl.Nozzleexitdiameter.desc
            tr("Enter zero when unknown; no powered base-drag correction is applied for this "
               "motor."),
            kHintFontChange),
        4, 1);
}

void ThrustCurveMotorSelectionPanel::makeHideBoxes(QWidget& panel, QGridLayout& grid)
{
    //// Hide very similar thrust curves
    //: TCMotorSelPan.checkbox.hideSimilar
    m_hideSimilarBox = makeChild<QCheckBox>(&panel, tr("Hide very similar thrust curves"));
    m_hideSimilarBox->setObjectName(QStringLiteral("hideSimilarBox"));
    changeFontSize(*m_hideSimilarBox, kCheckBoxFontChange);
    m_hideSimilarBox->setChecked(m_selection->isHideSimilar());
    connect(m_hideSimilarBox, &QCheckBox::clicked, this,
            guarded(
                [this](bool checked) {
                    m_selection->setHideSimilar(checked);
                    updateData();
                },
                context()));
    grid.addWidget(&indented(panel, *m_hideSimilarBox), 5, 0, 1, 2);

    //// Hide unavailable motors
    //: TCMotorSelPan.checkbox.hideUnavailable
    m_hideUnavailableBox =
        makeChild<QCheckBox>(&panel, tr("Hide motors which are not in regular production"));
    m_hideUnavailableBox->setObjectName(QStringLiteral("hideUnavailableBox"));
    changeFontSize(*m_hideUnavailableBox, kCheckBoxFontChange);
    m_hideUnavailableBox->setChecked(m_selection->isHideUnavailable());
    connect(m_hideUnavailableBox, &QCheckBox::clicked, this,
            guarded([this](bool checked) { m_motorFilterPanel->setHideUnavailable(checked); },
                    context()));
    grid.addWidget(&indented(panel, *m_hideUnavailableBox), 6, 0, 1, 2);
}

void ThrustCurveMotorSelectionPanel::makeNameColumnRow(QWidget& panel, QGridLayout& grid)
{
    //: TCMotorSelPan.lbl.motorNameColumn
    auto* motorNameColumn = makeChild<QLabel>(&panel, tr("Motor name column:"));
    motorNameColumn->setObjectName(QStringLiteral("motorNameColumn"));
    //: TCMotorSelPan.lbl.motorNameColumn.ttip
    motorNameColumn->setToolTip(
        tr("Select which motor property to display in the motor name "
           "column."));
    auto* buttons = makeChild<QWidget>(&panel);
    auto* row     = makeChild<QHBoxLayout>(buttons);
    row->setContentsMargins(0, 0, 0, 0);
    //: TCMotorSelPan.btn.commonName
    m_commonName = makeChild<QRadioButton>(buttons, tr("Use common name"));
    m_commonName->setObjectName(QStringLiteral("commonName"));
    //: TCMotorSelPan.btn.designation
    m_designation = makeChild<QRadioButton>(buttons, tr("Use manufacturer's designation"));
    m_designation->setObjectName(QStringLiteral("designation"));
    auto* group = makeChild<QButtonGroup>(buttons);
    group->addButton(m_commonName);
    group->addButton(m_designation);
    connect(m_commonName, &QRadioButton::clicked, this,
            guarded([this] { nameColumnClicked(false); }, context()));
    connect(m_designation, &QRadioButton::clicked, this,
            guarded([this] { nameColumnClicked(true); }, context()));
    const bool initValue = m_services.preferences().getMotorNameColumn();
    m_commonName->setChecked(!initValue);
    m_designation->setChecked(initValue);
    row->addWidget(m_commonName);
    row->addWidget(m_designation);
    row->addStretch(1);
    grid.addWidget(&indented(panel, *motorNameColumn), 7, 0);
    grid.addWidget(buttons, 7, 1);
}

void ThrustCurveMotorSelectionPanel::makeTable(QWidget& panel, QGridLayout& grid)
{
    m_table = makeChild<QTableView>(&panel);
    m_table->setObjectName(QStringLiteral("table"));
    m_table->setModel(m_tableModel);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    // The rows are the core's sort (MotorTableModel); the header toggles its keys.
    m_table->setSortingEnabled(false);
    m_table->verticalHeader()->hide();
    QHeaderView* header = m_table->horizontalHeader();
    m_tableModel->connectHeader(*header);
    for (int column = 0; column < MotorTableModel::kColumnCount; ++column)
    {
        header->resizeSection(column, MotorTableModel::preferredWidth(column));
    }
    header->setStretchLastSection(true);
    m_table->setMinimumHeight(kTableMinimumHeight);
    m_table->viewport()->installEventFilter(this);
    // Set selection and double-click listeners
    connect(m_table->selectionModel(), &QItemSelectionModel::selectionChanged, this,
            guarded([this] { selectMotorFromTable(); }, context()));
    grid.addWidget(m_table, 8, 0, 1, 2);
    grid.setRowStretch(8, 1);
}

void ThrustCurveMotorSelectionPanel::makeInfoRow(QWidget& panel, QGridLayout& grid)
{
    auto* infoRow = makeChild<QWidget>(&panel);
    infoRow->setObjectName(QStringLiteral("infoRow"));
    auto* row = makeChild<QHBoxLayout>(infoRow);
    row->setContentsMargins(kParagraphGap, 0, 0, 0);
    m_nrOfMotorsLabel =
        makeChild<StyledLabel>(infoRow, QString(), kInfoFontChange, StyledLabel::Style::ITALIC);
    m_nrOfMotorsLabel->setObjectName(QStringLiteral("nrOfMotorsLabel"));
    //: TCMotorSelPan.lbl.ttip.nrOfMotors
    m_nrOfMotorsLabel->setToolTip(
        tr("Number of motors currently visible in the motor selection "
           "table"));
    m_motorDbVersionLabel =
        makeChild<StyledLabel>(infoRow, QString(), kInfoFontChange, StyledLabel::Style::ITALIC);
    m_motorDbVersionLabel->setObjectName(QStringLiteral("motorDbVersionLabel"));
    row->addWidget(m_nrOfMotorsLabel, 1);
    row->addWidget(m_motorDbVersionLabel);
    updateNrOfMotors();
    updateMotorDatabaseVersion();
    // sorterChanged and the filter's change event: every new order of the rows.
    connect(m_tableModel, &QAbstractItemModel::layoutChanged, this,
            guarded([this] { updateNrOfMotors(); }, context()));
    connect(m_tableModel, &QAbstractItemModel::modelReset, this,
            guarded([this] { updateNrOfMotors(); }, context()));
    grid.addWidget(infoRow, 9, 0, 1, 2);
}

void ThrustCurveMotorSelectionPanel::makeSearchRow(QWidget& panel, QGridLayout& grid)
{
    //// Search:
    //: TCMotorSelPan.lbl.Search
    grid.addWidget(makeChild<StyledLabel>(&panel, tr("Search:")), 10, 0);
    m_searchField = makeChild<QLineEdit>(&panel);
    m_searchField->setObjectName(QStringLiteral("searchField"));
    connect(m_searchField, &QLineEdit::textChanged, this,
            guarded([this](const QString& text) { searchChanged(text); }, context()));
    grid.addWidget(m_searchField, 10, 1);
}

// ------------------------------------------------------------------------------ the selection

void ThrustCurveMotorSelectionPanel::setMotorMountAndConfig(const FlightConfigurationId& fcid,
                                                            const MotorMount&            mount)
{
    const ThrustCurveMotorSelection::MountConfiguration configuration =
        MotorChooserInput::fromMount(mount, fcid);
    if (configuration.motor != nullptr)
    {
        // QtRocket's fix (see the class comment): the row an earlier question selected goes, so
        // that it cannot replace the configured motor when the mount's limits hide its set
        // (OpenRocket's table keeps it selected, and its selection listener selects it). The
        // table's selection handler finds no row (selectMotorFromTable()).
        m_table->clearSelection();
    }
    m_selection->setMotorMountAndConfig(configuration);
    // motorFilterPanel.setMotorMount(mountToEdit): the table sorted again for the mount (and for
    // a set of its own of a motor of no set).
    m_motorFilterPanel->setMotorMount(&mount);
    if (m_selection->getSelectedMotor() != nullptr)
    {
        // select(motorToSelect): the new set's curves, details and delays.
        updateData();
        showDelays();
    }
    showNozzle();
    scrollSelectionVisible();
}

void ThrustCurveMotorSelectionPanel::selectMotorFromTable()
{
    const QModelIndexList rows = m_table->selectionModel()->selectedRows();
    if (rows.isEmpty())
    {
        return;
    }
    const ThrustCurveMotorSet&       set      = m_tableModel->motorSet(rows.front().row());
    const std::optional<std::size_t> modelRow = m_selection->getModel().getIndex(set);
    if (!modelRow.has_value())
    {
        return;
    }
    const ThrustCurveMotorSet* previousSet   = m_selection->getSelectedMotorSet();
    const auto                 previousMotor = m_selection->getSelectedMotor();
    m_selection->selectMotorFromTable(*modelRow);
    if (m_selection->getSelectedMotor() != previousMotor)
    {
        motorSelected(previousSet);
    }
}

std::shared_ptr<const ThrustCurveMotor> ThrustCurveMotorSelectionPanel::getSelectedMotor() const
{
    return m_selection->getSelectedMotor();
}

double ThrustCurveMotorSelectionPanel::getSelectedDelay() const
{
    return m_selection->getSelectedDelay();
}

double ThrustCurveMotorSelectionPanel::getSelectedNozzleExitDiameter() const
{
    return m_selection->getNozzleExitDiameter();
}

void ThrustCurveMotorSelectionPanel::selectedMotor(const ThrustCurveMotor& motor)
{
    m_selection->storeSelectedMotor(motor);
}

QWidget& ThrustCurveMotorSelectionPanel::getDefaultFocus() const noexcept
{
    return *m_searchField;
}

void ThrustCurveMotorSelectionPanel::commitEdits()
{
    delayEditFinished();
    static_cast<void>(m_nozzleExitDiameterSpinner->commitEdit());
}

void ThrustCurveMotorSelectionPanel::setMotorDatabaseVersion(std::optional<std::int64_t> version)
{
    m_motorDatabaseVersion = version;
    updateMotorDatabaseVersion();
}

void ThrustCurveMotorSelectionPanel::curveChosen(int index)
{
    if (index < 0 || static_cast<std::size_t>(index) >= m_curves.size())
    {
        return;
    }
    // A copy: the list refills from the selection.
    const std::shared_ptr<const ThrustCurveMotor> motor =
        m_curves.at(static_cast<std::size_t>(index));
    if (motor == m_selection->getSelectedMotor())
    {
        return;
    }
    const ThrustCurveMotorSet* previousSet = m_selection->getSelectedMotorSet();
    m_selection->select(motor);
    motorSelected(previousSet);
}

void ThrustCurveMotorSelectionPanel::motorSelected(const ThrustCurveMotorSet* previousSet)
{
    showNozzle();
    updateData();
    if (m_selection->getSelectedMotorSet() != previousSet)
    {
        showDelays();
    }
    scrollSelectionVisible();
}

void ThrustCurveMotorSelectionPanel::updateData()
{
    if (m_selection->getSelectedMotorSet() == nullptr)
    {
        // No motor selected
        {
            const QSignalBlocker blocker(m_curveSelectionBox);
            m_curveSelectionBox->clear();
        }
        m_curves.clear();
        m_curveSelectionBox->setEnabled(false);
        m_curveSelectionLabel->setEnabled(false);
        setSelectionWidgetsEnabled(false);
        m_motorInformationPanel->clearData();
        m_table->clearSelection();
        return;
    }
    setSelectionWidgetsEnabled(true);
    // Check which thrust curves to display
    fillCurveList();
    const bool several = m_curves.size() > 1;
    m_curveSelectionBox->setEnabled(several);
    m_curveSelectionLabel->setEnabled(several);
    m_motorInformationPanel->updateData(m_curves, m_selection->getSelectedMotor());
}

void ThrustCurveMotorSelectionPanel::fillCurveList()
{
    const std::vector<MotorHolder> items = m_selection->getCurveItems();
    m_curves.clear();
    {
        // Bug 20: the refill selects nothing (OpenRocket's addElement() and setSelectedIndex()
        // fire the box's action, which selects the first curve).
        const QSignalBlocker blocker(m_curveSelectionBox);
        m_curveSelectionBox->clear();
        for (const MotorHolder& item : items)
        {
            m_curves.push_back(item.getMotor());
            m_curveSelectionBox->addItem(
                QtStrings::fromUtf8(item.toString(m_services.preferences())));
        }
        const auto found = std::ranges::find(m_curves, m_selection->getSelectedMotor());
        m_curveSelectionBox->setCurrentIndex(
            found == m_curves.end() ? -1
                                    : static_cast<int>(std::distance(m_curves.begin(), found)));
    }
    updateCurveColors();
}

void ThrustCurveMotorSelectionPanel::updateCurveColors()
{
    const QPalette& colors = palette();
    {
        const QSignalBlocker blocker(m_curveSelectionBox);
        for (int index = 0; index < m_curveSelectionBox->count(); ++index)
        {
            m_curveSelectionBox->setItemData(index, PlotColors::curveColor(index, colors),
                                             Qt::ForegroundRole);
        }
    }
    // The closed box: the selected curve's colour (the renderer's unselected item).
    QPalette  box     = m_curveSelectionBox->palette();
    const int current = m_curveSelectionBox->currentIndex();
    for (const QPalette::ColorGroup group : {QPalette::Active, QPalette::Inactive})
    {
        const QColor text = current < 0 ? colors.color(group, QPalette::ButtonText)
                                        : PlotColors::curveColor(current, colors);
        box.setColor(group, QPalette::ButtonText, text);
        box.setColor(group, QPalette::Text,
                     current < 0 ? colors.color(group, QPalette::Text) : text);
    }
    m_curveSelectionBox->setPalette(box);
}

// ------------------------------------------------------------------------------ the delay

void ThrustCurveMotorSelectionPanel::showDelays()
{
    const QSignalBlocker blocker(m_delayBox);
    m_delayBox->clear();
    for (const std::string& item : m_selection->getDelayItems())
    {
        m_delayBox->addItem(QtStrings::fromUtf8(item));
    }
    if (m_selection->getSelectedMotor() == nullptr)
    {
        //// Display nothing
        m_delaySelected.reset();
        m_delayBox->setCurrentIndex(-1);
        m_delayBox->setEditText(QString());
        return;
    }
    const QString text = QtStrings::fromUtf8(m_selection->getDelayText());
    m_delaySelected    = text;
    m_delayBox->setCurrentIndex(m_delayBox->findText(text, Qt::MatchExactly));
    m_delayBox->setEditText(text);
}

void ThrustCurveMotorSelectionPanel::delayAction(const std::optional<QString>& selected)
{
    if (!selected.has_value())
    {
        // "Selected charge delay is null"
        return;
    }
    //// Plugged (or None), or Java's parseDouble; text it cannot read keeps the delay.
    m_selection->setDelayText(QtStrings::toUtf8(*selected));
    showDelays();
}

void ThrustCurveMotorSelectionPanel::delayItemChosen(int index)
{
    if (index < 0)
    {
        return;
    }
    // setSelectedIndex(): one action event, also for the item already selected.
    m_delaySelected = m_delayBox->itemText(index);
    delayAction(m_delaySelected);
}

void ThrustCurveMotorSelectionPanel::delayEntered()
{
    // The editor's action: the typed text becomes the selected item (an action event when it is
    // another), then the box's "comboBoxEdited" action event reads the selected item.
    const QString typed = m_delayBox->lineEdit()->text();
    if (m_delaySelected != typed)
    {
        m_delaySelected = typed;
        delayAction(m_delaySelected);
    }
    delayAction(m_delaySelected);
}

void ThrustCurveMotorSelectionPanel::delayEditFinished()
{
    // The focus leaves the editor: Enter's action when the typed text is not the selected item.
    if (m_delaySelected == m_delayBox->lineEdit()->text())
    {
        return;
    }
    delayEntered();
}

// ------------------------------------------------------------------------------ the nozzle

void ThrustCurveMotorSelectionPanel::showNozzle()
{
    m_nozzleExitDiameterModel->setValue(m_selection->getNozzleExitDiameter());
}

void ThrustCurveMotorSelectionPanel::nozzleChanged()
{
    // The DoubleModel's bound: at most the selected curve's diameter.
    const double value = m_nozzleExitDiameterModel->value();
    m_selection->setNozzleExitDiameter(value);
    const double kept = m_selection->getNozzleExitDiameter();
    if (kept != value)
    {
        m_nozzleExitDiameterModel->setValue(kept);
    }
}

void ThrustCurveMotorSelectionPanel::setSelectionWidgetsEnabled(bool enabled)
{
    m_ejectionChargeDelayLabel->setEnabled(enabled);
    m_delayBox->setEnabled(enabled);
    m_nozzleExitDiameterLabel->setEnabled(enabled);
    m_nozzleExitDiameterSpinner->setFieldEnabled(enabled);
    m_nozzleExitDiameterUnitSelector->setFieldEnabled(enabled);
}

// ------------------------------------------------------------------------------ the table

void ThrustCurveMotorSelectionPanel::nameColumnClicked(bool designation)
{
    m_services.preferences().setMotorNameColumn(designation);
    // Bug 10: the table's rows sorted again keep the selection on its set (OpenRocket re-selects
    // the row of the same index).
    m_tableModel->refresh();
    {
        const QSignalBlocker blocker(m_curveSelectionBox);
        for (std::size_t index = 0; index < m_curves.size(); ++index)
        {
            m_curveSelectionBox->setItemText(
                static_cast<int>(index),
                QtStrings::fromUtf8(
                    MotorHolder(m_curves[index], index).toString(m_services.preferences())));
        }
    }
    scrollSelectionVisible();
}

void ThrustCurveMotorSelectionPanel::searchChanged(const QString& text)
{
    m_selection->getRowFilter().setSearchText(QtStrings::toUtf8(text));
    m_tableModel->refresh();
    scrollSelectionVisible();
}

void ThrustCurveMotorSelectionPanel::filterChanged()
{
    m_tableModel->refresh();
    scrollSelectionVisible();
}

void ThrustCurveMotorSelectionPanel::scrollSelectionVisible()
{
    const ThrustCurveMotorSet* set = m_selection->getSelectedMotorSet();
    if (set == nullptr)
    {
        return;
    }
    // -1: filtered out, or not in the table's sets yet.
    const int row = m_tableModel->rowOf(*set);
    if (row < 0)
    {
        return;
    }
    const QModelIndex index = m_tableModel->index(row, 0);
    m_table->selectionModel()->setCurrentIndex(
        index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    m_table->scrollTo(index, QAbstractItemView::EnsureVisible);
}

void ThrustCurveMotorSelectionPanel::updateNrOfMotors()
{
    const int rowCount = m_tableModel->rowCount();
    //: TCMotorSelPan.lbl.nrOfMotors.None
    const QString motorCount = rowCount > 0 ? QString::number(rowCount) : tr("None");
    //: TCMotorSelPan.lbl.nrOfMotors
    m_nrOfMotorsLabel->setText(tr("Number of motors:") + QStringLiteral(" ") + motorCount);
}

void ThrustCurveMotorSelectionPanel::updateMotorDatabaseVersion()
{
    const QString versionText = m_motorDatabaseVersion.has_value() && *m_motorDatabaseVersion > 0
                                    ? QString::number(*m_motorDatabaseVersion)
                                    //: TCMotorSelPan.lbl.motorDbVersion.Unknown
                                    : tr("Unknown");
    //: TCMotorSelPan.lbl.motorDbVersion
    m_motorDbVersionLabel->setText(tr("Motor DB version:") + QStringLiteral(" ") + versionText);
}

void ThrustCurveMotorSelectionPanel::updateDimColors()
{
    const QColor dim = ThemeColors::color(ThemeColors::Key::TEXT_DIM, palette());
    for (QLabel* label : {m_nrOfMotorsLabel, m_motorDbVersionLabel})
    {
        QPalette colors = label->palette();
        colors.setColor(QPalette::WindowText, dim);
        label->setPalette(colors);
    }
}

// ------------------------------------------------------------------------------ events

bool ThrustCurveMotorSelectionPanel::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::Wheel &&
        ((watched == m_curveSelectionBox && !m_curveSelectionBox->hasFocus()) ||
         (watched == m_delayBox && !m_delayBox->hasFocus())))
    {
        // On to the widgets around it (Java's combo boxes take no wheel).
        event->ignore();
        return true;
    }
    if (watched == m_table->viewport())
    {
        const auto* mouse = dynamic_cast<const QMouseEvent*>(event);
        switch (mouse == nullptr ? QEvent::None : event->type())
        {
            case QEvent::MouseButtonPress:
                m_doubleClickPosition.reset();
                break;
            case QEvent::MouseButtonDblClick:
                if (mouse->button() == Qt::LeftButton)
                {
                    m_doubleClickPosition = mouse->position().toPoint();
                }
                break;
            case QEvent::MouseButtonRelease:
                if (mouse->button() == Qt::LeftButton && m_doubleClickPosition.has_value())
                {
                    const QPoint moved = mouse->position().toPoint() - *m_doubleClickPosition;
                    m_doubleClickPosition.reset();
                    if (moved.manhattanLength() < QApplication::startDragDistance())
                    {
                        Q_EMIT okRequested();
                    }
                }
                break;
            default:
                break;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void ThrustCurveMotorSelectionPanel::changeEvent(QEvent* event)
{
    QWidget::changeEvent(event);
    if (event->type() == QEvent::PaletteChange)
    {
        updateDimColors();
        updateCurveColors();
    }
}

}  // namespace QtRocket::Gui
