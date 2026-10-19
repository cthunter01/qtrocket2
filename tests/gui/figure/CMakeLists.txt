# The tests of the GUI area figure (gui/src/figure): XTests.cpp for the class X, lower_snake_case_tests.cpp
# for tests about no one class, in alphabetical order (ignoring case).
target_sources(QtRocket_gui_tests PRIVATE
    CaretTests.cpp
    CGCaretTests.cpp
    CPCaretTests.cpp
    RecordingPaintDevice.cpp
    rocket_figure_paint_golden_tests.cpp
    RocketFigurePainterTests.cpp
    RocketFigureTests.cpp
    RulerTests.cpp
    ScaleFigureTests.cpp
    ScaleScrollAreaTests.cpp
    ScaleSelectorTests.cpp
    ScrollAreaZoomModelTests.cpp
    TestScaleFigure.cpp
)
