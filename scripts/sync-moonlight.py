"""Weekly proposal only: never resolve conflicts or merge a GitHub PR."""
import json
import os
from pathlib import Path
import subprocess

BRANCH = 'automation/moonlight-upstream-sync'
RECORD = '.upstream/moonlight.json'
ISSUE_TITLE = 'maintenance: Moonlight upstream integration requires review'


def run(*args, check=True):
    result = subprocess.run(args, text=True, encoding='utf-8', stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE)
    if check and result.returncode:
        raise RuntimeError(f'{args[0]} failed ({result.returncode}):\n{result.stdout}\n{result.stderr}')
    return result


def git(*args, check=True):
    return run('git', *args, check=check)


def merge(target):
    result = git('merge', '--no-ff', '--no-commit', target, check=False)
    if result.returncode:
        conflicts = git('diff', '--name-only', '--diff-filter=U').stdout.splitlines()
        git('merge', '--abort', check=False)
        if not conflicts:
            raise RuntimeError(result.stderr + result.stdout)
        return conflicts
    return []


def find_pr():
    return json.loads(run('gh', 'pr', 'list', '--state', 'open', '--base', 'main',
                          '--head', BRANCH, '--json', 'number').stdout)


def publish_pr(body_path):
    existing = find_pr()
    if existing:
        run('gh', 'pr', 'edit', str(existing[0]['number']), '--body-file', str(body_path))
    else:
        run('gh', 'pr', 'create', '--base', 'main', '--head', BRANCH,
            '--title', 'upstream: sync Moonlight PC', '--body-file', str(body_path))


def notice(body_path):
    # Enumerate open issues and match the exact stable title, avoiding search-index lag.
    issues = json.loads(run('gh', 'api', '--paginate', '--slurp',
                            'repos/{owner}/{repo}/issues?state=open&per_page=100').stdout)
    matches = [i for page in issues for i in page
               if 'pull_request' not in i and i['title'] == ISSUE_TITLE]
    if matches:
        run('gh', 'issue', 'edit', str(matches[0]['number']), '--body-file', str(body_path))
    else:
        run('gh', 'issue', 'create', '--title', ISSUE_TITLE, '--body-file', str(body_path))


def validate(base):
    git('diff', '--check', base)
    run('pwsh', '-NoProfile', '-File', 'scripts/apply-common-c-p1a.ps1')
    for test in ('baseline-preflight', 'package-architecture-tests',
                 'arm64-package-repair', 'pyrowave-package-tests'):
        run('pwsh', '-NoProfile', '-File', f'scripts/test-{test}.ps1')
    run('cmake', '-S', 'tests/pyrowave', '-B', 'build/upstream-parser', '-A', 'x64')
    run('cmake', '--build', 'build/upstream-parser', '--config', 'Release', '--parallel', '2')
    run('ctest', '--test-dir', 'build/upstream-parser', '-C', 'Release', '--output-on-failure')


def main():
    baseline = json.loads(Path(RECORD).read_text(encoding='utf-8'))
    if baseline['repository'] != 'moonlight-stream/moonlight-qt' or baseline['branch'] != 'master':
        raise RuntimeError('Unexpected upstream repository or branch')
    git('config', 'user.name', 'github-actions[bot]')
    git('config', 'user.email', '41898282+github-actions[bot]@users.noreply.github.com')
    git('fetch', '--no-tags', 'https://github.com/moonlight-stream/moonlight-qt.git',
        '+refs/heads/master:refs/remotes/moonlight/master')
    old, new = baseline['commit'], git('rev-parse', 'moonlight/master').stdout.strip()
    base = git('rev-parse', 'HEAD').stdout.strip()
    summary = Path(os.environ.get('GITHUB_STEP_SUMMARY', 'build/upstream-summary.md'))
    summary.parent.mkdir(parents=True, exist_ok=True)
    if old == new:
        summary.write_text('Moonlight upstream is unchanged.\n', encoding='utf-8')
        return
    if git('merge-base', '--is-ancestor', old, new, check=False).returncode:
        raise RuntimeError('Upstream no longer descends from the baseline; manual review required')
    if git('merge-base', '--is-ancestor', old, base, check=False).returncode:
        raise RuntimeError('Recorded baseline is not in Asteria history')
    count = git('rev-list', '--count', f'{old}..{new}').stdout.strip()
    body = (f'Upstream range: `{old}` → `{new}` ({count} commits).\n\n'
            f'[Compare](https://github.com/moonlight-stream/moonlight-qt/compare/{old}...{new})\n\n'
            'Human review is required for Asteria-specific behavior and semantic conflicts. '
            'This automation never auto-merges or chooses conflict sides.\n\n')
    body_path = Path('build/upstream-notice.md')
    body_path.parent.mkdir(parents=True, exist_ok=True)
    # Capture the remote SHA for an explicit lease; only the disposable automation branch is replaced.
    remote = git('ls-remote', '--heads', 'origin', f'refs/heads/{BRANCH}').stdout.split()
    remote_sha = remote[0] if remote else ''
    if remote_sha:
        git('fetch', 'origin', f'refs/heads/{BRANCH}')
        record = git('show', f'{remote_sha}:{RECORD}', check=False)
        if (record.returncode == 0 and json.loads(record.stdout)['commit'] == new
                and git('merge-base', '--is-ancestor', base, remote_sha, check=False).returncode == 0):
            # A validated proposal already includes current main and upstream. Repair a missing PR too.
            body_path.write_text(body + 'Validation: existing validated proposal reused.\n', encoding='utf-8')
            publish_pr(body_path)
            summary.write_text(body_path.read_text(encoding='utf-8'), encoding='utf-8')
            return
    git('switch', '-c', BRANCH)
    conflicts = merge(new)
    if conflicts:
        body_path.write_text(body + 'Merge aborted. Manual integration required. Conflicted files:\n\n'
                             + ''.join(f'- `{f}`\n' for f in conflicts), encoding='utf-8')
        notice(body_path)
        summary.write_text(body_path.read_text(encoding='utf-8'), encoding='utf-8')
        return
    try:
        baseline.update(ref='master', commit=new)
        Path(RECORD).write_text(json.dumps(baseline, indent=2) + '\n', encoding='utf-8')
        git('add', RECORD)
        git('commit', '-m', f'Merge Moonlight upstream {new[:12]} and record baseline')
        git('submodule', 'update', '--init', '--recursive')
        validate(base)
    except Exception as exc:
        body_path.write_text(body + f'Validation failed; no branch published.\n\n```\n{exc}\n```\n', encoding='utf-8')
        notice(body_path)
        summary.write_text(body_path.read_text(encoding='utf-8'), encoding='utf-8')
        raise
    body_path.write_text(body + 'Validation: diff whitespace, PowerShell preflight/package guards, '
                         'CMake configuration and GPU-free PyroWave regression tests passed.\n\n'
                         'Qt/qmake, settings UI, live input, runtime/GPU and full x64/ARM64 package '
                         'qualification were not run by this weekly job. GITHUB_TOKEN PRs do not '
                         'trigger normal PR workflows: manually dispatch the Windows baseline and '
                         'PyroWave workflows on this branch before merging.\n', encoding='utf-8')
    git('push', f'--force-with-lease=refs/heads/{BRANCH}:{remote_sha}', 'origin',
        f'HEAD:refs/heads/{BRANCH}')
    publish_pr(body_path)
    summary.write_text(body_path.read_text(encoding='utf-8'), encoding='utf-8')


if __name__ == '__main__':
    main()
