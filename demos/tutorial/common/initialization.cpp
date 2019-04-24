/*!
 * \file initialization.cpp
 * \brief file initialization.cpp
 *
 * Copyright 2019 by Intel.
 *
 * Contact: kevin.rogovin@intel.com
 *
 * This Source Code Form is subject to the
 * terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with
 * this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 *
 * \author Kevin Rogovin <kevin.rogovin@intel.com>
 */

//! [ExampleInitialization]

#include "initialization.hpp"

static
void*
get_proc(fastuidraw::c_string proc_name)
{
  return SDL_GL_GetProcAddress(proc_name);
}

Initialization::
Initialization(DemoRunner *runner, int argc, char **argv):
  Demo(runner, argc, argv)
{
  /* The GL (or GLES) backend need a way to fetch the GL (or GLES)
   * function pointers. It is the application's responsibility to
   * provide to FastUIDraw a function to fetch the GL (or GLES)
   * function pointers. We wrap the SDL function SDL_GL_GetProcAddress()
   * in get_proc() static function to make sure that the function
   * signatures match precisely. DANGER: on MS-Windows, the function
   * to fetch GL function pointers requires that the GL context
   * with which they are used is current (this is not the case on
   * Unix). An additional danger with MS-Windows is that the function
   * pointers fetched may or may not be compatible with a different
   * GL context.
   */
  fastuidraw::gl_binding::get_proc_function(get_proc);

  /* Now that FastUIDraw can fetch the GL (or GLES) function pointers
   * we can now create our FastUIDraw objects.
   *
   * The first object to create is the fastuidraw::gl::PaintEngineGL
   * which embodies how FastUIDraw uses the GL (or GLES) to draw. A
   * fastuidraw::gl::PaintEngineGL's configuration is controlled by
   * a fastuidraw::gl::PaintEngineGL::ConfigurationGL value. For this
   * example, we let FastUIDraw query the GL context properties and
   * from that decide all the values within the configuration.
   * The class fastuidraw::gl::PaintEngineGL is thread safe and an
   * application should create only a single such object.
   */
  fastuidraw::gl::PainterEngineGL::ConfigurationGL engine_params;
  engine_params.configure_from_context();

  m_painter_engine_gl = fastuidraw::gl::PainterEngineGL::create(engine_params);

  /* Now that we have the fastuidraw::PainterEngine derived class,
   * we can create our fastuidraw::Painter object. A fastuidraw::Painter
   * object is a HEAVY object (because it implements various pools)
   * and such objects should not be created within ones render/event
   * loops. However, it is perfectly fine to create multiple
   * fastuidraw::Painter objects using the same fastuidraw::PainterEngine
   * object. In addition, the class fastuidraw::Painter is NOT thread
   * safe and a fixed fastuidraw::Painter should only be accessed
   * by one thread at time.
   *
   * FastUIDraw makes heavy use of reference counting to make memory
   * managment easier. In addition, FastUIDraw provides the macros
   * FASTUIDRAWnew and FASTUIDRAWdelete which under debug builds
   * record object creation and deletion so that if an object is
   * created and not deleted an error message is printed and to
   * also print an error message if an object is deleted with
   * FASTUIDRAWdelete that was not created with FASTUIDRAWnew. The
   * main consequence of the system is that the creation of any
   * FastUIDraw objects must be done with FASTUIDRAWnew.
   */
  m_painter = FASTUIDRAWnew fastuidraw::Painter(m_painter_engine_gl);

  /* Create the surface to which the Painter will render content. */
  m_surface_gl = FASTUIDRAWnew fastuidraw::gl::PainterSurfaceGL(window_dimensions(), *m_painter_engine_gl);
}

void
Initialization::
handle_event(const SDL_Event &ev)
{
  switch (ev.type)
    {
      case SDL_WINDOWEVENT:
      if (ev.window.event == SDL_WINDOWEVENT_RESIZED)
        {
          /* The window is resized so we need to adjust our surface
           * to the new size of the window. The reference_counted_ptr
           * interface will automacially delete the underlying
           * PainterSurfaceGL that we had made earlier
           */
          fastuidraw::ivec2 new_dims(ev.window.data1,ev.window.data2);
          m_surface_gl = FASTUIDRAWnew fastuidraw::gl::PainterSurfaceGL(new_dims, *m_painter_engine_gl);
        }
      break;
    }
}

void
Initialization::
draw_frame(void)
{
  /* We first need to set the viewport for surface when we start to draw */
  fastuidraw::vec2 window_dims(window_dimensions());
  fastuidraw::PainterSurface::Viewport vwp(0, 0, window_dims.x(), window_dims.y());
  m_surface_gl->viewport(vwp);

  /* fastuidraw::Painter builds commands to send to the underlying 3D API.
   * Drawing commands to Painter may only be executed within a begin()/end()
   * pair. In addition, the effects on the surface to which the Painter
   * is drawing does not take effect until end() is called.
   */
  m_painter->begin(m_surface_gl, fastuidraw::Painter::y_increases_downwards);

  /* Later examples we will get into the various drawing commands of
   * fastuidraw::Painter, for now we just draw a white rect.
   */
  fastuidraw::PainterBrush brush;

  brush.color(1.0f, 1.0f, 1.0f, 1.0f);
  m_painter->fill_rect(&brush,
                       fastuidraw::Rect()
                       .min_point(window_dims * 0.25f)
                       .max_point(window_dims * 0.75f));

  /* Issue fastuidraw::Painter::end() to send the accumulate draw commands
   * to the underlying 3D API to draw the content.
   */
  m_painter->end();

  /* FastUIDraw's GL (and GLES) backend provide automatic GL (GLES)
   * function pointer fetching. In addition, with the FastUIDraw debug
   * build, call the GL (or GLES) functions through FastUIDraw adds
   * GL error checking that will print to stderr any GL errors encounted
   * together with the file and line number of the GL function that
   * triggered the error. To call the GL function glFoo through this
   * interface, one uses the macro-function fastuidraw_glFoo. The release
   * builds do NOT do the error checking making the cost of going through
   * the fastuidraw_glFoo macros to have no overhead.
   */

  /* Make sure we are rendering to the default fraembuffer of GL */
  fastuidraw_glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

  /* Clear the framebuffer */
  fastuidraw_glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

  /* blit the contents of m_surface to the default framebuffer
   * with nearest filtering
   */
  m_surface_gl->blit_surface(GL_NEAREST);
}

Initialization::
~Initialization()
{
  /* Recall that demo_framework does not destroy the window
   * or GL context untils its dtor. Hence, the GL context is
   * current at our dtor. When the reference counted pointers
   * have their dtors' called, they will decrement the reference
   * count which when it reaches zero will delete the object.
   * Of critical importance is that the last reference to the
   * fastuidraw::gl::PainterEngineGL goes away with a GL context
   * current so that the dtor of fastuidraw::gl::PainterEngineGL
   * will be able to call GL (or GLES) functions to free GL
   * resources. Since the dtor's of the reference counters
   * are called by C++ once this function exists, we have nothing
   * to do actually for FastUIDraw cleanup.
   */
}

//! [ExampleInitialization]
