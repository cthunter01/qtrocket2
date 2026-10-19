# GUI area 6, figure: the widgets that draw the rocket and the fins (the geometry is the core's figure/).
# Each source with its header (${gui_include_dir}/figure/X.h): AUTOMOC reads only the headers listed here.
target_sources(QtRocket_gui PRIVATE
    Ruler.cpp ${gui_include_dir}/figure/Ruler.h
    ScaleFigure.cpp ${gui_include_dir}/figure/ScaleFigure.h
    ScaleScrollArea.cpp ${gui_include_dir}/figure/ScaleScrollArea.h
    ScaleSelector.cpp ${gui_include_dir}/figure/ScaleSelector.h
    ScrollAreaZoomModel.cpp ${gui_include_dir}/figure/ScrollAreaZoomModel.h
    ZoomModel.cpp ${gui_include_dir}/figure/ZoomModel.h
)
