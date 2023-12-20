#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include "controls.h"
#include "device.h"
#include "renderer.h"
#include <QQmlContext>

int main(int argc, char *argv[])
{

    QGuiApplication app(argc, argv);
    QQmlApplicationEngine engine;

    engine.rootContext()->setContextProperty("device",&Device::deviceListModel);
    engine.rootContext()->setContextProperty("format",&Device::formatListModel);
    engine.rootContext()->setContextProperty("resolution",&Device::resolutionListModel);
    engine.rootContext()->setContextProperty("fps",&Device::fpsListModel);

    qmlRegisterType<Device>("qml.components.device", 1, 0, "Devices");
    qmlRegisterType<Buttons>("qml.components.buttons", 1, 0, "Control");
    qmlRegisterType<Renderer>("qml.components.frames", 1, 0, "Renderer");

    engine.load(QUrl(QStringLiteral("qrc:/main.qml")));

    if (engine.rootObjects().isEmpty())
        return -1;

    return app.exec();
}
