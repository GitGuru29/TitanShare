#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QIcon>
#include <QDir>
#include <QStandardPaths>
#include <QFileSystemWatcher>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>
#include <unistd.h>

class AppModel : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString pinCode READ pinCode NOTIFY pinCodeChanged)
    Q_PROPERTY(QString deviceName READ deviceName NOTIFY deviceNameChanged)
public:
    QString m_pinCode = "------";
    QString m_deviceName = "Linux PC";
    
    QString pinCode() const { return m_pinCode; }
    void setPinCode(const QString& p) { if(m_pinCode != p) { m_pinCode = p; emit pinCodeChanged(); } }
    
    QString deviceName() const { return m_deviceName; }
    void setDeviceName(const QString& n) { if(m_deviceName != n) { m_deviceName = n; emit deviceNameChanged(); } }
signals:
    void pinCodeChanged();
    void deviceNameChanged();
};

// ─── IPC: read the PIN JSON written by the daemon ────────────────────────────
// Must match config::PIN_IPC_PATH in the daemon (/run/titanshare/ via RuntimeDirectory)
static const QString PIN_IPC_PATH = "/run/titanshare/titanshare-pin.json";

static QString readPin(QString& hostOut) {
    QFile f(PIN_IPC_PATH);
    if (!f.open(QIODevice::ReadOnly)) return "------";
    auto doc = QJsonDocument::fromJson(f.readAll());
    auto obj = doc.object();
    hostOut = obj.value("host").toString("Linux PC");
    return obj.value("pin").toString("------");
}

static QString localHostname() {
    char buf[256]{};
    gethostname(buf, sizeof(buf) - 1);
    return QString::fromUtf8(buf);
}

int main(int argc, char *argv[]) {
    qputenv("QT_ENABLE_HIGHDPI_SCALING", "1");
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

    QGuiApplication app(argc, argv);
    app.setApplicationName("TitanShare");
    app.setOrganizationName("TitanShare");

    QQmlApplicationEngine engine;

    AppModel *appModel = new AppModel();
    engine.rootContext()->setContextProperty("AppModel", appModel);

    // ── Watch for daemon PIN updates ─────────────────────────────────
    QFileSystemWatcher watcher;
    watcher.addPath(PIN_IPC_PATH);
    // Also poll once per second in case inotify misses the first write
    QTimer* pollTimer = new QTimer(&app);
    pollTimer->setInterval(1000);
    pollTimer->start();

    auto refreshPin = [&]() {
        QString h;
        QString p = readPin(h);
        if (h.isEmpty()) h = localHostname();
        appModel->setDeviceName(h);
        appModel->setPinCode(p.isEmpty() || p == "------" ? "------" : p);
    };

    refreshPin();

    QObject::connect(&watcher, &QFileSystemWatcher::fileChanged,
                     &app, [&](const QString&) {
        refreshPin();
        // Re-add in case inotify removed it after the write
        if (!watcher.files().contains(PIN_IPC_PATH))
            watcher.addPath(PIN_IPC_PATH);
    });
    QObject::connect(pollTimer, &QTimer::timeout, &app, [&]() {
        refreshPin();
    });

    // ── Load QML ─────────────────────────────────────────────────────
    const QUrl url(QStringLiteral("qrc:/qml/Main.qml"));
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                     &app, [url](QObject *obj, const QUrl &objUrl) {
        if (!obj && url == objUrl)
            QCoreApplication::exit(-1);
    }, Qt::QueuedConnection);

    engine.load(url);
    return app.exec();
}

#include "main.moc"
