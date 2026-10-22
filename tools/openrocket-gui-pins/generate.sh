#!/usr/bin/env bash
# Regenerates the pin tables of the GUI tests from OpenRocket's own Swing code (see README.md): runs each
# family's Java probe against OpenRocket's compiled classes and turns its transcript into the table.
#
# Usage:
#   tools/openrocket-gui-pins/generate.sh [--check] [<family>...]
#
# The families (all of them when none is named):
#   figure-paint      tests/gui/figure/RocketFigurePaintPins.h
#   rocket-info       tests/gui/figure/RocketInfoPins.h
#   ribbon            tests/gui/widgets/RibbonPins.h
#   rocket-panel      tests/gui/figure/RocketPanelPins.h (the probe runs under xvfb-run)
#   preferences       tests/gui/dialogs/PreferencesPagePins.h
#   scale-figure      the table kFigureRows of tests/gui/figure/ScaleFigureTests.cpp
#   material-binding  the six row and group tables of tests/gui/binding/MaterialBindingTests.cpp
#   material-combo    the texts of tests/gui/widgets/MaterialComboBoxTests.cpp (checked only: the test
#                     assembles its tables by hand from the rows, see README.md)
#
# Every table is written into the work directory first (a spliced test file into a copy of the committed
# one) and compared with the committed file: an equal table leaves the committed file as it is, another one
# replaces it. Two families compare apart from Java's run-to-run noise (README.md, "Reproducibility"):
# figure-paint the order of shapes of equal display order, of motors and of hit points, rocket-info the last
# bits (at most 4 units in the last place) of floating-point values; such a difference alone leaves the
# committed table as it is. --check compares only: the exit status is 1 when a table differs. material-combo
# is compared either way.
#
# Environment:
#   OPENROCKET_DIR        the OpenRocket checkout (default: "openrocket" next to the main working tree of the
#                         QtRocket repository); it should be at the commit of tests/data/goldens/manifest.json,
#                         which the headers record (and the golden tests check)
#   JAVA, JAVAC           the java and javac executables (default: on PATH); they must be Java 17
#   OPENROCKET_CLASSPATH  the runtime class path of OpenRocket's swing module (the compiled classes and
#                         resources of core and swing and the jars of their dependencies). When it is not set,
#                         the script runs, as tools/openrocket-panel-pins/generate.sh does:
#                           1. ./gradlew :swing:classes in the checkout (incremental: the classes match the
#                              checked-out sources)
#                           2. ./gradlew --init-script <tmp> :swing:qtrocketPrintRuntimeClasspath (a task defined
#                              only in a temporary init script; OpenRocket's build files are not changed)
#   CLANG_FORMAT          the clang-format executable (default: clang-format on PATH)
#   ALLOW_DIRTY=1         accept an OpenRocket checkout with modified tracked files
#   GUI_PINS_WORK         keep the work files (the probes' transcripts and logs, the classes) in this directory
#                         instead of a temporary one
#
# Requires python3, clang-format (the generators format what they write with the repository's .clang-format)
# and, for rocket-panel, xvfb-run.
set -euo pipefail

all_families=(figure-paint rocket-info ribbon rocket-panel preferences scale-figure material-binding
    material-combo)

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
motors="${openrocket}/core/src/main/resources/datafiles/thrustcurves/initial_motors.db"
if [[ ! -f "${services}" || ! -f "${motors}" ]]; then
    echo "error: ${openrocket} is not an OpenRocket checkout (set OPENROCKET_DIR)" >&2
    exit 1
fi
for tool in python3 "${clang_format}"; do
    if ! command -v "${tool}" > /dev/null; then
        echo "error: ${tool} is not on PATH" >&2
        exit 1
    fi
done
if [[ " ${families[*]} " == *" rocket-panel "* ]] && ! command -v xvfb-run > /dev/null; then
    echo "error: xvfb-run is not on PATH (rocket-panel's probe needs a display)" >&2
    exit 1
fi

# Double.toString, which writes many numbers of the transcripts, changed in JDK 19 (JDK-4511638).
java_version="$("${java}" -XshowSettings:properties -version 2>&1 |
    sed -n 's/^ *java\.specification\.version = //p')"
if [[ "${java_version}" != 17 ]]; then
    echo "error: ${java} is Java ${java_version:-(unknown)}; the pins are made with Java 17 (set JAVA)" >&2
    exit 1
fi

commit="$(git -C "${openrocket}" rev-parse HEAD)"
if ! git -C "${openrocket}" diff --quiet HEAD --; then
    echo "OpenRocket checkout: modified tracked files" >&2
    if [[ "${ALLOW_DIRTY:-0}" != 1 ]]; then
        echo "error: the pins would not match the checkout's commit (ALLOW_DIRTY=1 overrides)" >&2
        exit 1
    fi
    commit="${commit}-dirty"
fi
recorded="$(sed -n 's/^ *"openrocket": {"commit": "\([0-9a-f]*\)".*/\1/p' \
    "${repo}/tests/data/goldens/manifest.json" 2>/dev/null || true)"
if [[ -n "${recorded}" && "${recorded}" != "${commit}" ]]; then
    echo "warning: the goldens come from OpenRocket ${recorded}, the checkout is at ${commit}" >&2
fi
echo "OpenRocket: ${openrocket} @ ${commit}" >&2

if [[ -n "${GUI_PINS_WORK:-}" ]]; then
    mkdir -p "${GUI_PINS_WORK}"
    work="$(cd "${GUI_PINS_WORK}" && pwd)"
else
    work="$(mktemp -d)"
    trap 'rm -rf "${work}"' EXIT
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

# Runs the probe of a family, its transcript (standard output) into ${work}/<family>.txt.
#   run_probe <family> <class> headless|xvfb [<argument>...]
run_probe() {
    local family="$1" class="$2" display="$3"
    shift 3
    echo "running ${class} (${family})" >&2
    # core's test bootstrap class is compiled from OpenRocket's test sources, not copied.
    rm -rf "${work}/classes/${family}"
    mkdir -p "${work}/classes/${family}"
    "${javac}" --release 17 -encoding UTF-8 -nowarn -proc:none -cp "${classpath}" \
        -d "${work}/classes/${family}" "${services}" "${here}"/shared/probe/*.java "${here}/${family}"/probe/*.java
    # A fresh java.util.prefs store for every probe (OpenRocket's preferences at their defaults); a fixed
    # locale, time zone and encoding; OpenRocket's motor database; no display but a virtual one of its own.
    rm -rf "${work}/prefs" "${work}/tmp"
    mkdir -p "${work}/prefs" "${work}/tmp"
    local -a launcher=() awt=(-Djava.awt.headless=true)
    if [[ "${display}" == xvfb ]]; then
        # xvfb-run sends the command's standard error to its standard output: the probe's goes to its log.
        launcher=(xvfb-run -a -s "-screen 0 1280x1024x24" bash -c 'exec "$@" 2>> "$0"' "${work}/${family}.log")
        awt=(-Djava.awt.headless=false)
    fi
    env -u DISPLAY -u WAYLAND_DISPLAY "${launcher[@]}" "${java}" -Xmx2g "${awt[@]}" \
        -Duser.language=en -Duser.country=US -Duser.timezone=UTC -Dfile.encoding=UTF-8 \
        -Djava.util.prefs.userRoot="${work}/prefs" -Djava.util.prefs.systemRoot="${work}/prefs" \
        -Djava.io.tmpdir="${work}/tmp" -Dorg.sqlite.tmpdir="${work}/tmp" \
        -Dlogback.configurationFile="${here}/logback.xml" -Dprobe.motors="${motors}" \
        -cp "${work}/classes/${family}:${classpath}" "probe.${class}" "$@" \
        > "${work}/${family}.txt" 2> "${work}/${family}.log" || {
        echo "error: ${class} failed (its log: ${work}/${family}.log)" >&2
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

# Compares a generated table with the committed one; installs it (unless --check) when they differ.
#   finish <family> <repository path>
finish() {
    local family="$1" file="$2"
    local path="${out}/$(basename "${file}")"
    local -a compare=(cmp -s)
    case "${family}" in
    figure-paint) compare=(python3 -I "${here}/${family}/compare_paint_pins.py") ;;
    rocket-info) compare=(python3 -I "${here}/${family}/compare_info_pins.py") ;;
    esac
    if "${compare[@]}" "${repo}/${file}" "${path}"; then
        echo "same: ${file}" >&2
    elif [[ "${check}" == true ]]; then
        echo "DIFFERENT: ${file} (the generated one: ${path})" >&2
        status=1
    else
        cp "${path}" "${repo}/${file}"
        echo "wrote: ${file}" >&2
    fi
}

status=0
for family in "${families[@]}"; do
    case "${family}" in
    figure-paint)
        run_probe "${family}" FigurePaintProbe headless "${examples}"
        file=tests/gui/figure/RocketFigurePaintPins.h
        python3 -I "${here}/${family}/gen_rocket_figure_paint_pins.py" "${work}/${family}.txt" \
            "$(generated "${file}")" "${commit}"
        ;;
    rocket-info)
        run_probe "${family}" RocketInfoProbe headless "${examples}"
        file=tests/gui/figure/RocketInfoPins.h
        python3 -I "${here}/${family}/gen_rocket_info_pins.py" "${work}/${family}.txt" "$(generated "${file}")" \
            "${commit}"
        ;;
    ribbon)
        run_probe "${family}" RibbonProbe headless "${examples}"
        file=tests/gui/widgets/RibbonPins.h
        python3 -I "${here}/${family}/gen_ribbon_pins.py" "${work}/${family}.txt" "$(generated "${file}")" \
            "${commit}"
        ;;
    rocket-panel)
        # RocketPanel's clicks ask the toolkit for the menu shortcut key, which a headless AWT refuses.
        run_probe "${family}" RocketPanelProbe xvfb "${examples}"
        file=tests/gui/figure/RocketPanelPins.h
        python3 -I "${here}/${family}/gen_rocket_panel_pins.py" "${work}/${family}.txt" "$(generated "${file}")" \
            "${commit}"
        ;;
    preferences)
        run_probe "${family}" PreferencesProbe headless
        file=tests/gui/dialogs/PreferencesPagePins.h
        python3 -I "${here}/${family}/gen_preferences_pins.py" "${work}/${family}.txt" "$(generated "${file}")" \
            "${commit}"
        ;;
    scale-figure)
        run_probe "${family}" ScaleProbe headless "${examples}/A simple model rocket.ork" \
            "${examples}/Two stage high power rocket.ork"
        file=tests/gui/figure/ScaleFigureTests.cpp
        python3 -I "${here}/${family}/figure_rows.py" "${work}/${family}.txt" > "${work}/${family}.rows"
        python3 -I "${here}/${family}/splice_figure_rows.py" "${work}/${family}.rows" "$(generated "${file}")"
        ;;
    material-binding)
        run_probe "${family}" B2Probe headless
        file=tests/gui/binding/MaterialBindingTests.cpp
        python3 -I "${here}/${family}/rows.py" "${work}/${family}.txt" > "${work}/${family}.rows"
        spliced="$(generated "${file}")"
        python3 -I "${here}/${family}/splice_material_rows.py" "${work}/${family}.rows" "${spliced}"
        "${clang_format}" -i --style="file:${repo}/.clang-format" "${spliced}"
        ;;
    material-combo)
        run_probe "${family}" B4Probe headless
        python3 -I "${here}/${family}/rows.py" "${work}/${family}.txt" > "${work}/${family}.rows"
        if python3 -I "${here}/${family}/check_combo_rows.py" "${work}/${family}.rows" \
            "${repo}/tests/gui/widgets/MaterialComboBoxTests.cpp"; then
            echo "same: tests/gui/widgets/MaterialComboBoxTests.cpp (its texts)" >&2
        else
            echo "DIFFERENT: tests/gui/widgets/MaterialComboBoxTests.cpp (its texts; the rows:" \
                "${work}/${family}.rows)" >&2
            status=1
        fi
        continue
        ;;
    esac
    finish "${family}" "${file}"
done
exit "${status}"
