/*!
 * \file qt_demo.cpp
 * \brief file qt_demo.cpp
 *
 * Copyright 2013 by Nomovok Ltd.
 *
 * Contact: info@nomovok.com
 *
 * This Source Code Form is subject to the
 * terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with
 * this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 *
 * \author Kevin Rogovin <kevin.rogovin@nomovok.com>
 *
 */

#include <QtGlobal>
#include <QtOpenGL>
#include <QGLWidget>
#include "qt_demo.hpp"

/////////////////////////////////////////
// demo_widget methods
demo_widget::
demo_widget(qt_demo *q):
  m_demo(q)
{
  if(q->m_hide_cursor.m_value)
    {
      setCursor(Qt::BlankCursor);
    }

  setAttribute(Qt::WA_AcceptTouchEvents);
  setAttribute(Qt::WA_DeleteOnClose);

  if(q->m_fullscreen.m_value)
    {
      showFullScreen();
    }
  else
    {
      show();
    }
}

demo_widget::
~demo_widget()
{
  m_demo->on_widget_delete();
}

void
demo_widget::
paintEvent(QPaintEvent *)
{
  if(!m_demo->m_inited)
    {
      m_demo->m_inited = true;
      m_demo->derived_init(width(), height());
    }

  QPainter painter(this);
  m_demo->paint(&painter);
  update();
}

bool
demo_widget::
event(QEvent *ev)
{
  m_demo->handle_event(ev);
  return QWidget::event(ev);
}


//////////////////////////////////////////
// demo_widget_gl methods
demo_widget_gl::
demo_widget_gl(qt_demo *q):
  QGLWidget(q->computeFormat(), 0, 0, q->computeFlags()),
  m_demo(q)
{
  if(q->m_hide_cursor.m_value)
    {
      setCursor(Qt::BlankCursor);
    }

  setAttribute(Qt::WA_AcceptTouchEvents);
  setAttribute(Qt::WA_DeleteOnClose);

  if(q->m_fullscreen.m_value)
    {
      showFullScreen();
    }
  else
    {
      show();
    }
}

demo_widget_gl::
~demo_widget_gl()
{
  m_demo->on_widget_delete();
}

void
demo_widget_gl::
paintEvent(QPaintEvent *)
{
  makeCurrent();

  if(!m_demo->m_inited)
    {
      m_demo->m_inited = true;
      m_demo->derived_init(width(), height());
    }

  QPainter painter(this);
  m_demo->paint(&painter);
  painter.end();
  update();
}

bool
demo_widget_gl::
event(QEvent *ev)
{
  m_demo->handle_event(ev);
  return QGLWidget::event(ev);
}

/////////////////////////////
// qt_demo methods
qt_demo::
qt_demo(void):
  m_red_bits(-1, "red_bits",
             "Bpp of red channel, non-positive values mean use Qt defaults",
             *this),
  m_green_bits(-1, "green_bits",
               "Bpp of green channel, non-positive values mean use Qt defaults",
               *this),
  m_blue_bits(-1, "blue_bits",
              "Bpp of blue channel, non-positive values mean use Qt defaults",
              *this),
  m_alpha_bits(-1, "alpha_bits",
               "Bpp of alpha channel, non-positive values mean use Qt defaults",
               *this),
  m_depth_bits(-1, "depth_bits",
               "Bpp of depth buffer, non-positive values mean use Qt defaults",
               *this),
  m_stencil_bits(-1, "stencil_bits",
                 "Bpp of stencil buffer, non-positive values mean use Qt defaults",
                 *this),
  m_fullscreen(false, "fullscreen", "fullscreen mode", *this),
  m_hide_cursor(false, "hide_cursor", "If true, hide the mouse cursor with a Qt call", *this),
  m_use_msaa(false, "enable_msaa", "If true enables MSAA", *this),
  m_msaa(4, "msaa_samples",
         "If greater than 0, specifies the number of samples "
         "to request for MSAA. If not, Qt will choose the "
         "sample count as the highest available value",
         *this),
  m_use_gl_widget(true, "use_gl_widget", "If true, use a QGLWidget. If false, use a QWidget", *this),
  m_gl_major(3, "gl_major", "GL major version", *this),
  m_gl_minor(3, "gl_minor", "GL minor version", *this),
  m_gl_forward_compatible_context(false, "foward_context", "if true request forward compatible context", *this),
  m_gl_debug_context(false, "debug_context", "if true request a context with debug", *this),
  m_gl_core_profile(true, "core_context", "if true request a context which is core profile", *this),
  m_log_gl_commands("", "log_gl", "if non-empty, GL commands are logged to the named file. "
                    "If value is stderr then logged to stderr, if value is stdout logged to stdout", *this),
  m_log_alloc_commands("", "log_alloc", "If non empty, logs allocs and deallocs to the named file", *this),
  m_print_gl_info(false, "print_gl_info", "If true print to stdout GL information", *this),
  m_demo_options("Demo Options", *this),
  m_inited(false),
  m_widget(NULL)
{}


QGLFormat
qt_demo::
computeFormat(void)
{
  QGLFormat fmt;

  if(m_red_bits.m_value>0)
    {
      fmt.setRedBufferSize(m_red_bits.m_value);
    }

  if(m_green_bits.m_value>0)
    {
      fmt.setGreenBufferSize(m_green_bits.m_value);
    }

  if(m_blue_bits.m_value>0)
    {
      fmt.setBlueBufferSize(m_blue_bits.m_value);
    }

  if(m_alpha_bits.m_value>0)
    {
      fmt.setAlphaBufferSize(m_alpha_bits.m_value);
    }

  if(m_depth_bits.m_value>0)
    {
      fmt.setDepthBufferSize(m_depth_bits.m_value);
    }

  if(m_stencil_bits.m_value>0)
    {
      fmt.setStencilBufferSize(m_stencil_bits.m_value);
    }

  if(m_use_msaa.m_value)
    {
      fmt.setSampleBuffers(true);
      if(m_msaa.m_value>0)
        {
          fmt.setSamples(m_msaa.m_value);
        }
    }

  return fmt;
}

Qt::WindowFlags
qt_demo::
computeFlags(void)
{
  return 0;
}

void
qt_demo::
end_demo(int return_value)
{
  /* todo: transmit return_value in the main.
   */
  (void)return_value;
  m_widget->close();
}

QSizeF
qt_demo::
dimensions(void)
{
  QSizeF return_value;
  if(m_widget != NULL)
    {
      return_value.rwidth() = m_widget->width();
      return_value.rheight() = m_widget->height();
    }
  else
    {
      return_value.rwidth() = 0.0;
      return_value.rheight() = 0.0;
    }
  return return_value;
}

int
qt_demo::
main(int argc, char **argv)
{
  if(argc == 2 and std::string(argv[1]) == std::string("-help"))
    {
      std::cout << "\n\nUsage: " << argv[0];
      print_help(std::cout);
      print_detailed_help(std::cout);

      std::cout << "\nDon't foget Qt's -geometry XxY+A+B to set "
                << "the window size to XxY and position to (A,B).\n";
      return 0;
    }

  //create widget and application:
  QApplication qapp(argc, argv);

  std::cout << "\n\nRunning: \"";
  for(int i=0;i<argc;++i)
    {
      std::cout << argv[i] << " ";
    }

  parse_command_line(argc, argv);
  std::cout << "\n\n" << std::flush;

  if(m_use_gl_widget.m_value)
    {
      m_widget = new demo_widget_gl(this);
    }
  else
    {
      m_widget = new demo_widget(this);
    }

  int return_value;
  return_value = qapp.exec();

  return return_value;
}
