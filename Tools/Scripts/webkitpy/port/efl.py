# Copyright (C) 2011 ProFUSION Embedded Systems. All rights reserved.
# Copyright (C) 2011 Samsung Electronics. All rights reserved.
# Copyright (C) 2012 Intel Corporation
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

"""WebKit Efl implementation of the Port interface."""

import os

from webkitpy.common.system import path
from webkitpy.layout_tests.models.test_configuration import TestConfiguration
from webkitpy.port.base import Port
from webkitpy.port.pulseaudio_sanitizer import PulseAudioSanitizer
from webkitpy.port.xorgdriver import XorgDriver
from webkitpy.port.xvfbdriver import XvfbDriver
from webkitpy.port.linux_get_crash_log import GDBCrashLogGenerator


class EflPort(Port):
    port_name = 'efl'

    def __init__(self, *args, **kwargs):
        super(EflPort, self).__init__(*args, **kwargs)

        self._jhbuild_wrapper = [self.path_from_webkit_base('Tools', 'jhbuild', 'jhbuild-wrapper'), '--efl', 'run']

        self.set_option_default('wrapper', ' '.join(self._jhbuild_wrapper))
        self.webprocess_cmd_prefix = self.get_option('webprocess_cmd_prefix')

        self._pulseaudio_sanitizer = PulseAudioSanitizer()

    def _port_flag_for_scripts(self):
        return "--efl"

    def setup_test_run(self):
        super(EflPort, self).setup_test_run()
        self._pulseaudio_sanitizer.unload_pulseaudio_module()

    def setup_environ_for_server(self, server_name=None):
        env = super(EflPort, self).setup_environ_for_server(server_name)

        # If DISPLAY environment variable is unset in the system
        # e.g. on build bot, remove DISPLAY variable from the dictionary
        if not 'DISPLAY' in os.environ:
            del env['DISPLAY']

        if 'ACCESSIBILITY_EAIL_LIBRARY_PATH' in os.environ:
            env['ACCESSIBILITY_EAIL_LIBRARY_PATH'] = os.environ['ACCESSIBILITY_EAIL_LIBRARY_PATH']

        env['TEST_RUNNER_INJECTED_BUNDLE_FILENAME'] = self._build_path('lib', 'libTestRunnerInjectedBundle.so')
        env['TEST_RUNNER_PLUGIN_PATH'] = self._build_path('lib', 'plugins')

        # Silence GIO warnings about using the "memory" GSettings backend.
        env['GSETTINGS_BACKEND'] = 'memory'

        if self.webprocess_cmd_prefix:
            env['WEB_PROCESS_CMD_PREFIX'] = self.webprocess_cmd_prefix

        return env

    def supports_per_test_timeout(self):
        return True

    def default_timeout_ms(self):
        # Tests run considerably slower under gdb
        # or valgrind.
        if self.get_option('webprocess_cmd_prefix'):
            return 350 * 1000
        return super(EflPort, self).default_timeout_ms()

    def clean_up_test_run(self):
        super(EflPort, self).clean_up_test_run()
        self._pulseaudio_sanitizer.restore_pulseaudio_module()

    def _generate_all_test_configurations(self):
        return [TestConfiguration(version=self._version, architecture='x86', build_type=build_type) for build_type in self.ALL_BUILD_TYPES]

    def _driver_class(self):
        if os.environ.get("USE_NATIVE_XDISPLAY"):
            return XorgDriver
        return XvfbDriver

    def _path_to_driver(self):
        return self._build_path('bin', self.driver_name())

    def _path_to_image_diff(self):
        return self._build_path('bin', 'ImageDiff')

    def _image_diff_command(self, *args, **kwargs):
        return self._jhbuild_wrapper + super(EflPort, self)._image_diff_command(*args, **kwargs)

    def _path_to_webcore_library(self):
        static_path = self._build_path('lib', 'libwebcore_efl.a')
        dyn_path = self._build_path('lib', 'libwebcore_efl.so')
        return static_path if self._filesystem.exists(static_path) else dyn_path

    def _search_paths(self):
        search_paths = []
        search_paths.append(self.port_name)
        search_paths.append('wk2')
        return search_paths

    def default_baseline_search_path(self):
        return map(self._webkit_baseline_path, self._search_paths())

    def _port_specific_expectations_files(self):
        # FIXME: We should be able to use the default algorithm here.
        return list(reversed([self._filesystem.join(self._webkit_baseline_path(p), 'TestExpectations') for p in self._search_paths()]))

    def show_results_html_file(self, results_filename):
        self._run_script("run-minibrowser", [path.abspath_to_uri(self.host.platform, results_filename)])

    def check_sys_deps(self, needs_http):
        return super(EflPort, self).check_sys_deps(needs_http) and self._driver_class().check_driver(self)

    def build_webkit_command(self, build_style=None):
        command = super(EflPort, self).build_webkit_command(build_style)
        command.extend(["--efl", "--update-efl"])
        command.append(super(EflPort, self).make_args())
        return command

    def _get_crash_log(self, name, pid, stdout, stderr, newer_than):
        return GDBCrashLogGenerator(name, pid, newer_than, self._filesystem, self.path_to_script("process-linux-coredump"), self._path_to_driver).generate_crash_log(stdout, stderr)

    def test_expectations_file_position(self):
        # EFL port baseline search path is efl -> wk2 -> generic (as efl-wk2 and efl baselines are merged), so port test expectations file is at third to last position.
        return 2
