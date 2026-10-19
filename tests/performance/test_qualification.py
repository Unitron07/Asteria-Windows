"""Synthetic evidence only; no real Asteria process or hardware benchmark."""
import copy
from contextlib import redirect_stdout
import io
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'scripts'))
import benchmark as b
import benchmark_runner as runner
import performance

class QualificationTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup)
        self.root=Path(self.temp.name)
        self.small=b.read_json(ROOT/'docs/benchmark-1440p120-250mbps.template.json')
        self.m=b.prepare_config(self.small)
        text=Path(__file__).with_name('native-summary-console.txt').read_text(encoding='utf-8')
        self.record=json.loads(text.splitlines()[-1].split('PYROWAVE_NATIVE_STREAM_SUMMARY ',1)[1])

    def capture(self,rate=250000):
        r=performance.load(Path(__file__).with_name('synthetic.json'))
        r.update(runId='synthetic-capture',sourceRevision=self.m['sourceRevision'],architecture='arm64',
                 gpu=self.record['selectedGpu'],backend='NATIVE_VULKAN',decoderPath='fragment')
        r['video'].update(width=2560,height=1440,targetFps=120,requestedBitrateKbps=rate,vsync=True)
        r['metrics']['newFrameSubmissionInterval'].update(scope='CPU:successive_new_video_vkQueueSubmit_success_callbacks',clock='steady_clock')
        r['metrics']['frameAgeAtPickup']['clock']='steady_clock'
        return r

    def evidence(self,record=None,capture=None):
        (self.root/'console.log').write_text('PYROWAVE_NATIVE_STREAM_SUMMARY '+json.dumps(record or self.record),encoding='utf-8')
        (self.root/'capture.json').write_text(json.dumps(capture or self.capture()),encoding='utf-8')
        m=copy.deepcopy(self.m)
        m.update(status='incomplete',captures=['capture.json'],logs=['console.log'],exitCode=0,
                 executableSha256='a'*64,timestamp='2026-10-18T00:00:00Z')
        return m

    def package(self):
        package=self.root/'package';package.mkdir()
        exe=package/'Asteria.exe'
        pe=bytearray(72);pe[:2]=b'MZ';pe[60:64]=(64).to_bytes(4,'little');pe[64:68]=b'PE\0\0';pe[68:70]=(0xaa64).to_bytes(2,'little')
        exe.write_bytes(pe);(package/'portable.dat').touch();(package/'pyrowave').mkdir()
        dll=package/'pyrowave/libpyrowave-shared-0.dll';dll.write_bytes(b'synthetic-runtime-only')
        runtime=dict(codecCommit='186f0393b77f7755953b5ecde994bb1cec2e4155',bitstreamId='186f0393',apiVersion='0.6.0',
            graniteCommit='b6cffd5ce81f540f0855e6778428483e14763d9b',architecture='arm64',
            factoryCleanupPatchSha256='8fe5906706bb27814ed7344f8932cd5ccf00c368ed24d253839d13cfeeb78c86',sha256=runner.digest(dll))
        runner.save_new(package/'pyrowave/pyrowave-runtime.json',runtime)
        files=[{'path':str(p.relative_to(package)).replace('\\','/'),'sha256':runner.digest(p)} for p in package.rglob('*') if p.is_file()]
        runner.save_new(package/'stage5-package-manifest.json',dict(sourceRevision=self.m['sourceRevision'],architecture='arm64',files=files))
        return exe

    def test_minimal_defaults_and_explicit_controlled_gate(self):
        b.validate_ready(self.m)
        self.assertEqual(b.qualification(self.m),'exploratory')
        self.assertEqual(self.m['context']['measurementSeconds'],60)
        self.assertIsNone(self.m['gpu'])
        self.assertIn('NOT CAUSALLY QUALIFIED',b.qualification_label(self.m))
        self.assertEqual(len(b.context_warnings(self.m)),1)
        self.assertNotEqual(b.match_key(self.m),b.match_key(dict(self.m,qualification='controlled')))
        self.assertLess(len(b.context_warnings(self.m)[0]),260)
        with self.assertRaisesRegex(ValueError,'owner setup'):b.validate_ready(self.m,'controlled')
        with self.assertRaisesRegex(ValueError,'unknown config'):b.prepare_config(dict(self.small,typo=True))
        with self.assertRaisesRegex(ValueError,'qualification'):b.prepare_config(dict(self.small,qualification='auto'))

    def test_essential_settings_cannot_be_omitted_or_invalid(self):
        for field,value in (('sourceRevision',None),('sourceRevision','not-a-sha'),('actualDecoderPath',None),('actualBackend','CPU_I420_FALLBACK')):
            m=copy.deepcopy(self.m);m[field]=value
            with self.subTest(field=field,value=value),self.assertRaises(ValueError):b.validate_ready(m)
        for field,value in (('bitrateKbps',None),('vsync',None),('targetFps',60),('warmupSeconds',-1),('measurementSeconds',0)):
            m=copy.deepcopy(self.m);m['context'][field]=value
            with self.subTest(field=field),self.assertRaises(ValueError):b.validate_ready(m)

    def test_optional_context_known_in_only_one_config_is_a_warning(self):
        other=copy.deepcopy(self.m);other['context']['bitrateKbps']=500000
        other['driver']='synthetic-driver';other['context']['hostWorkload']='synthetic scene'
        b.validate_bitrate_pair([self.m,other])
        self.assertTrue(b.context_warnings(self.m));self.assertTrue(b.context_warnings(other))
        with self.assertRaises(ValueError):b.validate_bitrate_pair([self.m,other],'controlled')

    def test_minimal_cli_is_ready_with_warnings_controlled_is_fatal(self):
        paths=[str(ROOT/f'docs/benchmark-1440p120-{rate}mbps.template.json') for rate in (250,500)]
        cmd=[sys.executable,str(ROOT/'scripts/performance.py'),'benchmark-ready',*paths]
        result=subprocess.run(cmd,capture_output=True,text=True)
        self.assertEqual(result.returncode,0,result.stderr)
        self.assertIn('EXPLORATORY / NOT CAUSALLY QUALIFIED',result.stdout)
        self.assertEqual(result.stdout.count('WARNING:'),1)
        result=subprocess.run(cmd+['--qualification','controlled'],capture_output=True,text=True)
        self.assertNotEqual(result.returncode,0)

    def test_exploratory_mode_keeps_all_evidence_gates(self):
        runner.validate_attempt(self.evidence(),self.root)
        for field,value in (('sourceRevision','0'*40),('selectedGpu','other GPU'),('backend','CPU_I420_FALLBACK'),
                            ('actualDecoderPath','compute'),('bitstreamId','wrong'),('cleanupOkay',False),
                            ('timelineErrors',1),('fatalPresenterErrors',1),('testOnly',True)):
            m=self.evidence(record=dict(self.record,**{field:value}))
            with self.subTest(log=field),self.assertRaises(ValueError):runner.validate_attempt(m,self.root)
        for field,value in (('requestedBitrateKbps',500000),('width',1920),('targetFps',60),('vsync',False)):
            capture=self.capture();capture['video'][field]=value
            with self.subTest(video=field),self.assertRaises(ValueError):runner.validate_attempt(self.evidence(capture=capture),self.root)
        for field,value in (('durationSeconds',59),('counterOverflow',True),('requestedDurationSeconds',30)):
            capture=self.capture();capture['capture'][field]=value
            with self.subTest(capture=field),self.assertRaises(ValueError):runner.validate_attempt(self.evidence(capture=capture),self.root)
        m=self.evidence();m['exitCode']=1
        with self.assertRaisesRegex(ValueError,'exit'):runner.validate_attempt(m,self.root)

    def test_completed_new_manifest_is_rechecked_but_old_bytes_stay_compatible(self):
        m=self.evidence();runner.validate_attempt(m,self.root);m['status']='complete'
        runner.save_new(self.root/'manifest.json',m)
        loaded=b.load_manifest(self.root/'manifest.json')
        self.assertIn('EXPLORATORY / NOT CAUSALLY QUALIFIED',b.report([loaded]))
        for field,value in (('durationSeconds',59),('counterOverflow',True)):
            capture=self.capture();capture['capture'][field]=value
            (self.root/'capture.json').write_text(json.dumps(capture))
            with self.subTest(field=field),self.assertRaises(ValueError):b.load_manifest(self.root/'manifest.json')
        (self.root/'capture.json').write_text(json.dumps(self.capture()))
        legacy=copy.deepcopy(m);legacy.pop('qualification');legacy['evidenceGroup']='phase1b-controlled'
        runner.save_new(self.root/'old.json',legacy);before=(self.root/'old.json').read_bytes()
        b.load_manifest(self.root/'old.json')
        self.assertEqual((self.root/'old.json').read_bytes(),before)
        # An existing full-template session without the new qualification field
        # can still resume; completed evidence must not be changed or relaunched.
        session=self.root/'legacy-session';attempt=session/'run-01-ON-old';attempt.mkdir(parents=True)
        legacy['ordinal']=1
        for name in ('console.log','capture.json'):(attempt/name).write_bytes((self.root/name).read_bytes())
        runner.save_new(attempt/'manifest.json',legacy)
        template=copy.deepcopy(self.m);template.pop('qualification');template['evidenceGroup']='phase1b-controlled'
        plan=dict(template=template,order=['ON'],executableSha256=legacy['executableSha256'])
        runner.save_new(session/'plan.json',plan)
        before=(attempt/'manifest.json').read_bytes()
        self.assertEqual(runner.resume_plan(session,plan),{1})
        self.assertEqual((attempt/'manifest.json').read_bytes(),before)

    def test_automatic_discovery_is_read_only_and_failure_is_nonfatal(self):
        exe=self.package()
        values=dict(applicationVersion='synthetic-version',videoControllers=[{'Name':self.record['selectedGpu'],'DriverVersion':'synthetic-driver'}],
                    batteries=[{'EstimatedChargeRemaining':75}],brightness=[{'CurrentBrightness':50}],warnings=[])
        with patch.object(runner.platform,'platform',return_value='synthetic Windows'),patch.object(runner.subprocess,'run',return_value=subprocess.CompletedProcess([],0,json.dumps(values))) as read:
            data=runner.discover_evidence(exe)
        self.assertEqual(read.call_count,1)
        self.assertEqual(read.call_args.args[0][0],'powershell.exe')
        self.assertEqual(data['runtimeManifest']['identity']['bitstreamId'],'186f0393')
        m=self.evidence();runner.validate_attempt(m,self.root);runner.apply_discovery(m,data)
        self.assertEqual(m['driver'],'synthetic-driver')
        self.assertEqual(m['applicationVersion'],'synthetic-version')
        with patch.object(runner.platform,'platform',return_value='synthetic Windows'),patch.object(runner.subprocess,'run',side_effect=OSError('synthetic unavailable')):
            data=runner.discover_evidence(exe)
        self.assertTrue(data['warnings']);self.assertIn('clientOs',data)

    def test_package_identity_and_runtime_pins_are_still_fatal(self):
        exe=self.package()
        self.assertEqual(runner.verify_package(exe,self.m['sourceRevision'],'arm64'),runner.digest(exe))
        with self.assertRaisesRegex(ValueError,'source/architecture'):runner.verify_package(exe,'0'*40,'arm64')
        with self.assertRaisesRegex(ValueError,'source/architecture'):runner.verify_package(exe,self.m['sourceRevision'],'x64')
        path=exe.parent/'pyrowave/pyrowave-runtime.json';runtime=b.read_json(path)
        runtime['codecCommit']='0'*40;path.write_text(json.dumps(runtime))
        # Even a self-consistent file inventory cannot bypass the pinned runtime identity.
        manifest=exe.parent/'stage5-package-manifest.json';m=b.read_json(manifest)
        for entry in m['files']:
            if entry['path']=='pyrowave/pyrowave-runtime.json':entry['sha256']=runner.digest(path)
        manifest.write_text(json.dumps(m))
        with self.assertRaisesRegex(ValueError,'runtime pins'):runner.verify_package(exe,self.m['sourceRevision'],'arm64')

    @unittest.skipUnless(sys.platform=='win32','Windows orchestration; synthetic process only')
    def test_minimal_250_and_500_full_runner_exports_and_resume(self):
        exe=self.package();launches=[]
        class Child:
            returncode=0
            def poll(self):return 0
        def launch(command,**kwargs):
            launches.append(command)
            kwargs['stdout'].write(('PYROWAVE_NATIVE_STREAM_SUMMARY '+json.dumps(self.record)).encode('utf-8'))
            capture=self.capture(rate)
            runner.save_new(Path(kwargs['env']['ASTERIA_PERF_CAPTURE'])/'capture.json',capture)
            return Child()
        data={'clientOs':'synthetic Windows','applicationVersion':'synthetic version','warnings':[],
              'videoControllers':[{'Name':self.record['selectedGpu'],'DriverVersion':'synthetic-driver'}]}
        for rate in (250000,500000):
            small=copy.deepcopy(self.small);small['context']['bitrateKbps']=rate
            config=self.root/f'{rate}.json';runner.save_new(config,small)
            args=type('Args',(),dict(exe=exe,config=config,session=self.root/str(rate),pairs=1,single_run=True,start='ON',cooldown=60,resume=False))()
            observed=data if rate==500000 else {'clientOs':'synthetic Windows','warnings':['Synthetic optional discovery unavailable']}
            with patch.object(runner,'no_running_client'),patch.object(runner,'discover_evidence',return_value=observed),patch.object(runner,'timer'),patch.object(runner.subprocess,'Popen',side_effect=launch),patch('builtins.input',return_value=''),redirect_stdout(io.StringIO()):
                with patch.object(sys,'argv',['benchmark_runner.py','--exe',str(exe),'--config',str(config),'--session',str(args.session),'--single-run']):
                    runner.main()  # The exact short owner command, no --pairs metadata needed.
                manifest=next(args.session.glob('run-*/manifest.json'));m=b.load_manifest(manifest)
                self.assertEqual(m['status'],'complete');self.assertEqual(m['qualification'],'exploratory')
                self.assertEqual(m['gpu'],self.record['selectedGpu']);self.assertEqual(m['driver'],'synthetic-driver' if rate==500000 else None)
                self.assertEqual(m['executableSha256'],runner.digest(exe));self.assertEqual(m['context']['clientOs'],'synthetic Windows')
                before=manifest.read_bytes();args.resume=True;runner.execute(args)
                self.assertEqual(manifest.read_bytes(),before)
                self.assertIsNone(m['context']['hostWorkload'])
        self.assertEqual(len(launches),2)
        exe.write_bytes(b'tampered')
        with self.assertRaisesRegex(ValueError,'hash mismatch'):runner.verify_package(exe,self.m['sourceRevision'],'arm64')

if __name__=='__main__':unittest.main()
