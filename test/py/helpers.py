# SPDX-License-Identifier: MPL-2.0
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.

from os.path import join, realpath, dirname
from tempfile import TemporaryDirectory

CIMBAR_SRC = realpath(join(dirname(realpath(__file__)), '..', '..'))
BIN_DIR = join(CIMBAR_SRC, 'dist', 'bin')


class TestDirMixin():
    def setUp(self):
        self.working_dir = TemporaryDirectory()
        super().setUp()

    def tearDown(self):
        super().tearDown()
        with self.working_dir:
            pass