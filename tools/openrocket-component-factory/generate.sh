#!/usr/bin/env bash
# Regenerates tests/data/component-factory/: add-buttons-rocket.ork.xml, the rocket.ork OpenRocket
# saves for a design built with 25 clicks on its add buttons, and (on standard output) the report
# whose blocks tests/core/rocket/ComponentFactoryTests.cpp and
# tests/core/file/openrocket/add_buttons_save_tests.cpp compare with, both written by OpenRocket's own
# code (ComponentFactoryDumper.java). See README.md.
#
# Usage:
#   tools/openrocket-component-factory/generate.sh [<output directory>] > <report file>
#
# The output directory defaults to tests/data/component-factory (the committed files).
#
# Environment:
#   OPENROCKET_DIR        the OpenRocket checkout (default: "openrocket" next to the main working tree of
#                         the QtRocket repository); it should be at the commit of
#                         tests/data/goldens/manifest.json
#   JAVA, JAVAC           the java and javac executables (default: on PATH); they must be Java 17
#   OPENROCKET_CLASSPATH  the runtime class path of OpenRocket's swing module (the compiled classes and
#                         resources of core and swing and the jars of their dependencies). When it is
#                         not set, the script runs, as tools/openrocket-test-documents/generate.sh does
#                         for the core:
#                           1. ./gradlew :swing:classes in the checkout (incremental: the classes match
#                              the checked-out sources)
#                           2. ./gradlew --init-script <tmp> :swing:qtrocketPrintRuntimeClasspath (a
#                              task defined only in a temporary init script; OpenRocket's build files
#                              are not changed)
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo="$(cd "${here}/../.." && pwd)"
# The main working tree of the QtRocket repository (a worktree's common git directory is the main one's .git).
main_tree="$(dirname "$(git -C "${repo}" rev-parse --path-format=absolute --git-common-dir)")"
openrocket="${OPENROCKET_DIR:-${main_tree}/../openrocket}"
openrocket="$(cd "${openrocket}" && pwd)"
java="${JAVA:-java}"
javac="${JAVAC:-javac}"
out="${1:-${repo}/tests/data/component-factory}"

services="${openrocket}/swing/src/test/java/info/openrocket/swing/ServicesForTesting.java"
if [[ ! -f "${services}" ]]; then
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

echo "OpenRocket: ${openrocket} @ $(git -C "${openrocket}" rev-parse HEAD 2>/dev/null || echo unknown)" >&2

# swing's test bootstrap class is compiled from OpenRocket's test sources, not copied.
mkdir -p "${work}/classes" "${work}/prefs" "${out}"
"${javac}" -encoding UTF-8 -nowarn -proc:none -cp "${classpath}" -d "${work}/classes" \
    "${services}" "${here}/ComponentFactoryDumper.java"

# A fixed locale, time zone and encoding; headless AWT (the add buttons are made, never shown); Java
# preferences kept out of the home directory.
"${java}" -Djava.awt.headless=true -Duser.language=en -Duser.country=US -Duser.timezone=UTC \
    -Dfile.encoding=UTF-8 -Dlogback.configurationFile="${here}/logback.xml" \
    -Djava.util.prefs.userRoot="${work}/prefs" -Djava.util.prefs.systemRoot="${work}/prefs" \
    -cp "${work}/classes:${classpath}" ComponentFactoryDumper "${out}"
