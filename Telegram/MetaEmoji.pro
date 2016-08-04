QT += core 

CONFIG(debug, debug|release) {
    DEFINES += _DEBUG
    OBJECTS_DIR = ./../DebugIntermediateEmoji
    MOC_DIR = ./GeneratedFiles/Debug
    DESTDIR = ./../DebugEmoji
}
CONFIG(release, debug|release) {
    OBJECTS_DIR = ./../ReleaseIntermediateEmoji
    MOC_DIR = ./GeneratedFiles/Release
    DESTDIR = ./../ReleaseEmoji
}

CONFIG += plugin static

macx {
    QMAKE_INFO_PLIST = ./SourceFiles/_other/Emoji.plist
    QMAKE_LFLAGS += -framework Cocoa
}

SOURCES += \
    ./SourceFiles/_other/memain.cpp \
    ./SourceFiles/_other/genemoji.cpp \

HEADERS += \
    ./SourceFiles/_other/memain.h \
    ./SourceFiles/_other/genemoji.h \

include(qt_static.pri)
