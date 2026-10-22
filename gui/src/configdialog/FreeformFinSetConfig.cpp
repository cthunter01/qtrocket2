#include "QtRocket/gui/configdialog/FreeformFinSetConfig.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include <QAbstractItemDelegate>
#include <QAbstractItemView>
#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QEvent>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QKeySequence>
#include <QLineEdit>
#include <QMenu>
#include <QModelIndex>
#include <QMouseEvent>
#include <QObject>
#include <QPoint>
#include <QPointer>
#include <QPushButton>
#include <QShortcut>
#include <QSplitter>
#include <QString>
#include <QStringList>
#include <QStyledItemDelegate>
#include <QTableView>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>
#include <Qt>

#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/gui/binding/ComponentSelection.h"
#include "QtRocket/gui/configdialog/FinPointTableModel.h"
#include "QtRocket/gui/configdialog/FinSetConfig.h"
#include "QtRocket/gui/figure/FinPointEditHistory.h"
#include "QtRocket/gui/figure/FinPointFigure.h"
#include "QtRocket/gui/figure/FinPointScrollArea.h"
#include "QtRocket/gui/figure/FinPointSelection.h"
#include "QtRocket/gui/figure/ScaleSelector.h"
#include "QtRocket/gui/figure/ScrollAreaZoomModel.h"
#include "QtRocket/gui/settings/GuiPreferenceKeys.h"
#include "QtRocket/gui/util/CustomFinImporter.h"
#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/gui/util/GuiServices.h"
#include "QtRocket/gui/util/Icons.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/util/UserPrompts.h"
#include "QtRocket/gui/widgets/DropdownButton.h"
#include "QtRocket/gui/widgets/StyledLabel.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/rocket/FreeformFinSet.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"
#include "QtRocket/util/Geometry2D.h"

namespace QtRocket::Gui
{

namespace
{

/// The gap before the right column of the General tab (Java: "gapright 40lp").
constexpr int kColumnGap = 40;

/// The rows of the General tab, and the row below them, which takes the extra height.
constexpr int kFinCountRow     = 0;
constexpr int kCantRow         = 1;
constexpr int kCrossSectionRow = 2;
constexpr int kThicknessRow    = 3;
constexpr int kRowCount        = 4;

/// The columns after ConfigFields' four: a gap, the right column.
constexpr int kSliderColumn = 3;
constexpr int kGapColumn    = 4;
constexpr int kRightColumn  = 5;

/// The hints' size relative to the default font (Java: StyledLabel(text, -2)).
constexpr double kHintSize = -2;
/// The splitter's least size (Java: "width 300lp:500lp:, height 100lp:250lp:").
constexpr int kSplitMinimumWidth  = 300;
constexpr int kSplitMinimumHeight = 100;
/// The gaps of the Shape tab (Java: "gap 5!", "gapleft para").
constexpr int kShapeGap     = 5;
constexpr int kParagraphGap = 12;

/// @p selection, whose main component must be a FreeformFinSet.
/// @throws BugError when it is not one (or the selection is null or invalid)
[[nodiscard]] std::shared_ptr<ComponentSelection> freeformSelection(
    std::shared_ptr<ComponentSelection> selection)
{
    if (selection != nullptr && selection->isValid() &&
        dynamic_cast<const FreeformFinSet*>(&selection->getMain()) == nullptr)
    {
        bug("The freeform fin set configuration needs a freeform fin set");
    }
    return selection;  // a null or invalid one is RocketComponentConfig's BugError
}

/// @p result of an edit whose index the editor chose itself: Java turns its failure into a
/// RuntimeException (a programming error here).
void expectSuccess(const Result<void>& result)
{
    if (!result.has_value())
    {
        bug("A fin point edit of the editor failed: " + result.error().message);
    }
}

/// The point table's delegate: Qt's, but an editor the user has typed in keeps its text when the
/// points change. Qt's view sets the text of every open editor again on dataChanged(), which the
/// table's refresh() emits for every cell; Java's JTable never sets an open editor's text again.
class FinPointCellDelegate final : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void setEditorData(QWidget* editor, const QModelIndex& index) const override
    {
        if (const auto* edit = qobject_cast<const QLineEdit*>(editor);
            edit != nullptr && edit->isModified())
        {
            return;
        }
        QStyledItemDelegate::setEditorData(editor, index);
    }
};

/// Whether @p type is an event of a mouse button or of its moves.
[[nodiscard]] bool isMouseEvent(QEvent::Type type) noexcept
{
    return type == QEvent::MouseButtonPress || type == QEvent::MouseButtonRelease ||
           type == QEvent::MouseButtonDblClick || type == QEvent::MouseMove;
}

/// @p text in lower case (ASCII; Locale.ENGLISH).
[[nodiscard]] std::string lowerCase(std::string_view text)
{
    std::string lower(text);
    std::ranges::transform(lower, lower.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return lower;
}

}  // namespace

FreeformFinSetConfig::FreeformFinSetConfig(OpenRocketDocument&                 document,
                                           const GuiServices&                  services,
                                           std::shared_ptr<ComponentSelection> selection,
                                           QWidget*                            parent)
  : FinSetConfig(document, services, freeformSelection(std::move(selection)), parent),
    m_finPoints(FinPointSelection::of(*this->selection()))
{
    QWidget& general = makeGeneralTab();
    QWidget& shape   = makeShapeTab();

    //// General and General properties
    //: FreeformFinSetCfg.tab.General
    insertTab(0, general, tr("General"),
              //: FreeformFinSetCfg.tab.ttip.General
              tr("General properties"));
    //// Shape and Fin shape
    //: FreeformFinSetCfg.tab.Shape
    insertTab(1, shape, tr("Shape"),
              //: FreeformFinSetCfg.tab.ttip.Finshape
              tr("Fin shape"));
    setSelectedTabIndex(0);

    addFinSetButtons();
}

// ---------------------------------------------------------------------------------- General

QWidget& FreeformFinSetConfig::makeGeneralTab()
{
    auto* page = makeChild<QWidget>(this);
    auto* grid = makeChild<QGridLayout>(page);

    //  Number of fins:
    //: FreeformFinSetCfg.lbl.Numberoffins
    addFinCountRow(*page, *grid, kFinCountRow, tr("Number of fins:"), QString());
    //  Fin cant
    //: FreeformFinSetCfg.lbl.Fincant
    addCantRow(*grid, kCantRow, tr("Fin cant:"),
               //: FreeformFinSetCfg.lbl.ttip.Fincant
               tr("The angle that the fins are canted with respect to the rocket body."));
    //  Cross section
    //: FreeformFinSetCfg.lbl.FincrossSection
    addCrossSectionRow(*page, *grid, kCrossSectionRow, tr("Fin cross section:"));
    //  Thickness:
    //: FreeformFinSetCfg.lbl.Thickness
    addThicknessRow(*grid, kThicknessRow, tr("Thickness:"));

    // Right side panel: the placement with the fin rotation (unbounded), the material and the
    // root fillets.
    grid->setColumnMinimumWidth(kGapColumn, kColumnGap);
    grid->addWidget(&makeRightColumn(*page, false), 0, kRightColumn, kRowCount + 1, 1,
                    Qt::AlignTop);
    grid->setColumnStretch(kSliderColumn, 1);
    grid->setRowStretch(kRowCount, 1);
    return *page;
}

// ---------------------------------------------------------------------------------- Shape

QWidget& FreeformFinSetConfig::makeShapeTab()
{
    auto* page = makeChild<QWidget>(this);
    m_shapeTab = page;
    page->setObjectName(QStringLiteral("shapeTab"));
    auto* layout = makeChild<QVBoxLayout>(page);
    layout->setSpacing(kShapeGap);

    // Create the figure
    const Preferences& preferences = services().preferences();
    m_figure                       = makeChild<FinPointFigure>(page, m_finPoints, preferences);
    m_area                         = makeChild<FinPointScrollArea>(page, m_figure, preferences);
    // Once the panel is released, the figure's mouse edits nothing (eventFilter()).
    m_figure->installEventFilter(this);

    // Create the table, the Point Actions button below it, the menus
    QWidget& pointList = makePointList(*page);
    makePointActions();

    // Remove focus from table when interacting on the figure: the cell editor commits on its
    // focus loss (Qt's delegate), then the selection is cleared.
    connect(m_area, &FinPointScrollArea::focusEntered, this,
            guarded([this] { selectPointRow(-1); }, QStringLiteral("finPointScrollArea")));
    connect(m_area, &FinPointScrollArea::refreshRequested, this,
            guarded([this] { updateFields(); }, QStringLiteral("finPointScrollArea")));
    connect(m_area, &FinPointScrollArea::editStateChanged, this,
            guarded([this] { updateFinPointUndoRedoControls(); },
                    QStringLiteral("finPointScrollArea")));
    connect(m_area, &FinPointScrollArea::pointSelectionCleared, this,
            guarded([this] { selectPointRow(-1); }, QStringLiteral("finPointScrollArea")));
    connect(
        m_area, &FinPointScrollArea::pointMenuRequested, this,
        guarded([this](int    index,
                       QPoint globalPosition) { figurePointMenuRequested(index, globalPosition); },
                QStringLiteral("finPointScrollArea")));

    // The zoom, fitting on first start-up
    m_zoomModel =
        makeChild<ScrollAreaZoomModel>(page, m_area, ScrollAreaZoomModel::Kind::USER_SCALE);
    m_selector = makeChild<ScaleSelector>(page, m_zoomModel);
    connect(
        m_area, &FinPointScrollArea::autoScrolled, this,
        guarded([this] { m_selector->updateZoomText(); }, QStringLiteral("finPointScrollArea")));
    m_area->setFitting(true);

    // first row: main display
    auto* splitPane = makeChild<QSplitter>(page, Qt::Horizontal);
    splitPane->setObjectName(QStringLiteral("finPointSplitter"));
    splitPane->addWidget(&pointList);
    splitPane->addWidget(m_area);
    // Java's resize weight 0.01: nearly all extra width goes to the figure.
    splitPane->setStretchFactor(0, 0);
    splitPane->setStretchFactor(1, 1);
    splitPane->setChildrenCollapsible(false);
    splitPane->setMinimumSize(kSplitMinimumWidth, kSplitMinimumHeight);
    layout->addWidget(splitPane, 1);

    // row of text directly below figure
    auto* hintRows = makeChild<QWidget>(page);
    auto* hints    = makeChild<QGridLayout>(hintRows);
    hints->setContentsMargins(0, 0, 0, 0);
    //: FreeformFinSetConfig.lbl.ctrlClick
    hints->addWidget(makeChild<StyledLabel>(hintRows, tr("Ctrl+click: Delete point"), kHintSize), 0,
                     0);
    //: FreeformFinSetConfig.lbl.clickDrag
    hints->addWidget(
        makeChild<StyledLabel>(hintRows, tr("Click+drag: Add and move points"), kHintSize), 0, 1);
    //: FreeformFinSetConfig.lbl.shiftClickDrag
    hints->addWidget(makeChild<StyledLabel>(
                         hintRows, tr("Shift+click+drag: Lock angle to previous point"), kHintSize),
                     1, 0);
    //: FreeformFinSetConfig.lbl.ctrlShiftClickDrag
    hints->addWidget(
        makeChild<StyledLabel>(hintRows, tr("Ctrl+shift+click+drag: Lock angle to following point"),
                               kHintSize),
        1, 1);
    layout->addWidget(hintRows);

    // row of controls at the bottom of the tab
    layout->addWidget(&makeBottomControls(*page), 0, Qt::AlignLeft);

    // The keys of the editor's own undo and redo, while the focus is in this tab
    auto* undoKey = makeChild<QShortcut>(page, QKeySequence(Qt::CTRL | Qt::Key_Z));
    undoKey->setObjectName(QStringLiteral("finPointUndoKey"));
    undoKey->setContext(Qt::WidgetWithChildrenShortcut);
    connect(undoKey, &QShortcut::activated, this,
            guarded([this] { performFinPointUndo(); }, QStringLiteral("finPointUndoKey")));
    auto* redoKey = makeChild<QShortcut>(page, QKeySequence(Qt::CTRL | Qt::Key_Y));
    redoKey->setObjectName(QStringLiteral("finPointRedoKey"));
    redoKey->setContext(Qt::WidgetWithChildrenShortcut);
    connect(redoKey, &QShortcut::activated, this,
            guarded([this] { performFinPointRedo(); }, QStringLiteral("finPointRedoKey")));
    updateFinPointUndoRedoControls();
    return *page;
}

QWidget& FreeformFinSetConfig::makePointList(QWidget& page)
{
    auto* panel  = makeChild<QWidget>(&page);
    auto* layout = makeChild<QVBoxLayout>(panel);
    layout->setContentsMargins(0, 0, 0, 0);

    m_tableModel = makeChild<FinPointTableModel>(panel, m_finPoints);
    m_table      = makeChild<QTableView>(panel);
    m_table->setObjectName(QStringLiteral("FinPoints"));
    m_table->setModel(m_tableModel);
    m_table->setItemDelegate(makeChild<FinPointCellDelegate>(m_table));
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    // A press of the left button edits an editable cell (Java's changeSelection() override), as
    // do typing, F2 and a double click.
    m_table->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed |
                             QAbstractItemView::AnyKeyPressed);
    m_table->verticalHeader()->hide();
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_table->viewport()->installEventFilter(this);
    connect(m_table, &QTableView::pressed, this,
            guarded(
                [this](const QModelIndex& index) {
                    if (index.flags().testFlag(Qt::ItemIsEditable))
                    {
                        m_table->edit(index);
                    }
                },
                QStringLiteral("FinPoints")));
    connect(m_table->selectionModel(), &QItemSelectionModel::selectionChanged, this,
            guarded([this] { pointSelectionChanged(); }, QStringLiteral("FinPoints")));
    connect(m_tableModel, &FinPointTableModel::pointEntered, this,
            guarded([this](int row, double x, double y) { setPointFromTable(row, x, y); },
                    QStringLiteral("FinPoints")));
    layout->addWidget(m_table, 1);

    // Point Actions button
    //: FreeformFinSetConfig.lbl.pointActions
    m_pointActions = makeChild<DropdownButton>(panel, tr("Point Actions\u2026"));
    m_pointActions->setObjectName(QStringLiteral("pointActionsButton"));
    m_pointActions->setAutoDefault(false);
    m_pointActions->setFocusPolicy(Qt::ClickFocus);
    // Scale Fin: Milestone 5 (the scale dialog).
    //// Import from image
    //: CustomFinImport.button.label
    m_importImage = m_pointActions->addMenuItem(tr("Import from image"));
    m_importImage->setObjectName(QStringLiteral("importImageAction"));
    connect(m_importImage, &QAction::triggered, this,
            guarded([this] { importImage(); }, QStringLiteral("importImageAction")));
    //// Export CSV
    //: FreeformFinSetConfig.lbl.exportCSV
    m_exportCsv = m_pointActions->addMenuItem(tr("Export CSV"));
    m_exportCsv->setObjectName(QStringLiteral("exportCsvAction"));
    connect(m_exportCsv, &QAction::triggered, this,
            guarded([this] { exportCsv(); }, QStringLiteral("exportCsvAction")));
    layout->addWidget(m_pointActions, 0, Qt::AlignLeft);
    return *panel;
}

void FreeformFinSetConfig::makePointActions()
{
    // Context menu for table
    //: FreeformFinSetConfig.lbl.insertPoint
    m_insertPoint = makeChild<QAction>(this, Icons::icon(Icons::Id::FILE_NEW), tr("Insert point"));
    m_insertPoint->setObjectName(QStringLiteral("insertPointAction"));
    connect(m_insertPoint, &QAction::triggered, this,
            guarded([this] { insertPoint(); }, QStringLiteral("insertPointAction")));
    //: FreeformFinSetConfig.lbl.deletePoint
    m_deletePoint =
        makeChild<QAction>(this, Icons::icon(Icons::Id::EDIT_DELETE), tr("Delete point"));
    m_deletePoint->setObjectName(QStringLiteral("deletePointAction"));
    connect(m_deletePoint, &QAction::triggered, this,
            guarded([this] { deletePoint(); }, QStringLiteral("deletePointAction")));
    m_tableMenu = makeChild<QMenu>(this);
    m_tableMenu->setObjectName(QStringLiteral("tablePointMenu"));
    m_tableMenu->addAction(m_insertPoint);
    m_tableMenu->addAction(m_deletePoint);

    // Context menu for figure
    m_figureMenu = makeChild<QMenu>(this);
    m_figureMenu->setObjectName(QStringLiteral("figurePointMenu"));
    m_figureMenu->addAction(m_deletePoint);
    // Enabled until the table's selection first changes (Java's actions start enabled).
}

QWidget& FreeformFinSetConfig::makeBottomControls(QWidget& page)
{
    auto* panel  = makeChild<QWidget>(&page);
    auto* layout = makeChild<QHBoxLayout>(panel);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(kShapeGap);

    // The zoom: outside the tab order, as Java's (its focus traversal policy lists none of it).
    for (QWidget* control : {static_cast<QWidget*>(m_selector->zoomOutButton()),
                             static_cast<QWidget*>(m_selector->scaleCombo()),
                             static_cast<QWidget*>(m_selector->zoomInButton()),
                             static_cast<QWidget*>(m_selector->zoomFitButton())})
    {
        control->setFocusPolicy(Qt::ClickFocus);
    }
    layout->addWidget(m_selector);

    m_undoButton = makeChild<QPushButton>(panel, Icons::icon(Icons::Id::EDIT_UNDO), QString());
    m_undoButton->setObjectName(QStringLiteral("finPointUndoButton"));
    //: FreeformFinSetConfig.btn.undo.ttip
    m_undoButton->setToolTip(tr("Undo last point edit"));
    m_undoButton->setFocusPolicy(Qt::NoFocus);
    m_undoButton->setAutoDefault(false);
    connect(m_undoButton, &QPushButton::clicked, this,
            guarded([this] { performFinPointUndo(); }, QStringLiteral("finPointUndoButton")));
    layout->addSpacing(kParagraphGap);
    layout->addWidget(m_undoButton);

    m_redoButton = makeChild<QPushButton>(panel, Icons::icon(Icons::Id::EDIT_REDO), QString());
    m_redoButton->setObjectName(QStringLiteral("finPointRedoButton"));
    //: FreeformFinSetConfig.btn.redo.ttip
    m_redoButton->setToolTip(tr("Redo last point edit"));
    m_redoButton->setFocusPolicy(Qt::NoFocus);
    m_redoButton->setAutoDefault(false);
    connect(m_redoButton, &QPushButton::clicked, this,
            guarded([this] { performFinPointRedo(); }, QStringLiteral("finPointRedoButton")));
    layout->addWidget(m_redoButton);

    // Checkbox to control whether rocket updates during dragging
    //: FreeformFinSetConfig.lbl.updateRocketWhileDraggingPoint
    m_liveUpdate = makeChild<QCheckBox>(panel, tr("Live update the rocket"));
    m_liveUpdate->setObjectName(QStringLiteral("UpdateRocketWhileDraggingPoint"));
    m_liveUpdate->setFocusPolicy(Qt::ClickFocus);
    m_liveUpdate->setChecked(services().preferences().getBoolean(
        GuiPreferenceKeys::kUpdateRocketWhileDraggingPoint,
        GuiPreferenceKeys::kUpdateRocketWhileDraggingPointDefault));
    //: FreeformFinSetConfig.lbl.updateRocketWhileDraggingPoint.ttip
    m_liveUpdate->setToolTip(
        tr("<html>If enabled, the rocket figure will update in real-time while dragging "
           "points.<br>Disabling this improves performance but updates only occur when dragging "
           "is finished.</html>"));
    connect(m_liveUpdate, &QCheckBox::clicked, this,
            guarded(
                [this](bool checked) {
                    services().preferences().putBoolean(
                        GuiPreferenceKeys::kUpdateRocketWhileDraggingPoint, checked);
                },
                QStringLiteral("UpdateRocketWhileDraggingPoint")));
    layout->addSpacing(kParagraphGap);
    layout->addWidget(m_liveUpdate);
    return *panel;
}

// ---------------------------------------------------------------------------------- state

int FreeformFinSetConfig::dragIndex() const noexcept
{
    return m_area->dragIndex();
}

int FreeformFinSetConfig::selectedPointRow() const
{
    const QModelIndexList rows = m_table->selectionModel()->selectedRows();
    return rows.isEmpty() ? -1 : rows.front().row();
}

void FreeformFinSetConfig::selectPointRow(int row)
{
    QItemSelectionModel& selection = *m_table->selectionModel();
    if (row < 0 || row >= m_tableModel->rowCount())
    {
        selection.clearSelection();
        return;
    }
    selection.select(m_tableModel->index(row, 0),
                     QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
}

void FreeformFinSetConfig::updateFields()
{
    FinSetConfig::updateFields();

    if (m_tableModel != nullptr)
    {
        // Java's fireTableDataChanged() clears the selection, which its listeners hear.
        m_tableModel->refresh();
        m_table->selectionModel()->clearSelection();
        // make sure to do this *after* the table data is updated.
        selectPointRow(dragIndex() >= 0 ? dragIndex() : -1);
    }

    if (m_figure != nullptr)
    {
        if (dragIndex() >= 0)
        {
            m_figure->setSelectedIndex(dragIndex());
        }
        else
        {
            m_figure->resetSelectedIndex();
        }
        m_figure->updateFigure();
    }

    if (m_area != nullptr)
    {
        m_area->revalidateFigure();
    }

    updateFinPointUndoRedoControls();
}

void FreeformFinSetConfig::pointSelectionChanged()
{
    updateActionStates();
    m_figure->setSelectedIndex(selectedPointRow());
    m_figure->updateFigure();
}

bool FreeformFinSetConfig::commitAll()
{
    // The cell editor first: its focus loss would commit it before any other field's.
    const bool cell = commitCellEditing();
    return FinSetConfig::commitAll() || cell;
}

bool FreeformFinSetConfig::editsEnded() const
{
    const std::shared_ptr<ComponentSelection>& selected = selection();
    return selected == nullptr || !selected->isValid();
}

QWidget* FreeformFinSetConfig::openCellEditor() const
{
    if (m_table == nullptr)
    {
        return nullptr;
    }
    for (int row = 0; row < m_tableModel->rowCount(); ++row)
    {
        for (int column = 0; column < FinPointTableModel::kColumnCount; ++column)
        {
            if (QWidget* editor = m_table->indexWidget(m_tableModel->index(row, column)))
            {
                return editor;
            }
        }
    }
    return nullptr;
}

void FreeformFinSetConfig::cancelCellEditing()
{
    if (QWidget* editor = openCellEditor())
    {
        // The view's closeEditor() takes the delegate off the editor first: its focus loss then
        // commits nothing.
        Q_EMIT m_table->itemDelegate()->closeEditor(editor,
                                                    QAbstractItemDelegate::RevertModelCache);
    }
}

bool FreeformFinSetConfig::commitCellEditing()
{
    const QPointer<QWidget> editor(openCellEditor());
    if (editor.isNull() || editsEnded())
    {
        return false;
    }
    const FreeformFinSet*         fins = m_finPoints.main();
    const std::vector<Coordinate> before =
        fins != nullptr ? fins->getFinPoints() : std::vector<Coordinate>();
    // What the delegate does on the editor's focus loss: the text set as the cell's edit
    // (setPointFromTable() through the model), then the editor closed.
    QAbstractItemDelegate& delegate = *m_table->itemDelegate();
    Q_EMIT delegate.commitData(editor.data());
    if (!editor.isNull() && openCellEditor() == editor.data())
    {
        Q_EMIT delegate.closeEditor(editor.data(), QAbstractItemDelegate::NoHint);
    }
    const FreeformFinSet* after = m_finPoints.main();
    return after != nullptr && after->getFinPoints() != before;
}

// ---------------------------------------------------------------------------------- actions

void FreeformFinSetConfig::beginFinPointEdit()
{
    if (m_figure != nullptr)
    {
        m_figure->editHistory().beginEdit();
    }
    updateFinPointUndoRedoControls();
}

void FreeformFinSetConfig::commitFinPointEdit()
{
    if (m_figure != nullptr)
    {
        m_figure->editHistory().commitEdit();
    }
    updateFinPointUndoRedoControls();
}

void FreeformFinSetConfig::performFinPointUndo()
{
    if (m_figure == nullptr || dragIndex() >= 0 || m_figure->editHistory().isEditInProgress() ||
        editsEnded())
    {
        return;
    }
    m_figure->editHistory().undo();
    updateFinPointUndoRedoControls();
}

void FreeformFinSetConfig::performFinPointRedo()
{
    if (m_figure == nullptr || dragIndex() >= 0 || m_figure->editHistory().isEditInProgress() ||
        editsEnded())
    {
        return;
    }
    m_figure->editHistory().redo();
    updateFinPointUndoRedoControls();
}

void FreeformFinSetConfig::updateFinPointUndoRedoControls()
{
    if (m_figure == nullptr || m_area == nullptr)
    {
        return;
    }
    const FinPointEditHistory& history = m_figure->editHistory();
    const bool                 busy    = dragIndex() >= 0 || history.isEditInProgress();
    if (m_undoButton != nullptr)
    {
        m_undoButton->setEnabled(!busy && history.canUndo());
    }
    if (m_redoButton != nullptr)
    {
        m_redoButton->setEnabled(!busy && history.canRedo());
    }
}

void FreeformFinSetConfig::updateActionStates()
{
    // If one of the actions is null, the rest will be too
    if (m_insertPoint == nullptr || m_table == nullptr)
    {
        return;
    }
    const int row  = selectedPointRow();
    const int rows = m_tableModel->rowCount();
    // You can't add to the last fin point
    m_insertPoint->setEnabled(row < rows - 1);
    // You can't delete the first or last fin point
    m_deletePoint->setEnabled(row > 0 && row < rows - 1);
}

void FreeformFinSetConfig::insertPoint()
{
    const int currentPointIdx = selectedPointRow();
    if (currentPointIdx == -1 || currentPointIdx >= m_tableModel->rowCount() - 1 || editsEnded())
    {
        return;
    }
    const FreeformFinSet* fins = m_finPoints.main();
    if (fins == nullptr)
    {
        return;
    }
    const std::vector<Coordinate> points       = fins->getFinPoints();
    const auto                    current      = static_cast<std::size_t>(currentPointIdx);
    const Coordinate&             currentPoint = points.at(current);
    const Coordinate&             nextPoint    = points.at(current + 1);
    const Point2D toAdd((currentPoint.x + nextPoint.x) / 2, (currentPoint.y + nextPoint.y) / 2);
    beginFinPointEdit();
    try
    {
        expectSuccess(m_finPoints.addPoint(current + 1, toAdd));
    }
    catch (...)
    {
        commitFinPointEdit();
        throw;
    }
    commitFinPointEdit();
}

void FreeformFinSetConfig::deletePoint()
{
    const int currentPointIdx = selectedPointRow();
    if (currentPointIdx == -1 || editsEnded())
    {
        return;
    }

    // Cancel any active cell editing before deleting the point
    cancelCellEditing();

    beginFinPointEdit();
    try
    {
        expectSuccess(m_finPoints.removePoint(static_cast<std::size_t>(currentPointIdx)));
    }
    catch (...)
    {
        commitFinPointEdit();
        throw;
    }
    commitFinPointEdit();
}

void FreeformFinSetConfig::setPointFromTable(int row, double x, double y)
{
    // A cell editor that commits as the released panel's dialog hides edits nothing.
    if (editsEnded())
    {
        return;
    }
    beginFinPointEdit();
    try
    {
        expectSuccess(m_finPoints.setPoint(static_cast<std::size_t>(row), x, y));
    }
    catch (...)
    {
        commitFinPointEdit();
        throw;
    }
    commitFinPointEdit();

    updateFields();
}

void FreeformFinSetConfig::importImage()
{
    Preferences&                 preferences = services().preferences();
    UserPrompts&                 prompts     = services().prompts();
    UserPrompts::OpenFileRequest request;
    // FileHelper.getImageFileFilter(), chosen, and the accept-all filter of Swing's file chooser
    // (FileChooser.acceptAllFileFilterText).
    request.nameFilters =
        QStringList{CustomFinImporter::imageNameFilter(), tr("All Files") + QStringLiteral(" (*)")};
    if (const std::optional<std::filesystem::path> directory = preferences.getDefaultDirectory())
    {
        request.directory = *directory;
    }
    //: CustomFinImport.description
    request.description =
        tr("The image will be converted internally to black and white image (black for the fin), "
           "so make sure you use a solid dark color for the fin, and white or a light color for "
           "the background. The fin must be touching the bottom of the image, which is the base "
           "of the fin.");
    // A modal dialog runs the event loop, in which the configuration dialog may go (an undo in
    // another window, for one): nothing of the panel is used once it has gone.
    const QPointer<FreeformFinSetConfig>             alive(this);
    const std::optional<UserPrompts::OpenFileAnswer> answer = prompts.askOpenFiles(this, request);
    if (alive.isNull() || editsEnded() || !answer.has_value() || answer->files.empty())
    {
        return;
    }

    bool startedUndo = false;
    // Java's finally: the undo operation ends, the edit of the history is committed and the
    // dialog's directory becomes the default one, whatever happened.
    const auto finish = [&] {
        if (alive.isNull())
        {
            // Gone during the message box, before any undo operation started.
            preferences.setDefaultDirectory(answer->directory);
            return;
        }
        try
        {
            if (startedUndo)
            {
                document().stopUndo();
            }
        }
        catch (...)
        {
            commitFinPointEdit();
            throw;
        }
        commitFinPointEdit();
        preferences.setDefaultDirectory(answer->directory);
    };
    beginFinPointEdit();
    try
    {
        const Result<std::vector<Coordinate>> points =
            CustomFinImporter::getPoints(answer->files.front());
        if (points.has_value())
        {
            //: CustomFinImport.undo
            document().startUndo(QtStrings::toUtf8(tr("Import freeform fin set")));
            startedUndo = true;
            m_finPoints.setPoints(*points);
        }
        else
        {
            prompts.showMessage(this, {.severity = UserPrompts::Severity::ERROR,
                                       //: CustomFinImport.error.title
                                       .title = tr("Error loading fin profile"),
                                       .lines = {QtStrings::fromUtf8(points.error().message)}});
        }
    }
    catch (...)
    {
        finish();
        throw;
    }
    finish();
}

void FreeformFinSetConfig::exportCsv()
{
    Preferences&                 preferences = services().preferences();
    UserPrompts&                 prompts     = services().prompts();
    UserPrompts::SaveFileRequest request;
    // FileHelper.CSV_FILTER, chosen, and the accept-all filter of Swing's file chooser
    // (FileChooser.acceptAllFileFilterText).
    request.nameFilters =
        //: FileHelper.CSV_FILTER
        QStringList{tr("Comma Separated Files (*.csv)"), tr("All Files") + QStringLiteral(" (*)")};
    if (const std::optional<std::filesystem::path> directory = preferences.getDefaultDirectory())
    {
        request.directory = *directory;
    }
    // A modal dialog runs the event loop, in which the configuration dialog may go (an undo in
    // another window, for one): nothing of the panel is used once it has gone.
    const QPointer<FreeformFinSetConfig>             alive(this);
    const std::optional<UserPrompts::SaveFileAnswer> answer = prompts.askSaveFile(this, request);
    if (alive.isNull() || !answer.has_value())
    {
        return;
    }
    const std::filesystem::path file = forceExtension(answer->file, "csv");
    std::error_code             error;
    if (std::filesystem::exists(file, error))
    {
        const bool overwrite = prompts.askOverwrite(this, QtStrings::fromPath(file.filename()));
        if (alive.isNull() || !overwrite)
        {
            return;
        }
    }

    if (const Result<void> written = writeCsvFile(*m_tableModel, file); !written.has_value())
    {
        // Deviation: Java prints the IOException's stack trace only.
        prompts.showMessage(this, {.severity = UserPrompts::Severity::ERROR,
                                   //: error.writing.title
                                   .title = tr("Error writing file"),
                                   //: error.writing.desc
                                   .lines = {tr("An error occurred while writing to the file:"),
                                             QtStrings::fromUtf8(written.error().message)}});
    }
    preferences.setDefaultDirectory(answer->directory);
}

Result<void> FreeformFinSetConfig::writeCsvFile(const FinPointTableModel&    model,
                                                const std::filesystem::path& file)
{
    return writeTextFile(file, QtStrings::toUtf8(model.csvText()));
}

std::filesystem::path FreeformFinSetConfig::forceExtension(const std::filesystem::path& file,
                                                           std::string_view             extension)
{
    const std::string      name              = pathToUtf8(file.filename());
    const std::size_t      index             = name.rfind('.');
    const std::string_view originalExtension = index != std::string::npos && index > 0
                                                   ? std::string_view(name).substr(index + 1)
                                                   : std::string_view();
    if (lowerCase(originalExtension) == lowerCase(extension))
    {
        return file;
    }
    const std::string withExtension = extension.starts_with('.')
                                          ? pathToUtf8(file) + std::string(extension)
                                          : pathToUtf8(file) + '.' + std::string(extension);
    return pathFromUtf8(withExtension);
}

// ---------------------------------------------------------------------------------- mouse

bool FreeformFinSetConfig::eventFilter(QObject* watched, QEvent* event)
{
    if (m_figure != nullptr && watched == m_figure)
    {
        if (!isMouseEvent(event->type()) || !editsEnded())
        {
            return false;
        }
        // Released: the mouse edits nothing; a drag ends (the rocket thawed).
        static_cast<void>(
            Guard::run([this] { m_area->endDrag(); }, QStringLiteral("finPointScrollArea")));
        return true;
    }
    if (m_table == nullptr || watched != m_table->viewport())
    {
        return FinSetConfig::eventFilter(watched, event);
    }
    const QEvent::Type type = event->type();
    if (type != QEvent::MouseButtonPress && type != QEvent::MouseButtonRelease &&
        type != QEvent::MouseButtonDblClick)
    {
        return false;
    }
    const auto&           mouse  = dynamic_cast<const QMouseEvent&>(*event);
    const Qt::MouseButton button = mouse.button();
    if (button != Qt::RightButton && button != Qt::MiddleButton)
    {
        return false;
    }
    // Java's table takes no selection from these buttons; the right one's click opens the menu.
    const QPoint position = mouse.position().toPoint();
    if (button == Qt::RightButton && type == QEvent::MouseButtonPress)
    {
        m_tableRightPress = position;
    }
    else if (button == Qt::RightButton && type == QEvent::MouseButtonRelease &&
             position == std::exchange(m_tableRightPress, QPoint(-1, -1)))
    {
        static_cast<void>(
            Guard::run([this, position,
                        &mouse] { tableRightClick(position, mouse.globalPosition().toPoint()); },
                       QStringLiteral("FinPoints")));
    }
    return true;
}

void FreeformFinSetConfig::tableRightClick(QPoint position, QPoint globalPosition)
{
    const int row = m_table->rowAt(position.y());
    // Select new row
    if (row != selectedPointRow())
    {
        if (row < 0 || row >= m_tableModel->rowCount())
        {
            return;
        }
        selectPointRow(row);
    }
    m_tableMenu->popup(globalPosition);
}

void FreeformFinSetConfig::figurePointMenuRequested(int index, QPoint globalPosition)
{
    // Select the point in the table so action state is correct
    selectPointRow(index);
    updateActionStates();
    m_figureMenu->popup(globalPosition);
}

}  // namespace QtRocket::Gui
