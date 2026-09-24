#!/usr/bin/env bash
# Regenerates tests/data/goldens/motors.json from OpenRocket's own code (MotorsDumper.java): every
# thrust curve OpenRocket loads from data/motors/initial_motors.db and from tests/data/motors/*,
# the database curves it skips as invalid, what it makes of the malformed and edge-case files and
# the database in tests/data/motors-edge/ (make-edge-corpus.py writes them), and the MD5 of its
# RockSimMotorWriter output for every bundled motor. The motors_golden_tests compare QtRocket
# against it.
#
# Usage:
#   tools/openrocket-goldens/motors/dump-motors.sh [OPENROCKET_DIR]
#
# OPENROCKET_DIR is an OpenRocket checkout (default: $OPENROCKET_DIR, else ../openrocket next to
# this repository). Needs JDK 17 (javac and java on PATH) and network access the first time Gradle
# resolves OpenRocket's dependencies. What it runs, in order:
#   1. ./gradlew :core:classes                       (incremental: builds the checked-out sources, so the
#                                                     classes always match the commit recorded below)
#   2. ./gradlew -q --init-script <tmp> :core:qtrocketPrintRuntimeClasspath
#      (a task defined only in the temporary init script below, which prints the core's runtime
#      classpath: its compiled classes and resources plus the dependency jars in ~/.gradle/caches;
#      OpenRocket's build files are not changed)
#   3. javac -cp <classpath> -d <tmp> MotorsDumper.java
#   4. java -cp <tmp>:<classpath> MotorsDumper data/motors/initial_motors.db tests/data/motors \
#          tests/data/motors-edge tests/data/goldens/motors.json <OpenRocket commit>
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo="$(cd "${here}/../../.." && pwd)"
openrocket="${1:-${OPENROCKET_DIR:-${repo}/../openrocket}}"
openrocket="$(cd "${openrocket}" && pwd)"

if [[ ! -x "${openrocket}/gradlew" ]]; then
    echo "error: ${openrocket} is not an OpenRocket checkout (no gradlew)" >&2
    exit 1
fi

work="$(mktemp -d)"
trap 'rm -rf "${work}"' EXIT

# 1. OpenRocket's compiled core, always rebuilt from the checkout (Gradle does nothing when the
#    classes are up to date), so that the golden and its recorded commit agree
(cd "${openrocket}" && ./gradlew -q :core:classes)

# 2. Its runtime classpath
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

# 3. and 4. Compile and run the dumper
mkdir -p "${work}/classes"
javac -cp "${classpath}" -d "${work}/classes" "${here}/MotorsDumper.java"
version="$(git -C "${openrocket}" rev-parse --short HEAD 2>/dev/null || echo unknown)"
mkdir -p "${repo}/tests/data/goldens"
java -cp "${work}/classes:${classpath}" MotorsDumper \
    "${repo}/data/motors/initial_motors.db" \
    "${repo}/tests/data/motors" \
    "${repo}/tests/data/motors-edge" \
    "${repo}/tests/data/goldens/motors.json" \
    "${version}"
