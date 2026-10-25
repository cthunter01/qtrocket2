# GUI area 7, figure: the widgets that draw the rocket and the fins (the geometry is the core's figure/).
# Each source with its header (${gui_include_dir}/figure/X.h): AUTOMOC reads only the headers listed here.
target_sources(QtRocket_gui PRIVATE
    Caret.cpp ${gui_include_dir}/figure/Caret.h
    CGCaret.cpp ${gui_include_dir}/figure/CGCaret.h
    CPCaret.cpp ${gui_include_dir}/figure/CPCaret.h
    DesignPhysics.cpp ${gui_include_dir}/figure/DesignPhysics.h
    ${gui_include_dir}/figure/FigureElement.h
    FinPointEditHistory.cpp ${gui_include_dir}/figure/FinPointEditHistory.h
    FinPointFigure.cpp ${gui_include_dir}/figure/FinPointFigure.h
    FinPointScrollArea.cpp ${gui_include_dir}/figure/FinPointScrollArea.h
    FinPointSelection.cpp ${gui_include_dir}/figure/FinPointSelection.h
    RocketFigure.cpp ${gui_include_dir}/figure/RocketFigure.h
    RocketFigurePainter.cpp ${gui_include_dir}/figure/RocketFigurePainter.h
    RocketInfo.cpp ${gui_include_dir}/figure/RocketInfo.h
    RocketPanel.cpp ${gui_include_dir}/figure/RocketPanel.h
    Ruler.cpp ${gui_include_dir}/figure/Ruler.h
    RulerUnitSelector.cpp ${gui_include_dir}/figure/RulerUnitSelector.h
    ScaleFigure.cpp ${gui_include_dir}/figure/ScaleFigure.h
    ScaleScrollArea.cpp ${gui_include_dir}/figure/ScaleScrollArea.h
    ScaleSelector.cpp ${gui_include_dir}/figure/ScaleSelector.h
    ScrollAreaZoomModel.cpp ${gui_include_dir}/figure/ScrollAreaZoomModel.h
    ViewRotationControl.cpp ${gui_include_dir}/figure/ViewRotationControl.h
    ZoomModel.cpp ${gui_include_dir}/figure/ZoomModel.h
)
