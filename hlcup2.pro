CONFIG -= qt qtquickcompiler
CONFIG += c++17 console

TARGET = hlcup2

INCLUDEPATH += \
    platform/x86_64 \
    platform/linux

SOURCES += \
    miniz.c \
    main.cpp

QMAKE_CXXFLAGS_RELEASE = -g -march=nehalem -Ofast -fno-stack-protector -flto -std=c++17
QMAKE_CFLAGS_RELEASE = -g -march=nehalem -Ofast -fno-stack-protector -flto -std=c11
QMAKE_LFLAGS_RELEASE = -g -Wl,-O1,--sort-common,--as-needed,-z,relro,-z,now -flto

#QMAKE_CXXFLAGS_RELEASE += -fprofile-dir=/tmp/pgo -fprofile-generate=/tmp/pgo
#QMAKE_CFLAGS_RELEASE   += -fprofile-dir=/tmp/pgo -fprofile-generate=/tmp/pgo
#QMAKE_LFLAGS_RELEASE   += -fprofile-dir=/tmp/pgo -fprofile-generate=/tmp/pgo

QMAKE_CXXFLAGS_RELEASE += -fprofile-dir=/tmp/pgo -fprofile-use=/tmp/pgo -fprofile-correction
QMAKE_CFLAGS_RELEASE   += -fprofile-dir=/tmp/pgo -fprofile-use=/tmp/pgo -fprofile-correction
QMAKE_LFLAGS_RELEASE   += -fprofile-dir=/tmp/pgo -fprofile-use=/tmp/pgo -fprofile-correction

#message($$QMAKE_LFLAGS_RELEASE)

DEFINES += _LARGEFILE64_SOURCE \
  MINIZ_NO_TIME \
  MINIZ_DISABLE_ZIP_READER_CRC32_CHECKS \
  MINIZ_NO_ARCHIVE_WRITING_APIS \
  MINIZ_NO_ZLIB_COMPATIBLE_NAMES \
  MINIZ_NO_ARCHIVE_WRITING_APIS

HEADERS += \
    TcpSocket.hpp \
    platform/x86_64/syscall.hpp \
    platform/linux/socket.hpp \
    miniz.h \
    AccountParser.hpp \
    Account.hpp \
    common.hpp \
    HttpParser.hpp \
    Request.hpp \
    ParseUtils.hpp \
    Time.hpp \
    platform/linux/io.hpp \
    fmt/format.hpp

RE2C_FILES += \
    AccountParser.cpp.re

RAGEL_FILES += \
    HttpParser.cpp.rl

DEPENDPATH += .
DEPENDPATH += ${OUT_PWD}
INCLUDEPATH += ${OUT_PWD}

re2c.name = RE2C
re2c.output  = ${QMAKE_FILE_BASE}
re2c.variable_out = SOURCES
re2c.commands = re2c -cfo ${QMAKE_FILE_OUT} ${QMAKE_FILE_NAME}
re2c.input = RE2C_FILES
re2c.dependency_type = TYPE_C
QMAKE_EXTRA_COMPILERS += re2c

ragel.output = $$OUT_PWD/ragel_${QMAKE_FILE_IN_BASE}
ragel.input = RAGEL_FILES
ragel.commands = ragel -G2 ${QMAKE_FILE_IN} -o ${QMAKE_FILE_OUT}
ragel.variable_out = SOURCES
ragel.name = RAGEL
QMAKE_EXTRA_COMPILERS += ragel
