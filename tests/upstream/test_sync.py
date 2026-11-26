import importlib.util
import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch

spec = importlib.util.spec_from_file_location('sync', Path(__file__).parents[2] / 'scripts/sync-moonlight.py')
sync = importlib.util.module_from_spec(spec)
spec.loader.exec_module(sync)


class SyncTests(unittest.TestCase):
    def setUp(self):
        self.original = Path.cwd()
        self.temp = tempfile.TemporaryDirectory()
        os.chdir(self.temp.name)
        # GitHub sets this variable even for the test step. Keep summaries in
        # the isolated fixture rather than writing into the runner's live summary.
        self.summary_env = patch.dict(os.environ, {
            'GITHUB_STEP_SUMMARY': str(Path(self.temp.name) / 'build/upstream-summary.md')})
        self.summary_env.start()
        sync.git('init', '-b', 'main')
        sync.git('config', 'user.name', 'Test')
        sync.git('config', 'user.email', 'test@example.invalid')
        Path('shared').write_text('original\n')
        sync.git('add', '.')
        sync.git('commit', '-m', 'base')
        self.base = sync.git('rev-parse', 'HEAD').stdout.strip()

    def tearDown(self):
        self.summary_env.stop()
        os.chdir(self.original)
        self.temp.cleanup()

    def change(self, filename, text):
        Path(filename).write_text(text)
        sync.git('add', filename)
        sync.git('commit', '-m', filename)

    def test_clean_merge_preserves_both_histories(self):
        sync.git('switch', '-c', 'upstream')
        self.change('upstream-feature', 'upstream\n')
        upstream = sync.git('rev-parse', 'HEAD').stdout.strip()
        sync.git('switch', 'main')
        self.change('asteria-feature', 'asteria\n')
        self.assertEqual(sync.merge(upstream), [])
        sync.git('commit', '-m', 'sync')
        self.assertEqual(sync.git('merge-base', '--is-ancestor', upstream, 'HEAD').returncode, 0)
        self.assertTrue(Path('asteria-feature').exists())
        self.assertTrue(Path('upstream-feature').exists())
        self.assertEqual(len(sync.git('rev-list', '--parents', '-1', 'HEAD').stdout.split()), 3)

    def test_conflict_aborts_without_overwriting_asteria(self):
        sync.git('switch', '-c', 'upstream')
        self.change('shared', 'upstream\n')
        sync.git('switch', 'main')
        self.change('shared', 'asteria\n')
        before = sync.git('rev-parse', 'HEAD').stdout
        self.assertEqual(sync.merge('upstream'), ['shared'])
        self.assertEqual(Path('shared').read_text(), 'asteria\n')
        self.assertEqual(sync.git('rev-parse', 'HEAD').stdout, before)
        self.assertEqual(sync.git('status', '--porcelain').stdout, '')

    def test_existing_pr_is_updated(self):
        Path('body.md').write_text('Review this range.\n')
        with patch.object(sync, 'find_pr', return_value=[{'number': 12}]), patch.object(sync, 'run') as run:
            sync.publish_pr(Path('body.md'))
            self.assertEqual(run.call_args.args[:5], ('gh', 'api', '--method', 'PATCH', 'repos/{owner}/{repo}/pulls/12'))
            self.assertEqual(json.loads(Path('body.json').read_text()), {'body': 'Review this range.\n'})

    def test_missing_pr_is_created_once(self):
        Path('body.md').write_text('Review this range.\n')
        with patch.object(sync, 'find_pr', return_value=[]), patch.object(sync, 'run') as run:
            sync.publish_pr(Path('body.md'))
            self.assertEqual(run.call_args.args[:5], ('gh', 'api', '--method', 'POST', 'repos/{owner}/{repo}/pulls'))
            self.assertEqual(json.loads(Path('body.json').read_text())['head'], sync.BRANCH)
            self.assertEqual(run.call_count, 1)

    def test_no_upstream_change_exits_without_publication(self):
        Path('.upstream').mkdir()
        Path(sync.RECORD).write_text(json.dumps({'repository': 'moonlight-stream/moonlight-qt',
                                              'branch': 'master', 'commit': self.base}))
        real_git = sync.git
        def fake_git(*args, **kwargs):
            if args[0] == 'fetch':
                return subprocess.CompletedProcess(args, 0, '', '')
            if args == ('rev-parse', 'moonlight/master'):
                return subprocess.CompletedProcess(args, 0, self.base, '')
            return real_git(*args, **kwargs)
        with patch.object(sync, 'git', side_effect=fake_git), patch.object(sync, 'publish_pr') as publish:
            sync.main()
            publish.assert_not_called()
        self.assertIn('unchanged', Path('build/upstream-summary.md').read_text())

    def test_clean_proposal_updates_baseline_and_reuses_remote_branch(self):
        # Exercise the complete Git path against a local bare origin, without GitHub writes.
        sync.git('switch', '-c', 'upstream')
        self.change('upstream-feature', 'upstream\n')
        new = sync.git('rev-parse', 'HEAD').stdout.strip()
        sync.git('switch', 'main')
        Path('.upstream').mkdir()
        Path(sync.RECORD).write_text(json.dumps({'repository': 'moonlight-stream/moonlight-qt',
                                              'branch': 'master', 'ref': 'v6.2.0', 'commit': self.base}))
        sync.git('add', sync.RECORD)
        sync.git('commit', '-m', 'Asteria baseline')
        Path('build').mkdir()
        sync.git('init', '--bare', 'build/origin.git')
        sync.git('remote', 'add', 'origin', str(Path('build/origin.git').resolve()))
        sync.git('update-ref', 'refs/remotes/moonlight/master', new)
        real_git = sync.git
        def fake_git(*args, **kwargs):
            if args[:2] == ('fetch', '--no-tags'):
                return subprocess.CompletedProcess(args, 0, '', '')
            return real_git(*args, **kwargs)
        with patch.object(sync, 'git', side_effect=fake_git), patch.object(sync, 'validate') as validate, \
                patch.object(sync, 'publish_pr') as publish:
            sync.main()
            self.assertEqual(json.loads(Path(sync.RECORD).read_text())['commit'], new)
            self.assertEqual(len(sync.git('rev-list', '--parents', '-1', 'HEAD').stdout.split()), 3)
            tip = sync.git('rev-parse', 'HEAD').stdout.strip()
            validate.assert_called_once()
            sync.git('switch', 'main')
            sync.git('branch', '-D', sync.BRANCH)
            sync.main()
            self.assertEqual(sync.git('ls-remote', 'origin', f'refs/heads/{sync.BRANCH}').stdout.split()[0], tip)
            self.assertEqual(validate.call_count, 1)
            self.assertEqual(publish.call_count, 2)

    def proposal_fixture(self):
        """A main branch with a recorded baseline, one upstream commit and a local bare origin."""
        sync.git('switch', '-c', 'upstream')
        self.change('upstream-feature', 'upstream\n')
        new = sync.git('rev-parse', 'HEAD').stdout.strip()
        sync.git('switch', 'main')
        Path('.upstream').mkdir()
        Path(sync.RECORD).write_text(json.dumps({'repository': 'moonlight-stream/moonlight-qt',
                                              'branch': 'master', 'ref': 'v6.2.0', 'commit': self.base}))
        sync.git('add', sync.RECORD)
        sync.git('commit', '-m', 'Asteria baseline')
        Path('build').mkdir()
        sync.git('init', '--bare', 'build/origin.git')
        sync.git('remote', 'add', 'origin', str(Path('build/origin.git').resolve()))
        sync.git('update-ref', 'refs/remotes/moonlight/master', new)
        return new

    def run_with_push(self, push_result):
        real_git = sync.git
        def fake_git(*args, **kwargs):
            if args[:2] == ('fetch', '--no-tags'):
                return subprocess.CompletedProcess(args, 0, '', '')
            if args[0] == 'push':
                return push_result(args)
            return real_git(*args, **kwargs)
        with patch.object(sync, 'git', side_effect=fake_git), patch.object(sync, 'validate'), \
                patch.object(sync, 'publish_pr') as publish, patch.object(sync, 'notice') as notice:
            try:
                sync.main()
                error = None
            except RuntimeError as exc:
                error = exc
        return error, publish, notice

    def test_rejected_workflow_push_is_reported_with_token_setup(self):
        # GitHub refuses GITHUB_TOKEN pushes that change workflows. That must reach the
        # maintenance issue with the fix, not end the job silently (run 45724209525).
        self.proposal_fixture()
        rejection = ('To https://github.com/Unitron07/Asteria-Windows\n ! [remote rejected] HEAD -> '
                     f'{sync.BRANCH} (refusing to allow a GitHub App to create or update workflow '
                     '`.github/workflows/build-appimage.yml` without `workflows` permission)\n')
        error, publish, notice = self.run_with_push(
            lambda args: subprocess.CompletedProcess(args, 1, '', rejection))
        self.assertIsInstance(error, RuntimeError)
        publish.assert_not_called()
        notice.assert_called_once()
        reported = Path('build/upstream-notice.md').read_text(encoding='utf-8')
        self.assertIn('Validation passed, but the proposal branch could not be pushed', reported)
        self.assertIn('UPSTREAM_SYNC_TOKEN', reported)
        self.assertIn('build-appimage.yml', reported)
        self.assertEqual(Path('build/upstream-summary.md').read_text(encoding='utf-8'), reported)

    def test_other_push_failure_is_reported_without_token_hint(self):
        self.proposal_fixture()
        error, publish, notice = self.run_with_push(
            lambda args: subprocess.CompletedProcess(args, 1, '', 'stale info\n'))
        self.assertIsInstance(error, RuntimeError)
        publish.assert_not_called()
        notice.assert_called_once()
        reported = Path('build/upstream-notice.md').read_text(encoding='utf-8')
        self.assertIn('stale info', reported)
        self.assertNotIn('UPSTREAM_SYNC_TOKEN', reported)

    def test_ci_guidance_follows_token_configuration(self):
        self.proposal_fixture()
        ok = lambda args: subprocess.CompletedProcess(args, 0, '', '')
        with patch.dict(os.environ, {'UPSTREAM_SYNC_TOKEN_CONFIGURED': 'false'}):
            error, publish, _ = self.run_with_push(ok)
        self.assertIsNone(error)
        publish.assert_called_once()
        self.assertIn('manually dispatch', Path('build/upstream-notice.md').read_text(encoding='utf-8'))
        sync.git('switch', 'main')
        sync.git('branch', '-D', sync.BRANCH)
        with patch.dict(os.environ, {'UPSTREAM_SYNC_TOKEN_CONFIGURED': 'true'}):
            error, publish, _ = self.run_with_push(ok)
        self.assertIsNone(error)
        body = Path('build/upstream-notice.md').read_text(encoding='utf-8')
        self.assertIn('Normal PR workflows run', body)
        self.assertNotIn('manually dispatch', body)


if __name__ == '__main__':
    unittest.main()
