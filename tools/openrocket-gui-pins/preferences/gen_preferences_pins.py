#!/usr/bin/env python3
"""Writes tests/gui/dialogs/PreferencesPagePins.h from the transcript of PreferencesProbe.java.

usage: gen_preferences_pins.py OUT.txt[.gz] HEADER COMMIT HOME

The transcript's pages (PAGE name ... END name, each ITEM one component; an item's text may run
over several lines), the two lines of the Default imperial and Default metric steps, and the
units node of the store after storeAndApplyPreferences() become C++ string tables, non-ASCII
characters as \\u escapes; HOME, the probe's user.home (generate.sh gives it a fresh one), is
written as <home>. So do the walks of the pages of Milestone 4 (WALKPAGE name ... END name, each
WALK line without "WALK ") and their steps (STEPS name ... END STEPS, each STEP line without
"STEP "). clang-format is run on the header.
"""
import gzip
import os
import pathlib
import subprocess
import sys

def escape(text):
    out = []
    for ch in text:
        code = ord(ch)
        if ch == '\\':
            out.append('\\\\')
        elif ch == '"':
            out.append('\\"')
        elif ch == '\n':
            out.append('\\n')
        elif code < 0x20:
            out.append('\\x%02x' % code)
        elif code < 0x80:
            out.append(ch)
        elif code <= 0xFFFF:
            out.append('\\u%04X' % code)
        else:
            out.append('\\U%08X' % code)
    return '"' + ''.join(out) + '"'

def main():
    source, header, commit, home = sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4]
    opener = gzip.open if source.endswith('.gz') else open
    with opener(source, 'rt', encoding='utf-8') as transcript:
        lines = transcript.read().split('\n')
    pages = {}
    steps = {}
    stores = {}
    walks = {}
    stepLists = {}
    page = None
    store = None
    walk = None
    stepList = None
    for line in lines:
        if walk is not None:
            if line == 'END ' + walk:
                walk = None
            elif line.startswith('WALK '):
                walks[walk].append(line[5:])
            else:
                raise SystemExit('unexpected line in the walk %s: %r' % (walk, line))
            continue
        if stepList is not None:
            if line == 'END STEPS':
                stepList = None
            else:
                stepLists[stepList].append(line[5:] if line.startswith('STEP ') else line)
            continue
        if line.startswith('WALKPAGE '):
            walk = line[9:]
            walks[walk] = []
            continue
        if line.startswith('STEPS '):
            stepList = line[6:]
            stepLists[stepList] = []
            continue
        if page is not None:
            if line == 'END ' + page:
                page = None
            elif line.startswith('ITEM '):
                # The home directory of the probe's run (the thrust curve and preset directories)
                # is no part of the pins.
                pages[page].append(line[5:].replace(home, '<home>'))
            elif pages[page]:
                pages[page][-1] += '\n' + line
            continue
        if store is not None:
            if line == 'END STORE':
                store = None
            elif line.startswith('VALUE '):
                stores[store].append(line[6:])
            continue
        if line.startswith('PAGE '):
            page = line[5:]
            pages[page] = []
        elif line.startswith('STORE '):
            store = line[6:]
            stores[store] = []
        elif line.startswith('STEP Default imperial:') or line.startswith('STEP Default metric:'):
            name, value = line[5:].split(':', 1)
            steps[name] = value.strip().split(' ')

    def table(name, items, comment):
        rows = ',\n'.join('    ' + escape(item) for item in items)
        return (f'/// {comment}\ninline constexpr std::array<std::string_view, {len(items)}> {name}{{\n'
                f'{rows}}};\n')

    # The groups' default units are shown one per combo box; "° N" holds a space, so the
    # steps are split by the combo boxes' units instead: rebuild them from the page's order.
    def stepUnits(name):
        text = [l for l in lines if l.startswith('STEP ' + name + ':')][0]
        text = text[len('STEP ' + name + ':') + 1:]
        combos = [item for item in pages['UNITS'] if item.startswith('COMBO ')]
        result = []
        for combo in combos:
            items = combo.split(' items=', 1)[1].split(' tip=', 1)[0].split(' disabled')[0].split('|')
            match = None
            for unit in sorted(items, key=len, reverse=True):
                if text.startswith(unit + ' ') or text == unit:
                    match = unit
                    break
            if match is None:
                raise SystemExit('cannot split %r at %r' % (text, items))
            result.append(match)
            text = text[len(match) + 1:]
        return result

    units = stores['AFTER storeAndApplyPreferences(prefs, null) with LENGTH in']
    units = [u[len('units/'):] for u in units if u.startswith('units/')]

    out = ['#pragma once', '',
           '// Generated by tools/openrocket-gui-pins/generate.sh (preferences/gen_preferences_pins.py) from',
           '// the transcript of preferences/probe/PreferencesProbe.java',
           '// (OpenRocket ' + commit + ',',
           '// SwingPreferences, FlatLaf Light); do not edit. A page item is its kind (LABEL, CHECK,',
           '// COMBO, SPINNER, BUTTON, SEPARATOR, FIELD, TEXT) and its text, then selected=, items=',
           '// (the items joined by |), disabled and tip= as the probe writes them. The walks of the',
           '// pages of Milestone 4 are in the form of tests/gui/dialogs/simulation/SimulationDialogWalk.h',
           '// (the probe\'s WALK lines), their steps the probe\'s STEP lines, both without the prefix.',
           '', '#include <array>', '#include <string_view>', '',
           'namespace QtRocket::Gui::Test::PreferencesPagePins', '{', '',
           'inline constexpr std::string_view kOpenRocketCommit = ' + escape(commit) + ';', '']
    out.append(table('kUnitsPage', pages['UNITS'], 'UnitsPreferencesPanel of a cleared store.'))
    out.append(table('kDesignPage', pages['DESIGN'], 'DesignPreferencesPanel of a cleared store.'))
    out.append(table('kGeneralPage', pages['GENERAL'],
                     'GeneralPreferencesPanel of a cleared store.'))
    out.append(table('kUnitsPageAfterSteps', pages['UNITS AFTER STEPS'],
                     'The Units page after the last unit chosen for Rocket dimensions, Default imperial, '
                     'Default metric and two clicks of the secondary stability box.'))
    out.append(table('kDefaultImperialUnits', stepUnits('Default imperial'),
                     'The Units page\'s combo boxes, in order, after Default imperial.'))
    out.append(table('kDefaultMetricUnits', stepUnits('Default metric'),
                     'The Units page\'s combo boxes, in order, after Default imperial, then Default metric.'))
    out.append(table('kStoredUnitsWithLengthInches', units,
                     'The units node after storeAndApplyPreferences() of the factory units with LENGTH in, '
                     '"GROUP=unit" sorted.'))
    # Milestone 4: the pages of the simulations, each over a store just cleared.
    out.append(table('kDesignPageSimulationSteps', stepLists['DESIGN'],
                     'The Design page\'s two boxes of the simulations clicked twice each: '
                     '"click \'<text>\' selected=<state> | <key>=<stored>".'))
    out.append(table('kSimulationPageWalk', walks['SIMULATION'],
                     'SimulationPreferencesPanel, in the form of the dialog walk (SimulationDialogWalk.h).'))
    out.append(table('kSimulationPageSteps', stepLists['SIMULATION'],
                     'The Simulation page\'s check boxes clicked, the geodetic computations chosen, then '
                     'the constant gravity.'))
    out.append(table('kSimulationPageWalkConstantGravity', walks['SIMULATION CONSTANT GRAVITY'],
                     'The Simulation page after those steps.'))
    out.append(table('kSimulationPageSteps2', stepLists['SIMULATION 2'],
                     'Then the gravity value, the time step and the maximum time typed, the seed\'s check '
                     'box, field, focus loss and prepareForSave(), and "Reset to default" after edits of '
                     'each value.'))
    out.append(table('kSimulationPageWalkAfterReset', walks['SIMULATION AFTER RESET'],
                     'The Simulation page after "Reset to default".'))
    out.append(table('kWarningsPageWalk', walks['WARNINGS'], 'WarningsPreferencesPanel.'))
    out.append(table('kWarningsPageSteps', stepLists['WARNINGS'],
                     'The Warnings page\'s thresholds typed, the dual deployment section expanded.'))
    out.append(table('kWarningsPageWalkExpanded', walks['WARNINGS EXPANDED'],
                     'The Warnings page after those steps.'))
    out.append(table('kWarningsPageWalkSingleCollapsed', walks['WARNINGS SINGLE COLLAPSED'],
                     'Then with the single deployment section collapsed.'))
    out.append(table('kLaunchPageWalk', walks['LAUNCH'], 'LaunchPreferencesPanel.'))
    out.append(table('kLaunchPageSteps', stepLists['LAUNCH'],
                     'The Launch page made, then its average wind, rod length and latitude typed.'))
    out.append('}  // namespace QtRocket::Gui::Test::PreferencesPagePins')
    open(header, 'w', encoding='ascii').write('\n'.join(out) + '\n')
    # The repository's style (its .clang-format, wherever the header is written: generate.sh
    # --check writes it into a temporary directory), so that a run reproduces the committed
    # header byte for byte.
    style = pathlib.Path(__file__).resolve().parents[3] / ".clang-format"
    clang_format = os.environ.get("CLANG_FORMAT", "clang-format")
    subprocess.run([clang_format, "-i", f"--style=file:{style}", header], check=True)

main()
