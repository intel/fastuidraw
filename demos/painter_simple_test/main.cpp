#include <iostream>
#include <sstream>
#include <fstream>

#include "sdl_painter_demo.hpp"
#include "simple_time.hpp"
#include "PanZoomTracker.hpp"
#include "read_path.hpp"
#include "ImageLoader.hpp"
#include "colorstop_command_line.hpp"

using namespace fastuidraw;

class painter_simple_test:public sdl_painter_demo
{
public:
  painter_simple_test(void)
  {}

protected:
  void
  draw_frame(void);

  void
  handle_event(const SDL_Event &ev);
};

void
painter_simple_test::
handle_event(const SDL_Event &ev)
{
  switch(ev.type)
    {
      case SDL_QUIT:
        end_demo(0);
        break;
    }
}

void
painter_simple_test::
draw_frame(void)
{
  m_painter->begin(m_surface, Painter::y_increases_downwards);

  fastuidraw::PainterBrush transparentYellow;
  transparentYellow.pen(1.0f, 1.0f, 0.0f, 0.5f);
  m_painter->fill_rect(fastuidraw::PainterData(&transparentYellow),
                       fastuidraw::vec2(0.0f, 0.0f),
                       fastuidraw::vec2(dimensions()));
  m_painter->end();
  fastuidraw_glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
  fastuidraw_glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
  m_surface->blit_surface(GL_NEAREST);
}

int
main(int argc, char **argv)
{
  painter_simple_test P;
  return P.main(argc, argv);
}
