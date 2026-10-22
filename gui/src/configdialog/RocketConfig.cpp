#include "QtRocket/gui/configdialog/RocketConfig.h"

#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>

#include <QBoxLayout>
#include <QEvent>
#include <QGridLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLatin1Char>
#include <QLineEdit>
#include <QObject>
#include <QPlainTextEdit>
#include <QString>
#include <QTabWidget>
#include <QTextOption>
#include <QTimer>
#include <QWidget>
#include <Qt>

#include "QtRocket/gui/binding/ComponentSelection.h"
#include "QtRocket/gui/binding/EnumBinding.h"
#include "QtRocket/gui/binding/PropertySource.h"
#include "QtRocket/gui/binding/StringBinding.h"
#include "QtRocket/gui/configdialog/RocketComponentConfig.h"
#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/gui/util/GuiServices.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/util/UserPrompts.h"
#include "QtRocket/gui/widgets/BindingScope.h"
#include "QtRocket/gui/widgets/EnumComboBox.h"
#include "QtRocket/gui/widgets/TextCommitter.h"
#include "QtRocket/rocket/DesignType.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/util/BugError.h"

namespace QtRocket::Gui
{

namespace
{

/// The selection of @p panel, which must be valid and edit the rocket.
[[nodiscard]] const std::shared_ptr<ComponentSelection>& rocketSelection(
    const RocketComponentConfig& panel)
{
    const std::shared_ptr<ComponentSelection>& selection = panel.selection();
    if (selection == nullptr || !selection->isValid())
    {
        bug("The configuration of the rocket needs a valid selection");
    }
    if (dynamic_cast<const Rocket*>(&selection->getMain()) == nullptr)
    {
        bug("The configuration of the rocket needs the rocket");
    }
    return selection;
}

}  // namespace

StringSource RocketConfig::designerSource()
{
    return StringSource::of<Rocket>({
        .get = [](const Rocket& rocket) { return rocket.getDesigner(); },
        .set = [](Rocket& rocket, const std::string& designer) { rocket.setDesigner(designer); },
    });
}

StringSource RocketConfig::kitNameSource()
{
    return StringSource::of<Rocket>({
        .get = [](const Rocket& rocket) { return rocket.getKitName(); },
        .set = [](Rocket& rocket, const std::string& kitName) { rocket.setKitName(kitName); },
    });
}

StringSource RocketConfig::revisionSource()
{
    return StringSource::of<Rocket>({
        .get = [](const Rocket& rocket) { return rocket.getRevision(); },
        .set = [](Rocket& rocket, const std::string& revision) { rocket.setRevision(revision); },
    });
}

EnumSource<DesignType> RocketConfig::designTypeSource()
{
    return EnumSource<DesignType>::of<Rocket>({
        .get = [](const Rocket& rocket) { return rocket.getDesignType(); },
        .set = [](Rocket& rocket, const DesignType& type) { rocket.setDesignType(type); },
    });
}

std::optional<UserPrompts::Message> RocketConfig::easterEggMessage(const QString& name)
{
    // Not in messages.properties: the texts of RocketConfig.addEasterEgg(), each message a
    // JOptionPane text whose lines are paragraphs.
    const auto message = [](const QString& title, const QString& text) {
        return UserPrompts::Message{.severity = UserPrompts::Severity::INFORMATION,
                                    .title    = title,
                                    .lines    = text.split(QLatin1Char('\n'))};
    };
    if (name == QStringLiteral("SA-508"))
    {
        return message(tr("Oh oh..."), tr("Houston, we have a problem.\n\nJust kidding, have "
                                          "fun building your 'Apollo 13' rocket!"));
    }
    if (name == QStringLiteral("SA-506"))
    {
        return message(tr("Or was that not the quote?"),
                       tr("One small step for a rocket, one giant leap for rocketkind."));
    }
    if (name == QStringLiteral("Vega"))
    {
        return message(tr("Vega, Ready for Launch and Laughs!"), tr("Viva las Vega!"));
    }
    if (name == QStringLiteral("Ariane 5"))
    {
        return message(
            tr("Happens to the best of us"),
            tr("Non, je ne regrette rien\u2026 except for that one overflow error\u2026"));
    }
    return std::nullopt;
}

RocketConfig::RocketConfig(OpenRocketDocument& document, const GuiServices& services,
                           std::shared_ptr<ComponentSelection> selection, QWidget* parent)
  : RocketComponentConfig(document, services, std::move(selection), parent)
{
    const std::shared_ptr<ComponentSelection>& valid    = rocketSelection(*this);
    BindingScope&                              bindings = scope();
    auto*                                      box      = qobject_cast<QBoxLayout*>(layout());
    if (box == nullptr || box->indexOf(&tabs()) < 0)
    {
        bug("The configuration of the rocket needs its base's layout of the tabs");
    }
    // The base's header, which the name field leaves.
    QWidget* header = nameField().parentWidget();

    m_rows = makeChild<QWidget>(this);
    m_rows->setObjectName(QStringLiteral("rows"));
    auto* grid = makeChild<QGridLayout>(m_rows);
    grid->setContentsMargins(0, 0, 0, 0);

    //: RocketCfg.lbl.Designname
    addRow(*grid, 0, tr("Design name:"), nameField());

    m_designer = &makeTextArea(60);
    m_designer->setObjectName(QStringLiteral("Designer"));
    //: RocketCfg.lbl.Designer
    addRow(*grid, 1, tr("Designer:"), *m_designer);

    const auto designType = bindings.make<EnumBinding<DesignType>>(
        valid, designTypeSource(), std::span<const DesignType>(kAllDesignTypes));
    m_designType = makeChild<EnumComboBox>(m_rows, designType);
    m_designType->setObjectName(QStringLiteral("DesignType"));
    //: RocketCfg.lbl.Designtype
    addRow(*grid, 2, tr("Design type:"), *m_designType);

    m_kitName = &makeTextArea(60);
    m_kitName->setObjectName(QStringLiteral("KitName"));
    //: RocketCfg.lbl.Kitname
    m_kitNameLabel = &addRow(*grid, 3, tr("Kit name:"), *m_kitName);

    // The base's comment area, with the base's committer (Java: commentTextArea).
    commentArea().setMinimumSize(400, 155);
    //: RocketCfg.lbl.Comments
    addRow(*grid, 4, tr("Comments:"), commentArea());

    m_revision = &makeTextArea(60);
    m_revision->setObjectName(QStringLiteral("Revision"));
    //: RocketCfg.lbl.Revisionhistory
    addRow(*grid, 5, tr("Revision history:"), *m_revision);
    grid->setColumnStretch(1, 1);
    // Java's "grow 30" and "grow 100".
    grid->setRowStretch(1, 30);
    grid->setRowStretch(3, 30);
    grid->setRowStretch(4, 100);
    grid->setRowStretch(5, 30);

    m_committer = makeChild<TextCommitter>(this);
    m_committer->addField(*m_designer, bindings.make<StringBinding>(valid, designerSource()));
    m_committer->addField(*m_kitName, bindings.make<StringBinding>(valid, kitNameSource()));
    m_committer->addField(*m_revision, bindings.make<StringBinding>(valid, revisionSource()));

    // Java's removeAll(): the rows in the place of the header and the tabs.
    box->insertWidget(box->indexOf(&tabs()), m_rows, 1);
    header->hide();
    tabs().hide();
    QWidget::setTabOrder(&nameField(), m_designer);
    QWidget::setTabOrder(m_designer, m_designType);
    QWidget::setTabOrder(m_designType, m_kitName);
    QWidget::setTabOrder(m_kitName, &commentArea());
    QWidget::setTabOrder(&commentArea(), m_revision);

    // The kit name follows the design type (Java: the combo box's action listener).
    bindings.keepConnection(designType->addChangeListener(
        [this] { Guard::run([this] { updateKitName(); }, QStringLiteral("DesignType")); }));
    updateKitName();

    nameField().installEventFilter(this);
}

RocketConfig::~RocketConfig() = default;

QLabel& RocketConfig::addRow(QGridLayout& grid, int row, const QString& text, QWidget& field)
{
    auto* label = makeChild<QLabel>(m_rows, text);
    // Java: "top, pad 4lp, gapright 10lp".
    grid.addWidget(label, row, 0, Qt::AlignLeft | Qt::AlignTop);
    grid.addWidget(&field, row, 1);
    label->setBuddy(&field);
    return *label;
}

QPlainTextEdit& RocketConfig::makeTextArea(int height)
{
    auto* area = makeChild<QPlainTextEdit>(m_rows);
    area->setLineWrapMode(QPlainTextEdit::WidgetWidth);
    area->setWordWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    // Java: "wmin 400lp, height 60lp:60lp:".
    area->setMinimumSize(400, height);
    return *area;
}

bool RocketConfig::isKitNameShown() const
{
    return !m_kitName->isHidden();
}

void RocketConfig::updateKitName()
{
    const std::shared_ptr<ComponentSelection>& valid = selection();
    if (valid == nullptr || !valid->isValid())
    {
        return;
    }
    const auto* rocket = dynamic_cast<const Rocket*>(&valid->getMain());
    if (rocket == nullptr)
    {
        return;
    }
    const bool original = rocket->getDesignType() == DesignType::ORIGINAL;
    if (m_kitName->isHidden() == original)
    {
        return;
    }
    m_kitNameLabel->setHidden(original);
    m_kitName->setHidden(original);
    // Java: parent.pack().
    if (QWidget* top = window(); top != this && top->isVisible())
    {
        top->adjustSize();
    }
}

bool RocketConfig::eventFilter(QObject* watched, QEvent* event)
{
    // Java's keyTyped(): the field's text and the typed character, before the field takes it.
    if (event->type() == QEvent::KeyPress && watched == &nameField())
    {
        if (const auto* key = dynamic_cast<const QKeyEvent*>(event);
            key != nullptr && !key->text().isEmpty())
        {
            if (std::optional<UserPrompts::Message> message =
                    easterEggMessage(nameField().text() + key->text()))
            {
                QTimer::singleShot(0, this, [this, shown = std::move(*message)] {
                    Guard::run([this, &shown] { services().prompts().showMessage(this, shown); },
                               QStringLiteral("Name"));
                });
            }
        }
    }
    return RocketComponentConfig::eventFilter(watched, event);
}

}  // namespace QtRocket::Gui
