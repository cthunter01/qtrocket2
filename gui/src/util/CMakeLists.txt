# GUI area 1, util: helpers every other area may use (ownership, strings, icons, services and the like).
# Each source with its header (${gui_include_dir}/util/X.h): AUTOMOC reads only the headers listed here.
target_sources(QtRocket_gui PRIVATE
    BuildInfo.cpp ${gui_include_dir}/util/BuildInfo.h
    ${gui_include_dir}/util/MakeChild.h
)
