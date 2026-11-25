#include "QtRocket/gui/dialogs/simulation/RandomSeedControls.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include <QChar>
#include <QCheckBox>
#include <QEvent>
#include <QFocusEvent>
// FieldValidator.h's inline messagePopup() needs the class.
#include <QLabel>  // IWYU pragma: keep
#include <QLineEdit>
#include <QObject>
#include <QString>
#include <QWidget>
#include <Qt>
#include <QtTypes>

#include "QtRocket/gui/binding/BindingContext.h"
#include "QtRocket/gui/binding/ObjectSelection.h"
#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/widgets/FieldValidator.h"
#include "QtRocket/simulation/SimulationOptions.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket::Gui
{

namespace
{

/// The context of the controls' reports (Guard).
QString context()
{
    return QStringLiteral("Random seed");
}

/// Java's JTextField(12): the field's width in columns.
constexpr int kFieldColumns = 12;

/// Whether a focus loss for @p reason is a temporary one (Java: FocusEvent.isTemporary()): the
/// window lost its activation, or a popup or the menu bar took the focus for a while.
[[nodiscard]] bool isTemporary(Qt::FocusReason reason) noexcept
{
    return reason == Qt::ActiveWindowFocusReason || reason == Qt::PopupFocusReason ||
           reason == Qt::MenuBarFocusReason;
}

}  // namespace

RandomSeedControls::RandomSeedControls(std::shared_ptr<ObjectSelection> options, QWidget* parent)
  : QObject(parent), m_options(std::move(options))
{
    if (parent == nullptr || m_options == nullptr || !m_options->holds<SimulationOptions>())
    {
        bug("The random seed controls need a parent and a selection of a simulation's "
            "SimulationOptions");
    }

    //: simedtdlg.checkbox.FixedRandomSeed
    m_checkBox = makeChild<QCheckBox>(parent, tr("Use fixed random seed:"));
    m_checkBox->setObjectName(QStringLiteral("fixedRandomSeedCheckBox"));
    //: simedtdlg.checkbox.ttip.FixedRandomSeed
    m_checkBox->setToolTip(tr("Reuse the same random seed for reproducible simulation runs."));

    m_field = makeChild<QLineEdit>(parent);
    m_field->setObjectName(QStringLiteral("randomSeedField"));
    //: simedtdlg.lbl.ttip.RandomSeed
    m_field->setToolTip(tr("A signed 32-bit integer used to initialize simulation randomness."));
    // Java: JTextField(12), placed "w 130lp!".
    m_field->setMinimumWidth(std::max(
        kFieldWidth, m_field->fontMetrics().horizontalAdvance(QLatin1Char('0')) * kFieldColumns));
    showOptions();

    m_validator = &FieldValidator::of(*m_field)
                       .errorIf(
                           [this] {
                               return m_checkBox->isChecked() &&
                                      !parseRandomSeed(m_field->text()).has_value();
                           },
                           //: simedtdlg.error.RandomSeed
                           tr("Enter a signed 32-bit integer."))
                       .showMessagePopup(FieldValidator::kMessagePopupDuration);
    m_validator->update();

    connect(m_checkBox, &QCheckBox::clicked, this,
            guarded([this](bool checked) { checkBoxClicked(checked); }, context()));
    connect(m_field, &QLineEdit::textChanged, this,
            guarded(
                [this] {
                    seedFromField();
                    m_validator->update();
                },
                context()));
    m_field->installEventFilter(this);
    m_optionsConnection =
        m_options->addListener([this](BindingContext::Phase phase) { optionsChanged(phase); });
}

RandomSeedControls::~RandomSeedControls() = default;

void RandomSeedControls::checkBoxClicked(bool checked)
{
    if (m_updating)
    {
        return;
    }
    SimulationOptions* const options = liveOptions();
    if (options == nullptr)
    {
        return;
    }
    m_updating = true;
    if (checked)
    {
        // Start each newly enabled fixed-seed session with a usable value that the user can
        // override.
        options->randomizeSeed();
        m_field->setText(QString::number(options->getRandomSeed()));
        m_field->setEnabled(true);
        options->setRandomSeedFixed(true);
    }
    else
    {
        options->setRandomSeedFixed(false);
        m_field->setText(QString());
        m_field->setEnabled(false);
    }
    m_updating = false;
    m_validator->update();
}

void RandomSeedControls::seedFromField()
{
    if (m_updating || !m_checkBox->isChecked())
    {
        return;
    }
    SimulationOptions* const options = liveOptions();
    if (options == nullptr)
    {
        return;
    }
    if (const std::optional<int> seed = parseRandomSeed(m_field->text()); seed.has_value())
    {
        m_updating = true;
        options->setRandomSeed(*seed);
        m_updating = false;
    }
}

void RandomSeedControls::replaceInvalidRandomSeed()
{
    if (!m_checkBox->isChecked() || parseRandomSeed(m_field->text()).has_value())
    {
        return;
    }
    SimulationOptions* const options = liveOptions();
    if (options == nullptr)
    {
        return;
    }
    m_updating = true;
    options->randomizeSeed();
    m_field->setText(QString::number(options->getRandomSeed()));
    m_updating = false;
    m_validator->update();
}

void RandomSeedControls::prepareForSimulation()
{
    SimulationOptions* const options = liveOptions();
    if (options == nullptr)
    {
        return;
    }
    m_updating = true;
    const bool useFixedSeed =
        applyRandomSeedInput(*options, m_checkBox->isChecked(), m_field->text());
    m_checkBox->setChecked(useFixedSeed);
    m_field->setEnabled(useFixedSeed);
    if (!useFixedSeed)
    {
        m_field->setText(QString());
    }
    m_updating = false;
    m_validator->update();
}

void RandomSeedControls::showOptions()
{
    const SimulationOptions* const options = liveOptions();
    if (options == nullptr)
    {
        return;
    }
    m_updating       = true;
    const bool fixed = options->isRandomSeedFixed();
    m_checkBox->setChecked(fixed);
    m_field->setEnabled(fixed);
    m_field->setText(fixed ? QString::number(options->getRandomSeed()) : QString());
    m_updating = false;
}

void RandomSeedControls::optionsChanged(BindingContext::Phase phase)
{
    if (phase != BindingContext::Phase::NOTIFY || m_updating)
    {
        return;
    }
    const SimulationOptions* const options = liveOptions();
    if (options == nullptr)
    {
        return;
    }
    // Shown again only when the options differ from what the controls show: a text that is no
    // seed is the user's, which its error outline marks.
    const bool               fixed = options->isRandomSeedFixed();
    const std::optional<int> shown = parseRandomSeed(m_field->text());
    if (m_checkBox->isChecked() != fixed ||
        (fixed && shown.has_value() && *shown != options->getRandomSeed()))
    {
        showOptions();
        m_validator->update();
    }
}

bool RandomSeedControls::applyRandomSeedInput(SimulationOptions& options, bool fixedSeedRequested,
                                              const QString& seedText)
{
    const std::optional<int> seed =
        fixedSeedRequested ? parseRandomSeed(seedText) : std::optional<int>();
    if (!seed.has_value())
    {
        options.setRandomSeedFixed(false);
        return false;
    }
    options.setRandomSeed(*seed);
    options.setRandomSeedFixed(true);
    return true;
}

std::optional<int> RandomSeedControls::parseRandomSeed(const QString& text)
{
    // String.trim(): the characters up to U+0020 at either end.
    qsizetype begin = 0;
    qsizetype end   = text.size();
    while (begin < end && text.at(begin).unicode() <= u' ')
    {
        ++begin;
    }
    while (end > begin && text.at(end - 1).unicode() <= u' ')
    {
        --end;
    }
    // Integer.parseInt(): an optional sign, then digits of any script (Character.digit(), which
    // takes a character of Unicode's decimal digits), in ASCII for Strings::parseInt(), which
    // checks the sign, the length and the range.
    std::string ascii;
    for (qsizetype index = begin; index < end; ++index)
    {
        const QChar character = text.at(index);
        if (index == begin && (character == u'+' || character == u'-'))
        {
            ascii.push_back(static_cast<char>(character.unicode()));
            continue;
        }
        const int digit = character.digitValue();
        if (character.category() != QChar::Number_DecimalDigit || digit < 0 || digit > 9)
        {
            return std::nullopt;
        }
        ascii.push_back(static_cast<char>('0' + digit));
    }
    return Strings::parseInt(ascii);
}

bool RandomSeedControls::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_field && event->type() == QEvent::FocusOut)
    {
        const auto* focus = dynamic_cast<const QFocusEvent*>(event);
        if (focus != nullptr && !isTemporary(focus->reason()))
        {
            Guard::run([this] { replaceInvalidRandomSeed(); }, context());
        }
    }
    return QObject::eventFilter(watched, event);
}

SimulationOptions* RandomSeedControls::liveOptions() const
{
    return m_options->isValid() ? &m_options->getObject<SimulationOptions>() : nullptr;
}

}  // namespace QtRocket::Gui
