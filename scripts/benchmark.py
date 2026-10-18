"""Companion manifests and analysis; no streaming control or second profiler."""
import json
from pathlib import Path
import re
import statistics
from collections import Counter
from datetime import datetime
import performance

IDENTITY = ('timestamp', 'sourceRevision', 'applicationVersion', 'architecture', 'gpu',
            'driver', 'codec', 'actualBackend', 'actualDecoderPath', 'bitstreamId', 'hostVersion', 'executableSha256')
MATCH = ('width', 'height', 'targetFps', 'bitrateKbps', 'vsync', 'displayRefreshHz',
         'hostWorkload', 'hostHardware', 'networkRoute', 'transport', 'linkSpeedMbps', 'mtu',
         'powerState', 'chargingState', 'brightness', 'clientOs', 'warmupSeconds', 'measurementSeconds')
EVIDENCE = ('stage5-qualification', 'official-v0.2.0', 'official-v0.3.0',
            'phase1a-initial', 'phase1a-metadata-fixed', 'phase1b-controlled')
COUNTERS = ('receivedFrames', 'decodedFrames', 'renderedFrames', 'presentationDrops',
            'nativeRetiredDrops', 'networkDrops', 'retainedFrameRedraws', 'overlayRedraws')

def read_json(path):
    path = Path(path)
    if path.stat().st_size > 2*1024*1024:
        raise ValueError('manifest exceeds 2 MiB')
    def reject(value):
        raise ValueError(f'invalid numeric constant: {value}')
    return json.loads(path.read_text(encoding='utf-8-sig'), parse_constant=reject)

def validate_manifest(m):
    if type(m) is not dict or type(m.get('benchmarkSchemaVersion')) is not int or m['benchmarkSchemaVersion'] != 1:
        raise ValueError('expected benchmarkSchemaVersion integer 1')
    for key in ('runId', 'condition', 'evidenceGroup', 'profiling', 'status', 'notes'):
        if not isinstance(m.get(key), str) or not m[key]:
            raise ValueError(f'missing {key}')
    if m['evidenceGroup'] not in EVIDENCE or m['profiling'] not in ('ON', 'OFF', 'historical'):
        raise ValueError('unknown evidence/profiling identity')
    if m['status'] not in ('planned', 'complete', 'incomplete', 'failed', 'interrupted', 'excluded'):
        raise ValueError('invalid run status')
    for key in IDENTITY:
        if key not in m or (m[key] is not None and (not isinstance(m[key], str) or not m[key].strip())):
            raise ValueError(f'{key} must be explicit nonempty string or null')
    if m['timestamp'] is not None:
        timestamp = datetime.fromisoformat(m['timestamp'].replace('Z', '+00:00'))
        if timestamp.tzinfo is None:
            raise ValueError('timestamp requires timezone')
    if m['sourceRevision'] is not None and not re.fullmatch('[0-9a-f]{40}', m['sourceRevision']):
        raise ValueError('sourceRevision must be exact SHA or null')
    if m['executableSha256'] is not None and not re.fullmatch('[0-9a-f]{64}', m['executableSha256']):
        raise ValueError('executableSha256 must be exact hash or null')
    if type(m.get('context')) is not dict or any(k not in m['context'] for k in MATCH):
        raise ValueError('context missing required keys; use explicit null for unknowns')
    c = m['context']
    for key in ('width', 'height', 'targetFps', 'bitrateKbps', 'displayRefreshHz', 'linkSpeedMbps', 'mtu', 'measurementSeconds'):
        if c[key] is not None and (not performance.number(c[key]) or c[key] <= 0):
            raise ValueError(f'invalid context.{key}')
    for key in ('width', 'height', 'mtu'):
        if c[key] is not None and type(c[key]) is not int:
            raise ValueError(f'integer context.{key} required')
    if c['warmupSeconds'] is not None and not performance.number(c['warmupSeconds']):
        raise ValueError('invalid warmup')
    if c['vsync'] is not None and type(c['vsync']) is not bool:
        raise ValueError('vsync must be bool or null')
    for key in ('hostWorkload', 'hostHardware', 'networkRoute', 'transport', 'powerState', 'chargingState', 'clientOs'):
        if c[key] is not None and (not isinstance(c[key], str) or not c[key].strip()):
            raise ValueError(f'invalid context.{key}')
    if c['brightness'] is not None and (not performance.number(c['brightness']) or c['brightness'] > 100):
        raise ValueError('brightness must be percent or null')
    if m.get('profile') not in ('1080p60', '1440p120', '1080p120', '1440p60'):
        raise ValueError('unknown profile')
    expected = {'1080p60': (1920,1080,60), '1440p120': (2560,1440,120),
                '1080p120': (1920,1080,120), '1440p60': (2560,1440,60)}[m['profile']]
    if tuple(c[k] for k in ('width','height','targetFps')) != expected:
        raise ValueError('declared profile does not match resolution/FPS')
    if type(m.get('captures')) is not list or type(m.get('logs')) is not list:
        raise ValueError('captures/logs lists required')
    if m['profiling'] != 'ON' and m['captures']:
        raise ValueError('OFF/historical runs cannot contain profiling captures')
    if m['status'] == 'excluded' and not m.get('exclusionReason'):
        raise ValueError('excluded runs require recorded reason')
    for key in ('captures', 'logs'):
        for path in m[key]:
            if not isinstance(path, str) or Path(path).is_absolute() or '..' in Path(path).parts:
                raise ValueError('evidence paths must be relative and contained')
    obs = m.get('commonObservation')
    if obs is not None:
        if type(obs) is not dict or obs.get('scope') != 'decoder-lifetime' or obs.get('method') != 'PYROWAVE_NATIVE_STREAM_SUMMARY':
            raise ValueError('unsupported common observation method/scope')
        for key, value in obs.get('counts', {}).items():
            if key not in COUNTERS or type(value) is not int or value < 0:
                raise ValueError('invalid common counter')
    return m

def safe_path(root, name):
    path = (root / name).resolve()
    if not path.is_relative_to(root.resolve()):
        raise ValueError('evidence escapes manifest directory')
    return path

def native_summary(m, root):
    """Select one real lifetime, sharing identical copies across log files only.

    A repeated record within ONE log remains ambiguous and must not be deduped:
    it could represent two real lifetimes. Compare parsed JSON, not log prefixes.
    """
    counts = Counter()
    records = {}
    seen_paths = set()
    for name in m['logs']:
        path = safe_path(root, name)
        if path in seen_paths:
            continue
        seen_paths.add(path)
        local = Counter()
        for lineno, line in enumerate(path.read_text(encoding='utf-8-sig', errors='replace').splitlines(), 1):
            marker = 'PYROWAVE_NATIVE_STREAM_SUMMARY '
            if marker not in line:
                continue
            try:
                def reject(value):
                    raise ValueError(f'invalid numeric constant: {value}')
                record = json.loads(line.split(marker, 1)[1], parse_constant=reject)
                if type(record) is not dict or type(record.get('testOnly')) is not bool:
                    raise ValueError('testOnly must be an explicit JSON boolean')
                if record['testOnly']:
                    continue  # Native preflight never counts as a stream lifetime.
                key = json.dumps(record, sort_keys=True, allow_nan=False)
            except (ValueError, TypeError) as error:
                raise ValueError(f'{name}:{lineno}: invalid native summary: {error}') from error
            local[key] += 1
            records[key] = record
        counts |= local  # Maximum multiplicity per payload across copies, not sum.
    real_count = sum(counts.values())
    if real_count != 1:
        raise ValueError(f'requires exactly one real native shutdown summary; found {real_count} real lifetimes (preflight excluded)')
    r = records[next(iter(counts))]
    for field, identity in (('sourceRevision', 'sourceRevision'), ('selectedGpu', 'gpu'), ('bitstreamId', 'bitstreamId')):
        if not isinstance(r.get(field), str) or not r[field] or r[field] != m[identity]:
            raise ValueError(f'shutdown summary {identity} mismatch')
    if r.get('backend') != 'NATIVE_VULKAN':
        raise ValueError('shutdown summary backend must be NATIVE_VULKAN')
    if not isinstance(r.get('actualDecoderPath'), str) or not r['actualDecoderPath'].strip():
        raise ValueError('shutdown summary missing actual decoder path')
    for field in ('backend', 'actualDecoderPath'):
        identity = 'actualBackend' if field == 'backend' else field
        if m.get(identity) is not None and m[identity] != r[field]:
            raise ValueError(f'common log provenance mismatch: {field}')
    if r.get('cleanupOkay') is not True:
        raise ValueError('native cleanup failure or missing cleanupOkay=true')
    for field in ('timelineErrors', 'fatalPresenterErrors'):
        if type(r.get(field)) is not int or r[field] != 0:
            raise ValueError(f'native {field} must be explicit zero')
    for field in set(COUNTERS).intersection(r) | {'decodedFrames', 'presentationDrops'}:
        if type(r.get(field)) is not int or not 0 <= r[field] <= 2**63-1:
            raise ValueError(f'invalid native counter: {field}')
    return r

def check_capture(m, run):
    c = m['context']
    for key in ('sourceRevision', 'architecture', 'gpu', 'backend'):
        wanted = m['actualBackend'] if key == 'backend' else m[key]
        if wanted is not None and run.get(key) != wanted:
            raise ValueError(f'capture identity mismatch: {key}')
    if m['codec'] is not None and run['codec']['name'] != m['codec']:
        raise ValueError('codec mismatch')
    if m['bitstreamId'] is not None and run['codec']['bitstreamId'] != m['bitstreamId']:
        raise ValueError('bitstream mismatch')
    if m['actualDecoderPath'] is not None and run.get('decoderPath') != m['actualDecoderPath']:
        raise ValueError('actual decoder path mismatch')
    for a, b in (('width','width'), ('height','height'), ('targetFps','targetFps'), ('bitrateKbps','requestedBitrateKbps'), ('vsync','vsync')):
        if c[a] is not None and run['video'][b] != c[a]:
            raise ValueError(f'capture profile mismatch: {a}')
    for a,b in (('warmupSeconds','warmupSeconds'), ('measurementSeconds','requestedDurationSeconds')):
        if c[a] is not None and run['capture'][b] != c[a]:
            raise ValueError(f'capture window policy mismatch: {a}')
    if run['backend'] == 'NATIVE_VULKAN':
        required = {'newFrameSubmissionInterval': ('CPU:successive_new_video_vkQueueSubmit_success_callbacks','steady_clock'),
                    'frameAgeAtPickup': (None,'steady_clock')}
        for key,(scope,clock) in required.items():
            metric=run['metrics'].get(key)
            if not metric or metric['clock'] != clock or (scope and metric['scope'] != scope):
                raise ValueError('native metadata predates correction or is missing')

def load_manifest(path):
    m = validate_manifest(read_json(path))
    root = Path(path).resolve().parent
    m = dict(m)
    m['_runs'] = []
    m['_evidenceErrors'] = []
    for p in m['captures']:
        try:
            run = performance.load(safe_path(root,p))
            check_capture(m,run)
            m['_runs'].append(run)
        except (ValueError, OSError) as error:
            if m['status']=='complete': raise
            m['_evidenceErrors'].append(str(error))
    for p in m['logs']:
        if not safe_path(root,p).is_file():
            raise ValueError('missing log evidence')
    if m.get('commonObservation') is not None:
        r=native_summary(m,root)
        counts=m['commonObservation']['counts']
        if any(r.get(k)!=v for k,v in counts.items()): raise ValueError('common counters differ from original log')
    return m

def issues(m):
    result=list(m.get('_evidenceErrors', []))
    missing=[k for k in IDENTITY if m[k] is None]+['context.'+k for k in MATCH if m['context'][k] is None]
    if missing: result.append('unknown: '+', '.join(missing))
    if m['status'] != 'complete': result.append('status='+m['status'])
    if m['profiling']=='ON':
        runs=m.get('_runs', [])
        if len(runs)!=1: result.append('requires exactly one decoder lifetime capture')
        if any(m['context']['measurementSeconds'] is None or r['capture']['durationSeconds'] < m['context']['measurementSeconds'] for r in runs): result.append('partial capture')
        if any(r['capture'].get('counterOverflow') for r in runs): result.append('counter overflow')
    if not m['logs']: result.append('missing common log evidence')
    if m['evidenceGroup']=='phase1b-controlled' and m.get('commonObservation') is None:
        result.append('missing common-method observations')
    return result

def distribution(values):
    if not values: return None
    median=statistics.median(values)
    mad=statistics.median(abs(v-median) for v in values)
    return {'n':len(values),'median':median,'minimum':min(values),'maximum':max(values),
            'sampleStdev':statistics.stdev(values) if len(values)>1 else None,
            'mad':mad,'outlierIndices':[i for i,v in enumerate(values) if mad and abs(v-median)>3*1.4826*mad]}

def match_key(m):
    return json.dumps([m['profile'],*[m[k] for k in ('architecture','gpu','driver','codec','bitstreamId','hostVersion')],m['context']],sort_keys=True)

def report(manifests):
    ids=[m['runId'] for m in manifests]
    if len(set(ids))!=len(ids): raise ValueError('duplicate benchmark runId')
    capture_ids=[r.get('runId') for m in manifests for r in m.get('_runs',[]) if r.get('runId')]
    if len(set(capture_ids))!=len(capture_ids): raise ValueError('capture reused as independent run')
    lines=['# Phase 1B recorded experiments','','All supplied attempts retained. Unknowns are unavailable, never zero. No winner or causal conclusion.','Lifetime logs and capture windows remain separate. Inferred networkDrops may include local decoder queue discard.','nativeRetiredDrops is presentation-drop retirement accounting; it is not added to presentationDrops.','','| Run | Condition | Status / qualifications | Lifetime presentationDrops / decoded |','| --- | --- | --- | --- |']
    groups={}
    for m in manifests:
        problem=issues(m)
        counts=(m.get('commonObservation') or {}).get('counts',{})
        d,n=counts.get('presentationDrops'),counts.get('decodedFrames')
        fraction=d/n if d is not None and n else None
        lines.append(f"| {m['runId']} | {m['condition']} / {m['profiling']} | {'; '.join(problem) or 'context complete'} | {d} / {n}; {fraction if fraction is not None else 'unavailable'} |")
        key=(m['evidenceGroup'],m['condition'],m['sourceRevision'],m['actualBackend'],m['actualDecoderPath'],m['profiling'],match_key(m))
        groups.setdefault(key,[]).append(m)
    lines += ['','## Individual observation windows','']
    for m in manifests:
        counts=(m.get('commonObservation') or {}).get('counts',{})
        lines += [f"### {m['runId']}",f'Common decoder-lifetime counts (includes warmup/manual delay): `{json.dumps(counts,sort_keys=True)}`.']
        for r in m.get('_runs',[]):
            seconds=r['capture']['durationSeconds']
            safe=seconds>0 and not r['capture'].get('counterOverflow')
            fps={k:r['counts'][k]/seconds if safe and r['counts'].get(k) is not None else None
                 for k in ('receivedFrames','decodedFrames','newVideoSubmissions')}
            lines += [f"Capture `{r.get('runId')}`: {seconds}s measured after {r['capture']['warmupSeconds']}s warmup.",
                      f"Capture-window counts (separate from lifetime): `{json.dumps(r['counts'],sort_keys=True)}`.",
                      f"Capture-window FPS by counter: `{json.dumps(fps,sort_keys=True)}`; observed fields: `{json.dumps(r.get('observed'),sort_keys=True)}`."]
        if m['profiling']=='OFF': lines.append('Capture-window histograms/FPS/percentiles unavailable: profiling was disabled.')
    lines += ['','## Repeated conditions','']
    summaries=[]
    for key,members in groups.items():
        usable=[m for m in members if m['status']=='complete']
        fractions=[]
        for m in usable:
            c=(m.get('commonObservation') or {}).get('counts',{})
            if c.get('decodedFrames') and c.get('presentationDrops') is not None: fractions.append(c['presentationDrops']/c['decodedFrames'])
        stats=distribution(fractions)
        summaries.append((key,members,stats))
        lines += [f"### {key[0]} / {key[1]} / {key[5]}",f'Source `{key[2]}`; backend `{key[3]}`; decoder `{key[4]}`.',f'Attempts {len(members)}, completed {len(usable)}. Lifetime drop-fraction statistics: `{json.dumps(stats)}`.', 'Outlier indices refer to completed observations in input order; flagged samples remain included.']
        if len(usable)<5: lines.append('INCOMPLETE: fewer than five completed repeats.')
        if any(issues(m) for m in members): lines.append('UNQUALIFIED: missing context or incomplete evidence; descriptive statistics only.')
        captures=[r for m in usable for r in m.get('_runs',[]) if len(m.get('_runs',[]))==1]
        if captures:
            lines += ['',performance.markdown(captures)]
            for name in sorted(set().union(*(r['metrics'] for r in captures))):
                values=[r['metrics'][name]['mean'] for r in captures if name in r['metrics'] and r['metrics'][name]['status']=='available']
                lines.append(f'{name} run-mean variation: `{json.dumps(distribution(values))}`')
    lines += ['','## Between conditions','']
    for i,(a,am,astats) in enumerate(summaries):
        for b,bm,bstats in summaries[i+1:]:
            prefix=f'{a[1]}/{a[5]} vs {b[1]}/{b[5]}'
            if a[0]!=b[0] or a[6]!=b[6]:
                lines.append(prefix+': NON-COMPARABLE evidence/profile/context; no deltas.')
                continue
            if {a[5],b[5]}=={'ON','OFF'} and (a[2]!=b[2] or {m['executableSha256'] for m in am+bm}=={None} or len({m['executableSha256'] for m in am+bm})!=1):
                lines.append(prefix+': NON-COMPARABLE ON/OFF binary source; no deltas.')
                continue
            if a[4]!=b[4]: lines.append(prefix+': actual decoder path differs; confounded, no codec-time equivalence.')
            complete=all(not issues(m) for m in am+bm) and len(am)>=5 and len(bm)>=5
            lines.append(prefix+(': context complete; correlation only.' if complete else ': UNQUALIFIED/descriptive only.'))
            if astats and bstats: lines.append(f"Lifetime drop-fraction median delta {bstats['median']-astats['median']:+.6f}; sample variation retained above. No pooled p99 or latency claim.")
            if a[3]!=b[3]: lines.append('Backend boundary differs: parser setup, async decode/readback, pickup and submission scopes require individual compatibility checks.')
    return '\n'.join(lines)+'\n'
