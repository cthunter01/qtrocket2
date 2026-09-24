#!/usr/bin/env bash
# Regenerates tests/data/goldens from a local OpenRocket checkout. See README.md.
#
#   tools/openrocket-goldens/generate.sh [--only <input name>]...
#
# Environment:
#   OPENROCKET_DIR   the OpenRocket checkout (default: "openrocket" next to the QtRocket repository)
#   JAVA             the java executable (default: java on PATH); it must be Java 17
#   ALLOW_DIRTY=1    accept an OpenRocket checkout with modified, untracked or ignored source files, or a
#                    preset submodule that is not at its recorded commit (manifest.json records "dirty")
#
# The checkout is only read: OpenRocket's core is compiled into tools/openrocket-goldens/build.
# motors.json (and tools/openrocket-goldens/motors/) are produced by motors/dump-motors.sh, not here.
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo="$(cd "$here/../.." && pwd)"
# The main working tree of the QtRocket repository (a worktree's common git directory is the main one's .git).
main_tree="$(dirname "$(git -C "$repo" rev-parse --path-format=absolute --git-common-dir)")"
openrocket="${OPENROCKET_DIR:-$main_tree/../openrocket}"
openrocket="$(cd "$openrocket" && pwd)"
java="${JAVA:-java}"

if [[ ! -f "$openrocket/core/src/main/java/info/openrocket/core/startup/Application.java" ]]; then
    echo "error: $openrocket is not an OpenRocket checkout (set OPENROCKET_DIR)" >&2
    exit 1
fi

# The goldens are generated with Java 17: Double.toString, which writes every number, changed in JDK 19
# (shortest representation, JDK-4511638), so another JDK would rewrite committed values.
java_version="$("$java" -XshowSettings:properties -version 2>&1 |
    sed -n 's/^ *java\.specification\.version = //p')"
if [[ "$java_version" != 17 ]]; then
    echo "error: $java is Java ${java_version:-(unknown)}; the goldens are generated with Java 17 (set JAVA)" >&2
    exit 1
fi

# The checkout must be exactly the recorded commits: no modified tracked file; no untracked or ignored file
# among what the build compiles or packages (build.gradle keeps the two ignored copies that OpenRocket's own
# build leaves in core/src/main/resources off the class path); the preset submodule initialized, at the
# commit the superproject records, and unmodified.
submodule_path=core/resources-src/datafiles/openrocket-database
sources=(core/src/main/java core/src/main/resources core/build.gradle core/libs
    core/src/test/java/info/openrocket/core/ServicesForTesting.java)
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
dirty=false
if ((${#problems[@]} > 0)); then
    for problem in "${problems[@]}"; do
        echo "OpenRocket checkout: $problem" >&2
    done
    if [[ "${ALLOW_DIRTY:-0}" != 1 ]]; then
        echo "error: the goldens would not match the recorded commits (ALLOW_DIRTY=1 overrides)" >&2
        exit 1
    fi
    dirty=true
fi
commit="$(git -C "$openrocket" rev-parse HEAD)"
# The commit the superproject records for the submodule (what a checkout of $commit gives).
preset_commit="$(git -C "$openrocket" rev-parse "HEAD:$submodule_path")"

echo "OpenRocket: $openrocket @ $commit (presets @ $preset_commit)" >&2

out="$repo/tests/data/goldens"
recorded="$(sed -n 's/^ *"openrocket": {"commit": "\([0-9a-f]*\)".*/\1/p' "$out/manifest.json" 2>/dev/null || true)"
if [[ -n "$recorded" && "$recorded" != "$commit" ]]; then
    echo "warning: the committed goldens come from OpenRocket $recorded, the checkout is at $commit;" >&2
    echo "         check out $recorded (and run git submodule update --init) to reproduce them" >&2
fi

# Build with OpenRocket's own Gradle wrapper (no wrapper is vendored here). The project directory
# is this one, so nothing is written into the checkout.
"$openrocket/gradlew" --quiet --console=plain -p "$here" -PopenrocketDir="$openrocket" goldensClasspath

work="$here/build/work"
mkdir -p "$out" "$work"

# A fixed locale, time zone and encoding; headless AWT (OpenRocket's core touches java.awt.geom).
"$java" -Djava.awt.headless=true -Duser.language=en -Duser.country=US -Duser.timezone=UTC \
    -Dfile.encoding=UTF-8 \
    -cp "$(cat "$here/build/goldens-classpath.txt")" info.qtrocket.goldens.GoldenDumper \
    --openrocket "$openrocket" --examples "$repo/data/examples" --out "$out" --work "$work" \
    --commit "$commit" --preset-commit "$preset_commit" --dirty "$dirty" "$@"

echo "Golden data size:" >&2
du -sh "$out" >&2
