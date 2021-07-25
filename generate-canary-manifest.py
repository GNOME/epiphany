# -*- coding: utf-8 -*-
# Copyright (C) 2021 Igalia S.L.
#
# This file is part of Epiphany.
#
# Epiphany is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# Epiphany is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with Epiphany.  If not, see <http://www.gnu.org/licenses/>.

from html.parser import HTMLParser
import argparse
import hashlib
import json
import os
import re
import sys
import urllib.request
import requests

ZIP_FILE = "webkitgtk.zip"

# FIXME: Might be worth adding some JSON file listing builds on the servers.
class MyHTMLParser(HTMLParser):
    builds = []
    def handle_starttag(self, tag, attrs):
        if tag != "a":
            return
        for (name, value) in attrs:
            if name == "href" and (value.startswith("release") or value.startswith("debug")):
                self.builds.append(value)

def download_zipped_build(build_type):
    url = f"https://webkitgtk-{build_type}.igalia.com/built-products/"
    with urllib.request.urlopen(url) as page_fd:
        parser = MyHTMLParser()
        parser.feed(page_fd.read().decode("utf-8"))
        try:
            latest = parser.builds[-1]
        except IndexError:
            print(f"No build found in {url}")
            return ("", "")

    print(f"Downloading build {latest} from {url}")
    zip_file = open(ZIP_FILE, "wb")

    # https://sumit-ghosh.com/articles/python-download-progress-bar/
    response = requests.get(f"{url}/{latest}", stream=True)
    total = response.headers.get('content-length')
    h = hashlib.new('sha256')
    if total is None:
        h.update(response.content)
        zip_file.write(response.content)
    else:
        downloaded = 0
        total = int(total)
        for data in response.iter_content(chunk_size=max(int(total / 1000), 1024 * 1024)):
            downloaded += len(data)
            h.update(data)
            zip_file.write(data)
            done = int(50 * downloaded / total)
            sys.stdout.write('\r[{}{}]'.format('█' * done, '.' * (50 - done)))
            sys.stdout.flush()
    sys.stdout.write('\n')
    checksum = h.hexdigest()
    return (ZIP_FILE, checksum)

def main(args):
    parser = argparse.ArgumentParser()
    type_group = parser.add_mutually_exclusive_group()
    type_group.add_argument("--debug", help="Download a debug build.",
                            dest='build_type', action="store_const", const="Debug")
    type_group.add_argument("--release", help="Download a release build.",
                            dest='build_type', action="store_const", const="Release")

    if len(args) == 0:
        parser.print_help(sys.stderr)
        return 1

    parsed, _ = parser.parse_known_args(args=args)
    zip_filename, checksum = download_zipped_build(parsed.build_type.lower())
    if not zip_filename:
        return 2

    manifest_path = "org.gnome.Epiphany.Canary.json"
    with open(f"{manifest_path}.in") as fd_in:
        json_input = json.load(fd_in)
        pwd = os.path.abspath(os.curdir)
        for module in json_input['modules']:
            if module['name'] == 'webkitgtk':
                path = os.path.join(pwd, zip_filename)
                module['sources'] = [{'type': 'archive', 'url': f'file://{path}', 'sha256': checksum,
                                      'strip-components': 0}]
            elif module['name'] == 'epiphany':
                module['sources'] = [{'type': 'dir', 'path': pwd}]
        with open(manifest_path, 'w') as fd_out:
            json.dump(json_input, fd_out, indent=4)

    return 0

if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
