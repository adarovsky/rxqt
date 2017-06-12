QT += core widgets concurrent

CONFIG += c++14

TARGET = rxsample
CONFIG += console
CONFIG -= app_bundle

#QMAKE_CXXFLAGS += -fsanitize=thread

TEMPLATE = app

INCLUDEPATH += Rx/v2/src
INCLUDEPATH += include

SOURCES += \
    sample/main.cpp \
    sample/sampledump.cpp

HEADERS += \
    include/rxqt.hpp \
    include/rxqt_signal.hpp \
    include/rxqt_event.hpp \
    include/rxqt-eventloop.hpp \
    include/rx-drop_map.hpp \
    include/rxqt_slot.hpp \
    sample/sampledump.h

