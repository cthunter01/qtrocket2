#!/usr/bin/env bash
# Regenerates the core's plot goldens (tests/data/goldens/plots/, see README.md) from OpenRocket's own plots: runs
# SimulationPlotsProbe against OpenRocket's compiled classes over the stored simulations of the example designs and
# turns its transcript into the two JSON tables.
#
# Usage:
#   tools/openrocket-plots/generate.sh [--check]
#
# Every table is written into the work directory first and compared with the committed file byte for byte: an
# equal table leaves the committed file as it is, another one replaces it. The probe prints the same on every
# run. --check compares only: the exit status is 1 when a table differs.
#
# Environment (as tools/openrocket-motor-chooser/generate.sh):
#   OPENROCKET_DIR        the OpenRocket checkout (default: "openrocket" next to the main working tree of the
#                         QtRocket repository), at the commit of tests/data/goldens/manifest.json, which the
#                         tables record
#   JAVA, JAVAC           the java and javac executables (default: on PATH); they must be Java 17
#   OPENROCKET_CLASSPATH  the runtime class path of OpenRocket's swing module (its compiled classes and resources,
#                         core's, and the jars of their dependencies), built from the checkout's HEAD (its
#                         build.properties says which commit). When it is not set, the script runs
#                           1. ./gradlew :swing:classes in the checkout (incremental)
#                           2. ./gradlew --init-script <tmp> :swing:qtrocketPrintRuntimeClasspath (a task defined
#                              only in a temporary init script; OpenRocket's build files are not changed)
#   ALLOW_DIRTY=1         accept an OpenRocket checkout with modified tracked files
#   ALLOW_OTHER_COMMIT=1  accept a checkout at another commit than the manifest's, or classes built from another
#                         commit than the checkout's HEAD (the tables record what was used)
#   PLOTS_WORK            keep the work files (the probe's transcript and log, the classes) in this directory
#                         instead of a temporary one (a temporary one is also kept when the script fails)
#
# Requires Linux and python3. The probe reads data/motors/initial_motors.db and data/examples (the files the tests
# read: OpenRocket's bundled database and examples, byte for byte) and nothing of the user's: it runs with a home
# directory and a java.util.prefs store of its own in the work directory. Its SwingPreferences live in that store,
# which only Linux's java.util.prefs implementation reads from there (elsewhere it would use the user's own
# OpenRocket preferences), so it runs on Linux only, as tools/openrocket-gui-pins does. It takes about fifteen
# seconds.
set -euo pipefail

check=false
for argument in "$@"; do
    if [[ "${argument}" == --check ]]; then
        check=true
    else
        echo "usage: $(basename "${BASH_SOURCE[0]}") [--check]" >&2
        exit 2
    fi
done
if [[ "$(uname -s)" != Linux ]]; then
    echo "error: the probe runs on Linux only: $(uname -s) ignores java.util.prefs.userRoot" >&2
    exit 1
fi

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo="$(cd "${here}/../.." && pwd)"
# The main working tree of the QtRocket repository (a worktree's common git directory is the main one's .git).
main_tree="$(dirname "$(git -C "${repo}" rev-parse --path-format=absolute --git-common-dir)")"
openrocket="${OPENROCKET_DIR:-${main_tree}/../openrocket}"
openrocket="$(cd "${openrocket}" && pwd)"
java="${JAVA:-java}"
javac="${JAVAC:-javac}"
services="${openrocket}/core/src/test/java/info/openrocket/core/ServicesForTesting.java"
uuids="${repo}/tools/openrocket-gui-pins/shared/probe/DeterministicUuids.java"
manifest="${repo}/tests/data/goldens/manifest.json"

if [[ ! -f "${openrocket}/swing/src/main/java/info/openrocket/swing/gui/plot/SimulationPlot.java" ]]; then
    echo "error: ${openrocket} is not an OpenRocket checkout (set OPENROCKET_DIR)" >&2
    exit 1
fi
if ! command -v python3 > /dev/null; then
    echo "error: python3 is not on PATH" >&2
    exit 1
fi

# Double.toString changed in JDK 19 (JDK-4511638); the probe prints hexadecimal doubles, but OpenRocket's texts
# (the tool tips' DecimalFormat) are pinned as Java 17 writes them.
java_version="$("${java}" -XshowSettings:properties -version 2>&1 |
    sed -n 's/^ *java\.specification\.version = //p')"
if [[ "${java_version}" != 17 ]]; then
    echo "error: ${java} is Java ${java_version:-(unknown)}; the tables are made with Java 17 (set JAVA)" >&2
    exit 1
fi

# The checkout: no modified tracked file.
commit="$(git -C "${openrocket}" rev-parse HEAD)"
dirty=false
if ! git -C "${openrocket}" diff --quiet HEAD --; then
    echo "OpenRocket checkout: modified tracked files" >&2
    if [[ "${ALLOW_DIRTY:-0}" != 1 ]]; then
        echo "error: the tables would not match the checkout's commit (ALLOW_DIRTY=1 overrides)" >&2
        exit 1
    fi
    dirty=true
fi
# The tables record the commit, and their tests expect the manifest's: another commit needs a decision.
recorded="$(sed -n 's/^ *"openrocket": {"commit": "\([0-9a-f]*\)".*/\1/p' "${manifest}" 2>/dev/null || true)"
if [[ -n "${recorded}" && "${recorded}" != "${commit}" ]]; then
    echo "OpenRocket checkout: at ${commit}; the goldens come from ${recorded}" >&2
    if [[ "${ALLOW_OTHER_COMMIT:-0}" != 1 ]]; then
        echo "error: check out ${recorded} (ALLOW_OTHER_COMMIT=1 overrides)" >&2
        exit 1
    fi
fi
echo "OpenRocket: ${openrocket} @ ${commit}" >&2

if [[ -n "${PLOTS_WORK:-}" ]]; then
    mkdir -p "${PLOTS_WORK}"
    work="$(cd "${PLOTS_WORK}" && pwd)"
else
    work="$(mktemp -d)"
    # Kept when the script fails, so that the messages' paths (the probe's log, a differing table) exist.
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

# The classes must be built from the checkout's HEAD: OpenRocket's build stamps the short commit hash into core's
# build.properties (build.version=<version>-SNAPSHOT-<hash>).
build_version="$(python3 -I "${repo}/tools/openrocket-gui-pins/shared/build_version.py" "${classpath}")" || {
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

# The examples, in the order of the goldens' manifest (the inputs of kind "example").
examples=()
while IFS= read -r example; do
    examples+=("${repo}/${example}")
done < <(python3 -I -c '
import json, sys
with open(sys.argv[1], encoding="utf-8") as f:
    for i in json.load(f)["inputs"]:
        if i["kind"] == "example":
            print(i["source"])
' "${manifest}")
if ((${#examples[@]} != 16)); then
    echo "error: the manifest lists ${#examples[@]} examples, not 16" >&2
    exit 1
fi

# The probe, compiled with OpenRocket's test bootstrap class (from its test sources, not copied) and the seeded
# UUIDs of tools/openrocket-gui-pins; run headless with a fixed locale, time zone and encoding, a fresh home
# directory and java.util.prefs store (OpenRocket's preferences at their defaults, nothing written to the user's
# home), SQLite's native library and every other temporary file in the work directory, and no display.
echo "running SimulationPlotsProbe" >&2
rm -rf "${work}/classes" "${work}/tmp" "${work}/prefs" "${work}/home"
mkdir -p "${work}/classes" "${work}/tmp" "${work}/prefs" "${work}/home"
"${javac}" --release 17 -encoding UTF-8 -nowarn -proc:none -cp "${classpath}" -d "${work}/classes" \
    "${services}" "${uuids}" "${here}"/probe/*.java 2> "${work}/javac.log" || {
    cat "${work}/javac.log" >&2
    exit 1
}
env -u DISPLAY -u WAYLAND_DISPLAY "${java}" -Xmx2g -Djava.awt.headless=true \
    -Duser.language=en -Duser.country=US -Duser.timezone=UTC -Dfile.encoding=UTF-8 \
    -Duser.home="${work}/home" \
    -Djava.util.prefs.userRoot="${work}/prefs" -Djava.util.prefs.systemRoot="${work}/prefs" \
    -Djava.io.tmpdir="${work}/tmp" -Dorg.sqlite.tmpdir="${work}/tmp" \
    -Dlogback.configurationFile="${repo}/tools/openrocket-gui-pins/logback.xml" -Dqtrocket.repo="${repo}" \
    -cp "${work}/classes:${classpath}" info.openrocket.swing.gui.plot.SimulationPlotsProbe "${examples[@]}" \
    > "${work}/SimulationPlotsProbe.txt" 2> "${work}/SimulationPlotsProbe.log" || {
    echo "error: SimulationPlotsProbe failed (its log: ${work}/SimulationPlotsProbe.log)" >&2
    tail -n 20 "${work}/SimulationPlotsProbe.log" >&2
    exit 1
}
python3 -I "${here}/gen_simulation_plots.py" "${work}/SimulationPlotsProbe.txt" "${out}" "${commit}" "${dirty}"

status=0
for file in tests/data/goldens/plots/simulation-plots.json tests/data/goldens/plots/simulation-plot-details.json; do
    path="${out}/$(basename "${file}")"
    if [[ -f "${repo}/${file}" ]] && cmp -s "${repo}/${file}" "${path}"; then
        echo "same: ${file}" >&2
    elif [[ "${check}" == true ]]; then
        echo "DIFFERENT: ${file} (the generated one: ${path})" >&2
        if [[ -f "${repo}/${file}" ]]; then
            diff -u "${repo}/${file}" "${path}" | head -n 40 >&2 || true
        fi
        status=1
    else
        mkdir -p "$(dirname "${repo}/${file}")"
        cp "${path}" "${repo}/${file}"
        echo "wrote: ${file}" >&2
    fi
done
exit "${status}"
