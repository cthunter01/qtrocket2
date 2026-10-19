#!/usr/bin/env bash
# Regenerates the pin tables of the figure's shape tests from OpenRocket (see ../README.md, "Figure pin
# tables"): tests/core/figure/BodyShapePins.h, FinMassShapePins.h and FigureGeometryPins.h.
#
#   tools/openrocket-goldens/figure-pins/generate.sh
#
# Environment (as for ../generate.sh):
#   OPENROCKET_DIR   the OpenRocket checkout (default: "openrocket" next to the QtRocket repository)
#   JAVA             the java executable (default: java on PATH); it must be Java 17, whose javac is
#                    taken from the same JDK (its java.home)
#   ALLOW_DIRTY=1    accept an OpenRocket checkout with modified, untracked or ignored source files (the
#                    headers then record the checkout as dirty)
#   PINS_OUT         write the three headers into this directory instead of tests/core/figure
#
# It builds the golden harness as ../generate.sh does (OpenRocket's core and the shape providers of its
# 2D rocket figure, compiled read-only into tools/openrocket-goldens/build), compiles the two probes of
# src/probe against it, runs them headless (their dumps go into build/figure-pins: every double as
# Double.toHexString) and writes the headers with the two generators, which hold the selection of what
# is pinned. The headers record the OpenRocket commit they come from. The checkout is only read; the dumps
# do not depend on the machine.
set -euo pipefail

if (($# > 0)); then
    echo "usage: $(basename "${BASH_SOURCE[0]}")   (see the head of the script for the environment)" >&2
    exit 2
fi

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
harness="$(cd "$here/.." && pwd)"
repo="$(cd "$harness/../.." && pwd)"
main_tree="$(dirname "$(git -C "$repo" rev-parse --path-format=absolute --git-common-dir)")"
openrocket="${OPENROCKET_DIR:-$main_tree/../openrocket}"
openrocket="$(cd "$openrocket" && pwd)"
java="${JAVA:-java}"
out="${PINS_OUT:-$repo/tests/core/figure}"

if [[ ! -f "$openrocket/core/src/main/java/info/openrocket/core/startup/Application.java" ]]; then
    echo "error: $openrocket is not an OpenRocket checkout (set OPENROCKET_DIR)" >&2
    exit 1
fi
properties="$("$java" -XshowSettings:properties -version 2>&1)"
java_version="$(sed -n 's/^ *java\.specification\.version = //p' <<<"$properties")"
if [[ "$java_version" != 17 ]]; then
    echo "error: $java is Java ${java_version:-(unknown)}; the pins are made with Java 17 (set JAVA)" >&2
    exit 1
fi
# The javac of that JDK (java on PATH can be a link that another JDK's javac sits beside).
javac="$(sed -n 's/^ *java\.home = //p' <<<"$properties")/bin/javac"

# The checkout must be exactly its commit among what the build compiles or the probes read, as
# ../generate.sh checks it: no modified tracked file, no untracked or ignored file there (build.gradle keeps
# the two ignored copies that OpenRocket's own build leaves in core/src/main/resources off the class path).
# The preset submodule is not among them: the probes load the examples without presets.
sources=(core/src/main/java core/src/main/resources core/build.gradle core/libs
    core/src/test/java/info/openrocket/core/ServicesForTesting.java
    swing/src/main/java/info/openrocket/swing/gui/rocketfigure
    swing/src/main/resources/META-INF/services/info.openrocket.swing.gui.rocketfigure.RocketComponentShapeService
    swing/src/main/java/info/openrocket/swing/gui/print/PrintUnit.java)
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
        echo "error: the pins would not match the checkout's commit (ALLOW_DIRTY=1 overrides)" >&2
        exit 1
    fi
    dirty=true
fi
recorded="$(sed -n 's/^ *"openrocket": {"commit": "\([0-9a-f]*\)".*/\1/p' "$repo/tests/data/goldens/manifest.json" 2>/dev/null || true)"
commit="$(git -C "$openrocket" rev-parse HEAD)"
if [[ -n "$recorded" && "$recorded" != "$commit" ]]; then
    echo "warning: the goldens come from OpenRocket $recorded, the checkout is at $commit" >&2
fi
echo "OpenRocket: $openrocket @ $commit" >&2

"$openrocket/gradlew" --quiet --console=plain -p "$harness" -PopenrocketDir="$openrocket" goldensClasspath
classpath="$(cat "$harness/build/goldens-classpath.txt")"

work="$harness/build/figure-pins"
rm -rf "$work"
mkdir -p "$work/classes" "$work/prefs" "$out"
# BodyShapesProbe asks PrintUnit (a self-contained enum of the Swing module) for the print scale.
"$javac" --release 17 -nowarn -proc:none -encoding UTF-8 -d "$work/classes" -cp "$classpath" \
    "$here/src/probe/BodyShapesProbe.java" "$here/src/probe/FinMassShapesProbe.java" \
    "$openrocket/swing/src/main/java/info/openrocket/swing/gui/print/PrintUnit.java"

motors="$openrocket/core/src/main/resources/datafiles/thrustcurves/initial_motors.db"
for probe in BodyShapesProbe:body-shapes.txt FinMassShapesProbe:fin-mass-shapes.txt; do
    echo "running ${probe%%:*}" >&2
    # A fresh java.util.prefs store: the userRoot and systemRoot properties isolate it on Linux only, so
    # openrocket.debug.prefs also points OpenRocket at its node "OpenRocket-debug", which SwingPreferences
    # clears when it starts and from which TranslatorProvider reads no locale, on every system.
    "$java" -Xmx2g -Djava.awt.headless=true -Duser.language=en -Duser.country=US -Duser.timezone=UTC \
        -Dfile.encoding=UTF-8 -Djava.util.prefs.userRoot="$work/prefs" -Djava.util.prefs.systemRoot="$work/prefs" \
        -Dopenrocket.debug.prefs=true \
        -Dorg.sqlite.tmpdir="$work" -Djava.io.tmpdir="$work" \
        -cp "$work/classes:$classpath" "probe.${probe%%:*}" "$repo/data/examples" "$work/${probe#*:}" "$motors"
done

python3 -I "$here/gen_body_shape_pins.py" "$work/body-shapes.txt" "$out/BodyShapePins.h" "$commit" "$dirty"
python3 -I "$here/gen_fin_mass_shape_pins.py" "$work/fin-mass-shapes.txt" "$out/FinMassShapePins.h" \
    "$out/FigureGeometryPins.h" "$commit" "$dirty"
echo "wrote $out/BodyShapePins.h, FinMassShapePins.h and FigureGeometryPins.h" >&2
