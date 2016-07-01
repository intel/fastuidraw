#pragma once

#include <QtGlobal>
#include <QtOpenGL>

#include <QWidget>
#include <QGLWidget>

#include "generic_command_line.hpp"

class qt_demo;


class demo_widget:public QWidget
{
public:
  demo_widget(qt_demo *q);
  ~demo_widget();

  virtual
  void
  paintEvent(QPaintEvent *);

  bool
  event(QEvent *ev);

private:
  qt_demo *m_demo;
};

class demo_widget_gl:public QGLWidget
{
public:
  demo_widget_gl(qt_demo *q);
  ~demo_widget_gl();

  virtual
  void
  paintEvent(QPaintEvent *);

  virtual
  bool
  event(QEvent *ev);

private:
  qt_demo *m_demo;
};

class qt_demo:public command_line_register
{
public:
  qt_demo(void);

  virtual
  ~qt_demo()
  {}

  int
  main(int argc, char **argv);

  void
  end_demo(int return_value);

  QSizeF
  dimensions(void);

  virtual
  void
  derived_init(int w, int h) = 0;

  virtual
  void
  on_widget_delete(void) = 0;

  virtual
  void
  paint(QPainter *p) = 0;

  virtual
  void
  handle_event(QEvent *ev) = 0;

private:
  friend class demo_widget;
  friend class demo_widget_gl;

  QGLFormat
  computeFormat(void);

  Qt::WindowFlags
  computeFlags(void);

  command_line_argument_value<int> m_red_bits;
  command_line_argument_value<int> m_green_bits;
  command_line_argument_value<int> m_blue_bits;
  command_line_argument_value<int> m_alpha_bits;
  command_line_argument_value<int> m_depth_bits;
  command_line_argument_value<int> m_stencil_bits;
  command_line_argument_value<bool> m_fullscreen;
  command_line_argument_value<bool> m_hide_cursor;
  command_line_argument_value<bool> m_use_msaa;
  command_line_argument_value<int> m_msaa;

  command_line_argument_value<bool> m_use_gl_widget;
  command_line_argument_value<int> m_gl_major, m_gl_minor;
  command_line_argument_value<bool> m_gl_forward_compatible_context;
  command_line_argument_value<bool> m_gl_debug_context;
  command_line_argument_value<bool> m_gl_core_profile;

  command_line_argument_value<std::string> m_log_gl_commands;
  command_line_argument_value<std::string> m_log_alloc_commands;
  command_line_argument_value<bool> m_print_gl_info;

  bool m_inited;
  QWidget *m_widget;
};
