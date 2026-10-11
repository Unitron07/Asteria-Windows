#include "pyrowave_decoder.h"
#include "pyrowave_perf_export.h"
#include "streaming/session.h"
#include "streaming/streamutils.h"
#include <QCoreApplication>
#include <QFile>
#include <QTextStream>
#include <memory>

namespace {
int failures=0,retained=0,detached=0;
PyroWaveVideoDecoder* destroying=nullptr;
QJsonObject retainedReport;
QString retainedDirectory;
void check(bool okay,const char* reason) {
    if(!okay) { ++failures; QTextStream(stderr)<<"FAIL: "<<reason<<'\n'; }
}
}
// The friend only seeds an initialized capture/resource state without a GPU.
// Actual delete runs the unmodified production destructor and calls exportPerf.
struct PyroWavePerformanceExportTest {
    static void seed(PyroWaveVideoDecoder& d,const QString& backend,const QString& directory) {
        d.m_Perf=std::make_unique<PyroWavePerf::Capture>();
        auto& c=*d.m_Perf;
        c.start=PyroWavePerf::nowUs()-80000000; c.warmup=10000000; c.duration=60000000;
        for(uint64_t value : {100,200,300}) {
            c.metrics[PyroWavePerf::PickupAge].record(value);
            c.metrics[PyroWavePerf::PresentationInterval].record(value*2);
        }
        c.metrics[PyroWavePerf::Assembly].record(17);
        c.counts[PyroWavePerf::Received]=31; c.counts[PyroWavePerf::Decoded]=29;
        c.counts[PyroWavePerf::Submitted]=27; c.counts[PyroWavePerf::NetworkDrops]=2;
        c.counts[PyroWavePerf::PresentationDrops]=2; c.counts[PyroWavePerf::RetainedRedraws]=4;
        c.counts[PyroWavePerf::OverlayRedraws]=1; c.counts[PyroWavePerf::VideoBytes]=1024;
        d.m_PerfBackend=backend; d.m_PerfDirectory=directory; d.m_PerfGpu="GPU-free teardown fixture";
        d.m_PerfDecoderPath="GPU-free initialized-state fixture";
        d.m_Width=1920; d.m_Height=1080; d.m_TargetFps=60; d.m_BitrateKbps=200000;
        d.m_TestOnly=false; // Exercise the production live overlay-detach path too.
        if(backend=="NATIVE_VULKAN") d.m_Native=std::make_unique<PyroWave::NativePresentation>(d.m_Runtime,d.m_Perf.get());
        if(backend=="LEGACY_D3D11_INTEROP") d.m_Gpu=std::make_unique<PyroWave::GpuPresentation>(d.m_Runtime);
        check(d.usesNativeVulkan()==(backend=="NATIVE_VULKAN"),"seeded native resource identity");
        check(bool(d.m_Gpu)==(backend=="LEGACY_D3D11_INTEROP"),"seeded legacy resource identity");
    }
    static QJsonObject before(const PyroWaveVideoDecoder& d) { return PyroWavePerf::serialize(*d.m_Perf); }
    static bool resourcesGone(const PyroWaveVideoDecoder& d) { return !d.m_Native && !d.m_Gpu; }
};

// No host, overlay rendering or transport clock is exercised by this test.
Session* Session::get() { static Session session; return &session; }
Overlay::OverlayManager& Session::getOverlayManager() { static Overlay::OverlayManager manager; return manager; }
void Session::retainPerformanceReport(const QString& directory,const QJsonObject& report) {
    check(destroying && PyroWavePerformanceExportTest::resourcesGone(*destroying),"export occurs after resource reset");
    ++retained; retainedDirectory=directory; retainedReport=report;
}
Overlay::OverlayManager::OverlayManager() {}
Overlay::OverlayManager::~OverlayManager() {}
void Overlay::OverlayManager::setOverlayRenderer(Overlay::IOverlayRenderer* renderer) {
    check(!renderer,"teardown detaches overlay renderer"); ++detached;
}
bool Overlay::OverlayManager::isOverlayEnabled(Overlay::OverlayType) { return false; }
void Overlay::OverlayManager::setOverlayTextUpdated(Overlay::OverlayType) {}
void Overlay::OverlayManager::updateOverlayText(Overlay::OverlayType,const char*) {}
SDL_Surface* Overlay::OverlayManager::getUpdatedOverlaySurface(Overlay::OverlayType) { return nullptr; }
void StreamUtils::scaleSourceToDestinationSurface(SDL_Rect*,SDL_Rect*) {}
extern "C" uint64_t LiGetMicroseconds(void) { return PyroWavePerf::nowUs(); }
extern "C" bool LiGetEstimatedRttInfo(uint32_t*,uint32_t*) { return false; }

int main(int argc,char** argv) {
    QCoreApplication application(argc,argv);
    if(argc!=2 || !QDir(argv[1]).exists()) return 2;
    for(const QString backend : {"NATIVE_VULKAN","LEGACY_D3D11_INTEROP","CPU_I420_FALLBACK"}) {
        retained=detached=0; retainedReport={};
        const auto directory=QDir(argv[1]).filePath(backend);
        check(QDir().mkpath(directory),"test output directory");
        auto decoder=std::make_unique<PyroWaveVideoDecoder>();
        PyroWavePerformanceExportTest::seed(*decoder,backend,directory);
        const auto before=PyroWavePerformanceExportTest::before(*decoder);
        destroying=decoder.get(); decoder.reset(); destroying=nullptr;
        check(retained==1 && detached==1,"one production teardown/export and live overlay detach");
        check(retainedReport["backend"].toString()==backend,"saved backend survives teardown");
        const auto metrics=retainedReport["metrics"].toObject();
        const auto submission=metrics["newFrameSubmissionInterval"].toObject();
        const auto pickup=metrics["frameAgeAtPickup"].toObject();
        const bool native=backend=="NATIVE_VULKAN";
        check(submission["scope"].toString()==(native ? "CPU:successive_new_video_vkQueueSubmit_success_callbacks" : "CPU:successive_new_video_SDL_RenderPresent_returns"),"submission scope matches saved backend after reset");
        check(pickup["clock"].toString()==(native ? "steady_clock" : "common-c monotonic"),"pickup clock matches saved backend after reset");
        check(submission["clock"].toString()=="steady_clock","submission interval retains steady clock for all backends");
        check(retainedReport["counts"]==before["counts"],"all counters preserved");
        auto expectedMetrics=before["metrics"].toObject();
        for(auto it=expectedMetrics.begin();it!=expectedMetrics.end();++it) {
            if(it.key()=="decoderCleanup") continue; // The production destructor records this sample.
            auto actual=metrics[it.key()].toObject(),expected=it.value().toObject();
            if(it.key()!="newFrameSubmissionInterval" && it.key()!="frameAgeAtPickup")
                check(actual["scope"]==expected["scope"] && actual["clock"]==expected["clock"],"other metric scopes/clocks remain unchanged");
            if(it.key()=="frameAgeAtPickup") check(actual["scope"]==expected["scope"],"pickup scope remains unchanged");
            actual.remove("scope"); actual.remove("clock"); expected.remove("scope"); expected.remove("clock");
            check(actual==expected,"every measurement value/count/percentile remains unchanged");
        }
        check(metrics["decoderCleanup"].toObject()["sampleCount"].toInteger()==1,"production cleanup measurement recorded");
        check(retainedDirectory==directory,"explicit output directory retained");
        check(PyroWavePerf::writeReport(retainedDirectory,retainedReport),"production atomic file export after decoder deletion");
        const auto files=QDir(directory).entryList({"*.json"},QDir::Files);
        check(files.size()==1,"one unique report written");
        if(files.size()==1) {
            QFile file(QDir(directory).filePath(files[0])); check(file.open(QIODevice::ReadOnly),"read exported capture");
            QJsonParseError error;
            const auto parsed=QJsonDocument::fromJson(file.readAll(),&error);
            check(error.error==QJsonParseError::NoError && parsed.object()==retainedReport,"exported JSON roundtrip");
        }
        QTextStream(stdout)<<"teardown/export backend="<<backend<<'\n';
    }
    QTextStream(stdout)<<(failures ? "FAIL" : "PASS")<<" production decoder teardown/export metadata, values and counters; failures="<<failures<<'\n';
    return failures ? 1 : 0;
}
