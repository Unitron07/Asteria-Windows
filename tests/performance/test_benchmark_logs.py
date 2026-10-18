"""Synthetic production-format logs; no Surface measurements are asserted."""
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

MARKER='PYROWAVE_NATIVE_STREAM_SUMMARY '
FIXTURE=Path(__file__).with_name('native-summary-console.txt')
OLD_FAILURE='requires one real native shutdown summary; multiple lifetimes/reconnects are incomplete'

class NativeLogTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory(); self.addCleanup(self.temp.cleanup)
        self.root=Path(self.temp.name)
        self.m=b.read_json(ROOT/'docs/benchmark-manifest.template.json')
        self.m.update(logs=['console.log'],exitCode=0)
        self.records=[json.loads(line.split(MARKER,1)[1]) for line in FIXTURE.read_text(encoding='utf-8').splitlines() if MARKER in line]
        self.real=copy.deepcopy(self.records[1])

    def write(self,records,name='console.log',pretty=False):
        text=''.join('00:01:15 - SDL Info (0): '+MARKER+json.dumps(r,sort_keys=not pretty,separators=None if pretty else (',',':'))+'\n' for r in records)
        (self.root/name).write_text(text,encoding='utf-8-sig')

    def prepare_failed_pilot(self):
        session=self.root/'session'; source=session/'run-01-ON-synthetic'; source.mkdir(parents=True)
        runner.save_new(session/'plan.json',{'fixtureOnly':True})
        self.root=source
        self.m.update(status='failed',failure=OLD_FAILURE,logs=[],
                      runId='synthetic-pilot-run',ordinal=1,timestamp='2026-10-17T00:00:00+00:00',
                      ownerConfirmedLiveAt='2026-10-17T00:00:00+00:00',
                      disconnectPromptAt='2026-10-17T00:01:12+00:00',
                      captures=['captures/synthetic.json'])
        self.write(self.records)
        capture=performance.load(Path(__file__).with_name('synthetic.json'))
        capture.update(runId='synthetic-capture',sourceRevision=self.m['sourceRevision'],architecture='arm64',gpu=self.m['gpu'],backend='NATIVE_VULKAN',decoderPath='fragment')
        capture['video'].update(width=2560,height=1440,targetFps=120,requestedBitrateKbps=500000)
        capture['metrics']['newFrameSubmissionInterval'].update(scope='CPU:successive_new_video_vkQueueSubmit_success_callbacks',clock='steady_clock')
        capture['metrics']['frameAgeAtPickup']['clock']='steady_clock'
        (source/'captures').mkdir()
        runner.save_new(source/'captures/synthetic.json',capture)
        runner.save_new(source/'manifest.json',self.m)
        return source,source.parent.parent/'recovered'

    def test_console_first_class_collection_and_preflight_ignored(self):
        package=self.root/'package'; package.mkdir()
        self.write(self.records)
        self.m['logs']=runner.collect_logs(package,{},self.root)
        self.assertEqual(self.m['logs'],['console.log'])
        runner.summarize_log(self.m,self.root)
        self.assertEqual(self.m['actualBackend'],'NATIVE_VULKAN')
        self.assertEqual(self.m['commonObservation']['counts']['decodedFrames'],100)

    def test_preflight_excluded_before_provenance_validation(self):
        preflight={'testOnly':True,'cleanupOkay':False,'sourceRevision':'preflight-only','timelineErrors':9}
        self.write([preflight,self.real,preflight])
        self.assertEqual(b.native_summary(self.m,self.root),self.real)

    def test_missing_preflight_only_and_multiple_real_rejected(self):
        for records,count in (([],0),([self.records[0]],0),([self.real,dict(self.real,decodedFrames=101)],2),([self.real,self.real],2)):
            with self.subTest(count=count,records=len(records)):
                self.write(records)
                with self.assertRaisesRegex(ValueError,f'found {count} real lifetimes'):
                    runner.summarize_log(self.m,self.root)

    def test_identical_copies_across_logs_deduplicated_but_same_log_repetition_rejected(self):
        self.write(self.records)
        self.write([dict(reversed(list(self.real.items())))],'Asteria-1.log',pretty=True)
        self.m['logs']=['console.log','Asteria-1.log','console.log']
        self.assertEqual(b.native_summary(self.m,self.root),self.real)
        self.write([self.real,self.real],'Asteria-1.log')
        with self.assertRaisesRegex(ValueError,'found 2'): b.native_summary(self.m,self.root)

    def test_different_real_summary_in_another_log_not_dropped(self):
        self.write(self.records)
        self.write([dict(self.real,sourceRevision='0'*40)],'Asteria-2.log')
        self.m['logs'].append('Asteria-2.log')
        with self.assertRaisesRegex(ValueError,'found 2'): b.native_summary(self.m,self.root)

    def test_real_identity_backend_cleanup_and_errors_remain_strict(self):
        changes={'sourceRevision':'0'*40,'selectedGpu':'wrong GPU','bitstreamId':'wrong',
                 'backend':'CPU_I420_FALLBACK','actualDecoderPath':'',
                 'cleanupOkay':False,'timelineErrors':1,'fatalPresenterErrors':1,
                 'decodedFrames':-1,'presentationDrops':True}
        for field,value in changes.items():
            with self.subTest(field=field):
                self.write([self.records[0],dict(self.real,**{field:value})])
                with self.assertRaises(ValueError): b.native_summary(self.m,self.root)
        for field in ('cleanupOkay','timelineErrors','fatalPresenterErrors','testOnly'):
            bad=dict(self.real);del bad[field];self.write([bad])
            with self.subTest(missing=field),self.assertRaises(ValueError): b.native_summary(self.m,self.root)
        for field,value in (('cleanupOkay',1),('timelineErrors',False),('fatalPresenterErrors',None),('testOnly','false')):
            self.write([dict(self.real,**{field:value})])
            with self.subTest(type=field),self.assertRaises(ValueError): b.native_summary(self.m,self.root)

    def test_malformed_and_nonfinite_summary_rejected_with_log_location(self):
        for text in ('{broken','[]','{"testOnly":false,"decodedFrames":NaN}'):
            (self.root/'console.log').write_text('00:00:00 - SDL Info (0): '+MARKER+text)
            with self.subTest(text=text),self.assertRaisesRegex(ValueError,'console.log:1: invalid native summary'):
                b.native_summary(self.m,self.root)

    def test_analyzer_reuses_parser_and_rechecks_health_and_original_counters(self):
        self.write(self.records)
        runner.summarize_log(self.m,self.root)
        self.m['profiling']='OFF'; self.m['status']='complete'
        runner.save_new(self.root/'manifest.json',self.m)
        self.assertEqual(b.load_manifest(self.root/'manifest.json')['actualDecoderPath'],'fragment')
        self.write([dict(self.real,cleanupOkay=False)])
        with self.assertRaisesRegex(ValueError,'cleanup'): b.load_manifest(self.root/'manifest.json')
        self.write(self.records)
        m=b.read_json(self.root/'manifest.json'); m['commonObservation']['counts']['decodedFrames']=123
        (self.root/'manifest.json').write_text(json.dumps(m))
        with self.assertRaisesRegex(ValueError,'counters differ'): b.load_manifest(self.root/'manifest.json')

    def test_offline_recovery_preserves_originals_and_validates_new_copy(self):
        source,target=self.prepare_failed_pilot()
        before={p.relative_to(source).as_posix():p.read_bytes() for p in source.rglob('*') if p.is_file()}
        with patch.object(runner.subprocess,'Popen',side_effect=AssertionError('offline recovery must not launch')):
            manifest=runner.revalidate_attempt(source,target)
        m=b.load_manifest(manifest)
        self.assertEqual(m['status'],'complete');self.assertEqual(m['logs'],['console.log'])
        self.assertEqual(m['runId'],self.m['runId']);self.assertEqual(m['ordinal'],1)
        self.assertNotIn('failure',m);self.assertNotIn('finishedAt',m)
        self.assertEqual(m['revalidation']['originalFailure'],OLD_FAILURE)
        self.assertEqual((target/'original-manifest.json').read_bytes(),before['manifest.json'])
        for name,data in before.items(): self.assertEqual((source/name).read_bytes(),data)
        self.assertEqual((target/'captures/synthetic.json').read_bytes(),before['captures/synthetic.json'])
        with self.assertRaisesRegex(ValueError,'already exists'):runner.revalidate_attempt(source,target)
        with self.assertRaisesRegex(ValueError,'outside the original session'):runner.revalidate_attempt(source,source.parent/'run-recovered')

    def test_offline_recovery_rejects_partial_capture_and_unrelated_failures(self):
        source,target=self.prepare_failed_pilot()
        capture=source/'captures/synthetic.json';r=b.read_json(capture);r['capture']['durationSeconds']=59
        capture.write_text(json.dumps(r))
        with self.assertRaisesRegex(ValueError,'partial'):runner.revalidate_attempt(source,target)
        self.assertFalse(target.exists());r['capture']['durationSeconds']=60;capture.write_text(json.dumps(r))
        self.write([dict(self.real,timelineErrors=1)])
        with self.assertRaisesRegex(ValueError,'timelineErrors'):runner.revalidate_attempt(source,target)
        self.assertFalse(target.exists());self.write(self.records)
        for field,value in (('exitCode',1),('failure','fixture stream interruption'),('ownerConfirmedLiveAt',None)):
            m=dict(self.m,**{field:value});(source/'manifest.json').write_text(json.dumps(m))
            with self.subTest(field=field),self.assertRaises(ValueError):runner.revalidate_attempt(source,target)
            self.assertFalse(target.exists())

    def test_offline_copy_detects_concurrent_changes_without_completing(self):
        source,target=self.prepare_failed_pilot()
        original_copy=runner.shutil.copyfileobj
        def changed(src,dest):
            original_copy(src,dest)
            if Path(src.name).name=='console.log':(source/'console.log').write_text('changed during copy')
        with patch.object(runner.shutil,'copyfileobj',side_effect=changed):
            with self.assertRaisesRegex(ValueError,'evidence changed'):runner.revalidate_attempt(source,target)
        self.assertFalse((target/'manifest.json').exists())

    def test_offline_cli_requires_no_live_arguments_and_never_overwrites(self):
        source,target=self.prepare_failed_pilot()
        command=[sys.executable,str(ROOT/'scripts/benchmark_runner.py'),'--revalidate',str(source),'--recovery-output',str(target)]
        result=subprocess.run(command,capture_output=True,text=True)
        self.assertEqual(result.returncode,0,result.stderr)
        self.assertIn('REVALIDATED COPY:',result.stdout)
        snapshot=(target/'manifest.json').read_bytes()
        result=subprocess.run(command,capture_output=True,text=True)
        self.assertNotEqual(result.returncode,0); self.assertIn('already exists',result.stderr)
        self.assertEqual((target/'manifest.json').read_bytes(),snapshot)
        self.assertEqual(b.read_json(source/'manifest.json')['status'],'failed')

    def test_offline_capture_inventory_must_match_original_manifest(self):
        source,target=self.prepare_failed_pilot()
        m=b.read_json(source/'manifest.json');m['captures']=['captures/missing.json']
        (source/'manifest.json').write_text(json.dumps(m))
        with self.assertRaisesRegex(ValueError,'inventory'):runner.revalidate_attempt(source,target)
        self.assertFalse(target.exists())

if __name__=='__main__':unittest.main()
