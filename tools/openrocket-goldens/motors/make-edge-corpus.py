#!/usr/bin/env python3
"""Writes tests/data/motors-edge/: malformed and edge-case motor files and a small motor database.

MotorsDumper.java records what OpenRocket makes of each of them in tests/data/goldens/motors.json
(its "edgeFiles" and "edgeDatabase"), so that motors_golden_tests pin QtRocket's acceptance,
messages and data on them against OpenRocket's own. The files are committed; run this again only
to change the corpus, then run dump-motors.sh.

Usage: tools/openrocket-goldens/motors/make-edge-corpus.py
"""

import io
import os
import sqlite3
import struct
import zipfile
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "..", "..", "tests", "data", "motors-edge")

ENGINE = (
    '<engine mfg="{mfg}" code="B6-4" Type="single-use" dia="18." len="70." initWt="20." '
    'propWt="10." delays="4" auto-calc-mass="1" auto-calc-cg="1">'
)
POINTS = (
    '<data>\n<eng-data t="0.0" f="0.0"/>\n<eng-data t="0.5" f="10.0"/>\n'
    '<eng-data t="1.0" f="0.0"/>\n</data>\n'
)


def rse(comments="A motor", mfg="Estes", data=POINTS, prolog='<?xml version="1.0" encoding="UTF-8"?>\n',
        after="", engine=None):
    engine = engine if engine is not None else ENGINE.format(mfg=mfg)
    return (prolog + "<engine-database>\n<engine-list>\n" + engine + "\n<comments>" + comments +
            "</comments>\n" + data + "</engine>\n</engine-list>\n</engine-database>\n" + after)


ENG = "; a comment\nA8 18 70 3-5 0.0033 0.0162 Estes\n0.0 0.0\n0.5 5.0\n1.0 0.0\n"
ENG2 = "B6 18 70 4 0.006 0.02 Estes\n0.0 0.0\n0.4 6.0\n0.8 0.0\n"

FIXED_TIME = (2020, 1, 1, 0, 0, 0)


def zip_bytes(entries, method=zipfile.ZIP_DEFLATED):
    buffer = io.BytesIO()
    with zipfile.ZipFile(buffer, "w", method) as archive:
        for name, data in entries:
            info = zipfile.ZipInfo(name, FIXED_TIME)
            info.compress_type = method
            archive.writestr(info, data)
    return buffer.getvalue()


def local_entry(name_bytes, data, crc=None, flags=0):
    crc = zlib.crc32(data) & 0xFFFFFFFF if crc is None else crc
    return struct.pack("<IHHHHHIIIHH", 0x04034B50, 20, flags, 0, 0, 0x5021, crc, len(data), len(data),
                       len(name_bytes), 0) + name_bytes + data


def descriptor_entry(name, data, signature):
    compressor = zlib.compressobj(6, zlib.DEFLATED, -15)
    compressed = compressor.compress(data) + compressor.flush()
    name_bytes = name.encode()
    entry = struct.pack("<IHHHHHIIIHH", 0x04034B50, 20, 8, 8, 0, 0x5021, 0, 0, 0, len(name_bytes), 0)
    entry += name_bytes + compressed
    if signature:
        entry += struct.pack("<I", 0x08074B50)
    return entry + struct.pack("<III", zlib.crc32(data) & 0xFFFFFFFF, len(compressed), len(data))


def main():
    os.makedirs(OUT, exist_ok=True)
    files = {}

    # RockSim engine files: XML that OpenRocket's parser rejects, and the SimpleSAX quirk
    files["rse-bom.rse"] = "﻿" + rse()
    files["rse-trailing-content.rse"] = rse(after="junk\n")
    files["rse-comments-quirk.rse"] = rse(comments="a<b/>c")
    files["rse-bare-ampersand.rse"] = rse(comments="AT&T rocks")
    files["rse-less-than-in-attribute.rse"] = rse(mfg="A<B")
    files["rse-control-character.rse"] = rse(comments="a\x01z")
    files["rse-unbound-prefix.rse"] = rse().replace("<engine-database>", "<x:engine-database>").replace(
        "</engine-database>", "</x:engine-database>")
    files["rse-double-dash-comment.rse"] = rse(after="<!-- a -- b -->\n")
    files["rse-cdata-end-in-text.rse"] = rse(comments="a]]>z")
    files["rse-space-before-declaration.rse"] = "  " + rse()
    files["rse-version-2.rse"] = rse(prolog='<?xml version="2.0"?>\n')
    files["rse-nul-after-root.rse"] = rse(after="\0junk")
    files["rse-duplicate-expanded-name.rse"] = rse(engine=ENGINE.format(mfg="Estes")[:-1] +
                                                   ' xmlns:u="urn:a" xmlns:v="urn:a" u:x="1" v:x="2">')
    files["rse-undeclared-entity.rse"] = rse(comments="Estes &amp; Cox &nbsp;")
    files["rse-char-reference-zero.rse"] = rse(comments="a&#0;z")
    files["rse-handler-error-then-malformed.rse"] = (
        '<engine-database><engine code="B6"/><foo></bar></engine-database>\n')
    files["rse-handler-error-then-trailing.rse"] = (
        '<engine-database><engine code="B6"/></engine-database>\njunk\n')
    files["rse-empty-doctype.rse"] = rse(prolog='<?xml version="1.0"?>\n<!DOCTYPE engine-database>\n')
    files["rse-internal-subset-comment.rse"] = rse(
        prolog='<?xml version="1.0"?>\n<!DOCTYPE engine-database [ <!-- no declarations --> ]>\n')
    files["rse-xml-1.1.rse"] = rse(prolog='<?xml version="1.1"?>\n', comments="a&#1;z\u0085next")
    files["rse-namespaces.rse"] = rse().replace("<engine-database>",
                                                '<r:engine-database xmlns:r="urn:r">').replace(
        "</engine-database>", "</r:engine-database>")
    files["rse-one-point.rse"] = rse(data='<data>\n<eng-data t="0.0" f="1.0"/>\n</data>\n')
    files["rse-two-zero-points.rse"] = rse(
        data='<data>\n<eng-data t="0.0" f="1.0"/>\n<eng-data t="0.0" f="1.0"/>\n</data>\n')
    files["rse-huge-thrust.rse"] = rse(
        data='<data>\n<eng-data t="0.0" f="0.0"/>\n<eng-data t="0.5" f="2e7"/>\n'
             '<eng-data t="1.0" f="0.0"/>\n</data>\n')
    files["rse-surrogate-bytes.rse"] = None  # written as bytes below

    # RASP engine files: line ends
    files["eng-crlf.eng"] = ENG.replace("\n", "\r\n")
    files["eng-cr.eng"] = ENG.replace("\n", "\r")

    for name, text in files.items():
        if text is not None:
            with open(os.path.join(OUT, name), "w", encoding="utf-8", newline="") as out:
                out.write(text)
    # CESU-8 (an encoded surrogate) and a truncated sequence in the description
    with open(os.path.join(OUT, "rse-surrogate-bytes.rse"), "wb") as out:
        out.write(rse(comments="a@b@c").encode().replace(b"@", b"\xed\xa0\x80", 1).replace(b"@", b"\xe2\x82", 1))

    # ZIP archives, read entry by entry from their local headers as ZipInputStream reads them
    eng, eng2 = ENG.encode(), ENG2.encode()
    intact = zip_bytes([("a.eng", eng), ("b.eng", eng2)])
    central = intact.find(b"PK\x01\x02")
    binaries = {
        "zip-no-central-directory.zip": intact[:central],
        "zip-trailing-garbage.zip": intact[:central] + b"garbage",
        "zip-short-local-header.zip": b"PK\x03\x04\x14\x00\x00\x00",
        "zip-bad-local-crc.zip": local_entry(b"a.eng", eng, crc=0x6C4B3B66),
        "zip-cp437-name.zip": local_entry(b"m\x81nchen.eng", eng),
        "zip-stored.zip": zip_bytes([("dir/", b""), ("dir/a.eng", eng), ("notes.txt", b"hello")],
                                    zipfile.ZIP_STORED),
        "zip-nested.zip": zip_bytes([("inner.zip", intact), ("c.eng", eng2)]),
        "zip-data-descriptors.zip": descriptor_entry("a.eng", eng, True) +
                                    descriptor_entry("b.eng", eng2, False) + b"PK\x01\x02",
    }
    # the central directory lists the entries in the other order
    reordered = bytearray(intact)
    records = [reordered.find(b"PK\x01\x02"), reordered.find(b"PK\x01\x02", central + 4)]
    end = reordered.find(b"PK\x05\x06")
    first, second = bytes(reordered[records[0]:records[1]]), bytes(reordered[records[1]:end])
    reordered[records[0]:end] = second + first
    binaries["zip-central-directory-reordered.zip"] = bytes(reordered)
    for name, data in binaries.items():
        with open(os.path.join(OUT, name), "wb") as out:
            out.write(data)

    # A motor database with the curves OpenRocket's reader skips and invalid UTF-8 text
    database = os.path.join(OUT, "edge.db")
    if os.path.exists(database):
        os.remove(database)
    connection = sqlite3.connect(database)
    connection.executescript("""
        CREATE TABLE meta (key TEXT PRIMARY KEY, value TEXT NOT NULL);
        CREATE TABLE manufacturers (id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT NOT NULL UNIQUE,
            abbrev TEXT);
        CREATE TABLE motors (id INTEGER PRIMARY KEY AUTOINCREMENT, manufacturer_id INTEGER NOT NULL,
            tc_motor_id TEXT, designation TEXT NOT NULL, common_name TEXT, impulse_class TEXT,
            diameter REAL, length REAL, total_impulse REAL, avg_thrust REAL, max_thrust REAL,
            burn_time REAL, propellant_weight REAL, total_weight REAL, type TEXT, delays TEXT,
            case_info TEXT, prop_info TEXT, sparky INTEGER, info_url TEXT, data_files INTEGER,
            updated_on TEXT, description TEXT, source TEXT,
            FOREIGN KEY (manufacturer_id) REFERENCES manufacturers(id));
        CREATE TABLE thrust_curves (id INTEGER PRIMARY KEY AUTOINCREMENT, motor_id INTEGER NOT NULL,
            tc_simfile_id TEXT, source TEXT, format TEXT, license TEXT, info_url TEXT, data_url TEXT,
            total_impulse REAL, avg_thrust REAL, max_thrust REAL, burn_time REAL,
            FOREIGN KEY (motor_id) REFERENCES motors(id) ON DELETE CASCADE);
        CREATE TABLE thrust_data (id INTEGER PRIMARY KEY AUTOINCREMENT, curve_id INTEGER NOT NULL,
            time_seconds REAL NOT NULL, force_newtons REAL NOT NULL,
            FOREIGN KEY (curve_id) REFERENCES thrust_curves(id) ON DELETE CASCADE);
        INSERT INTO meta VALUES ('schema_version', '3');
        INSERT INTO manufacturers (id, name, abbrev) VALUES (1, 'Estes', 'Estes');
        INSERT INTO manufacturers (id, name, abbrev) VALUES (2, CAST(X'4573FF746573' AS TEXT),
            CAST(X'4573FF' AS TEXT));
    """)
    motors = [
        # id, manufacturer, designation, curves
        (1, 1, "A8-3", [[(0.0, 0.0), (0.5, 5.0), (1.0, 0.0)]]),
        (2, 1, "B6-4", []),                                  # no curves
        (3, 1, "C6-5", [[]]),                                # a curve without data
        (4, 1, "D12-5", [[(0.0, 1.0)]]),                     # one point left
        (5, 1, "E9-6", [[(0.0, 0.0), (0.5, 2e7), (1.0, 0.0)]]),  # thrust out of range
        (6, 2, None, [[(0.0, 0.0), (0.4, 6.0), (0.8, 0.0)]]),    # invalid UTF-8 text
    ]
    curve_id = 1
    for motor_id, manufacturer, designation, curves in motors:
        connection.execute(
            "INSERT INTO motors (id, manufacturer_id, designation, common_name, diameter, length, "
            "propellant_weight, total_weight, type, delays, description, source) VALUES "
            "(?, ?, ?, ?, 0.018, 0.07, 0.003, 0.016, 'SU', '3,5', 'edge', 'cert')",
            (motor_id, manufacturer, designation or "", designation or ""))
        if designation is None:
            connection.execute(
                "UPDATE motors SET designation = CAST(X'4136EDA0802D33' AS TEXT), "
                "common_name = CAST(X'4136EDA080' AS TEXT) WHERE id = ?", (motor_id,))
        for points in curves:
            connection.execute("INSERT INTO thrust_curves (id, motor_id, source) VALUES (?, ?, 'cert')",
                               (curve_id, motor_id))
            for time, force in points:
                connection.execute(
                    "INSERT INTO thrust_data (curve_id, time_seconds, force_newtons) VALUES (?, ?, ?)",
                    (curve_id, time, force))
            curve_id += 1
    connection.commit()
    connection.execute("VACUUM")
    connection.close()


if __name__ == "__main__":
    main()
