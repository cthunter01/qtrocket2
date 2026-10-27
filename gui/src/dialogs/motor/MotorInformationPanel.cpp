#include "QtRocket/gui/dialogs/motor/MotorInformationPanel.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <format>
#include <iterator>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <QChar>
#include <QColor>
#include <QDateTime>
#include <QEvent>
#include <QFont>
#include <QFontMetrics>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMargins>
#include <QPalette>
#include <QPlainTextEdit>
#include <QSize>
#include <QString>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextOption>
#include <QTimeZone>
#include <QWidget>
#include <Qt>
#include <QtTypes>

#include "QtRocket/gui/dialogs/ThrustCurveMotorPlotDialog.h"
#include "QtRocket/gui/plot/ThrustCurvePlot.h"
#include "QtRocket/gui/plot/ThrustCurvePreview.h"
#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/util/ThemeColors.h"
#include "QtRocket/motor/CaseInfo.h"
#include "QtRocket/motor/Manufacturer.h"
#include "QtRocket/motor/Motor.h"
#include "QtRocket/motor/ThrustCurveMotor.h"
#include "QtRocket/motor/chooser/MotorClass.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket::Gui
{

namespace
{

/// The context of the panel's guarded slots.
[[nodiscard]] QString context()
{
    return QStringLiteral("Motor information panel");
}

/// The description: a read-only plain-text area (Java's JTextArea) whose preferred size is
/// kCommentRows rows of five columns (new JTextArea(5, 5)) in a scroll pane.
class CommentArea final : public QPlainTextEdit
{
public:
    explicit CommentArea(QWidget* parent) : QPlainTextEdit(parent) { }

    [[nodiscard]] QSize sizeHint() const override
    {
        const QFontMetrics metrics(font());
        const int          frame    = 2 * frameWidth();
        const QMargins     margins  = contentsMargins();
        constexpr int      kColumns = 5;
        return {(kColumns * metrics.horizontalAdvance(QLatin1Char('m'))) + frame + margins.left() +
                    margins.right(),
                (MotorInformationPanel::kCommentRows * metrics.lineSpacing()) +
                    (2 * static_cast<int>(document()->documentMargin())) + frame + margins.top() +
                    margins.bottom()};
    }
};

// ---------------------------------------------------------------- Java's Instant.parse()

/// The fields of an ISO-8601 instant as DateTimeFormatter.ISO_INSTANT reads it.
struct InstantFields
{
    std::int64_t year{0};
    int          month{0};
    int          day{0};
    int          hour{0};
    int          minute{0};
    int          second{0};
    /// Whether the fraction of the second is not zero (only 24:00:00 cares).
    bool fraction{false};
    /// The offset from UTC in seconds.
    int offset{0};
};

/// Reads the text @p text from @p position on, one ASCII character at a time.
class InstantReader
{
public:
    explicit InstantReader(std::string_view text) noexcept : m_text(text) { }

    [[nodiscard]] bool atEnd() const noexcept { return m_position == m_text.size(); }

    /// Takes @p c (or, with @p ignoreCase, its other ASCII case) when it comes next.
    [[nodiscard]] bool take(char c, bool ignoreCase = false) noexcept
    {
        if (atEnd())
        {
            return false;
        }
        const char next = m_text[m_position];
        if (next == c || (ignoreCase && (next | 0x20) == (c | 0x20)))
        {
            ++m_position;
            return true;
        }
        return false;
    }

    /// The number of the ASCII digits that come next, between @p least and @p most of them, as
    /// many as there are; nullopt when fewer than @p least.
    [[nodiscard]] std::optional<std::int64_t> digits(std::size_t least, std::size_t most) noexcept
    {
        std::int64_t value = 0;
        std::size_t  count = 0;
        while (count < most && !atEnd() && m_text[m_position] >= '0' && m_text[m_position] <= '9')
        {
            value = (value * 10) + (m_text[m_position] - '0');
            ++m_position;
            ++count;
        }
        if (count < least)
        {
            return std::nullopt;
        }
        m_lastCount = count;
        return value;
    }

    /// How many digits the last digits() read.
    [[nodiscard]] std::size_t lastCount() const noexcept { return m_lastCount; }

    /// Whether a digit comes next.
    [[nodiscard]] bool digitNext() const noexcept
    {
        return !atEnd() && m_text[m_position] >= '0' && m_text[m_position] <= '9';
    }

private:
    std::string_view m_text;
    std::size_t      m_position{0};
    std::size_t      m_lastCount{0};
};

/// Two digits from @p reader, at most @p maximum; nullopt otherwise.
[[nodiscard]] std::optional<int> twoDigits(InstantReader& reader, int maximum)
{
    const std::optional<std::int64_t> value = reader.digits(2, 2);
    if (!value.has_value() || *value > maximum)
    {
        return std::nullopt;
    }
    return static_cast<int>(*value);
}

/// The proleptic Gregorian leap year rule.
[[nodiscard]] constexpr bool isLeapYear(std::int64_t year) noexcept
{
    return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
}

/// The days of @p month (1 to 12) in @p year.
[[nodiscard]] constexpr int daysInMonth(std::int64_t year, int month) noexcept
{
    if (month == 2)
    {
        return isLeapYear(year) ? 29 : 28;
    }
    return month == 4 || month == 6 || month == 9 || month == 11 ? 30 : 31;
}

/// The year: four digits without a sign, a '-' and four or more digits, or a '+' and five or more
/// (ISO_INSTANT's year field, at most ten digits).
[[nodiscard]] std::optional<std::int64_t> readYear(InstantReader& reader)
{
    constexpr std::size_t kPadded    = 4;
    constexpr std::size_t kMaxDigits = 10;
    if (reader.take('+'))
    {
        const std::optional<std::int64_t> year = reader.digits(kPadded + 1, kMaxDigits);
        return year;
    }
    if (reader.take('-'))
    {
        const std::optional<std::int64_t> year = reader.digits(kPadded, kMaxDigits);
        if (!year.has_value() || *year == 0)
        {
            return std::nullopt;
        }
        return -*year;
    }
    const std::optional<std::int64_t> year = reader.digits(kPadded, kPadded);
    if (!year.has_value() || reader.digitNext())
    {
        return std::nullopt;
    }
    return year;
}

/// The date "<year>-MM-dd" into @p fields.
[[nodiscard]] bool readDate(InstantReader& reader, InstantFields& fields)
{
    constexpr int                     kMonths = 12;
    const std::optional<std::int64_t> year    = readYear(reader);
    if (!year.has_value() || !reader.take('-'))
    {
        return false;
    }
    const std::optional<int> month = twoDigits(reader, kMonths);
    if (!month.has_value() || *month == 0 || !reader.take('-'))
    {
        return false;
    }
    const std::optional<int> day = twoDigits(reader, daysInMonth(*year, *month));
    if (!day.has_value() || *day == 0)
    {
        return false;
    }
    fields.year  = *year;
    fields.month = *month;
    fields.day   = *day;
    return true;
}

/// The time "HH:mm:ss[.fraction]" into @p fields: 24:00:00 (a fraction of zero) and 23:59:60
/// (the leap second) are kept as read.
[[nodiscard]] bool readTime(InstantReader& reader, InstantFields& fields)
{
    constexpr int            kLastHour   = 24;
    constexpr int            kLastMinute = 59;
    constexpr int            kLeapSecond = 60;
    const std::optional<int> hour        = twoDigits(reader, kLastHour);
    if (!hour.has_value() || !reader.take(':'))
    {
        return false;
    }
    const std::optional<int> minute = twoDigits(reader, kLastMinute);
    if (!minute.has_value() || !reader.take(':'))
    {
        return false;
    }
    const std::optional<int> second = twoDigits(reader, kLeapSecond);
    if (!second.has_value())
    {
        return false;
    }
    bool fraction = false;
    if (reader.take('.'))
    {
        constexpr std::size_t             kNanoDigits = 9;
        const std::optional<std::int64_t> digits      = reader.digits(0, kNanoDigits);
        if (!digits.has_value() || reader.digitNext())
        {
            return false;
        }
        fraction = *digits != 0;
    }
    constexpr int kLastRegularHour = 23;
    if (*hour == kLastHour && (*minute != 0 || *second != 0 || fraction))
    {
        return false;
    }
    if (*second == kLeapSecond && (*hour != kLastRegularHour || *minute != kLastMinute))
    {
        return false;
    }
    fields.hour     = *hour;
    fields.minute   = *minute;
    fields.second   = *second;
    fields.fraction = fraction;
    return true;
}

/// The offset "Z" (any case), or "+HH:MM" or "+HH:MM:SS" (or '-'), at most 18 hours, into
/// @p fields.
[[nodiscard]] bool readOffset(InstantReader& reader, InstantFields& fields)
{
    if (reader.take('Z', true))
    {
        fields.offset = 0;
        return true;
    }
    int sign = 1;
    if (reader.take('-'))
    {
        sign = -1;
    }
    else if (!reader.take('+'))
    {
        return false;
    }
    constexpr int            kMaxHours       = 18;
    constexpr int            kLastMinute     = 59;
    constexpr int            kSecondsPerHour = 3600;
    constexpr int            kSecondsPerMin  = 60;
    const std::optional<int> hours           = twoDigits(reader, kMaxHours);
    if (!hours.has_value() || !reader.take(':'))
    {
        return false;
    }
    const std::optional<int> minutes = twoDigits(reader, kLastMinute);
    if (!minutes.has_value())
    {
        return false;
    }
    int seconds = 0;
    if (reader.take(':'))
    {
        const std::optional<int> read = twoDigits(reader, kLastMinute);
        if (!read.has_value())
        {
            return false;
        }
        seconds = *read;
    }
    const int total = (*hours * kSecondsPerHour) + (*minutes * kSecondsPerMin) + seconds;
    if (total > kMaxHours * kSecondsPerHour)
    {
        return false;
    }
    fields.offset = sign * total;
    return true;
}

/// The days from 1970-01-01 to the date @p year-@p month-@p day of the proleptic Gregorian
/// calendar (days_from_civil() of Howard Hinnant's "chrono-Compatible Low-Level Date Algorithms",
/// in 64 bits).
[[nodiscard]] constexpr std::int64_t daysFromCivil(std::int64_t year, int month, int day) noexcept
{
    const std::int64_t y         = month <= 2 ? year - 1 : year;
    const std::int64_t era       = (y >= 0 ? y : y - 399) / 400;
    const std::int64_t yearOfEra = y - (era * 400);
    const std::int64_t dayOfYear =
        (((153 * (month > 2 ? month - 3 : month + 9)) + 2) / 5) + day - 1;
    const std::int64_t dayOfEra =
        (yearOfEra * 365) + (yearOfEra / 4) - (yearOfEra / 100) + dayOfYear;
    return (era * 146097) + dayOfEra - 719468;
}

/// A date of the proleptic Gregorian calendar.
struct CivilDate
{
    std::int64_t year{0};
    int          month{0};
    int          day{0};
};

/// The date @p days after 1970-01-01 (civil_from_days() of the same paper: the inverse of
/// daysFromCivil()).
[[nodiscard]] constexpr CivilDate civilFromDays(std::int64_t days) noexcept
{
    const std::int64_t z        = days + 719468;
    const std::int64_t era      = (z >= 0 ? z : z - 146096) / 146097;
    const std::int64_t dayOfEra = z - (era * 146097);
    const std::int64_t yearOfEra =
        (dayOfEra - (dayOfEra / 1460) + (dayOfEra / 36524) - (dayOfEra / 146096)) / 365;
    const std::int64_t dayOfYear =
        dayOfEra - ((365 * yearOfEra) + (yearOfEra / 4) - (yearOfEra / 100));
    const std::int64_t shifted = ((5 * dayOfYear) + 2) / 153;
    const int          day     = static_cast<int>(dayOfYear - (((153 * shifted) + 2) / 5) + 1);
    const int          month   = static_cast<int>(shifted < 10 ? shifted + 3 : shifted - 9);
    return {.year = yearOfEra + (era * 400) + (month <= 2 ? 1 : 0), .month = month, .day = day};
}

/// Java's Instant range: -1000000000-01-01T00:00:00Z to +1000000000-12-31T23:59:59.999999999Z, in
/// seconds since the epoch.
constexpr std::int64_t kMinInstantSecond = -31557014167219200;
constexpr std::int64_t kMaxInstantSecond = 31556889864403199;

/// Java's Instant.parse(@p text): the instant's seconds since 1970-01-01T00:00:00Z (a leap second
/// counts as the second before it, 24:00:00 as the next day's start); nullopt for a text Java
/// refuses.
[[nodiscard]] std::optional<std::int64_t> parseInstant(std::string_view text)
{
    InstantReader reader(text);
    InstantFields fields;
    if (!readDate(reader, fields) || !reader.take('T', true) || !readTime(reader, fields) ||
        !readOffset(reader, fields) || !reader.atEnd())
    {
        return std::nullopt;
    }
    constexpr int          kLastHour       = 24;
    constexpr int          kLeapSecond     = 60;
    constexpr std::int64_t kSecondsPerDay  = 86400;
    constexpr std::int64_t kSecondsPerHour = 3600;
    constexpr std::int64_t kSecondsPerMin  = 60;
    std::int64_t           days            = daysFromCivil(fields.year, fields.month, fields.day);
    int                    hour            = fields.hour;
    if (hour == kLastHour)
    {
        hour = 0;
        ++days;
    }
    const int          second      = fields.second == kLeapSecond ? kLeapSecond - 1 : fields.second;
    const std::int64_t epochSecond = (days * kSecondsPerDay) + (hour * kSecondsPerHour) +
                                     (fields.minute * kSecondsPerMin) + second - fields.offset;
    if (epochSecond < kMinInstantSecond || epochSecond > kMaxInstantSecond)
    {
        return std::nullopt;
    }
    return epochSecond;
}

/// Java's LocalDate.toString(): the year in four digits at least, "+" before one of more, "-"
/// before a negative one; then "-MM-dd".
[[nodiscard]] std::string localDateText(const CivilDate& date)
{
    constexpr std::int64_t kFourDigits = 9999;
    std::string            year;
    if (date.year > kFourDigits)
    {
        year = std::format("+{}", date.year);
    }
    else if (date.year < 0)
    {
        year = std::format("-{:04}", -date.year);
    }
    else
    {
        year = std::format("{:04}", date.year);
    }
    return std::format("{}-{:02}-{:02}", year, date.month, date.day);
}

}  // namespace

MotorInformationPanel::MotorInformationPanel(const GuiServices& services, QWidget* parent)
  : QWidget(parent), m_services(services)
{
    setObjectName(QStringLiteral("motorInformationPanel"));
    // MigLayout("fill"): the labels' column, the values' column taking the width.
    auto* layout = makeChild<QGridLayout>(this);
    layout->setColumnStretch(1, 1);

    //: TCMotorSelPan.lbl.Manufacturer
    m_manufacturerLabel =
        &addRow(*layout, tr("Manufacturer:"), QStringLiteral("manufacturerLabel"));
    //: TCMotorSelPan.lbl.Designation
    m_designationLabel = &addRow(*layout, tr("Designation:"), QStringLiteral("designationLabel"));
    //: TCMotorSelPan.lbl.CommonName
    m_commonNameLabel = &addRow(*layout, tr("Common name:"), QStringLiteral("commonNameLabel"));

    //// Total impulse: the impulse, then its class ("split", "gapleft unrel").
    //: TCMotorSelPan.lbl.Totalimpulse
    m_totalImpulseLabel =
        &addRow(*layout, tr("Total impulse:"), QStringLiteral("totalImpulseLabel"));
    layout->removeWidget(m_totalImpulseLabel);
    auto* impulseRow    = makeChild<QWidget>(this);
    auto* impulseLayout = makeChild<QHBoxLayout>(impulseRow);
    impulseLayout->setContentsMargins(0, 0, 0, 0);
    impulseLayout->setSpacing(0);
    impulseLayout->addWidget(m_totalImpulseLabel);
    impulseLayout->addSpacing(kUnrelatedGap);
    m_classificationLabel = makeChild<QLabel>(impulseRow);
    m_classificationLabel->setObjectName(QStringLiteral("classificationLabel"));
    impulseLayout->addWidget(m_classificationLabel);
    impulseLayout->addStretch(1);
    layout->addWidget(impulseRow, m_nextRow - 1, 1);

    //: TCMotorSelPan.lbl.Avgthrust
    m_avgThrustLabel = &addRow(*layout, tr("Avg. thrust:"), QStringLiteral("avgThrustLabel"));
    //: TCMotorSelPan.lbl.Maxthrust
    m_maxThrustLabel = &addRow(*layout, tr("Max. thrust:"), QStringLiteral("maxThrustLabel"));
    //: TCMotorSelPan.lbl.Burntime
    m_burnTimeLabel = &addRow(*layout, tr("Burn time:"), QStringLiteral("burnTimeLabel"));
    //: TCMotorSelPan.lbl.Launchmass
    m_launchMassLabel = &addRow(*layout, tr("Launch mass:"), QStringLiteral("launchMassLabel"));
    //: TCMotorSelPan.lbl.Emptymass
    m_emptyMassLabel = &addRow(*layout, tr("Empty mass:"), QStringLiteral("emptyMassLabel"));
    //: TCMotorSelPan.lbl.Motortype
    m_motorTypeLabel = &addRow(*layout, tr("Motor type:"), QStringLiteral("motorTypeLabel"));
    //: TCMotorSelPan.lbl.Caseinfo
    m_caseInfoLabel = &addRow(*layout, tr("Case info:"), QStringLiteral("caseInfoLabel"));
    //: TCMotorSelPan.lbl.Propinfo
    m_propInfoLabel = &addRow(*layout, tr("Propellant:"), QStringLiteral("propInfoLabel"));
    //: TCMotorSelPan.lbl.CompatibleCases
    m_compatibleCasesLabel =
        &addRow(*layout, tr("Compatible Cases:"), QStringLiteral("compatibleCasesLabel"));
    m_compatibleCasesLabel->setWordWrap(true);
    //: TCMotorSelPan.lbl.Datapoints
    m_dataPointsLabel = &addRow(*layout, tr("Data points:"), QStringLiteral("dataPointsLabel"));
    // "wrap para"
    layout->setRowMinimumHeight(m_nextRow++, kParagraphGap);

    //: TCMotorSelPan.lbl.LastUpdatedOn
    m_lastUpdatedOnLabel =
        &addRow(*layout, tr("Last updated:"), QStringLiteral("lastUpdatedOnLabel"));
    //: TCMotorSelPan.lbl.Source
    m_sourceLabel = &addRow(*layout, tr("Source:"), QStringLiteral("sourceLabel"));

    makeDescriptionAndPreview(*layout);

    clearData();
    updateColors();
}

MotorInformationPanel::~MotorInformationPanel() = default;

void MotorInformationPanel::makeDescriptionAndPreview(QGridLayout& layout)
{
    // The description: "spanx, grow, pushy, wrap para".
    m_comment = makeChild<CommentArea>(this);
    m_comment->setObjectName(QStringLiteral("comment"));
    m_comment->setReadOnly(true);
    m_comment->setLineWrapMode(QPlainTextEdit::WidgetWidth);
    m_comment->setWordWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    {
        QFont font = m_comment->font();
        if (font.pointSizeF() > 0.0)
        {
            font.setPointSizeF(std::max(1.0, font.pointSizeF() + kCommentFontChange));
        }
        else
        {
            font.setPixelSize(std::max(1, font.pixelSize() + static_cast<int>(kCommentFontChange)));
        }
        m_comment->setFont(font);
    }
    layout.addWidget(m_comment, m_nextRow, 0, 1, 2);
    layout.setRowStretch(m_nextRow++, 1);
    layout.setRowMinimumHeight(m_nextRow++, kParagraphGap);

    // The thrust curve plot: "width 300:300:, height 180:180:, grow, spanx".
    m_preview = makeChild<ThrustCurvePreview>(this);
    m_preview->setObjectName(QStringLiteral("layer"));
    layout.addWidget(m_preview, m_nextRow++, 0, 1, 2);
    connect(m_preview, &ThrustCurvePreview::plotRequested, this,
            guarded([this] { plotRequested(); }, context()));
}

QLabel& MotorInformationPanel::addRow(QGridLayout& grid, const QString& text, const QString& name)
{
    auto* label = makeChild<QLabel>(this, text);
    auto* value = makeChild<QLabel>(this);
    value->setObjectName(name);
    value->setTextFormat(Qt::PlainText);
    grid.addWidget(label, m_nextRow, 0);
    grid.addWidget(value, m_nextRow, 1);
    ++m_nextRow;
    return *value;
}

void MotorInformationPanel::clearData()
{
    m_selectedMotor.reset();
    m_curves.clear();
    for (QLabel* label :
         {m_manufacturerLabel, m_designationLabel, m_commonNameLabel, m_totalImpulseLabel,
          m_classificationLabel, m_avgThrustLabel, m_maxThrustLabel, m_burnTimeLabel,
          m_launchMassLabel, m_emptyMassLabel, m_motorTypeLabel, m_caseInfoLabel, m_propInfoLabel,
          m_compatibleCasesLabel, m_dataPointsLabel, m_lastUpdatedOnLabel, m_sourceLabel})
    {
        label->clear();
    }
    m_totalImpulseLabel->setToolTip(QString());
    m_classificationLabel->setToolTip(QString());
    setComment({});
    m_preview->chart().clear();
}

void MotorInformationPanel::updateData(
    std::span<const std::shared_ptr<const ThrustCurveMotor>> motors,
    const std::shared_ptr<const ThrustCurveMotor>&           selectedMotor)
{
    if (selectedMotor == nullptr)
    {
        clearData();
        return;
    }
    for (const std::shared_ptr<const ThrustCurveMotor>& motor : motors)
    {
        QTROCKET_ASSERT(motor != nullptr);
    }
    m_curves.assign(motors.begin(), motors.end());
    m_selectedMotor               = selectedMotor;
    const ThrustCurveMotor& motor = *selectedMotor;

    // Update thrust curve data
    m_manufacturerLabel->setText(QtStrings::fromUtf8(motor.getManufacturer().getDisplayName()));
    m_designationLabel->setText(QtStrings::fromUtf8(motor.getDesignation()));
    m_commonNameLabel->setText(QtStrings::fromUtf8(motor.getCommonName()));
    const double     impulse     = motor.getTotalImpulseEstimate();
    const MotorClass motorClass  = getMotorClass(impulse);
    const QString    description = QtStrings::fromUtf8(getClassDescription(motorClass));
    m_totalImpulseLabel->setText(QtStrings::defaultUnitText(UnitGroupId::IMPULSE, impulse));
    m_classificationLabel->setText(QStringLiteral("(") +
                                   QtStrings::fromUtf8(getDescription(motorClass, impulse)) +
                                   QStringLiteral(")"));
    m_totalImpulseLabel->setToolTip(description);
    m_classificationLabel->setToolTip(description);

    m_avgThrustLabel->setText(
        QtStrings::defaultUnitText(UnitGroupId::FORCE, motor.getAverageThrustEstimate()));
    m_maxThrustLabel->setText(
        QtStrings::defaultUnitText(UnitGroupId::FORCE, motor.getMaxThrustEstimate()));
    m_burnTimeLabel->setText(
        QtStrings::defaultUnitText(UnitGroupId::SHORT_TIME, motor.getBurnTimeEstimate()));
    m_launchMassLabel->setText(
        QtStrings::defaultUnitText(UnitGroupId::MASS, motor.getLaunchMass()));
    m_emptyMassLabel->setText(
        QtStrings::defaultUnitText(UnitGroupId::MASS, motor.getBurnoutMass()));
    m_motorTypeLabel->setText(QtStrings::fromUtf8(name(motor.getMotorType())));
    m_caseInfoLabel->setText(QtStrings::fromUtf8(motor.getCaseInfo()));
    m_propInfoLabel->setText(QtStrings::fromUtf8(motor.getPropellantInfo()));
    std::vector<std::string_view> cases;
    for (const CaseInfo caseInfo : motor.getCompatibleCases())
    {
        cases.push_back(toString(caseInfo));
    }
    m_compatibleCasesLabel->setText(QtStrings::fromUtf8(Strings::join(", ", cases)));
    m_dataPointsLabel->setText(QString::number(motor.getTimePoints().size() - 1));
    m_lastUpdatedOnLabel->setText(
        formatUpdatedOn(motor.getUpdatedOn(), QTimeZone::systemTimeZone()));
    m_sourceLabel->setText(formatSource(motor.getDataSource()));

    setComment(motor.getDescription());

    // Update the plot
    const auto found = std::ranges::find(m_curves, selectedMotor);
    const int  index =
        found == m_curves.end() ? -1 : static_cast<int>(std::distance(m_curves.begin(), found));
    m_preview->chart().setCurves(m_curves, index);
}

QString MotorInformationPanel::formatUpdatedOn(std::string_view updatedOn, const QTimeZone& zone)
{
    const std::string_view text = Strings::trim(updatedOn);
    if (text.empty())
    {
        return {};
    }
    const std::optional<std::int64_t> epochSecond = parseInstant(text);
    if (!epochSecond.has_value())
    {
        return QtStrings::fromUtf8(text);
    }
    // The zone's offset at the instant (QDateTime holds about 292 million years each side of
    // 1970: an instant beyond takes the offset there, which the zone's last rules give).
    constexpr std::int64_t kQtSecondsLimit = 9'000'000'000'000'000;
    const QDateTime        utc             = QDateTime::fromSecsSinceEpoch(
        std::clamp(*epochSecond, -kQtSecondsLimit, kQtSecondsLimit), QTimeZone::utc());
    constexpr std::int64_t kSecondsPerDay = 86400;
    const std::int64_t     local = *epochSecond + (utc.isValid() ? zone.offsetFromUtc(utc) : 0);
    // The floor of the division, for the days before 1970.
    const std::int64_t days = (local >= 0 ? local : local - (kSecondsPerDay - 1)) / kSecondsPerDay;
    return QtStrings::fromUtf8(localDateText(civilFromDays(days)));
}

QString MotorInformationPanel::formatSource(std::string_view source)
{
    const std::string_view text = Strings::trim(source);
    if (text.empty())
    {
        return {};
    }
    if (Strings::javaEqualsIgnoreCase(text, "thrustcurve.org"))
    {
        // A literal of OpenRocket's (MotorInformationPanel.java), not translated there.
        return QStringLiteral("ThrustCurve.org");
    }
    return QtStrings::fromUtf8(text);
}

void MotorInformationPanel::setComment(std::string_view text)
{
    const std::string_view trimmed = Strings::trim(text);
    m_noComment                    = trimmed.empty();
    QFont font                     = m_comment->font();
    font.setItalic(m_noComment);
    m_comment->setFont(font);
    if (m_noComment)
    {
        //// No description available.
        //: TCMotorSelPan.noDescription
        m_comment->setPlainText(tr("No description available."));
    }
    else
    {
        m_comment->setPlainText(QtStrings::fromUtf8(trimmed));
    }
    updateColors();
    // setCaretPosition(0): the start, scrolled to the top.
    m_comment->moveCursor(QTextCursor::Start);
    m_comment->ensureCursorVisible();
}

void MotorInformationPanel::changeEvent(QEvent* event)
{
    QWidget::changeEvent(event);
    if (event->type() == QEvent::PaletteChange)
    {
        updateColors();
    }
}

void MotorInformationPanel::updateColors()
{
    const QColor dim  = ThemeColors::color(ThemeColors::Key::TEXT_DIM, palette());
    const QColor text = ThemeColors::color(ThemeColors::Key::TEXT, palette());
    {
        QPalette colors = m_classificationLabel->palette();
        colors.setColor(QPalette::WindowText, dim);
        m_classificationLabel->setPalette(colors);
    }
    QPalette colors = m_comment->palette();
    colors.setColor(QPalette::Text, m_noComment ? dim : text);
    m_comment->setPalette(colors);
}

void MotorInformationPanel::plotRequested()
{
    if (m_selectedMotor == nullptr)
    {
        return;
    }
    const auto found = std::ranges::find(m_curves, m_selectedMotor);
    const int  index =
        found == m_curves.end() ? -1 : static_cast<int>(std::distance(m_curves.begin(), found));
    // Open plot dialog
    static_cast<void>(ThrustCurveMotorPlotDialog::showPlot(m_curves, index, m_services, *this));
}

}  // namespace QtRocket::Gui
