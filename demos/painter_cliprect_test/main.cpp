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

class painter_clip_test:public sdl_painter_demo
{
public:
  painter_clip_test(void);

protected:
  void
  derived_init(int, int);

  void
  draw_frame(void);

  void
  handle_event(const SDL_Event &ev);

private:

  void
  draw_scene(bool with_clipping);

  bool m_draw_overlay, m_use_matrices;
  PanZoomTrackerSDLEvent m_zoomer;
};

painter_clip_test::
painter_clip_test():
  m_draw_overlay(false),
  m_use_matrices(false)
{
}

void
painter_clip_test::
derived_init(int, int)
{
}


void
painter_clip_test::
draw_scene(bool with_clipping)
{
  vec2 wh(dimensions());

  /* set clipping to screen center
   */
  if (with_clipping)
    {
      m_painter->clip_in_rect(Rect()
                              .min_point(wh * 0.25f)
                              .max_point(wh * 0.75f));
    }

  /* draw a green quad over the clipped region
   */
  PainterBrush brush;
  brush.color(0.0f, 1.0f, 0.0f, 0.5f);
  m_painter->fill_rect(PainterData(&brush),
                       Rect().size(wh));

  /* draw half size.
   */
  if (m_use_matrices)
    {
      ScaleTranslate<float> sc(0.5f);
      m_painter->concat(sc.matrix3());
    }
  else
    {
      m_painter->scale(0.5f);
    }

  /* move (0,0) to wh*0.5
   */
  if (m_use_matrices)
    {
      ScaleTranslate<float> sc(wh * 0.5f);
      m_painter->concat(sc.matrix3());
    }
  else
    {
      m_painter->translate(wh * 0.5f);
    }

  /* clip again
   */
  if (with_clipping)
    {
      m_painter->clip_in_rect(Rect()
                              .min_point(wh * 0.125f)
                              .size(wh * 0.25f));
    }

  /* draw a blue quad
   */
  brush.color(0.0f, 0.0f, 1.0f, 0.5f);
  m_painter->fill_rect(PainterData(&brush),
                       Rect().size(wh * 0.5f));

  /* rotate by 30 degrees
   */
  float r = (0.125f + 0.25f) * 0.5f;

  m_painter->translate(wh * r);
  m_painter->rotate(30.0f * float(FASTUIDRAW_PI) / 180.0f);
  brush.color(1.0f, 1.0f, 1.0f, 0.5f);
  m_painter->fill_rect(PainterData(&brush),
                       Rect()
                       .min_point(wh * (r * 0.25f))
                       .size(wh * (r * 0.25f)));
}

void
painter_clip_test::
draw_frame(void)
{
  m_painter->begin(m_surface, Painter::y_increases_downwards);
  m_zoomer.transformation().concat_to_painter(m_painter);

  m_painter->save();
  draw_scene(true);
  m_painter->restore();
  if (m_draw_overlay)
    {
      draw_scene(false);
    }

  m_painter->end();
  fastuidraw_glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
  fastuidraw_glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
  m_surface->blit_surface(GL_NEAREST);
}

void
painter_clip_test::
handle_event(const SDL_Event &ev)
{
  m_zoomer.handle_event(ev);
  switch(ev.type)
    {
      case SDL_QUIT:
        end_demo(0);
        break;

    case SDL_KEYUP:
      switch(ev.key.keysym.sym)
        {
        case SDLK_ESCAPE:
          end_demo(0);
          break;
        case SDLK_o:
          m_draw_overlay = !m_draw_overlay;
          break;
        case SDLK_m:
          m_use_matrices = !m_use_matrices;
          std::cout << "Use matrics: " << m_use_matrices << "\n";
          break;
        }
      break;
    };
}

int
main(int argc, char **argv)
{
  painter_clip_test P;
  return P.main(argc, argv);
}
