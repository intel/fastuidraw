#include <iostream>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <bitset>
#include <math.h>

#include "sdl_cairo_demo.hpp"
#include "simple_time.hpp"
#include "PanZoomTracker.hpp"
#include "read_path.hpp"
#include "cycle_value.hpp"

const char*
on_off(bool v)
{
  return v ? "ON" : "OFF";
}

template<typename T>
class label_values:public std::vector<std::pair<T, std::string> >
{
public:
  typedef std::pair<T, std::string> entry;

  label_values&
  add_value(T v, const std::string &p)
  {
    this->push_back(entry(v, p));
    return *this;
  }
};

class DashPattern:public std::vector<double>
{
public:
  DashPattern&
  add_draw_skip(double d, double s)
  {
    this->push_back(d);
    this->push_back(s);
    return *this;
  }
};

class painter_stroke_test:public sdl_cairo_demo
{
public:
  painter_stroke_test(void);


protected:

  void
  derived_init(int w, int h);

  void
  draw_frame(void);

  void
  handle_event(const SDL_Event &ev);

private:

  void
  construct_path(void);

  void
  construct_dash_patterns(void);

  void
  update_cts_params(void);

  command_line_argument_value<float> m_change_stroke_width_rate;
  command_line_argument_value<std::string> m_path_file;

  cairo_path_t *m_path;
  std::vector<DashPattern> m_dash_patterns;

  PanZoomTrackerSDLEvent m_zoomer;
  label_values<cairo_line_join_t> m_join_labels;
  label_values<cairo_line_cap_t> m_cap_labels;

  unsigned int m_join_style;
  unsigned int m_cap_style;
  unsigned int m_dash;

  float m_stroke_width;
  bool m_stroke_aa;

  vec2 m_shear, m_shear2;
  float m_angle;

  simple_time m_draw_timer;
};

//////////////////////////////////////
// painter_stroke_test methods
painter_stroke_test::
painter_stroke_test(void):
  sdl_cairo_demo("painter-stroke-test"),
  m_change_stroke_width_rate(10.0f, "change_stroke_width_rate",
                             "rate of change in pixels/sec for changing stroke width "
                             "when changing stroke when key is down",
                             *this),
  m_path_file("", "path_file",
              "if non-empty read the geometry of the path from the specified file, "
              "otherwise use a default path",
              *this),
  m_path(NULL),
  m_join_style(0),
  m_cap_style(0),
  m_dash(0),
  m_stroke_width(1.0f),
  m_stroke_aa(true),
  m_shear(1.0f, 1.0f),
  m_shear2(1.0f, 1.0f),
  m_angle(0.0f)
{
  std::cout << "Controls:\n"
            << "\ta: toggle anti-aliased stroking\n"
            << "\tj: cycle through join styles for stroking\n"
            << "\tc: cycle through cap style for stroking\n"
            << "\td: cycle through dash patterns\n"
            << "\t[: decrease stroke width(hold left-shift for slower rate and right shift for faster)\n"
            << "\t]: increase stroke width(hold left-shift for slower rate and right shift for faster)\n"
            << "\tq: reset shear to 1.0\n"
            << "\t6: x-shear (hold ctrl to decrease, hold enter for shear2)\n"
            << "\t7: y-shear (hold ctrl to decrease, hold enter for shear2)\n"
            << "\t0: Rotate left\n"
            << "\t9: Rotate right\n"
            << "\tMiddle Mouse Draw: set p0(starting position top left) {drawn black with white inside} of gradient\n"
            << "\tLeft Mouse Drag: pan\n"
            << "\tHold Left Mouse, then drag up/down: zoom out/in\n";


  m_join_labels
    .add_value(CAIRO_LINE_JOIN_ROUND, "rounded_joins")
    .add_value(CAIRO_LINE_JOIN_BEVEL, "bevel_joins")
    .add_value(CAIRO_LINE_JOIN_MITER, "miter_joins");

  m_cap_labels
    .add_value(CAIRO_LINE_CAP_BUTT, "no_caps")
    .add_value(CAIRO_LINE_CAP_ROUND, "rounded_caps")
    .add_value(CAIRO_LINE_CAP_SQUARE, "square_caps");
}


void
painter_stroke_test::
update_cts_params(void)
{
  const Uint8 *keyboard_state = SDL_GetKeyboardState(NULL);
  assert(keyboard_state != NULL);

  float speed = static_cast<float>(m_draw_timer.restart()), speed_stroke, speed_shear;

  if(keyboard_state[SDL_SCANCODE_LSHIFT])
    {
      speed *= 0.1f;
    }
  if(keyboard_state[SDL_SCANCODE_RSHIFT])
    {
      speed *= 10.0f;
    }

  speed_shear = 0.01f * speed;
  if(keyboard_state[SDL_SCANCODE_LCTRL] || keyboard_state[SDL_SCANCODE_RCTRL])
    {
      speed_shear = -speed_shear;
    }

  vec2 *pshear(&m_shear);
  const char *shear_txt = "";
  if(keyboard_state[SDL_SCANCODE_RETURN])
    {
      pshear = &m_shear2;
      shear_txt = "2";
    }

  if(keyboard_state[SDL_SCANCODE_6])
    {
      pshear->x() += speed_shear;
      std::cout << "Shear" << shear_txt << " set to: " << *pshear << "\n";
    }
  if(keyboard_state[SDL_SCANCODE_7])
    {
      pshear->y() += speed_shear;
      std::cout << "Shear " << shear_txt << " set to: " << *pshear << "\n";
    }

  if(keyboard_state[SDL_SCANCODE_9])
    {
      m_angle += speed * 0.1f;
      std::cout << "Angle set to: " << m_angle << "\n";
    }
  if(keyboard_state[SDL_SCANCODE_0])
    {
      m_angle -= speed * 0.1f;
      std::cout << "Angle set to: " << m_angle << "\n";
    }

  speed_stroke = speed * m_change_stroke_width_rate.m_value;
  speed_stroke /= m_zoomer.transformation().scale();

  if(keyboard_state[SDL_SCANCODE_RIGHTBRACKET])
    {
      m_stroke_width += speed_stroke;
    }

  if(keyboard_state[SDL_SCANCODE_LEFTBRACKET] && m_stroke_width > 0.0f)
    {
      m_stroke_width -= speed_stroke;
      m_stroke_width = std::max(m_stroke_width, 0.0f);
    }

  if(keyboard_state[SDL_SCANCODE_RIGHTBRACKET] || keyboard_state[SDL_SCANCODE_LEFTBRACKET])
    {
      std::cout << "Stroke width set to: " << m_stroke_width << "\n";
    }
}

void
painter_stroke_test::
handle_event(const SDL_Event &ev)
{
  m_zoomer.handle_event(ev);
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

        case SDLK_a:
          m_stroke_aa = !m_stroke_aa;
          std::cout << "Anti-aliasing stroking = " << m_stroke_aa << "\n";
          break;

        case SDLK_q:
          m_shear = m_shear2 = vec2(1.0f, 1.0f);
          break;

        case SDLK_j:
          cycle_value(m_join_style, ev.key.keysym.mod & (KMOD_SHIFT|KMOD_CTRL|KMOD_ALT), m_join_labels.size());
          std::cout << "Join drawing mode set to: " << m_join_labels[m_join_style].second << "\n";
          break;

        case SDLK_c:
          cycle_value(m_cap_style, ev.key.keysym.mod & (KMOD_SHIFT|KMOD_CTRL|KMOD_ALT), m_cap_labels.size());
          std::cout << "Cap drawing mode set to: " << m_cap_labels[m_cap_style].second << "\n";
          break;

        case SDLK_d:
          cycle_value(m_dash, ev.key.keysym.mod & (KMOD_SHIFT|KMOD_CTRL|KMOD_ALT), m_dash_patterns.size());
          std::cout << "Set to stroke dashed with pattern: {";
          for(unsigned int i = 0; i < m_dash_patterns[m_dash].size(); i += 2)
            {
              if(i != 0)
                {
                  std::cout << ", ";
                }
              std::cout << "Draw(" << m_dash_patterns[m_dash][i] << "), Space(";
              if(i + 1 < m_dash_patterns[m_dash].size())
                {
                  std::cout << m_dash_patterns[m_dash][i];
                }
              else
                {
                  std::cout << float(0);
                }
              std::cout << ")";
            }
          std::cout << "}\n";
          break;
        }
      break;
    };
}

void
painter_stroke_test::
construct_path(void)
{
  if(!m_path_file.m_value.empty())
    {
      std::ifstream path_file(m_path_file.m_value.c_str());
      if(path_file)
        {
          std::stringstream buffer;
          buffer << path_file.rdbuf();
          m_path = read_path(m_cairo, buffer.str());
          return;
        }
    }
  else
    {
      cairo_new_path(m_cairo);

      cairo_move_to(m_cairo, 300.0, 300.0);
      cairo_close_path(m_cairo);

      cairo_move_to(m_cairo, 50.0, 35.0);
      cairo_bezier_to(m_cairo, 60.0, 50.0, 70.0, 35.0);
      cairo_arc_degrees_to(m_cairo, 180.0, 70.0, -100.0);
      cairo_curve_to(m_cairo, 60.0, -150.0, 30.0, -50.0, 0.0, -100.0);
      cairo_arc_degrees_to(m_cairo, 90.0, 50.0, 35.0);
      cairo_close_path(m_cairo);

      cairo_move_to(m_cairo, 200.0, 200.0);
      cairo_line_to(m_cairo, 400.0, 200.0);
      cairo_line_to(m_cairo, 400.0, 400.0);
      cairo_line_to(m_cairo, 200.0, 400.0);
      cairo_line_to(m_cairo, 200.0, 200.0);
      cairo_close_path(m_cairo);

      cairo_move_to(m_cairo, -50.0, 100.0);
      cairo_line_to(m_cairo, 0.0, 200.0);
      cairo_line_to(m_cairo, 100.0, 300.0);
      cairo_line_to(m_cairo, 150.0, 325.0);
      cairo_line_to(m_cairo, 150.0, 100.0);
      cairo_line_to(m_cairo, -50.0, 100.0);
      cairo_close_path(m_cairo);

      m_path = cairo_copy_path(m_cairo);
      cairo_new_path(m_cairo);
    }
}

void
painter_stroke_test::
construct_dash_patterns(void)
{
  m_dash_patterns.push_back(DashPattern());
  m_dash_patterns.push_back(DashPattern());
  m_dash_patterns.back()
    .add_draw_skip(20.0, 10.0)
    .add_draw_skip(10.0, 15.0)
    .add_draw_skip(10.0, 10.0)
    .add_draw_skip(5.0, 10.0);
}

void
painter_stroke_test::
draw_frame(void)
{
  ivec2 wh(dimensions());

  update_cts_params();

  cairo_save(m_cairo);

  //clear
  cairo_set_operator(m_cairo, CAIRO_OPERATOR_SOURCE);
  cairo_set_source_rgba(m_cairo, 1.0, 0.0, 0.0, 1.0);
  cairo_paint(m_cairo);

  /* apply m_zoomer
   */
  cairo_identity_matrix(m_cairo);
  cairo_translate(m_cairo, m_zoomer.transformation().translation());
  cairo_scale(m_cairo, m_zoomer.transformation().scale(), m_zoomer.transformation().scale());

  /* apply shear
   */
  cairo_scale(m_cairo, m_shear.x(), m_shear.y());

  /* apply rotation
   */
  cairo_rotate(m_cairo, m_angle * M_PI / 180.0f);

  /* apply shear2
   */
  cairo_scale(m_cairo, m_shear2.x(), m_shear2.y());

  if(!m_dash_patterns[m_dash].empty())
    {
      cairo_set_dash(m_cairo, &m_dash_patterns[m_dash][0], m_dash_patterns[m_dash].size(), 0.0);
    }
  else
    {
      cairo_set_dash(m_cairo, NULL, 0, 0.0);
    }

  if(m_path == NULL)
    {
      construct_path();
    }

  assert(m_path != NULL);
  cairo_new_path(m_cairo);
  cairo_append_path(m_cairo, m_path);
  cairo_set_line_join(m_cairo, m_join_labels[m_join_style].first);
  cairo_set_line_cap(m_cairo, m_cap_labels[m_cap_style].first);
  cairo_set_operator(m_cairo, CAIRO_OPERATOR_OVER);
  cairo_set_source_rgba(m_cairo, 0.0, 0.0, 1.0, 0.5);
  cairo_set_line_width(m_cairo, m_stroke_width);
  cairo_stroke(m_cairo);

  cairo_restore(m_cairo);
}

void
painter_stroke_test::
derived_init(int w, int h)
{
  //put into unit of per ms.
  m_change_stroke_width_rate.m_value /= 1000.0f;

  construct_dash_patterns();

  m_draw_timer.restart();
}

int
main(int argc, char **argv)
{
  painter_stroke_test P;
  return P.main(argc, argv);
}
