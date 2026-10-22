#pragma once

// Helpers of the tests of the panels of the body components (BodyTubeConfig, NoseConeConfig,
// TransitionConfig) and of the other panels of a kind: a panel the factory makes, and the number
// rows a panel shows.

#include <initializer_list>
#include <memory>
#include <span>
#include <string>

#include <QObject>
#include <QPointer>
#include <QWidget>

#include "ConfigDialogTestSupport.h"
#include "QtRocket/gui/binding/ComponentSelection.h"
#include "QtRocket/gui/configdialog/ConfigPanelFactory.h"
#include "QtRocket/gui/configdialog/RocketComponentConfig.h"
#include "QtRocket/util/BugError.h"

namespace QtRocket
{
class OpenRocketDocument;
class RocketComponent;
}  // namespace QtRocket

namespace QtRocket::Gui::Test
{

/// The panel ConfigPanelFactory::create() makes for @p main (and @p others) as a P, in a widget of
/// its own; released when it goes (Java: invalidateMe()). get() is null when the factory made
/// another kind of panel, which -> and * report as a BugError.
template <class P = RocketComponentConfig>
class FactoryPanel
{
public:
    FactoryPanel(OpenRocketDocument& document, const ConfigDialogServices& services,
                 RocketComponent& main, std::span<RocketComponent* const> others = {})
      : m_host(std::make_unique<QWidget>()),
        m_config(ConfigPanelFactory::create(document, services.services(),
                                            std::make_shared<ComponentSelection>(main, others),
                                            m_host.get())),
        m_panel(qobject_cast<P*>(m_config.data()))
    {
    }
    ~FactoryPanel()
    {
        if (RocketComponentConfig* config = m_config.data(); config != nullptr)
        {
            config->release();
        }
    }
    FactoryPanel(const FactoryPanel&)            = delete;
    FactoryPanel& operator=(const FactoryPanel&) = delete;
    FactoryPanel(FactoryPanel&&)                 = delete;
    FactoryPanel& operator=(FactoryPanel&&)      = delete;

    [[nodiscard]] P* get() const noexcept { return m_panel; }

    /// The panel.
    /// @throws BugError when the factory made another kind of panel
    P* operator->() const { return &panel(); }
    P& operator*() const { return panel(); }

private:
    [[nodiscard]] P& panel() const
    {
        if (m_panel == nullptr)
        {
            bug("The factory made another kind of panel");
        }
        return *m_panel;
    }

    std::unique_ptr<QWidget>        m_host;
    QPointer<RocketComponentConfig> m_config;
    P*                              m_panel;
};

/// The properties of @p properties whose number row under @p root lacks its spin box, its unit
/// selector or its slider (each named after the property), one per line; empty when none does.
[[nodiscard]] std::string missingNumberRows(const QWidget&                     root,
                                            std::initializer_list<const char*> properties);

}  // namespace QtRocket::Gui::Test
