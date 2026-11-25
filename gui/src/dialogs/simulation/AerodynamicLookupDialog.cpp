#include "QtRocket/gui/dialogs/simulation/AerodynamicLookupDialog.h"

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include <QChar>
#include <QColor>
#include <QComboBox>
#include <QDialog>
#include <QEvent>
#include <QFont>
#include <QFontDatabase>
#include <QFontMetrics>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPalette>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPushButton>
#include <QString>
#include <QStringList>
#include <QTabWidget>
#include <QTextDocument>
#include <QVBoxLayout>
#include <QWidget>
#include <Qt>
#include <QtContainerFwd>

#include "QtRocket/aero/lookup/CsvMachAoALookup.h"
#include "QtRocket/aero/lookup/MachAoALookup.h"
#include "QtRocket/gui/binding/ObjectSelection.h"
#include "QtRocket/gui/util/FileNames.h"
#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/gui/util/GuiServices.h"
#include "QtRocket/gui/util/Icons.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/util/ThemeColors.h"
#include "QtRocket/gui/util/UserPrompts.h"
#include "QtRocket/gui/widgets/FieldSeparatorComboBox.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/simulation/SimulationOptions.h"
#include "QtRocket/util/DecimalFormat.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket::Gui
{

namespace
{

using Table = AerodynamicLookupDialog::Table;

/// The context of the dialog's reports (Guard).
QString context()
{
    return QStringLiteral("Aerodynamic lookup tables");
}

/// Java's colours of the text areas, in every theme: white, with the example in grey and the
/// loaded data in black.
[[nodiscard]] QPalette textColors(QPalette palette, bool loaded)
{
    // Java's Color.WHITE, Color.BLACK and Color.GRAY (128, 128, 128; Qt::gray is lighter).
    constexpr int kGray = 128;
    palette.setColor(QPalette::Base, QColor(Qt::white));
    palette.setColor(QPalette::Text, loaded ? QColor(Qt::black) : QColor(kGray, kGray, kGray));
    return palette;
}

/// The lines of a message: @p text split at its line breaks (Java's "%n").
[[nodiscard]] QStringList messageLines(const QString& text)
{
    return text.split(u'\n');
}

/// A button that Enter does not click (Java's dialog has no default button).
[[nodiscard]] QPushButton* makeButton(QWidget* parent, const QString& text, const QString& name)
{
    auto* button = makeChild<QPushButton>(parent, text);
    button->setObjectName(name);
    button->setAutoDefault(false);
    return button;
}

}  // namespace

AerodynamicLookupDialog::AerodynamicLookupDialog(SimulationOptions& options,
                                                 const GuiServices& services, QWidget* parent)
  : QDialog(parent),
    m_services(services),
    m_options(ObjectSelection::make<SimulationOptions>(options)),
    m_tabs(makeChild<QTabWidget>(this))
{
    setObjectName(QStringLiteral("aerodynamicLookupDialog"));
    //: AerodynamicLookupDialog.title
    setWindowTitle(tr("Aerodynamic lookup tables"));
    setWindowModality(Qt::ApplicationModal);

    Section& drag   = section(Table::DRAG);
    drag.csv        = options.getDragLookupCsvPath();
    drag.table      = options.getDragLookupTable();
    Section& stable = section(Table::STABILITY);
    stable.csv      = options.getStabilityLookupCsvPath();
    stable.table    = options.getStabilityLookupTable();

    // Java: MigLayout("fill, gap para"): the tabs, then the buttons on the right.
    auto* content = makeChild<QVBoxLayout>(this);
    m_tabs->setObjectName(QStringLiteral("tabbedPane"));
    //: AerodynamicLookupDialog.lbl.drag
    m_tabs->addTab(buildSection(Table::DRAG), tr("Drag CSV"));
    //: AerodynamicLookupDialog.lbl.stability
    m_tabs->addTab(buildSection(Table::STABILITY), tr("Stability CSV"));
    content->addWidget(m_tabs, 1);

    auto* buttonRow = makeChild<QWidget>(this);
    auto* buttons   = makeChild<QHBoxLayout>(buttonRow);
    buttons->setContentsMargins(0, 0, 0, 0);
    buttons->addStretch(1);
    //: button.cancel
    m_cancelButton = makeButton(buttonRow, tr("Cancel"), QStringLiteral("cancelButton"));
    buttons->addWidget(m_cancelButton);
    //: button.ok
    m_okButton = makeButton(buttonRow, tr("OK"), QStringLiteral("okButton"));
    buttons->addWidget(m_okButton);
    content->addWidget(buttonRow);
    connect(m_cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_okButton, &QPushButton::clicked, this,
            guarded([this] { static_cast<void>(applyAndClose()); }, context()));

    updateDisplays();
    updateColors();
}

AerodynamicLookupDialog::~AerodynamicLookupDialog() = default;

// ---------------------------------------------------------------------------------- the layout

QWidget* AerodynamicLookupDialog::buildSection(Table table)
{
    Section&      s      = section(table);
    const bool    drag   = table == Table::DRAG;
    const QString prefix = drag ? QStringLiteral("drag") : QStringLiteral("stability");
    auto* const   page   = makeChild<QWidget>(m_tabs);
    // Java: MigLayout("fill, ins para, gap para").
    auto* const grid = makeChild<QGridLayout>(page);

    // Load from file button and the refresh button in one cell, Clear beside them.
    auto* loadRow = makeChild<QWidget>(page);
    auto* load    = makeChild<QHBoxLayout>(loadRow);
    load->setContentsMargins(0, 0, 0, 0);
    //: AerodynamicLookupDialog.btn.loadFromFile
    s.load = makeButton(loadRow, tr("Load from file..."), prefix + QStringLiteral("LoadButton"));
    load->addWidget(s.load);
    s.refresh = makeButton(loadRow, QString(), prefix + QStringLiteral("RefreshButton"));
    s.refresh->setIcon(Icons::icon(Icons::Id::REFRESH));
    //: AerodynamicLookupDialog.btn.refresh.ttip
    s.refresh->setToolTip(tr("Reload data from the CSV file if it exists"));
    s.refresh->setEnabled(false);  // Initially disabled until data is loaded
    load->addWidget(s.refresh);
    grid->addWidget(loadRow, 0, 0, Qt::AlignLeft);
    //: AerodynamicLookupDialog.btn.clear
    s.clear = makeButton(
        page, tr("Clear"),
        drag ? QStringLiteral("clearDragButton") : QStringLiteral("clearStabilityButton"));
    grid->addWidget(s.clear, 0, 1, Qt::AlignLeft);

    // Summary label (on its own row)
    s.summary = makeChild<QLabel>(page);
    s.summary->setObjectName(prefix + QStringLiteral("SummaryLabel"));
    s.summary->setTextFormat(Qt::RichText);
    grid->addWidget(s.summary, 1, 0, 1, 3);

    // Field separator
    const QString tip =
        //: SimExpPan.lbl.longA1
        tr("<html>The string used to separate the fields in the exported file. <br>") +
        //: SimExpPan.lbl.longA2
        tr("Use ',' for a Comma Separated Values (CSV) file.");
    //: SimExpPan.lbl.Fieldsepstr
    auto* separatorLabel = makeChild<QLabel>(page, tr("Field separator string:"));
    separatorLabel->setToolTip(tip);
    grid->addWidget(separatorLabel, 2, 0);
    s.separatorCombo = makeChild<FieldSeparatorComboBox>(page);
    s.separatorCombo->setObjectName(prefix + QStringLiteral("SeparatorCombo"));
    s.separatorCombo->setToolTip(tip);
    grid->addWidget(s.separatorCombo, 2, 1, Qt::AlignLeft);

    // Example format textarea
    //: AerodynamicLookupDialog.lbl.formatHint
    s.textBox = makeChild<QGroupBox>(page, tr("Example format:"));
    s.textBox->setObjectName(prefix + QStringLiteral("ExampleBorder"));
    auto* boxLayout = makeChild<QVBoxLayout>(s.textBox);
    s.text          = makeChild<QPlainTextEdit>(s.textBox);
    s.text->setObjectName(prefix + QStringLiteral("ExampleArea"));
    QFont fixed = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    fixed.setPointSizeF(s.text->font().pointSizeF());
    s.text->setFont(fixed);
    s.text->setLineWrapMode(QPlainTextEdit::NoWrap);
    s.text->setMinimumHeight((QFontMetrics(fixed).lineSpacing() * kTextRows) +
                             (2 * s.text->frameWidth()) +
                             (2 * static_cast<int>(s.text->document()->documentMargin())));
    boxLayout->addWidget(s.text);
    grid->addWidget(s.textBox, 3, 0, 1, 3);
    grid->setRowStretch(3, 1);
    grid->setColumnStretch(2, 1);

    connect(s.load, &QPushButton::clicked, this,
            guarded([this, table] { chooseLookup(table); }, context()));
    connect(s.refresh, &QPushButton::clicked, this,
            guarded([this, table] { refreshLookup(table); }, context()));
    connect(s.clear, &QPushButton::clicked, this,
            guarded([this, table] { clearLookup(table); }, context()));
    // Just update the example format display, don't reload the file: the separator is only
    // used when loading a new file.
    connect(s.separatorCombo, &QComboBox::currentTextChanged, this,
            guarded([this, table] { updateExampleFormat(table); }, context()));
    connect(s.text, &QPlainTextEdit::textChanged, this,
            guarded(
                [this, table] {
                    if (!m_settingText && section(table).listening)
                    {
                        markModified(table);
                    }
                },
                context()));
    return page;
}

// --------------------------------------------------------------------------------- the actions

void AerodynamicLookupDialog::chooseLookup(Table table)
{
    if (liveOptions() == nullptr)
    {
        return;
    }
    // The services' objects outlive the dialog, which may go during the question.
    Preferences&                            preferences = m_services.preferences();
    UserPrompts&                            prompts     = m_services.prompts();
    const QPointer<AerodynamicLookupDialog> alive(this);

    UserPrompts::OpenFileRequest request;
    //: AerodynamicLookupDialog.title.choose
    request.title              = tr("Select CSV file");
    request.nameFilters        = QStringList{csvNameFilter(), FileNames::allFilesFilter()};
    request.selectedNameFilter = csvNameFilter();
    // Java selects the table's file, then sets the default directory: the name of the file in
    // the default directory.
    if (const std::optional<std::filesystem::path> directory = preferences.getDefaultDirectory())
    {
        request.directory = *directory;
    }
    if (const std::optional<std::filesystem::path>& csv = section(table).csv; csv.has_value())
    {
        request.fileName = QtStrings::fromPath(csv->filename());
    }
    const std::optional<UserPrompts::OpenFileAnswer> answer = prompts.askOpenFiles(this, request);
    if (alive.isNull() || !answer.has_value() || answer->files.empty())
    {
        return;
    }
    preferences.setDefaultDirectory(answer->directory);
    const std::filesystem::path selected  = answer->files.front();
    const std::optional<char>   separator = separatorFor(table, selected);
    if (!separator.has_value())
    {
        return;
    }
    Result<MachAoALookup> read =
        CsvMachAoALookup::fromCsv(selected, valueColumns(table), *separator);
    if (!read.has_value())
    {
        showLookupError(selected, QtStrings::fromUtf8(read.error().message));
        return;
    }
    storeReadTable(table, selected, std::make_shared<const MachAoALookup>(std::move(*read)),
                   section(table).separatorCombo->separatorChar());
}

void AerodynamicLookupDialog::reloadLookup(Table table)
{
    const std::optional<std::filesystem::path> csv = section(table).csv;
    if (!csv.has_value())
    {
        return;
    }
    const std::optional<char> separator = separatorFor(table, *csv);
    if (!separator.has_value())
    {
        return;
    }
    Result<MachAoALookup> read = CsvMachAoALookup::fromCsv(*csv, valueColumns(table), *separator);
    if (!read.has_value())
    {
        showLookupError(*csv, QtStrings::fromUtf8(read.error().message));
        return;
    }
    storeReadTable(table, *csv, std::make_shared<const MachAoALookup>(std::move(*read)),
                   section(table).separatorCombo->separatorChar());
}

void AerodynamicLookupDialog::storeReadTable(Table table, const std::filesystem::path& path,
                                             std::shared_ptr<const MachAoALookup> read,
                                             QChar                                separator)
{
    SimulationOptions* const options = liveOptions();
    if (options == nullptr)
    {
        return;
    }
    Section& s = section(table);
    // Clear stored CSV rows when loading new file - will read from file. The options make the
    // path absolute and normalise it (Java: normalizePath()); the section takes it from them.
    if (table == Table::DRAG)
    {
        options->setDragLookup(path, read, std::nullopt);
        s.csv = options->getDragLookupCsvPath();
    }
    else
    {
        options->setStabilityLookup(path, read, std::nullopt);
        s.csv = options->getStabilityLookupCsvPath();
    }
    s.table     = std::move(read);
    s.separator = separator;
    updateDisplays(table);
}

void AerodynamicLookupDialog::refreshLookup(Table table)
{
    const std::optional<std::filesystem::path> csv = section(table).csv;
    if (!csv.has_value())
    {
        return;
    }
    std::error_code error;
    if (!std::filesystem::exists(*csv, error))
    {
        m_services.prompts().showMessage(
            this, UserPrompts::Message{
                      .severity = UserPrompts::Severity::WARNING,
                      //: AerodynamicLookupDialog.error.title
                      .title = tr("Failed to load CSV"),
                      //: AerodynamicLookupDialog.error.fileNotFound (Java's %n is the line break)
                      .lines = messageLines(
                          tr("The CSV file does not exist:\n%1").arg(QtStrings::fromPath(*csv))),
                  });
        return;
    }
    reloadLookup(table);
}

void AerodynamicLookupDialog::clearLookup(Table table)
{
    Section& s = section(table);
    s.csv.reset();
    s.table.reset();
    updateDisplays(table);
}

bool AerodynamicLookupDialog::parseEdit(Table table, std::optional<std::vector<std::string>>& rows)
{
    Section& s = section(table);
    if (!s.modified || !s.csv.has_value())
    {
        return true;
    }
    const std::optional<char> separator = separatorFor(table, *s.csv);
    if (!separator.has_value())
    {
        return false;
    }
    // Store all lines (including comments) to preserve them (Java: String.lines(), each
    // trimmed).
    const std::vector<std::string> lines =
        CsvMachAoALookup::splitLines(QtStrings::toUtf8(s.text->toPlainText()));
    std::vector<std::string> allLines;
    allLines.reserve(lines.size());
    for (const std::string& line : lines)
    {
        allLines.emplace_back(Strings::trim(line));
    }
    // Filter comments only when parsing for the table
    std::vector<std::string> dataLines;
    for (const std::string& line : allLines)
    {
        if (!line.empty() && !line.starts_with('#'))
        {
            dataLines.push_back(line);
        }
    }
    Result<MachAoALookup> parsed =
        CsvMachAoALookup::parse(dataLines, valueColumns(table), *separator);
    if (!parsed.has_value())
    {
        showLookupError(*s.csv, QtStrings::fromUtf8(parsed.error().message));
        return false;  // Don't close dialog on error
    }
    s.table = std::make_shared<const MachAoALookup>(std::move(*parsed));
    rows    = std::move(allLines);  // Store all lines including comments
    return true;
}

bool AerodynamicLookupDialog::applyAndClose()
{
    SimulationOptions* const options = liveOptions();
    if (options == nullptr)
    {
        return false;
    }
    // Validate and apply the lookups that were modified
    std::optional<std::vector<std::string>> dragRows = options->getDragLookupCsvRows();
    if (!parseEdit(Table::DRAG, dragRows))
    {
        return false;
    }
    std::optional<std::vector<std::string>> stabilityRows = options->getStabilityLookupCsvRows();
    if (!parseEdit(Table::STABILITY, stabilityRows))
    {
        return false;
    }

    // Apply the lookups
    const Section& drag = section(Table::DRAG);
    if (!drag.csv.has_value() || drag.table == nullptr)
    {
        options->clearDragLookup();
    }
    else
    {
        options->setDragLookup(drag.csv, drag.table, std::move(dragRows));
    }
    const Section& stability = section(Table::STABILITY);
    if (!stability.csv.has_value() || stability.table == nullptr)
    {
        options->clearStabilityLookup();
    }
    else
    {
        options->setStabilityLookup(stability.csv, stability.table, std::move(stabilityRows));
    }

    section(Table::DRAG).modified      = false;
    section(Table::STABILITY).modified = false;
    accept();
    return true;
}

// --------------------------------------------------------------------------------- the display

void AerodynamicLookupDialog::updateDisplays(std::optional<Table> changed)
{
    // A bug of OpenRocket's, found while porting, fixed: Java shows both sections again from the
    // options, so that a load, a refresh or Clear of one table throws away an edit of the other
    // table's text (its "(modified)" goes and OK keeps the options' rows). The other section is
    // shown again only while it has no edit.
    const auto shown = [this, changed](Table table) {
        return !changed.has_value() || *changed == table || !section(table).modified;
    };
    const bool drag      = shown(Table::DRAG);
    const bool stability = shown(Table::STABILITY);
    if (drag)
    {
        updateSectionDisplay(Table::DRAG);
    }
    if (stability)
    {
        updateSectionDisplay(Table::STABILITY);
    }
    if (drag)
    {
        updateExampleFormat(Table::DRAG);
    }
    if (stability)
    {
        updateExampleFormat(Table::STABILITY);
    }
}

void AerodynamicLookupDialog::updateSectionDisplay(Table table)
{
    Section& s = section(table);
    // Reset modification flag when clearing or loading new file
    s.modified          = false;
    const bool hasTable = s.csv.has_value() && s.table != nullptr;
    // Update separator combo box if a file is loaded
    if (hasTable)
    {
        s.separatorCombo->setSeparatorChar(s.separator);
    }
    if (!hasTable)
    {
        updateSummaryLabel(table);
        s.clear->setEnabled(false);
        // Disable refresh button when no data is loaded
        s.refresh->setEnabled(false);
        return;
    }
    updateSummaryLabel(table);
    s.clear->setEnabled(true);
    // Enable refresh button when data is loaded
    s.refresh->setEnabled(true);
}

void AerodynamicLookupDialog::updateExampleFormat(Table table)
{
    Section&    s         = section(table);
    const QChar separator = s.separatorCombo->separatorChar();
    if (s.csv.has_value() && s.table != nullptr)
    {
        // Show loaded data - make editable. Use stored CSV rows if available (preserves edits),
        // otherwise read from file.
        std::optional<std::vector<std::string>> lines;
        if (const SimulationOptions* const options = liveOptions(); options != nullptr)
        {
            lines = table == Table::DRAG ? options->getDragLookupCsvRows()
                                         : options->getStabilityLookupCsvRows();
        }
        if (!lines.has_value() || lines->empty())
        {
            // No stored rows, read from file
            Result<std::vector<std::string>> read = CsvMachAoALookup::readLines(*s.csv);
            if (read.has_value())
            {
                lines = std::move(*read);
            }
            else
            {
                lines.reset();
            }
        }
        if (lines.has_value())
        {
            // The editable text is the complete table, including comments and empty lines.
            QStringList text;
            for (const std::string& line : *lines)
            {
                text.append(QtStrings::fromUtf8(line));
            }
            setAreaText(table, text.join(u'\n'));
            s.text->setReadOnly(false);
            s.text->setPalette(textColors(s.text->palette(), true));
            //: AerodynamicLookupDialog.lbl.loadedData
            s.textBox->setTitle(tr("Loaded data:"));
            s.listening = true;
            return;
        }
        // If we can't read the file, fall back to example
    }
    // Show example format
    setAreaText(table, exampleText(table, separator));
    s.text->setReadOnly(true);
    s.text->setPalette(textColors(s.text->palette(), false));
    //: AerodynamicLookupDialog.lbl.formatHint
    s.textBox->setTitle(tr("Example format:"));
    s.listening = false;
}

void AerodynamicLookupDialog::markModified(Table table)
{
    Section& s = section(table);
    if (!s.modified)
    {
        s.modified = true;
        updateSummaryLabel(table);
    }
}

void AerodynamicLookupDialog::updateSummaryLabel(Table table)
{
    const Section& s = section(table);
    if (!s.csv.has_value() || s.table == nullptr)
    {
        s.summary->setText(QStringLiteral("<html>") + summaryText(std::nullopt, nullptr) +
                           QStringLiteral("</html>"));
        s.summary->setToolTip(QString());
        return;
    }
    QString fullText = summaryText(s.csv, s.table.get());
    QString escaped  = QtStrings::fromUtf8(Strings::escapeHtml(QtStrings::toUtf8(fullText)));
    if (s.modified)
    {
        // Java's literals.
        escaped += QStringLiteral(" <i>(modified)</i>");
        fullText += QStringLiteral(" (modified)");
    }
    s.summary->setText(QStringLiteral("<html>") + escaped + QStringLiteral("</html>"));
    s.summary->setToolTip(fullText);
}

void AerodynamicLookupDialog::setAreaText(Table table, const QString& text)
{
    m_settingText = true;
    section(table).text->setPlainText(text);
    m_settingText = false;
}

void AerodynamicLookupDialog::showLookupError(const std::filesystem::path& path,
                                              const QString&               reason)
{
    m_services.prompts().showMessage(
        this, UserPrompts::Message{
                  .severity = UserPrompts::Severity::ERROR,
                  .title    = tr("Failed to load CSV"),
                  //: AerodynamicLookupDialog.error.msg (Java's %n is the line break)
                  .lines = messageLines(
                      tr("Failed to load %1:\n%2").arg(QtStrings::fromPath(path), reason)),
              });
}

std::optional<char> AerodynamicLookupDialog::separatorFor(Table                        table,
                                                          const std::filesystem::path& path)
{
    const QChar separator = section(table).separatorCombo->separatorChar();
    if (separator.unicode() > 0x7F)
    {
        //: (QtRocket's own text: the separator of a lookup table must be one byte)
        showLookupError(path, tr("The field separator must be an ASCII character."));
        return std::nullopt;
    }
    return static_cast<char>(separator.unicode());
}

void AerodynamicLookupDialog::changeEvent(QEvent* event)
{
    QDialog::changeEvent(event);
    if (event->type() == QEvent::PaletteChange)
    {
        updateColors();
    }
}

void AerodynamicLookupDialog::updateColors()
{
    const QColor info = ThemeColors::color(ThemeColors::Key::INFO, palette());
    for (const Section& s : m_sections)
    {
        QPalette summary = s.summary->palette();
        summary.setColor(QPalette::WindowText, info);
        s.summary->setPalette(summary);
    }
}

// ---------------------------------------------------------------------------------- the texts

QString AerodynamicLookupDialog::formatLookupSummary(const MachAoALookup& table)
{
    QStringList columns;
    for (const std::string& name : table.getValueColumns())
    {
        columns.append(QtStrings::fromUtf8(name).toUpper());
    }
    const DecimalFormat format("0.###");
    const auto          number = [&format](double value) {
        return QtStrings::fromUtf8(format.format(value));
    };
    const QString machMin = number(table.getMinMach());
    const QString machMax = number(table.getMaxMach());
    if (table.hasAoA())
    {
        //: AerodynamicLookupDialog.summaryWithAoA
        return tr("Mach %1 \u2013 %2; AoA %3 \u2013 %4\u00B0; columns: %5")
            .arg(machMin, machMax, number(table.getMinAoA()), number(table.getMaxAoA()),
                 columns.join(QStringLiteral(", ")));
    }
    //: AerodynamicLookupDialog.summaryNoAoA
    return tr("Mach %1 \u2013 %2; columns: %3")
        .arg(machMin, machMax, columns.join(QStringLiteral(", ")));
}

QString AerodynamicLookupDialog::summaryText(const std::optional<std::filesystem::path>& path,
                                             const MachAoALookup*                        table)
{
    if (!path.has_value() || table == nullptr)
    {
        //: AerodynamicLookupDialog.summary.none
        return tr("Using built-in Barrowman data");
    }
    const std::filesystem::path name = path->filename();
    return QtStrings::fromPath(name.empty() ? *path : name) + QStringLiteral(" - ") +
           formatLookupSummary(*table);
}

QString AerodynamicLookupDialog::csvNameFilter()
{
    // Qt reads a filter's patterns from its parentheses, separated by spaces.
    //: AerodynamicLookupDialog.csvFilter
    return tr("CSV files (*.csv, *.txt, *.dat)")
        .replace(QStringLiteral(", *"), QStringLiteral(" *"));
}

QString AerodynamicLookupDialog::exampleText(Table table, QChar separator)
{
    const QString example =
        table == Table::DRAG
            //: AerodynamicLookupDialog.example.drag
            ? tr("Mach,AoA,Cd\n0.30,0,0.35\n0.30,5,0.42")
            //: AerodynamicLookupDialog.example.stability
            : tr("Mach,AoA,Cn,Cm,Cp\n0.30,0,0.12,-0.01,1.45\n0.30,5,0.15,-0.03,1.40");
    // Replace commas with the selected separator
    QString text = example;
    return text.replace(u',', separator);
}

// --------------------------------------------------------------------------------- the access

AerodynamicLookupDialog::Section& AerodynamicLookupDialog::section(Table table) noexcept
{
    return table == Table::DRAG ? std::get<0>(m_sections) : std::get<1>(m_sections);
}

const AerodynamicLookupDialog::Section& AerodynamicLookupDialog::section(Table table) const noexcept
{
    return table == Table::DRAG ? std::get<0>(m_sections) : std::get<1>(m_sections);
}

const std::vector<std::string>& AerodynamicLookupDialog::valueColumns(Table table)
{
    static const std::vector<std::string> kDrag{"cd"};
    static const std::vector<std::string> kStability{"cn", "cm", "cp"};
    return table == Table::DRAG ? kDrag : kStability;
}

SimulationOptions* AerodynamicLookupDialog::liveOptions() const
{
    return m_options->isValid() ? &m_options->getObject<SimulationOptions>() : nullptr;
}

QPushButton& AerodynamicLookupDialog::loadButton(Table table) const noexcept
{
    return *section(table).load;
}

QPushButton& AerodynamicLookupDialog::refreshButton(Table table) const noexcept
{
    return *section(table).refresh;
}

QPushButton& AerodynamicLookupDialog::clearButton(Table table) const noexcept
{
    return *section(table).clear;
}

QLabel& AerodynamicLookupDialog::summaryLabel(Table table) const noexcept
{
    return *section(table).summary;
}

FieldSeparatorComboBox& AerodynamicLookupDialog::separatorComboBox(Table table) const noexcept
{
    return *section(table).separatorCombo;
}

QGroupBox& AerodynamicLookupDialog::textBox(Table table) const noexcept
{
    return *section(table).textBox;
}

QPlainTextEdit& AerodynamicLookupDialog::textArea(Table table) const noexcept
{
    return *section(table).text;
}

const std::optional<std::filesystem::path>& AerodynamicLookupDialog::csvPath(
    Table table) const noexcept
{
    return section(table).csv;
}

const std::shared_ptr<const MachAoALookup>& AerodynamicLookupDialog::lookupTable(
    Table table) const noexcept
{
    return section(table).table;
}

bool AerodynamicLookupDialog::isModified(Table table) const noexcept
{
    return section(table).modified;
}

}  // namespace QtRocket::Gui
