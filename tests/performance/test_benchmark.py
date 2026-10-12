import copy
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'scripts'))
import benchmark as b
import benchmark_runner as runner
import performance

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
                (package/f'Asteria-{len(modes)}.log').write_text('INFO PYROWAVE_NATIVE_STREAM_SUMMARY '+json.dumps(record))
                if mode=='ON':
                    r=performance.load(Path(__file__).with_name('synthetic.json'))
                    r.update(runId=str(len(modes)),sourceRevision=self.m['sourceRevision'],architecture='arm64',gpu=self.m['gpu'],backend='NATIVE_VULKAN',decoderPath='fragment')
                    r['video'].update(width=2560,height=1440,targetFps=120,requestedBitrateKbps=500000)
                    r['metrics']['newFrameSubmissionInterval'].update(scope='CPU:successive_new_video_vkQueueSubmit_success_callbacks',clock='steady_clock')
                    r['metrics']['frameAgeAtPickup']['clock']='steady_clock'
                    runner.save_new(Path(env['ASTERIA_PERF_CAPTURE'])/'synthetic.json',r)
                return Child()
            args=type('Args',(),dict(config=config,exe=exe,session=session,pairs=2,cooldown=0,resume=False))()
            with patch.object(runner,'verify_package',return_value='a'*64),patch.object(runner,'no_running_client'),patch.object(runner,'timer'),patch.object(runner.subprocess,'Popen',side_effect=launch),patch('builtins.input',return_value=''):
                runner.execute(args)
                self.assertEqual(modes,['ON','OFF','OFF','ON'])
                for p in session.glob('run-*/manifest.json'):
                    m=b.load_manifest(p); self.assertEqual(m['status'],'complete')
                    self.assertEqual(len(m['_runs']),1 if m['profiling']=='ON' else 0)
                args.resume=True; runner.execute(args)
                self.assertEqual(len(modes),4)

if __name__=='__main__': unittest.main()
