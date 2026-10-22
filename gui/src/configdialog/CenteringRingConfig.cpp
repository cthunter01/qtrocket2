#include "QtRocket/gui/configdialog/CenteringRingConfig.h"

#include <memory>
#include <utility>

#include <QString>
#include <QWidget>

#include "QtRocket/gui/configdialog/RingComponentConfig.h"

namespace QtRocket::Gui
{

CenteringRingConfig::CenteringRingConfig(OpenRocketDocument& document, const GuiServices& services,
                                         std::shared_ptr<ComponentSelection> selection,
                                         QWidget*                            parent)
  : RingComponentConfig(document, services, std::move(selection), parent)
{
    addGeneralTab(
        {//: CenteringRingCfg.tab.Thickness
         .length = tr("Thickness:"),
         //: CenteringRingCfg.tab.Outerdiam
         .outer = tr("Outer diameter:"),
         //: CenteringRingCfg.tab.Innerdiam
         .inner     = tr("Inner diameter:"),
         .thickness = QString()},
        //: CenteringRingCfg.tab.General
        tr("General"),
        //: CenteringRingCfg.tab.Generalproperties
        tr("General properties"));
}

}  // namespace QtRocket::Gui
