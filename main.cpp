
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include "devicebackend.h"
#include "theorymanager.h"

int main(int argc, char *argv[]){
    // 支持高DPI
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);

	QGuiApplication app(argc, argv);

    // 注册 C++ 类 (使其能在 QML 中作为类型使用)
	qmlRegisterType<DeviceBackend>("DeviceBackend", 1, 0, "DeviceBackend");
    qmlRegisterType<TheoryManager>("TheoryModule", 1, 0, "TheoryManager");

	QQmlApplicationEngine engine;

    const QUrl url(QStringLiteral("qrc:/main.qml"));

    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,&app,[url](QObject *obj, const QUrl &objUrl){
        if (!obj && url == objUrl)
            QCoreApplication::exit(-1);
    },Qt::QueuedConnection);

    engine.load(url);

	return app.exec();
}
