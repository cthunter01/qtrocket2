# GUI area 6, figure: the widgets that draw the rocket and the fins (the geometry is the core's figure/).
# Each source with its header (${gui_include_dir}/figure/X.h): AUTOMOC reads only the headers listed here.
target_sources(QtRocket_gui PRIVATE
    Caret.cpp ${gui_include_dir}/figure/Caret.h
    CGCaret.cpp ${gui_include_dir}/figure/CGCaret.h
    CPCaret.cpp ${gui_include_dir}/figure/CPCaret.h
    ${gui_include_dir}/figure/FigureElement.h
    RocketFigure.cpp ${gui_include_dir}/figure/RocketFigure.h
    RocketFigurePainter.cpp ${gui_include_dir}/figure/RocketFigurePainter.h
    Ruler.cpp ${gui_include_dir}/figure/Ruler.h
    ScaleFigure.cpp ${gui_include_dir}/figure/ScaleFigure.h
    ScaleScrollArea.cpp ${gui_include_dir}/figure/ScaleScrollArea.h
    ScaleSelector.cpp ${gui_include_dir}/figure/ScaleSelector.h
    ScrollAreaZoomModel.cpp ${gui_include_dir}/figure/ScrollAreaZoomModel.h
    ZoomModel.cpp ${gui_include_dir}/figure/ZoomModel.h
)
