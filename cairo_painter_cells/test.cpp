#include "sdl_cairo_demo.hpp"
#include "random.hpp"
#include "simple_time.hpp"

class test:public sdl_cairo_demo
{
public:
  test(void):
    m_x(0.0),
    m_y(0.0),
    m_dx(100.0),
    m_dy(100.0)
  {}

  ~test()
  {}

protected:

  virtual
  void
  handle_event(const SDL_Event &ev)
  {
    switch(ev.type)
    {
    case SDL_QUIT:
      end_demo(0);
      break;

    case SDL_WINDOWEVENT:
      if(ev.window.event == SDL_WINDOWEVENT_RESIZED)
        {
          on_resize(ev.window.data1, ev.window.data2);
        }
      break;

    case SDL_KEYUP:
      switch(ev.key.keysym.sym)
        {
        case SDLK_ESCAPE:
          end_demo(0);
          break;
        }
    }
  }

  virtual
  void
  draw_frame(void)
  {
    std::pair<int, int> wh;
    double r, g, b;
    int64_t delta_time_us;
    double delta_time_s, dx, dy;

    delta_time_us = m_timer.restart_us();
    delta_time_s = static_cast<double>(delta_time_us) / 1e6;
    r = 0.0;
    g = 1.0;
    b = 1.0;
    wh = dimensions();

    cairo_set_operator(m_cairo, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_rgb(m_cairo, 0.5, 0.5, 0.5);
    cairo_paint(m_cairo);

    cairo_set_operator(m_cairo, CAIRO_OPERATOR_OVER);
    cairo_new_path(m_cairo);
    cairo_rectangle(m_cairo, m_x - 50.0, m_y - 50.0, 100.0, 100.0);
    cairo_set_source_rgb(m_cairo, r, g, b);
    cairo_fill(m_cairo);

    dx = delta_time_s * m_dx;
    dy = delta_time_s * m_dy;

    if(m_x + dx > wh.first || m_x + m_dx < 0.0)
      {
        m_dx = -m_dx;
        dx = -dx;
      }

    if(m_y + dy > wh.second || m_y + m_dy < 0.0)
      {
        m_dy = -m_dy;
        dy = -dy;
      }

    m_x += dx;
    m_y += dy;
  }

private:
  double m_x, m_y, m_dx, m_dy;
  simple_time m_timer;
};

int
main(int argc, char **argv)
{
  test P;
  return P.main(argc, argv);
}
