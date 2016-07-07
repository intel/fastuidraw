#include "sdl_cairo_demo.hpp"
#include "random.hpp"
#include "simple_time.hpp"
#include "ScaleTranslate.hpp"
#include "ImageLoader.hpp"

class test:public sdl_cairo_demo
{
public:
  test(void):
    m_demo_options("Demo Options", *this),
    m_image_file("", "image", "Image to draw to moving rectangle", *this),
    m_x(0.0),
    m_y(0.0),
    m_dx(100.0),
    m_dy(100.0),
    m_image(NULL),
    m_pattern(NULL),
    m_pattern_dims(0.0, 0.0)
  {}

  ~test()
  {
    if(m_pattern)
      {
        cairo_pattern_destroy(m_pattern);
      }

    if(m_image)
      {
        cairo_surface_destroy(m_image);
      }
  }

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
  derived_init(int, int)
  {
    m_image = create_image_from_file(m_image_file.m_value);
    if(m_image)
      {
        m_pattern = cairo_pattern_create_for_surface(m_image);
        m_pattern_dims.x() = cairo_image_surface_get_width(m_image);
        m_pattern_dims.y() = cairo_image_surface_get_height(m_image);
      }
    else
      {
        m_pattern_dims = vec2(100.0, 100.0);
      }
  }

  virtual
  void
  draw_frame(void)
  {
    cairo_save(m_cairo);

    std::pair<int, int> wh;
    int64_t delta_time_us;
    double delta_time_s, dx, dy;

    delta_time_us = m_timer.restart_us();
    delta_time_s = static_cast<double>(delta_time_us) / 1e6;
    wh = dimensions();

    cairo_set_operator(m_cairo, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_rgb(m_cairo, 0.5, 0.5, 0.5);
    cairo_paint(m_cairo);

    cairo_set_operator(m_cairo, CAIRO_OPERATOR_OVER);
    cairo_translate(m_cairo, m_x - m_pattern_dims.x() * 0.5, m_y - m_pattern_dims.y() * 0.5);
    cairo_save(m_cairo);
    if(m_pattern)
      {
        cairo_set_source(m_cairo, m_pattern);
        cairo_rectangle(m_cairo, 0.0, 0.0, m_pattern_dims.x(), m_pattern_dims.y());
        cairo_fill(m_cairo);
      }
    else
      {
        double r, g, b;
        r = 0.0;
        g = 1.0;
        b = 1.0;
        cairo_new_path(m_cairo);
        cairo_rectangle(m_cairo, 0.0, 0.0, m_pattern_dims.x(), m_pattern_dims.y());
        cairo_set_source_rgb(m_cairo, r, g, b);
        cairo_fill(m_cairo);
      }
    cairo_restore(m_cairo);

    cairo_save(m_cairo);
    cairo_set_source_rgb(m_cairo, 1.0, 1.0, 0.0);
    cairo_set_font_size (m_cairo, 240.0);
    cairo_rotate(m_cairo, 45.0 * M_PI / 180.0);
    cairo_move_to (m_cairo, 0, 0);
    cairo_show_text (m_cairo, "Hello World");
    cairo_restore(m_cairo);

    dx = delta_time_s * m_dx;
    dy = delta_time_s * m_dy;

    if(m_x + dx > wh.first || m_x + dx < 0.0)
      {
        m_dx = -m_dx;
        dx = -dx;
      }

    if(m_y + dy > wh.second || m_y + dy < 0.0)
      {
        m_dy = -m_dy;
        dy = -dy;
      }

    m_x += dx;
    m_y += dy;

    cairo_restore(m_cairo);
  }

private:
  command_separator m_demo_options;
  command_line_argument_value<std::string> m_image_file;

  double m_x, m_y, m_dx, m_dy;
  simple_time m_timer;
  cairo_surface_t *m_image;
  cairo_pattern_t *m_pattern;
  vec2 m_pattern_dims;
};

int
main(int argc, char **argv)
{
  test P;
  return P.main(argc, argv);
}
