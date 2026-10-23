#!/usr/bin/env bash
# Section 7's interop check: OpenRocket reads the design files QtRocket saved. See README.md, "Interop check".
#
#   tools/openrocket-goldens/interop.sh [<saves directory>]
#
# The saves are written first by DISABLED_ tests of the C++ tests (tests/core/goldens/ork_interop_saves_tests.cpp,
# then the GUI's save of its from-scratch design, tests/gui/exit/from_scratch_exit_tests.cpp):
#
#   build/clang-debug/bin/QtRocket_core_tests --gtest_also_run_disabled_tests --gtest_filter='OrkInteropSaves.*'
#   build/clang-debug/bin/QtRocket_gui_tests --gtest_also_run_disabled_tests --gtest_filter='FromScratchInteropSave.*'
#
# which write them into build/clang-debug/interop-saves, the default directory of this script.
#
# Environment:
#   OPENROCKET_DIR   the OpenRocket checkout (default: "openrocket" next to the QtRocket repository)
#   JAVA             the java executable (default: java on PATH); it must be Java 17
#   ALLOW_DIRTY=1    accept an OpenRocket checkout with modified, untracked or ignored source files, or a
#                    preset submodule that is not at its recorded commit (the comparisons with the goldens
#                    can then fail for that reason alone)
#
# The checkout is only read: OpenRocket's core (and the shape providers of the Swing module's 2D rocket
# figure) is compiled into tools/openrocket-goldens/build, as generate.sh does. The exit status is InteropCheck's: 0 when every file passes, 1 when one fails (the report on standard
# output says why), 2 on a usage error.
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo="$(cd "$here/../.." && pwd)"
if (($# > 1)) || [[ "${1:-}" == -* ]]; then
    echo "usage: $(basename "${BASH_SOURCE[0]}") [<saves directory>]" >&2
    exit 2
fi
saves="${1:-$repo/build/clang-debug/interop-saves}"
if [[ ! -d "$saves" ]]; then
    echo "error: $saves is no directory; write the saves first (see the head of this script)" >&2
    exit 2
fi
saves="$(cd "$saves" && pwd)"

# The main working tree of the QtRocket repository (a worktree's common git directory is the main one's .git).
main_tree="$(dirname "$(git -C "$repo" rev-parse --path-format=absolute --git-common-dir)")"
openrocket="${OPENROCKET_DIR:-$main_tree/../openrocket}"
openrocket="$(cd "$openrocket" && pwd)"
java="${JAVA:-java}"

if [[ ! -f "$openrocket/core/src/main/java/info/openrocket/core/startup/Application.java" ]]; then
    echo "error: $openrocket is not an OpenRocket checkout (set OPENROCKET_DIR)" >&2
    exit 1
fi

# Java 17, as for the goldens: the dumps the check compares with them are written with Double.toString, which
# changed in JDK 19.
java_version="$("$java" -XshowSettings:properties -version 2>&1 |
    sed -n 's/^ *java\.specification\.version = //p')"
if [[ "$java_version" != 17 ]]; then
    echo "error: $java is Java ${java_version:-(unknown)}; the check runs with Java 17 (set JAVA)" >&2
    exit 1
fi

# The checkout must be the commit the goldens were made from, without local changes (generate.sh's checks).
submodule_path=core/resources-src/datafiles/openrocket-database
sources=(core/src/main/java core/src/main/resources core/build.gradle core/libs
    core/src/test/java/info/openrocket/core/ServicesForTesting.java
    swing/src/main/java/info/openrocket/swing/gui/rocketfigure
    swing/src/main/resources/META-INF/services/info.openrocket.swing.gui.rocketfigure.RocketComponentShapeService)
ignored_copies='^!! core/src/main/resources/(ReleaseNotes\.md|datafiles/components/database/)'
problems=()
if ! git -C "$openrocket" diff --quiet HEAD --; then
    problems+=("modified tracked files")
fi
unexpected="$(git -C "$openrocket" status --porcelain --ignored --untracked-files=all -- "${sources[@]}" |
    grep -Ev "$ignored_copies" || true)"
if [[ -n "$unexpected" ]]; then
    problems+=("untracked or ignored files among the sources:"$'\n'"$unexpected")
fi
submodule_state="$(git -C "$openrocket" submodule status -- "$submodule_path")"
if [[ "${submodule_state:0:1}" != " " ]]; then
    problems+=("the preset submodule is not initialized at its recorded commit (git submodule update --init)")
elif [[ -n "$(git -C "$openrocket/$submodule_path" status --porcelain --ignored --untracked-files=all)" ]]; then
    problems+=("the preset submodule has local changes")
fi
if ((${#problems[@]} > 0)); then
    for problem in "${problems[@]}"; do
        echo "OpenRocket checkout: $problem" >&2
    done
    if [[ "${ALLOW_DIRTY:-0}" != 1 ]]; then
        echo "error: the checkout is not the one of the goldens (ALLOW_DIRTY=1 overrides)" >&2
        exit 1
    fi
fi
goldens="$repo/tests/data/goldens"
commit="$(git -C "$openrocket" rev-parse HEAD)"
recorded="$(sed -n 's/^ *"openrocket": {"commit": "\([0-9a-f]*\)".*/\1/p' "$goldens/manifest.json")"
if [[ "$recorded" != "$commit" ]]; then
    echo "warning: the goldens come from OpenRocket $recorded, the checkout is at $commit;" >&2
    echo "         the comparisons with the goldens can fail for that reason alone" >&2
fi
echo "OpenRocket: $openrocket @ $commit" >&2

"$openrocket/gradlew" --quiet --console=plain -p "$here" -PopenrocketDir="$openrocket" goldensClasspath

work="$here/build/work"
mkdir -p "$work"

# A fixed locale, time zone and encoding; headless AWT (OpenRocket's core touches java.awt.geom).
status=0
"$java" -Djava.awt.headless=true -Duser.language=en -Duser.country=US -Duser.timezone=UTC \
    -Dfile.encoding=UTF-8 \
    -cp "$(cat "$here/build/goldens-classpath.txt")" info.qtrocket.goldens.InteropCheck \
    --openrocket "$openrocket" --goldens "$goldens" --examples "$repo/data/examples" \
    --legacy "$repo/tests/data/ork" --saves "$saves" --work "$work" || status=$?
exit "$status"
