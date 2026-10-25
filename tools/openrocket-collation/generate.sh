#!/usr/bin/env bash
# Regenerates the generated files of QtRocket's util/JavaCollator and util/AlphanumComparator (see README.md):
#   tables     src/util/JavaCollationTables.h             JavaCollationTables.java, from the JDK alone
#   collator   tests/core/util/JavaCollatorPins.h         JavaCollatorPins.java, from the JDK alone
#   alphanum   tests/core/util/AlphanumComparatorPins.h   AlphanumComparatorPins.java, from OpenRocket's
#                                                         AlphanumComparator
#
# Usage:
#   tools/openrocket-collation/generate.sh [--check] [<part>...]   (default: all three)
#
# Every file is written into a work directory first, formatted with clang-format (the repository's
# .clang-format), and compared with the committed one byte for byte: an equal file leaves the committed one as
# it is, another one replaces it. The programs print the same on every run. --check compares only: the exit
# status is 1 when a file differs.
#
# Environment:
#   JAVA, JAVAC           the java and javac executables (default: on PATH); they must be Java 17
#   CLANG_FORMAT          the clang-format executable (default: on PATH)
#   THOROUGH=1            (tables) also check every triple of mapped chars for context (three more minutes)
#   OPENROCKET_DIR        (alphanum) the OpenRocket checkout (default: "openrocket" next to the main working
#                         tree of the QtRocket repository), at the commit of tests/data/goldens/manifest.json.
#                         The script compiles the checkout's AlphanumComparator.java itself (it needs nothing
#                         but the JDK), so no build of OpenRocket is needed.
#   ALLOW_OTHER_COMMIT=1  (alphanum) accept a checkout at another commit than the manifest's
set -euo pipefail

all_parts=(tables collator alphanum)

check=false
parts=()
for argument in "$@"; do
    if [[ "${argument}" == --check ]]; then
        check=true
    elif [[ " ${all_parts[*]} " == *" ${argument} "* ]]; then
        parts+=("${argument}")
    else
        echo "usage: $(basename "${BASH_SOURCE[0]}") [--check] [<part>...]   (parts: ${all_parts[*]})" >&2
        exit 2
    fi
done
if ((${#parts[@]} == 0)); then
    parts=("${all_parts[@]}")
fi

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo="$(cd "${here}/../.." && pwd)"
java="${JAVA:-java}"
javac="${JAVAC:-javac}"
clang_format="${CLANG_FORMAT:-clang-format}"

# The collation data is JDK 17's, and OpenRocket's releases bundle Java 17.
java_version="$("${java}" -XshowSettings:properties -version 2>&1 |
    sed -n 's/^ *java\.specification\.version = //p')"
if [[ "${java_version}" != 17 ]]; then
    echo "error: ${java} is Java ${java_version:-(unknown)}; the files are written with Java 17 (set JAVA)" >&2
    exit 1
fi

work="$(mktemp -d)"
trap 'rm -rf "${work}"' EXIT
mkdir -p "${work}/classes" "${work}/out"

# Runs a program in en_US (Collator.getInstance() follows the default locale), its output into ${work}/<class>.h.
#   run <class path> <class> [<argument>...]
run() {
    local classpath="$1" class="$2"
    shift 2
    env -u DISPLAY -u WAYLAND_DISPLAY "${java}" -Djava.awt.headless=true -Duser.language=en -Duser.country=US \
        -Duser.variant= -Duser.timezone=UTC -Dfile.encoding=UTF-8 -cp "${classpath}" "${class}" "$@" \
        > "${work}/${class}.h"
}

# Formats ${work}/<class>.h as the repository formats <repository path>, compares it with the committed file and
# installs it (unless --check) when they differ.
#   finish <class> <repository path>
finish() {
    local class="$1" file="$2"
    local path="${work}/out/$(basename "${file}")"
    "${clang_format}" --assume-filename="${repo}/${file}" < "${work}/${class}.h" > "${path}"
    if [[ -f "${repo}/${file}" ]] && cmp -s "${repo}/${file}" "${path}"; then
        echo "same: ${file}" >&2
    elif [[ "${check}" == true ]]; then
        echo "DIFFERENT: ${file}" >&2
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
for part in "${parts[@]}"; do
    case "${part}" in
    tables)
        "${javac}" --release 17 -encoding UTF-8 -nowarn -d "${work}/classes" "${here}/JavaCollationTables.java"
        if [[ "${THOROUGH:-0}" == 1 ]]; then
            run "${work}/classes" JavaCollationTables thorough
        else
            run "${work}/classes" JavaCollationTables
        fi
        finish JavaCollationTables src/util/JavaCollationTables.h
        ;;
    collator)
        "${javac}" --release 17 -encoding UTF-8 -nowarn -d "${work}/classes" "${here}/JavaCollatorPins.java"
        run "${work}/classes" JavaCollatorPins
        finish JavaCollatorPins tests/core/util/JavaCollatorPins.h
        ;;
    alphanum)
        # The main working tree of the QtRocket repository (a worktree's common git directory is the main one's
        # .git).
        main_tree="$(dirname "$(git -C "${repo}" rev-parse --path-format=absolute --git-common-dir)")"
        openrocket="${OPENROCKET_DIR:-${main_tree}/../openrocket}"
        openrocket="$(cd "${openrocket}" && pwd)"
        source="core/src/main/java/info/openrocket/core/util/AlphanumComparator.java"
        if [[ ! -f "${openrocket}/${source}" ]]; then
            echo "error: ${openrocket} is not an OpenRocket checkout (set OPENROCKET_DIR)" >&2
            exit 1
        fi
        commit="$(git -C "${openrocket}" rev-parse HEAD)"
        recorded="$(sed -n 's/^ *"openrocket": {"commit": "\([0-9a-f]*\)".*/\1/p' \
            "${repo}/tests/data/goldens/manifest.json" 2>/dev/null || true)"
        if [[ -n "${recorded}" && "${recorded}" != "${commit}" && "${ALLOW_OTHER_COMMIT:-0}" != 1 ]]; then
            echo "error: OpenRocket is at ${commit}; the goldens come from ${recorded} (ALLOW_OTHER_COMMIT=1" \
                "overrides)" >&2
            exit 1
        fi
        if ! git -C "${openrocket}" diff --quiet HEAD -- "${source}"; then
            echo "error: ${openrocket}/${source} is modified" >&2
            exit 1
        fi
        echo "OpenRocket: ${openrocket} @ ${commit}" >&2
        mkdir -p "${work}/openrocket"
        "${javac}" --release 17 -encoding UTF-8 -nowarn -proc:none -d "${work}/openrocket" "${openrocket}/${source}"
        "${javac}" --release 17 -encoding UTF-8 -nowarn -proc:none -cp "${work}/openrocket" -d "${work}/classes" \
            "${here}/AlphanumComparatorPins.java"
        run "${work}/classes:${work}/openrocket" AlphanumComparatorPins "${commit}"
        finish AlphanumComparatorPins tests/core/util/AlphanumComparatorPins.h
        ;;
    esac
done
exit "${status}"
