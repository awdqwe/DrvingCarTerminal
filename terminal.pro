TEMPLATE = app
TARGET = CarTerminal
QT += core gui widgets network qml quick
CONFIG += c++11

# 链接底层硬件驱动库
LIBS += -lbcm2835

HEADERS += MFRC522.h devicebackend.h rfidthread.h theorymanager.h
SOURCES += main.cpp MFRC522.cpp devicebackend.cpp rfidthread.cpp theorymanager.cpp

RESOURCES += qml.qrc