#include "sdl_cairo_demo.hpp"
#include "random.hpp"

class test:public sdl_cairo_demo
{
public:
  test(void)
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

    r = random_value(0.0, 1.0);
    g = random_value(0.0, 1.0);
    b = random_value(0.0, 1.0);
    wh = dimensions();
    cairo_new_path(m_cairo);
    cairo_rectangle(m_cairo, 0.0, 0.0, wh.first, wh.second);
    cairo_set_source_rgb(m_cairo, r, g, b);
    cairo_fill(m_cairo);
  }
};

int
main(int argc, char **argv)
{
  test P;
  return P.main(argc, argv);
}
