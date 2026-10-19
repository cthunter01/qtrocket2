#!/usr/bin/env bash
# Regenerates tests/data/goldens/gui/from-scratch/rocket.ork and state.json: OpenRocket's own GUI
# code runs the from-scratch scenario (scenario.json in that directory), headless. See README.md.
#
#   tools/openrocket-gui-reference/generate.sh
#
# Environment:
#   OPENROCKET_DIR   the OpenRocket checkout (default: "openrocket" next to the main working tree of
#                    the QtRocket repository); it should be at the commit state.json records
#   JAVA             the java executable (default: java on PATH); it must be Java 17
#   ALLOW_DIRTY=1    accept an OpenRocket checkout with modified, untracked or ignored source files
#                    (state.json records "dirty": true)
#   GUI_REFERENCE_OUT  write into this directory instead of tests/data/goldens/gui/from-scratch (the
#                    scenario is still read from there)
#   GUI_REFERENCE_WORK keep the work files (the saved archives, the second run's output) in this
#                    directory instead of a temporary one that is deleted at the end
#
# The tool runs twice, into the output directory and into a temporary one, and the script fails
# unless both runs wrote the same files: the reference is deterministic.
#
# The checkout is only read: OpenRocket's core and Swing modules are compiled into
# tools/openrocket-gui-reference/build.
set -euo pipefail

if (($# > 0)); then
    echo "usage: $(basename "${BASH_SOURCE[0]}")   (see the head of the script for the environment)" >&2
    exit 2
fi

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo="$(cd "$here/../.." && pwd)"
# The main working tree of the QtRocket repository (a worktree's common git directory is the main one's .git).
main_tree="$(dirname "$(git -C "$repo" rev-parse --path-format=absolute --git-common-dir)")"
openrocket="${OPENROCKET_DIR:-$main_tree/../openrocket}"
openrocket="$(cd "$openrocket" && pwd)"
java="${JAVA:-java}"

if [[ ! -f "$openrocket/swing/src/main/java/info/openrocket/swing/gui/main/BasicFrame.java" ]]; then
    echo "error: $openrocket is not an OpenRocket checkout (set OPENROCKET_DIR)" >&2
    exit 1
fi

# The reference is written with Java 17, as the goldens are: Double.toString, which writes every
# number of the saved file, changed in JDK 19 (JDK-4511638).
java_version="$("$java" -XshowSettings:properties -version 2>&1 |
    sed -n 's/^ *java\.specification\.version = //p')"
if [[ "$java_version" != 17 ]]; then
    echo "error: $java is Java ${java_version:-(unknown)}; the reference is written with Java 17 (set JAVA)" >&2
    exit 1
fi

# The checkout must be exactly its commit among what the build compiles or puts on the class path: no
# modified tracked file, no untracked or ignored file there (build.gradle keeps the two ignored copies
# that OpenRocket's own build leaves in core/src/main/resources off the class path, as
# tools/openrocket-goldens does).
sources=(core/src/main/java core/src/main/resources core/build.gradle core/libs
    swing/src/main/java swing/src/main/resources swing/build.gradle)
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
dirty=false
if ((${#problems[@]} > 0)); then
    for problem in "${problems[@]}"; do
        echo "OpenRocket checkout: $problem" >&2
    done
    if [[ "${ALLOW_DIRTY:-0}" != 1 ]]; then
        echo "error: the reference would not match the checkout's commit (ALLOW_DIRTY=1 overrides)" >&2
        exit 1
    fi
    dirty=true
fi
commit="$(git -C "$openrocket" rev-parse HEAD)"
echo "OpenRocket: $openrocket @ $commit" >&2

committed="$repo/tests/data/goldens/gui/from-scratch"
scenario="$committed/scenario.json"
out="${GUI_REFERENCE_OUT:-$committed}"
recorded="$(sed -n 's/^ *"commit": "\([0-9a-f]*\)".*/\1/p' "$committed/state.json" 2>/dev/null | head -n 1 || true)"
if [[ -n "$recorded" && "$recorded" != "$commit" ]]; then
    echo "warning: the committed reference comes from OpenRocket $recorded, the checkout is at $commit" >&2
fi

# Build with OpenRocket's own Gradle wrapper (no wrapper is vendored here). The project directory is
# this one, so nothing is written into the checkout.
"$openrocket/gradlew" --quiet --console=plain -p "$here" -PopenrocketDir="$openrocket" guiReferenceClasspath

if [[ -n "${GUI_REFERENCE_WORK:-}" ]]; then
    work="$GUI_REFERENCE_WORK"
    rm -rf "$work"
    mkdir -p "$work"
else
    work="$(mktemp -d)"
    trap 'rm -rf "$work"' EXIT
fi
mkdir -p "$out"
out="$(cd "$out" && pwd)"

# One run of the tool into $1, with a fresh java.util.prefs store (OpenRocket's preferences at their
# defaults), a fixed locale, time zone and encoding, headless AWT and no display. The userRoot and
# systemRoot properties isolate the store on Linux only (macOS and Windows ignore them), so
# openrocket.debug.prefs also points OpenRocket at its node "OpenRocket-debug", which SwingPreferences
# clears when it starts and from which TranslatorProvider reads no locale: the user's own OpenRocket
# preferences are neither read nor written, on every system.
run_tool() {
    local target="$1" name="$2"
    mkdir -p "$work/$name/prefs" "$target"
    env -u DISPLAY -u WAYLAND_DISPLAY "$java" -Xmx2g -Djava.awt.headless=true \
        -Duser.language=en -Duser.country=US -Duser.timezone=UTC -Dfile.encoding=UTF-8 \
        -Djava.util.prefs.userRoot="$work/$name/prefs" -Djava.util.prefs.systemRoot="$work/$name/prefs" \
        -Dopenrocket.debug.prefs=true \
        -cp "$(cat "$here/build/gui-reference-classpath.txt")" info.qtrocket.guireference.GuiReference \
        --scenario "$scenario" --out "$target" --work "$work/$name/saves" \
        --commit "$commit" --dirty "$dirty"
}

run_tool "$out" first
run_tool "$work/second/out" second
for file in rocket.ork state.json; do
    if ! cmp -s "$out/$file" "$work/second/out/$file"; then
        echo "error: two runs of the tool wrote different $file" >&2
        diff "$out/$file" "$work/second/out/$file" | head -n 20 >&2 || true
        exit 1
    fi
done
echo "Two runs wrote the same rocket.ork and state.json into $out" >&2
