#!/usr/bin/env python3
"""Prints OpenRocket's build.version from the first build.properties on a class path.

usage: build_version.py CLASSPATH

CLASSPATH is a ':'-separated class path; each directory is searched for build.properties, each jar
for the entry build.properties. OpenRocket's build stamps the short commit hash into core's
(build.version=<version>-SNAPSHOT-<hash>). Exit status 1 when no build.properties on the class path
has a build.version.
"""
import os
import sys
import zipfile


def version_of(text):
    for line in text.splitlines():
        if line.startswith("build.version="):
            return line[len("build.version="):].strip()
    return None


def main():
    for entry in sys.argv[1].split(":"):
        text = None
        if os.path.isdir(entry):
            path = os.path.join(entry, "build.properties")
            if os.path.isfile(path):
                with open(path, encoding="utf-8") as f:
                    text = f.read()
        elif entry.endswith(".jar") and os.path.isfile(entry):
            with zipfile.ZipFile(entry) as jar:
                if "build.properties" in jar.namelist():
                    text = jar.read("build.properties").decode("utf-8")
        version = version_of(text) if text is not None else None
        if version:
            print(version)
            return
    sys.exit(1)


if __name__ == "__main__":
    main()
