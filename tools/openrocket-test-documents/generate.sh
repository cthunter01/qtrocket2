#!/usr/bin/env bash
# Regenerates tests/data/goldens/test-documents/*.ork, OpenRocket's saves of the 16 document makers
# of TestRockets, with OpenRocket's own code (TestDocumentsDumper.java), and prints the report the
# hand-copied tables of tests/core/file/openrocket/OpenRocketSaverTests.cpp come from. See README.md.
#
# Usage:
#   tools/openrocket-test-documents/generate.sh [<output directory>] > <report file>
#
# The output directory defaults to tests/data/goldens/test-documents (the committed files).
#
# Environment:
#   OPENROCKET_DIR        the OpenRocket checkout (default: "openrocket" next to the main working tree of
#                         the QtRocket repository); it should be at the commit of
#                         tests/data/goldens/manifest.json
#   JAVA, JAVAC           the java and javac executables (default: on PATH); they must be Java 17
#   OPENROCKET_CLASSPATH  the runtime class path of OpenRocket's core (its compiled classes and resources
#                         and the jars of its dependencies). When it is not set, the script runs, as
#                         tools/openrocket-goldens/motors/dump-motors.sh does:
#                           1. ./gradlew :core:classes in the checkout (incremental: the classes match
#                              the checked-out sources)
#                           2. ./gradlew --init-script <tmp> :core:qtrocketPrintRuntimeClasspath (a task
#                              defined only in a temporary init script; OpenRocket's build files are
#                              not changed)
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo="$(cd "${here}/../.." && pwd)"
# The main working tree of the QtRocket repository (a worktree's common git directory is the main one's .git).
main_tree="$(dirname "$(git -C "${repo}" rev-parse --path-format=absolute --git-common-dir)")"
openrocket="${OPENROCKET_DIR:-${main_tree}/../openrocket}"
openrocket="$(cd "${openrocket}" && pwd)"
java="${JAVA:-java}"
javac="${JAVAC:-javac}"
out="${1:-${repo}/tests/data/goldens/test-documents}"

if [[ ! -f "${openrocket}/core/src/test/java/info/openrocket/core/ServicesForTesting.java" ]]; then
    echo "error: ${openrocket} is not an OpenRocket checkout (set OPENROCKET_DIR)" >&2
    exit 1
fi

# Double.toString, which writes every number of the files, changed in JDK 19 (JDK-4511638).
java_version="$("${java}" -XshowSettings:properties -version 2>&1 |
    sed -n 's/^ *java\.specification\.version = //p')"
if [[ "${java_version}" != 17 ]]; then
    echo "error: ${java} is Java ${java_version:-(unknown)}; the files are written with Java 17 (set JAVA)" >&2
    exit 1
fi

work="$(mktemp -d)"
trap 'rm -rf "${work}"' EXIT

classpath="${OPENROCKET_CLASSPATH:-}"
if [[ -z "${classpath}" ]]; then
    (cd "${openrocket}" && ./gradlew -q :core:classes)
    cat > "${work}/classpath.init.gradle" <<'GRADLE'
allprojects {
    afterEvaluate { project ->
        if (project.path == ':core') {
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
        :core:qtrocketPrintRuntimeClasspath | sed -n 's/^QTROCKET_CP=//p')"
    if [[ -z "${classpath}" ]]; then
        echo "error: could not get OpenRocket's core runtime classpath from Gradle" >&2
        exit 1
    fi
fi

echo "OpenRocket: ${openrocket} @ $(git -C "${openrocket}" rev-parse HEAD 2>/dev/null || echo unknown)" >&2

# OpenRocketSaverTest's bootstrap class is compiled from OpenRocket's test sources, not copied.
mkdir -p "${work}/classes" "${work}/prefs" "${out}"
"${javac}" -encoding UTF-8 -nowarn -proc:none -cp "${classpath}" -d "${work}/classes" \
    "${openrocket}/core/src/test/java/info/openrocket/core/ServicesForTesting.java" \
    "${here}/TestDocumentsDumper.java"

# A fixed locale, time zone and encoding; headless AWT; Java preferences kept out of the home directory.
"${java}" -Djava.awt.headless=true -Duser.language=en -Duser.country=US -Duser.timezone=UTC \
    -Dfile.encoding=UTF-8 -Dlogback.configurationFile="${here}/logback.xml" \
    -Djava.util.prefs.userRoot="${work}/prefs" -Djava.util.prefs.systemRoot="${work}/prefs" \
    -cp "${work}/classes:${classpath}" TestDocumentsDumper "${openrocket}" "${out}"
