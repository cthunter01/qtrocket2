#!/usr/bin/env bash
# Regenerates the pin tables of the configuration panels' golden tests from OpenRocket's own Swing panels (see
# README.md): tests/gui/configdialog/BodyPanelPins.h, LugPanelPins.h, FinPanelPins.h and FreeformPanelPins.h.
#
# Usage:
#   tools/openrocket-panel-pins/generate.sh [--check]
#
# --check writes the headers into a temporary directory instead and compares them with the committed ones byte
# for byte: the exit status is 1 when one of them differs.
#
# Environment:
#   OPENROCKET_DIR        the OpenRocket checkout (default: "openrocket" next to the main working tree of the
#                         QtRocket repository); it should be at the commit of tests/data/goldens/manifest.json,
#                         which the headers record (and the golden tests check)
#   JAVA, JAVAC           the java and javac executables (default: on PATH); they must be Java 17
#   OPENROCKET_CLASSPATH  the runtime class path of OpenRocket's swing module (the compiled classes and
#                         resources of core and swing and the jars of their dependencies). When it is not set,
#                         the script runs, as tools/openrocket-fin-import/generate.sh does:
#                           1. ./gradlew :swing:classes in the checkout (incremental: the classes match the
#                              checked-out sources)
#                           2. ./gradlew --init-script <tmp> :swing:qtrocketPrintRuntimeClasspath (a task defined
#                              only in a temporary init script; OpenRocket's build files are not changed)
#   ALLOW_DIRTY=1         accept an OpenRocket checkout with modified tracked files
#   PINS_OUT              write the headers into this directory instead of tests/gui/configdialog
#   PANEL_PINS_WORK       keep the work files (the probes' transcripts and logs) in this directory instead of a
#                         temporary one
#
# Requires xvfb-run (the panels live in a JDialog, which needs a display), python3 and clang-format (the
# generators format the headers with the repository's .clang-format).
set -euo pipefail

check=false
if (($# == 1)) && [[ "$1" == --check ]]; then
    check=true
elif (($# > 0)); then
    echo "usage: $(basename "${BASH_SOURCE[0]}") [--check]   (see the head of the script for the environment)" >&2
    exit 2
fi

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo="$(cd "${here}/../.." && pwd)"
# The main working tree of the QtRocket repository (a worktree's common git directory is the main one's .git).
main_tree="$(dirname "$(git -C "${repo}" rev-parse --path-format=absolute --git-common-dir)")"
openrocket="${OPENROCKET_DIR:-${main_tree}/../openrocket}"
openrocket="$(cd "${openrocket}" && pwd)"
java="${JAVA:-java}"
javac="${JAVAC:-javac}"
committed="${repo}/tests/gui/configdialog"

services="${openrocket}/core/src/test/java/info/openrocket/core/ServicesForTesting.java"
motors="${openrocket}/core/src/main/resources/datafiles/thrustcurves/initial_motors.db"
if [[ ! -f "${services}" || ! -f "${motors}" ]]; then
    echo "error: ${openrocket} is not an OpenRocket checkout (set OPENROCKET_DIR)" >&2
    exit 1
fi
for tool in xvfb-run python3 clang-format; do
    if ! command -v "${tool}" > /dev/null; then
        echo "error: ${tool} is not on PATH" >&2
        exit 1
    fi
done

# Double.toString, which writes the numbers of the transcripts, changed in JDK 19 (JDK-4511638).
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

if [[ -n "${PANEL_PINS_WORK:-}" ]]; then
    work="${PANEL_PINS_WORK}"
    mkdir -p "${work}"
else
    work="$(mktemp -d)"
    trap 'rm -rf "${work}"' EXIT
fi
if [[ "${check}" == true ]]; then
    out="${work}/headers"
else
    out="${PINS_OUT:-${committed}}"
fi
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

# core's test bootstrap class is compiled from OpenRocket's test sources, not copied.
rm -rf "${work}/classes"
mkdir -p "${work}/classes"
"${javac}" --release 17 -encoding UTF-8 -nowarn -proc:none -cp "${classpath}" -d "${work}/classes" \
    "${services}" "${here}"/src/probe/*.java

probes=(BodyPanelProbe:gen_body_panel_pins.py:BodyPanelPins.h
    LugPanelProbe:gen_lug_panel_pins.py:LugPanelPins.h
    FinPanelProbe:gen_fin_panel_pins.py:FinPanelPins.h
    FreeformPanelProbe:gen_freeform_panel_pins.py:FreeformPanelPins.h)
for entry in "${probes[@]}"; do
    IFS=: read -r probe generator header <<< "${entry}"
    echo "running ${probe}" >&2
    # A fresh java.util.prefs store for every probe (OpenRocket's preferences at their defaults); a fixed
    # locale, time zone and encoding; a display of its own (xvfb-run -a), not the user's.
    rm -rf "${work}/prefs"
    mkdir -p "${work}/prefs"
    env -u DISPLAY -u WAYLAND_DISPLAY xvfb-run -a "${java}" -Xmx2g -Djava.awt.headless=false \
        -Duser.language=en -Duser.country=US -Duser.timezone=UTC -Dfile.encoding=UTF-8 \
        -Djava.util.prefs.userRoot="${work}/prefs" -Djava.util.prefs.systemRoot="${work}/prefs" \
        -Djava.io.tmpdir="${work}" -Dorg.sqlite.tmpdir="${work}" \
        -cp "${work}/classes:${classpath}" "probe.${probe}" "${repo}/data/examples" "${work}/${probe}.txt" \
        "${motors}" > "${work}/${probe}.log" 2>&1 || {
        echo "error: ${probe} failed (its log: ${work}/${probe}.log)" >&2
        exit 1
    }
    python3 -I "${here}/${generator}" "${work}/${probe}.txt" "${out}/${header}" "${commit}"
done

if [[ "${check}" == true ]]; then
    status=0
    for entry in "${probes[@]}"; do
        IFS=: read -r probe generator header <<< "${entry}"
        if cmp -s "${out}/${header}" "${committed}/${header}"; then
            echo "same: ${header}" >&2
        else
            echo "DIFFERENT: ${header}" >&2
            status=1
        fi
    done
    exit "${status}"
fi
echo "wrote ${out}/BodyPanelPins.h, LugPanelPins.h, FinPanelPins.h and FreeformPanelPins.h" >&2
