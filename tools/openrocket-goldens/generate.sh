#!/usr/bin/env bash
# Regenerates tests/data/goldens from a local OpenRocket checkout. See README.md.
#
#   tools/openrocket-goldens/generate.sh [--only <input name>]...
#
# Environment:
#   OPENROCKET_DIR   the OpenRocket checkout (default: "openrocket" next to the QtRocket repository)
#   JAVA             the java executable (default: java on PATH; OpenRocket needs Java 17 or 21)
#   ALLOW_DIRTY=1    accept an OpenRocket checkout with modified tracked files
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
if [[ "${ALLOW_DIRTY:-0}" != 1 ]] && ! git -C "$openrocket" diff --quiet HEAD --; then
    echo "error: the OpenRocket checkout has modified tracked files; the goldens would not match" >&2
    echo "       the recorded commit (set ALLOW_DIRTY=1 to override)" >&2
    exit 1
fi
commit="$(git -C "$openrocket" rev-parse HEAD)"
preset_commit="$(git -C "$openrocket/core/resources-src/datafiles/openrocket-database" rev-parse HEAD 2>/dev/null || echo unknown)"

echo "OpenRocket: $openrocket @ $commit" >&2

# Build with OpenRocket's own Gradle wrapper (no wrapper is vendored here). The project directory
# is this one, so nothing is written into the checkout.
"$openrocket/gradlew" --quiet --console=plain -p "$here" -PopenrocketDir="$openrocket" goldensClasspath

out="$repo/tests/data/goldens"
work="$here/build/work"
mkdir -p "$out" "$work"

# A fixed locale, time zone and encoding; headless AWT (OpenRocket's core touches java.awt.geom).
"$java" -Djava.awt.headless=true -Duser.language=en -Duser.country=US -Duser.timezone=UTC \
    -Dfile.encoding=UTF-8 \
    -cp "$(cat "$here/build/goldens-classpath.txt")" info.qtrocket.goldens.GoldenDumper \
    --openrocket "$openrocket" --examples "$repo/data/examples" --out "$out" --work "$work" \
    --commit "$commit" --preset-commit "$preset_commit" "$@"

echo "Golden data size:" >&2
du -sh "$out" >&2
