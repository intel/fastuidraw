QT += opengl
HEADERS += qt_demo.hpp
LIBS += -lboost_system -lboost_thread

SOURCES = qt_demo.cpp \
	generic_command_line.cpp \
        PanZoomTracker.cpp \
	main.cpp
