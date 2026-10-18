#!/usr/bin/env python3
"""Windows semi-automated profiling experiment. Python standard library only."""
import argparse
import copy
import csv
from datetime import datetime, timezone
import hashlib
import io
import json
import os
from pathlib import Path
import subprocess
import shutil
import time
import uuid
import benchmark

PERF_ENV=('ASTERIA_PERF_CAPTURE','ASTERIA_PERF_WARMUP_SECONDS','ASTERIA_PERF_DURATION_SECONDS')
HISTORICAL={'42f756e465288157608fe894e3a3dfe800b05344','2f29a8963078dadbe4e5031791a6390098e4635d'}

def now(): return datetime.now(timezone.utc).isoformat()

def save_new(path, value):
    # Exclusive create applies to every evidence/state file, including resume.
    with Path(path).open('x',encoding='utf-8',newline='\n') as f:
        json.dump(value,f,indent=2,allow_nan=False)
        f.write('\n')

def digest(path):
    h=hashlib.sha256()
    with Path(path).open('rb') as f:
        for chunk in iter(lambda:f.read(1024*1024),b''): h.update(chunk)
    return h.hexdigest()

def child_env(mode, directory, warmup, duration, parent=None):
    env={k:v for k,v in (os.environ if parent is None else parent).items() if not k.upper().startswith('ASTERIA_PERF_')}
    if mode=='ON':
        env.update(dict(zip(PERF_ENV,(str(Path(directory).resolve()),str(warmup),str(duration)))))
    elif mode!='OFF': raise ValueError('runner supports ON/OFF only')
    return env

def order(pairs, start='ON'):
    if type(pairs) is not int or not 1<=pairs<=100: raise ValueError('pairs must be 1..100')
    if start not in ('ON','OFF'): raise ValueError('start must be ON or OFF')
    # ON OFF OFF ON ON OFF ...; alternate the first condition of each pair.
    sequence=[mode for i in range(pairs) for mode in (('ON','OFF') if i%2==0 else ('OFF','ON'))]
    return sequence if start=='ON' else [('OFF' if m=='ON' else 'ON') for m in sequence]

def verify_package(exe, source, architecture):
    exe=Path(exe).resolve()
    root=exe.parent
    if exe.name.lower()!='asteria.exe' or not (root/'portable.dat').is_file():
        raise ValueError('select a portable Asteria.exe with existing portable.dat')
    if source in HISTORICAL: raise ValueError('historical releases are not runner targets; leave them untouched')
    manifest=benchmark.read_json(root/'stage5-package-manifest.json')
    if manifest['sourceRevision']!=source or manifest['architecture']!=architecture:
        raise ValueError('package source/architecture mismatch')
    entries=manifest['files']
    if not any(x['path']=='Asteria.exe' for x in entries): raise ValueError('executable absent from package manifest')
    for item in entries:
        path=benchmark.safe_path(root,item['path'])
        if digest(path)!=item['sha256']: raise ValueError('package hash mismatch: '+item['path'])
    runtime=benchmark.read_json(root/'pyrowave/pyrowave-runtime.json')
    pins={'codecCommit':'186f0393b77f7755953b5ecde994bb1cec2e4155','bitstreamId':'186f0393','apiVersion':'0.6.0',
          'graniteCommit':'b6cffd5ce81f540f0855e6778428483e14763d9b','architecture':architecture,
          'factoryCleanupPatchSha256':'8fe5906706bb27814ed7344f8932cd5ccf00c368ed24d253839d13cfeeb78c86'}
    if any(runtime.get(k)!=v for k,v in pins.items()): raise ValueError('runtime pins mismatch')
    if digest(root/'pyrowave/libpyrowave-shared-0.dll')!=runtime['sha256']: raise ValueError('runtime hash mismatch')
    with exe.open('rb') as f:
        if f.read(2)!=b'MZ': raise ValueError('invalid PE')
        f.seek(60); offset=int.from_bytes(f.read(4),'little'); f.seek(offset)
        if f.read(4)!=b'PE\0\0' or int.from_bytes(f.read(2),'little')!={'arm64':0xaa64,'x64':0x8664}[architecture]:
            raise ValueError('executable PE architecture mismatch')
    return digest(exe)

def no_running_client():
    result=subprocess.run(['tasklist','/FO','CSV','/NH'],capture_output=True,text=True,check=True)
    if any(row and row[0].lower() in ('asteria.exe','moonlight.exe') for row in csv.reader(io.StringIO(result.stdout))):
        raise ValueError('close existing Asteria/Moonlight processes normally before launching/resuming')

def collect_logs(root, before, target):
    # The redirected console is already in the attempt directory; never recopy it.
    logs=['console.log'] if (target/'console.log').is_file() else []
    for path in sorted(root.glob('Asteria-*.log')):
        if before.get(path.name)==digest(path): continue
        dest=target/path.name
        if dest.exists():
            if digest(dest)==digest(path):
                logs.append(dest.name)
                continue
            dest=target/(path.stem+'-'+uuid.uuid4().hex[:8]+path.suffix)
        with dest.open('xb') as f: f.write(path.read_bytes())
        logs.append(dest.name)
    return logs

def summarize_log(m, root):
    r=benchmark.native_summary(m,root)
    m['actualBackend']=r['backend']; m['actualDecoderPath']=r['actualDecoderPath']
    m['commonObservation']={'method':'PYROWAVE_NATIVE_STREAM_SUMMARY','scope':'decoder-lifetime',
        'counts':{k:r[k] for k in benchmark.COUNTERS if k in r},
        'transportLoss':None,'decoderQueueDiscards':None,'rttMs':None,'jitterMs':None,
        'payloadMbps':None,'audioLossEpisodes':None,
        'limitations':'Lifetime includes warmup and manual disconnect delay. networkDrops are inferred gaps, not attributable transport loss.'}

def validate_attempt(m, root):
    """The same evidence gates apply to a live finish and offline recovery."""
    benchmark.validate_manifest(m)
    if m['profiling']=='OFF' and m['captures']:
        raise ValueError('OFF unexpectedly produced telemetry')
    if m['profiling']=='ON' and len(m['captures'])!=1:
        raise ValueError('ON requires exactly one lifetime capture')
    summarize_log(m,root)
    for name in m['captures']:
        run=benchmark.performance.load(benchmark.safe_path(root,name))
        benchmark.check_capture(m,run)
        if run['capture']['durationSeconds']<m['context']['measurementSeconds']:
            raise ValueError('partial measurement window')
        if run['capture'].get('counterOverflow'):
            raise ValueError('capture counter overflow')
    if type(m.get('exitCode')) is not int or m['exitCode']!=0:
        raise ValueError('application exit must be recorded as zero')

def revalidate_attempt(source, destination):
    """Explicit offline recovery to a NEW standalone directory; no process launch."""
    source=Path(source).resolve(); destination=Path(destination).resolve()
    if destination.exists(): raise ValueError('recovery output already exists')
    if destination.is_relative_to(source) or source.is_relative_to(destination):
        raise ValueError('recovery output and original attempt must be separate')
    # Keep the recovered copy out of resume's glob: it is the SAME run, not a repeat.
    if (source.parent/'plan.json').exists() and destination.is_relative_to(source.parent):
        raise ValueError('recovery output must be outside the original session')
    original_manifest_hash=digest(source/'manifest.json')
    m=benchmark.validate_manifest(benchmark.read_json(source/'manifest.json'))
    old_error='requires one real native shutdown summary; multiple lifetimes/reconnects are incomplete'
    if m['status']!='failed' or m.get('failure')!=old_error or m['profiling']!='ON':
        raise ValueError('automatic recovery is restricted to the original profiling-ON log-selection failure')
    m=copy.deepcopy(m)
    m['logs']=list(dict.fromkeys(m['logs']+(['console.log'] if (source/'console.log').is_file() else [])+
                                [p.name for p in sorted(source.glob('Asteria-*.log'))]))
    found=[str(p.relative_to(source)) for p in sorted((source/'captures').glob('*.json'))]
    if {benchmark.safe_path(source,p) for p in m['captures']}!={benchmark.safe_path(source,p) for p in found}:
        raise ValueError('original manifest capture inventory differs from files on disk')
    m['captures']=found
    for field in ('ownerConfirmedLiveAt','disconnectPromptAt'):
        if not isinstance(m.get(field),str): raise ValueError(f'missing original timing evidence: {field}')
        value=datetime.fromisoformat(m[field].replace('Z','+00:00'))
        if value.tzinfo is None: raise ValueError('original timing evidence requires timezone')
    if datetime.fromisoformat(m['disconnectPromptAt'].replace('Z','+00:00')) <= datetime.fromisoformat(m['ownerConfirmedLiveAt'].replace('Z','+00:00')):
        raise ValueError('original timing evidence is out of order')
    inputs=set(m['logs']+m['captures']+['manifest.json'])
    if (source/'started.json').is_file(): inputs.add('started.json')
    hashes={name:digest(benchmark.safe_path(source,name)) for name in sorted(inputs)}
    if hashes['manifest.json']!=original_manifest_hash: raise ValueError('original manifest changed during recovery')
    validate_attempt(m,source)  # No output or status promotion before evidence passes.
    if any(digest(benchmark.safe_path(source,p))!=h for p,h in hashes.items()):
        raise ValueError('original evidence changed during validation')
    if set(m['logs']+m['captures']) & {'manifest.json','original-manifest.json','revalidation.json'}:
        raise ValueError('reserved recovery evidence filename')
    destination.mkdir(parents=True,exist_ok=False)
    for name in sorted(inputs):
        path=benchmark.safe_path(source,name)
        dest=destination/('original-manifest.json' if name=='manifest.json' else name)
        dest.parent.mkdir(parents=True,exist_ok=True)
        with dest.open('xb') as output, path.open('rb') as original:
            shutil.copyfileobj(original,output)
        if digest(dest)!=hashes[name]: raise ValueError('evidence changed during recovery copy')
    validate_attempt(m,destination)  # Re-read the exact copied evidence before completion.
    if any(digest(benchmark.safe_path(source,p))!=h for p,h in hashes.items()):
        raise ValueError('original evidence changed during recovery')
    audit={'validatedAt':now(),'originalStatus':'failed','originalFailure':m.pop('failure'),
           'originalManifestSha256':hashes['manifest.json'],'originalFileSha256':hashes,
           'method':'offline console/preflight selection correction; original evidence unchanged',
           'limitations':'Same runId/ordinal and measurement window. Not an independent repeat; original session is not advanced.'}
    m['revalidation']=audit
    m['status']='complete'
    save_new(destination/'revalidation.json',audit)
    save_new(destination/'manifest.json',m)
    validated=benchmark.load_manifest(destination/'manifest.json')
    with (destination/'summary.md').open('x',encoding='utf-8') as output:
        output.write(benchmark.report([validated]))
    return destination/'manifest.json'

def timer(process, seconds):
    deadline=time.monotonic()+seconds
    while time.monotonic()<deadline:
        if process.poll() is not None: raise ValueError('application exited during timed run')
        time.sleep(min(1,max(0,deadline-time.monotonic())))

def resume_plan(session, desired):
    path=session/'plan.json'
    if path.exists():
        stored=benchmark.read_json(path)
        if stored!=desired: raise ValueError('resume config/build/order differs from stored plan')
    else: save_new(path,desired)
    completed=set()
    for p in session.glob('run-*/manifest.json'):
        m=benchmark.read_json(p)
        if m['status']=='complete':
            benchmark.load_manifest(p)
            ordinal=m.get('ordinal')
            if type(ordinal) is not int or not 1<=ordinal<=len(desired['order']):
                raise ValueError('completed attempt ordinal outside saved plan')
            template=desired['template']
            if any(m[k]!=template[k] for k in ('sourceRevision','architecture','gpu','driver','codec','bitstreamId','hostVersion','profile','context')) or m['executableSha256']!=desired['executableSha256'] or m['profiling']!=desired['order'][ordinal-1]:
                raise ValueError('completed attempt does not belong to saved experiment')
            if m['ordinal'] in completed: raise ValueError('duplicate completed plan ordinal')
            completed.add(m['ordinal'])
    return completed

def execute(args):
    if os.name!='nt': raise ValueError('live runner requires Windows; fixture tests are portable')
    if not 0<=args.cooldown<=3600: raise ValueError('cooldown must be 0..3600 seconds')
    template=benchmark.validate_manifest(benchmark.read_json(args.config))
    if template['evidenceGroup']!='phase1b-controlled' or template['status']!='planned' or template['captures'] or template['logs']:
        raise ValueError('config must be an empty planned controlled run')
    c=template['context']
    if type(c['warmupSeconds']) is not int or not 0<=c['warmupSeconds']<=3600 or type(c['measurementSeconds']) is not int or not 1<=c['measurementSeconds']<=86400:
        raise ValueError('warmup/duration outside production environment bounds')
    exe=args.exe.resolve()
    binary_hash=verify_package(exe,template['sourceRevision'],template['architecture'])
    session=args.session.resolve()
    if session.exists() and not args.resume: raise ValueError('session exists; use --resume with identical plan')
    session.mkdir(parents=True,exist_ok=args.resume)
    desired={'schemaVersion':1,'executable':str(exe),'executableSha256':binary_hash,
             'template':template,'order':order(args.pairs,args.start),'cooldownSeconds':args.cooldown}
    # Exclusive lock; stale lock requires owner to verify the previous runner/client exited.
    lock=session/'runner.lock'
    with lock.open('x') as f: f.write(str(os.getpid()))
    try:
        no_running_client()
        completed=resume_plan(session,desired)
        for ordinal,mode in enumerate(desired['order'],1):
            if ordinal in completed: continue
            no_running_client()
            # Reverify every attempt so ON and OFF use exactly the same package bytes.
            if verify_package(exe,template['sourceRevision'],template['architecture'])!=binary_hash:
                raise ValueError('binary changed within experiment')
            target=session/f'run-{ordinal:02d}-{mode}-{uuid.uuid4().hex[:8]}'
            target.mkdir()
            captures=target/'captures'; captures.mkdir()
            m=copy.deepcopy(template)
            m.update(runId=str(uuid.uuid4()),timestamp=now(),ordinal=ordinal,profiling=mode,
                     condition='C' if mode=='ON' else 'D',status='incomplete',executableSha256=binary_hash,
                     actualBackend=None,actualDecoderPath=None)
            save_new(target/'started.json',m)
            before={p.name:digest(p) for p in exe.parent.glob('Asteria-*.log')}
            process=None
            try:
                input(f'Run {ordinal}/{len(desired["order"])} {mode}: confirm settings/workload ready and other clients closed; Enter launches. ')
                with (target/'console.log').open('xb') as console:
                    process=subprocess.Popen([str(exe)],cwd=exe.parent,
                        env=child_env(mode,captures,c['warmupSeconds'],c['measurementSeconds']),stdout=console,stderr=subprocess.STDOUT)
                    input('Connect ONE PyroWave stream manually. When video is stable, press Enter here. Do not resize/reconnect during the run. ')
                    m['ownerConfirmedLiveAt']=now()
                    print(f'Timing {c["warmupSeconds"]}s warmup + {c["measurementSeconds"]}s measurement + 2s export-window margin. Keep workload running.')
                    timer(process,c['warmupSeconds']+c['measurementSeconds']+2)
                    m['disconnectPromptAt']=now()
                    input('Disconnect normally now. Wait for cleanup, close Asteria normally, then press Enter. ')
                    # Never terminate: closing the process early can lose deferred exports.
                    while process.poll() is None:
                        input('Asteria is still running. Close it normally after disconnect/cleanup; Enter rechecks. ')
                    m['exitCode']=process.returncode
                m['logs']=collect_logs(exe.parent,before,target)
                m['captures']=[str(p.relative_to(target)) for p in sorted(captures.glob('*.json'))]
                validate_attempt(m,target)
                m['status']='complete'
            except (Exception,KeyboardInterrupt) as error:
                m['status']='interrupted' if isinstance(error,KeyboardInterrupt) else 'failed'
                m['failure']=str(error) or 'owner interruption'
                # Best-effort snapshot without killing a live client. Resume never treats this as completed.
                try:
                    m['logs']=collect_logs(exe.parent,before,target)
                except OSError as snapshot_error:
                    m['snapshotFailure']=str(snapshot_error)
                found=[str(p.relative_to(target)) for p in sorted(captures.glob('*.json'))]
                m['captures']=found if mode=='ON' else []
                if mode=='OFF' and found: m['unexpectedCaptures']=found
                save_new(target/'manifest.json',m)
                raise
            m['finishedAt']=now()
            save_new(target/'manifest.json',m)
            manifests=[benchmark.load_manifest(p) for p in sorted(session.glob('run-*/manifest.json'))]
            with (session/f'summary-{uuid.uuid4().hex}.md').open('x',encoding='utf-8') as f:
                f.write(benchmark.report(manifests))
            print(f'Preserved {target}; cool down {args.cooldown}s. No power/network settings changed.')
            if ordinal<len(desired['order']): time.sleep(args.cooldown)
    finally:
        lock.unlink()

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--exe',type=Path)
    parser.add_argument('--config',type=Path)
    parser.add_argument('--session',type=Path)
    parser.add_argument('--revalidate',type=Path,help='offline validation of an original failed ON attempt; never launches Asteria')
    parser.add_argument('--recovery-output',type=Path,help='new standalone directory outside the original session')
    parser.add_argument('--pairs',type=int,default=5)
    parser.add_argument('--start',choices=('ON','OFF'),default='ON')
    parser.add_argument('--cooldown',type=int,default=60)
    parser.add_argument('--resume',action='store_true')
    args=parser.parse_args()
    if args.revalidate:
        if not args.recovery_output or any((args.exe,args.config,args.session,args.resume)):
            parser.error('--revalidate requires --recovery-output and cannot launch/resume a session')
    elif not all((args.exe,args.config,args.session)) or args.recovery_output:
        parser.error('live mode requires --exe, --config and --session; recovery uses --revalidate and --recovery-output')
    try:
        if args.revalidate:
            print(f'REVALIDATED COPY: {revalidate_attempt(args.revalidate,args.recovery_output)}; originals unchanged; original session not advanced')
        else:
            execute(args)
    except (Exception,KeyboardInterrupt) as error:
        parser.exit(1,f'STOP: {error or "interrupted"}. Preserve results and disconnect/close any live client normally.\n')

if __name__=='__main__': main()
