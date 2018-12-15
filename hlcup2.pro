CONFIG -= qt
CONFIG += c++17 console

TARGET = htcup2

INCLUDEPATH += \
    platform/x86_64 \
    platform/linux

SOURCES += \
    main.cpp

HEADERS += \
    TcpSocket.hpp \
    platform/x86_64/syscall.hpp \
    platform/linux/socket.hpp

