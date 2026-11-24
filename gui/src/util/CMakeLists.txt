# GUI area 1, util: helpers every other area may use (ownership, strings, icons, services and the like).
# Each source with its header (${gui_include_dir}/util/X.h): AUTOMOC reads only the headers listed here.
target_sources(QtRocket_gui PRIVATE
    BuildInfo.cpp ${gui_include_dir}/util/BuildInfo.h
    ColorConversion.cpp ${gui_include_dir}/util/ColorConversion.h
    ComponentIcons.cpp ${gui_include_dir}/util/ComponentIcons.h
    ${gui_include_dir}/util/CsvOptionValues.h
    CustomFinImporter.cpp ${gui_include_dir}/util/CustomFinImporter.h
    EnumText.cpp ${gui_include_dir}/util/EnumText.h
    Guard.cpp ${gui_include_dir}/util/Guard.h
    ${gui_include_dir}/util/GuiServices.h
    Icons.cpp ${gui_include_dir}/util/Icons.h
    ${gui_include_dir}/util/MakeChild.h
    MemoryPlotDefaults.cpp ${gui_include_dir}/util/MemoryPlotDefaults.h
    MenuText.cpp ${gui_include_dir}/util/MenuText.h
    ${gui_include_dir}/util/PlotDefaults.h
    QtStrings.cpp ${gui_include_dir}/util/QtStrings.h
    ${gui_include_dir}/util/SimulationDialogs.h
    ${gui_include_dir}/util/SimulationExecutor.h
    SimulationIcons.cpp ${gui_include_dir}/util/SimulationIcons.h
    SimulationStatusColors.cpp ${gui_include_dir}/util/SimulationStatusColors.h
    SvgIconEngine.cpp ${gui_include_dir}/util/SvgIconEngine.h
    TableSortKeys.cpp ${gui_include_dir}/util/TableSortKeys.h
    ThemeColors.cpp ${gui_include_dir}/util/ThemeColors.h
    ${gui_include_dir}/util/UserPrompts.h
)
