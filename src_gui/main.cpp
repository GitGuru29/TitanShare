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
    Q_PROPERTY(bool transferActive READ transferActive NOTIFY transferChanged)
    Q_PROPERTY(bool transferIsSending READ transferIsSending NOTIFY transferChanged)
    Q_PROPERTY(QString transferFilename READ transferFilename NOTIFY transferChanged)
    Q_PROPERTY(double transferProgress READ transferProgress NOTIFY transferChanged)

public:
    QString m_pinCode = "------";
    QString m_deviceName = "Linux PC";
    
    bool m_transferActive = false;
    bool m_transferIsSending = false;
    QString m_transferFilename = "";
    double m_transferProgress = 0.0;
    
    QString pinCode() const { return m_pinCode; }
    void setPinCode(const QString& p) { if(m_pinCode != p) { m_pinCode = p; emit pinCodeChanged(); } }
    
    QString deviceName() const { return m_deviceName; }
    void setDeviceName(const QString& n) { if(m_deviceName != n) { m_deviceName = n; emit deviceNameChanged(); } }

    bool transferActive() const { return m_transferActive; }
    bool transferIsSending() const { return m_transferIsSending; }
    QString transferFilename() const { return m_transferFilename; }
    double transferProgress() const { return m_transferProgress; }

    void updateTransferState(bool active, bool isSending, const QString& filename, double progress) {
        if (m_transferActive != active || m_transferIsSending != isSending || 
            m_transferFilename != filename || m_transferProgress != progress) {
            m_transferActive = active;
            m_transferIsSending = isSending;
            m_transferFilename = filename;
            m_transferProgress = progress;
            emit transferChanged();
        }
    }

    signals:
    void pinCodeChanged();
    void deviceNameChanged();
    void transferChanged();

public:
    Q_INVOKABLE void handleDroppedFiles(const QList<QUrl>& urls) {
        QString home = qgetenv("HOME");
        QString destDir = (!home.isEmpty())
            ? home + "/.local/share/titanshare/send_to_android/"
            : "/var/lib/titanshare/send_to_android/";
        QDir dir(destDir);
        if (!dir.exists()) {
            dir.mkpath(".");
        }
        
        for (const QUrl& url : urls) {
            if (url.isLocalFile()) {
                QString src = url.toLocalFile();
                QFileInfo fi(src);
                if (fi.isFile()) {
                    QString dst = destDir + fi.fileName();
                    QFile::remove(dst); // Overwrite if exists
                    QFile::copy(src, dst);
                }
            }
        }
    }
};

// ─── IPC: read the PIN JSON written by the daemon ────────────────────────────
static QString getIpcFilePath(const QString& filename) {
    if (QFile::exists("/run/titanshare/" + filename)) {
        return "/run/titanshare/" + filename;
    }
    QString xdgRun = qgetenv("XDG_RUNTIME_DIR");
    if (!xdgRun.isEmpty()) {
        QString p = xdgRun + "/titanshare/" + filename;
        if (QFile::exists(p)) return p;
    }
    QString home = qgetenv("HOME");
    if (!home.isEmpty()) {
        QString p = home + "/.local/share/titanshare/" + filename;
        if (QFile::exists(p)) return p;
    }
    if (QFile::exists("/tmp/titanshare/" + filename)) {
        return "/tmp/titanshare/" + filename;
    }
    if (!xdgRun.isEmpty()) return xdgRun + "/titanshare/" + filename;
    if (!home.isEmpty()) return home + "/.local/share/titanshare/" + filename;
    return "/run/titanshare/" + filename;
}

static QString readPin(QString& hostOut) {
    QString path = getIpcFilePath("titanshare-pin.json");
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return "------";
    auto doc = QJsonDocument::fromJson(f.readAll());
    auto obj = doc.object();
    hostOut = obj.value("host").toString("Linux PC");
    return obj.value("pin").toString("------");
}

static void readTransferState(AppModel* model) {
    QString path = getIpcFilePath("transfer.json");
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        model->updateTransferState(false, false, "", 0.0);
        return;
    }
    auto doc = QJsonDocument::fromJson(f.readAll());
    auto obj = doc.object();
    model->updateTransferState(
        obj.value("active").toBool(false),
        obj.value("is_sending").toBool(false),
        obj.value("filename").toString(""),
        obj.value("progress").toDouble(0.0)
    );
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
    QString curPinPath = getIpcFilePath("titanshare-pin.json");
    QString curTransferPath = getIpcFilePath("transfer.json");
    if (QFile::exists(curPinPath)) watcher.addPath(curPinPath);
    if (QFile::exists(curTransferPath)) watcher.addPath(curTransferPath);

    // Also poll once per second in case inotify misses the first write
    QTimer* pollTimer = new QTimer(&app);
    pollTimer->setInterval(100); // 100ms for smooth animations
    pollTimer->start();

    auto refreshPin = [&]() {
        QString h;
        QString p = readPin(h);
        if (h.isEmpty()) h = localHostname();
        appModel->setDeviceName(h);
        appModel->setPinCode(p.isEmpty() || p == "------" ? "------" : p);
    };

    refreshPin();
    readTransferState(appModel);

    QObject::connect(&watcher, &QFileSystemWatcher::fileChanged,
                     &app, [&](const QString& path) {
        if (path.endsWith("titanshare-pin.json")) {
            refreshPin();
            if (!watcher.files().contains(path) && QFile::exists(path)) watcher.addPath(path);
        } else if (path.endsWith("transfer.json")) {
            readTransferState(appModel);
            if (!watcher.files().contains(path) && QFile::exists(path)) watcher.addPath(path);
        }
    });

    QObject::connect(pollTimer, &QTimer::timeout, &app, [&]() {
        refreshPin();
        readTransferState(appModel);
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
