#include "QtRocket/gui/widgets/StageSelector.h"

#include <algorithm>
#include <cstddef>
#include <vector>

#include <QFont>
#include <QHBoxLayout>
#include <QIcon>
#include <QSizePolicy>
#include <QString>
#include <QToolButton>
#include <QWidget>
#include <Qt>

#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/gui/util/Icons.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/ComponentAssembly.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Uuid.h"

namespace QtRocket::Gui
{

namespace
{

/// What a button shows for one stage.
struct StageButtonState
{
    QString text;
    bool    active{false};
    bool    enabled{false};
    QString toolTip;
};

/// The stages of @p rocket, in tree order (getAllChildAssemblies(), the AxialStages: booster sets
/// are AxialStages too).
[[nodiscard]] std::vector<const AxialStage*> stagesOf(const Rocket& rocket)
{
    std::vector<const AxialStage*> stages;
    for (const ComponentAssembly* assembly : rocket.getAllChildAssemblies())
    {
        if (const auto* stage = dynamic_cast<const AxialStage*>(assembly))
        {
            stages.push_back(stage);
        }
    }
    return stages;
}

/// A button's text: the name as it is (a QToolButton takes '&' for a mnemonic).
[[nodiscard]] QString buttonText(const AxialStage& stage)
{
    QString text = QtStrings::fromUtf8(stage.getName());
    text.replace(QLatin1Char('&'), QStringLiteral("&&"));
    return text;
}

}  // namespace

StageSelector::StageSelector(Rocket& rocket, QWidget* parent)
  : QWidget(parent),
    m_rocket(&rocket),
    m_layout(makeChild<QHBoxLayout>(this)),
    m_inactiveIcon(Icons::icon(Icons::Id::COMPONENT_DISABLED))
{
    // MigLayout "gap rel!, insets 0": no margins, the style's spacing between related items.
    m_layout->setContentsMargins(0, 0, 0, 0);
    // The buttons stay packed at the left whatever width the selector gets (Java's ribbon cell
    // "pushx" leaves the selector at its preferred size): a stretch after them, and no growing
    // in a layout, which the stretch would otherwise ask for.
    m_layout->addStretch(1);
    setSizePolicy(QSizePolicy::Maximum, sizePolicy().verticalPolicy());
    m_rocketConnection = rocket.addComponentChangeListener(
        guarded([this] { updateButtons(); }, QStringLiteral("Stage selector")));
    updateButtons();
}

QToolButton* StageSelector::button(std::size_t index) const
{
    QTROCKET_ASSERT(index < m_buttons.size());
    return m_buttons[index];
}

const Uuid& StageSelector::stageId(std::size_t index) const
{
    QTROCKET_ASSERT(index < m_stageIds.size());
    return m_stageIds[index];
}

void StageSelector::updateButtons()
{
    const FlightConfiguration&           configuration = m_rocket->getSelectedConfiguration();
    const std::vector<const AxialStage*> stages        = stagesOf(*m_rocket);

    // First, count the stages that are active and can be toggled (isStageActive() already asks
    // for children; Java asks again).
    const auto activeStageCount =
        std::ranges::count_if(stages, [&configuration](const AxialStage* stage) {
            return configuration.isStageActive(stage->getStageNumber()) &&
                   stage->getChildCount() > 0;
        });

    resizeButtons(stages.size());
    for (std::size_t i = 0; i < stages.size(); ++i)
    {
        const AxialStage& stage = *stages[i];
        StageButtonState  state;
        state.text    = buttonText(stage);
        state.active  = configuration.isStageActive(stage.getStageNumber());
        state.enabled = stage.getChildCount() > 0;
        if (!state.enabled)
        {
            //: RocketPanel.btn.Stages.NoChildren.ttip
            state.toolTip =
                tr("<html>This stage does not have child components and is therefore "
                   "marked as inactive. <br>Add components to the stage to activate "
                   "it.</html>");
        }
        else if (state.active && activeStageCount <= 1)
        {
            // The only active stage cannot be deactivated.
            state.enabled = false;
            //: RocketPanel.btn.Stages.LastActive.ttip
            state.toolTip = tr("Cannot deactivate the only active stage.");
        }
        else
        {
            //: RocketPanel.btn.Stages.Toggle.ttip
            state.toolTip = tr("Toggle this button to activate/deactivate the stage.");
        }

        QToolButton* button = m_buttons[i];
        button->setText(state.text);
        button->setChecked(state.active);
        button->setEnabled(state.enabled);
        button->setToolTip(state.toolTip);
        // A disabled button in italics without an icon; an enabled one upright, with the icon of
        // an inactive component while its stage is inactive.
        QFont font;
        font.setItalic(!state.enabled);
        button->setFont(font);
        button->setIcon(state.enabled && !state.active ? m_inactiveIcon : QIcon());
        m_stageIds[i] = stage.getId();
    }
    equalizeHeights();
}

void StageSelector::toggleStage(Rocket& rocket, int stageNumber)
{
    FlightConfiguration& configuration = rocket.getSelectedConfiguration();
    configuration.toggleStage(stageNumber);
    rocket.fireComponentChangeEvent(kToggleChange, configuration.getFlightConfigurationId());
}

void StageSelector::buttonClicked(std::size_t index)
{
    if (index >= m_stageIds.size())
    {
        return;
    }
    const AxialStage* stage = m_rocket->getStage(m_stageIds[index]);
    if (stage == nullptr)
    {
        // The stage is gone (an event the selector has not seen yet): show the stages as they
        // are now.
        updateButtons();
        return;
    }
    toggleStage(*m_rocket, stage->getStageNumber());
}

void StageSelector::resizeButtons(std::size_t count)
{
    while (m_buttons.size() > count)
    {
        // Not deleted at once: its click may be the one being delivered.
        QToolButton* surplus = m_buttons.back();
        m_buttons.pop_back();
        m_stageIds.pop_back();
        m_layout->removeWidget(surplus);
        surplus->hide();
        surplus->deleteLater();
    }
    while (m_buttons.size() < count)
    {
        const std::size_t index  = m_buttons.size();
        auto*             button = makeChild<QToolButton>(this);
        button->setCheckable(true);
        button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        connect(button, &QToolButton::clicked, this,
                guarded([this, index] { buttonClicked(index); }, QStringLiteral("Stage selector")));
        // Before the stretch.
        m_layout->insertWidget(static_cast<int>(index), button);
        m_buttons.push_back(button);
        m_stageIds.emplace_back();
    }
}

void StageSelector::equalizeHeights() const
{
    int height = 0;
    for (const QToolButton* button : m_buttons)
    {
        height = std::max(height, button->sizeHint().height());
    }
    for (QToolButton* button : m_buttons)
    {
        button->setMinimumHeight(height);
    }
}

}  // namespace QtRocket::Gui
