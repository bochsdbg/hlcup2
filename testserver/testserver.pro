TEMPLATE = app
CONFIG += console c++17
CONFIG -= app_bundle
CONFIG -= qt

#QMAKE_CXXFLAGS_RELEASE = -std=c++17 -Ofast -fno-stack-protector -fno-ident -fno-rtti -fno-exceptions -fomit-frame-pointer -fno-asynchronous-unwind-tables
#QMAKE_LFLAGS_RELEASE = -nostdlib -shared -Wl,--build-id=none -Wl,-gc-sections -rdynamic -Wl,-as-needed -Wl,-x -Wl,--no-ld-generated-unwind-info

#QMAKE_CXXFLAGS_RELEASE -= -O2
#QMAKE_CXXFLAGS_RELEASE += -g
#QMAKE_LFLAGS -= -O1

QMAKE_CXXFLAGS += -g -static -static-libgcc -pthread
QMAKE_LFLAGS += -g -static -static-libgcc -pthread

SOURCES += \
        main.cpp

INCLUDEPATH = \
  ../platform/x86_64 \
  ../platform/linux

HEADERS += \
    virtio_net.hpp \
    utils.hpp
