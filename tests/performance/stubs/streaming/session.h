#pragma once
#include "streaming/video/overlaymanager.h"
#include <QJsonObject>

// Only the application/connection boundary is replaced. The decoder destructor,
// native/legacy resource destructors, exportPerf(), serializer and file writer
// are compiled from production sources. No host connection or GPU is required.
class Session {
public:
    static Session* get();
    Overlay::OverlayManager& getOverlayManager();
    int performanceBitrateKbps() const { return 200000; }
    void retainPerformanceReport(const QString& directory,const QJsonObject& report);
};
