#include "QtRocket/gui/dialogs/simulation/CsvImportSettingsDialog.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <map>
#include <numbers>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include <QAbstractTextDocumentLayout>
#include <QCheckBox>
#include <QColor>
#include <QDialog>
#include <QFont>
#include <QFontDatabase>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QObject>
#include <QPalette>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSizeF>
#include <QSpinBox>
#include <QStackedWidget>
#include <QString>
#include <QStringList>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QWidget>
#include <Qt>
#include <QtContainerFwd>
#include <QtTypes>

#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/util/UserPrompts.h"
#include "QtRocket/gui/widgets/FieldSeparatorComboBox.h"
#include "QtRocket/gui/widgets/FieldValidator.h"
#include "QtRocket/gui/widgets/StyledLabel.h"
#include "QtRocket/gui/widgets/UnitSelector.h"
#include "QtRocket/preferences/PreferenceKeys.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/unit/Unit.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/BugError.h"

namespace QtRocket::Gui
{

namespace
{

using Row = CsvImportSettingsDialog::Row;

/// The context of the dialog's reports (Guard).
QString context()
{
    return QStringLiteral("Wind Data Import Settings");
}

/// The rows in their order.
constexpr std::array<Row, CsvImportSettingsDialog::kRowCount> kRows{
    Row::ALTITUDE, Row::SPEED, Row::DIRECTION, Row::STANDARD_DEVIATION};

/// The name of @p row in the Java fields' names ("altitude", "stdDeviation").
[[nodiscard]] QString rowName(Row row)
{
    switch (row)
    {
        case Row::ALTITUDE:
            return QStringLiteral("altitude");
        case Row::SPEED:
            return QStringLiteral("speed");
        case Row::DIRECTION:
            return QStringLiteral("direction");
        case Row::STANDARD_DEVIATION:
            return QStringLiteral("stdDeviation");
    }
    QTROCKET_UNREACHABLE();
}

/// MigLayout's paragraph gap ("gap para", "wrap para") and the dialog's insets ("insets dialog"),
/// pixels.
constexpr int kParagraphGap = 12;
constexpr int kDialogInsets = 12;
/// How much smaller the help texts are, points (deriveFont(getSize() - 1f)).
constexpr double kHelpSizeDecrease = -1.0;
/// The default field separator (FieldSeparatorComboBox.loadFromExportPreferences()).
constexpr std::string_view kDefaultSeparator = ",";

/// The preview's example values in SI units (Java's updatePreview()): the altitudes, speeds,
/// directions and standard deviations of its four rows.
constexpr std::array<double, 4> kAltitudes{100.0, 200.0, 300.0, 400.0};
constexpr std::array<double, 4> kSpeeds{5.0, 7.5, 10.0, 12.5};
constexpr std::array<double, 4> kDirections{std::numbers::pi / 2, std::numbers::pi * 5 / 12,
                                            std::numbers::pi * 7 / 12, std::numbers::pi * 3 / 4};
constexpr std::array<double, 4> kStdDevs{1.0, 1.5, 2.0, 2.5};

/// Java's String.trim(): @p text without the characters at or below U+0020 at either end.
[[nodiscard]] QString javaTrim(const QString& text)
{
    qsizetype start = 0;
    qsizetype end   = text.size();
    while (start < end && text.at(start).unicode() <= u' ')
    {
        ++start;
    }
    while (end > start && text.at(end - 1).unicode() <= u' ')
    {
        --end;
    }
    return text.sliced(start, end - start);
}

/// Java's hasNonEmptyText(): the field's text is not blank.
[[nodiscard]] bool hasNonEmptyText(const QLineEdit& field)
{
    return !javaTrim(field.text()).isEmpty();
}

/// The unit of @p column, which an answer must have.
[[nodiscard]] const Unit& unitOf(const UserPrompts::WindCsvColumn& column)
{
    if (column.unit == nullptr)
    {
        bug("A column of the wind CSV import settings has no unit");
    }
    return *column.unit;
}

}  // namespace

// ------------------------------------------------------------------------------- the statics

QString CsvImportSettingsDialog::importColumn(const Column& column, bool hasHeader)
{
    // Java: getAltitudeColumn(), ...: the name, or the index as text.
    return hasHeader ? column.name : QString::number(column.index);
}

const CsvImportSettingsDialog::Column& CsvImportSettingsDialog::columnOf(const Settings& settings,
                                                                         Row             row)
{
    switch (row)
    {
        case Row::ALTITUDE:
            return settings.altitude;
        case Row::SPEED:
            return settings.speed;
        case Row::DIRECTION:
            return settings.direction;
        case Row::STANDARD_DEVIATION:
            return settings.standardDeviation;
    }
    QTROCKET_UNREACHABLE();
}

const UnitGroup& CsvImportSettingsDialog::unitGroupOf(Row row)
{
    // Java: new DoubleModel(0, UnitGroup.UNITS_DISTANCE), UNITS_WINDSPEED, UNITS_ANGLE.
    switch (row)
    {
        case Row::ALTITUDE:
            return unitGroup(UnitGroupId::DISTANCE);
        case Row::SPEED:
        case Row::STANDARD_DEVIATION:
            return unitGroup(UnitGroupId::WINDSPEED);
        case Row::DIRECTION:
            return unitGroup(UnitGroupId::ANGLE);
    }
    QTROCKET_UNREACHABLE();
}

CsvImportSettingsDialog::Settings CsvImportSettingsDialog::storedSettings(
    const Preferences& preferences)
{
    Settings settings;
    settings.hasHeader = preferences.isMultiLevelWindCsvImportHeader();
    settings.altitude  = {
        .name  = QtStrings::fromUtf8(preferences.getMultiLevelWindCsvImportAltitudeColumn()),
        .index = preferences.getMultiLevelWindCsvImportAltitudeColumnIndex(),
        .unit  = &preferences.getMultiLevelWindCsvImportAltitudeUnit()};
    settings.speed = {
        .name  = QtStrings::fromUtf8(preferences.getMultiLevelWindCsvImportSpeedColumn()),
        .index = preferences.getMultiLevelWindCsvImportSpeedColumnIndex(),
        .unit  = &preferences.getMultiLevelWindCsvImportSpeedUnit()};
    settings.direction = {
        .name  = QtStrings::fromUtf8(preferences.getMultiLevelWindCsvImportDirectionColumn()),
        .index = preferences.getMultiLevelWindCsvImportDirectionColumnIndex(),
        .unit  = &preferences.getMultiLevelWindCsvImportDirectionUnit()};
    settings.standardDeviation = {
        .name  = QtStrings::fromUtf8(preferences.getMultiLevelWindCsvImportStddevColumn()),
        .index = preferences.getMultiLevelWindCsvImportStddevColumnIndex(),
        .unit  = &preferences.getMultiLevelWindCsvImportStddevUnit()};
    // FieldSeparatorComboBox.loadFromExportPreferences(): an empty separator leaves ",".
    const std::string separator =
        preferences.getString(PreferenceKeys::kExportFieldSeparator, kDefaultSeparator);
    settings.separator =
        QtStrings::fromUtf8(separator.empty() ? std::string(kDefaultSeparator) : separator);
    return settings;
}

void CsvImportSettingsDialog::storeSettings(Preferences& preferences, const Settings& settings)
{
    // MultiLevelWindEditDialog.importLevels(), in its order.
    preferences.setMultiLevelWindCsvImportHeader(settings.hasHeader);

    preferences.setMultiLevelWindCsvImportAltitudeColumn(QtStrings::toUtf8(settings.altitude.name));
    preferences.setMultiLevelWindCsvImportSpeedColumn(QtStrings::toUtf8(settings.speed.name));
    preferences.setMultiLevelWindCsvImportDirectionColumn(
        QtStrings::toUtf8(settings.direction.name));
    preferences.setMultiLevelWindCsvImportStddevColumn(
        QtStrings::toUtf8(settings.standardDeviation.name));

    preferences.setMultiLevelWindCsvImportAltitudeColumnIndex(settings.altitude.index);
    preferences.setMultiLevelWindCsvImportSpeedColumnIndex(settings.speed.index);
    preferences.setMultiLevelWindCsvImportDirectionColumnIndex(settings.direction.index);
    preferences.setMultiLevelWindCsvImportStddevColumnIndex(settings.standardDeviation.index);

    preferences.setMultiLevelWindCsvImportAltitudeUnit(unitOf(settings.altitude));
    preferences.setMultiLevelWindCsvImportSpeedUnit(unitOf(settings.speed));
    preferences.setMultiLevelWindCsvImportDirectionUnit(unitOf(settings.direction));
    preferences.setMultiLevelWindCsvImportStddevUnit(unitOf(settings.standardDeviation));

    // OpenRocket bug 34: Java stores getSeparator(), the separator box's item text ("SPACE",
    // "TAB"), as the export's separator, which the export's box then shows as its first character
    // "S" or "T" (and writes between the fields); the separator itself is stored here, as the
    // export's own options store it (CsvOptionPreferences::store()).
    preferences.putString(PreferenceKeys::kExportFieldSeparator,
                          QtStrings::toUtf8(settings.separator));
}

QString CsvImportSettingsDialog::displayedSeparator(const QString& comboText)
{
    // Java: "Replace tab and space with their visible representation for display".
    if (comboText == FieldSeparatorComboBox::tabText())
    {
        return QStringLiteral("\\t");
    }
    if (comboText == FieldSeparatorComboBox::spaceText())
    {
        return QStringLiteral("\\s");
    }
    return comboText;
}

std::vector<CsvImportSettingsDialog::PreviewPart> CsvImportSettingsDialog::previewParts(
    const Settings& settings, const QString& separator)
{
    std::vector<PreviewPart> parts;
    const auto               add = [&parts](const QString& text, PreviewStyle style) {
        parts.push_back({.text = text, .style = style});
    };
    const bool    header        = settings.hasHeader;
    const QString altitude      = importColumn(settings.altitude, header);
    const QString speed         = importColumn(settings.speed, header);
    const QString direction     = importColumn(settings.direction, header);
    const QString stdDeviation  = importColumn(settings.standardDeviation, header);
    const bool    withDeviation = !stdDeviation.isEmpty();

    // Java's literals, not translated.
    add(QStringLiteral("# You can add comments with a hash"), PreviewStyle::OPTIONAL);
    add(QStringLiteral("\n"), PreviewStyle::OPTIONAL);

    if (header)
    {
        // The header row
        add(altitude + separator, PreviewStyle::NORMAL);
        add(speed + separator, PreviewStyle::NORMAL);
        add(direction, PreviewStyle::NORMAL);
        if (withDeviation)
        {
            add(separator + stdDeviation, PreviewStyle::OPTIONAL);
        }
        add(QStringLiteral("\n"), PreviewStyle::NORMAL);
    }
    else
    {
        // No header: a comment showing which column is which
        add(QStringLiteral("# Column mapping: "), PreviewStyle::OPTIONAL);
        add(altitude + QStringLiteral("=Altitude, "), PreviewStyle::INDEX);
        add(speed + QStringLiteral("=Speed, "), PreviewStyle::INDEX);
        add(direction + QStringLiteral("=Direction"), PreviewStyle::INDEX);
        if (withDeviation)
        {
            add(QStringLiteral(", ") + stdDeviation + QStringLiteral("=StdDev"),
                PreviewStyle::OPTIONAL);
        }
        add(QStringLiteral("\n"), PreviewStyle::NORMAL);
    }

    // The sample rows, in the selected units
    for (std::size_t i = 0; i < kAltitudes.size(); ++i)
    {
        add(QtStrings::unitText(unitOf(settings.altitude), kAltitudes.at(i)) + separator,
            PreviewStyle::NORMAL);
        add(QtStrings::unitText(unitOf(settings.speed), kSpeeds.at(i)) + separator,
            PreviewStyle::NORMAL);
        add(QtStrings::unitText(unitOf(settings.direction), kDirections.at(i)),
            PreviewStyle::NORMAL);
        if (withDeviation)
        {
            add(separator + QtStrings::unitText(unitOf(settings.standardDeviation), kStdDevs.at(i)),
                PreviewStyle::OPTIONAL);
        }
        add(QStringLiteral("\n"), PreviewStyle::NORMAL);
    }

    // An ellipsis: more data could follow
    add(QString(QChar(0x2026)), PreviewStyle::NORMAL);
    return parts;
}

QColor CsvImportSettingsDialog::previewColor(PreviewStyle style)
{
    switch (style)
    {
        case PreviewStyle::NORMAL:
            return {Qt::black};
        case PreviewStyle::OPTIONAL:
            // Java's Color.GRAY.
            return {128, 128, 128};
        case PreviewStyle::INDEX:
            // "Dark green".
            return {0, 100, 0};
    }
    QTROCKET_UNREACHABLE();
}

// ------------------------------------------------------------------------------- the dialog

CsvImportSettingsDialog::CsvImportSettingsDialog(const Settings& initial, UserPrompts& prompts,
                                                 QWidget* parent)
  : QDialog(parent), m_prompts(&prompts), m_scrollArea(makeChild<QScrollArea>(this))
{
    setObjectName(QStringLiteral("csvImportSettingsDialog"));
    //: CSVImportSettingsDialog.title
    setWindowTitle(tr("Wind Data Import Settings"));
    setWindowModality(Qt::ApplicationModal);

    // Java: the main panel in a scroll pane without a border, the buttons below it at the right.
    m_scrollArea->setObjectName(QStringLiteral("scrollPane"));
    m_scrollArea->setFrameShape(QFrame::NoFrame);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->verticalScrollBar()->setSingleStep(kScrollStep);
    m_scrollArea->setWidget(&buildMainPanel(initial));

    auto* const buttonPanel = makeChild<QWidget>(this);
    buttonPanel->setObjectName(QStringLiteral("buttonPanel"));
    auto* const buttonLayout = makeChild<QHBoxLayout>(buttonPanel);
    buttonLayout->addStretch(1);
    //: button.cancel
    m_cancelButton = makeChild<QPushButton>(buttonPanel, tr("Cancel"));
    m_cancelButton->setObjectName(QStringLiteral("cancelButton"));
    m_cancelButton->setAutoDefault(false);
    //: button.ok
    m_okButton = makeChild<QPushButton>(buttonPanel, tr("OK"));
    m_okButton->setObjectName(QStringLiteral("okButton"));
    // Java: getRootPane().setDefaultButton(okButton).
    m_okButton->setDefault(true);
    buttonLayout->addWidget(m_cancelButton);
    buttonLayout->addWidget(m_okButton);

    auto* const layout = makeChild<QVBoxLayout>(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_scrollArea, 1);
    layout->addWidget(buttonPanel);

    connectFields();
    connect(m_cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_okButton, &QPushButton::clicked, this, guarded([this] { tryAccept(); }, context()));

    // Initial column field validation setup, and the initial preview.
    updateColumnFieldValidation();
    if (m_hasHeader->isChecked())
    {
        validateColumnNames();
    }
    else
    {
        validateColumnIndices();
    }
    updatePreview();

    resize(kWidth, kHeight);
}

CsvImportSettingsDialog::~CsvImportSettingsDialog() = default;

QWidget& CsvImportSettingsDialog::buildMainPanel(const Settings& initial)
{
    // Java: MigLayout("fill, insets dialog, gap para"): the description, the CSV settings and the
    // preview, one below the other across the width.
    auto* const mainPanel = makeChild<QWidget>(m_scrollArea);
    mainPanel->setObjectName(QStringLiteral("mainPanel"));
    auto* const layout = makeChild<QVBoxLayout>(mainPanel);
    layout->setContentsMargins(kDialogInsets, kDialogInsets, kDialogInsets, kDialogInsets);
    layout->setSpacing(kParagraphGap);

    //: CSVImportSettingsDialog.description
    m_description = makeChild<StyledLabel>(
        mainPanel, tr("This dialog configures how the data is structured in your CSV file."));
    m_description->setObjectName(QStringLiteral("descriptionArea"));
    m_description->setTextFormat(Qt::PlainText);
    m_description->setWordWrap(true);
    layout->addWidget(m_description);

    //: CSVImportSettingsDialog.csvSettings
    m_csvSettings = makeChild<QGroupBox>(mainPanel, tr("CSV Settings"));
    m_csvSettings->setObjectName(QStringLiteral("csvSettings"));
    buildCsvSettings(initial);
    layout->addWidget(m_csvSettings);

    //: CSVImportSettingsDialog.preview
    m_previewBox = makeChild<QGroupBox>(mainPanel, tr("CSV Example Contents Preview"));
    m_previewBox->setObjectName(QStringLiteral("previewPanel"));
    buildPreview();
    layout->addWidget(m_previewBox);
    layout->addStretch(1);
    return *mainPanel;
}

void CsvImportSettingsDialog::buildCsvSettings(const Settings& initial)
{
    // Java: MigLayout() without "fill": the grid's columns keep their preferred widths, the empty
    // last column here takes the rest.
    auto* const   grid  = makeChild<QGridLayout>(m_csvSettings);
    constexpr int kSpan = 4;
    grid->setColumnStretch(kSpan - 1, 1);

    //: CSVImportSettingsDialog.columnMappingHelp
    m_mappingHelp = makeChild<StyledLabel>(
        m_csvSettings,
        tr("Specify which columns in your CSV file contain each required data type and the "
           "units used in the file."),
        kHelpSizeDecrease);
    m_mappingHelp->setObjectName(QStringLiteral("columnMappingHelp"));
    m_mappingHelp->setTextFormat(Qt::PlainText);
    m_mappingHelp->setWordWrap(true);
    grid->addWidget(m_mappingHelp, 0, 0, 1, kSpan);
    // "wrap para"
    grid->setRowMinimumHeight(1, kParagraphGap);

    //: CSVImportSettingsDialog.hasHeaders
    m_hasHeader = makeChild<QCheckBox>(m_csvSettings, tr("CSV file has a header row"));
    m_hasHeader->setObjectName(QStringLiteral("hasHeaderCheckBox"));
    //: CSVImportSettingsDialog.hasHeaders.ttip
    m_hasHeader->setToolTip(tr("Select if your CSV file includes column names in the first row"));
    m_hasHeader->setChecked(initial.hasHeader);
    grid->addWidget(m_hasHeader, 2, 0, 1, kSpan);

    // The grid's header labels
    m_dataTypeLabel = makeChild<StyledLabel>(m_csvSettings,
                                             //: CSVImportSettingsDialog.dataType
                                             tr("Data Type"), StyledLabel::Style::BOLD);
    m_dataTypeLabel->setObjectName(QStringLiteral("dataTypeLabel"));
    m_dataTypeLabel->setMinimumWidth(kLabelColumnWidth);
    grid->addWidget(m_dataTypeLabel, 3, 0);
    // A label of its own, so that its text can change.
    m_columnTypeLabel = makeChild<StyledLabel>(m_csvSettings, QString(), StyledLabel::Style::BOLD);
    m_columnTypeLabel->setObjectName(QStringLiteral("columnTypeLabel"));
    grid->addWidget(m_columnTypeLabel, 3, 1);
    m_unitLabel = makeChild<StyledLabel>(m_csvSettings,
                                         //: CSVImportSettingsDialog.unit
                                         tr("Unit"), StyledLabel::Style::BOLD);
    m_unitLabel->setObjectName(QStringLiteral("unitLabel"));
    m_unitLabel->setMinimumWidth(kLabelColumnWidth);
    grid->addWidget(m_unitLabel, 3, 2);

    int gridRow = 4;
    for (const Row row : kRows)
    {
        buildRow(row, gridRow, columnOf(initial, row));
        ++gridRow;
    }

    // A separator line across the grid ("spanx, growx, wrap para")
    auto* const line = makeChild<QFrame>(m_csvSettings);
    line->setObjectName(QStringLiteral("separator"));
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    m_separatorLine = line;
    grid->addWidget(line, gridRow, 0, 1, kSpan);
    ++gridRow;
    grid->setRowMinimumHeight(gridRow, kParagraphGap);
    ++gridRow;

    //// Field separator
    //: CSVImportSettingsDialog.separator
    m_separatorLabel = makeChild<QLabel>(m_csvSettings, tr("Field Separator"));
    m_separatorLabel->setObjectName(QStringLiteral("separatorLabel"));
    grid->addWidget(m_separatorLabel, gridRow, 0);
    m_separatorComboBox = makeChild<FieldSeparatorComboBox>(m_csvSettings);
    m_separatorComboBox->setObjectName(QStringLiteral("separatorComboBox"));
    // FieldSeparatorComboBox.loadFromExportPreferences(): an empty separator changes nothing.
    if (!initial.separator.isEmpty())
    {
        m_separatorComboBox->setSeparatorString(initial.separator);
    }
    grid->addWidget(m_separatorComboBox, gridRow, 1, Qt::AlignLeft);

    m_columnTypeLabel->setText(columnTypeText(m_hasHeader->isChecked()));
}

void CsvImportSettingsDialog::buildRow(Row row, int gridRow, const Column& initial)
{
    RowWidgets& widgets = m_rows.at(static_cast<std::size_t>(row));
    QString     text;
    switch (row)
    {
        case Row::ALTITUDE:
            //: CSVImportSettingsDialog.altitude
            text = tr("Altitude");
            break;
        case Row::SPEED:
            //: CSVImportSettingsDialog.speed
            text = tr("Speed");
            break;
        case Row::DIRECTION:
            //: CSVImportSettingsDialog.direction
            text = tr("Direction");
            break;
        case Row::STANDARD_DEVIATION:
            //: CSVImportSettingsDialog.stdDev
            text = tr("Standard Deviation (optional)");
            break;
    }
    const QString name = rowName(row);
    auto* const   grid = qobject_cast<QGridLayout*>(m_csvSettings->layout());
    QTROCKET_ASSERT(grid != nullptr);

    widgets.label = makeChild<QLabel>(m_csvSettings, text);
    widgets.label->setObjectName(name + QStringLiteral("Label"));
    grid->addWidget(widgets.label, gridRow, 0);

    // Java: a CardLayout panel of the name's text field ("text") and the index's spinner
    // ("spinner").
    widgets.panel = makeChild<QStackedWidget>(m_csvSettings);
    widgets.panel->setObjectName(name + QStringLiteral("ColumnPanel"));
    widgets.field = makeChild<QLineEdit>(widgets.panel, initial.name);
    widgets.field->setObjectName(name + QStringLiteral("ColumnField"));
    widgets.panel->addWidget(widgets.field);
    // Java: new SpinnerNumberModel(index, 0, Integer.MAX_VALUE, 1), the format "#".
    widgets.spinner = makeChild<QSpinBox>(widgets.panel);
    widgets.spinner->setObjectName(name + QStringLiteral("ColumnSpinner"));
    widgets.spinner->setRange(0, std::numeric_limits<int>::max());
    widgets.spinner->setValue(initial.index);
    widgets.panel->addWidget(widgets.spinner);
    grid->addWidget(widgets.panel, gridRow, 1);

    widgets.unit = makeChild<UnitSelector>(m_csvSettings, unitGroupOf(row));
    widgets.unit->setObjectName(name + QStringLiteral("UnitSelector"));
    if (initial.unit != nullptr)
    {
        widgets.unit->setSelectedUnit(*initial.unit);
    }
    grid->addWidget(widgets.unit, gridRow, 2, Qt::AlignLeft);
}

void CsvImportSettingsDialog::buildPreview()
{
    // Java: MigLayout("fillx"): the help text, then the text pane.
    auto* const layout = makeChild<QVBoxLayout>(m_previewBox);
    layout->setSpacing(kParagraphGap);

    //: CSVImportSettingsDialog.previewHelp
    m_previewHelp = makeChild<StyledLabel>(
        m_previewBox,
        tr("An example of how the first lines of data in you CSV file should look like for some "
           "example data. Make sure the data is correctly separated and that the column names "
           "are correct.\nYou can add blank lines or comment lines starting with a hash (#)."),
        kHelpSizeDecrease);
    m_previewHelp->setObjectName(QStringLiteral("previewHelp"));
    m_previewHelp->setTextFormat(Qt::PlainText);
    m_previewHelp->setWordWrap(true);
    layout->addWidget(m_previewHelp);

    // Java: a JTextPane, not editable, in a monospaced 12 point font, white, with a margin of 5;
    // it is as tall as its text (the dialog scrolls, not the pane).
    m_previewTextPane = makeChild<QTextEdit>(m_previewBox);
    m_previewTextPane->setObjectName(QStringLiteral("previewTextPane"));
    m_previewTextPane->setReadOnly(true);
    m_previewTextPane->setAcceptRichText(false);
    m_previewTextPane->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_previewTextPane->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    font.setPointSize(kPreviewFontSize);
    m_previewTextPane->setFont(font);
    m_previewTextPane->document()->setDocumentMargin(kPreviewMargin);
    // Java: setBackground(Color.WHITE), its texts black, gray and green, in every theme.
    QPalette palette = m_previewTextPane->palette();
    palette.setColor(QPalette::Base, Qt::white);
    palette.setColor(QPalette::Text, Qt::black);
    m_previewTextPane->setPalette(palette);
    connect(m_previewTextPane->document()->documentLayout(),
            &QAbstractTextDocumentLayout::documentSizeChanged, this,
            [this] { fitPreviewHeight(); });
    layout->addWidget(m_previewTextPane);
}

void CsvImportSettingsDialog::connectFields()
{
    // Java's DocumentListener on the name fields, ChangeListeners on the spinners, the header
    // check box's ItemListener, ItemListeners on the separator box and the unit selectors.
    for (const Row row : kRows)
    {
        const RowWidgets& widgets = widgetsOf(row);
        connect(widgets.field, &QLineEdit::textChanged, this,
                guarded(
                    [this] {
                        validateColumnNames();
                        updatePreview();
                    },
                    context()));
        connect(widgets.spinner, &QSpinBox::valueChanged, this,
                guarded(
                    [this] {
                        validateColumnIndices();
                        updatePreview();
                    },
                    context()));
        connect(widgets.unit, &UnitSelector::unitSelected, this,
                guarded([this] { updatePreview(); }, context()));
    }
    connect(m_hasHeader, &QCheckBox::toggled, this,
            guarded([this] { headerChanged(); }, context()));
    connect(m_separatorComboBox, &FieldSeparatorComboBox::currentTextChanged, this,
            guarded([this] { updatePreview(); }, context()));
}

const CsvImportSettingsDialog::RowWidgets& CsvImportSettingsDialog::widgetsOf(Row row) const
{
    return m_rows.at(static_cast<std::size_t>(row));
}

QLabel& CsvImportSettingsDialog::rowLabel(Row row) const
{
    return *widgetsOf(row).label;
}

QStackedWidget& CsvImportSettingsDialog::columnPanel(Row row) const
{
    return *widgetsOf(row).panel;
}

QLineEdit& CsvImportSettingsDialog::columnField(Row row) const
{
    return *widgetsOf(row).field;
}

QSpinBox& CsvImportSettingsDialog::columnSpinner(Row row) const
{
    return *widgetsOf(row).spinner;
}

UnitSelector& CsvImportSettingsDialog::unitSelector(Row row) const
{
    return *widgetsOf(row).unit;
}

// ------------------------------------------------------------------------------- the settings

CsvImportSettingsDialog::Settings CsvImportSettingsDialog::settings() const
{
    const auto column = [this](Row row) {
        const RowWidgets& widgets = widgetsOf(row);
        return Column{.name  = javaTrim(widgets.field->text()),
                      .index = widgets.spinner->value(),
                      .unit  = &widgets.unit->getSelectedUnit()};
    };
    return {.hasHeader         = hasHeader(),
            .altitude          = column(Row::ALTITUDE),
            .speed             = column(Row::SPEED),
            .direction         = column(Row::DIRECTION),
            .standardDeviation = column(Row::STANDARD_DEVIATION),
            // OpenRocket bug 34: the separator itself, where Java's getSeparator() gives the
            // item's text.
            .separator = m_separatorComboBox->separatorString()};
}

bool CsvImportSettingsDialog::hasHeader() const
{
    return m_hasHeader->isChecked();
}

QString CsvImportSettingsDialog::importColumn(Row row) const
{
    return importColumn(columnOf(settings(), row), hasHeader());
}

std::vector<CsvImportSettingsDialog::PreviewPart> CsvImportSettingsDialog::preview() const
{
    return previewParts(settings(), displayedSeparator(m_separatorComboBox->currentText()));
}

// ------------------------------------------------------------------------------- the validation

QString CsvImportSettingsDialog::columnTypeText(bool hasHeader)
{
    if (hasHeader)
    {
        //: CSVImportSettingsDialog.columnName
        return tr("Column Name");
    }
    //: CSVImportSettingsDialog.columnIndex
    return tr("Column Index");
}

void CsvImportSettingsDialog::headerChanged()
{
    const bool hasHeaders = m_hasHeader->isChecked();

    // Update the column type label based on whether headers are present
    m_columnTypeLabel->setText(columnTypeText(hasHeaders));

    updateColumnFieldValidation();

    // If switching to non-header mode, validate indices immediately
    if (!hasHeaders)
    {
        validateColumnIndices();
    }
    else
    {
        validateColumnNames();
    }

    updatePreview();
}

void CsvImportSettingsDialog::updateColumnFieldValidation()
{
    const bool hasHeaders = m_hasHeader->isChecked();
    for (const RowWidgets& widgets : m_rows)
    {
        widgets.panel->setCurrentWidget(hasHeaders ? static_cast<QWidget*>(widgets.field)
                                                   : static_cast<QWidget*>(widgets.spinner));
    }
}

void CsvImportSettingsDialog::validateColumnNames()
{
    if (!m_hasHeader->isChecked())
    {
        return;
    }

    resetErrorOutlines();

    // The fields by their trimmed names (Java: a HashMap, of which only the groups count).
    std::map<QString, std::vector<QLineEdit*>> fieldsByName;
    const auto                                 add = [&fieldsByName](QLineEdit& field) {
        // addColumnNameForValidation(): an empty name is no column.
        if (const QString name = javaTrim(field.text()); !name.isEmpty())
        {
            fieldsByName[name].push_back(&field);
        }
    };
    for (const Row row : {Row::ALTITUDE, Row::SPEED, Row::DIRECTION})
    {
        QLineEdit& field = *widgetsOf(row).field;
        if (!hasNonEmptyText(field))
        {
            FieldValidator::setOutline(field, FieldValidator::Outline::ERROR);
        }
        add(field);
    }
    add(*widgetsOf(Row::STANDARD_DEVIATION).field);

    for (const auto& [name, fields] : fieldsByName)
    {
        if (fields.size() > 1)
        {
            for (QLineEdit* const field : fields)
            {
                FieldValidator::setOutline(*field, FieldValidator::Outline::ERROR);
            }
        }
    }
}

bool CsvImportSettingsDialog::hasInvalidColumnNames() const
{
    return std::ranges::any_of(m_rows, [](const RowWidgets& widgets) {
        return FieldValidator::outlineOf(*widgets.field) == FieldValidator::Outline::ERROR;
    });
}

bool CsvImportSettingsDialog::hasDuplicateColumnNames() const
{
    std::set<QString> names;
    for (const RowWidgets& widgets : m_rows)
    {
        if (const QString name = javaTrim(widgets.field->text());
            !name.isEmpty() && !names.insert(name).second)
        {
            return true;
        }
    }
    return false;
}

QStringList CsvImportSettingsDialog::columnNameErrorLines() const
{
    const bool emptyRequired  = !hasNonEmptyText(*widgetsOf(Row::ALTITUDE).field) ||
                                !hasNonEmptyText(*widgetsOf(Row::SPEED).field) ||
                                !hasNonEmptyText(*widgetsOf(Row::DIRECTION).field);
    const bool duplicateNames = hasDuplicateColumnNames();

    //: CSVImportSettingsDialog.emptyColumnNameError
    const QString empty =
        tr("Missing required column name(s). Please fill in the highlighted fields.");
    //: CSVImportSettingsDialog.duplicateColumnNameError
    const QString duplicate =
        tr("Duplicate column names found. Please ensure that each column name is unique.");
    if (emptyRequired && duplicateNames)
    {
        // Java: the two joined by "\n", which the message shows as two lines.
        return {empty, duplicate};
    }
    if (emptyRequired)
    {
        return {empty};
    }
    return {duplicate};
}

void CsvImportSettingsDialog::validateColumnIndices()
{
    // Only validate if headers are not selected
    if (m_hasHeader->isChecked())
    {
        return;
    }

    resetErrorOutlines();

    const int altIndex    = widgetsOf(Row::ALTITUDE).spinner->value();
    const int speedIndex  = widgetsOf(Row::SPEED).spinner->value();
    const int dirIndex    = widgetsOf(Row::DIRECTION).spinner->value();
    const int stdDevIndex = widgetsOf(Row::STANDARD_DEVIATION).spinner->value();

    // Java's checks, in its order (its spinners as their rows)
    std::set<int> usedIndices;
    std::set<Row> errorSpinners;

    usedIndices.insert(altIndex);

    if (!usedIndices.insert(speedIndex).second)
    {
        errorSpinners.insert(Row::SPEED);
        if (altIndex == speedIndex)
        {
            errorSpinners.insert(Row::ALTITUDE);
        }
    }

    if (!usedIndices.insert(dirIndex).second)
    {
        errorSpinners.insert(Row::DIRECTION);
        if (altIndex == dirIndex)
        {
            errorSpinners.insert(Row::ALTITUDE);
        }
        if (speedIndex == dirIndex)
        {
            errorSpinners.insert(Row::SPEED);
        }
    }

    // The standard deviation's index only while its field is used (its name is not blank)
    if (hasNonEmptyText(*widgetsOf(Row::STANDARD_DEVIATION).field) &&
        !usedIndices.insert(stdDevIndex).second)
    {
        errorSpinners.insert(Row::STANDARD_DEVIATION);
        if (altIndex == stdDevIndex)
        {
            errorSpinners.insert(Row::ALTITUDE);
        }
        if (speedIndex == stdDevIndex)
        {
            errorSpinners.insert(Row::SPEED);
        }
        if (dirIndex == stdDevIndex)
        {
            errorSpinners.insert(Row::DIRECTION);
        }
    }

    for (const Row row : errorSpinners)
    {
        FieldValidator::setOutline(*widgetsOf(row).spinner, FieldValidator::Outline::ERROR);
    }
}

bool CsvImportSettingsDialog::hasDuplicateColumnIndices()
{
    // Java: validates first (this applies the outlines), then reads them.
    validateColumnIndices();
    return std::ranges::any_of(m_rows, [](const RowWidgets& widgets) {
        return FieldValidator::outlineOf(*widgets.spinner) == FieldValidator::Outline::ERROR;
    });
}

void CsvImportSettingsDialog::resetErrorOutlines()
{
    for (const RowWidgets& widgets : m_rows)
    {
        FieldValidator::setOutline(*widgets.field, FieldValidator::Outline::NONE);
    }
    for (const RowWidgets& widgets : m_rows)
    {
        FieldValidator::setOutline(*widgets.spinner, FieldValidator::Outline::NONE);
    }
}

void CsvImportSettingsDialog::tryAccept()
{
    // Validate column mappings one final time
    validateColumnIndices();
    validateColumnNames();

    //: CSVImportSettingsDialog.validationError
    const QString title = tr("Incorrect Data Input");
    // When headers are used, validate column names (required + unique)
    if (m_hasHeader->isChecked() && hasInvalidColumnNames())
    {
        m_prompts->showMessage(this, {.severity = UserPrompts::Severity::ERROR,
                                      .title    = title,
                                      .lines    = columnNameErrorLines()});
        return;
    }

    // When headers are not used, check for duplicate column indices
    if (!m_hasHeader->isChecked() && hasDuplicateColumnIndices())
    {
        //: CSVImportSettingsDialog.duplicateColumnError
        const QString duplicate =
            tr("Duplicate column indices found. Please ensure that each column index is unique.");
        m_prompts->showMessage(
            this, {.severity = UserPrompts::Severity::ERROR, .title = title, .lines = {duplicate}});
        return;
    }

    accept();
}

// ------------------------------------------------------------------------------- the preview

void CsvImportSettingsDialog::updatePreview()
{
    QTextDocument& document = *m_previewTextPane->document();
    document.clear();
    QTextCursor cursor(&document);
    for (const PreviewPart& part : preview())
    {
        QTextCharFormat format;
        format.setForeground(previewColor(part.style));
        cursor.insertText(part.text, format);
    }
    fitPreviewHeight();
}

void CsvImportSettingsDialog::fitPreviewHeight()
{
    const QSizeF size = m_previewTextPane->document()->size();
    m_previewTextPane->setFixedHeight(static_cast<int>(std::ceil(size.height())) +
                                      (2 * m_previewTextPane->frameWidth()));
}

}  // namespace QtRocket::Gui
