#!/usr/bin/env bash
# Regenerates the pin tables of the GUI tests from OpenRocket's own Swing code (see README.md): runs each
# family's Java probe against OpenRocket's compiled classes and turns its transcript into the table.
#
# Usage:
#   tools/openrocket-gui-pins/generate.sh [--check] [<family>...]
#
# The families (all of them when none is named):
#   figure-paint       tests/gui/figure/RocketFigurePaintPins.h
#   rocket-info        tests/gui/figure/RocketInfoPins.h
#   ribbon             tests/gui/widgets/RibbonPins.h
#   rocket-panel       tests/gui/figure/RocketPanelPins.h (the probe runs under xvfb-run)
#   preferences        tests/gui/dialogs/PreferencesPagePins.h
#   scale-figure       the table kFigureRows of tests/gui/figure/ScaleFigureTests.cpp
#   material-binding   the six row and group tables of tests/gui/binding/MaterialBindingTests.cpp
#   material-combo     the groups and searches of tests/gui/widgets/MaterialComboBoxTests.cpp (checked only:
#                      the test assembles its tables by hand, see README.md)
#   component-editing  the ActionsProbe and ClipboardProbe tables of tests/gui/main/ComponentEditingTests.cpp
#   component-drop     the DndProbe table of tests/gui/tree/ComponentDropTests.cpp
#   actions-rules      the table kTranscripts of tests/gui/main/RocketActionsTests.cpp
#   thrust-curves      tests/gui/plot/ThrustCurvePins.h (the probe runs under xvfb-run)
#   flight-config      tests/gui/flightconfig/FlightConfigurationTablePins.h (the tables of the Motors &
#                      Configuration tab) and FlightConfigurationOperationPins.h (its operations; the probe
#                      runs under xvfb-run)
#   motor-chooser      tests/gui/dialogs/motor/MotorChooserPins.h
#   preset-chooser     tests/gui/dialogs/preset/PresetChooserPins.h (its probe runs under xvfb-run), and the
#                      values tests/gui/settings/TableUiPreferencesTests.cpp,
#                      tests/gui/binding/PresetBindingTests.cpp, tests/gui/widgets/PresetComboBoxTests.cpp,
#                      tests/gui/widgets/TableRowTraversalTests.cpp, the ComponentPreset*Tests.cpp of
#                      tests/gui/dialogs/preset/, tests/gui/configdialog/ComponentConfigDialogTests.cpp and
#                      RocketComponentConfigTests.cpp pin by hand (checked only, see README.md; two of their
#                      probes run under xvfb-run, one of them shows the parts library and presses its keys)
#   open-designs       tests/gui/exit/OpenDesignsTabPins.h (the Motors & Configuration tab of each design of
#                      OpenDesignsExitGolden) and OpenDesignsPartsLibraryPins.h (the parts library of each
#                      component of its examples that takes presets; both probes run under xvfb-run)
#
# Every table is written into the work directory first (a spliced test file into a copy of the committed
# one) and compared with the committed file byte for byte: an equal table leaves the committed file as it
# is, another one replaces it. Every family is reproducible (the figure-paint and rocket-info probes seed
# OpenRocket's UUIDs with shared/probe/DeterministicUuids.java: README.md, "Reproducibility").
# --check compares only: the exit status is 1 when a table differs. scale-figure, material-binding and
# material-combo also compare the values tests pin by hand with their hand-pinned.txt, in either mode, and
# never rewrite it (README.md, "Values pinned by hand"); so does preset-chooser.
#
# Environment:
#   OPENROCKET_DIR        the OpenRocket checkout (default: "openrocket" next to the main working tree of the
#                         QtRocket repository), at the commit of tests/data/goldens/manifest.json, which the
#                         headers record, with its preset submodule initialized at the recorded commit
#   JAVA, JAVAC           the java and javac executables (default: on PATH); they must be Java 17
#   OPENROCKET_CLASSPATH  the runtime class path of OpenRocket's swing module (the compiled classes and
#                         resources of core and swing and the jars of their dependencies), built from the
#                         checkout's HEAD (its build.properties says which commit). When it is not set, the
#                         script runs, as tools/openrocket-panel-pins/generate.sh does:
#                           1. ./gradlew :swing:classes in the checkout (incremental: the classes match the
#                              checked-out sources)
#                           2. ./gradlew --init-script <tmp> :swing:qtrocketPrintRuntimeClasspath (a task defined
#                              only in a temporary init script; OpenRocket's build files are not changed)
#   CLANG_FORMAT          the clang-format executable (default: clang-format on PATH)
#   ALLOW_DIRTY=1         accept an OpenRocket checkout with modified tracked files
#   ALLOW_OTHER_COMMIT=1  accept a checkout at another commit than the manifest's, or classes built from
#                         another commit than the checkout's HEAD (the headers record what was used)
#   GUI_PINS_WORK         keep the work files (the probes' transcripts and logs, the classes) in this directory
#                         instead of a temporary one (a temporary one is also kept when the script fails)
#
# Requires Linux (the probes' preferences store is isolated through java.util.prefs.userRoot, which only
# Linux's preferences implementation reads: elsewhere the probes would clear the user's own OpenRocket
# preferences), python3, clang-format (the generators format what they write with the repository's
# .clang-format) and, for rocket-panel, thrust-curves, flight-config, preset-chooser and open-designs,
# xvfb-run.
set -euo pipefail

all_families=(figure-paint rocket-info ribbon rocket-panel preferences scale-figure material-binding
    material-combo component-editing component-drop actions-rules thrust-curves flight-config motor-chooser
    preset-chooser open-designs)

check=false
families=()
for argument in "$@"; do
    if [[ "${argument}" == --check ]]; then
        check=true
    elif [[ " ${all_families[*]} " == *" ${argument} "* ]]; then
        families+=("${argument}")
    else
        echo "usage: $(basename "${BASH_SOURCE[0]}") [--check] [<family>...]   (families: ${all_families[*]})" >&2
        exit 2
    fi
done
if ((${#families[@]} == 0)); then
    families=("${all_families[@]}")
fi

# java.util.prefs.userRoot and systemRoot, which give every probe a fresh preferences store, are read by
# Linux's FileSystemPreferences only; macOS keeps the store in a plist and Windows in the registry, where
# the probes' clearPreferences() and storeAndApplyPreferences() would act on the user's own OpenRocket
# preferences. OpenRocket's own debug node (-Dopenrocket.debug.prefs) is no way out: every SwingPreferences
# made under it removes the node the earlier ones hold, and three probes fail.
if [[ "$(uname -s)" != Linux ]]; then
    echo "error: the probes run on Linux only: $(uname -s) ignores java.util.prefs.userRoot, and they would" \
        "clear your own OpenRocket preferences" >&2
    exit 1
fi
if [[ "$(uname -m)" != x86_64 ]]; then
    echo "warning: the committed tables were made on x86_64; HotSpot's Math.sin and cos may differ in the" \
        "last bit on $(uname -m)" >&2
fi

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo="$(cd "${here}/../.." && pwd)"
# The main working tree of the QtRocket repository (a worktree's common git directory is the main one's .git).
main_tree="$(dirname "$(git -C "${repo}" rev-parse --path-format=absolute --git-common-dir)")"
openrocket="${OPENROCKET_DIR:-${main_tree}/../openrocket}"
openrocket="$(cd "${openrocket}" && pwd)"
java="${JAVA:-java}"
javac="${JAVAC:-javac}"
clang_format="${CLANG_FORMAT:-clang-format}"
export CLANG_FORMAT="${clang_format}"
examples="${repo}/data/examples"

services="${openrocket}/core/src/test/java/info/openrocket/core/ServicesForTesting.java"
swing_services="${openrocket}/swing/src/test/java/info/openrocket/swing/ServicesForTesting.java"
motors="${openrocket}/core/src/main/resources/datafiles/thrustcurves/initial_motors.db"
if [[ ! -f "${services}" || ! -f "${swing_services}" || ! -f "${motors}" ]]; then
    echo "error: ${openrocket} is not an OpenRocket checkout (set OPENROCKET_DIR)" >&2
    exit 1
fi
for tool in python3 "${clang_format}"; do
    if ! command -v "${tool}" > /dev/null; then
        echo "error: ${tool} is not on PATH" >&2
        exit 1
    fi
done
for family in rocket-panel thrust-curves flight-config preset-chooser open-designs; do
    if [[ " ${families[*]} " == *" ${family} "* ]] && ! command -v xvfb-run > /dev/null; then
        echo "error: xvfb-run is not on PATH (${family}'s probe needs a display)" >&2
        exit 1
    fi
done

# Double.toString, which writes many numbers of the transcripts, changed in JDK 19 (JDK-4511638).
java_version="$("${java}" -XshowSettings:properties -version 2>&1 |
    sed -n 's/^ *java\.specification\.version = //p')"
if [[ "${java_version}" != 17 ]]; then
    echo "error: ${java} is Java ${java_version:-(unknown)}; the pins are made with Java 17 (set JAVA)" >&2
    exit 1
fi

# The checkout: no modified tracked file, and the preset submodule initialized at the commit the
# superproject records (a clone without it makes Gradle run "git submodule update --remote", which moves it).
commit="$(git -C "${openrocket}" rev-parse HEAD)"
problems=()
if ! git -C "${openrocket}" diff --quiet HEAD --; then
    problems+=("modified tracked files")
fi
while IFS= read -r submodule; do
    if [[ -n "${submodule}" && "${submodule:0:1}" != " " ]]; then
        problems+=("a submodule is not initialized at its recorded commit (git submodule update --init):"$'\n'"${submodule}")
    fi
done < <(git -C "${openrocket}" submodule status)
if ((${#problems[@]} > 0)); then
    for problem in "${problems[@]}"; do
        echo "OpenRocket checkout: ${problem}" >&2
    done
    if [[ "${ALLOW_DIRTY:-0}" != 1 ]]; then
        echo "error: the pins would not match the checkout's commit (ALLOW_DIRTY=1 overrides)" >&2
        exit 1
    fi
    commit="${commit}-dirty"
fi
# The headers record the commit, and their tests expect the manifest's: another commit needs a decision.
recorded="$(sed -n 's/^ *"openrocket": {"commit": "\([0-9a-f]*\)".*/\1/p' \
    "${repo}/tests/data/goldens/manifest.json" 2>/dev/null || true)"
if [[ -n "${recorded}" && "${recorded}" != "${commit%-dirty}" ]]; then
    echo "OpenRocket checkout: at ${commit}; the goldens and pins come from ${recorded}" >&2
    if [[ "${ALLOW_OTHER_COMMIT:-0}" != 1 ]]; then
        echo "error: check out ${recorded} (ALLOW_OTHER_COMMIT=1 overrides)" >&2
        exit 1
    fi
fi
echo "OpenRocket: ${openrocket} @ ${commit}" >&2

if [[ -n "${GUI_PINS_WORK:-}" ]]; then
    mkdir -p "${GUI_PINS_WORK}"
    work="$(cd "${GUI_PINS_WORK}" && pwd)"
else
    work="$(mktemp -d)"
    # Kept when the script fails, so that the messages' paths (a probe's log, a differing table) exist.
    trap 'rc=$?; if ((rc == 0)); then rm -rf "${work}"; else echo "the work files: ${work}" >&2; fi' EXIT
fi
out="${work}/out"
rm -rf "${out}"
mkdir -p "${out}"

classpath="${OPENROCKET_CLASSPATH:-}"
if [[ -z "${classpath}" ]]; then
    (cd "${openrocket}" && ./gradlew -q :swing:classes)
    cat > "${work}/classpath.init.gradle" <<'GRADLE'
allprojects {
    afterEvaluate { project ->
        if (project.path == ':swing') {
            project.tasks.register('qtrocketPrintRuntimeClasspath') {
                def classpath = project.sourceSets.main.runtimeClasspath
                doLast {
                    println 'QTROCKET_CP=' + classpath.asPath
                }
            }
        }
    }
}
GRADLE
    classpath="$("${openrocket}/gradlew" -p "${openrocket}" -q --init-script "${work}/classpath.init.gradle" \
        :swing:qtrocketPrintRuntimeClasspath | sed -n 's/^QTROCKET_CP=//p')"
    if [[ -z "${classpath}" ]]; then
        echo "error: could not get OpenRocket's swing runtime classpath from Gradle" >&2
        exit 1
    fi
fi

# The classes must be built from the checkout's HEAD: OpenRocket's build stamps the short commit hash into
# core's build.properties (build.version=<version>-SNAPSHOT-<hash>).
build_version="$(python3 -I "${here}/shared/build_version.py" "${classpath}")" || {
    echo "error: no build.properties with a build.version on the class path" >&2
    exit 1
}
build_hash="${build_version##*-SNAPSHOT-}"
if [[ "${build_hash}" == "${build_version}" || ! "${build_hash}" =~ ^[0-9a-f]{7,40}$ ||
    "${commit}" != "${build_hash}"* ]]; then
    echo "OpenRocket classes: build.version=${build_version}, the checkout is at ${commit}" >&2
    if [[ "${ALLOW_OTHER_COMMIT:-0}" != 1 ]]; then
        echo "error: the classes are not built from the checkout's HEAD (rebuild them, or ALLOW_OTHER_COMMIT=1)" >&2
        exit 1
    fi
fi

# Runs a probe of a family, its transcript (standard output) into ${work}/<class>.txt, its log into
# ${work}/<class>.log.
#   run_probe <family> <class> headless|xvfb [<argument>...]
run_probe() {
    local family="$1" class="$2" display="$3"
    shift 3
    echo "running ${class} (${family})" >&2
    # The test bootstrap classes of core and swing are compiled from OpenRocket's test sources, not copied.
    rm -rf "${work}/classes/${family}"
    mkdir -p "${work}/classes/${family}"
    "${javac}" --release 17 -encoding UTF-8 -nowarn -proc:none -cp "${classpath}" \
        -d "${work}/classes/${family}" "${services}" "${swing_services}" "${here}"/shared/probe/*.java \
        "${here}/${family}"/probe/*.java 2> "${work}/${class}.javac.log" || {
        cat "${work}/${class}.javac.log" >&2
        exit 1
    }
    # A fresh java.util.prefs store and home directory for every probe (OpenRocket's preferences at their
    # defaults, nothing written to the user's home); a fixed locale, time zone and encoding; OpenRocket's
    # motor database; no display but a virtual one of its own.
    rm -rf "${work}/prefs" "${work}/tmp" "${work}/home"
    mkdir -p "${work}/prefs" "${work}/tmp" "${work}/home"
    local -a launcher=() awt=(-Djava.awt.headless=true)
    if [[ "${display}" == xvfb ]]; then
        # xvfb-run sends the command's standard error to its standard output: the probe's goes to its log.
        launcher=(xvfb-run -a -s "-screen 0 1280x1024x24" bash -c 'exec "$@" 2>> "$0"' "${work}/${class}.log")
        awt=(-Djava.awt.headless=false)
    fi
    env -u DISPLAY -u WAYLAND_DISPLAY ${launcher[@]+"${launcher[@]}"} "${java}" -Xmx2g "${awt[@]}" \
        -Duser.language=en -Duser.country=US -Duser.timezone=UTC -Dfile.encoding=UTF-8 \
        -Duser.home="${work}/home" \
        -Djava.util.prefs.userRoot="${work}/prefs" -Djava.util.prefs.systemRoot="${work}/prefs" \
        -Djava.io.tmpdir="${work}/tmp" -Dorg.sqlite.tmpdir="${work}/tmp" \
        -Dlogback.configurationFile="${here}/logback.xml" -Dprobe.motors="${motors}" \
        -cp "${work}/classes/${family}:${classpath}" "probe.${class}" "$@" \
        > "${work}/${class}.txt" 2> "${work}/${class}.log" || {
        echo "error: ${class} failed (its log: ${work}/${class}.log)" >&2
        tail -n 20 "${work}/${class}.log" >&2
        exit 1
    }
}

# Prints the path a family's table is generated into, ${out}/<file name>, after copying the committed file
# there (a generator overwrites a header; a table in test code is spliced into the copy).
#   generated <repository path>
generated() {
    local path="${out}/$(basename "$1")"
    cp "${repo}/$1" "${path}"
    echo "${path}"
}

# Formats a spliced test file as the committed ones are formatted.
#   format <path>
format() {
    "${clang_format}" -i --style="file:${repo}/.clang-format" "$1"
}

# Compares a generated table with the committed one, byte for byte; installs it (unless --check) when they
# differ.
#   finish <repository path>
finish() {
    local file="$1"
    local path="${out}/$(basename "${file}")"
    if cmp -s "${repo}/${file}" "${path}"; then
        echo "same: ${file}" >&2
    elif [[ "${check}" == true ]]; then
        echo "DIFFERENT: ${file} (the generated one: ${path})" >&2
        diff -u "${repo}/${file}" "${path}" | head -n 40 >&2 || true
        status=1
    else
        cp "${path}" "${repo}/${file}"
        echo "wrote: ${file}" >&2
    fi
}

# Checks the lines of a probe's transcript that tests pin by hand (those no table holds) against the
# family's snapshot hand-pinned.txt; never rewrites it (README.md, "Values pinned by hand").
#   check_hand_pins <family> <class> <tests> [--exclude <regex> | --exclude-in <section> <regex>]...
check_hand_pins() {
    local family="$1" class="$2" tests="$3"
    shift 3
    if python3 -I "${here}/shared/check_hand_pins.py" "${work}/${class}.txt" "${here}/${family}/hand-pinned.txt" \
        "${work}/${class}.hand-pinned.txt" "$@"; then
        echo "same: the values ${tests} pin by hand" >&2
    else
        echo "DIFFERENT: the values ${tests} pin by hand: review them, then copy" \
            "${work}/${class}.hand-pinned.txt to ${family}/hand-pinned.txt" >&2
        status=1
    fi
}

status=0
for family in "${families[@]}"; do
    case "${family}" in
    figure-paint)
        run_probe "${family}" FigurePaintProbe headless "${examples}"
        file=tests/gui/figure/RocketFigurePaintPins.h
        python3 -I "${here}/${family}/gen_rocket_figure_paint_pins.py" "${work}/FigurePaintProbe.txt" \
            "$(generated "${file}")" "${commit}"
        ;;
    rocket-info)
        run_probe "${family}" RocketInfoProbe headless "${examples}"
        file=tests/gui/figure/RocketInfoPins.h
        python3 -I "${here}/${family}/gen_rocket_info_pins.py" "${work}/RocketInfoProbe.txt" \
            "$(generated "${file}")" "${commit}"
        ;;
    ribbon)
        run_probe "${family}" RibbonProbe headless "${examples}"
        file=tests/gui/widgets/RibbonPins.h
        python3 -I "${here}/${family}/gen_ribbon_pins.py" "${work}/RibbonProbe.txt" "$(generated "${file}")" \
            "${commit}"
        ;;
    rocket-panel)
        # RocketPanel's clicks ask the toolkit for the menu shortcut key, which a headless AWT refuses.
        run_probe "${family}" RocketPanelProbe xvfb "${examples}"
        file=tests/gui/figure/RocketPanelPins.h
        python3 -I "${here}/${family}/gen_rocket_panel_pins.py" "${work}/RocketPanelProbe.txt" \
            "$(generated "${file}")" "${commit}"
        ;;
    preferences)
        run_probe "${family}" PreferencesProbe headless
        file=tests/gui/dialogs/PreferencesPagePins.h
        python3 -I "${here}/${family}/gen_preferences_pins.py" "${work}/PreferencesProbe.txt" \
            "$(generated "${file}")" "${commit}" "${work}/home"
        ;;
    scale-figure)
        run_probe "${family}" ScaleProbe headless "${examples}/A simple model rocket.ork" \
            "${examples}/Two stage high power rocket.ork"
        check_hand_pins "${family}" ScaleProbe "ScaleSelectorTests.cpp and RulerTests.cpp" --exclude '^== figure '
        file=tests/gui/figure/ScaleFigureTests.cpp
        python3 -I "${here}/${family}/figure_rows.py" "${work}/ScaleProbe.txt" > "${work}/ScaleProbe.rows"
        python3 -I "${here}/${family}/splice_figure_rows.py" "${work}/ScaleProbe.rows" "$(generated "${file}")"
        ;;
    material-binding)
        run_probe "${family}" B2Probe headless
        check_hand_pins "${family}" B2Probe "the binding tests (tests/gui/binding)" \
            --exclude-in '^(BULK \(with user and document materials\)|LINE|SURFACE): size=' '^  [0-9]+ ' \
            --exclude-in '^(BULK|LINE|SURFACE) groups:$' '^  [^ ]+: '
        file=tests/gui/binding/MaterialBindingTests.cpp
        python3 -I "${here}/${family}/rows.py" "${work}/B2Probe.txt" > "${work}/B2Probe.rows"
        spliced="$(generated "${file}")"
        python3 -I "${here}/${family}/splice_material_rows.py" "${work}/B2Probe.rows" "${spliced}"
        format "${spliced}"
        ;;
    material-combo)
        run_probe "${family}" B4Probe headless
        python3 -I "${here}/${family}/rows.py" "${work}/B4Probe.txt" > "${work}/B4Probe.rows"
        if python3 -I "${here}/${family}/check_combo_rows.py" "${work}/B4Probe.rows" \
            "${repo}/tests/gui/widgets/MaterialComboBoxTests.cpp"; then
            echo "same: tests/gui/widgets/MaterialComboBoxTests.cpp (its groups and searches)" >&2
        else
            echo "DIFFERENT: tests/gui/widgets/MaterialComboBoxTests.cpp (its groups and searches, from" \
                "${work}/B4Probe.rows: edit the test by hand)" >&2
            status=1
        fi
        check_hand_pins "${family}" B4Probe "MaterialComboBoxTests.cpp and EnumComboBoxTests.cpp" \
            --exclude '^  menu ' --exclude "^search '"
        continue
        ;;
    component-editing)
        run_probe "${family}" ActionsProbe headless
        run_probe "${family}" ClipboardProbe headless
        file=tests/gui/main/ComponentEditingTests.cpp
        spliced="$(generated "${file}")"
        python3 -I "${here}/${family}/gen_component_editing.py" "${here}/${family}/probe/ActionsProbe.java" \
            "${work}/ActionsProbe.txt" "${here}/${family}/probe/ClipboardProbe.java" "${work}/ClipboardProbe.txt" \
            "${spliced}"
        format "${spliced}"
        ;;
    component-drop)
        run_probe "${family}" DndProbe headless
        file=tests/gui/tree/ComponentDropTests.cpp
        spliced="$(generated "${file}")"
        python3 -I "${here}/${family}/gen_component_drop.py" "${here}/${family}/probe/DndProbe.java" \
            "${work}/DndProbe.txt" "${spliced}"
        format "${spliced}"
        ;;
    actions-rules)
        run_probe "${family}" ActionsRulesProbe headless
        file=tests/gui/main/RocketActionsTests.cpp
        spliced="$(generated "${file}")"
        python3 -I "${here}/${family}/gen_actions_rules.py" "${work}/ActionsRulesProbe.txt" "${spliced}"
        format "${spliced}"
        ;;
    thrust-curves)
        # ThrustCurveMotorPlotDialog is a JDialog, which a headless AWT refuses to make (it is never shown).
        run_probe "${family}" ThrustCurvesProbe xvfb
        file=tests/gui/plot/ThrustCurvePins.h
        python3 -I "${here}/${family}/gen_thrust_curve_pins.py" "${work}/ThrustCurvesProbe.txt" \
            "$(generated "${file}")" "${commit}"
        ;;
    flight-config)
        run_probe "${family}" FlightConfigurationTablesProbe headless "${examples}"
        file=tests/gui/flightconfig/FlightConfigurationTablePins.h
        python3 -I "${here}/${family}/gen_flight_configuration_table_pins.py" \
            "${work}/FlightConfigurationTablesProbe.txt" "$(generated "${file}")" "${commit}"
        # Part 2, the operations: OpenRocket's real panels and their modal dialogs, which need a display.
        run_probe "${family}" FlightConfigurationOperationsProbe xvfb "${examples}"
        operations=tests/gui/flightconfig/FlightConfigurationOperationPins.h
        python3 -I "${here}/${family}/gen_flight_configuration_operation_pins.py" \
            "${work}/FlightConfigurationOperationsProbe.txt" "$(generated "${operations}")" "${commit}"
        finish "${operations}"
        ;;
    motor-chooser)
        # OpenRocket's ThrustCurveMotorSelectionPanel is a JPanel: it runs headless (the probe does what
        # MotorChooserDialog, a JDialog, adds to it).
        run_probe "${family}" MotorChooserMountsProbe headless "${examples}"
        file=tests/gui/dialogs/motor/MotorChooserPins.h
        python3 -I "${here}/${family}/gen_motor_chooser_mount_pins.py" "${work}/MotorChooserMountsProbe.txt" \
            "$(generated "${file}")" "${commit}"
        ;;
    preset-chooser)
        # The presets of tests/core/rocket/preset/TestPresetDatabase.h's smallPresetDatabase(), in its order,
        # then the bundled presets' directory (ComponentPresetTableProbe's bundled database).
        presets=("${repo}/tests/data/presets/every-kind.orc" "${repo}/tests/data/presets/legacy.orc"
            "${repo}/data/components/database/estes_ps2.orc")
        run_probe "${family}" PresetGuiProbe headless "${presets[@]}" "${repo}/data/components"
        # The configuration panels need a parent dialog and the parts library is shown (a robot presses
        # its keys): both refused by a headless AWT. Their transcripts follow PresetGuiProbe's in the
        # family's one snapshot.
        run_probe "${family}" ConfigPresetRowProbe xvfb "${presets[@]}"
        run_probe "${family}" PresetShownProbe xvfb "${presets[@]}"
        cat "${work}/PresetGuiProbe.txt" "${work}/ConfigPresetRowProbe.txt" "${work}/PresetShownProbe.txt" \
            > "${work}/PresetHandPins.txt"
        check_hand_pins "${family}" PresetHandPins \
            "TableUiPreferencesTests.cpp, PresetBindingTests.cpp, PresetComboBoxTests.cpp, TableRowTraversalTests.cpp, the ComponentPreset*Tests.cpp of tests/gui/dialogs/preset, ComponentConfigDialogTests.cpp and RocketComponentConfigTests.cpp"
        # The parts library's dialog over the bundled presets: a JDialog, which a headless AWT refuses to make
        # (it is never shown).
        run_probe "${family}" PresetChooserProbe xvfb "${repo}/data/components"
        file=tests/gui/dialogs/preset/PresetChooserPins.h
        python3 -I "${here}/${family}/gen_preset_chooser_pins.py" "${work}/PresetChooserProbe.txt" \
            "$(generated "${file}")" "${commit}"
        ;;
    open-designs)
        # The designs of OpenDesignsExitGolden, in its order; OpenRocket's FlightConfigurationPanel makes a
        # MotorChooserDialog, a JDialog, which a headless AWT refuses to make (it is never shown). The six
        # presets of the example designs come from the bundled presets' directory.
        python3 -I "${here}/${family}/designs.py" "${repo}" > "${work}/open-designs.txt"
        run_probe "${family}" OpenDesignsTabProbe xvfb "${work}/open-designs.txt" "${repo}/data/components"
        tab=tests/gui/exit/OpenDesignsTabPins.h
        python3 -I "${here}/${family}/gen_open_designs_tab_pins.py" "${work}/OpenDesignsTabProbe.txt" \
            "$(generated "${tab}")" "${commit}"
        finish "${tab}"
        # The parts library of each component of the examples that takes presets, over the bundled presets:
        # OpenRocket's ComponentPresetChooserDialog, a JDialog (never shown).
        run_probe "${family}" OpenDesignsPartsLibraryProbe xvfb "${work}/open-designs.txt" "${repo}/data/components"
        file=tests/gui/exit/OpenDesignsPartsLibraryPins.h
        python3 -I "${here}/${family}/gen_open_designs_parts_library_pins.py" \
            "${work}/OpenDesignsPartsLibraryProbe.txt" "$(generated "${file}")" "${commit}"
        ;;
    esac
    finish "${file}"
done
exit "${status}"
