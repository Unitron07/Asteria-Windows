#pragma once
#include "pyrowave_perf.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QDir>
#include <QUuid>
#include <QString>

namespace PyroWavePerf {
// Called only on main, after Session has excluded decode and resource teardown.
inline QJsonObject serialize(const Capture& capture) {
    QJsonObject metrics, counts;
    constexpr auto JsonMax=uint64_t(std::numeric_limits<qint64>::max());
    bool counterOverflow=capture.counterOverflow.load();
    for (size_t i=0; i<MetricCount; ++i) {
        const auto& h=capture.metrics[i]; const auto n=h.count.load();
        const bool overflow=h.overflow.load() || n>JsonMax || h.maximum.load()>JsonMax;
        const bool available=n && !overflow;
        QJsonObject m{{"unit","us"},{"sampleCount",n<=JsonMax ? QJsonValue(qint64(n)) : QJsonValue(QJsonValue::Null)},
            {"scope",Definitions[i].scope},{"clock",Definitions[i].clock},
            {"status",available ? "available" : overflow ? "overflow" : "unavailable"},
            {"percentileMethod","nearest-rank-log2-8-upper-bound"}};
        m["mean"]=available ? QJsonValue(double(h.sum.load())/double(n)) : QJsonValue(QJsonValue::Null);
        m["minimum"]=available ? QJsonValue(qint64(h.minimum.load())) : QJsonValue(QJsonValue::Null);
        m["maximum"]=available ? QJsonValue(qint64(h.maximum.load())) : QJsonValue(QJsonValue::Null);
        for (auto p : {50,95,99}) m[QString("p%1").arg(p)]=available ? QJsonValue(qint64(h.percentile(unsigned(p)))) : QJsonValue(QJsonValue::Null);
        metrics[Definitions[i].name]=m;
    }
    for (size_t i=0; i<CounterCount; ++i) {
        const auto value=capture.counts[i].load();
        counts[CounterNames[i]]=value<=JsonMax ? QJsonValue(qint64(value)) : QJsonValue(QJsonValue::Null);
        counterOverflow=counterOverflow || value>JsonMax;
    }
    for(const char* name : {"presenterGpuExecution","displayCompletion","physicalScanout","endToEndLatency"}) {
        metrics[name]=QJsonObject{{"unit","us"},{"sampleCount",0},{"scope",QString("unavailable:")+name},
            {"clock","unavailable"},{"status","unavailable"},{"percentileMethod","unavailable"},
            {"mean",QJsonValue(QJsonValue::Null)},{"minimum",QJsonValue(QJsonValue::Null)},{"maximum",QJsonValue(QJsonValue::Null)},
            {"p50",QJsonValue(QJsonValue::Null)},{"p95",QJsonValue(QJsonValue::Null)},{"p99",QJsonValue(QJsonValue::Null)}};
    }
    QJsonArray events;
    for (size_t i=0; i<std::min<uint64_t>(Capture::EventCapacity,capture.eventCount.load()); ++i) {
        const auto& e=capture.events[i];
        events.append(QJsonObject{{"relativeUs",qint64(e.relativeUs)},{"frameNumber",qint64(e.frame)},
            {"slot",e.slot},{"stage",StageNames[size_t(e.stage)]}});
    }
    const auto elapsed=capture.end>=capture.start ? capture.end-capture.start : 0;
    const auto measured=elapsed>capture.warmup ? std::min(capture.duration,elapsed-capture.warmup) : 0;
    const auto omitted=capture.eventCount.load()>Capture::EventCapacity ? capture.eventCount.load()-Capture::EventCapacity : 0;
    return {{"schemaVersion",1},{"application","Asteria"},{"buildKind","instrumented-development"},
        {"releaseBaseline","v0.3.0-immutable-reference"},{"metrics",metrics},{"counts",counts},
        {"capture",QJsonObject{{"durationSeconds",double(measured)/1000000},
            {"requestedDurationSeconds",double(capture.duration)/1000000},{"warmupSeconds",double(capture.warmup)/1000000},
            {"storageBytes",qint64(sizeof(Capture))},{"counterOverflow",counterOverflow},
            {"eventCapacity",int(Capture::EventCapacity)},{"eventsOmitted",omitted<=JsonMax ? QJsonValue(qint64(omitted)) : QJsonValue(QJsonValue::Null)}}},
        {"frameEvents",events},
        {"limitations",QJsonArray{"presenter GPU execution unavailable (no query instrumentation)",
            "display completion, physical scanout and end-to-end latency unavailable",
            "interarrival covers delivered decode units, not all network packets",
            "network drops inferred from frame-number gaps; transport jitter cause unavailable",
            "capture window is not the scope of cumulative native codec GPU reports",
            "decoder initialization/cleanup are per lifetime, outside the warmup window",
            "driver, process GPU utilization, GPU clocks, power and thermals require companion tools",
            "histogram percentiles approximate upward within 12.5% for integer microseconds",
            "histograms/raw samples not exported; pooled percentiles unavailable"}}};
}
// Caller supplies only privacy-safe fields. No arbitrary configuration dump.
inline bool writeReport(const QString& directory, const QJsonObject& report) {
    QSaveFile file(QDir(directory).filePath("asteria-perf-"+QUuid::createUuid().toString(QUuid::WithoutBraces)+".json"));
    file.setDirectWriteFallback(false);
    if (!file.open(QIODevice::WriteOnly)) return false;
    const auto bytes=QJsonDocument(report).toJson(QJsonDocument::Indented);
    return file.write(bytes)==bytes.size() && file.commit();
}
}
