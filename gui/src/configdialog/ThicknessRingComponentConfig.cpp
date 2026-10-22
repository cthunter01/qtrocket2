#include "QtRocket/gui/configdialog/ThicknessRingComponentConfig.h"

#include <memory>
#include <utility>

#include <QWidget>

#include "QtRocket/gui/configdialog/RingComponentConfig.h"

namespace QtRocket::Gui
{

ThicknessRingComponentConfig::ThicknessRingComponentConfig(
    OpenRocketDocument& document, const GuiServices& services,
    std::shared_ptr<ComponentSelection> selection, QWidget* parent)
  : RingComponentConfig(document, services, std::move(selection), parent)
{
    addGeneralTab(
        {//: ThicknessRingCompCfg.tab.Length
         .length = tr("Length:"),
         //: ThicknessRingCompCfg.tab.Outerdiam
         .outer = tr("Outer diameter:"),
         //: ThicknessRingCompCfg.tab.Innerdiam
         .inner = tr("Inner diameter:"),
         //: ThicknessRingCompCfg.tab.Wallthickness
         .thickness = tr("Wall thickness:")},
        //: ThicknessRingCompCfg.tab.General
        tr("General"),
        //: ThicknessRingCompCfg.tab.Generalprop
        tr("General properties"));
}

}  // namespace QtRocket::Gui
