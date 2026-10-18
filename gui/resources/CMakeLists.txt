# The resources of QtRocket_gui (icons, ...), compiled into the library: only through this qt_add_resources() call,
# which builds an object library that every program linking QtRocket_gui gets, so no Q_INIT_RESOURCE() is needed.
# Never a .qrc file with AUTORCC: in a static library it builds, but its files are missing at run time (the linker
# drops its static initialiser). Resource paths are case-sensitive: file names in lower case. List each file, in
# alphabetical order; the origin and licence of each set go into a README.md beside them and into NOTICE.
qt_add_resources(QtRocket_gui "gui_resources"
    PREFIX "/"
    FILES
)
