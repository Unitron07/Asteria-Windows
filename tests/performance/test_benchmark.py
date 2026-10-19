import copy
import json
from pathlib import Path
import sys
import subprocess
import tempfile
import unittest
from unittest.mock import patch

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'scripts'))
import benchmark as b
import benchmark_runner as runner
import performance

def ready_config(m):
    """Synthetic setup only; no hardware measurements."""
    m=copy.deepcopy(m)
    m.update(applicationVersion='synthetic',driver='synthetic-driver',hostVersion='synthetic-host',
             actualBackend='NATIVE_VULKAN',actualDecoderPath='fragment',experimentStage='pilot',qualification='controlled')
    for key in b.MATCH:
        if m['context'][key] is None: m['context'][key]='synthetic-setting'
    m['context'].update(vsync=False,displayRefreshHz=120,linkSpeedMbps=1000,mtu=1500,brightness=50)
    m['context'].update({k:'synthetic-setting' for k in b.SETUP})
    m['context']['hostWorkloadSha256']='a'*64
    return m

class BenchmarkTests(unittest.TestCase):
    def setUp(self):
        self.m=b.read_json(ROOT/'docs/benchmark-manifest.template.json')

    def test_template_unknowns_and_profile_gate(self):
        b.validate_manifest(self.m)
        self.assertIn('unknown:', ';'.join(b.issues(self.m)))
        self.m['context']['targetFps']=60
        with self.assertRaisesRegex(ValueError,'profile'): b.validate_manifest(self.m)

    def test_missing_vs_explicit_null_and_strict_types(self):
        for field in b.IDENTITY:
            m=copy.deepcopy(self.m); del m[field]
            with self.assertRaises(ValueError): b.validate_manifest(m)
        for value in (True,-1,float('nan')):
            m=copy.deepcopy(self.m); m['context']['bitrateKbps']=value
            with self.assertRaises(ValueError): b.validate_manifest(m)
        self.m['timestamp']='2026-10-11T01:00:00'
        with self.assertRaisesRegex(ValueError,'timezone'): b.validate_manifest(self.m)

    def test_off_does_not_inherit_any_capture_variable(self):
        parent={'asteria_perf_capture':'BAD','ASTERIA_PERF_WARMUP_SECONDS':'5','ASTERIA_PERF_FUTURE':'BAD','PATH':'kept'}
        env=runner.child_env('OFF',ROOT,10,60,parent)
        self.assertEqual(env,{'PATH':'kept'})
        on=runner.child_env('ON',ROOT,10,60,parent)
        self.assertEqual(on['ASTERIA_PERF_WARMUP_SECONDS'],'10')
        self.assertEqual(on['ASTERIA_PERF_DURATION_SECONDS'],'60')
        self.assertTrue(Path(on['ASTERIA_PERF_CAPTURE']).is_absolute())
        self.assertEqual(parent['ASTERIA_PERF_FUTURE'],'BAD')

    def test_counterbalanced_five_pairs(self):
        sequence=runner.order(5)
        self.assertEqual(sequence,['ON','OFF','OFF','ON','ON','OFF','OFF','ON','ON','OFF'])
        self.assertEqual(sequence.count('ON'),5)
        self.assertEqual(runner.order(5,'OFF'),['OFF','ON','ON','OFF','OFF','ON','ON','OFF','OFF','ON'])
        for value in (0,True,101):
            with self.assertRaises(ValueError): runner.order(value)

    def test_no_overwrite_and_safe_resume(self):
        with tempfile.TemporaryDirectory() as d:
            root=Path(d); plan={'order':runner.order(5)}
            self.assertEqual(runner.resume_plan(root,plan),set())
            self.assertEqual(runner.resume_plan(root,plan),set())
            with self.assertRaises(ValueError): runner.resume_plan(root,{'order':runner.order(4)})
            with self.assertRaises(FileExistsError): runner.save_new(root/'plan.json',plan)
            attempt=root/'run-01-ON-old'; attempt.mkdir()
            m=copy.deepcopy(self.m); m.update(status='interrupted',ordinal=1)
            runner.save_new(attempt/'manifest.json',m)
            self.assertEqual(runner.resume_plan(root,plan),set())
            with self.assertRaises(ValueError): b.safe_path(root,'../outside')

    def test_log_collection_preserves_changed_and_original_snapshots(self):
        with tempfile.TemporaryDirectory() as d:
            root=Path(d); target=root/'results'; target.mkdir()
            log=root/'Asteria-1.log'; log.write_text('first')
            names=runner.collect_logs(root,{},target)
            self.assertEqual(runner.collect_logs(root,{},target),names)
            log.write_text('second')
            names2=runner.collect_logs(root,{},target)
            self.assertNotEqual(names,names2)
            self.assertEqual((target/names[0]).read_text(),'first')

    def test_native_metadata_and_actual_decoder_capture_matching(self):
        r=performance.load(Path(__file__).with_name('synthetic.json'))
        r.update(sourceRevision=self.m['sourceRevision'],architecture='arm64',gpu=self.m['gpu'],backend='NATIVE_VULKAN',decoderPath='fragment')
        r['video'].update(width=2560,height=1440,targetFps=120,requestedBitrateKbps=500000)
        r['metrics']['newFrameSubmissionInterval'].update(scope='CPU:successive_new_video_vkQueueSubmit_success_callbacks',clock='steady_clock')
        r['metrics']['frameAgeAtPickup']['clock']='steady_clock'
        self.m.update(actualBackend='NATIVE_VULKAN',actualDecoderPath='fragment')
        b.check_capture(self.m,r)
        r['metrics']['frameAgeAtPickup']['clock']='common-c monotonic'
        with self.assertRaisesRegex(ValueError,'metadata'): b.check_capture(self.m,r)
        r['metrics']['frameAgeAtPickup']['clock']='steady_clock'; r['decoderPath']='compute'
        with self.assertRaisesRegex(ValueError,'decoder path'): b.check_capture(self.m,r)

    def test_off_rejects_on_capture_and_retirement_not_double_counted(self):
        self.m.update(profiling='OFF',captures=['capture.json'])
        with self.assertRaisesRegex(ValueError,'OFF'): b.validate_manifest(self.m)
        self.m.update(captures=[],status='complete',logs=['log'],commonObservation={
            'method':'PYROWAVE_NATIVE_STREAM_SUMMARY','scope':'decoder-lifetime',
            'counts':{'decodedFrames':100,'presentationDrops':10,'nativeRetiredDrops':10,'networkDrops':50}})
        text=b.report([self.m])
        self.assertIn('10 / 100; 0.1',text)
        self.assertIn('local decoder queue discard',text)
        self.assertIn('decoder-lifetime counts',text)
        self.assertIn('profiling was disabled',text)
        self.assertNotIn('pooledPercentiles": 0',text)

    def test_mismatched_sources_profiles_and_duplicate_identity(self):
        other=copy.deepcopy(self.m); other.update(runId='other',condition='D',profiling='OFF')
        with self.assertRaisesRegex(ValueError,'duplicate'): b.report([self.m,self.m])
        text=b.report([self.m,other]); self.assertIn('NON-COMPARABLE ON/OFF binary',text)
        other['context']['networkRoute']='different'
        self.assertIn('NON-COMPARABLE evidence/profile',b.report([self.m,other]))

    def test_variation_outliers_retained(self):
        stats=b.distribution([1,2,2,3,100])
        self.assertEqual(stats['n'],5); self.assertEqual(stats['median'],2)
        self.assertEqual(stats['maximum'],100); self.assertEqual(stats['outlierIndices'],[4])
        self.assertIsNone(b.distribution([1])['sampleStdev'])

    def test_ready_requires_context_but_old_unknown_evidence_remains_readable(self):
        b.validate_manifest(self.m)
        with self.assertRaises(ValueError): b.validate_ready(self.m,'controlled')
        ready=ready_config(self.m)
        b.validate_ready(ready)
        for key in b.MATCH+b.SETUP:
            m=copy.deepcopy(ready); m['context'][key]=None
            with self.subTest(key=key),self.assertRaises(ValueError):
                b.validate_ready(m)
        for key in ('driver','hostVersion','actualDecoderPath'):
            m=copy.deepcopy(ready);m[key]=None
            with self.subTest(key=key),self.assertRaises(ValueError): b.validate_ready(m)
        for value in ('TODO','unknown','unavailable',''):
            m=copy.deepcopy(ready);m['context']['hostWorkload']=value
            with self.subTest(value=value),self.assertRaises(ValueError): b.validate_ready(m)
        ready['context']['hostWorkloadSha256']='not-a-hash'
        with self.assertRaisesRegex(ValueError,'SHA-256'): b.validate_ready(ready)

    def test_bitrate_pair_rejects_all_context_and_identity_drift(self):
        a=ready_config(self.m);a['context']['bitrateKbps']=250000
        b500=copy.deepcopy(a);b500['context']['bitrateKbps']=500000
        b.validate_bitrate_pair([a,b500])
        for key in b.MATCH+b.SETUP:
            if key=='bitrateKbps': continue
            bad=copy.deepcopy(b500)
            value=bad['context'][key]
            bad['context'][key]=(not value if type(value) is bool else value+1 if type(value) in (int,float) else 'b'*64 if key=='hostWorkloadSha256' else value+'-changed')
            with self.subTest(key=key),self.assertRaises(ValueError): b.validate_bitrate_pair([a,bad])
        for key,value in (('actualDecoderPath','compute'),('sourceRevision','b'*40),('experimentStage','repeated'),('profiling','OFF'),('driver','other')):
            bad=copy.deepcopy(b500);bad[key]=value
            with self.subTest(key=key),self.assertRaises(ValueError): b.validate_bitrate_pair([a,bad])
        self.assertNotEqual(b.match_key(a),b.match_key(dict(a,experimentStage='repeated')))
        with self.assertRaises(ValueError): b.validate_bitrate_pair([a])

    def test_bitrate_templates_and_ready_cli(self):
        with tempfile.TemporaryDirectory() as d:
            paths=[]
            for rate in (250,500):
                m=b.read_json(ROOT/f'docs/benchmark-1440p120-{rate}mbps.template.json')
                m=b.prepare_config(m)
                b.validate_ready(m)
                with self.assertRaises(ValueError): b.validate_ready(m,'controlled')
                path=Path(d)/f'{rate}.json';runner.save_new(path,m);paths.append(str(path))
            command=[sys.executable,str(ROOT/'scripts/performance.py'),'benchmark-ready',*paths]
            result=subprocess.run(command,capture_output=True,text=True)
            self.assertEqual(result.returncode,0,result.stderr)
            self.assertIn('READY:',result.stdout)
            m=b.read_json(paths[1]);m['context']['vsync']=False
            Path(paths[1]).write_text(json.dumps(m),encoding='utf-8')
            result=subprocess.run(command,capture_output=True,text=True)
            self.assertNotEqual(result.returncode,0)
            self.assertIn('matched configs',result.stderr)

    @unittest.skipUnless(sys.platform=='win32','Windows launch gate')
    def test_missing_setup_blocks_before_package_session_or_process(self):
        with tempfile.TemporaryDirectory() as d:
            root=Path(d);config=root/'config.json';runner.save_new(config,ready_config(self.m))
            args=type('Args',(),dict(config=config,cooldown=60,qualification='controlled'))()
            m=b.read_json(config);m['context']['networkRoute']=None;config.write_text(json.dumps(m))
            with patch.object(runner,'verify_package') as package,patch.object(runner.subprocess,'Popen') as launch:
                with self.assertRaisesRegex(ValueError,'owner setup'): runner.execute(args)
                package.assert_not_called();launch.assert_not_called()

    def test_real_shutdown_summary_validation(self):
        with tempfile.TemporaryDirectory() as d:
            root=Path(d); self.m['logs']=['Asteria-1.log']
            record={'testOnly':False,'sourceRevision':self.m['sourceRevision'],'bitstreamId':self.m['bitstreamId'],
                    'selectedGpu':self.m['gpu'],'backend':'NATIVE_VULKAN','actualDecoderPath':'fragment',
                    'cleanupOkay':True,'timelineErrors':0,'fatalPresenterErrors':0,'decodedFrames':100,'presentationDrops':10}
            log=root/self.m['logs'][0]
            log.write_text('INFO PYROWAVE_NATIVE_STREAM_SUMMARY '+json.dumps(record))
            runner.summarize_log(self.m,root)
            self.assertEqual(self.m['commonObservation']['scope'],'decoder-lifetime')
            self.assertIsNone(self.m['commonObservation']['transportLoss'])
            record['sourceRevision']='0'*40
            log.write_text('INFO PYROWAVE_NATIVE_STREAM_SUMMARY '+json.dumps(record))
            with self.assertRaisesRegex(ValueError,'source'): runner.summarize_log(self.m,root)

    @unittest.skipUnless(sys.platform=='win32','live orchestration fixture uses Windows runner platform gate')
    def test_end_to_end_semi_automated_order_exports_and_resume(self):
        # Exercise production orchestration with a fake process, not a stream/GPU.
        self.m=ready_config(self.m)
        with tempfile.TemporaryDirectory() as d:
            root=Path(d); package=root/'package'; package.mkdir(); exe=package/'Asteria.exe'
            config=root/'config.json'; runner.save_new(config,self.m)
            session=root/'experiment'; modes=[]
            class Child:
                returncode=0
                def poll(self): return 0
            def launch(command,**kw):
                env=kw['env']; mode='ON' if 'ASTERIA_PERF_CAPTURE' in env else 'OFF'; modes.append(mode)
                record={'testOnly':False,'sourceRevision':self.m['sourceRevision'],'bitstreamId':self.m['bitstreamId'],
                        'selectedGpu':self.m['gpu'],'backend':'NATIVE_VULKAN','actualDecoderPath':'fragment',
                        'cleanupOkay':True,'timelineErrors':0,'fatalPresenterErrors':0,'decodedFrames':100,'presentationDrops':10}
                # Real Surface format: preflight and live summary ONLY in redirected
                # console output, with no Asteria-*.log in the package directory.
                text=Path(__file__).with_name('native-summary-console.txt').read_text(encoding='utf-8')
                kw['stdout'].write(text.encode('utf-8'))
                kw['stdout'].flush()
                if mode=='ON':
                    r=performance.load(Path(__file__).with_name('synthetic.json'))
                    r.update(runId=str(len(modes)),sourceRevision=self.m['sourceRevision'],architecture='arm64',gpu=self.m['gpu'],backend='NATIVE_VULKAN',decoderPath='fragment')
                    r['video'].update(width=2560,height=1440,targetFps=120,requestedBitrateKbps=500000,vsync=self.m['context']['vsync'])
                    r['metrics']['newFrameSubmissionInterval'].update(scope='CPU:successive_new_video_vkQueueSubmit_success_callbacks',clock='steady_clock')
                    r['metrics']['frameAgeAtPickup']['clock']='steady_clock'
                    runner.save_new(Path(env['ASTERIA_PERF_CAPTURE'])/'synthetic.json',r)
                return Child()
            args=type('Args',(),dict(config=config,exe=exe,session=session,pairs=2,start='ON',cooldown=0,resume=False))()
            with patch.object(runner,'verify_package',return_value='a'*64),patch.object(runner,'discover_evidence',return_value={'warnings':[]}),patch.object(runner,'no_running_client'),patch.object(runner,'timer'),patch.object(runner.subprocess,'Popen',side_effect=launch),patch('builtins.input',return_value=''):
                with patch.object(runner,'timer',side_effect=ValueError('fixture stream interruption')):
                    with self.assertRaisesRegex(ValueError,'interruption'): runner.execute(args)
                failed=next(session.glob('run-*/manifest.json'))
                self.assertEqual(b.read_json(failed)['status'],'failed')
                original=failed.read_bytes()
                args.resume=True
                runner.execute(args)
                self.assertEqual(modes,['ON','ON','OFF','OFF','ON'])
                self.assertEqual(failed.read_bytes(),original)
                for p in session.glob('run-*/manifest.json'):
                    m=b.load_manifest(p)
                    if m['status']=='failed': continue
                    self.assertEqual(m['status'],'complete')
                    self.assertEqual(m['logs'],['console.log'])
                    self.assertEqual(len(m['_runs']),1 if m['profiling']=='ON' else 0)
                args.resume=True; runner.execute(args)
                self.assertEqual(len(modes),5)
                # Same orchestration, one ON launch for a bitrate condition; resume
                # must not turn it into another launch or a profiling-OFF pair.
                args.session=root/'single';args.pairs=1;args.single_run=True;args.resume=False
                runner.execute(args)
                self.assertEqual(modes,['ON','ON','OFF','OFF','ON','ON'])
                args.resume=True;runner.execute(args)
                self.assertEqual(len(modes),6)
                args.pairs=2
                with self.assertRaisesRegex(ValueError,'--pairs 1'):runner.execute(args)
                args.session=session;args.pairs=2;args.single_run=False
                # A requested fragment path must survive launch and be checked
                # against real log evidence rather than silently replaced.
                changed=copy.deepcopy(self.m);changed['actualDecoderPath']='compute'
                badconfig=root/'wrong-path.json';runner.save_new(badconfig,changed)
                args.config=badconfig;args.session=root/'wrong-path';args.resume=False
                with self.assertRaisesRegex(ValueError,'actualDecoderPath'):runner.execute(args)
                failed_path=next(args.session.glob('run-*/manifest.json'))
                self.assertEqual(b.read_json(failed_path)['status'],'failed')
                args.config=config;args.session=session;args.resume=True
                # A completed manifest copied from another experiment cannot satisfy resume.
                path=next(p for p in session.glob('run-*/manifest.json') if b.read_json(p)['status']=='complete')
                m=b.read_json(path); m['context']['networkRoute']='tampered'
                path.write_text(json.dumps(m),encoding='utf-8')
                with self.assertRaisesRegex(ValueError,'saved experiment'): runner.execute(args)

if __name__=='__main__': unittest.main()
