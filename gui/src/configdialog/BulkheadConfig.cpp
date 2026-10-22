#include "QtRocket/gui/configdialog/BulkheadConfig.h"

#include <memory>
#include <utility>

#include <QString>
#include <QWidget>

#include "QtRocket/gui/configdialog/RingComponentConfig.h"

namespace QtRocket::Gui
{

BulkheadConfig::BulkheadConfig(OpenRocketDocument& document, const GuiServices& services,
                               std::shared_ptr<ComponentSelection> selection, QWidget* parent)
  : RingComponentConfig(document, services, std::move(selection), parent)
{
    addGeneralTab(
        {//: BulkheadCfg.tab.Thickness
         .length = tr("Thickness:"),
         //: BulkheadCfg.tab.Diameter
         .outer     = tr("Diameter:"),
         .inner     = QString(),
         .thickness = QString()},
        //: BulkheadCfg.tab.General
        tr("General"),
        //: BulkheadCfg.tab.Generalproperties
        tr("General properties"));
}

}  // namespace QtRocket::Gui
