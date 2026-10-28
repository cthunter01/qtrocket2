#!/usr/bin/env bash
# Regenerates the GUI references of tests/data/goldens/gui/<scenario>/ (rocket.ork and state.json): OpenRocket's
# own GUI code runs the scenario of that directory (scenario.json). A scenario of version 1 (from-scratch, the
# exit check of Milestone 2) runs headless; one of version 2 (Milestone 3, the Motors & Configuration tab) runs
# OpenRocket's real panels and dialogs on a virtual display of xvfb-run. See README.md.
#
#   tools/openrocket-gui-reference/generate.sh [--check] [<scenario>...]
#
# The scenarios are the directories of tests/data/goldens/gui that hold a scenario.json (all of them when none is
# named). --check writes nothing into them: it runs each scenario as usual into the work directory and compares
# the files with the committed ones, and its exit status is 1 when one differs (or is missing).
#
# Environment:
#   OPENROCKET_DIR   the OpenRocket checkout (default: "openrocket" next to the main working tree of the QtRocket
#                    repository); it should be at the commit state.json records, with its preset submodule
#                    (core/resources-src/datafiles/openrocket-database) initialized at the commit it records
#                    (needed for the scenarios of version 2, whose start-up loads the bundled presets)
#   JAVA             the java executable (default: java on PATH); it must be Java 17
#   ALLOW_DIRTY=1    accept an OpenRocket checkout with modified, untracked or ignored source files, or a preset
#                    submodule at another commit or with local changes (state.json records "dirty": true)
#   GUI_REFERENCE_SCENARIOS  the directory of the scenario directories (default tests/data/goldens/gui): to try a
#                    scenario before it is committed
#   GUI_REFERENCE_OUT  write <this directory>/<scenario>/ instead of the scenario's directory (the scenario is
#                    still read from there)
#   GUI_REFERENCE_WORK keep the work files (per scenario the saved archives, the second run's output, the
#                    preferences and home directories, the logs) in this directory instead of a temporary one that
#                    is deleted at the end
#
# Every scenario runs twice, into its output directory and into a temporary one, and the script fails unless both
# runs wrote the same files: the reference is deterministic.
#
# The checkout is only read: OpenRocket's core and Swing modules are compiled into tools/openrocket-gui-reference/build.
set -euo pipefail

check=false
names=()
for argument in "$@"; do
    case "$argument" in
    --check) check=true ;;
    -*)
        echo "usage: $(basename "${BASH_SOURCE[0]}") [--check] [<scenario>...]   (see the head of the script)" >&2
        exit 2
        ;;
    *) names+=("$argument") ;;
    esac
done

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo="$(cd "$here/../.." && pwd)"
# The main working tree of the QtRocket repository (a worktree's common git directory is the main one's .git).
main_tree="$(dirname "$(git -C "$repo" rev-parse --path-format=absolute --git-common-dir)")"
openrocket="${OPENROCKET_DIR:-$main_tree/../openrocket}"
openrocket="$(cd "$openrocket" && pwd)"
java="${JAVA:-java}"
scenarios_dir="${GUI_REFERENCE_SCENARIOS:-$repo/tests/data/goldens/gui}"
scenarios_dir="$(cd "$scenarios_dir" && pwd)"

if [[ ! -f "$openrocket/swing/src/main/java/info/openrocket/swing/gui/main/BasicFrame.java" ]]; then
    echo "error: $openrocket is not an OpenRocket checkout (set OPENROCKET_DIR)" >&2
    exit 1
fi

# The scenarios and their schema versions.
if ((${#names[@]} == 0)); then
    for scenario in "$scenarios_dir"/*/scenario.json; do
        [[ -f "$scenario" ]] && names+=("$(basename "$(dirname "$scenario")")")
    done
fi
if ((${#names[@]} == 0)); then
    echo "error: no scenario in $scenarios_dir" >&2
    exit 1
fi
declare -A versions=()
any_v2=false
for name in "${names[@]}"; do
    scenario="$scenarios_dir/$name/scenario.json"
    if [[ ! -f "$scenario" ]]; then
        echo "error: no scenario $scenario" >&2
        exit 2
    fi
    version="$(sed -n 's/^ *"schemaVersion": *\([0-9][0-9]*\).*/\1/p' "$scenario" | head -n 1)"
    case "$version" in
    1) ;;
    2) any_v2=true ;;
    *)
        echo "error: $scenario has no schemaVersion 1 or 2" >&2
        exit 1
        ;;
    esac
    versions["$name"]="$version"
done
if [[ "$any_v2" == true ]] && ! command -v xvfb-run > /dev/null; then
    echo "error: xvfb-run is not on PATH (the scenarios of version 2 run OpenRocket's panels on a virtual display)" >&2
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
# tools/openrocket-goldens does); for a scenario of version 2 also the preset submodule, whose orc/ files
# build.gradle copies onto the class path, initialized at the commit the checkout records and unmodified.
sources=(core/src/main/java core/src/main/resources core/build.gradle core/libs
    swing/src/main/java swing/src/main/resources swing/build.gradle)
ignored_copies='^!! core/src/main/resources/(ReleaseNotes\.md|datafiles/components/database/)'
submodule_path=core/resources-src/datafiles/openrocket-database
problems=()
if ! git -C "$openrocket" diff --quiet HEAD --; then
    problems+=("modified tracked files")
fi
unexpected="$(git -C "$openrocket" status --porcelain --ignored --untracked-files=all -- "${sources[@]}" |
    grep -Ev "$ignored_copies" || true)"
if [[ -n "$unexpected" ]]; then
    problems+=("untracked or ignored files among the sources:"$'\n'"$unexpected")
fi
if [[ "$any_v2" == true ]]; then
    submodule_state="$(git -C "$openrocket" submodule status -- "$submodule_path")"
    if [[ "${submodule_state:0:1}" != " " ]]; then
        problems+=("the preset submodule is not initialized at its recorded commit (git submodule update --init)")
    elif [[ -n "$(git -C "$openrocket/$submodule_path" status --porcelain --ignored --untracked-files=all)" ]]; then
        problems+=("the preset submodule has local changes")
    fi
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
# The commit the superproject records for the submodule (what a checkout of $commit gives).
preset_commit="$(git -C "$openrocket" rev-parse "HEAD:$submodule_path")"
echo "OpenRocket: $openrocket @ $commit (presets @ $preset_commit)" >&2

# Build with OpenRocket's own Gradle wrapper (no wrapper is vendored here). The project directory is
# this one, so nothing is written into the checkout.
"$openrocket/gradlew" --quiet --console=plain -p "$here" -PopenrocketDir="$openrocket" guiReferenceClasspath

if [[ -n "${GUI_REFERENCE_WORK:-}" ]]; then
    work="$GUI_REFERENCE_WORK"
    rm -rf "$work"
    mkdir -p "$work"
    work="$(cd "$work" && pwd)"
else
    work="$(mktemp -d)"
    trap 'rm -rf "$work"' EXIT
fi
classpath="$(cat "$here/build/gui-reference-classpath.txt")"

# One run of the tool for the scenario $1 into $2, its work files in $work/$1/$3: a fresh java.util.prefs store
# (OpenRocket's preferences at their defaults), a fixed locale, time zone and encoding. The userRoot and
# systemRoot properties isolate the store on Linux only (macOS and Windows ignore them), so
# openrocket.debug.prefs also points OpenRocket at its node "OpenRocket-debug", which SwingPreferences clears
# when it starts and from which TranslatorProvider reads no locale: the user's own OpenRocket preferences are
# neither read nor written, on every system.
#   version 1: headless AWT and no display, as in Milestone 2;
#   version 2: a virtual display of its own (xvfb-run; the display of the session is unset, so nothing is shown
#              on the desktop), a fresh home directory (OpenRocket's motor library, thrust curve and preset
#              directories: the bundled motors and presets alone) and temporary directory.
run_tool() {
    local name="$1" target="$2" run="$3"
    local scenario="$scenarios_dir/$name/scenario.json" base="$work/$name/$run"
    rm -rf "$base"
    mkdir -p "$base/prefs" "$target"
    if [[ "${versions[$name]}" == 1 ]]; then
        env -u DISPLAY -u WAYLAND_DISPLAY "$java" -Xmx2g -Djava.awt.headless=true \
            -Duser.language=en -Duser.country=US -Duser.timezone=UTC -Dfile.encoding=UTF-8 \
            -Djava.util.prefs.userRoot="$base/prefs" -Djava.util.prefs.systemRoot="$base/prefs" \
            -Dopenrocket.debug.prefs=true \
            -cp "$classpath" info.qtrocket.guireference.GuiReference \
            --scenario "$scenario" --out "$target" --work "$base/saves" \
            --commit "$commit" --dirty "$dirty"
        return
    fi
    mkdir -p "$base/home" "$base/tmp"
    # xvfb-run sends the command's standard error to its standard output: the tool's goes to its log.
    if ! env -u DISPLAY -u WAYLAND_DISPLAY xvfb-run -a -s "-screen 0 1280x1024x24" \
        bash -c 'exec "$@" 2> "$0"' "$base/log.txt" \
        "$java" -Xmx2g -Djava.awt.headless=false \
        -Duser.language=en -Duser.country=US -Duser.timezone=UTC -Dfile.encoding=UTF-8 \
        -Duser.home="$base/home" -Djava.io.tmpdir="$base/tmp" -Dorg.sqlite.tmpdir="$base/tmp" \
        -Djava.util.prefs.userRoot="$base/prefs" -Djava.util.prefs.systemRoot="$base/prefs" \
        -Dopenrocket.debug.prefs=true \
        -cp "$classpath" info.qtrocket.guireference.GuiReference \
        --scenario "$scenario" --out "$target" --work "$base/saves" \
        --commit "$commit" --dirty "$dirty" --repo "$repo" --preset-commit "$preset_commit" \
        > "$base/stdout.txt"; then
        echo "error: the tool failed on $name (its log: $base/log.txt)" >&2
        tail -n 30 "$base/log.txt" >&2
        exit 1
    fi
    grep -E '^Wrote ' "$base/log.txt" >&2 || true
}

status=0
for name in "${names[@]}"; do
    committed="$scenarios_dir/$name"
    if [[ "$check" == true ]]; then
        out="$work/$name/first/out"
    elif [[ -n "${GUI_REFERENCE_OUT:-}" ]]; then
        out="$GUI_REFERENCE_OUT/$name"
    else
        out="$committed"
    fi
    mkdir -p "$out"
    out="$(cd "$out" && pwd)"
    recorded="$(sed -n 's/^ *"commit": "\([0-9a-f]*\)".*/\1/p' "$committed/state.json" 2>/dev/null | head -n 1 || true)"
    if [[ -n "$recorded" && "$recorded" != "$commit" ]]; then
        echo "warning: the committed reference of $name comes from OpenRocket $recorded, the checkout is at $commit" >&2
    fi
    echo "scenario $name (version ${versions[$name]})" >&2
    run_tool "$name" "$out" first-run
    run_tool "$name" "$work/$name/second/out" second-run
    for file in rocket.ork state.json; do
        if ! cmp -s "$out/$file" "$work/$name/second/out/$file"; then
            echo "error: two runs of the tool wrote different $file for $name" >&2
            diff "$out/$file" "$work/$name/second/out/$file" | head -n 20 >&2 || true
            exit 1
        fi
    done
    echo "Two runs wrote the same rocket.ork and state.json for $name into $out" >&2
    if [[ "$check" == true ]]; then
        for file in rocket.ork state.json; do
            if cmp -s "$out/$file" "$committed/$file"; then
                echo "$name/$file: same" >&2
            else
                echo "$name/$file: DIFFERS from the committed file" >&2
                status=1
            fi
        done
    fi
done
exit "$status"
