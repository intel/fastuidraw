#include <iostream>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <bitset>
#include <math.h>

#include <fastuidraw/util/util.hpp>

#include "sdl_painter_demo.hpp"
#include "simple_time.hpp"
#include "PanZoomTracker.hpp"
#include "read_path.hpp"
#include "read_dash_pattern.hpp"
#include "ImageLoader.hpp"
#include "colorstop_command_line.hpp"
#include "cycle_value.hpp"
#include "ostream_utility.hpp"

using namespace fastuidraw;

const char*
on_off(bool v)
{
  return v ? "ON" : "OFF";
}

class DashPatternList:public command_line_argument
{
public:
  DashPatternList(command_line_register &parent):
    command_line_argument(parent)
  {
    m_desc = produce_formatted_detailed_description("add_dash_pattern filename",
                                                    "Adds a dash pattern to use source from a file");
  }

  virtual
  int
  check_arg(const std::vector<std::string> &args, int location)
  {
    if(static_cast<unsigned int>(location + 1) < args.size() && args[location] == "add_dash_pattern")
      {
        m_files.push_back(args[location + 1]);
        std::cout << "\nAdded dash pattern from file " << args[location + 1];
        return 2;
      }
    return 0;
  }

  virtual
  void
  print_command_line_description(std::ostream &ostr) const
  {
    ostr << "[add_dash_pattern file]";
  }

  virtual
  void
  print_detailed_description(std::ostream &ostr) const
  {
    ostr << m_desc;
  }

  std::vector<std::string> m_files;

private:
  std::string m_desc;
};

class WindingValueFillRule:public Painter::CustomFillRuleBase
{
public:
  WindingValueFillRule(int v):
    m_winding_number(v)
  {}

  bool
  operator()(int w) const
  {
    return w == m_winding_number;
  }

private:
  int m_winding_number;
};

void
enable_wire_frame(bool b)
{
  #ifndef FASTUIDRAW_GL_USE_GLES
    {
      if(b)
        {
          glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
          glLineWidth(4.0);
        }
      else
        {
          glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        }
    }
  #else
    {
      FASTUIDRAWunused(b);
    }
  #endif
}

class painter_stroke_test:public sdl_painter_demo
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

  enum gradient_draw_mode
    {
      draw_no_gradient,
      draw_linear_gradient,
      draw_radial_gradient,

      number_gradient_draw_modes
    };

  enum image_filter
    {
      no_image,
      image_nearest_filter,
      image_linear_filter,
      image_cubic_filter,

      number_image_filter_modes
    };

  typedef std::pair<std::string,
                    reference_counted_ptr<ColorStopSequenceOnAtlas> > named_color_stop;

  void
  construct_path(void);

  void
  create_stroked_path_attributes(void);

  void
  construct_color_stops(void);

  void
  construct_dash_patterns(void);

  vec2
  item_coordinates(ivec2 c);

  vec2
  brush_item_coordinate(ivec2 c);

  void
  update_cts_params(void);

  command_line_argument_value<float> m_change_miter_limit_rate;
  command_line_argument_value<float> m_change_stroke_width_rate;
  command_line_argument_value<float> m_window_change_rate;
  command_line_argument_value<float> m_radial_gradient_change_rate;
  command_line_argument_value<std::string> m_path_file;
  DashPatternList m_dash_pattern_files;
  command_line_argument_value<bool> m_print_path;
  color_stop_arguments m_color_stop_args;
  command_line_argument_value<std::string> m_image_file;
  command_line_argument_value<unsigned int> m_image_slack;
  command_line_argument_value<int> m_sub_image_x, m_sub_image_y;
  command_line_argument_value<int> m_sub_image_w, m_sub_image_h;
  command_line_argument_value<std::string> m_font_file;

  Path m_path;
  float m_max_miter;
  reference_counted_ptr<Image> m_image;
  uvec2 m_image_offset, m_image_size;
  std::vector<named_color_stop> m_color_stops;
  std::vector<std::vector<PainterDashedStrokeParams::DashPatternElement> > m_dash_patterns;
  reference_counted_ptr<const FontBase> m_font;

  PanZoomTrackerSDLEvent m_zoomer;
  vecN<std::string, number_gradient_draw_modes> m_gradient_mode_labels;
  vecN<std::string, PainterEnums::number_cap_styles> m_cap_labels;
  vecN<std::string, PainterEnums::number_join_styles> m_join_labels;
  vecN<std::string, PainterEnums::fill_rule_data_count> m_fill_labels;
  vecN<std::string, number_image_filter_modes> m_image_filter_mode_labels;

  PainterPackedValue<PainterBrush> m_black_pen;
  PainterPackedValue<PainterBrush> m_white_pen;
  PainterPackedValue<PainterBrush> m_transparent_blue_pen;

  unsigned int m_join_style;
  unsigned int m_cap_style;
  bool m_close_contour;
  unsigned int m_fill_rule;
  /* m_dash pattern:
      0 -> undashed stroking
      [1, m_dash_patterns.size()] -> dashed stroking
   */
  unsigned int m_dash;

  bool
  is_dashed_stroking(void)
  {
    return m_dash != 0;
  }

  unsigned int
  dash_pattern(void)
  {
    return m_dash - 1;
  }

  unsigned int m_end_fill_rule;
  bool m_have_miter_limit;
  float m_miter_limit, m_stroke_width;
  bool m_draw_fill;
  unsigned int m_active_color_stop;
  unsigned int m_gradient_draw_mode;
  bool m_repeat_gradient;
  unsigned int m_image_filter;
  bool m_draw_stats;

  vec2 m_gradient_p0, m_gradient_p1;
  float m_gradient_r0, m_gradient_r1;
  bool m_translate_brush, m_matrix_brush;

  bool m_repeat_window;
  vec2 m_repeat_xy, m_repeat_wh;

  bool m_clipping_window;
  vec2 m_clipping_xy, m_clipping_wh;

  bool m_stroke_aa;
  bool m_wire_frame;
  bool m_stroke_width_in_pixels;
  bool m_force_square_viewport;

  bool m_fill_by_clipping;
  vec2 m_shear, m_shear2;
  bool m_draw_grid;

  float m_angle;

  simple_time m_draw_timer;
  Path m_grid_path;
  bool m_grid_path_dirty;

  Path m_clip_window_path;
  bool m_clip_window_path_dirty;
};

//////////////////////////////////////
// painter_stroke_test methods
painter_stroke_test::
painter_stroke_test(void):
  sdl_painter_demo("painter-stroke-test"),
  m_change_miter_limit_rate(1.0f, "miter_limit_rate",
                            "rate of change in in stroke widths per second for "
                            "changing the miter limit when the when key is down",
                            *this),
  m_change_stroke_width_rate(10.0f, "change_stroke_width_rate",
                             "rate of change in pixels/sec for changing stroke width "
                             "when changing stroke when key is down",
                             *this),
  m_window_change_rate(10.0f, "change_rate_brush_repeat_window",
                       "rate of change in pixels/sec when changing the repeat window",
                       *this),
  m_radial_gradient_change_rate(0.1f, "change_rate_brush_radial_gradient",
                                "rate of change in pixels/sec when changing the radial gradient radius",
                                *this),
  m_path_file("", "path_file",
              "if non-empty read the geometry of the path from the specified file, "
              "otherwise use a default path",
              *this),
  m_dash_pattern_files(*this),
  m_print_path(false, "print_path",
               "If true, print the geometry data of the path drawn to stdout",
               *this),
  m_color_stop_args(*this),
  m_image_file("", "image", "if a valid file name, apply an image to drawing the fill", *this),
  m_image_slack(0, "image_slack", "amount of slack on tiles when loading image", *this),
  m_sub_image_x(0, "sub_image_x",
                "x-coordinate of top left corner of sub-image rectange (negative value means no-subimage)",
                *this),
  m_sub_image_y(0, "sub_image_y",
                "y-coordinate of top left corner of sub-image rectange (negative value means no-subimage)",
                *this),
  m_sub_image_w(-1, "sub_image_w",
                "sub-image width of sub-image rectange (negative value means no-subimage)",
                *this),
  m_sub_image_h(-1, "sub_image_h",
                "sub-image height of sub-image rectange (negative value means no-subimage)",
                *this),
  m_font_file("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", "font",
              "File from which to take font", *this),
  m_join_style(PainterEnums::rounded_joins),
  m_cap_style(PainterEnums::flat_caps),
  m_close_contour(true),
  m_fill_rule(PainterEnums::odd_even_fill_rule),
  m_dash(0),
  m_end_fill_rule(PainterEnums::fill_rule_data_count),
  m_have_miter_limit(true),
  m_miter_limit(5.0f),
  m_stroke_width(1.0f),
  m_draw_fill(false),
  m_active_color_stop(0),
  m_gradient_draw_mode(draw_no_gradient),
  m_repeat_gradient(true),
  m_image_filter(image_nearest_filter),
  m_draw_stats(false),
  m_translate_brush(false),
  m_matrix_brush(false),
  m_repeat_window(false),
  m_clipping_window(false),
  m_stroke_aa(true),
  m_wire_frame(false),
  m_stroke_width_in_pixels(false),
  m_force_square_viewport(false),
  m_fill_by_clipping(false),
  m_shear(1.0f, 1.0f),
  m_shear2(1.0f, 1.0f),
  m_draw_grid(false),
  m_angle(0.0f),
  m_grid_path_dirty(true),
  m_clip_window_path_dirty(true)
{
  std::cout << "Controls:\n"
            << "\ta: toggle anti-aliased stroking\n"
            << "\tj: cycle through join styles for stroking\n"
            << "\tc: cycle through cap style for stroking\n"
            << "\tx: toggle closing contour on stroking\n"
            << "\td: cycle through dash patterns\n"
            << "\t[: decrease stroke width(hold left-shift for slower rate and right shift for faster)\n"
            << "\t]: increase stroke width(hold left-shift for slower rate and right shift for faster)\n"
            << "\tp: toggle stroke width in pixels or local coordinates\n"
            << "\t5: toggle drawing grid\n"
            << "\tq: reset shear to 1.0\n"
            << "\t6: x-shear (hold ctrl to decrease, hold enter for shear2)\n"
            << "\t7: y-shear (hold ctrl to decrease, hold enter for shear2)\n"
            << "\t0: Rotate left\n"
            << "\t9: Rotate right\n"
            << "\tv: toggle force square viewport\n"
            << "\tb: decrease miter limit(hold left-shift for slower rate and right shift for faster)\n"
            << "\tn: increase miter limit(hold left-shift for slower rate and right shift for faster)\n"
            << "\tm: toggle miter limit enforced\n"
            << "\tf: toggle drawing path fill\n"
            << "\tr: cycle through fill rules\n"
            << "\te: toggle fill by drawing clip rect\n"
            << "\ti: cycle through image filter to apply to fill (no image, nearest, linear, cubic)\n"
            << "\ts: cycle through defined color stops for gradient\n"
            << "\tg: cycle through gradient types (linear or radial)\n"
            << "\th: toggle repeat gradient\n"
            << "\tt: toggle translate brush\n"
            << "\ty: toggle matrix brush\n"
            << "\to: toggle clipping window\n"
            << "\t4,6,2,8 (number pad): change location of clipping window\n"
            << "\tctrl-4,6,2,8 (number pad): change size of clipping window\n"
            << "\tw: toggle brush repeat window active\n"
            << "\tarrow keys: change location of brush repeat window\n"
            << "\tctrl-arrow keys: change size of brush repeat window\n"
            << "\tMiddle Mouse Draw: set p0(starting position top left) {drawn black with white inside} of gradient\n"
            << "\t1/2 : decrease/increase r0 of gradient(hold left-shift for slower rate and right shift for faster)\n"
            << "\t3/4 : decrease/increase r1 of gradient(hold left-shift for slower rate and right shift for faster)\n"
            << "\tl: draw Painter stats\n"
            << "\tRight Mouse Draw: set p1(starting position bottom right) {drawn white with black inside} of gradient\n"
            << "\tLeft Mouse Drag: pan\n"
            << "\tHold Left Mouse, then drag up/down: zoom out/in\n";

  m_gradient_mode_labels[draw_no_gradient] = "draw_no_gradient";
  m_gradient_mode_labels[draw_linear_gradient] = "draw_linear_gradient";
  m_gradient_mode_labels[draw_radial_gradient] = "draw_radial_gradient";

  m_join_labels[PainterEnums::no_joins] = "no_joins";
  m_join_labels[PainterEnums::rounded_joins] = "rounded_joins";
  m_join_labels[PainterEnums::bevel_joins] = "bevel_joins";
  m_join_labels[PainterEnums::miter_joins] = "miter_joins";

  m_cap_labels[PainterEnums::flat_caps] = "flat_caps";
  m_cap_labels[PainterEnums::rounded_caps] = "rounded_caps";
  m_cap_labels[PainterEnums::square_caps] = "square_caps";

  m_fill_labels[PainterEnums::odd_even_fill_rule] = "odd_even_fill_rule";
  m_fill_labels[PainterEnums::nonzero_fill_rule] = "nonzero_fill_rule";
  m_fill_labels[PainterEnums::complement_odd_even_fill_rule] = "complement_odd_even_fill_rule";
  m_fill_labels[PainterEnums::complement_nonzero_fill_rule] = "complement_nonzero_fill_rule";

  m_image_filter_mode_labels[no_image] = "no_image";
  m_image_filter_mode_labels[image_nearest_filter] = "image_nearest_filter";
  m_image_filter_mode_labels[image_linear_filter] = "image_linear_filter";
  m_image_filter_mode_labels[image_cubic_filter] = "image_cubic_filter";
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
  if(!m_stroke_width_in_pixels)
    {
      speed_stroke /= m_zoomer.transformation().scale();
    }

  if(keyboard_state[SDL_SCANCODE_RIGHTBRACKET])
    {
      m_grid_path_dirty = true;
      m_stroke_width += speed_stroke;
    }

  if(keyboard_state[SDL_SCANCODE_LEFTBRACKET] && m_stroke_width > 0.0f)
    {
      m_grid_path_dirty = true;
      m_stroke_width -= speed_stroke;
      m_stroke_width = fastuidraw::t_max(m_stroke_width, 0.0f);
    }

  if(keyboard_state[SDL_SCANCODE_RIGHTBRACKET] || keyboard_state[SDL_SCANCODE_LEFTBRACKET])
    {
      std::cout << "Stroke width set to: " << m_stroke_width << "\n";
    }

  if(m_repeat_window)
    {
      vec2 *changer;
      float delta, delta_y;

      delta = m_window_change_rate.m_value * speed / m_zoomer.transformation().scale();
      if(keyboard_state[SDL_SCANCODE_LCTRL] || keyboard_state[SDL_SCANCODE_RCTRL])
        {
          changer = &m_repeat_wh;
          delta_y = delta;
        }
      else
        {
          changer = &m_repeat_xy;
          delta_y = -delta;
        }

      if(keyboard_state[SDL_SCANCODE_UP])
        {
          changer->y() += delta_y;
          changer->y() = fastuidraw::t_max(0.0f, changer->y());
        }

      if(keyboard_state[SDL_SCANCODE_DOWN])
        {
          changer->y() -= delta_y;
          changer->y() = fastuidraw::t_max(0.0f, changer->y());
        }

      if(keyboard_state[SDL_SCANCODE_RIGHT])
        {
          changer->x() += delta;
        }

      if(keyboard_state[SDL_SCANCODE_LEFT])
        {
          changer->x() -= delta;
          changer->x() = fastuidraw::t_max(0.0f, changer->x());
        }

      if(keyboard_state[SDL_SCANCODE_UP] || keyboard_state[SDL_SCANCODE_DOWN]
         || keyboard_state[SDL_SCANCODE_RIGHT] || keyboard_state[SDL_SCANCODE_LEFT])
        {
          std::cout << "Brush repeat window set to: xy = " << m_repeat_xy << " wh = " << m_repeat_wh << "\n";
        }
    }


  if(m_gradient_draw_mode == draw_radial_gradient)
    {
      float delta;

      delta = m_radial_gradient_change_rate.m_value * speed / m_zoomer.transformation().scale();
      if(keyboard_state[SDL_SCANCODE_1])
        {
          m_gradient_r0 -= delta;
          m_gradient_r0 = fastuidraw::t_max(0.0f, m_gradient_r0);
        }

      if(keyboard_state[SDL_SCANCODE_2])
        {
          m_gradient_r0 += delta;
        }
      if(keyboard_state[SDL_SCANCODE_3])
        {
          m_gradient_r1 -= delta;
          m_gradient_r1 = fastuidraw::t_max(0.0f, m_gradient_r1);
        }

      if(keyboard_state[SDL_SCANCODE_4])
        {
          m_gradient_r1 += delta;
        }

      if(keyboard_state[SDL_SCANCODE_1] || keyboard_state[SDL_SCANCODE_2]
         || keyboard_state[SDL_SCANCODE_3] || keyboard_state[SDL_SCANCODE_4])
        {
          std::cout << "Radial gradient values set to: r0 = "
                    << m_gradient_r0 << " r1 = " << m_gradient_r1 << "\n";
        }
    }


  if(m_join_style == PainterEnums::miter_joins && m_have_miter_limit)
    {
      if(keyboard_state[SDL_SCANCODE_N])
        {
          m_miter_limit += m_change_miter_limit_rate.m_value * speed;
          m_miter_limit = fastuidraw::t_min(m_max_miter, m_miter_limit);
        }

      if(keyboard_state[SDL_SCANCODE_B])
        {
          m_miter_limit -= m_change_miter_limit_rate.m_value * speed;
          m_miter_limit = fastuidraw::t_max(0.0f, m_miter_limit);
        }

      if(keyboard_state[SDL_SCANCODE_N] || keyboard_state[SDL_SCANCODE_B])
        {
          std::cout << "Miter-limit set to: " << m_miter_limit << "\n";
        }
    }

  if(m_clipping_window)
    {
      vec2 *changer;
      float delta, delta_y;

      delta = m_window_change_rate.m_value * speed / m_zoomer.transformation().scale();
      if(keyboard_state[SDL_SCANCODE_LCTRL] || keyboard_state[SDL_SCANCODE_RCTRL])
        {
          changer = &m_clipping_wh;
          delta_y = delta;
        }
      else
        {
          changer = &m_clipping_xy;
          delta_y = -delta;
        }

      if(keyboard_state[SDL_SCANCODE_KP_8])
        {
          changer->y() += delta_y;
        }

      if(keyboard_state[SDL_SCANCODE_KP_2])
        {
          changer->y() -= delta_y;
        }

      if(keyboard_state[SDL_SCANCODE_KP_6])
        {
          changer->x() += delta;
        }

      if(keyboard_state[SDL_SCANCODE_KP_4])
        {
          changer->x() -= delta;
        }

      if(keyboard_state[SDL_SCANCODE_KP_2] || keyboard_state[SDL_SCANCODE_KP_4]
         || keyboard_state[SDL_SCANCODE_KP_6] || keyboard_state[SDL_SCANCODE_KP_8])
        {
          m_clip_window_path_dirty = true;
          std::cout << "Clipping window set to: xy = " << m_clipping_xy << " wh = " << m_clipping_wh << "\n";
        }
    }
}



vec2
painter_stroke_test::
brush_item_coordinate(ivec2 c)
{
  vec2 p;
  p = item_coordinates(c);

  if(m_matrix_brush)
    {
      p *= m_zoomer.transformation().scale();
    }

  if(m_translate_brush)
    {
      p += m_zoomer.transformation().translation();
    }
  return p;
}

vec2
painter_stroke_test::
item_coordinates(ivec2 c)
{
  /* m_zoomer.transformation() gives the transformation
     from item coordiantes to screen coordinates. We
     Want the inverse.
   */
  vec2 p(c);
  return m_zoomer.transformation().apply_inverse_to_point(p);
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
          m_grid_path_dirty = true;
        }
      break;

    case SDL_MOUSEMOTION:
      {
        ivec2 c(ev.motion.x + ev.motion.xrel,
                ev.motion.y + ev.motion.yrel);

        if(ev.motion.state & SDL_BUTTON(SDL_BUTTON_MIDDLE))
          {
            m_gradient_p0 = brush_item_coordinate(c);
          }
        else if(ev.motion.state & SDL_BUTTON(SDL_BUTTON_RIGHT))
          {
            m_gradient_p1 = brush_item_coordinate(c);
          }
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

        case SDLK_5:
          m_draw_grid = !m_draw_grid;
          if(m_draw_grid)
            {
              std::cout << "Draw grid\n";
            }
          else
            {
              std::cout << "Don't draw grid\n";
            }
          break;

        case SDLK_q:
          m_shear = m_shear2 = vec2(1.0f, 1.0f);
          break;

        case SDLK_p:
          m_stroke_width_in_pixels = !m_stroke_width_in_pixels;
          if(m_stroke_width_in_pixels)
            {
              std::cout << "Stroke width specified in pixels\n";
            }
          else
            {
              std::cout << "Stroke width specified in local coordinates\n";
            }
          break;

        case SDLK_v:
          m_force_square_viewport = !m_force_square_viewport;
          if(m_force_square_viewport)
            {
              std::cout << "Viewport made to square that contains window\n";
            }
          else
            {
              std::cout << "Viewport matches window dimension\n";
            }
          break;

        case SDLK_o:
          m_clipping_window = !m_clipping_window;
          std::cout << "Clipping window: " << on_off(m_clipping_window) << "\n";
          break;

        case SDLK_w:
          m_repeat_window = !m_repeat_window;
          std::cout << "Brush Repeat window: " << on_off(m_repeat_window) << "\n";
          break;

        case SDLK_y:
          m_matrix_brush = !m_matrix_brush;
          std::cout << "Matrix brush: " << on_off(m_matrix_brush) << "\n";
          break;

        case SDLK_t:
          m_translate_brush = !m_translate_brush;
          std::cout << "Translate brush: " << on_off(m_translate_brush) << "\n";
          break;

        case SDLK_h:
          if(m_gradient_draw_mode != draw_no_gradient)
            {
              m_repeat_gradient = !m_repeat_gradient;
              if(!m_repeat_gradient)
                {
                  std::cout << "non-";
                }
              std::cout << "repeat gradient mode\n";
            }
          break;

        case SDLK_m:
          if(m_join_style == PainterEnums::miter_joins)
            {
              m_have_miter_limit = !m_have_miter_limit;
              std::cout << "Miter limit ";
              if(!m_have_miter_limit)
                {
                  std::cout << "NOT ";
                }
              std::cout << "applied\n";
            }
          break;

        case SDLK_i:
          if(m_image && m_draw_fill)
            {
              cycle_value(m_image_filter, ev.key.keysym.mod & (KMOD_SHIFT|KMOD_CTRL|KMOD_ALT), number_image_filter_modes);
              std::cout << "Image filter mode set to: " << m_image_filter_mode_labels[m_image_filter] << "\n";
              if(m_image_filter == image_linear_filter && m_image->slack() < 1)
                {
                  std::cout << "\tWarning: image slack = " << m_image->slack()
                            << " which insufficient to correctly apply linear filter (requires atleast 1)\n";
                }
              else if(m_image_filter == image_cubic_filter && m_image->slack() < 2)
                {
                  std::cout << "\tWarning: image slack = " << m_image->slack()
                            << " which insufficient to correctly apply cubic filter (requires atleast 2)\n";
                }
            }
          break;

        case SDLK_s:
          if(m_draw_fill)
            {
              cycle_value(m_active_color_stop, ev.key.keysym.mod & (KMOD_SHIFT|KMOD_CTRL|KMOD_ALT), m_color_stops.size());
              std::cout << "Drawing color stop: " << m_color_stops[m_active_color_stop].first << "\n";
            }
          break;

        case SDLK_g:
          if(m_draw_fill)
            {
              cycle_value(m_gradient_draw_mode, ev.key.keysym.mod & (KMOD_SHIFT|KMOD_CTRL|KMOD_ALT), number_gradient_draw_modes);
              std::cout << "Gradient mode set to: " << m_gradient_mode_labels[m_gradient_draw_mode] << "\n";
            }
          break;

        case SDLK_j:
          cycle_value(m_join_style, ev.key.keysym.mod & (KMOD_SHIFT|KMOD_CTRL|KMOD_ALT), PainterEnums::number_join_styles);
          std::cout << "Join drawing mode set to: " << m_join_labels[m_join_style] << "\n";
          break;

        case SDLK_d:
          cycle_value(m_dash, ev.key.keysym.mod & (KMOD_SHIFT|KMOD_CTRL|KMOD_ALT), m_dash_patterns.size() + 1);
          if(is_dashed_stroking())
            {
              unsigned int P;
              P = dash_pattern();
              std::cout << "Set to stroke dashed with pattern: {";
              for(unsigned int i = 0; i < m_dash_patterns[P].size(); ++i)
                {
                  if(i != 0)
                    {
                      std::cout << ", ";
                    }
                  std::cout << "Draw(" << m_dash_patterns[P][i].m_draw_length
                            << "), Space(" << m_dash_patterns[P][i].m_space_length
                            << ")";
                }
              std::cout << "}\n";
            }
          else
            {
              std::cout << "Set to stroke non-dashed\n";
            }
          break;

        case SDLK_c:
          cycle_value(m_cap_style, ev.key.keysym.mod & (KMOD_SHIFT|KMOD_CTRL|KMOD_ALT), PainterEnums::number_cap_styles);
          std::cout << "Cap drawing mode set to: " << m_cap_labels[m_cap_style] << "\n";
          break;

        case SDLK_x:
          m_close_contour = !m_close_contour;
          std::cout << "Stroking contours as ";
          if(m_close_contour)
            {
              std::cout << "closed.\n";
            }
          else
            {
              std::cout << "open.\n";
            }
          break;

        case SDLK_r:
          if(m_draw_fill)
            {
              cycle_value(m_fill_rule, ev.key.keysym.mod & (KMOD_SHIFT|KMOD_CTRL|KMOD_ALT), m_end_fill_rule);
              if(m_fill_rule < PainterEnums::fill_rule_data_count)
                {
                  std::cout << "Fill rule set to: " << m_fill_labels[m_fill_rule] << "\n";
                }
              else
                {
                  const_c_array<int> wnd;
                  int value;
                  wnd = m_path.tessellation()->filled()->winding_numbers();
                  value = wnd[m_fill_rule - PainterEnums::fill_rule_data_count];
                  std::cout << "Fill rule set to custom fill rule: winding_number == "
                            << value << "\n";
                }
            }
          break;

        case SDLK_e:
          if(m_draw_fill)
            {
              m_fill_by_clipping = !m_fill_by_clipping;
              std::cout << "Set to ";
              if(m_fill_by_clipping)
                {
                  std::cout << "fill by drawing rectangle clipped to path\n";
                }
              else
                {
                  std::cout << "fill by drawing fill\n";
                }
            }
          break;

        case SDLK_f:
          m_draw_fill = !m_draw_fill;
          std::cout << "Set to ";
          if(!m_draw_fill)
            {
              std::cout << "NOT ";
            }
          std::cout << "draw path fill\n";
          break;

        case SDLK_SPACE:
          m_wire_frame = !m_wire_frame;
          std::cout << "Wire Frame = " << m_wire_frame << "\n";
          break;

        case SDLK_l:
          m_draw_stats = !m_draw_stats;
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
          read_path(m_path, buffer.str());
          return;
        }
    }

  m_path << vec2(300.0f, 300.0f)
         << Path::contour_end()
         << vec2(50.0f, 35.0f)
         << Path::control_point(60.0f, 50.0f)
         << vec2(70.0f, 35.0f)
         << Path::arc_degrees(180.0, vec2(70.0f, -100.0f))
         << Path::control_point(60.0f, -150.0f)
         << Path::control_point(30.0f, -50.0f)
         << vec2(0.0f, -100.0f)
         << Path::contour_end_arc_degrees(90.0f)
         << vec2(200.0f, 200.0f)
         << vec2(400.0f, 200.0f)
         << vec2(400.0f, 400.0f)
         << vec2(200.0f, 400.0f)
         << Path::contour_end()
         << vec2(-50.0f, 100.0f)
         << vec2(0.0f, 200.0f)
         << vec2(100.0f, 300.0f)
         << vec2(150.0f, 325.0f)
         << vec2(150.0f, 100.0f)
         << Path::contour_end();
}

void
painter_stroke_test::
create_stroked_path_attributes(void)
{
  reference_counted_ptr<const TessellatedPath> tessellated;
  reference_counted_ptr<const StrokedPath> stroked;
  const PainterAttributeData *data;
  const_c_array<PainterAttribute> miter_points;

  m_max_miter = 0.0f;
  tessellated = m_path.tessellation(-1.0f);
  stroked = tessellated->stroked();
  data = &stroked->miter_joins();

  miter_points = data->attribute_data_chunk(StrokedPath::join_chunk_with_closing_edge);
  for(unsigned p = 0, endp = miter_points.size(); p < endp; ++p)
    {
      float v;
      StrokedPath::point pt;

      StrokedPath::point::unpack_point(&pt, miter_points[p]);
      v = pt.miter_distance();
      if(isfinite(v))
        {
          m_max_miter = fastuidraw::t_max(m_max_miter, fastuidraw::t_abs(v));
        }
    }
  m_miter_limit = fastuidraw::t_min(100.0f, m_max_miter); //100 is an insane miter limit.

  if(m_print_path.m_value)
    {
      reference_counted_ptr<const TessellatedPath> tess;

      tess = m_path.tessellation();
      std::cout << "Path tessellated:\n";
      for(unsigned int c = 0; c < tess->number_contours(); ++c)
        {
          std::cout << "\tContour #" << c << "\n";
          for(unsigned int e = 0; e < tess->number_edges(c); ++e)
            {
              fastuidraw::const_c_array<fastuidraw::TessellatedPath::point> pts;

              std::cout << "\t\tEdge #" << e << " has "
                        << tess->edge_point_data(c, e).size() << " pts\n";
              pts = tess->edge_point_data(c, e);
              for(unsigned int i = 0; i < pts.size(); ++i)
                {
                  std::cout << "\t\t\tPoint #" << i << ":\n"
                            << "\t\t\t\tp          = " << pts[i].m_p << "\n"
                            << "\t\t\t\tp_t        = " << pts[i].m_p_t << "\n"
                            << "\t\t\t\tedge_d     = " << pts[i].m_distance_from_edge_start << "\n"
                            << "\t\t\t\tcontour_d  = " << pts[i].m_distance_from_contour_start << "\n"
                            << "\t\t\t\tedge_l     = " << pts[i].m_edge_length << "\n"
                            << "\t\t\t\tcontour_l  = " << pts[i].m_open_contour_length << "\n"
                            << "\t\t\t\tcontour_cl = " << pts[i].m_closed_contour_length << "\n";
                }
            }
        }
    }
}

void
painter_stroke_test::
construct_color_stops(void)
{
  ColorStopSequence S;
  reference_counted_ptr<ColorStopSequenceOnAtlas> h;

  S.add(ColorStop(u8vec4(0, 255, 0, 255), 0.0f));
  S.add(ColorStop(u8vec4(0, 255, 255, 255), 0.33f));
  S.add(ColorStop(u8vec4(255, 255, 0, 255), 0.66f));
  S.add(ColorStop(u8vec4(255, 0, 0, 255), 1.0f));
  h = FASTUIDRAWnew ColorStopSequenceOnAtlas(S, m_painter->colorstop_atlas(), 8);
  m_color_stops.push_back(named_color_stop("Default ColorStop Sequence", h));

  for(color_stop_arguments::hoard::const_iterator
        iter = m_color_stop_args.m_values.begin(),
        end = m_color_stop_args.m_values.end();
      iter != end; ++iter)
    {
      reference_counted_ptr<ColorStopSequenceOnAtlas> h;
      h = FASTUIDRAWnew ColorStopSequenceOnAtlas(iter->second->m_stops,
                                                 m_painter->colorstop_atlas(),
                                                 iter->second->m_discretization);
      m_color_stops.push_back(named_color_stop(iter->first, h));
    }
}

void
painter_stroke_test::
construct_dash_patterns(void)
{
  std::vector<PainterDashedStrokeParams::DashPatternElement> tmp;
  for(unsigned int i = 0, endi = m_dash_pattern_files.m_files.size(); i < endi; ++i)
    {
      std::ifstream file(m_dash_pattern_files.m_files[i].c_str());

      read_dash_pattern(tmp, file);
      if(!tmp.empty())
        {
          m_dash_patterns.push_back(std::vector<PainterDashedStrokeParams::DashPatternElement>());
          std::swap(tmp, m_dash_patterns.back());
        }
      tmp.clear();
    }

  if(m_dash_patterns.empty())
    {
      m_dash_patterns.push_back(std::vector<PainterDashedStrokeParams::DashPatternElement>());
      m_dash_patterns.back().push_back(PainterDashedStrokeParams::DashPatternElement(20.0f, 10.0f));
      m_dash_patterns.back().push_back(PainterDashedStrokeParams::DashPatternElement(15.0f, 10.0f));
      m_dash_patterns.back().push_back(PainterDashedStrokeParams::DashPatternElement(10.0f, 10.0f));
      m_dash_patterns.back().push_back(PainterDashedStrokeParams::DashPatternElement( 5.0f, 10.0f));
    }
}

void
painter_stroke_test::
draw_frame(void)
{
  ivec2 wh(dimensions());

  update_cts_params();

  glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  enable_wire_frame(m_wire_frame);

  m_painter->begin();

  if(m_force_square_viewport)
    {
      int d;
      d = std::max(wh.x(), wh.y());
      on_resize(d, d);
      /* Make the viewport dimensions as a square.
         The projection matrix below is the matrix to use for
         orthogonal projection so that the top is 0
      */
      float3x3 proj(float_orthogonal_projection_params(0, d, wh.y(), wh.y() - d));
      m_painter->transformation(proj);
    }
  else
    {
      on_resize(wh.x(), wh.y());
      float3x3 proj(float_orthogonal_projection_params(0, wh.x(), wh.y(), 0));
      m_painter->transformation(proj);
    }

  m_painter->save();

  /* draw grid using painter.
   */
  if(m_draw_grid && m_stroke_width_in_pixels && m_stroke_width > 0.0f)
    {
      if(m_grid_path_dirty && m_stroke_width > 0.0f)
        {
          Path grid_path;
          for(float x = 0, endx = wh.x(); x < endx; x += m_stroke_width)
            {
              grid_path << vec2(x, 0.0f)
                        << vec2(x, wh.y())
                        << Path::contour_end();
            }
          for(float y = 0, endy = wh.y(); y < endy; y += m_stroke_width)
            {
              grid_path << vec2(0.0f, y)
                        << vec2(wh.x(), y)
                        << Path::contour_end();
            }
          m_grid_path_dirty = false;
          m_grid_path.swap(grid_path);
        }

      PainterStrokeParams st;
      st.miter_limit(-1.0f);
      st.width(2.0f);

      PainterBrush stroke_pen;
      stroke_pen.pen(1.0f, 1.0f, 1.0f, 1.0f);

      m_painter->stroke_path(PainterData(&stroke_pen, &st),
                             m_grid_path,
                             false, PainterEnums::flat_caps, PainterEnums::no_joins,
                             false);
    }

  /* apply m_zoomer
   */
  m_painter->concat(m_zoomer.transformation().matrix3());

  /* apply shear
   */
  m_painter->shear(m_shear.x(), m_shear.y());

  /* apply rotation
   */
  m_painter->rotate(m_angle * M_PI / 180.0f);

  /* apply shear2
   */
  m_painter->shear(m_shear2.x(), m_shear2.y());

  if(m_clipping_window)
    {
      if(m_clip_window_path_dirty)
        {
          Path clip_window_path;
          clip_window_path << m_clipping_xy
                           << vec2(m_clipping_xy.x(), m_clipping_xy.y() + m_clipping_wh.y())
                           << m_clipping_xy + m_clipping_wh
                           << vec2(m_clipping_xy.x() + m_clipping_wh.x(), m_clipping_xy.y())
                           << Path::contour_end();
          m_clip_window_path.swap(clip_window_path);
          m_clip_window_path_dirty = false;
        }

      PainterBrush white;
      white.pen(1.0f, 1.0f, 1.0f, 1.0f);
      PainterStrokeParams st;
      st.miter_limit(-1.0f);
      st.width(4.0f);
      m_painter->save();
      m_painter->clipOutPath(m_clip_window_path, PainterEnums::nonzero_fill_rule);
      m_painter->stroke_path(PainterData(&white, &st), m_clip_window_path,
                             true, PainterEnums::flat_caps, PainterEnums::miter_joins,
                             false);
      m_painter->restore();
      m_painter->clipInRect(m_clipping_xy, m_clipping_wh);
    }


  if(m_draw_fill)
    {
      PainterBrush fill_brush;

      fill_brush.pen(1.0f, 1.0f, 1.0f, 1.0f);
      if(m_translate_brush)
        {
          fill_brush.transformation_translate(m_zoomer.transformation().translation());
        }
      else
        {
          fill_brush.no_transformation_translation();
        }

      if(m_matrix_brush)
        {
          float2x2 m;
          m(0, 0) = m(1, 1) = m_zoomer.transformation().scale();
          fill_brush.transformation_matrix(m);
        }
      else
        {
          fill_brush.no_transformation_matrix();
        }

      if(m_repeat_window)
        {
          fill_brush.repeat_window(m_repeat_xy, m_repeat_wh);
        }
      else
        {
          fill_brush.no_repeat_window();
        }

      if(m_gradient_draw_mode == draw_linear_gradient)
        {
          fill_brush.linear_gradient(m_color_stops[m_active_color_stop].second,
                                     m_gradient_p0, m_gradient_p1, m_repeat_gradient);
        }
      else if(m_gradient_draw_mode == draw_radial_gradient)
        {
          fill_brush.radial_gradient(m_color_stops[m_active_color_stop].second,
                                     m_gradient_p0, m_gradient_r0,
                                     m_gradient_p1, m_gradient_r1,
                                     m_repeat_gradient);
        }
      else
        {
          fill_brush.no_gradient();
        }


      if(!m_image || m_image_filter == no_image)
        {
          fill_brush.no_image();
        }
      else
        {
          enum PainterBrush::image_filter f;
          switch(m_image_filter)
            {
            case image_nearest_filter:
              f = PainterBrush::image_filter_nearest;
              break;
            case image_linear_filter:
              f = PainterBrush::image_filter_linear;
              break;
            case image_cubic_filter:
              f = PainterBrush::image_filter_cubic;
              break;
            default:
              assert(!"Incorrect value for m_image_filter!");
              f = PainterBrush::image_filter_nearest;
            }
          fill_brush.sub_image(m_image, m_image_offset, m_image_size, f);
        }

      if(m_fill_rule < PainterEnums::fill_rule_data_count)
        {
          enum PainterEnums::fill_rule_t v;
          v = static_cast<enum PainterEnums::fill_rule_t>(m_fill_rule);
          if(m_fill_by_clipping)
            {
              m_painter->save();
              m_painter->clipInPath(m_path, v);
              m_painter->transformation(float3x3());
              m_painter->draw_rect(PainterData(&fill_brush), vec2(-1.0f, -1.0f), vec2(2.0f, 2.0f));
              m_painter->restore();
            }
          else
            {
              m_painter->fill_path(PainterData(&fill_brush), m_path, v);
            }
        }
      else
        {
          const_c_array<int> wnd;
          int value;

          wnd = m_path.tessellation()->filled()->winding_numbers();
          value = wnd[m_fill_rule - PainterEnums::fill_rule_data_count];

          if(m_fill_by_clipping)
            {
              m_painter->save();
              m_painter->clipInPath(m_path, WindingValueFillRule(value));
              m_painter->transformation(float3x3());
              m_painter->draw_rect(PainterData(&fill_brush), vec2(-1.0f, -1.0f), vec2(2.0f, 2.0f));
              m_painter->restore();
            }
          else
            {
              m_painter->fill_path(PainterData(&fill_brush), m_path, WindingValueFillRule(value));
            }
        }
    }

  if(!m_transparent_blue_pen)
    {
      m_transparent_blue_pen = m_painter->packed_value_pool().create_packed_value(PainterBrush().pen(1.0f, 1.0f, 1.0f, 0.5f));
    }

  if(m_stroke_width > 0.0f)
    {
      if(is_dashed_stroking())
        {
          PainterDashedStrokeParams st;
          if(m_have_miter_limit)
            {
              st.miter_limit(m_miter_limit);
            }
          else
            {
              st.miter_limit(-1.0f);
            }
          st.width(m_stroke_width);

          unsigned int D(dash_pattern());
          const_c_array<PainterDashedStrokeParams::DashPatternElement> dash_ptr(&m_dash_patterns[D][0], m_dash_patterns[D].size());
          st.dash_pattern(dash_ptr);

          if(m_stroke_width_in_pixels)
            {
              m_painter->stroke_dashed_path_pixel_width(PainterData(m_transparent_blue_pen, &st),
                                                        m_path, m_close_contour,
                                                        static_cast<enum PainterEnums::cap_style>(m_cap_style),
                                                        static_cast<enum PainterEnums::join_style>(m_join_style),
                                                        m_stroke_aa);
            }
          else
            {
              m_painter->stroke_dashed_path(PainterData(m_transparent_blue_pen, &st),
                                            m_path, m_close_contour,
                                            static_cast<enum PainterEnums::cap_style>(m_cap_style),
                                            static_cast<enum PainterEnums::join_style>(m_join_style),
                                            m_stroke_aa);
            }
        }
      else
        {
          PainterStrokeParams st;
          if(m_have_miter_limit)
            {
              st.miter_limit(m_miter_limit);
            }
          else
            {
              st.miter_limit(-1.0f);
            }
          st.width(m_stroke_width);

          if(m_stroke_width_in_pixels)
            {
              m_painter->stroke_path_pixel_width(PainterData(m_transparent_blue_pen, &st),
                                                 m_path, m_close_contour,
                                                 static_cast<enum PainterEnums::cap_style>(m_cap_style),
                                                 static_cast<enum PainterEnums::join_style>(m_join_style),
                                                 m_stroke_aa);
            }
          else
            {
              m_painter->stroke_path(PainterData(m_transparent_blue_pen, &st),
                                     m_path, m_close_contour,
                                     static_cast<enum PainterEnums::cap_style>(m_cap_style),
                                     static_cast<enum PainterEnums::join_style>(m_join_style),
                                     m_stroke_aa);
            }
        }
    }

  if(m_draw_fill && m_gradient_draw_mode != draw_no_gradient)
    {
      float r0, r1;
      vec2 p0, p1;
      float inv_scale;

      inv_scale = 1.0f / m_zoomer.transformation().scale();
      r0 = 15.0f * inv_scale;
      r1 = 30.0f * inv_scale;

      p0 = m_gradient_p0;
      p1 = m_gradient_p1;


      if(m_translate_brush)
        {
          p0 -= m_zoomer.transformation().translation();
          p1 -= m_zoomer.transformation().translation();
        }

      if(m_matrix_brush)
        {
          p0 *= inv_scale;
          p1 *= inv_scale;
        }

      if(!m_black_pen)
        {
          assert(!m_white_pen);
          m_white_pen = m_painter->packed_value_pool().create_packed_value(PainterBrush().pen(1.0, 1.0, 1.0, 1.0));
          m_black_pen = m_painter->packed_value_pool().create_packed_value(PainterBrush().pen(0.0, 0.0, 0.0, 1.0));
        }

      m_painter->draw_rect(PainterData(m_black_pen), p0 - vec2(r1) * 0.5, vec2(r1));
      m_painter->draw_rect(PainterData(m_white_pen), p0 - vec2(r0) * 0.5, vec2(r0));

      m_painter->draw_rect(PainterData(m_white_pen), p1 - vec2(r1) * 0.5, vec2(r1));
      m_painter->draw_rect(PainterData(m_black_pen), p1 - vec2(r0) * 0.5, vec2(r0));
    }

  m_painter->restore();

  if(m_draw_stats)
    {
      std::ostringstream ostr;
      ostr << "\nAttribs: "
           << m_painter->query_stat(PainterPacker::num_attributes)
           << "\nIndices: "
           << m_painter->query_stat(PainterPacker::num_indices)
           << "\nGenericData: "
           << m_painter->query_stat(PainterPacker::num_generic_datas)
           << "\n";

      PainterBrush brush;
      brush.pen(0.0f, 1.0f, 1.0f, 1.0f);
      draw_text(ostr.str(), 32.0f, m_font, GlyphRender(curve_pair_glyph), PainterData(&brush));
    }

  m_painter->end();

}

void
painter_stroke_test::
derived_init(int w, int h)
{
  //put into unit of per ms.
  m_window_change_rate.m_value /= 1000.0f;
  m_change_stroke_width_rate.m_value /= 1000.0f;
  m_change_miter_limit_rate.m_value /= 1000.0f;
  m_font = FontFreeType::create(m_font_file.m_value.c_str(), m_ft_lib, FontFreeType::RenderParams());

  construct_path();
  create_stroked_path_attributes();
  construct_color_stops();
  construct_dash_patterns();
  m_end_fill_rule = m_path.tessellation()->filled()->winding_numbers().size() + PainterEnums::fill_rule_data_count;

  /* set transformation to center and contain path.
   */
  vec2 p0, p1, delta, dsp(w, h), ratio, mid;
  float mm;
  p0 = m_path.tessellation()->bounding_box_min();
  p1 = m_path.tessellation()->bounding_box_max();

  delta = p1 - p0 + vec2(m_stroke_width, m_stroke_width);
  ratio = delta / dsp;
  mm = t_max(0.00001f, t_max(ratio.x(), ratio.y()) );
  mid = 0.5 * (p1 + p0);

  ScaleTranslate<float> sc, tr1, tr2;
  tr1.translation(-mid);
  sc.scale( 1.0f / mm);
  tr2.translation(dsp * 0.5f);
  m_zoomer.transformation(tr2 * sc * tr1);

  if(!m_image_file.m_value.empty())
    {
      std::vector<u8vec4> image_data;
      ivec2 image_size;

      image_size = load_image_to_array(m_image_file.m_value, image_data);
      if(image_size.x() > 0 && image_size.y() > 0)
        {
          m_image = Image::create(m_painter->image_atlas(), image_size.x(), image_size.y(),
                                  cast_c_array(image_data), m_image_slack.m_value);
        }
    }

  if(m_image)
    {
      if(m_sub_image_x.m_value < 0 || m_sub_image_y.m_value < 0
         || m_sub_image_w.m_value < 0 || m_sub_image_h.m_value < 0)
        {
          m_image_offset = uvec2(0, 0);
          m_image_size = uvec2(m_image->dimensions());
        }
      else
        {
          m_image_offset = uvec2(m_sub_image_x.m_value, m_sub_image_y.m_value);
          m_image_size = uvec2(m_sub_image_w.m_value, m_sub_image_h.m_value);
        }
    }

  m_gradient_p0 = item_coordinates(ivec2(0, 0));
  m_gradient_p1 = item_coordinates(ivec2(w, h));

  m_gradient_r0 = 0.0f;
  m_gradient_r1 = 200.0f / m_zoomer.transformation().scale();

  m_repeat_xy = vec2(0.0f, 0.0f);
  m_repeat_wh = m_path.tessellation()->bounding_box_max() - m_path.tessellation()->bounding_box_min();

  m_clipping_xy = m_path.tessellation()->bounding_box_min();
  m_clipping_wh = m_repeat_wh;

  m_draw_timer.restart();
}

int
main(int argc, char **argv)
{
  painter_stroke_test P;
  return P.main(argc, argv);
}
