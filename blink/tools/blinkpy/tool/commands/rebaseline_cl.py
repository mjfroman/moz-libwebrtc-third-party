# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""A command to fetch new baselines from try jobs for the current CL."""

import collections
import itertools
import json
import logging
import optparse
import re

from blinkpy.common.net.git_cl import GitCL, TryJobStatus
from blinkpy.common.net.rpc import Build, RPCError
from blinkpy.common.path_finder import PathFinder
from blinkpy.tool.commands.build_resolver import (
    BuildResolver,
    UnresolvedBuildException,
)
from blinkpy.tool.commands.command import check_file_option
from blinkpy.tool.commands.rebaseline import AbstractParallelRebaselineCommand
from blinkpy.tool.commands.rebaseline import TestBaselineSet

_log = logging.getLogger(__name__)


class RebaselineCL(AbstractParallelRebaselineCommand):
    name = 'rebaseline-cl'
    help_text = 'Fetches new baselines for a CL from test runs on try bots.'
    long_help = (
        'This command downloads new baselines for failing web '
        'tests from archived try job test results. Cross-platform '
        'baselines are deduplicated after downloading.  Without '
        'positional parameters or --test-name-file, all failing tests '
        'are rebaselined. If positional parameters are provided, '
        'they are interpreted as test names to rebaseline.')

    show_in_main_help = True
    argument_names = '[testname,...]'

    only_changed_tests_option = optparse.make_option(
        '--only-changed-tests',
        action='store_true',
        default=False,
        help='Only update files for tests directly modified in the CL.')
    no_trigger_jobs_option = optparse.make_option(
        '--no-trigger-jobs',
        dest='trigger_jobs',
        action='store_false',
        default=True,
        help='Do not trigger any try jobs.')
    test_name_file_option = optparse.make_option(
        '--test-name-file',
        action='callback',
        callback=check_file_option,
        type='string',
        help=('Read names of tests to update from this file, '
              'one test per line.'))
    patchset_option = optparse.make_option(
        '--patchset',
        default=None,
        type='int',
        help='Patchset number to fetch results from.')

    def __init__(self):
        super(RebaselineCL, self).__init__(options=[
            self.only_changed_tests_option,
            self.no_trigger_jobs_option,
            optparse.make_option(
                '--fill-missing',
                dest='fill_missing',
                action='store_true',
                default=None,
                help='If some platforms have no try job results, use results '
                'from try job results of other platforms.'),
            optparse.make_option('--no-fill-missing',
                                 dest='fill_missing',
                                 action='store_false'),
            optparse.make_option(
                '--use-blink-try-bots-only',
                dest='use_blink_try_bots_only',
                action='store_true',
                default=False,
                help='Use only the try jobs results for rebaselining. '
                'Default behavior is to use results from both CQ builders '
                'and try bots.'),
            self.test_name_file_option,
            optparse.make_option(
                '--builders',
                default=None,
                action='append',
                help=('Comma-separated-list of builders to pull new baselines '
                      'from (can also be provided multiple times).')),
            self.patchset_option,
            optparse.make_option('--resultDB',
                                 dest='resultDB',
                                 default=False,
                                 action='store_true',
                                 help=('Fetch results from resultDB(WIP). '
                                       'Works with --test-name-file '
                                       'and positional parameters')),
            self.no_optimize_option,
            self.dry_run_option,
            self.results_directory_option,
        ])
        self.git_cl = None
        self._use_blink_try_bots_only = False
        self._builders = []
        self._resultdb_fetcher = False

    def execute(self, options, args, tool):
        self._tool = tool
        self._dry_run = options.dry_run
        self._resultdb_fetcher = options.resultDB
        self.git_cl = self.git_cl or GitCL(tool)
        # '--dry-run' implies '--no-trigger-jobs'.
        options.trigger_jobs = options.trigger_jobs and not self._dry_run
        if args and options.test_name_file:
            _log.error('Aborted: Cannot combine --test-name-file and '
                       'positional parameters.')
            return 1

        if not self.check_ok_to_run():
            return 1

        self._use_blink_try_bots_only = options.use_blink_try_bots_only
        self._builders = options.builders

        build_resolver = BuildResolver(
            self._tool.builders,
            self.git_cl,
            can_trigger_jobs=(options.trigger_jobs and not self._dry_run))
        builds = [Build(builder) for builder in self.selected_try_bots]
        try:
            jobs = build_resolver.resolve_builds(builds, options.patchset)
        except RPCError as error:
            _log.error('%s', error)
            _log.error('Request payload: %s',
                       json.dumps(error.request_body, indent=2))
            return 1
        except UnresolvedBuildException as error:
            _log.error('%s', error)
            return 1

        jobs_to_results = self._fetch_results(jobs)
        builders_with_results = {b.builder_name for b in jobs_to_results}
        builders_without_results = (
            set(self.selected_try_bots) - builders_with_results)
        interrupted_builders = self._remove_interrupted_builders(
            jobs_to_results)
        if builders_without_results:
            _log.warning('There are some builders with no results:')
            for builder in sorted(builders_without_results):
                _log.warning('  %s', builder)
        if interrupted_builders:
            _log.warning('There are some builders that were interrupted.')
            _log.warning('Some shards may have timed out or exited early '
                         'due to excessive unexpected failures:')
            for builder in sorted(interrupted_builders):
                _log.warning('  %s', builder)

        incomplete_builders = builders_without_results | interrupted_builders
        if options.fill_missing is None and incomplete_builders:
            should_continue = self._tool.user.confirm(
                'Would you like to continue?',
                default=self._tool.user.DEFAULT_NO)
            if not should_continue:
                _log.info('Aborting.')
                return 1
            options.fill_missing = self._tool.user.confirm(
                'Would you like to try to fill in missing results with '
                'available results?\n'
                'Note: This is generally not suggested unless the results '
                'are platform agnostic.',
                default=self._tool.user.DEFAULT_NO)
            if not options.fill_missing:
                _log.info('Please rebaseline again for builders '
                          'with incomplete results later.')

        if options.test_name_file:
            test_baseline_set = self._make_test_baseline_set_from_file(
                options.test_name_file, jobs_to_results)
        elif args:
            test_baseline_set = self._make_test_baseline_set_for_tests(
                args, jobs_to_results)
        else:
            test_baseline_set = self._make_test_baseline_set(
                jobs_to_results, options.only_changed_tests)

        if options.fill_missing:
            self.fill_in_missing_results(test_baseline_set)

        self.rebaseline(options, test_baseline_set)
        return 0

    def _remove_interrupted_builders(self, jobs_to_results):
        interrupted_builders = set()
        # Iterate over a shallow copy of `items()`, which is a view of a
        # dictionary being mutated.
        for build, step_results in list(jobs_to_results.items()):
            if any(step_result.interrupted for step_result in step_results):
                interrupted_builders.add(build.builder_name)
                del jobs_to_results[build]
        return interrupted_builders

    def check_ok_to_run(self):
        unstaged_baselines = self.unstaged_baselines()
        if unstaged_baselines:
            _log.error('Aborting: there are unstaged baselines:')
            for path in unstaged_baselines:
                _log.error('  %s', path)
            return False
        return True

    @property
    def selected_try_bots(self):
        try_builders = set()
        if self._builders:
            for builder_names in self._builders:
                try_builders.update(builder_names.split(','))
        else:
            try_builders = frozenset(
                self._tool.builders.filter_builders(
                    is_try=True, exclude_specifiers={'android'}))

        if self._use_blink_try_bots_only:
            try_builders = try_builders - self.cq_try_bots
        elif not self._builders:
            # User did not specify builders and --use-blink-try-bots-only in
            # command line. Trigger default set of builders in this case, that
            # is CQ builders plus blink-rel builders that covers additional platforms.
            # Running duplicated builders for the same platform wastes resource, and
            # causes problem to rebaseline as we will randomly choose a builder later.
            to_remove = set()
            for try_builder, cq_builder in self.try_bots_with_cq_mirror:
                if (try_builder in try_builders
                        and cq_builder in try_builders):
                    to_remove.add(try_builder)
            try_builders = try_builders - to_remove

        return set([
            builder for builder in try_builders
            if not self._tool.builders.is_wpt_builder(builder)
        ])

    @property
    def cq_try_bots(self):
        return frozenset(self._tool.builders.all_cq_try_builder_names())

    @property
    def try_bots_with_cq_mirror(self):
        return self._tool.builders.try_bots_with_cq_mirror()

    def _fetch_results(self, jobs):
        """Fetches results for all of the given builds.

        There should be a one-to-one correspondence between Builds, supported
        platforms, and try bots. If not all of the builds can be fetched, then
        continuing with rebaselining may yield incorrect results, when the new
        baselines are deduped, an old baseline may be kept for the platform
        that's missing results.

        Args:
            jobs: A dict mapping Build objects to TryJobStatus objects.

        Returns:
            A dict mapping Builds to lists of WebTestResults for all completed
            jobs.
        """
        results_fetcher = self._tool.results_fetcher
        builds_to_results = collections.defaultdict(list)

        for build, status in jobs.items():
            if status == TryJobStatus('COMPLETED', 'SUCCESS'):
                _log.debug('No baselines to download for passing %r build %s.',
                           build.builder_name, build.build_number
                           or '(unknown)')
                # This empty entry indicates the builder is not missing.
                builds_to_results[build] = []
                continue
            if status != TryJobStatus('COMPLETED', 'FAILURE'):
                # Only completed failed builds will contain actual failed
                # web tests to download baselines for.
                continue

            step_names = results_fetcher.get_layout_test_step_names(build)
            unavailable_step_names = []
            if self._resultdb_fetcher:
                maybe_results = results_fetcher.fetch_results_from_resultdb_layout_tests(
                    build, True)
                if maybe_results:
                    builds_to_results[build].append(maybe_results)
                else:
                    # The results don't have step-level granularity, so just
                    # log all of them.
                    unavailable_step_names.extend(step_names)
            else:
                for step_name in step_names:
                    maybe_result = results_fetcher.fetch_results(
                        build, False, step_name)
                    if maybe_result:
                        builds_to_results[build].append(maybe_result)
                    else:
                        unavailable_step_names.append(step_name)

            if unavailable_step_names:
                _log.warning('Failed to fetch some results for "%s".',
                             build.builder_name)
                for step_name in unavailable_step_names:
                    results_url = results_fetcher.results_url(
                        build.builder_name, build.build_number, step_name)
                    _log.warning('Results URL: %s/results.html', results_url)
        return builds_to_results

    def _make_test_baseline_set_from_file(self, filename, builds_to_results):
        tests = []
        try:
            with self._tool.filesystem.open_text_file_for_reading(
                    filename) as fh:
                _log.info('Reading list of tests to rebaseline '
                          'from %s', filename)
                for test in fh.readlines():
                    test = test.strip()
                    if not test or test.startswith('#'):
                        continue
                    tests.append(test)
        except IOError:
            _log.info('Could not read test names from %s', filename)
        return self._make_test_baseline_set_for_tests(tests, builds_to_results)

    def _test_exists(self, results, test):
        if self._resultdb_fetcher:
            return results.fail_result_exists_resultdb(test)
        return results.result_for_test(test)

    def _make_test_baseline_set_for_tests(self, tests, builds_to_results):
        """Determines the set of test baselines to fetch from a list of tests.

        Args:
            tests: A list of tests.
            builds_to_results: A dict mapping Builds to lists of WebTestResults.

        Returns:
            A TestBaselineSet object.
        """
        test_baseline_set = TestBaselineSet(self._tool)
        for test, (build, builder_results) in itertools.product(
                tests, builds_to_results.items()):
            for step_results in builder_results:
                # Check for bad user-supplied test names early to create a
                # smaller test baseline set and send fewer bad requests.
                if self._test_exists(step_results, test):
                    test_baseline_set.add(test, build,
                                          step_results.step_name())
        return test_baseline_set

    def _make_test_baseline_set(self, builds_to_results, only_changed_tests):
        """Determines the set of test baselines to fetch.

        The list of tests are not explicitly provided, so all failing tests or
        modified tests will be rebaselined (depending on only_changed_tests).

        Args:
            builds_to_results: A dict mapping Builds to lists of WebTestResults.
            only_changed_tests: Whether to only include baselines for tests that
               are changed in this CL. If False, all new baselines for failing
               tests will be downloaded, even for tests that were not modified.

        Returns:
            A TestBaselineSet object.
        """
        if only_changed_tests:
            files_in_cl = self._tool.git().changed_files(diff_filter='AM')
            # In the changed files list from Git, paths always use "/" as
            # the path separator, and they're always relative to repo root.
            test_base = self._test_base_path()
            tests_in_cl = {
                f[len(test_base):]
                for f in files_in_cl if f.startswith(test_base)
            }

        test_baseline_set = TestBaselineSet(self._tool, prefix_mode=False)
        for build, builder_results in builds_to_results.items():
            for step_results in builder_results:
                if self._resultdb_fetcher:
                    tests_to_rebaseline = self._tests_to_rebaseline_resultDB(
                        build, step_results)
                else:
                    tests_to_rebaseline = self._tests_to_rebaseline(
                        build, step_results)
                # Here we have a concrete list of tests so we don't need prefix lookup.
                for test in tests_to_rebaseline:
                    if only_changed_tests and test not in tests_in_cl:
                        continue
                    test_baseline_set.add(test, build,
                                          step_results.step_name())
        return test_baseline_set

    def _test_base_path(self):
        """Returns the relative path from the repo root to the web tests."""
        finder = PathFinder(self._tool.filesystem)
        return self._tool.filesystem.relpath(
            finder.web_tests_dir(), finder.path_from_chromium_base()) + '/'

    def _tests_to_rebaseline_resultDB(self, build, web_test_results):
        """Fetches a list of tests that should be rebaselined for some build.

        Args:
            build: A Build instance.
            web_test_results: A WebTestResults instance or None.

        Returns:
            A sorted list of tests to rebaseline for this build.
        """
        if web_test_results is None:
            return []

        failed_tests = web_test_results.failed_unexpected_resultdb()
        failed_test_names = [
            r['testId'][len('ninja://:blink_web_tests') + 1:]
            for r in failed_tests
        ]

        return failed_test_names

    def _tests_to_rebaseline(self, build, web_test_results):
        """Fetches a list of tests that should be rebaselined for some build.

        Args:
            build: A Build instance.
            web_test_results: A WebTestResults instance.

        Returns:
            A sorted list of tests to rebaseline for this build.
        """
        unexpected_results = web_test_results.didnt_run_as_expected_results()
        tests = sorted(
            r.test_name() for r in unexpected_results
            if r.is_missing_baseline() or r.has_non_reftest_mismatch())
        if not tests:
            # no need to fetch retry summary in this case
            return []

        test_suite = re.sub('\s*\(.*\)$', '', web_test_results.step_name())
        new_failures = self._fetch_tests_with_new_failures(build, test_suite)
        if new_failures is None:
            _log.warning('No retry summary available for ("%s", "%s").',
                         build.builder_name, test_suite)
        else:
            tests = [t for t in tests if t in new_failures]
        return tests

    def _fetch_tests_with_new_failures(self, build, test_suite):
        """For a given test suite in the try job, lists tests that only failed
        with the patch.

        If a test failed only with the patch but not without, then that
        indicates that the failure is actually related to the patch and
        is not failing at HEAD.

        If the list of new failures could not be obtained, this returns None.
        """
        results_fetcher = self._tool.results_fetcher
        content = results_fetcher.fetch_retry_summary_json(build, test_suite)
        if content is None:
            return None
        try:
            retry_summary = json.loads(content)
            return set(retry_summary['failures'])
        except (ValueError, KeyError):
            _log.warning('Unexpected retry summary content:\n%s', content)
            return None

    def fill_in_missing_results(self, test_baseline_set):
        """Adds entries, filling in results for missing jobs.

        For each test prefix, if there is an entry missing for some port,
        then an entry should be added for that port using a build that is
        available.

        For example, if there's no entry for the port "win-win10", but there
        is an entry for the "win-win11" port, then an entry might be added
        for "win-win10" using the results from "win-win11".
        """
        all_ports = {
            self._tool.builders.port_name_for_builder_name(b)
            for b in self.selected_try_bots
        }
        for test_prefix in test_baseline_set.test_prefixes():
            build_port_pairs = test_baseline_set.build_port_pairs(test_prefix)
            missing_ports = all_ports - {p for _, p in build_port_pairs}
            if not missing_ports:
                continue
            _log.info('For %s:', test_prefix)
            for port in sorted(missing_ports):
                build = self._choose_fill_in_build(port, build_port_pairs)
                _log.info('Using "%s" build %d for %s.', build.builder_name,
                          build.build_number, port)
                test_baseline_set.add(test_prefix, build, port_name=port)
        return test_baseline_set

    def _choose_fill_in_build(self, target_port, build_port_pairs):
        """Returns a Build to use to supply results for the given port.

        Ideally, this should return a build for a similar port so that the
        results from the selected build may also be correct for the target port.
        """

        # A full port name should normally always be of the form <os>-<version>;
        # for example "win-win11", or "linux-trusty". For the test port used in
        # unit tests, though, the full port name may be "test-<os>-<version>".
        def os_name(port):
            if '-' not in port:
                return port
            return port[:port.rfind('-')]

        # If any Build exists with the same OS, use the first one.
        target_os = os_name(target_port)
        same_os_builds = sorted(
            b for b, p in build_port_pairs if os_name(p) == target_os)
        if same_os_builds:
            return same_os_builds[0]

        # Otherwise, perhaps any build will do, for example if the results are
        # the same on all platforms. In this case, just return the first build.
        return sorted(build_port_pairs)[0][0]
