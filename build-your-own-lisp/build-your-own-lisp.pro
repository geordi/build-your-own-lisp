TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += parsing.c \
    mpc.c

LIBS += -ledit

HEADERS += \
    mpc.h

CFLAGS += -fpermissive
