#include "MainWindow.h"

#include <QApplication>
#include <QString>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    MainWindow window(QStringLiteral(ALNORIS_PROJECT_ROOT));
    window.show();
    return app.exec();
}
