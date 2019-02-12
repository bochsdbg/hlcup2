TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
CONFIG -= qt

QMAKE_CFLAGS += -march=native

QMAKE_CFLAGS_RELEASE += -static -static-libgcc
QMAKE_LFLAGS_RELEASE += -static -static-libgcc

SOURCES += \
        main.c
