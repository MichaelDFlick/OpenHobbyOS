#include <QtWidgets/QApplication>
#include <QtWidgets/QLabel>
#include <QtCore/QTimer>
#include <QtGui/QFont>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    QLabel label("Hello from Qt6 on OpenHobbyOS!");
    QFont font("Monospace", 24);
    label.setFont(font);
    label.setAlignment(Qt::AlignCenter);
    label.setMinimumSize(600, 200);
    label.show();

    QTimer::singleShot(5000, &app, &QApplication::quit);

    return app.exec();
}
