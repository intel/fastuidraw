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
#include "text_helper.hpp"
#include "command_line_list.hpp"

using namespace fastuidraw;

c_string
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
    if (static_cast<unsigned int>(location + 1) < args.size() && args[location] == "add_dash_pattern")
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

class WindingValueFillRule:public CustomFillRuleBase
{
public:
  WindingValueFillRule(int v = 0):
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

bool
everything_filled(int)
{
  return true;
}

bool
is_miter_join_style(unsigned int js)
{
  return js == PainterEnums::miter_clip_joins
    || js == PainterEnums::miter_bevel_joins
    || js == PainterEnums::miter_joins;
}

#ifndef FASTUIDRAW_GL_USE_GLES

class EnableWireFrameAction:public PainterDraw::Action
{
public:
  explicit
  EnableWireFrameAction(bool b):
    m_lines(b)
  {}

  virtual
  fastuidraw::gpu_dirty_state
  execute(PainterDraw::APIBase *) const
  {
    if (m_lines)
      {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glLineWidth(4.0);
      }
    else
      {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
      }
    return 0u;
  }

private:
  bool m_lines;
};

#endif

class PerPath
{
public:
  PerPath(const Path &path, const std::string &label,
          int w, int h, bool from_gylph);

  Path m_path;
  std::string m_label;
  bool m_from_glyph;
  unsigned int m_fill_rule;
  unsigned int m_end_fill_rule;
  vec2 m_shear, m_shear2;
  float m_angle;
  PanZoomTrackerSDLEvent m_path_zoomer;

  bool m_translate_brush, m_matrix_brush;

  vec2 m_gradient_p0, m_gradient_p1;
  float m_gradient_r0, m_gradient_r1;
  bool m_repeat_gradient;

  bool m_repeat_window;
  vec2 m_repeat_xy, m_repeat_wh;

  bool m_clipping_window;
  vec2 m_clipping_xy, m_clipping_wh;
};

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
  construct_paths(int w, int h);

  void
  per_path_processing(void);

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

  PanZoomTrackerSDLEvent&
  zoomer(void)
  {
    return m_paths[m_selected_path].m_path_zoomer;
  }

  const Path&
  path(void)
  {
    return m_paths[m_selected_path].m_path;
  }

  unsigned int&
  current_fill_rule(void)
  {
    return m_paths[m_selected_path].m_fill_rule;
  }

  unsigned int
  current_end_fill_rule(void)
  {
    return m_paths[m_selected_path].m_end_fill_rule;
  }

  vec2&
  shear(void)
  {
    return m_paths[m_selected_path].m_shear;
  }

  vec2&
  shear2(void)
  {
    return m_paths[m_selected_path].m_shear2;
  }

  float&
  angle(void)
  {
    return m_paths[m_selected_path].m_angle;
  }

  vec2&
  gradient_p0(void)
  {
    return m_paths[m_selected_path].m_gradient_p0;
  }

  vec2&
  gradient_p1(void)
  {
    return m_paths[m_selected_path].m_gradient_p1;
  }

  float&
  gradient_r0(void)
  {
    return m_paths[m_selected_path].m_gradient_r0;
  }

  float&
  gradient_r1(void)
  {
    return m_paths[m_selected_path].m_gradient_r1;
  }

  bool&
  repeat_gradient(void)
  {
    return m_paths[m_selected_path].m_repeat_gradient;
  }

  bool&
  translate_brush(void)
  {
    return m_paths[m_selected_path].m_translate_brush;
  }

  bool&
  matrix_brush(void)
  {
    return m_paths[m_selected_path].m_matrix_brush;
  }

  bool&
  repeat_window(void)
  {
    return m_paths[m_selected_path].m_repeat_window;
  }

  vec2&
  repeat_xy(void)
  {
    return m_paths[m_selected_path].m_repeat_xy;
  }

  vec2&
  repeat_wh(void)
  {
    return m_paths[m_selected_path].m_repeat_wh;
  }

  bool&
  clipping_window(void)
  {
    return m_paths[m_selected_path].m_clipping_window;
  }

  vec2&
  clipping_xy(void)
  {
    return m_paths[m_selected_path].m_clipping_xy;
  }

  vec2&
  clipping_wh(void)
  {
    return m_paths[m_selected_path].m_clipping_wh;
  }

  void
  draw_rect(const vec2 &pt, float r, const PainterData &d);

  void
  draw_scene(bool drawing_wire_frame);

  command_line_argument_value<float> m_change_miter_limit_rate;
  command_line_argument_value<float> m_change_stroke_width_rate;
  command_line_argument_value<float> m_window_change_rate;
  command_line_argument_value<float> m_radial_gradient_change_rate;
  command_line_list<std::string> m_path_file_list;
  DashPatternList m_dash_pattern_files;
  command_line_argument_value<bool> m_print_path;
  color_stop_arguments m_color_stop_args;
  command_line_argument_value<std::string> m_image_file;
  command_line_argument_value<unsigned int> m_image_slack;
  command_line_argument_value<bool> m_use_atlas;
  command_line_argument_value<int> m_sub_image_x, m_sub_image_y;
  command_line_argument_value<int> m_sub_image_w, m_sub_image_h;
  command_line_argument_value<std::string> m_font_file;
  command_line_list<uint32_t> m_character_code_list;
  command_line_argument_value<float> m_stroke_red;
  command_line_argument_value<float> m_stroke_green;
  command_line_argument_value<float> m_stroke_blue;
  command_line_argument_value<float> m_stroke_alpha;
  command_line_argument_value<float> m_fill_red;
  command_line_argument_value<float> m_fill_green;
  command_line_argument_value<float> m_fill_blue;
  command_line_argument_value<float> m_fill_alpha;
  command_line_argument_value<float> m_draw_line_red;
  command_line_argument_value<float> m_draw_line_green;
  command_line_argument_value<float> m_draw_line_blue;
  command_line_argument_value<float> m_draw_line_alpha;
  command_line_argument_value<bool> m_init_pan_zoom;
  command_line_argument_value<float> m_initial_zoom;
  command_line_argument_value<float> m_initial_pan_x;
  command_line_argument_value<float> m_initial_pan_y;

  std::vector<PerPath> m_paths;
  reference_counted_ptr<Image> m_image;
  uvec2 m_image_offset, m_image_size;
  std::vector<named_color_stop> m_color_stops;
  std::vector<std::vector<PainterDashedStrokeParams::DashPatternElement> > m_dash_patterns;
  reference_counted_ptr<const FontBase> m_font;

  vecN<std::string, number_gradient_draw_modes> m_gradient_mode_labels;
  vecN<std::string, PainterEnums::number_cap_styles> m_cap_labels;
  vecN<std::string, PainterEnums::number_join_styles> m_join_labels;
  vecN<std::string, PainterEnums::fill_rule_data_count> m_fill_labels;
  vecN<std::string, number_image_filter_modes> m_image_filter_mode_labels;

  PainterPackedValue<PainterBrush> m_black_pen;
  PainterPackedValue<PainterBrush> m_white_pen;
  PainterPackedValue<PainterBrush> m_stroke_pen;
  PainterPackedValue<PainterBrush> m_draw_line_pen;

  Path m_rect;

  unsigned int m_selected_path;
  unsigned int m_join_style;
  unsigned int m_cap_style;
  bool m_close_contour;

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

  bool m_have_miter_limit;
  float m_miter_limit, m_stroke_width;
  bool m_draw_fill, m_aa_fill_by_stroking;
  unsigned int m_active_color_stop;
  unsigned int m_gradient_draw_mode;
  unsigned int m_image_filter;
  bool m_apply_mipmapping;
  bool m_draw_stats;
  float m_curve_flatness;

  bool m_with_aa;
  bool m_wire_frame;
  bool m_stroke_width_in_pixels;

  bool m_fill_by_clipping;
  bool m_draw_grid;

  simple_time m_draw_timer, m_fps_timer;
  Path m_grid_path;
  bool m_grid_path_dirty;

  Path m_clip_window_path;
  bool m_clip_window_path_dirty;
};

///////////////////////////////////
// PerPath methods
PerPath::
PerPath(const Path &path, const std::string &label, int w, int h, bool from_gylph):
  m_path(path),
  m_label(label),
  m_from_glyph(from_gylph),
  m_fill_rule(PainterEnums::odd_even_fill_rule),
  m_end_fill_rule(PainterEnums::fill_rule_data_count),
  m_shear(1.0f, 1.0f),
  m_shear2(1.0f, 1.0f),
  m_angle(0.0f),
  m_translate_brush(false),
  m_matrix_brush(false),
  m_repeat_gradient(true),
  m_repeat_window(false),
  m_clipping_window(false)
{
  m_end_fill_rule =
    m_path.tessellation()->filled()->subset(0).winding_numbers().size() + PainterEnums::fill_rule_data_count;

  /* set transformation to center and contain path. */
  vec2 p0, p1, delta, dsp(w, h), ratio, mid;
  float mm;
  p0 = m_path.tessellation()->bounding_box_min();
  p1 = m_path.tessellation()->bounding_box_max();

  delta = p1 - p0;
  ratio = delta / dsp;
  mm = t_max(0.00001f, t_max(ratio.x(), ratio.y()) );
  mid = 0.5 * (p1 + p0);

  ScaleTranslate<float> sc, tr1, tr2;
  tr1.translation(-mid);
  sc.scale( 1.0f / mm);
  tr2.translation(dsp * 0.5f);
  m_path_zoomer.transformation(tr2 * sc * tr1);

  m_gradient_p0 = p0;
  m_gradient_p1 = p1;

  m_gradient_r0 = 0.0f;
  m_gradient_r1 = 200.0f / m_path_zoomer.transformation().scale();

  m_repeat_xy = vec2(0.0f, 0.0f);
  m_repeat_wh = m_path.tessellation()->bounding_box_max() - m_path.tessellation()->bounding_box_min();

  m_clipping_xy = m_path.tessellation()->bounding_box_min();
  m_clipping_wh = m_repeat_wh;
}

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
  m_path_file_list("add_path_file",
                   "add a path read from file to path list; if path list is empty then "
                   "a default path will be used to render ",
                   *this),
  m_dash_pattern_files(*this),
  m_print_path(false, "print_path",
               "If true, print the geometry data of the path drawn to stdout",
               *this),
  m_color_stop_args(*this),
  m_image_file("", "image", "if a valid file name, apply an image to drawing the fill", *this),
  m_image_slack(0, "image_slack", "amount of slack on tiles when loading image", *this),
  m_use_atlas(true, "use_atlas",
              "If false, each image is realized as a texture; if "
              "GL_ARB_bindless_texture or GL_NV_bindless_texture "
              "is supported, the Image objects are realized as bindless "
              "texture, thus avoding draw breaks; if both of these "
              "extensions is not present, then images are realized as "
              "bound textures which means that a draw break will be present "
              "whenever the image changes, harming performance.",
              *this),
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
  m_font_file(default_font(), "font", "File from which to take font", *this),
  m_character_code_list("add_path_character_code",
                        "add a path of a glyph selected via character code", *this),
  m_stroke_red(1.0f, "stroke_red", "red component of stroking pen color", *this),
  m_stroke_green(1.0f, "stroke_green", "green component of stroking pen color", *this),
  m_stroke_blue(1.0f, "stroke_blue", "blue component of stroking pen olor", *this),
  m_stroke_alpha(0.5f, "stroke_alpha", "alpha component of stroking pen color", *this),
  m_fill_red(1.0f, "fill_red", "red component of fill pen color", *this),
  m_fill_green(1.0f, "fill_green", "green component of fill pen color", *this),
  m_fill_blue(1.0f, "fill_blue", "blue component of fill pen color", *this),
  m_fill_alpha(1.0f, "fill_alpha", "alpha component of fill pen color", *this),
  m_draw_line_red(1.0f, "draw_line_red", "red component when showing line-rasterization", *this),
  m_draw_line_green(0.0f, "draw_line_green", "green component when showing line-rasterization", *this),
  m_draw_line_blue(0.0f, "draw_line_blue", "blue component when showing line-rasterization", *this),
  m_draw_line_alpha(0.4f, "draw_line_alpha", "alpha component when showing line-rasterization", *this),
  m_init_pan_zoom(false, "init_pan_zoom", "If true, initialize the view with values given by "
                  "initial_zoom, initial_pan_x and initial_pan_y; if false initialize each path "
                  "view so that the entire path just fits on screen", *this),
  m_initial_zoom(1.0f, "initial_zoom", "initial zoom for view if init_pan_zoom is true", *this),
  m_initial_pan_x(0.0f, "initial_pan_x", "initial x-offset for view if init_pan_zoom is true", *this),
  m_initial_pan_y(0.0f, "initial_pan_y", "initial y-offset for view if init_pan_zoom is true", *this),
  m_selected_path(0),
  m_join_style(PainterEnums::rounded_joins),
  m_cap_style(PainterEnums::square_caps),
  m_close_contour(true),
  m_dash(0),
  m_have_miter_limit(true),
  m_miter_limit(5.0f),
  m_stroke_width(10.0f),
  m_draw_fill(false), m_aa_fill_by_stroking(false),
  m_active_color_stop(0),
  m_gradient_draw_mode(draw_no_gradient),
  m_image_filter(image_nearest_filter),
  m_apply_mipmapping(false),
  m_draw_stats(false),
  m_with_aa(true),
  m_wire_frame(false),
  m_stroke_width_in_pixels(false),
  m_fill_by_clipping(false),
  m_draw_grid(false),
  m_grid_path_dirty(true),
  m_clip_window_path_dirty(true)
{
  std::cout << "Controls:\n"
            << "\tn: toogle using arc-paths for stroking\n"
            << "\tk: select next path\n"
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
            << "\tv: toggle linearize from arc-path\n"
            << "\tb: decrease miter limit(hold left-shift for slower rate and right shift for faster)\n"
            << "\tn: increase miter limit(hold left-shift for slower rate and right shift for faster)\n"
            << "\tm: toggle miter limit enforced\n"
            << "\tf: toggle drawing path fill\n"
            << "\tr: cycle through fill rules\n"
            << "\te: toggle fill by drawing clip rect\n"
            << "\ti: cycle through image filter to apply to fill (no image, nearest, linear, cubic)\n"
            << "\tctrl-i: toggle mipmap filtering when applying an image\n"
            << "\ts: cycle through defined color stops for gradient\n"
            << "\tg: cycle through gradient types (linear or radial)\n"
            << "\th: toggle repeat gradient\n"
            << "\tt: toggle translate brush\n"
            << "\ty: toggle matrix brush\n"
            << "\to: toggle clipping window\n"
            << "\tz: increase/decrease curve flatness\n"
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
  m_join_labels[PainterEnums::miter_clip_joins] = "miter_clip_joins";
  m_join_labels[PainterEnums::miter_bevel_joins] = "miter_bevel_joins";
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

  m_rect << vec2(-0.5f, -0.5f)
         << vec2(-0.5f, +0.5f)
         << vec2(+0.5f, +0.5f)
         << vec2(+0.5f, -0.5f)
         << Path::contour_end();
}


void
painter_stroke_test::
update_cts_params(void)
{
  const Uint8 *keyboard_state = SDL_GetKeyboardState(nullptr);
  FASTUIDRAWassert(keyboard_state != nullptr);

  float speed = static_cast<float>(m_draw_timer.restart_us()), speed_stroke, speed_shear;
  speed /= 1000.0f;

  if (keyboard_state[SDL_SCANCODE_LSHIFT])
    {
      speed *= 0.1f;
    }
  if (keyboard_state[SDL_SCANCODE_RSHIFT])
    {
      speed *= 10.0f;
    }

  speed_shear = 0.01f * speed;
  if (keyboard_state[SDL_SCANCODE_LCTRL] || keyboard_state[SDL_SCANCODE_RCTRL])
    {
      speed_shear = -speed_shear;
    }

  vec2 *pshear(&shear());
  c_string shear_txt = "";
  if (keyboard_state[SDL_SCANCODE_RETURN])
    {
      pshear = &shear2();
      shear_txt = "2";
    }

  if (keyboard_state[SDL_SCANCODE_6])
    {
      pshear->x() += speed_shear;
      std::cout << "Shear" << shear_txt << " set to: " << *pshear << "\n";
    }
  if (keyboard_state[SDL_SCANCODE_7])
    {
      pshear->y() += speed_shear;
      std::cout << "Shear " << shear_txt << " set to: " << *pshear << "\n";
    }

  if (keyboard_state[SDL_SCANCODE_9])
    {
      angle() += speed * 0.1f;
      std::cout << "Angle set to: " << angle() << "\n";
    }
  if (keyboard_state[SDL_SCANCODE_0])
    {
      angle() -= speed * 0.1f;
      std::cout << "Angle set to: " << angle() << "\n";
    }

  speed_stroke = speed * m_change_stroke_width_rate.value();
  if (!m_stroke_width_in_pixels)
    {
      speed_stroke /= zoomer().transformation().scale();
    }

  if (keyboard_state[SDL_SCANCODE_RIGHTBRACKET])
    {
      m_grid_path_dirty = true;
      m_stroke_width += speed_stroke;
    }

  if (keyboard_state[SDL_SCANCODE_LEFTBRACKET] && m_stroke_width > 0.0f)
    {
      m_grid_path_dirty = true;
      m_stroke_width -= speed_stroke;
      m_stroke_width = fastuidraw::t_max(m_stroke_width, 0.0f);
    }

  if (keyboard_state[SDL_SCANCODE_RIGHTBRACKET] || keyboard_state[SDL_SCANCODE_LEFTBRACKET])
    {
      std::cout << "Stroke width set to: " << m_stroke_width << "\n";
    }

  if (repeat_window())
    {
      vec2 *changer;
      float delta, delta_y;

      delta = m_window_change_rate.value() * speed / zoomer().transformation().scale();
      if (keyboard_state[SDL_SCANCODE_LCTRL] || keyboard_state[SDL_SCANCODE_RCTRL])
        {
          changer = &repeat_wh();
          delta_y = delta;
        }
      else
        {
          changer = &repeat_xy();
          delta_y = -delta;
        }

      if (keyboard_state[SDL_SCANCODE_UP])
        {
          changer->y() += delta_y;
          changer->y() = fastuidraw::t_max(0.0f, changer->y());
        }

      if (keyboard_state[SDL_SCANCODE_DOWN])
        {
          changer->y() -= delta_y;
          changer->y() = fastuidraw::t_max(0.0f, changer->y());
        }

      if (keyboard_state[SDL_SCANCODE_RIGHT])
        {
          changer->x() += delta;
        }

      if (keyboard_state[SDL_SCANCODE_LEFT])
        {
          changer->x() -= delta;
          changer->x() = fastuidraw::t_max(0.0f, changer->x());
        }

      if (keyboard_state[SDL_SCANCODE_UP] || keyboard_state[SDL_SCANCODE_DOWN]
         || keyboard_state[SDL_SCANCODE_RIGHT] || keyboard_state[SDL_SCANCODE_LEFT])
        {
          std::cout << "Brush repeat window set to: xy = " << repeat_xy() << " wh = " << repeat_wh() << "\n";
        }
    }


  if (m_gradient_draw_mode == draw_radial_gradient)
    {
      float delta;

      delta = m_radial_gradient_change_rate.value() * speed / zoomer().transformation().scale();
      if (keyboard_state[SDL_SCANCODE_1])
        {
          gradient_r0() -= delta;
          gradient_r0() = fastuidraw::t_max(0.0f, gradient_r0());
        }

      if (keyboard_state[SDL_SCANCODE_2])
        {
          gradient_r0() += delta;
        }
      if (keyboard_state[SDL_SCANCODE_3])
        {
          gradient_r1() -= delta;
          gradient_r1() = fastuidraw::t_max(0.0f, gradient_r1());
        }

      if (keyboard_state[SDL_SCANCODE_4])
        {
          gradient_r1() += delta;
        }

      if (keyboard_state[SDL_SCANCODE_1] || keyboard_state[SDL_SCANCODE_2]
         || keyboard_state[SDL_SCANCODE_3] || keyboard_state[SDL_SCANCODE_4])
        {
          std::cout << "Radial gradient values set to: r0 = "
                    << gradient_r0() << " r1 = " << gradient_r1() << "\n";
        }
    }


  if (is_miter_join_style(m_join_style) && m_have_miter_limit)
    {
      if (keyboard_state[SDL_SCANCODE_N])
        {
          m_miter_limit += m_change_miter_limit_rate.value() * speed;
        }

      if (keyboard_state[SDL_SCANCODE_B])
        {
          m_miter_limit -= m_change_miter_limit_rate.value() * speed;
          m_miter_limit = fastuidraw::t_max(0.0f, m_miter_limit);
        }

      if (keyboard_state[SDL_SCANCODE_N] || keyboard_state[SDL_SCANCODE_B])
        {
          std::cout << "Miter-limit set to: " << m_miter_limit << "\n";
        }
    }

  if (clipping_window())
    {
      vec2 *changer;
      float delta, delta_y;

      delta = m_window_change_rate.value() * speed / zoomer().transformation().scale();
      if (keyboard_state[SDL_SCANCODE_LCTRL] || keyboard_state[SDL_SCANCODE_RCTRL])
        {
          changer = &clipping_wh();
          delta_y = delta;
        }
      else
        {
          changer = &clipping_xy();
          delta_y = -delta;
        }

      if (keyboard_state[SDL_SCANCODE_KP_8])
        {
          changer->y() += delta_y;
        }

      if (keyboard_state[SDL_SCANCODE_KP_2])
        {
          changer->y() -= delta_y;
        }

      if (keyboard_state[SDL_SCANCODE_KP_6])
        {
          changer->x() += delta;
        }

      if (keyboard_state[SDL_SCANCODE_KP_4])
        {
          changer->x() -= delta;
        }

      if (keyboard_state[SDL_SCANCODE_KP_2] || keyboard_state[SDL_SCANCODE_KP_4]
         || keyboard_state[SDL_SCANCODE_KP_6] || keyboard_state[SDL_SCANCODE_KP_8])
        {
          m_clip_window_path_dirty = true;
          std::cout << "Clipping window set to: xy = " << clipping_xy() << " wh = " << clipping_wh() << "\n";
        }
    }
}

void
painter_stroke_test::
draw_rect(const vec2 &pt, float r, const PainterData &d)
{
  m_painter->save();
  m_painter->translate(pt);
  m_painter->scale(r);
  m_painter->fill_path(d, m_rect, PainterEnums::odd_even_fill_rule, true);
  m_painter->restore();
}

vec2
painter_stroke_test::
brush_item_coordinate(ivec2 scr)
{
  vec2 p;
  p = item_coordinates(scr);

  if (matrix_brush())
    {
      p *= zoomer().transformation().scale();
    }

  if (translate_brush())
    {
      p += zoomer().transformation().translation();
    }
  return p;
}

vec2
painter_stroke_test::
item_coordinates(ivec2 scr)
{
  vec2 p(scr);

  /* unapply zoomer()
   */
  p = zoomer().transformation().apply_inverse_to_point(p);

  /* unapply shear()
   */
  p /= shear();

  /* unapply rotation by angle()
   */
  float s, c, a;
  float2x2 tr;
  a = -angle() * M_PI / 180.0f;
  s = t_sin(a);
  c = t_cos(a);

  tr(0, 0) = c;
  tr(1, 0) = s;
  tr(0, 1) = -s;
  tr(1, 1) = c;

  p = tr * p;

  /* unapply shear2()
   */
  p /= shear2();

  /* unapply glyph-flip
   */
  if (m_paths[m_selected_path].m_from_glyph)
    {
      float y;
      y = path().tessellation()->bounding_box_min().y()
        + path().tessellation()->bounding_box_max().y();
      p.y() -= y;
      p.y() *= -1.0f;
    }

  return p;
}

void
painter_stroke_test::
handle_event(const SDL_Event &ev)
{
  zoomer().handle_event(ev);
  switch(ev.type)
    {
    case SDL_QUIT:
      end_demo(0);
      break;

    case SDL_WINDOWEVENT:
      if (ev.window.event == SDL_WINDOWEVENT_RESIZED)
        {
          m_grid_path_dirty = true;
          on_resize(ev.window.data1, ev.window.data2);
        }
      break;

    case SDL_MOUSEMOTION:
      {
        ivec2 c(ev.motion.x + ev.motion.xrel,
                ev.motion.y + ev.motion.yrel);

        if (ev.motion.state & SDL_BUTTON(SDL_BUTTON_MIDDLE))
          {
            gradient_p0() = brush_item_coordinate(c);
          }
        else if (ev.motion.state & SDL_BUTTON(SDL_BUTTON_RIGHT))
          {
            gradient_p1() = brush_item_coordinate(c);
          }
      }
      break;

    case SDL_KEYUP:
      switch(ev.key.keysym.sym)
        {
        case SDLK_ESCAPE:
          end_demo(0);
          break;

        case SDLK_n:
          m_painter->stroke_arc_path(!m_painter->stroke_arc_path());
          if (m_painter->stroke_arc_path())
            {
              std::cout << "Arc-path stroking\n";
            }
          else
            {
              std::cout << "Linear stroking\n";
            }
          break;

        case SDLK_v:
          m_painter->linearize_from_arc_path(!m_painter->linearize_from_arc_path());
          if (m_painter->linearize_from_arc_path())
            {
              std::cout << "Linearize from arc-path\n";
            }
          else
            {
              std::cout << "Linearize from original path\n";
            }
          break;

        case SDLK_k:
          cycle_value(m_selected_path, ev.key.keysym.mod & (KMOD_SHIFT|KMOD_CTRL|KMOD_ALT), m_paths.size());
          std::cout << "Path " << m_paths[m_selected_path].m_label << " selected\n";
          m_clip_window_path_dirty = true;
          break;

        case SDLK_a:
          m_with_aa = !m_with_aa;
          std::cout << "Anti-aliasing stroking and filling = " << m_with_aa << "\n";
          if (m_with_aa && m_surface->properties().msaa() > 1)
            {
              std::cout << "WARNING: doing shader based anti-aliasing with an MSAA render target is BAD\n";
            }
          break;

        case SDLK_5:
          m_draw_grid = !m_draw_grid;
          if (m_draw_grid)
            {
              std::cout << "Draw grid\n";
            }
          else
            {
              std::cout << "Don't draw grid\n";
            }
          break;

        case SDLK_q:
          shear() = shear2() = vec2(1.0f, 1.0f);
          break;

        case SDLK_p:
          m_stroke_width_in_pixels = !m_stroke_width_in_pixels;
          if (m_stroke_width_in_pixels)
            {
              std::cout << "Stroke width specified in pixels\n";
            }
          else
            {
              std::cout << "Stroke width specified in local coordinates\n";
            }
          break;

        case SDLK_o:
          clipping_window() = !clipping_window();
          std::cout << "Clipping window: " << on_off(clipping_window()) << "\n";
          break;

        case SDLK_w:
          repeat_window() = !repeat_window();
          std::cout << "Brush Repeat window: " << on_off(repeat_window()) << "\n";
          break;

        case SDLK_y:
          matrix_brush() = !matrix_brush();
          std::cout << "Matrix brush: " << on_off(matrix_brush()) << "\n";
          break;

        case SDLK_t:
          translate_brush() = !translate_brush();
          std::cout << "Translate brush: " << on_off(translate_brush()) << "\n";
          break;

        case SDLK_h:
          if (m_gradient_draw_mode != draw_no_gradient)
            {
              repeat_gradient() = !repeat_gradient();
              if (!repeat_gradient())
                {
                  std::cout << "non-";
                }
              std::cout << "repeat gradient mode\n";
            }
          break;

        case SDLK_m:
          if (is_miter_join_style(m_join_style))
            {
              m_have_miter_limit = !m_have_miter_limit;
              std::cout << "Miter limit ";
              if (!m_have_miter_limit)
                {
                  std::cout << "NOT ";
                }
              std::cout << "applied\n";
            }
          break;

        case SDLK_i:
          if (m_image && m_draw_fill)
            {
              if (ev.key.keysym.mod & (KMOD_CTRL|KMOD_ALT))
                {
                  m_apply_mipmapping = !m_apply_mipmapping;
                  std::cout << "Mipmapping ";
                  if (!m_apply_mipmapping)
                    {
                      std::cout << "NOT ";
                    }
                  std::cout << "applied.\n";
                }
              else
                {
                  cycle_value(m_image_filter, ev.key.keysym.mod & KMOD_SHIFT, number_image_filter_modes);
                  std::cout << "Image filter mode set to: " << m_image_filter_mode_labels[m_image_filter] << "\n";
                  if (m_image_filter == image_linear_filter && m_image->slack() < 1)
                    {
                      std::cout << "\tWarning: image slack = " << m_image->slack()
                                << " which insufficient to correctly apply linear filter (requires atleast 1)\n";
                    }
                  else if (m_image_filter == image_cubic_filter && m_image->slack() < 2)
                    {
                      std::cout << "\tWarning: image slack = " << m_image->slack()
                                << " which insufficient to correctly apply cubic filter (requires atleast 2)\n";
                    }
                }
            }
          break;

        case SDLK_s:
          if (m_draw_fill)
            {
              cycle_value(m_active_color_stop, ev.key.keysym.mod & (KMOD_SHIFT|KMOD_CTRL|KMOD_ALT), m_color_stops.size());
              std::cout << "Drawing color stop: " << m_color_stops[m_active_color_stop].first << "\n";
            }
          break;

        case SDLK_g:
          if (m_draw_fill)
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
          if (is_dashed_stroking())
            {
              unsigned int P;
              P = dash_pattern();
              std::cout << "Set to stroke dashed with pattern: {";
              for(unsigned int i = 0; i < m_dash_patterns[P].size(); ++i)
                {
                  if (i != 0)
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
          if (m_close_contour)
            {
              std::cout << "closed.\n";
            }
          else
            {
              std::cout << "open.\n";
            }
          break;

        case SDLK_r:
          if (m_draw_fill)
            {
              cycle_value(current_fill_rule(),
                          ev.key.keysym.mod & (KMOD_SHIFT|KMOD_CTRL|KMOD_ALT),
                          current_end_fill_rule() + 1);
              if (current_fill_rule() < PainterEnums::fill_rule_data_count)
                {
                  std::cout << "Fill rule set to: " << m_fill_labels[current_fill_rule()] << "\n";
                }
              else if (current_fill_rule() == current_end_fill_rule())
                {
                  std::cout << "Fill rule set to custom fill rule: all winding numbers filled\n";
                }
              else
                {
                  c_array<const int> wnd;
                  int value;
                  wnd = path().tessellation()->filled()->subset(0).winding_numbers();
                  value = wnd[current_fill_rule() - PainterEnums::fill_rule_data_count];
                  std::cout << "Fill rule set to custom fill rule: winding_number == "
                            << value << "\n";
                }
            }
          break;

        case SDLK_e:
          if (m_draw_fill)
            {
              m_fill_by_clipping = !m_fill_by_clipping;
              std::cout << "Set to ";
              if (m_fill_by_clipping)
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
          if (!m_draw_fill)
            {
              std::cout << "NOT ";
            }
          std::cout << "draw path fill\n";
          break;

        case SDLK_u:
          if (m_draw_fill & m_with_aa)
            {
              m_aa_fill_by_stroking = !m_aa_fill_by_stroking;
              std::cout << "Set to ";
              if (!m_aa_fill_by_stroking)
                {
                  std::cout << "NOT ";
                }
              std::cout << "AA fill by stroking\n";
            }
          break;

        case SDLK_SPACE:
          m_wire_frame = !m_wire_frame;
          std::cout << "Wire Frame = " << m_wire_frame << "\n";
          break;

        case SDLK_l:
          m_draw_stats = !m_draw_stats;
          break;

        case SDLK_z:
          if (ev.key.keysym.mod & (KMOD_SHIFT|KMOD_CTRL|KMOD_ALT))
            {
              m_curve_flatness *= 0.5f;
            }
          else
            {
              m_curve_flatness *= 2.0f;
            }
          std::cout << "Painter::curveFlatness set to " << m_curve_flatness << "\n";
          break;
        }
      break;
    };
}

void
painter_stroke_test::
construct_paths(int w, int h)
{
  for(const std::string &file : m_path_file_list)
    {
      std::ifstream path_file(file.c_str());
      if (path_file)
        {
          std::stringstream buffer;
          Path P;

          buffer << path_file.rdbuf();
          read_path(P, buffer.str());
          if (P.number_contours() > 0)
            {
              m_paths.push_back(PerPath(P, file, w, h, false));
            }
        }
    }

  for (uint32_t character_code : m_character_code_list)
    {
      uint32_t glyph_code;
      GlyphRender renderer(distance_field_glyph);
      Glyph g;

      glyph_code = m_font->glyph_code(character_code);
      g = m_glyph_cache->fetch_glyph(renderer, m_font, glyph_code);
      if (g.valid() && g.path().number_contours() > 0)
        {
          std::ostringstream str;
          str << "character code:" << character_code;
          m_paths.push_back(PerPath(g.path(), str.str(), w, h, true));
        }
    }

  if (m_paths.empty())
    {
      Path path;

      path << vec2(50.0f, 35.0f)
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
           << Path::contour_end()
           << vec2(300.0f, 300.0f)
           << Path::contour_end();
      m_paths.push_back(PerPath(path, "Default Path", w, h, false));
    }

  if (m_init_pan_zoom.value())
    {
      for (PerPath &P : m_paths)
        {
          ScaleTranslate<float> v;

          v.translation_x(m_initial_pan_x.value());
          v.translation_y(m_initial_pan_y.value());
          v.scale(m_initial_zoom.value());
          P.m_path_zoomer.transformation(v);
        }
    }
}

void
painter_stroke_test::
per_path_processing(void)
{
  m_miter_limit = 0.0f;
  for(const PerPath &P : m_paths)
    {
      reference_counted_ptr<const TessellatedPath> tess;
      const StrokedCapsJoins *stroked;
      const PainterAttributeData *data;
      c_array<const PainterAttribute> miter_points;

      tess = P.m_path.tessellation(-1.0f);
      stroked = &tess->stroked()->caps_joins();
      data = &stroked->miter_clip_joins();

      for(unsigned int J = 0, endJ = stroked->number_joins(true); J < endJ; ++J)
        {
          unsigned int chunk;

          chunk = stroked->join_chunk(J);
          miter_points = data->attribute_data_chunk(chunk);
          for(unsigned p = 0, endp = miter_points.size(); p < endp; ++p)
            {
              float v;
              StrokedPoint pt;

              StrokedPoint::unpack_point(&pt, miter_points[p]);
              v = pt.miter_distance();
              if (std::isfinite(v))
                {
                  m_miter_limit = fastuidraw::t_max(m_miter_limit, fastuidraw::t_abs(v));
                }
            }
        }

      if (m_print_path.value())
        {
          std::cout << "Path \"" << P.m_label << "\" tessellated:\n";
          for(unsigned int c = 0; c < tess->number_contours(); ++c)
            {
              std::cout << "\tContour #" << c << "\n";
              for(unsigned int e = 0; e < tess->number_edges(c); ++e)
                {
                  fastuidraw::c_array<const fastuidraw::TessellatedPath::segment> segs;

                  std::cout << "\t\tEdge #" << e << " has "
                            << tess->edge_segment_data(c, e).size() << " segments\n";
                  segs = tess->edge_segment_data(c, e);
                  for(unsigned int i = 0; i < segs.size(); ++i)
                    {
                      std::cout << "\t\t\tSegment #" << i << ":\n"
                                << "\t\t\t\tstart_p    = " << segs[i].m_start_pt << "\n"
                                << "\t\t\t\tend_p      = " << segs[i].m_end_pt << "\n"
                                << "\t\t\t\tlength     = " << segs[i].m_length << "\n"
                                << "\t\t\t\tedge_d     = " << segs[i].m_distance_from_edge_start << "\n"
                                << "\t\t\t\tcontour_d  = " << segs[i].m_distance_from_contour_start << "\n"
                                << "\t\t\t\tedge_l     = " << segs[i].m_edge_length << "\n"
                                << "\t\t\t\tcontour_l  = " << segs[i].m_open_contour_length << "\n"
                                << "\t\t\t\tcontour_cl = " << segs[i].m_closed_contour_length << "\n";
                    }
                }
            }
        }
    }
  m_miter_limit = fastuidraw::t_min(100.0f, m_miter_limit); //100 is an insane miter limit.
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
        iter = m_color_stop_args.values().begin(),
        end = m_color_stop_args.values().end();
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
      if (!tmp.empty())
        {
          m_dash_patterns.push_back(std::vector<PainterDashedStrokeParams::DashPatternElement>());
          std::swap(tmp, m_dash_patterns.back());
        }
      tmp.clear();
    }

  if (m_dash_patterns.empty())
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
draw_scene(bool drawing_wire_frame)
{
  if (!m_draw_line_pen)
    {
      PainterBrush br;
      br.pen(m_draw_line_red.value(), m_draw_line_green.value(),
             m_draw_line_blue.value(), m_draw_line_alpha.value());
      m_draw_line_pen = m_painter->packed_value_pool().create_packed_value(br);
    }

  if (m_paths[m_selected_path].m_from_glyph)
    {
      /* Glyphs have y-increasing upwards, rather than
       * downwards; so we reverse the y
       */
      float y;
      y = path().tessellation()->bounding_box_min().y()
        + path().tessellation()->bounding_box_max().y();
      m_painter->translate(vec2(0.0f, y));
      m_painter->shear(1.0f, -1.0f);
    }

  if (clipping_window() && !drawing_wire_frame)
    {
      if (m_clip_window_path_dirty)
        {
          Path clip_window_path;
          clip_window_path << clipping_xy()
                           << vec2(clipping_xy().x(), clipping_xy().y() + clipping_wh().y())
                           << clipping_xy() + clipping_wh()
                           << vec2(clipping_xy().x() + clipping_wh().x(), clipping_xy().y())
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
                             true, PainterEnums::flat_caps,
                             PainterEnums::miter_clip_joins,
                             false);
      m_painter->restore();
      m_painter->clipInRect(clipping_xy(), clipping_wh());
    }

  if (m_draw_fill)
    {
      PainterBrush fill_brush;

      fill_brush.pen(m_fill_red.value(), m_fill_green.value(),
                     m_fill_blue.value(), m_fill_alpha.value());
      if (translate_brush())
        {
          fill_brush.transformation_translate(zoomer().transformation().translation());
        }
      else
        {
          fill_brush.no_transformation_translation();
        }

      if (matrix_brush())
        {
          float2x2 m;
          m(0, 0) = m(1, 1) = zoomer().transformation().scale();
          fill_brush.transformation_matrix(m);
        }
      else
        {
          fill_brush.no_transformation_matrix();
        }

      if (repeat_window())
        {
          fill_brush.repeat_window(repeat_xy(), repeat_wh());
        }
      else
        {
          fill_brush.no_repeat_window();
        }

      if (m_gradient_draw_mode == draw_linear_gradient)
        {
          fill_brush.linear_gradient(m_color_stops[m_active_color_stop].second,
                                     gradient_p0(), gradient_p1(), repeat_gradient());
        }
      else if (m_gradient_draw_mode == draw_radial_gradient)
        {
          fill_brush.radial_gradient(m_color_stops[m_active_color_stop].second,
                                     gradient_p0(), gradient_r0(),
                                     gradient_p1(), gradient_r1(),
                                     repeat_gradient());
        }
      else
        {
          fill_brush.no_gradient();
        }


      if (!m_image || m_image_filter == no_image)
        {
          fill_brush.no_image();
        }
      else
        {
          enum PainterBrush::image_filter f;
          unsigned int mip_max_level;

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
              FASTUIDRAWassert(!"Incorrect value for m_image_filter!");
              f = PainterBrush::image_filter_nearest;
            }
          mip_max_level = (m_apply_mipmapping) ? m_image->number_mipmap_levels() : 0u;
          fill_brush.sub_image(m_image, m_image_offset, m_image_size, f,
                               mip_max_level);
        }

      CustomFillRuleFunction fill_rule_function(everything_filled);
      WindingValueFillRule value_fill_rule;
      CustomFillRuleBase *fill_rule(&fill_rule_function);

      if (current_fill_rule() < PainterEnums::fill_rule_data_count)
        {
          fill_rule_function = CustomFillRuleFunction(static_cast<PainterEnums::fill_rule_t>(current_fill_rule()));
        }
      else if (current_fill_rule() != current_end_fill_rule())
        {
          int value;
          c_array<const int> wnd;

          wnd = path().tessellation()->filled()->subset(0).winding_numbers();
          value = wnd[current_fill_rule() - PainterEnums::fill_rule_data_count];
          value_fill_rule = WindingValueFillRule(value);
          fill_rule = &value_fill_rule;
        }

      PainterData D;
      if (drawing_wire_frame)
        {
          D = PainterData(m_draw_line_pen);
        }
      else
        {
          D = PainterData(&fill_brush);
        }

      if (m_fill_by_clipping)
        {
          m_painter->save();
          m_painter->clipInPath(path(), *fill_rule);
          m_painter->transformation(float3x3());
          m_painter->draw_rect(D, vec2(-1.0f, -1.0f), vec2(2.0f, 2.0f));
          m_painter->restore();
        }
      else
        {
          m_painter->fill_path(D, path(), *fill_rule, m_with_aa && !m_aa_fill_by_stroking);
        }

      if (m_aa_fill_by_stroking && m_with_aa && !drawing_wire_frame)
        {
          PainterStrokeParams st;
          st
            .miter_limit(-1.0f)
            .width(2.0f)
            .stroking_units(PainterStrokeParams::pixel_stroking_units);

          m_painter->stroke_path(PainterData(&fill_brush, &st),
                                 path(), true,
                                 PainterEnums::flat_caps,
                                 PainterEnums::bevel_joins,
                                 true);
        }
    }

  if (!m_stroke_pen)
    {
      PainterBrush br;
      br.pen(m_stroke_red.value(), m_stroke_green.value(), m_stroke_blue.value(), m_stroke_alpha.value());
      m_stroke_pen = m_painter->packed_value_pool().create_packed_value(br);
    }

  if (m_stroke_width > 0.0f)
    {
      PainterPackedValue<PainterBrush> *stroke_pen;
      stroke_pen = (!drawing_wire_frame) ? &m_stroke_pen : &m_draw_line_pen;

      if (is_dashed_stroking())
        {
          PainterDashedStrokeParams st;
          if (m_have_miter_limit)
            {
              st.miter_limit(m_miter_limit);
            }
          else
            {
              st.miter_limit(-1.0f);
            }
          st.width(m_stroke_width);

          unsigned int D(dash_pattern());
          c_array<const PainterDashedStrokeParams::DashPatternElement> dash_ptr(&m_dash_patterns[D][0],
                                                                                m_dash_patterns[D].size());
          st.dash_pattern(dash_ptr);
          if (m_stroke_width_in_pixels)
            {
              st.stroking_units(PainterStrokeParams::pixel_stroking_units);
            }


          m_painter->stroke_dashed_path(PainterData(*stroke_pen, &st),
                                        path(), m_close_contour,
                                        static_cast<enum PainterEnums::cap_style>(m_cap_style),
                                        static_cast<enum PainterEnums::join_style>(m_join_style),
                                        m_with_aa);

        }
      else
        {
          PainterStrokeParams st;
          if (m_have_miter_limit)
            {
              st.miter_limit(m_miter_limit);
            }
          else
            {
              st.miter_limit(-1.0f);
            }
          st.width(m_stroke_width);
          if (m_stroke_width_in_pixels)
            {
              st.stroking_units(PainterStrokeParams::pixel_stroking_units);
            }

          m_painter->stroke_path(PainterData(*stroke_pen, &st),
                                 path(), m_close_contour,
                                 static_cast<enum PainterEnums::cap_style>(m_cap_style),
                                 static_cast<enum PainterEnums::join_style>(m_join_style),
                                 m_with_aa);

        }
    }

  if (m_draw_fill && m_gradient_draw_mode != draw_no_gradient && !drawing_wire_frame)
    {
      float r0, r1;
      vec2 p0, p1;
      float inv_scale;

      inv_scale = 1.0f / zoomer().transformation().scale();
      r0 = 15.0f * inv_scale;
      r1 = 30.0f * inv_scale;

      p0 = gradient_p0();
      p1 = gradient_p1();

      if (translate_brush())
        {
          p0 -= zoomer().transformation().translation();
          p1 -= zoomer().transformation().translation();
        }

      if (matrix_brush())
        {
          p0 *= inv_scale;
          p1 *= inv_scale;
        }

      if (!m_black_pen)
        {
          FASTUIDRAWassert(!m_white_pen);
          m_white_pen = m_painter->packed_value_pool().create_packed_value(PainterBrush().pen(1.0, 1.0, 1.0, 1.0));
          m_black_pen = m_painter->packed_value_pool().create_packed_value(PainterBrush().pen(0.0, 0.0, 0.0, 1.0));
        }

      draw_rect(p0, r1, PainterData(m_black_pen));
      draw_rect(p0, r0, PainterData(m_white_pen));

      draw_rect(p1, r1, PainterData(m_white_pen));
      draw_rect(p1, r0, PainterData(m_black_pen));
    }
}

void
painter_stroke_test::
draw_frame(void)
{
  ivec2 wh(dimensions());
  float us;
  PainterBackend::Surface::Viewport vwp;

  us = static_cast<float>(m_fps_timer.restart_us());

  update_cts_params();
  vwp.m_dimensions = wh;

  /* we must set the viewport of the surface
     OUTSIDE of Painter::begin()/Painter::end().
   */
  m_surface->viewport(vwp);
  m_painter->begin(m_surface);

  float3x3 proj(float_orthogonal_projection_params(0, wh.x(), wh.y(), 0));
  m_painter->transformation(proj);

  m_painter->curveFlatness(m_curve_flatness);
  m_painter->save();

  /* draw grid using painter. */
  if (m_draw_grid && m_stroke_width_in_pixels && m_stroke_width > 0.0f)
    {
      if (m_grid_path_dirty && m_stroke_width > 0.0f)
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

  /* apply zoomer() */
  m_painter->concat(zoomer().transformation().matrix3());

  /* apply shear */
  m_painter->shear(shear().x(), shear().y());

  /* apply rotation */
  m_painter->rotate(angle() * M_PI / 180.0f);

  /* apply shear2 */
  m_painter->shear(shear2().x(), shear2().y());

  /* draw the scene */
  draw_scene(false);
  #ifndef FASTUIDRAW_GL_USE_GLES
    {
      if (m_wire_frame)
        {
          m_painter->queue_action(FASTUIDRAWnew EnableWireFrameAction(true));
          draw_scene(true);
          m_painter->queue_action(FASTUIDRAWnew EnableWireFrameAction(false));
        }
    }
  #endif

  m_painter->restore();

  if (m_draw_stats)
    {
      std::ostringstream ostr;
      ivec2 mouse_position;

      SDL_GetMouseState(&mouse_position.x(), &mouse_position.y());
      ostr << "FPS = ";
      if (us > 0.0f)
        {
          ostr << 1000.0f * 1000.0f / us;
        }
      else
        {
          ostr << "NAN";
        }

      ostr << "\nms = " << us / 1000.0f
           << "\nDrawing Path: " << m_paths[m_selected_path].m_label
           << "\nAttribs: "
           << m_painter->query_stat(PainterPacker::num_attributes)
           << "\nIndices: "
           << m_painter->query_stat(PainterPacker::num_indices)
           << "\nGenericData: "
           << m_painter->query_stat(PainterPacker::num_generic_datas)
           << "\nPainter Z: " << m_painter->current_z()
           << "\nMouse position:"
           << item_coordinates(mouse_position)
           << "\ncurveFlatness: " << m_curve_flatness
           << "\nView:\n\tzoom = " << zoomer().transformation().scale()
           << "\n\ttranslation = " << zoomer().transformation().translation()
           << "\n";

      PainterBrush brush;
      brush.pen(0.0f, 1.0f, 1.0f, 1.0f);
      draw_text(ostr.str(), 32.0f, m_font, GlyphRender(curve_pair_glyph), PainterData(&brush));
    }

  m_painter->end();
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
  m_surface->blit_surface(GL_NEAREST);
}

void
painter_stroke_test::
derived_init(int w, int h)
{
  //put into unit of per ms.
  m_window_change_rate.value() /= 1000.0f;
  m_change_stroke_width_rate.value() /= 1000.0f;
  m_change_miter_limit_rate.value() /= 1000.0f;

  // generate font
  reference_counted_ptr<FreeTypeFace::GeneratorFile> gen;
  gen = FASTUIDRAWnew FreeTypeFace::GeneratorFile(m_font_file.value().c_str(), 0);
  m_font = FASTUIDRAWnew FontFreeType(gen, FontFreeType::RenderParams(), m_ft_lib);
  if (gen->check_creation() != routine_success)
    {
      std::cout << "\n-----------------------------------------------------"
                << "\nWarning: unable to create font from file \""
                << m_font_file.value() << "\"\n"
                << "-----------------------------------------------------\n";
    }

  construct_paths(w, h);
  per_path_processing();
  construct_color_stops();
  construct_dash_patterns();

  /* set shader anti-alias to false if doing msaa rendering */
  if (m_surface->properties().msaa() > 1)
    {
      m_with_aa = false;
    }

  if (!m_image_file.value().empty())
    {
      ImageLoader image_data(m_image_file.value());
      if (image_data.non_empty())
        {
          if (m_use_atlas.value())
            {
              m_image = Image::create(m_painter->image_atlas(),
                                      image_data.width(), image_data.height(),
                                      image_data, m_image_slack.value());
            }
          else
            {
              m_image = gl::ImageAtlasGL::TextureImage::create(image_data.width(),
                                                               image_data.height(),
                                                               image_data.num_mipmap_levels(),
                                                               image_data);
            }
        }
    }

  if (m_image)
    {
      if (m_sub_image_x.value() < 0 || m_sub_image_y.value() < 0
         || m_sub_image_w.value() < 0 || m_sub_image_h.value() < 0)
        {
          m_image_offset = uvec2(0, 0);
          m_image_size = uvec2(m_image->dimensions());
        }
      else
        {
          m_image_offset = uvec2(m_sub_image_x.value(), m_sub_image_y.value());
          m_image_size = uvec2(m_sub_image_w.value(), m_sub_image_h.value());
        }
    }

  m_curve_flatness = m_painter->curveFlatness();
  m_draw_timer.restart();
  m_fps_timer.restart();
}

int
main(int argc, char **argv)
{
  painter_stroke_test P;
  return P.main(argc, argv);
}
