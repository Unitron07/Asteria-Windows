#include "streamingpreferences.h"
#include "utils.h"
#include <QtTest>
#include <QGuiApplication>
#include <QSettings>
#include <QTemporaryDir>
#include <QProcess>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQuickView>
#include <QQuickItem>
#include <QQmlContext>
#include <QQmlExpression>
#include <QTextStream>
#include <memory>

// Only desktop discovery/polling/navigation are stubbed. Preferences, settings
// persistence, the QML page and its controls are the production implementations.
namespace WMUtils {
bool isRunningWayland() { return false; }
bool isGpuSlow() { return false; }
}
class DesktopStub : public QObject {
    Q_OBJECT
    Q_PROPERTY(QSize maximumResolution READ maximumResolution CONSTANT)
    Q_PROPERTY(bool hasDesktopEnvironment READ yes CONSTANT)
    Q_PROPERTY(bool hasDiscordIntegration READ no CONSTANT)
    Q_PROPERTY(bool isDarwin READ no CONSTANT)
    Q_PROPERTY(bool rendererAlwaysFullScreen READ no CONSTANT)
    Q_PROPERTY(bool supportsHdr READ yes CONSTANT)
    Q_PROPERTY(bool usesMaterial3Theme READ no CONSTANT)
public:
    bool yes() const { return true; }
    bool no() const { return false; }
    QSize maximumResolution() const { return {8192,8192}; }
    Q_INVOKABLE void refreshDisplays() {}
    Q_INVOKABLE QRect getNativeResolution(int index) { return index == 0 ? QRect(0,0,3840,2160) : QRect(); }
    Q_INVOKABLE QRect getSafeAreaResolution(int index) { return getNativeResolution(index); }
    Q_INVOKABLE int getRefreshRate(int index) { return index == 0 ? 120 : 0; }
};
class NavigationStub : public QObject {
    Q_OBJECT
public:
    Q_INVOKABLE void setUiNavMode(bool) {}
    Q_INVOKABLE int getConnectedGamepads() { return 0; }
};
class ComputerStub : public QObject {
    Q_OBJECT
public:
    Q_INVOKABLE void stopPollingAsync() {}
    Q_INVOKABLE void startPolling() {}
};
class WindowStub : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool pollingActive MEMBER pollingActive)
    Q_PROPERTY(bool clearOnBack MEMBER clearOnBack)
public:
    bool pollingActive = false, clearOnBack = false;
};
using Prefs = StreamingPreferences;
static QJsonObject snapshot(Prefs* p) {
    return {{"codec",int(p->videoCodecConfig)}, {"bitrate",p->bitrateKbps},
            {"auto",p->autoAdjustBitrate}, {"width",p->width}, {"height",p->height}, {"fps",p->fps}};
}
class SettingsTests : public QObject {
    Q_OBJECT
    QTemporaryDir directory;
    Prefs* prefs = nullptr;
    void select(Prefs::VideoCodecConfig codec) {
        QVERIFY(prefs->setProperty("videoCodecConfig", QVariant::fromValue(codec)));
    }
    QJsonObject restart() {
        prefs->save();
        QSettings().sync();
        QProcess child;
        child.start(QCoreApplication::applicationFilePath(), {"--read-settings",directory.path()});
        if (!child.waitForFinished(30000) || child.exitStatus()!=QProcess::NormalExit || child.exitCode()!=0) {
            qWarning()<<child.readAllStandardError(); return {};
        }
        return QJsonDocument::fromJson(child.readAllStandardOutput()).object();
    }
private slots:
    void initTestCase() {
        QVERIFY(directory.isValid());
        QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,directory.path());
        prefs = Prefs::get();
    }
    void init() { QSettings().clear(); prefs->reload(); }
    void codecCaps() {
        for (auto codec : {Prefs::VCC_AUTO,Prefs::VCC_FORCE_H264,Prefs::VCC_FORCE_HEVC,
                           Prefs::VCC_FORCE_HEVC_HDR_DEPRECATED,Prefs::VCC_FORCE_AV1}) {
            QCOMPARE(Prefs::getMaximumBitrate(codec),500000);
        }
        QCOMPARE(Prefs::getMaximumBitrate(Prefs::VCC_FORCE_PYROWAVE),3000000);
        QCOMPARE(prefs->metaObject()->indexOfProperty("unlockBitrate"),-1);
    }
    void pyroDefaults_data() {
        QTest::addColumn<int>("width"); QTest::addColumn<int>("height");
        QTest::addColumn<int>("fps"); QTest::addColumn<int>("expected");
        QTest::newRow("720p60")<<1280<<720<<60<<88500;
        QTest::newRow("1080p60")<<1920<<1080<<60<<199000;
        QTest::newRow("1440p60")<<2560<<1440<<60<<354000;
        QTest::newRow("4K60")<<3840<<2160<<60<<796500;
        QTest::newRow("1080p120")<<1920<<1080<<120<<398000;
        QTest::newRow("1440p120")<<2560<<1440<<120<<708000;
        QTest::newRow("4K120")<<3840<<2160<<120<<1592500;
    }
    void pyroDefaults() {
        QFETCH(int,width); QFETCH(int,height); QFETCH(int,fps); QFETCH(int,expected);
        QCOMPARE(Prefs::getDefaultBitrateForCodec(width,height,fps,false,Prefs::VCC_FORCE_PYROWAVE),expected);
        QVERIFY(qAbs(expected-double(width)*height*fps*1.6/1000.0)<=250.0);
        // An unsupported profile setting must not change the P1a recommendation.
        QCOMPARE(Prefs::getDefaultBitrateForCodec(width,height,fps,true,Prefs::VCC_FORCE_PYROWAVE),expected);
    }
    void defaultBoundsAndStandardFormula() {
        QCOMPARE(Prefs::getDefaultBitrateForCodec(1,1,1,false,Prefs::VCC_FORCE_PYROWAVE),500);
        QCOMPARE(Prefs::getDefaultBitrateForCodec(8192,8192,9999,false,Prefs::VCC_FORCE_PYROWAVE),3000000);
        QCOMPARE(Prefs::getDefaultBitrate(1920,1080,60,false),20000);
        QCOMPARE(Prefs::getDefaultBitrate(1920,1080,120,false),28000);
        for (auto codec : {Prefs::VCC_AUTO,Prefs::VCC_FORCE_H264,Prefs::VCC_FORCE_HEVC,Prefs::VCC_FORCE_AV1}) {
            for (bool yuv444 : {false,true}) {
                QCOMPARE(Prefs::getDefaultBitrateForCodec(3840,2160,120,yuv444,codec),
                         Prefs::getDefaultBitrate(3840,2160,120,yuv444));
            }
        }
    }
    void manualResolutionFpsAndRestart() {
        select(Prefs::VCC_FORCE_PYROWAVE);
        prefs->setProperty("autoAdjustBitrate",false);
        prefs->setProperty("bitrateKbps",750000);
        prefs->setProperty("width",2560); prefs->setProperty("height",1440);
        QCOMPARE(prefs->bitrateKbps,750000);
        prefs->setProperty("fps",120);
        QCOMPARE(prefs->bitrateKbps,750000); QVERIFY(!prefs->autoAdjustBitrate);
        const auto expected = snapshot(prefs);
        QCOMPARE(restart(),expected);
        prefs->reload(); QCOMPARE(snapshot(prefs),expected);
    }
    void manualCodecCapAndRestart() {
        select(Prefs::VCC_FORCE_PYROWAVE);
        prefs->setProperty("autoAdjustBitrate",false); prefs->setProperty("bitrateKbps",750000);
        select(Prefs::VCC_FORCE_AV1);
        QCOMPARE(prefs->bitrateKbps,500000); QVERIFY(!prefs->autoAdjustBitrate);
        const auto expected = snapshot(prefs);
        QCOMPARE(restart(),expected);
        select(Prefs::VCC_FORCE_PYROWAVE); prefs->setProperty("bitrateKbps",350000);
        for (auto codec : {Prefs::VCC_FORCE_AV1,Prefs::VCC_FORCE_H264,Prefs::VCC_FORCE_HEVC,Prefs::VCC_AUTO}) {
            select(codec); QCOMPARE(prefs->bitrateKbps,350000); QVERIFY(!prefs->autoAdjustBitrate);
        }
    }
    void automaticChangesAndDefault() {
        select(Prefs::VCC_FORCE_PYROWAVE);
        prefs->setProperty("width",2560); prefs->setProperty("height",1440); prefs->setProperty("fps",120);
        QCOMPARE(prefs->bitrateKbps,708000); QVERIFY(prefs->autoAdjustBitrate);
        prefs->setProperty("width",1920); prefs->setProperty("height",1080);
        QCOMPARE(prefs->bitrateKbps,398000);
        prefs->setProperty("fps",60); QCOMPARE(prefs->bitrateKbps,199000);
        select(Prefs::VCC_FORCE_AV1); QCOMPARE(prefs->bitrateKbps,20000);
        prefs->setProperty("enableYUV444",true); QCOMPARE(prefs->bitrateKbps,40000);
        prefs->setProperty("autoAdjustBitrate",false);
        // Manual equal-to-default values still have a way back to automatic.
        prefs->useDefaultBitrate(); QVERIFY(prefs->autoAdjustBitrate);
        select(Prefs::VCC_FORCE_PYROWAVE); QCOMPARE(prefs->bitrateKbps,199000);
        const auto expected = snapshot(prefs); QCOMPARE(restart(),expected);
    }
    void oldUnlockIgnored() {
        QSettings settings;
        settings.setValue("unlockbitrate",false); settings.setValue("autoadjustbitrate",false);
        settings.setValue("bitrate",350000); settings.sync(); prefs->reload();
        QCOMPARE(prefs->bitrateKbps,350000);
        settings.setValue("unlockbitrate",true); settings.sync(); prefs->reload();
        QCOMPARE(prefs->bitrateKbps,350000); QCOMPARE(prefs->maximumBitrateKbps(),500000);
    }
    void productionSettingsUi() {
        qmlRegisterSingletonType<Prefs>("StreamingPreferences",1,0,"StreamingPreferences",
            [](QQmlEngine* engine,QJSEngine*) -> QObject* {
                auto p=Prefs::get(engine); QQmlEngine::setObjectOwnership(p,QQmlEngine::CppOwnership); return p;
            });
        qmlRegisterSingletonType<DesktopStub>("SystemProperties",1,0,"SystemProperties",[](QQmlEngine*,QJSEngine*) -> QObject* { return new DesktopStub; });
        qmlRegisterSingletonType<NavigationStub>("SdlGamepadKeyNavigation",1,0,"SdlGamepadKeyNavigation",[](QQmlEngine*,QJSEngine*) -> QObject* { return new NavigationStub; });
        qmlRegisterSingletonType<ComputerStub>("ComputerManager",1,0,"ComputerManager",[](QQmlEngine*,QJSEngine*) -> QObject* { return new ComputerStub; });
        WindowStub window;
        QQuickView view;
        view.rootContext()->setContextProperty("window",&window);
        view.rootContext()->setContextProperty("stackView",view.contentItem());
        view.setResizeMode(QQuickView::SizeRootObjectToView);
        view.resize(1400,1000);
        view.setSource(QUrl("qrc:/settings/SettingsView.qml"));
        QCOMPARE(view.status(),QQuickView::Ready);
        view.show(); QVERIFY(QTest::qWaitForWindowExposed(&view));
        auto root=view.rootObject(); QVERIFY(root);
        auto expression=[root](const QString& code) {
            QQmlExpression e(QQmlEngine::contextForObject(root),root,code);
            auto result=e.evaluate(); if(e.hasError()) qWarning()<<e.error(); return result;
        };
        auto codec=expression("codecComboBox").value<QObject*>(); QVERIFY(codec);
        auto slider=expression("slider").value<QObject*>(); QVERIFY(slider);
        auto reset=expression("resetBitrateButton").value<QObject*>(); QVERIFY(reset);
        QCOMPARE(expression("codecListModel.count").toInt(),5);
        QVERIFY(expression("codecComboBox.parent === resolutionComboBox.parent && codecComboBox.parent === fpsComboBox.parent").toBool());
        QVERIFY(expression("(function() { var item = codecComboBox; while (item) { if (item === basicSettingsGroupBox) return true; item = item.parent; } return false; })()").toBool());
        int codecSelectors = 0;
        for (auto child : root->findChildren<QObject*>()) {
            QVERIFY(!child->property("text").toString().contains("Unlock bitrate",Qt::CaseInsensitive));
            if (child->metaObject()->indexOfMethod("textAt(int)") >= 0) {
                QString label;
                QVERIFY(QMetaObject::invokeMethod(child,"textAt",Q_RETURN_ARG(QString,label),Q_ARG(int,1)));
                if (label=="H.264") ++codecSelectors;
            }
        }
        QCOMPARE(codecSelectors,1);
        codec->setProperty("currentIndex",4);
        QVERIFY(QMetaObject::invokeMethod(codec,"activated",Q_ARG(int,4)));
        QCOMPARE(prefs->videoCodecConfig,Prefs::VCC_FORCE_PYROWAVE);
        QCOMPARE(slider->property("to").toInt(),3000000);
        QVERIFY(prefs->autoAdjustBitrate);
        QCOMPARE(slider->property("value").toInt(),88500);
        slider->setProperty("value",750000); QVERIFY(QMetaObject::invokeMethod(slider,"moved"));
        QCOMPARE(prefs->bitrateKbps,750000); QVERIFY(!prefs->autoAdjustBitrate);
        expression("resolutionComboBox.currentIndex = 2; resolutionComboBox.activated(2)");
        QVERIFY(expression("(function() { for (var i = 0; i < fpsListModel.count; i++) { if (Number(fpsListModel.get(i).video_fps) === 120 && !fpsListModel.get(i).is_custom) { fpsComboBox.currentIndex = i; fpsComboBox.activated(i); return true; } } return false; })()").toBool());
        QCOMPARE(prefs->width,2560); QCOMPARE(prefs->height,1440); QCOMPARE(prefs->fps,120);
        QCOMPARE(prefs->bitrateKbps,750000); QVERIFY(!prefs->autoAdjustBitrate);
        codec->setProperty("currentIndex",3); QVERIFY(QMetaObject::invokeMethod(codec,"activated",Q_ARG(int,3)));
        QCOMPARE(prefs->bitrateKbps,500000); QVERIFY(!prefs->autoAdjustBitrate);
        QCOMPARE(slider->property("to").toInt(),500000);
        QCOMPARE(slider->property("value").toInt(),500000);
        QVERIFY(reset->property("visible").toBool());
        QVERIFY(QMetaObject::invokeMethod(reset,"clicked"));
        QVERIFY(prefs->autoAdjustBitrate);
        codec->setProperty("currentIndex",4); QVERIFY(QMetaObject::invokeMethod(codec,"activated",Q_ARG(int,4)));
        prefs->setProperty("width",2560); prefs->setProperty("height",1440); prefs->setProperty("fps",120);
        QCOMPARE(prefs->bitrateKbps,708000); QCOMPARE(slider->property("value").toInt(),708000);
        QVERIFY(reset->property("text").toString().contains("708"));
        QVERIFY(!reset->property("visible").toBool());
        // Use the actual tab focus chain and responsive Flow at both sizes.
        auto resolution=qobject_cast<QQuickItem*>(expression("resolutionComboBox").value<QObject*>()); QVERIFY(resolution);
        resolution->forceActiveFocus(Qt::TabFocusReason); QTest::keyClick(&view,Qt::Key_Tab);
        QVERIFY(expression("fpsComboBox.activeFocus").toBool());
        QTest::keyClick(&view,Qt::Key_Tab); QVERIFY(expression("codecComboBox.activeFocus").toBool());
        QTest::qWait(100);
        QVERIFY(view.grabWindow().save("settings-wide.png"));
        view.resize(640,1000); QTest::qWait(100);
        QVERIFY(expression("codecComboBox.x + codecComboBox.width <= codecComboBox.parent.width + 1").toBool());
        QVERIFY(expression("codecComboBox.y >= fpsComboBox.y").toBool());
        QVERIFY(view.grabWindow().save("settings-narrow.png"));
    }
};
int main(int argc,char** argv) {
    QGuiApplication app(argc,argv);
    QCoreApplication::setOrganizationName("AsteriaSettingsTests");
    QCoreApplication::setApplicationName("Preferences");
    QSettings::setDefaultFormat(QSettings::IniFormat);
    if (argc==3 && QString::fromLocal8Bit(argv[1])=="--read-settings") {
        QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,QString::fromLocal8Bit(argv[2]));
        QTextStream(stdout)<<QJsonDocument(snapshot(Prefs::get())).toJson(QJsonDocument::Compact);
        return 0;
    }
    SettingsTests tests;
    return QTest::qExec(&tests,argc,argv);
}
#include "settings_tests.moc"
