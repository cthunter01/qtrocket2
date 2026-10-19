#pragma once

// What a component tree view shows, written down as the scouting's Java probes print OpenRocket's
// JTree (TreeProbe, ViewProbe: printExpansion()), so that a test compares the view with the
// transcript of OpenRocket line for line.

#include <string>
#include <string_view>

namespace QtRocket::Gui
{
class ComponentTreeView;
}

namespace QtRocket::Gui::Test
{

/// "<label>: rows=<visible rows> {<name><state>, ...} selected=[<name>, ...]": every component
/// of the view's rocket in tree order, its state "+" when it is visible and expanded, "-" when
/// it is visible and collapsed, "(hidden)" when an ancestor is collapsed (JTree's isExpanded(),
/// isVisible()); then the names of the selected components in selection order.
[[nodiscard]] std::string treeTranscript(std::string_view label, const ComponentTreeView& view);

}  // namespace QtRocket::Gui::Test
