import copy
import importlib.util
import json
import math
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('performance', ROOT/'scripts/performance.py')
perf = importlib.util.module_from_spec(spec)
spec.loader.exec_module(perf)

class AnalysisTests(unittest.TestCase):
    def setUp(self):
        self.run = perf.load(Path(__file__).with_name('synthetic.json'))

    def test_roundtrip(self):
        self.assertEqual(perf.validate(json.loads(json.dumps(self.run))), self.run)

    def test_version_and_identity(self):
        for bad in (2, None, True, 1.0):
            run = copy.deepcopy(self.run); run['schemaVersion'] = bad
            with self.assertRaises(ValueError): perf.validate(run)
        del self.run['sourceRevision']
        with self.assertRaises(ValueError): perf.validate(self.run)

    def test_null_missing_and_units(self):
        metric = self.run['metrics']['vulkanPresentCall']
        self.assertEqual(metric['status'], 'unavailable')
        self.assertIsNone(metric['mean'])
        metric['mean'] = 0
        with self.assertRaises(ValueError): perf.validate(self.run)

    def test_invalid_statistics(self):
        for bad in (-1, math.nan, math.inf, True):
            run = copy.deepcopy(self.run)
            run['metrics']['frameAssembly']['mean'] = bad
            with self.assertRaises(ValueError): perf.validate(run)
        self.run['metrics']['frameAssembly']['p99'] = 0
        with self.assertRaises(ValueError): perf.validate(self.run)

    def test_weighted_mean_and_run_variation(self):
        other = copy.deepcopy(self.run)
        other['metrics']['frameAssembly'].update(sampleCount=30, mean=20, minimum=20, maximum=20, p50=20, p95=20, p99=20)
        first = self.run['metrics']['frameAssembly']
        first.update(sampleCount=10, mean=10, minimum=10, maximum=10, p50=10, p95=10, p99=10)
        summary = perf.summarize([self.run, other])['frameAssembly']
        self.assertEqual(summary['sampleCount'], 40)
        self.assertEqual(summary['weightedMean'], 17.5)
        self.assertEqual(summary['medianRunMean'], 15)
        self.assertEqual(summary['minimumRunMean'], 10)
        self.assertEqual(summary['maximumRunMean'], 20)
        self.assertIsNone(summary['pooledPercentiles'])
        self.assertEqual([p['p99'] for p in summary['perRunPercentiles']], [10,20])

    def test_metric_compatibility(self):
        other = copy.deepcopy(self.run)
        for field in ('scope', 'unit', 'clock', 'percentileMethod'):
            bad = copy.deepcopy(other)
            bad['metrics']['frameAssembly'][field] = 'different'
            with self.assertRaises(ValueError): perf.summarize([self.run, bad])
        other['sourceRevision'] = 'other'
        other['metrics']['frameAssembly']['scope'] = 'different'
        self.assertIn('frameAssembly: NON-COMPARABLE', perf.markdown([self.run, other]))

    def test_profile_mismatch(self):
        other = copy.deepcopy(self.run)
        other['video']['targetFps'] = 120
        self.assertIn('NON-COMPARABLE profile/context mismatch', perf.markdown([self.run, other]))
        other=copy.deepcopy(self.run); other['capture']['warmupSeconds']=20
        self.assertIn('NON-COMPARABLE profile/context mismatch', perf.markdown([self.run,other]))
        other=copy.deepcopy(self.run); other['capture']['durationSeconds']=30
        self.assertIn('Partial capture',perf.markdown([other]))

    def test_codec_pin_required(self):
        self.run['codec']={}
        with self.assertRaises(ValueError): perf.validate(self.run)

    def test_missing_metrics(self):
        self.run['metrics'] = {}
        self.assertEqual(perf.summarize([self.run]), {})

    def test_same_stream_is_not_independent_runs(self):
        self.run['runId']='same'
        with self.assertRaises(ValueError): perf.summarize([self.run,copy.deepcopy(self.run)])

    def test_event_bound_and_counter_overflow(self):
        self.run['counts']['receivedFrames'] = 2**64-1
        with self.assertRaises(ValueError): perf.validate(self.run)
        self.run['capture']['counterOverflow'] = True
        self.run['counts']['receivedFrames'] = None
        perf.validate(self.run)
        self.run['frameEvents'] = [{'relativeUs':1,'frameNumber':1,'slot':1,'stage':'published'}]*513
        with self.assertRaises(ValueError): perf.validate(self.run)

if __name__ == '__main__':
    unittest.main()
