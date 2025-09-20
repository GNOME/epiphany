# -*- coding: utf-8 -*-
# Copyright (C) 2021-2024 Igalia S.L.
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

import argparse
import json
import os
import sys
import urllib.request
import urllib.parse

ARCHIVE_FILE = "webkitgtk.zip"

def download_nightly_build(verbose):
    url = "https://webkitgtk.org/built-products/x86_64/release/nightly/GNOMEWebCanary"
    with urllib.request.urlopen(f"{url}/LAST-IS") as fd:
        latest = urllib.parse.quote(fd.read().strip().decode('utf8'))

    print(f"Downloading build {latest} from {url}")
    archive = open(ARCHIVE_FILE, "wb")

    def update(blocks, bs, size):
        done = int(50 * blocks * bs / size)
        sys.stdout.write('\r[{}{}]'.format('█' * done, '.' * (50 - done)))
        sys.stdout.flush()

    args = []
    if verbose:
        args.append(update)

    archive_url = f"{url}/{latest}"
    urllib.request.urlretrieve(archive_url, ARCHIVE_FILE, *args)

    shasum_url = archive_url.replace('.zip', '.sha256sum')
    with urllib.request.urlopen(shasum_url) as fd:
        output = fd.read().strip()
        checksum = output.split(b' ')[0].decode('utf8')

    return (ARCHIVE_FILE, checksum)

def main(args):
    parser = argparse.ArgumentParser()
    parser.add_argument("--verbose", help="Show progress bar.", action=argparse.BooleanOptionalAction, default=False)

    parsed, _ = parser.parse_known_args(args=args)
    archive_filename, checksum = download_nightly_build(parsed.verbose)
    if not archive_filename:
        return 2

    manifest_path = "org.gnome.Epiphany.Canary.json"
    with open(f"{manifest_path}.in") as fd_in:
        json_input = json.load(fd_in)
        pwd = os.path.abspath(os.curdir)
        for module in json_input['modules']:
            if module['name'] == 'webkitgtk':
                path = os.path.join(pwd, archive_filename)
                module['sources'] = [{'type': 'archive', 'url': f'file://{path}', 'sha256': checksum,
                                      'strip-components': 0}]
            elif module['name'] == 'epiphany':
                module['sources'] = [{'type': 'dir', 'path': pwd}]
        with open(manifest_path, 'w') as fd_out:
            json.dump(json_input, fd_out, indent=4)

    return 0

if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
