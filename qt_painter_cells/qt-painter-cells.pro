QT += opengl
HEADERS += qt_demo.hpp
QMAKE_CXXFLAGS += -std=c++11
LIBS += -lboost_system -lboost_thread

SOURCES = qt_demo.cpp \
	generic_command_line.cpp \
        PainterWidget.cpp \
        random.cpp \
        cell_group.cpp \
        table.cpp \
        cell.cpp \
        PanZoomTracker.cpp \
	main.cpp
