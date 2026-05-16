#include "MainWindow.h"

#include <QApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMessageBox>
#include <QString>
#include <QSurfaceFormat>
#include <QTextStream>
#include <QTimer>

namespace {

QString startupLogPath()
{
    return QDir(QCoreApplication::applicationDirPath()).filePath("alnoris_startup.log");
}

void appendStartupLog(const QString& line)
{
    QFile file(startupLogPath());
    if (!file.open(QIODevice::Append | QIODevice::Text)) {
        return;
    }
    QTextStream stream(&file);
    stream << QDateTime::currentDateTime().toString(Qt::ISODate) << " " << line << '\n';
}

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication::setAttribute(Qt::AA_UseDesktopOpenGL);
    QSurfaceFormat format;
    format.setRenderableType(QSurfaceFormat::OpenGL);
    format.setVersion(3, 3);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setDepthBufferSize(24);
    format.setStencilBufferSize(8);
    format.setAlphaBufferSize(8);
    format.setSwapBehavior(QSurfaceFormat::DoubleBuffer);
    format.setSamples(4);
    QSurfaceFormat::setDefaultFormat(format);

    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("Alnoris Battery Simulator"));
    appendStartupLog(QString("startup begin | app_dir=%1 | cwd=%2 | project_root=%3")
        .arg(QCoreApplication::applicationDirPath(), QDir::currentPath(), QStringLiteral(ALNORIS_PROJECT_ROOT)));

    try {
        MainWindow window(QStringLiteral(ALNORIS_PROJECT_ROOT));
        appendStartupLog("main window constructed");
        window.show();
        window.raise();
        window.activateWindow();
        QTimer::singleShot(0, [&window]() {
            window.raise();
            window.activateWindow();
        });
        appendStartupLog("main window shown");
        return app.exec();
    } catch (const std::exception& exc) {
        appendStartupLog(QString("startup exception: %1").arg(exc.what()));
        QMessageBox::critical(nullptr, "Startup Error", QString("The application failed to start.\n\n%1\n\nLog: %2").arg(exc.what(), startupLogPath()));
        return 1;
    } catch (...) {
        appendStartupLog("startup exception: unknown");
        QMessageBox::critical(nullptr, "Startup Error", QString("The application failed to start.\n\nLog: %1").arg(startupLogPath()));
        return 1;
    }
}
