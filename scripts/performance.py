#!/usr/bin/env python3
"""Local-only schema validation and repeated-run analysis; standard library."""
import argparse
import json
import math
from pathlib import Path
import statistics

FIELDS = ('mean', 'p50', 'p95', 'p99', 'minimum', 'maximum')
CONTEXT = ('hostBuild', 'workloadId', 'networkType', 'displayMode', 'powerState', 'chargingState', 'tailscale', 'hostWorkload')

def number(value):
    return type(value) in (int, float) and math.isfinite(value) and value >= 0

def validate(run):
    if type(run) is not dict or type(run.get('schemaVersion')) is not int or run['schemaVersion'] != 1:
        raise ValueError('unsupported or missing schemaVersion (expected integer 1)')
    for name in ('application', 'buildKind', 'releaseBaseline', 'sourceRevision', 'architecture', 'gpu', 'backend'):
        if not isinstance(run.get(name), str) or not run[name]:
            raise ValueError(f'missing identity: {name}')
    if run['application'] != 'Asteria':
        raise ValueError('wrong application')
    if not isinstance(run.get('codec'), dict) or not isinstance(run.get('video'), dict):
        raise ValueError('missing codec/video context')
    for field in ('width', 'height', 'targetFps', 'requestedBitrateKbps'):
        if not number(run['video'].get(field)) or run['video'][field] <= 0:
            raise ValueError(f'invalid video.{field}')
    if any(type(run['video'][field]) is not int for field in ('width', 'height')):
        raise ValueError('video dimensions must be integers')
    if not all(isinstance(run['codec'].get(field),str) and run['codec'][field] for field in ('name','commit','bitstreamId','apiVersion')):
        raise ValueError('missing codec identity/pin')
    if type(run['video'].get('vsync')) is not bool:
        raise ValueError('missing video.vsync')
    for field in ('durationSeconds', 'requestedDurationSeconds', 'warmupSeconds'):
        if not number(run.get('capture', {}).get(field)):
            raise ValueError(f'invalid capture.{field}')
    if run['capture']['durationSeconds'] > run['capture']['requestedDurationSeconds']:
        raise ValueError('capture duration exceeds requested window')
    if not isinstance(run.get('limitations'), list) or not all(isinstance(x, str) for x in run['limitations']):
        raise ValueError('missing limitations')
    if not isinstance(run.get('counts'), dict) or not isinstance(run.get('metrics'), dict):
        raise ValueError('missing counts/metrics')
    for name, value in run['counts'].items():
        if value is None and run['capture'].get('counterOverflow'):
            continue
        if type(value) is not int or not 0 <= value <= 2**63-1:
            raise ValueError(f'invalid counter: {name}')
    for name, metric in run['metrics'].items():
        if not isinstance(metric, dict):
            raise ValueError(f'invalid metric: {name}')
        for field in ('unit', 'scope', 'clock', 'percentileMethod'):
            if not isinstance(metric.get(field), str) or not metric[field]:
                raise ValueError(f'{name}: missing {field}')
        count = metric.get('sampleCount')
        if not (count is None and metric.get('status')=='overflow') and (type(count) is not int or count < 0):
            raise ValueError(f'{name}: invalid sampleCount')
        status = metric.get('status')
        if status not in ('available', 'unavailable', 'overflow'):
            raise ValueError(f'{name}: invalid status')
        if any(field not in metric for field in FIELDS):
            raise ValueError(f'{name}: missing summary field')
        if status != 'available':
            if any(metric[field] is not None for field in FIELDS):
                raise ValueError(f'{name}: unavailable values must be null')
            if status == 'unavailable' and count != 0:
                raise ValueError(f'{name}: unavailable must have zero samples')
        else:
            if not count or not all(number(metric[field]) for field in FIELDS):
                raise ValueError(f'{name}: invalid available summary')
            low, high = metric['minimum'], metric['maximum']
            if not low <= metric['mean'] <= high or not low <= metric['p50'] <= metric['p95'] <= metric['p99'] <= high:
                raise ValueError(f'{name}: unordered statistics')
    for event in run.get('frameEvents', []):
        if event.get('stage') not in ('received', 'published', 'submitted', 'dropped', 'rejected'):
            raise ValueError('invalid frame event stage')
        if not number(event.get('relativeUs')) or type(event.get('frameNumber')) is not int or not 0 <= event['frameNumber'] < 2**32:
            raise ValueError('invalid frame event identity/time')
        if type(event.get('slot')) is not int or not -1 <= event['slot'] <= 2:
            raise ValueError('invalid GPU slot')
    if len(run.get('frameEvents', [])) > 512:
        raise ValueError('event storage bound exceeded')
    return run

def load(path):
    path = Path(path)
    if path.stat().st_size > 2*1024*1024:
        raise ValueError('capture exceeds 2 MiB safety bound')
    def reject_constant(value):
        raise ValueError(f'invalid JSON numeric constant: {value}')
    return validate(json.loads(path.read_text(encoding='utf-8-sig'), parse_constant=reject_constant))

def profile(run):
    fields={k: run.get(k) for k in ('architecture', 'gpu', 'driver', 'codec', 'video', 'benchmarkContext')}
    fields['capturePolicy']={k:run['capture'][k] for k in ('warmupSeconds','requestedDurationSeconds')}
    return json.dumps(fields,sort_keys=True)

def compatible(a, b):
    return all(a.get(k) == b.get(k) for k in ('unit', 'scope', 'clock', 'percentileMethod'))

def summarize(runs):
    if not runs:
        raise ValueError('no runs')
    identities = [r['runId'] for r in runs if r.get('runId')]
    if len(set(identities)) != len(identities):
        raise ValueError('multiple decoder lifetimes from one stream are not independent repeated runs; analyze separately')
    result = {}
    for name in sorted(set().union(*(r['metrics'] for r in runs))):
        present = [r['metrics'][name] for r in runs if name in r['metrics']]
        if any(not compatible(present[0], m) for m in present[1:]):
            raise ValueError(f'incompatible metric definitions within group: {name}')
        samples = [m for m in present if m['status'] == 'available']
        if not samples:
            result[name] = {'availableRuns': 0, 'pooledPercentiles': None}
            continue
        n = sum(m['sampleCount'] for m in samples)
        means = [m['mean'] for m in samples]
        result[name] = {'availableRuns': len(samples), 'totalRuns': len(runs), 'sampleCount': n,
            'unit': samples[0]['unit'], 'scope': samples[0]['scope'],
            'weightedMean': sum(m['mean']*m['sampleCount'] for m in samples)/n,
            'medianRunMean': statistics.median(means), 'minimumRunMean': min(means), 'maximumRunMean': max(means),
            'perRunPercentiles': [{k: m[k] for k in ('sampleCount', 'p50', 'p95', 'p99')} for m in samples],
            'pooledPercentiles': None}
    return result

def markdown(runs):
    groups = {}
    for run in runs:
        key = (run['sourceRevision'], run['backend'], run['buildKind'], profile(run))
        groups.setdefault(key, []).append(run)
    lines = ['# Asteria recorded performance runs', '',
        'All supplied runs are included. Percentiles remain per run; pooled percentiles are unavailable.',
        'Weighted means use sample counts. Median/min/max describe run means, not pooled frame tails.',
        'Submission and WSI call times do not measure display completion or end-to-end latency.', '']
    summaries = []
    for index, (key, members) in enumerate(groups.items(), 1):
        summary = summarize(members)
        summaries.append((key, members, summary))
        lines += [f'## Group {index}: {key[1]} / {key[2]}', '', f'Source: `{key[0]}`. Runs: {len(members)}.',
            f'Profile: `{key[3]}`', '', '| Metric | Runs | Samples | Weighted mean | Median run mean | Run mean min–max |',
            '| --- | ---: | ---: | ---: | ---: | --- |']
        tails = []
        for name, m in summary.items():
            if not m['availableRuns']:
                lines.append(f'| {name} | 0 | 0 | unavailable | unavailable | unavailable |')
                continue
            lines.append(f"| {name} ({m['unit']}) | {m['availableRuns']} | {m['sampleCount']} | {m['weightedMean']:.3f} | {m['medianRunMean']:.3f} | {m['minimumRunMean']:.3f}–{m['maximumRunMean']:.3f} |")
            tails.append(f"Per-run {name} tails (sample count, p50/p95/p99): " + '; '.join(f"{p['sampleCount']}: {p['p50']}/{p['p95']}/{p['p99']}" for p in m['perRunPercentiles']))
        lines += ['', *[tail+'\n' for tail in tails]]
        if any(r['capture']['durationSeconds']<r['capture']['requestedDurationSeconds'] for r in members):
            lines += ['', 'Partial capture: at least one stream ended before its requested window; benchmark qualification incomplete.']
        lines += ['', 'Limitations: ' + '; '.join(sorted(set().union(*(r['limitations'] for r in members)))), '']
    if len(summaries) > 1:
        base, base_runs, base_stats = summaries[0]
        lines += ['## Descriptive comparisons against first group', '', 'Negative mean deltas indicate lower CPU duration within the same measurement scope.', '']
        for key, members, summary in summaries[1:]:
            incomplete = any(any(c not in r.get('benchmarkContext', {}) for c in CONTEXT) or not r.get('driver') for r in base_runs+members)
            if key[3] != base[3]:
                lines.append(f'{key[1]}: NON-COMPARABLE profile/context mismatch; no deltas calculated.')
                continue
            if incomplete:
                lines.append('Context incomplete: descriptive only; matched hardware/workload comparison is unqualified.')
            for name in sorted(set(base_stats) | set(summary)):
                a, b = base_stats.get(name), summary.get(name)
                ma, mb = base_runs[0]['metrics'].get(name), members[0]['metrics'].get(name)
                if not a or not b or not a['availableRuns'] or not b['availableRuns'] or not ma or not mb or not compatible(ma, mb):
                    lines.append(f'{name}: NON-COMPARABLE/missing/unavailable.')
                elif base[1] != key[1] and name == 'parserPreparation':
                    lines.append(f'{name}: NON-COMPARABLE output setup differs across backends.')
                else:
                    delta = b['medianRunMean']-a['medianRunMean']
                    percent = f" ({delta/a['medianRunMean']*100:+.2f}%)" if a['medianRunMean'] else ''
                    lines.append(f"{name}: median run mean delta {delta:+.3f} {b['unit']}{percent}.")
    return '\n'.join(lines) + '\n'

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('command', choices=('validate', 'report', 'summary'))
    parser.add_argument('files', nargs='+', type=Path)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    try:
        runs = [load(p) for p in args.files]
        if args.command == 'validate':
            text = f'VALID: {len(runs)} schemaVersion=1 captures\n'
        elif args.command == 'report':
            text = markdown(runs)
        else:
            if len({profile(r) for r in runs}) != 1 or len({(r['sourceRevision'], r['backend'], r['buildKind']) for r in runs}) != 1:
                raise ValueError('summary requires one source/backend/build kind/profile group; use report for comparisons')
            text = json.dumps(summarize(runs), indent=2, allow_nan=False)+'\n'
        if args.output:
            args.output.write_text(text, encoding='utf-8')
        else:
            print(text, end='')
    except (ValueError, KeyError, TypeError, OSError) as error:
        parser.exit(1, f'ERROR: {error}\n')

if __name__ == '__main__':
    main()
