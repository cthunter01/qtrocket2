# GUI area 3, binding: the value bindings between widgets and components. Qt-free: no Qt header and no header of
# another GUI area here, only the core and binding/ itself (lint.layers).
# Each source with its header (${gui_include_dir}/binding/X.h): AUTOMOC reads only the headers listed here.
target_sources(QtRocket_gui PRIVATE
    Binding.cpp ${gui_include_dir}/binding/Binding.h
    ComponentSelection.cpp ${gui_include_dir}/binding/ComponentSelection.h
    DoubleBinding.cpp ${gui_include_dir}/binding/DoubleBinding.h
    ${gui_include_dir}/binding/PropertySource.h
    SliderMapping.cpp ${gui_include_dir}/binding/SliderMapping.h
)
