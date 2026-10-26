#!/usr/bin/env bash
# Regenerates the pin tables of the core's motor-chooser tests from OpenRocket's own code (see README.md):
# runs each family's Java probe against OpenRocket's compiled classes and turns its transcript into the table.
#
# Usage:
#   tools/openrocket-motor-chooser/generate.sh [--check] [<family>...]
#
# The families (all of them when none is named):
#   correlation   tests/core/motor/MotorCorrelationPins.h
#   chooser       tests/core/motor/chooser/MotorChooserPins.h (OpenRocket's real Swing selection panel, headless;
#                 Linux only, see below)
#
# Every table is written into the work directory first and compared with the committed file byte for byte: an
# equal table leaves the committed file as it is, another one replaces it. The probes print the same on every
# run. --check compares only: the exit status is 1 when a table differs.
#
# Environment (as tools/openrocket-gui-pins/generate.sh):
#   OPENROCKET_DIR        the OpenRocket checkout (default: "openrocket" next to the main working tree of the
#                         QtRocket repository), at the commit of tests/data/goldens/manifest.json, which the
#                         headers record
#   JAVA, JAVAC           the java and javac executables (default: on PATH); they must be Java 17
#   OPENROCKET_CLASSPATH  the runtime class path of OpenRocket's core module (its compiled classes and
#                         resources and the jars of its dependencies; the swing module's class path, which
#                         holds it, does as well), built from the checkout's HEAD (its build.properties says
#                         which commit); the chooser family needs the swing module's. When it is not set, the
#                         script runs, for the module the families need (core, or swing for chooser):
#                           1. ./gradlew :<module>:classes in the checkout (incremental: the classes match the
#                              checked-out sources)
#                           2. ./gradlew --init-script <tmp> :<module>:qtrocketPrintRuntimeClasspath (a task
#                              defined only in a temporary init script; OpenRocket's build files are not
#                              changed)
#   CLANG_FORMAT          the clang-format executable (default: clang-format on PATH)
#   ALLOW_DIRTY=1         accept an OpenRocket checkout with modified tracked files
#   ALLOW_OTHER_COMMIT=1  accept a checkout at another commit than the manifest's, or classes built from
#                         another commit than the checkout's HEAD (the headers record what was used)
#   MOTOR_CHOOSER_PINS_WORK  keep the work files (the probes' transcripts and logs, the classes) in this
#                         directory instead of a temporary one (a temporary one is also kept when the script
#                         fails)
#
# Requires python3 and clang-format (the generators format what they write with the repository's
# .clang-format). The probes read data/motors/initial_motors.db, the database the tests read (OpenRocket's
# bundled one, byte for byte), and nothing of the user's: each runs with a home directory and a
# java.util.prefs store of its own in the work directory. The chooser probe keeps OpenRocket's preferences
# there (SwingPreferences), which only Linux's java.util.prefs implementation reads from that place: it runs
# on Linux only (elsewhere it would clear the user's own OpenRocket preferences), as
# tools/openrocket-gui-pins does.
set -euo pipefail

all_families=(correlation chooser)

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

# The chooser probe needs OpenRocket's Swing classes and its preferences store isolated (see above).
needs_swing=false
if [[ " ${families[*]} " == *" chooser "* ]]; then
    needs_swing=true
    if [[ "$(uname -s)" != Linux ]]; then
        echo "error: the chooser probe runs on Linux only: $(uname -s) ignores java.util.prefs.userRoot, and" \
            "it would clear your own OpenRocket preferences" >&2
        exit 1
    fi
fi
module=core
if [[ "${needs_swing}" == true ]]; then
    module=swing
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
motors="${repo}/data/motors/initial_motors.db"
custom_motor="${repo}/tests/data/motors/test1.eng"
services="${openrocket}/core/src/test/java/info/openrocket/core/ServicesForTesting.java"
swing_services="${openrocket}/swing/src/test/java/info/openrocket/swing/ServicesForTesting.java"
uuids="${repo}/tools/openrocket-gui-pins/shared/probe/DeterministicUuids.java"

if [[ ! -f "${openrocket}/core/src/main/java/info/openrocket/core/utils/MotorCorrelation.java" ]]; then
    echo "error: ${openrocket} is not an OpenRocket checkout (set OPENROCKET_DIR)" >&2
    exit 1
fi
for tool in python3 "${clang_format}"; do
    if ! command -v "${tool}" > /dev/null; then
        echo "error: ${tool} is not on PATH" >&2
        exit 1
    fi
done

# Double.toString, which writes the decimal numbers of the transcripts, changed in JDK 19 (JDK-4511638).
java_version="$("${java}" -XshowSettings:properties -version 2>&1 |
    sed -n 's/^ *java\.specification\.version = //p')"
if [[ "${java_version}" != 17 ]]; then
    echo "error: ${java} is Java ${java_version:-(unknown)}; the pins are made with Java 17 (set JAVA)" >&2
    exit 1
fi

# The checkout: no modified tracked file.
commit="$(git -C "${openrocket}" rev-parse HEAD)"
if ! git -C "${openrocket}" diff --quiet HEAD --; then
    echo "OpenRocket checkout: modified tracked files" >&2
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

if [[ -n "${MOTOR_CHOOSER_PINS_WORK:-}" ]]; then
    mkdir -p "${MOTOR_CHOOSER_PINS_WORK}"
    work="$(cd "${MOTOR_CHOOSER_PINS_WORK}" && pwd)"
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
    (cd "${openrocket}" && ./gradlew -q ":${module}:classes")
    sed "s/@MODULE@/${module}/" > "${work}/classpath.init.gradle" <<'GRADLE'
allprojects {
    afterEvaluate { project ->
        if (project.path == ':@MODULE@') {
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
        ":${module}:qtrocketPrintRuntimeClasspath" | sed -n 's/^QTROCKET_CP=//p')"
    if [[ -z "${classpath}" ]]; then
        echo "error: could not get OpenRocket's ${module} runtime classpath from Gradle" >&2
        exit 1
    fi
fi

# The classes must be built from the checkout's HEAD: OpenRocket's build stamps the short commit hash into
# core's build.properties (build.version=<version>-SNAPSHOT-<hash>).
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

# Runs the probe of a family headless, its transcript (standard output) into ${work}/<class>.txt, its log
# (OpenRocket's warnings and errors) into ${work}/<class>.log. The family's probe/*.java are compiled with
# the sources in the array extra_sources (OpenRocket's test bootstrap classes, compiled from its test
# sources, not copied, and the seeded UUIDs of tools/openrocket-gui-pins).
#   extra_sources=(<source>...); run_probe <family> <class> [<argument>...]
run_probe() {
    local family="$1" class="$2"
    shift 2
    echo "running ${class} (${family})" >&2
    rm -rf "${work}/classes/${family}"
    mkdir -p "${work}/classes/${family}"
    "${javac}" --release 17 -encoding UTF-8 -nowarn -proc:none -cp "${classpath}" \
        -d "${work}/classes/${family}" ${extra_sources[@]+"${extra_sources[@]}"} "${here}/${family}"/probe/*.java \
        2> "${work}/${class}.javac.log" || {
        cat "${work}/${class}.javac.log" >&2
        exit 1
    }
    # A fixed locale, time zone and encoding; a fresh home directory and java.util.prefs store (OpenRocket's
    # preferences at their defaults, nothing written to the user's home); SQLite's native library and every
    # other temporary file in the work directory; no display.
    rm -rf "${work}/tmp" "${work}/prefs" "${work}/home"
    mkdir -p "${work}/tmp" "${work}/prefs" "${work}/home"
    env -u DISPLAY -u WAYLAND_DISPLAY "${java}" -Xmx2g -Djava.awt.headless=true \
        -Duser.language=en -Duser.country=US -Duser.timezone=UTC -Dfile.encoding=UTF-8 \
        -Duser.home="${work}/home" \
        -Djava.util.prefs.userRoot="${work}/prefs" -Djava.util.prefs.systemRoot="${work}/prefs" \
        -Djava.io.tmpdir="${work}/tmp" -Dorg.sqlite.tmpdir="${work}/tmp" \
        -Dlogback.configurationFile="${repo}/tools/openrocket-gui-pins/logback.xml" \
        -cp "${work}/classes/${family}:${classpath}" "probe.${class}" "$@" \
        > "${work}/${class}.txt" 2> "${work}/${class}.log" || {
        echo "error: ${class} failed (its log: ${work}/${class}.log)" >&2
        tail -n 20 "${work}/${class}.log" >&2
        exit 1
    }
}

# Compares a generated table (${out}/<file name>) with the committed one, byte for byte; installs it (unless
# --check) when they differ.
#   finish <repository path>
finish() {
    local file="$1"
    local path="${out}/$(basename "${file}")"
    if [[ -f "${repo}/${file}" ]] && cmp -s "${repo}/${file}" "${path}"; then
        echo "same: ${file}" >&2
    elif [[ "${check}" == true ]]; then
        echo "DIFFERENT: ${file} (the generated one: ${path})" >&2
        if [[ -f "${repo}/${file}" ]]; then
            diff -u "${repo}/${file}" "${path}" | head -n 40 >&2 || true
        fi
        status=1
    else
        cp "${path}" "${repo}/${file}"
        echo "wrote: ${file}" >&2
    fi
}

status=0
for family in "${families[@]}"; do
    case "${family}" in
    correlation)
        extra_sources=()
        run_probe "${family}" MotorCorrelationProbe "${motors}"
        file=tests/core/motor/MotorCorrelationPins.h
        python3 -I "${here}/${family}/gen_motor_correlation_pins.py" "${work}/MotorCorrelationProbe.txt" \
            "${out}/$(basename "${file}")" "${commit}"
        finish "${file}"
        ;;
    chooser)
        extra_sources=("${services}" "${swing_services}" "${uuids}")
        run_probe "${family}" MotorChooserProbe "${motors}" "${custom_motor}"
        file=tests/core/motor/chooser/MotorChooserPins.h
        python3 -I "${here}/${family}/gen_motor_chooser_pins.py" "${work}/MotorChooserProbe.txt" \
            "${out}/$(basename "${file}")" "${commit}"
        finish "${file}"
        ;;
    esac
done
exit "${status}"
