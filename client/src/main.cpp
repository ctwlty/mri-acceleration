#include "app/MainWindow.h"

#include <QApplication>
#include <QFile>

static QString loadStyleSheet()
{
    QFile file(":/app.qss");
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }
    return QString::fromUtf8(file.readAll());
}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("场景化核磁共振控制台"));
    app.setApplicationDisplayName(QStringLiteral("场景化核磁共振控制台"));
    app.setStyleSheet(loadStyleSheet());

    MainWindow window;
    window.show();

    return app.exec();
}

