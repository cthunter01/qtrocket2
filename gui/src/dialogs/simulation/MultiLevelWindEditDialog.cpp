#include "QtRocket/gui/dialogs/simulation/MultiLevelWindEditDialog.h"

#include <memory>

#include <QDialog>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QList>
#include <QPushButton>
#include <QRadioButton>
#include <QSplitter>
#include <QString>
#include <QVBoxLayout>
#include <QWidget>
#include <Qt>

#include "QtRocket/gui/dialogs/simulation/MultiLevelWindTableModel.h"
#include "QtRocket/gui/dialogs/simulation/MultiLevelWindTableView.h"
#include "QtRocket/gui/plot/WindProfilePlot.h"
#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/gui/util/GuiServices.h"
#include "QtRocket/gui/util/Icons.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/util/UserPrompts.h"
#include "QtRocket/gui/widgets/StyledLabel.h"
#include "QtRocket/models/MultiLevelPinkNoiseWindModel.h"
#include "QtRocket/models/WindModel.h"

namespace QtRocket::Gui
{

namespace
{

/// The context of the dialog's reports (Guard).
QString context()
{
    return QStringLiteral("Wind Profile Editor");
}

/// The empty border around the controls (Java: BorderFactory.createEmptyBorder(8, 8, 8, 8)).
constexpr int kControlsBorder = 8;
/// The empty border around the visualization's title (createEmptyBorder(6, 6, 6, 6)).
constexpr int kTitleBorder = 6;
/// MigLayout's paragraph gap ("gapleft para"), before "Import levels".
constexpr int kParagraphGap = 12;
/// How much larger the visualization's title is, points (StyledLabel(..., 1.5f, BOLD)).
constexpr double kTitleSizeIncrease = 1.5;

/// A button of the dialog: Enter presses none of them (Java's dialog has no default button).
[[nodiscard]] QPushButton* makeButton(QWidget& parent, const QString& text, const QString& name)
{
    auto* const button = makeChild<QPushButton>(&parent, text);
    button->setObjectName(name);
    button->setAutoDefault(false);
    return button;
}

}  // namespace

MultiLevelWindEditDialog::MultiLevelWindEditDialog(MultiLevelPinkNoiseWindModel& model,
                                                   const GuiServices& services, QWidget* parent)
  : QDialog(parent),
    m_services(services),
    m_model(&model),
    m_tableModel(makeChild<MultiLevelWindTableModel>(this, model, services.preferences())),
    m_splitPane(makeChild<QSplitter>(this, Qt::Horizontal))
{
    // SimulationConditionsPanel's variable.
    setObjectName(QStringLiteral("dialog"));
    //: WindProfileEditorDlg.title
    setWindowTitle(tr("Wind Profile Editor"));
    setWindowModality(Qt::ApplicationModal);

    m_splitPane->setObjectName(QStringLiteral("splitPane"));
    m_splitPane->setChildrenCollapsible(false);
    auto* const tablePanel = makeChild<QWidget>(m_splitPane);
    tablePanel->setObjectName(QStringLiteral("tablePanel"));
    buildTablePanel(*tablePanel);
    m_splitPane->addWidget(tablePanel);
    m_splitPane->addWidget(&buildVisualizationPanel());
    m_splitPane->setStretchFactor(0, kTableStretch);
    m_splitPane->setStretchFactor(1, kVisualizationStretch);

    // Java: the split pane in the centre, below it the Close button at the right.
    auto* const layout = makeChild<QVBoxLayout>(this);
    layout->addWidget(m_splitPane, 1);
    auto* const buttonPanel  = makeChild<QWidget>(this);
    auto* const buttonLayout = makeChild<QHBoxLayout>(buttonPanel);
    buttonLayout->setContentsMargins(0, 0, 0, 0);
    buttonLayout->addStretch(1);
    //: button.close
    m_closeButton = makeButton(*buttonPanel, tr("Close"), QStringLiteral("closeButton"));
    buttonLayout->addWidget(m_closeButton);
    layout->addWidget(buttonPanel);

    resize(kWidth, kHeight);
    setMinimumSize(kMinimumWidth, kMinimumHeight);
    // Java's split pane gives the table its preferred width, the columns', and the profile the
    // rest (the split pane shares out what is missing).
    m_splitPane->setSizes(
        {m_table->horizontalHeader()->length() + (2 * m_table->frameWidth()), kVisualizationWidth});

    connectParts();
    showAltitudeReference();
    showUnits();
    updateDeleteButton();
}

MultiLevelWindEditDialog::~MultiLevelWindEditDialog() = default;

void MultiLevelWindEditDialog::buildTablePanel(QWidget& panel)
{
    auto* const layout = makeChild<QVBoxLayout>(&panel);
    layout->setContentsMargins(0, 0, 0, 0);
    m_table = makeChild<MultiLevelWindTableView>(&panel, *m_tableModel);
    layout->addWidget(m_table, 1);
    auto* const controls = makeChild<QWidget>(&panel);
    controls->setObjectName(QStringLiteral("controlsPanel"));
    buildControls(*controls);
    layout->addWidget(controls);
}

void MultiLevelWindEditDialog::buildControls(QWidget& panel)
{
    auto* const grid = makeChild<QGridLayout>(&panel);
    grid->setContentsMargins(kControlsBorder, kControlsBorder, kControlsBorder, kControlsBorder);

    // Java: the altitude reference's panel spans the row ("spanx, growx, wrap").
    auto* const reference       = makeChild<QWidget>(&panel);
    auto* const referenceLayout = makeChild<QHBoxLayout>(reference);
    referenceLayout->setContentsMargins(0, 0, 0, 0);
    //: WindProfileEditorDlg.lbl.AltitudeReference
    m_altitudeRefLabel = makeChild<QLabel>(reference, tr("Altitude reference:"));
    m_altitudeRefLabel->setObjectName(QStringLiteral("altitudeRefLabel"));
    //: WindProfileEditorDlg.lbl.AltitudeReference.ttip
    m_altitudeRefLabel->setToolTip(tr("Set to which altitude the wind levels refer to."));
    //: WindProfileEditorDlg.radio.MSL
    m_mslButton = makeChild<QRadioButton>(reference, tr("Mean Sea Level (MSL)"));
    m_mslButton->setObjectName(QStringLiteral("butMSL"));
    //: WindProfileEditorDlg.radio.AGL
    m_aglButton = makeChild<QRadioButton>(reference, tr("Above Ground Level (AGL)"));
    m_aglButton->setObjectName(QStringLiteral("butAGL"));
    referenceLayout->addWidget(m_altitudeRefLabel);
    referenceLayout->addWidget(m_mslButton);
    referenceLayout->addWidget(m_aglButton);
    referenceLayout->addStretch(1);
    grid->addWidget(reference, 0, 0, 1, -1);

    //: WindProfileEditorDlg.but.AddNewLevel
    m_addButton = makeButton(panel, tr("Add new level"), QStringLiteral("addRowButton"));
    m_addButton->setIcon(Icons::icon(Icons::Id::FILE_NEW));
    //: WindProfileEditorDlg.but.DeleteLevel
    m_deleteButton = makeButton(panel, tr("Delete level"), QStringLiteral("deleteRowButton"));
    //: WindProfileEditorDlg.but.deleteWindLevel.ttip
    m_deleteButton->setToolTip(tr("Delete the selected wind level"));
    m_deleteButton->setIcon(Icons::icon(Icons::Id::EDIT_DELETE));
    //: WindProfileEditorDlg.but.importLevels
    m_importButton = makeButton(panel, tr("Import levels"), QStringLiteral("importButton"));
    //: WindProfileEditorDlg.but.importLevels.ttip
    m_importButton->setToolTip(tr("Import wind levels from a CSV file"));
    m_importButton->setIcon(Icons::icon(Icons::Id::FILE_IMPORT));
    //: WindProfileEditorDlg.but.ResetLevels
    m_resetButton = makeButton(panel, tr("Reset levels"), QStringLiteral("resetButton"));
    //: WindProfileEditorDlg.but.ResetLevels.ttip
    m_resetButton->setToolTip(tr("Reset wind levels to default"));

    // Java: "Add new level", "Delete level" ("growx"), "Import levels" ("gapleft para, wrap"), then
    // "Reset levels" below "Delete level" ("skip 1, growx").
    grid->addWidget(m_addButton, 1, 0);
    grid->addWidget(m_deleteButton, 1, 1);
    grid->setColumnMinimumWidth(2, kParagraphGap);
    grid->addWidget(m_importButton, 1, 3);
    grid->addWidget(m_resetButton, 2, 1);
    grid->setColumnStretch(4, 1);
}

QWidget& MultiLevelWindEditDialog::buildVisualizationPanel()
{
    // Java: a panel with the theme's border, the title at its top and the visualization below.
    auto* const panel = makeChild<QFrame>(m_splitPane);
    panel->setObjectName(QStringLiteral("visPanel"));
    panel->setFrameShape(QFrame::StyledPanel);
    auto* const layout = makeChild<QVBoxLayout>(panel);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    m_visualizationTitle = makeChild<StyledLabel>(
        panel,
        //: WindProfilePanel.title.WindProfileVisualization
        tr("Wind Profile Visualization"), kTitleSizeIncrease, StyledLabel::Style::BOLD);
    m_visualizationTitle->setObjectName(QStringLiteral("visualizationTitle"));
    m_visualizationTitle->setContentsMargins(kTitleBorder, kTitleBorder, kTitleBorder,
                                             kTitleBorder);
    layout->addWidget(m_visualizationTitle);
    m_plot = makeChild<WindProfilePlot>(panel, *m_model);
    layout->addWidget(m_plot, 1);
    return *panel;
}

void MultiLevelWindEditDialog::connectParts()
{
    connect(
        m_mslButton, &QRadioButton::clicked, this,
        guarded([this] { chooseAltitudeReference(WindModel::AltitudeReference::MSL); }, context()));
    connect(
        m_aglButton, &QRadioButton::clicked, this,
        guarded([this] { chooseAltitudeReference(WindModel::AltitudeReference::AGL); }, context()));
    connect(m_addButton, &QPushButton::clicked, this,
            guarded([this] { m_table->addLevel(); }, context()));
    connect(m_deleteButton, &QPushButton::clicked, this,
            guarded([this] { m_table->deleteSelectedLevel(); }, context()));
    connect(m_importButton, &QPushButton::clicked, this,
            guarded([this] { importLevels(); }, context()));
    connect(m_resetButton, &QPushButton::clicked, this,
            guarded([this] { resetLevels(); }, context()));
    connect(m_closeButton, &QPushButton::clicked, this, &QDialog::accept);

    connect(m_table, &MultiLevelWindTableView::selectedLevelChanged, this,
            guarded(
                [this] {
                    updateDeleteButton();
                    showSelectedLevel();
                },
                context()));
    connect(m_tableModel, &MultiLevelWindTableModel::levelsChanged, this,
            guarded([this] { updateDeleteButton(); }, context()));
    connect(m_tableModel, &MultiLevelWindTableModel::columnUnitChanged, this,
            guarded([this] { showUnits(); }, context()));
    m_connection = m_model->changed().connect([this] { showAltitudeReference(); });
}

void MultiLevelWindEditDialog::showAltitudeReference()
{
    if (!m_connection.connected())
    {
        // The model has gone: the buttons keep what they showed.
        return;
    }
    const bool agl = m_model->getAltitudeReference() == WindModel::AltitudeReference::AGL;
    (agl ? m_aglButton : m_mslButton)->setChecked(true);
}

void MultiLevelWindEditDialog::chooseAltitudeReference(WindModel::AltitudeReference reference)
{
    m_table->commitEditing();
    if (m_connection.connected())
    {
        m_model->setAltitudeReference(reference);
    }
}

void MultiLevelWindEditDialog::updateDeleteButton()
{
    // Java: enabled while a level is selected and there is more than one.
    m_deleteButton->setEnabled(m_table->selectedRow() >= 0 && m_tableModel->rowCount() > 1);
}

void MultiLevelWindEditDialog::showUnits()
{
    const MultiLevelWindTableModel& table = *m_tableModel;
    m_plot->setUnits({
        .altitude  = &table.columnUnit(MultiLevelWindTableModel::kAltitudeColumn),
        .speed     = &table.columnUnit(MultiLevelWindTableModel::kSpeedColumn),
        .direction = &table.columnUnit(MultiLevelWindTableModel::kDirectionColumn),
        .deviation = &table.columnUnit(MultiLevelWindTableModel::kDeviationColumn),
    });
}

void MultiLevelWindEditDialog::showSelectedLevel()
{
    MultiLevelWindTableModel::Level* const level = m_table->selectedLevel();
    m_plot->setSelectedLevel(
        level != nullptr
            ? std::weak_ptr<const MultiLevelWindTableModel::Level>(level->weak_from_this())
            : std::weak_ptr<const MultiLevelWindTableModel::Level>());
}

UserPrompts::Question MultiLevelWindEditDialog::overwriteLevelsQuestion()
{
    return {
        .severity = UserPrompts::Severity::WARNING,
        //: WindProfileEditorDlg.dlg.overwriteLevels.title
        .title = tr("Overwrite wind levels?"),
        //: WindProfileEditorDlg.dlg.overwriteLevels.msg
        .text = tr("Your current wind levels will be overwritten. Do you want to continue?"),
    };
}

void MultiLevelWindEditDialog::resetLevels()
{
    m_table->commitEditing();
    if (m_services.prompts().askYesNo(this, overwriteLevelsQuestion()))
    {
        m_tableModel->resetLevels();
    }
}

void MultiLevelWindEditDialog::importLevels()
{
    m_table->commitEditing();
    // Part S3 of Milestone 4's run 2: Java's importLevels() (the CSV import settings, the file,
    // the overwrite question, MultiLevelPinkNoiseWindModel::importLevelsFromCsv()).
}

void MultiLevelWindEditDialog::done(int result)
{
    // Java's spinner commits its text when the closing dialog takes its focus.
    Guard::run([this] { m_table->commitEditing(); }, context());
    QDialog::done(result);
}

}  // namespace QtRocket::Gui
