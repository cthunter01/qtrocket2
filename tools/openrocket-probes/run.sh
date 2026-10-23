#!/usr/bin/env bash
# Runs a probe of tools/openrocket-probes against OpenRocket's compiled classes and compares what it prints
# with expected.txt, the transcript the tests were pinned from by hand (see README.md).
#
# Usage:
#   tools/openrocket-probes/run.sh <probe>...   (the probes: the subdirectories, e.g. design-information)
#
# Exit status 1 when a transcript differs from its expected.txt (the difference on standard error, the new
# transcript in the work directory): review the tests that pin it, then copy it over expected.txt.
#
# Environment (as tools/openrocket-gui-pins/generate.sh):
#   OPENROCKET_DIR        the OpenRocket checkout (default: "openrocket" next to the main working tree of the
#                         QtRocket repository), at the commit of tests/data/goldens/manifest.json
#   JAVA, JAVAC           the java and javac executables (default: on PATH); they must be Java 17
#   OPENROCKET_CLASSPATH  the runtime class path of OpenRocket's swing module built from the checkout's HEAD
#                         (otherwise Gradle builds and prints it, as tools/openrocket-panel-pins does)
#   PROBES_WORK           keep the work files (classes, transcripts, logs) in this directory instead of a
#                         temporary one (a temporary one is also kept when the script fails)
#
# Requires Linux: the probes' preferences store is isolated through java.util.prefs.userRoot, which only
# Linux's preferences implementation reads.
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
if (($# == 0)); then
    echo "usage: $(basename "${BASH_SOURCE[0]}") <probe>...   (probes: $(cd "${here}" && echo */ | tr -d /))" >&2
    exit 2
fi
for probe in "$@"; do
    if [[ ! -d "${here}/${probe}/probe" || ! -f "${here}/${probe}/expected.txt" ]]; then
        echo "error: no probe ${probe} (a directory with probe/*.java and expected.txt) in ${here}" >&2
        exit 2
    fi
done
if [[ "$(uname -s)" != Linux ]]; then
    echo "error: the probes run on Linux only ($(uname -s) ignores java.util.prefs.userRoot)" >&2
    exit 1
fi

repo="$(cd "${here}/../.." && pwd)"
main_tree="$(dirname "$(git -C "${repo}" rev-parse --path-format=absolute --git-common-dir)")"
openrocket="$(cd "${OPENROCKET_DIR:-${main_tree}/../openrocket}" && pwd)"
java="${JAVA:-java}"
javac="${JAVAC:-javac}"
services="${openrocket}/core/src/test/java/info/openrocket/core/ServicesForTesting.java"
if [[ ! -f "${services}" ]]; then
    echo "error: ${openrocket} is not an OpenRocket checkout (set OPENROCKET_DIR)" >&2
    exit 1
fi
java_version="$("${java}" -XshowSettings:properties -version 2>&1 |
    sed -n 's/^ *java\.specification\.version = //p')"
if [[ "${java_version}" != 17 ]]; then
    echo "error: ${java} is Java ${java_version:-(unknown)}; the probes were run with Java 17 (set JAVA)" >&2
    exit 1
fi
commit="$(git -C "${openrocket}" rev-parse HEAD)"
recorded="$(sed -n 's/^ *"openrocket": {"commit": "\([0-9a-f]*\)".*/\1/p' \
    "${repo}/tests/data/goldens/manifest.json" 2>/dev/null || true)"
if [[ -n "${recorded}" && "${recorded}" != "${commit}" ]]; then
    echo "warning: the expected transcripts come from OpenRocket ${recorded}, the checkout is at ${commit}" >&2
fi
if ! git -C "${openrocket}" diff --quiet HEAD --; then
    echo "warning: the OpenRocket checkout has modified tracked files" >&2
fi

if [[ -n "${PROBES_WORK:-}" ]]; then
    mkdir -p "${PROBES_WORK}"
    work="$(cd "${PROBES_WORK}" && pwd)"
else
    work="$(mktemp -d)"
    trap 'rc=$?; if ((rc == 0)); then rm -rf "${work}"; else echo "the work files: ${work}" >&2; fi' EXIT
fi

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

status=0
for probe in "$@"; do
    rm -rf "${work}/${probe}"
    mkdir -p "${work}/${probe}/classes" "${work}/${probe}/prefs" "${work}/${probe}/home" "${work}/${probe}/tmp"
    "${javac}" --release 17 -encoding UTF-8 -nowarn -proc:none -cp "${classpath}" -d "${work}/${probe}/classes" \
        "${services}" "${here}/${probe}"/probe/*.java 2> "${work}/${probe}/javac.log" || {
        cat "${work}/${probe}/javac.log" >&2
        exit 1
    }
    for main in "${here}/${probe}"/probe/*Probe.java; do
        class="probe.$(basename "${main}" .java)"
        echo "running ${class} (${probe})" >&2
        env -u DISPLAY -u WAYLAND_DISPLAY "${java}" -Xmx2g -Djava.awt.headless=true \
            -Duser.language=en -Duser.country=US -Duser.timezone=UTC -Dfile.encoding=UTF-8 \
            -Duser.home="${work}/${probe}/home" -Djava.io.tmpdir="${work}/${probe}/tmp" \
            -Djava.util.prefs.userRoot="${work}/${probe}/prefs" -Djava.util.prefs.systemRoot="${work}/${probe}/prefs" \
            -Dlogback.configurationFile="${repo}/tools/openrocket-gui-pins/logback.xml" \
            -cp "${work}/${probe}/classes:${classpath}" "${class}" \
            >> "${work}/${probe}/transcript.txt" 2>> "${work}/${probe}/log.txt" || {
            echo "error: ${class} failed (its log: ${work}/${probe}/log.txt)" >&2
            tail -n 20 "${work}/${probe}/log.txt" >&2
            exit 1
        }
    done
    if cmp -s "${here}/${probe}/expected.txt" "${work}/${probe}/transcript.txt"; then
        echo "same: ${probe}/expected.txt" >&2
    else
        echo "DIFFERENT: ${probe}/expected.txt (the new transcript: ${work}/${probe}/transcript.txt)" >&2
        diff -u "${here}/${probe}/expected.txt" "${work}/${probe}/transcript.txt" | head -n 40 >&2 || true
        status=1
    fi
done
exit "${status}"
