# The lint tests (label lint): cmake -P scripts that check rules of the source tree the compiler cannot, with no build.
# lint.layers (cmake/CheckLayers.cmake): the include order of the core's subsystems and of the GUI's areas, the
#     Qt-free core and binding/, rocketplot's headers only in plot/, and the rules of gui/ (makeChild() as the only
#     `new`, no Q_SLOTS, no <windows.h>, no .ui or .qrc file).
# lint.no-qttest-macros (cmake/CheckNoQtTestMacros.cmake): none of QtTest's assertion macros in tests/gui.
# The GUI's smoke test (gui.version, tests/gui/CMakeLists.txt) carries the label too.
foreach(lint IN ITEMS layers no-qttest-macros)
    if(lint STREQUAL "layers")
        set(script ${PROJECT_SOURCE_DIR}/cmake/CheckLayers.cmake)
    else()
        set(script ${PROJECT_SOURCE_DIR}/cmake/CheckNoQtTestMacros.cmake)
    endif()
    add_test(NAME lint.${lint} COMMAND ${CMAKE_COMMAND} -DSOURCE_DIR=${PROJECT_SOURCE_DIR} -P ${script})
    set_tests_properties(lint.${lint} PROPERTIES LABELS lint)
endforeach()

# The lints run on planted trees too, written here into the build tree: a tree that keeps every rule (lint.*-pass-
# clean-tree), and one violation per tree, which the lint must name (a lint that could never fail would pass any
# tree). Plant contents hold no ';', '[' or ']'.
set(plants ${CMAKE_CURRENT_BINARY_DIR}/plants)
file(REMOVE_RECURSE ${plants})

# lint_plant(<tree> <file> <content>): writes the file into the planted tree.
function(lint_plant tree path content)
    file(WRITE ${plants}/${tree}/${path} "${content}\n")
endfunction()

# lint_plant_test(<tree> <layers|no-qttest-macros> [<regular expression>]): runs the lint on the planted tree. With
# a regular expression, the lint's output must match it (the violation it reports) and then the lint must fail: its
# error (a CMake Error, which ends cmake -P with a failure; ctest ignores the exit code of a test that has a
# PASS_REGULAR_EXPRESSION, so the pattern asks for the error). Without, the lint must pass.
function(lint_plant_test tree lint)
    if(lint STREQUAL "layers")
        set(script ${PROJECT_SOURCE_DIR}/cmake/CheckLayers.cmake)
        set(failure "lint\\.layers: the layers are broken")
    else()
        set(script ${PROJECT_SOURCE_DIR}/cmake/CheckNoQtTestMacros.cmake)
        set(failure "lint\\.no-qttest-macros: QtTest's assertion macros")
    endif()
    if(ARGC GREATER 2)
        set(name lint.${lint}-catches-${tree})
    else()
        set(name lint.${lint}-passes-${tree})
    endif()
    add_test(NAME ${name} COMMAND ${CMAKE_COMMAND} -DSOURCE_DIR=${plants}/${tree} -P ${script})
    set_tests_properties(${name} PROPERTIES LABELS lint)
    if(ARGC GREATER 2)
        set_tests_properties(${name} PROPERTIES PASS_REGULAR_EXPRESSION "${ARGV2}.*CMake Error.*${failure}")
    endif()
endfunction()

# Every rule kept, with figure/ in the core (above file/), rocketplot in plot/, the areas plot/ and flightconfig/ in
# their places, and words that only look like violations.
lint_plant(clean-tree src/figure/Shapes.cpp "#include \"QtRocket/file/GeneralRocketLoader.h\"")
lint_plant(clean-tree src/cli/Cli.cpp "#include \"QtRocket/figure/Shapes.h\"\n#include <vector>")
lint_plant(clean-tree src/file/ZipArchive.cpp "#include \"MinizipHandle.h\"\n#include <minizip/mz.h>")
lint_plant(clean-tree src/file/MinizipHandle.h "#include <minizip/mz.h>")
lint_plant(clean-tree gui/src/app/MacSetup.mm "#import <AppKit/AppKit.h>\n#include \"QtRocket/gui/main/MainWindow.h\"")
lint_plant(clean-tree include/QtRocket/util/Text.h "#include <string>\n// QWidget, Q_SLOTS and new are words")
lint_plant(clean-tree gui/include/QtRocket/gui/util/MakeChild.h "#include <QObject>\nreturn new T(parent)")
lint_plant(clean-tree gui/include/QtRocket/gui/binding/DoubleBinding.h
    "#include \"QtRocket/gui/binding/ComponentSelection.h\"\n#include \"QtRocket/rocket/Rocket.h\"")
lint_plant(clean-tree gui/src/widgets/UnitSpinBox.cpp
    "#include <QWidget>\n#include \"QtRocket/gui/binding/DoubleBinding.h\"\n// a new value, Q_SLOTS\n/* new\nnew */\nauto text = QStringLiteral(\"new \")\nm_new = renew(newValue)\nX(const X&) = delete")
lint_plant(clean-tree gui/src/app/Application.cpp
    "#include \"QtRocket/gui/main/MainWindow.h\"\n#include \"QtRocket/figure/Shapes.h\"")
lint_plant(clean-tree gui/main.cpp "#include \"QtRocket/gui/app/Application.h\"")
lint_plant(clean-tree tests/gui/util/MakeChildTests.cpp "QTest::keyClicks(widget, text)\nEXPECT_EQ(QTestable, 1)")
lint_plant(clean-tree gui/include/QtRocket/gui/plot/ThrustCurvePlot.h
    "#include <rocketplot/PlotWidget.h>\n#include \"QtRocket/gui/widgets/UnitSpinBox.h\"")
lint_plant(clean-tree gui/src/plot/ThrustCurvePlot.cpp
    "#include <rocketplot/text.h>\n#include \"QtRocket/gui/plot/ThrustCurvePlot.h\"")
lint_plant(clean-tree gui/src/tree/ComponentTreeModel.cpp "#include \"QtRocket/gui/plot/ThrustCurvePlot.h\"")
lint_plant(clean-tree gui/src/flightconfig/MotorConfigurationPanel.cpp
    "#include \"QtRocket/gui/dialogs/IgnitionSelectionDialog.h\"\n#include \"QtRocket/gui/plot/ThrustCurvePlot.h\"")
lint_plant(clean-tree gui/src/main/MainWindow.cpp "#include \"QtRocket/gui/flightconfig/FlightConfigurationPanel.h\"")
lint_plant_test(clean-tree layers)
lint_plant_test(clean-tree no-qttest-macros)

# One violation per tree.
lint_plant(core-order src/rocket/BodyTube.cpp "#include \"QtRocket/aero/BarrowmanCalculator.h\"")
lint_plant_test(core-order layers "src/rocket/BodyTube\\.cpp:1: rocket includes aero")
lint_plant(file-before-figure include/QtRocket/file/Saver.h "\n#include \"QtRocket/figure/Shapes.h\"")
lint_plant_test(file-before-figure layers "include/QtRocket/file/Saver\\.h:2: file includes figure")
lint_plant(core-before-cli src/document/Document.cpp "#include \"QtRocket/cli/Cli.h\"")
lint_plant_test(core-before-cli layers "src/document/Document\\.cpp:1: document includes cli")
lint_plant(unknown-subsystem include/QtRocket/widgets/Spinner.h "#include <string>")
lint_plant_test(unknown-subsystem layers "'widgets' is not a subsystem of the core's layer order")
lint_plant(qt-in-core include/QtRocket/util/Text.h "#include <string>\n#include <QString>")
lint_plant_test(qt-in-core layers "include/QtRocket/util/Text\\.h:2: the core includes the Qt header <QString>")
lint_plant(gui-in-core src/rocket/Rocket.cpp "#include \"QtRocket/gui/util/MakeChild.h\"")
lint_plant_test(gui-in-core layers "src/rocket/Rocket\\.cpp:1: the core includes the GUI header")
lint_plant(gui-order gui/src/tree/ComponentTreeModel.cpp "#include \"QtRocket/gui/main/MainWindow.h\"")
lint_plant_test(gui-order layers "gui/src/tree/ComponentTreeModel\\.cpp:1: tree includes main")
lint_plant(unknown-area gui/src/plots/Plot.cpp "#include <QWidget>")
lint_plant_test(unknown-area layers "'plots' is not an area of the GUI's layer order")
lint_plant(widgets-before-plot gui/src/widgets/Panel.cpp "#include \"QtRocket/gui/plot/PlotColors.h\"")
lint_plant_test(widgets-before-plot layers "gui/src/widgets/Panel\\.cpp:1: widgets includes plot")
lint_plant(plot-before-tree gui/include/QtRocket/gui/plot/Plot.h "#include \"QtRocket/gui/tree/ComponentTreeModel.h\"")
lint_plant_test(plot-before-tree layers "gui/include/QtRocket/gui/plot/Plot\\.h:1: plot includes tree")
lint_plant(dialogs-before-flightconfig gui/src/dialogs/IgnitionSelectionDialog.cpp
    "#include \"QtRocket/gui/flightconfig/FlightConfigurationPanel.h\"")
lint_plant_test(dialogs-before-flightconfig layers
    "gui/src/dialogs/IgnitionSelectionDialog\\.cpp:1: dialogs includes flightconfig")
lint_plant(flightconfig-before-main gui/src/flightconfig/FlightConfigurationPanel.cpp
    "#include \"QtRocket/gui/main/MainWindow.h\"")
lint_plant_test(flightconfig-before-main layers
    "gui/src/flightconfig/FlightConfigurationPanel\\.cpp:1: flightconfig includes main")
lint_plant(rocketplot-outside-plot gui/src/dialogs/ThrustCurveMotorPlotDialog.cpp
    "#include <QDialog>\n#include <rocketplot/PlotWidget.h>")
lint_plant_test(rocketplot-outside-plot layers
    "gui/src/dialogs/ThrustCurveMotorPlotDialog\\.cpp:2: <rocketplot/PlotWidget\\.h>: rocketplot's headers are included only in plot/")
lint_plant(rocketplot-in-a-header-outside-plot gui/include/QtRocket/gui/widgets/Chart.h "#include <RocketPlot/Axis.h>")
lint_plant_test(rocketplot-in-a-header-outside-plot layers
    "gui/include/QtRocket/gui/widgets/Chart\\.h:1: <RocketPlot/Axis\\.h>: rocketplot's headers")
lint_plant(rocketplot-in-the-application gui/main.cpp "#include <rocketplot/text.h>")
lint_plant_test(rocketplot-in-the-application layers "gui/main\\.cpp:1: <rocketplot/text\\.h>: rocketplot's headers")
lint_plant(rocketplot-in-core src/motor/ThrustCurve.cpp "#include <rocketplot/text.h>")
lint_plant_test(rocketplot-in-core layers
    "src/motor/ThrustCurve\\.cpp:1: the core includes rocketplot's header <rocketplot/text\\.h>")
lint_plant(quoted-rocketplot gui/src/plot/ThrustCurvePlot.cpp "#include \"rocketplot/PlotWidget.h\"")
lint_plant_test(quoted-rocketplot layers
    "gui/src/plot/ThrustCurvePlot\\.cpp:1: \"rocketplot/PlotWidget\\.h\": a quoted include is")
lint_plant(new-outside-make-child gui/src/widgets/Panel.cpp "#include <QLabel>\n\nauto* label = new QLabel(this)")
lint_plant_test(new-outside-make-child layers "gui/src/widgets/Panel\\.cpp:3: `new`")
lint_plant(slots gui/include/QtRocket/gui/widgets/Panel.h "class Panel\npublic Q_SLOTS:")
lint_plant_test(slots layers "gui/include/QtRocket/gui/widgets/Panel\\.h:2: Q_SLOTS")
lint_plant(windows-header gui/src/app/DataPaths.cpp "#include <Windows.h>")
lint_plant_test(windows-header layers "gui/src/app/DataPaths\\.cpp:1: <Windows\\.h>")
lint_plant(ui-file gui/src/dialogs/Preferences.ui "<ui/>")
lint_plant_test(ui-file layers "gui/src/dialogs/Preferences\\.ui:1: a \\.ui file")
lint_plant(ui-file-in-upper-case gui/src/dialogs/Preferences.UI "<ui/>")
lint_plant_test(ui-file-in-upper-case layers "gui/src/dialogs/Preferences\\.UI:1: a \\.ui file")
lint_plant(qrc-file gui/resources/icons.qrc "<RCC/>")
lint_plant_test(qrc-file layers "gui/resources/icons\\.qrc:1: a \\.qrc file")
lint_plant(qt-in-binding gui/src/binding/DoubleBinding.cpp "#include <QObject>")
lint_plant_test(qt-in-binding layers "gui/src/binding/DoubleBinding\\.cpp:1: binding/ includes the Qt header <QObject>")
lint_plant(gui-area-in-binding gui/include/QtRocket/gui/binding/IntBinding.h
    "#include \"QtRocket/gui/util/MakeChild.h\"")
lint_plant_test(gui-area-in-binding layers "gui/include/QtRocket/gui/binding/IntBinding\\.h:1: binding/ includes")
lint_plant(relative-include gui/src/widgets/Panel.cpp "#include <QWidget>\n#include \"../main/MainWindow.h\"")
lint_plant_test(relative-include layers "gui/src/widgets/Panel\\.cpp:2: \"\\.\\./main/MainWindow\\.h\": a relative include")
lint_plant(relative-angle-include src/rocket/Rocket.cpp "#include <../aero/Barrowman.h>")
lint_plant_test(relative-angle-include layers "src/rocket/Rocket\\.cpp:1: \"\\.\\./aero/Barrowman\\.h\": a relative include")
lint_plant(path-of-its-own src/rocket/Rocket.cpp "#include \"aero/Barrowman.h\"")
lint_plant_test(path-of-its-own layers "src/rocket/Rocket\\.cpp:1: \"aero/Barrowman\\.h\": a quoted include is")
lint_plant(no-header-beside gui/src/app/Application.cpp "#include \"MainWindow.h\"")
lint_plant_test(no-header-beside layers "gui/src/app/Application\\.cpp:1: \"MainWindow\\.h\": not a header beside the file")
lint_plant(objective-c-import src/util/Bundle.mm "#import <Foundation/Foundation.h>\n#import <QString>")
lint_plant_test(objective-c-import layers "src/util/Bundle\\.mm:2: the core includes the Qt header <QString>")
lint_plant(cli-in-gui gui/main.cpp "#include \"QtRocket/cli/DataDirectory.h\"")
lint_plant_test(cli-in-gui layers "gui/main\\.cpp:1: the GUI includes \"QtRocket/cli/DataDirectory\\.h\"")
lint_plant(qverify tests/gui/util/MakeChildTests.cpp "TEST(MakeChild, Owns)\n{\n    QVERIFY(child != nullptr)")
lint_plant_test(qverify no-qttest-macros "tests/gui/util/MakeChildTests\\.cpp:3: QVERIFY in:")
lint_plant(qtry-compare tests/gui/GuiTestSupport.h "// QTRY_COMPARE_WITH_TIMEOUT(a, b, 50)")
lint_plant_test(qtry-compare no-qttest-macros "tests/gui/GuiTestSupport\\.h:1: QTRY_COMPARE_WITH_TIMEOUT in:")
