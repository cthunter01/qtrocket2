#!/usr/bin/env bash
# Regenerates the two generated files of QtRocket's util/ExpressionParser (see README.md):
#   src/util/ExpressionParserTables.h              JavaCharacterTables.java, from the JDK alone
#   tests/core/util/ExpressionParserParityCases.h  ExpressionParserDumper.java, from OpenRocket's
#                                                  ExpressionParser on parity-inputs.txt
#
# Usage:
#   tools/openrocket-expression-parser/generate.sh [tables|parity|all]   (default: all)
#
# Both files are written in place, formatted with clang-format (the repository's .clang-format).
#
# Environment:
#   JAVA, JAVAC           the java and javac executables (default: on PATH); they must be Java 17
#   CLANG_FORMAT          the clang-format executable (default: on PATH)
#   OPENROCKET_DIR        (parity only) the OpenRocket checkout (default: "openrocket" next to the main
#                         working tree of the QtRocket repository); it should be at the commit of
#                         tests/data/goldens/manifest.json
#   OPENROCKET_CLASSPATH  (parity only) the runtime class path of OpenRocket's core (its compiled classes
#                         and resources and the jars of its dependencies). When it is not set, the script
#                         runs, as tools/openrocket-test-documents/generate.sh does:
#                           1. ./gradlew :core:classes in the checkout (incremental: the classes match
#                              the checked-out sources)
#                           2. ./gradlew --init-script <tmp> :core:qtrocketPrintRuntimeClasspath (a task
#                              defined only in a temporary init script; OpenRocket's build files are
#                              not changed)
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo="$(cd "${here}/../.." && pwd)"
java="${JAVA:-java}"
javac="${JAVAC:-javac}"
clang_format="${CLANG_FORMAT:-clang-format}"
what="${1:-all}"
case "${what}" in
    tables | parity | all) ;;
    *)
        echo "usage: $0 [tables|parity|all]" >&2
        exit 1
        ;;
esac

# The character data is JDK 17's (Unicode 13.0), and OpenRocket's releases bundle Java 17.
java_version="$("${java}" -XshowSettings:properties -version 2>&1 |
    sed -n 's/^ *java\.specification\.version = //p')"
if [[ "${java_version}" != 17 ]]; then
    echo "error: ${java} is Java ${java_version:-(unknown)}; the files are written with Java 17 (set JAVA)" >&2
    exit 1
fi

work="$(mktemp -d)"
trap 'rm -rf "${work}"' EXIT
mkdir -p "${work}/classes"

# Writes standard input to $1 (a path in the repository), formatted as the repository formats it.
write_formatted() {
    local target="$1"
    "${clang_format}" --assume-filename="${target}" > "${work}/formatted"
    mv "${work}/formatted" "${target}"
    echo "wrote ${target}" >&2
}

if [[ "${what}" != parity ]]; then
    "${javac}" -encoding UTF-8 -nowarn -d "${work}/classes" "${here}/JavaCharacterTables.java"
    "${java}" -Dfile.encoding=UTF-8 -cp "${work}/classes" JavaCharacterTables > "${work}/tables.h"
    write_formatted "${repo}/src/util/ExpressionParserTables.h" < "${work}/tables.h"
fi

if [[ "${what}" != tables ]]; then
    # The main working tree of the QtRocket repository (a worktree's common git directory is the main one's .git).
    main_tree="$(dirname "$(git -C "${repo}" rev-parse --path-format=absolute --git-common-dir)")"
    openrocket="${OPENROCKET_DIR:-${main_tree}/../openrocket}"
    openrocket="$(cd "${openrocket}" && pwd)"
    if [[ ! -f "${openrocket}/core/src/main/java/info/openrocket/core/util/ExpressionParser.java" ]]; then
        echo "error: ${openrocket} is not an OpenRocket checkout (set OPENROCKET_DIR)" >&2
        exit 1
    fi

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

    "${javac}" -encoding UTF-8 -nowarn -proc:none -cp "${classpath}" -d "${work}/classes" \
        "${here}/ExpressionParserDumper.java"
    # en_US: the parser takes its decimal separator and minus sign from the default locale.
    "${java}" -Djava.awt.headless=true -Duser.language=en -Duser.country=US -Duser.timezone=UTC \
        -Dfile.encoding=UTF-8 -Dlogback.configurationFile="${here}/logback.xml" \
        -cp "${work}/classes:${classpath}" ExpressionParserDumper "${here}/parity-inputs.txt" > "${work}/parity.h"
    write_formatted "${repo}/tests/core/util/ExpressionParserParityCases.h" < "${work}/parity.h"
fi
