QT += core widgets

CONFIG += c++14

TARGET = rxsample
CONFIG += console
CONFIG -= app_bundle

TEMPLATE = app

INCLUDEPATH += Rx/v2/src
INCLUDEPATH += include

SOURCES += \
    sample/main.cpp

HEADERS += \
    include/rxqt.hpp \
    include/rxqt_signal.hpp \
    include/rxqt_event.hpp \
    include/rxqt-eventloop.hpp \
    include/rx-drop_backpressure.hpp

