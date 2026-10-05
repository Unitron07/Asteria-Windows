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
        sync.git('init', '-b', 'main')
        sync.git('config', 'user.name', 'Test')
        sync.git('config', 'user.email', 'test@example.invalid')
        Path('shared').write_text('original\n')
        sync.git('add', '.')
        sync.git('commit', '-m', 'base')
        self.base = sync.git('rev-parse', 'HEAD').stdout.strip()

    def tearDown(self):
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
        with patch.object(sync, 'find_pr', return_value=[{'number': 12}]), patch.object(sync, 'run') as run:
            sync.publish_pr(Path('body.md'))
            self.assertEqual(run.call_args.args[:4], ('gh', 'pr', 'edit', '12'))

    def test_missing_pr_is_created_once(self):
        with patch.object(sync, 'find_pr', return_value=[]), patch.object(sync, 'run') as run:
            sync.publish_pr(Path('body.md'))
            self.assertEqual(run.call_args.args[:3], ('gh', 'pr', 'create'))
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


if __name__ == '__main__':
    unittest.main()
