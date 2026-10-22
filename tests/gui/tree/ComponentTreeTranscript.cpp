#include "tree/ComponentTreeTranscript.h"

#include <string>
#include <string_view>

#include <QItemSelectionModel>
#include <QModelIndex>

#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/gui/tree/ComponentTreeModel.h"
#include "QtRocket/gui/tree/ComponentTreeView.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"

namespace QtRocket::Gui::Test
{

std::string treeTranscript(std::string_view label, const ComponentTreeView& view)
{
    const ComponentTreeModel& model = view.componentModel();
    std::string               text{label};
    text += ": rows=" + std::to_string(view.visibleRows().size()) + " {";
    for (const RocketComponent& component : model.document().getRocket().subtree())
    {
        const QModelIndex index = model.indexOf(component);
        text += component.getName();
        if (!view.isRowVisible(index))
        {
            text += "(hidden)";
        }
        else
        {
            text += view.isExpanded(index) ? "+" : "-";
        }
        text += ", ";
    }
    text += "} selected=[";
    bool first = true;
    for (const RocketComponent* component :
         model.components(view.selectionModel()->selectedIndexes()))
    {
        text += first ? "" : ", ";
        text += component->getName();
        first = false;
    }
    return text + "]";
}

}  // namespace QtRocket::Gui::Test
