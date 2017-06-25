#-------------------------------------------------
#
# Project created by QtCreator 2017-06-22T19:05:05
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = fcvtc
TEMPLATE = app
QMAKE_CXXFLAGS += -Wno-write-strings


win32:!win32-g++ {
  contains(QT_ARCH, "x86_64") {
    message(Building for win 64bit)
#    LIBS += -lws2_32 -lkernel32 -luser32 -lgdi32 -lwinspool -lshell32 -lole32 -loleaut32 -luuid -lcomdlg32 -ladvapi32 -lrpcrt4
  } else {
    message(Building for win 32bit)
  }
}



macx {
  message(Building for macx)
  CONFIG += app_bundle
  QMAKE_LFLAGS_RELEASE += -framework cocoa
  QMAKE_LFLAGS += -Wl,-rpath,@executable_path/../Frameworks
  QMAKE_MACOSX_DEPLOYMENT_TARGET = 10.10
}



linux-g++ {
  message(Building for linux-g++)
  LTKLIBDIR = ../LTK/LTKCPP/Library
#  QMAKE_RPATHDIR = $$LIBDIR
  INCLUDEPATH += $$LTKLIBDIR
  LIBS += $$LTKLIBDIR/libltkcpp.a
}

message(QMAKE_REL_RPATH_BASE: $$QMAKE_REL_RPATH_BASE)

message(INCLUDE: $$INCLUDEPATH)
message(LIBS: $$LIBS)
#message(PATH: $$PATH)









SOURCES += main.cpp\
        mainwindow.cpp \
    creader.cpp

HEADERS  += mainwindow.h \
    creader.h

FORMS    += mainwindow.ui
