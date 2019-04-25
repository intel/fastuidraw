#include <iostream>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <bitset>
#include <math.h>

#include <fastuidraw/util/util.hpp>
#include <fastuidraw/painter/attribute_data/stroked_point.hpp>
#include <fastuidraw/painter/attribute_data/arc_stroked_point.hpp>

#include "sdl_painter_demo.hpp"
#include "simple_time.hpp"
#include "PanZoomTracker.hpp"
#include "read_path.hpp"
#include "path_util.hpp"
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

class character_code_range:public command_line_argument
{
public:
  explicit
  character_code_range(command_line_list<uint32_t> *p):
    command_line_argument(*p->parent()),
    m_p(p)
  {
    m_desc = produce_formatted_detailed_description("add_path_character_codes first last",
                                                    "add a set of paths from an inclusive range of character codes");
  }

  virtual
  int
  check_arg(const std::vector<std::string> &args, int location)
  {
    if (static_cast<unsigned int>(location + 2) < args.size() && args[location] == "add_path_character_codes")
      {
        uint32_t first, last;

        readvalue_from_string(first, args[location + 1]);
        readvalue_from_string(last, args[location + 2]);
        for (; first != last + 1; ++first)
          {
            m_p->insert(first);
          }
        return 3;
      }
    return 0;
  }

  virtual
  void
  print_command_line_description(std::ostream &ostr) const
  {
    ostr << "[add_path_character_codes first last] ";
  }

  virtual
  void
  print_detailed_description(std::ostream &ostr) const
  {
    ostr << m_desc;
  }

private:
  std::string m_desc;
  command_line_list<uint32_t> *m_p;
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

#ifndef FASTUIDRAW_GL_USE_GLES

class EnableWireFrameAction:public PainterDrawBreakAction
{
public:
  explicit
  EnableWireFrameAction(bool b):
    m_lines(b)
  {}

  virtual
  fastuidraw::gpu_dirty_state
  execute(PainterBackend*) const
  {
    if (m_lines)
      {
        fastuidraw_glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        fastuidraw_glLineWidth(4.0);
      }
    else
      {
        fastuidraw_glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
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
  std::string m_path_string;
  std::vector<vec2> m_pts, m_ctl_pts, m_arc_center_pts;
  std::string m_label;
  bool m_from_glyph;
  unsigned int m_fill_rule;
  unsigned int m_end_fill_rule;
  vec2 m_shear, m_shear2;
  float m_angle;
  PanZoomTrackerSDLEvent m_path_zoomer;

  bool m_matrix_brush;

  vec2 m_gradient_p0, m_gradient_p1;
  float m_gradient_r0, m_gradient_r1;
  float m_sweep_repeat_factor;
  enum PainterBrush::spread_type_t m_gradient_spread_type;

  bool m_repeat_window;
  vec2 m_repeat_xy, m_repeat_wh;
  enum PainterBrush::spread_type_t m_repeat_window_spread_type_x;
  enum PainterBrush::spread_type_t m_repeat_window_spread_type_y;

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
      draw_sweep_gradient,

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

  enum fill_mode_t
    {
      draw_fill_path,
      draw_fill_path_occludes_stroking,
      dont_draw_fill_path,

      number_fill_modes,
    };

  enum fill_by_mode_t
    {
      fill_by_filled_path,
      fill_by_clipping,
      fill_by_shader_filled_path,

      fill_by_number_modes
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
  item_coordinates(ivec2 c)
  {
    return item_coordinates(vec2(c));
  }

  vec2
  item_coordinates(vec2 c);

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

  float&
  sweep_repeat_factor(void)
  {
    return m_paths[m_selected_path].m_sweep_repeat_factor;
  }

  enum PainterBrush::spread_type_t&
  gradient_spread_type(void)
  {
    return m_paths[m_selected_path].m_gradient_spread_type;
  }

  enum PainterBrush::spread_type_t&
  repeat_window_spread_type_x(void)
  {
    return m_paths[m_selected_path].m_repeat_window_spread_type_x;
  }

  enum PainterBrush::spread_type_t&
  repeat_window_spread_type_y(void)
  {
    return m_paths[m_selected_path].m_repeat_window_spread_type_y;
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
  fill_centered_rect(const vec2 &pt, float r, const PainterData &d);

  void
  draw_scene(bool drawing_wire_frame);

  command_line_argument_value<float> m_change_miter_limit_rate;
  command_line_argument_value<float> m_change_stroke_width_rate;
  command_line_argument_value<float> m_window_change_rate;
  command_line_argument_value<float> m_radial_gradient_change_rate;
  command_line_argument_value<float> m_sweep_factor_gradient_change_rate;
  command_line_list<std::string> m_path_file_list;
  DashPatternList m_dash_pattern_files;
  command_line_argument_value<bool> m_print_path;
  color_stop_arguments m_color_stop_args;
  command_line_argument_value<std::string> m_image_file;
  command_line_argument_value<bool> m_use_atlas;
  command_line_argument_value<int> m_sub_image_x, m_sub_image_y;
  command_line_argument_value<int> m_sub_image_w, m_sub_image_h;
  command_line_argument_value<std::string> m_font_file;
  command_line_list<uint32_t> m_character_code_list;
  character_code_range m_character_code_list_range_adder;
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
  vecN<std::string, number_image_filter_modes> m_image_filter_mode_labels;
  vecN<std::string, number_fill_modes> m_draw_fill_labels;
  vecN<std::string, PainterBrush::number_spread_types> m_spread_type_labels;
  vecN<std::string, fill_by_number_modes> m_fill_by_mode_labels;

  PainterData::brush_value m_black_pen;
  PainterData::brush_value m_white_pen;
  PainterData::brush_value m_stroke_pen;
  PainterData::brush_value m_draw_line_pen;
  PainterData::brush_value m_blue_pen;
  PainterData::brush_value m_red_pen;
  PainterData::brush_value m_green_pen;

  Path m_rect;

  unsigned int m_selected_path;
  enum Painter::join_style m_join_style;
  enum Painter::cap_style m_cap_style;

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

  float m_miter_limit, m_stroke_width;
  unsigned int m_draw_fill;
  bool m_aa_stroke_mode;
  enum Painter::stroking_method_t m_stroking_mode;
  bool m_aa_fill_mode;
  unsigned int m_active_color_stop;
  unsigned int m_gradient_draw_mode;
  unsigned int m_image_filter;
  bool m_apply_mipmapping;
  bool m_draw_stats;
  float m_curve_flatness;
  bool m_draw_path_pts;

  bool m_wire_frame;
  bool m_stroke_width_in_pixels;

  int m_fill_by_mode;
  bool m_draw_grid;

  simple_time m_draw_timer, m_fps_timer;
  Path m_grid_path;
  bool m_grid_path_dirty;

  Path m_clip_window_path;
  bool m_clip_window_path_dirty;

  float3x3 m_pixel_matrix;
  int m_show_surface, m_last_shown_surface;
};

///////////////////////////////////
// PerPath methods
PerPath::
PerPath(const Path &path, const std::string &label, int w, int h, bool from_gylph):
  m_path(path),
  m_label(label),
  m_from_glyph(from_gylph),
  m_fill_rule(Painter::odd_even_fill_rule),
  m_end_fill_rule(Painter::number_fill_rule),
  m_shear(1.0f, 1.0f),
  m_shear2(1.0f, 1.0f),
  m_angle(0.0f),
  m_matrix_brush(false),
  m_gradient_spread_type(PainterBrush::spread_repeat),
  m_repeat_window(false),
  m_repeat_window_spread_type_x(PainterBrush::spread_repeat),
  m_repeat_window_spread_type_y(PainterBrush::spread_repeat),
  m_clipping_window(false)
{
  m_end_fill_rule =
    m_path.tessellation()->filled()->subset(0).winding_numbers().size() + Painter::number_fill_rule;

  /* set transformation to center and contain path. */
  vec2 p0, p1, delta, dsp(w, h), ratio, mid;
  const Rect &R(m_path.tessellation()->bounding_box());
  float mm;

  p0 = R.m_min_point;
  p1 = R.m_max_point;

  if (m_from_glyph)
    {
      /* the path is rendered y-flipped, so we need to adjust
       * p0 and p1 correctly for it.
       */
      p0.y() *= -1.0f;
      p1.y() *= -1.0f;
      std::swap(p0.y(), p1.y());
    }

  delta = p1 - p0;
  ratio = delta / dsp;
  mm = t_max(0.00001f, t_max(ratio.x(), ratio.y()) );
  mid = 0.5 * (p1 + p0);

  ScaleTranslate<float> sc, tr1, tr2;

  tr1.translation(-mid);
  sc.scale(1.0f / mm);
  tr2.translation(dsp * 0.5f);

  m_path_zoomer.transformation(tr2 * sc * tr1);

  m_gradient_p0 = p0;
  m_gradient_p1 = p1;

  m_gradient_r0 = 0.0f;
  m_gradient_r1 = 200.0f / m_path_zoomer.transformation().scale();

  m_sweep_repeat_factor = 1.0f;

  m_repeat_xy = vec2(0.0f, 0.0f);
  m_repeat_wh = p1 - p0;

  m_clipping_xy = p0;
  m_clipping_wh = m_repeat_wh;

  extract_path_info(m_path, &m_pts, &m_ctl_pts, &m_arc_center_pts, &m_path_string);
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
  m_sweep_factor_gradient_change_rate(0.05f, "change_rate_brush_sweep_factor_gradient",
                                      "rate of change in units/sec when changing the sweep factor",
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
  m_character_code_list_range_adder(&m_character_code_list),
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
  m_join_style(Painter::rounded_joins),
  m_cap_style(Painter::square_caps),
  m_dash(0),
  m_miter_limit(5.0f),
  m_stroke_width(0.0f),
  m_draw_fill(draw_fill_path),
  m_aa_stroke_mode(true),
  m_stroking_mode(Painter::stroking_method_fastest),
  m_aa_fill_mode(true),
  m_active_color_stop(0),
  m_gradient_draw_mode(draw_no_gradient),
  m_image_filter(image_nearest_filter),
  m_apply_mipmapping(false),
  m_draw_stats(false),
  m_draw_path_pts(false),
  m_wire_frame(false),
  m_stroke_width_in_pixels(false),
  m_fill_by_mode(fill_by_filled_path),
  m_draw_grid(false),
  m_grid_path_dirty(true),
  m_clip_window_path_dirty(true),
  m_show_surface(0),
  m_last_shown_surface(0)
{
  std::cout << "Controls:\n"
            << "\tv: cycle through stroking modes\n"
            << "\tk: select next path\n"
            << "\ta: cycle through anti-aliased modes for stroking\n"
            << "\tu: cycle through anti-aliased modes for filling\n"
            << "\tj: cycle through join styles for stroking\n"
            << "\tc: cycle through cap style for stroking\n"
            << "\td: cycle through dash patterns\n"
            << "\t[: decrease stroke width(hold left-shift for slower rate and right shift for faster)\n"
            << "\t]: increase stroke width(hold left-shift for slower rate and right shift for faster)\n"
            << "\tp: toggle stroke width in pixels or local coordinates\n"
            << "\tctrl-p: toggle showing points (blue), control pts(blue) and arc-center(green) of Path\n"
            << "\tshift-p: print current path to console\n"
            << "\t5: toggle drawing grid\n"
            << "\tq: reset shear to 1.0\n"
            << "\t6: x-shear (hold ctrl to decrease, hold enter for shear2)\n"
            << "\t7: y-shear (hold ctrl to decrease, hold enter for shear2)\n"
            << "\t0: Rotate left\n"
            << "\t9: Rotate right\n"
            << "\tb: decrease miter limit(hold left-shift for slower rate and right shift for faster)\n"
            << "\tn: increase miter limit(hold left-shift for slower rate and right shift for faster)\n"
            << "\tm: toggle miter limit enforced\n"
            << "\tf: cycle drawing path filled (not filled, filled, filled and occludes stroking)\n"
            << "\tr: cycle through fill rules\n"
            << "\te: toggle fill by drawing clip rect\n"
            << "\ti: cycle through image filter to apply to fill (no image, nearest, linear, cubic)\n"
            << "\tctrl-i: toggle mipmap filtering when applying an image\n"
            << "\ts: cycle through defined color stops for gradient\n"
            << "\tg: cycle through gradient types (linear or radial)\n"
            << "\th: cycle though gradient spead types\n"
            << "\ty: toggle applying matrix brush so that brush appears to be in pixel coordinates\n"
            << "\to: toggle clipping window\n"
            << "\tctrl-o: cycle through buffers to show\n"
            << "\tz: increase/decrease curve flatness\n"
            << "\t4,6,2,8 (number pad): change location of clipping window\n"
            << "\tctrl-4,6,2,8 (number pad): change size of clipping window\n"
            << "\tw: toggle brush repeat window active\n"
            << "\tshift-w: cycle though y-repeat window spread modes\n"
            << "\tctrl-w: cycle though y-repeat window spread modes\n"
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
  m_gradient_mode_labels[draw_sweep_gradient] = "draw_sweep_gradient";

  m_spread_type_labels[PainterBrush::spread_clamp] = "spread_clamp";
  m_spread_type_labels[PainterBrush::spread_repeat] = "spread_repeat";
  m_spread_type_labels[PainterBrush::spread_mirror_repeat] = "spread_mirror_repeat";
  m_spread_type_labels[PainterBrush::spread_mirror] = "spread_mirror";

  m_fill_by_mode_labels[fill_by_filled_path] = "FilledPath";
  m_fill_by_mode_labels[fill_by_clipping] = "clipping against FilledPath";
  m_fill_by_mode_labels[fill_by_shader_filled_path] = "ShaderFilledPath";

  m_image_filter_mode_labels[no_image] = "no_image";
  m_image_filter_mode_labels[image_nearest_filter] = "image_nearest_filter";
  m_image_filter_mode_labels[image_linear_filter] = "image_linear_filter";
  m_image_filter_mode_labels[image_cubic_filter] = "image_cubic_filter";

  m_draw_fill_labels[draw_fill_path] = "draw_fill";
  m_draw_fill_labels[draw_fill_path_occludes_stroking] = "draw_fill_path_occludes_stroking";
  m_draw_fill_labels[dont_draw_fill_path] = "dont_draw_fill_path";

  m_rect << vec2(-0.5f, -0.5f)
         << vec2(-0.5f, +0.5f)
         << vec2(+0.5f, +0.5f)
         << vec2(+0.5f, -0.5f)
         << Path::contour_close();
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

  if (m_gradient_draw_mode == draw_sweep_gradient)
    {
      float delta;

      delta = m_sweep_factor_gradient_change_rate.value();
      if (keyboard_state[SDL_SCANCODE_1])
        {
          sweep_repeat_factor() -= delta;
        }
      if (keyboard_state[SDL_SCANCODE_2])
        {
          sweep_repeat_factor() += delta;
        }

      if (keyboard_state[SDL_SCANCODE_1] || keyboard_state[SDL_SCANCODE_2])
        {
          std::cout << "Sweep Repeat factor set to: "
                    << sweep_repeat_factor() << "\n";
        }
    }

  if (Painter::is_miter_join(m_join_style))
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
fill_centered_rect(const vec2 &pt, float r, const PainterData &draw)
{
  Rect rect;
  vec2 sz(0.5f * r);

  rect.m_min_point = pt - sz;
  rect.m_max_point = pt + sz;
  m_painter->fill_rect(draw, rect, m_aa_fill_mode);
}

vec2
painter_stroke_test::
brush_item_coordinate(ivec2 q)
{
  vec2 p;

  if (matrix_brush())
    {
      p = vec2(q);
    }
  else
    {
      p = item_coordinates(q);
    }
  return p;
}

vec2
painter_stroke_test::
item_coordinates(vec2 p)
{
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
  a = -angle() * FASTUIDRAW_PI / 180.0f;
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

        case SDLK_v:
          cycle_value(m_stroking_mode, ev.key.keysym.mod & (KMOD_SHIFT|KMOD_CTRL|KMOD_ALT),
                      Painter::number_stroking_methods);
          std::cout << "Stroking mode set to: " << Painter::label(m_stroking_mode) << "\n";
          break;

        case SDLK_k:
          cycle_value(m_selected_path, ev.key.keysym.mod & (KMOD_SHIFT|KMOD_CTRL|KMOD_ALT), m_paths.size());
          std::cout << "Path " << m_paths[m_selected_path].m_label << " selected\n";
          m_clip_window_path_dirty = true;
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
          if (ev.key.keysym.mod & KMOD_CTRL)
            {
              m_draw_path_pts = !m_draw_path_pts;
              if (m_draw_path_pts)
                {
                  std::cout << "Draw Path Points\n";
                }
              else
                {
                  std::cout << "Do not draw Path Points\n";
                }
            }
          else if(ev.key.keysym.mod & KMOD_SHIFT)
            {
              std::cout << m_paths[m_selected_path].m_path_string;
            }
          else
            {
              m_stroke_width_in_pixels = !m_stroke_width_in_pixels;
              if (m_stroke_width_in_pixels)
                {
                  std::cout << "Stroke width specified in pixels\n";
                }
              else
                {
                  std::cout << "Stroke width specified in local coordinates\n";
                }
            }
          break;

        case SDLK_o:
          if (ev.key.keysym.mod & KMOD_CTRL)
            {
              if (ev.key.keysym.mod & (KMOD_SHIFT | KMOD_ALT))
                {
                  if (m_show_surface > 0)
                    {
                      --m_show_surface;
                    }
                }
              else
                {
                  ++m_show_surface;
                }
            }
          else
            {
              clipping_window() = !clipping_window();
              std::cout << "Clipping window: " << on_off(clipping_window()) << "\n";
            }
          break;

        case SDLK_w:
          if (!(ev.key.keysym.mod & (KMOD_SHIFT | KMOD_CTRL)))
            {
              repeat_window() = !repeat_window();
              std::cout << "Brush Repeat window: " << on_off(repeat_window()) << "\n";
            }
          else if (repeat_window())
            {
              if (ev.key.keysym.mod & KMOD_SHIFT)
                {
                  cycle_value(repeat_window_spread_type_x(), false, PainterBrush::number_spread_types);
                  std::cout << "Brush Repeat window x-spread-type set to "
                            << m_spread_type_labels[repeat_window_spread_type_x()]
                            << "\n";
                }

              if (ev.key.keysym.mod & KMOD_CTRL)
                {
                  cycle_value(repeat_window_spread_type_y(), false, PainterBrush::number_spread_types);
                  std::cout << "Brush Repeat window y-spread-type set to "
                            << m_spread_type_labels[repeat_window_spread_type_y()]
                            << "\n";
                }
            }
          break;

        case SDLK_y:
          matrix_brush() = !matrix_brush();
          std::cout << "Make brush appear as-if in pixel coordinates: " << on_off(matrix_brush()) << "\n";
          break;

        case SDLK_h:
          if (m_gradient_draw_mode != draw_no_gradient)
            {
              cycle_value(gradient_spread_type(),
                          ev.key.keysym.mod & KMOD_SHIFT,
                          PainterBrush::number_spread_types);
              std::cout << "Gradient spread type set to : "
                        << m_spread_type_labels[gradient_spread_type()]
                        << "\n";
            }
          break;

        case SDLK_i:
          if (m_image && m_draw_fill != dont_draw_fill_path)
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
                }
            }
          break;

        case SDLK_s:
          if (m_draw_fill != dont_draw_fill_path)
            {
              cycle_value(m_active_color_stop, ev.key.keysym.mod & (KMOD_SHIFT|KMOD_CTRL|KMOD_ALT), m_color_stops.size());
              std::cout << "Drawing color stop: " << m_color_stops[m_active_color_stop].first << "\n";
            }
          break;

        case SDLK_g:
          if (m_draw_fill != dont_draw_fill_path)
            {
              cycle_value(m_gradient_draw_mode, ev.key.keysym.mod & (KMOD_SHIFT|KMOD_CTRL|KMOD_ALT), number_gradient_draw_modes);
              std::cout << "Gradient mode set to: " << m_gradient_mode_labels[m_gradient_draw_mode] << "\n";
            }
          break;

        case SDLK_j:
          cycle_value(m_join_style, ev.key.keysym.mod & (KMOD_SHIFT|KMOD_CTRL|KMOD_ALT), Painter::number_join_styles);
          std::cout << "Join drawing mode set to: " << Painter::label(m_join_style) << "\n";
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
          cycle_value(m_cap_style, ev.key.keysym.mod & (KMOD_SHIFT|KMOD_CTRL|KMOD_ALT), Painter::number_cap_styles);
          std::cout << "Cap drawing mode set to: " << Painter::label(m_cap_style) << "\n";
          break;

        case SDLK_r:
          if (m_draw_fill != dont_draw_fill_path)
            {
              cycle_value(current_fill_rule(),
                          ev.key.keysym.mod & (KMOD_SHIFT|KMOD_CTRL|KMOD_ALT),
                          current_end_fill_rule() + 1);
              if (current_fill_rule() < Painter::number_fill_rule)
                {
                  std::cout << "Fill rule set to: "
                            << Painter::label(static_cast<enum Painter::fill_rule_t>(current_fill_rule()))
                            << "\n";
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
                  value = wnd[current_fill_rule() - Painter::number_fill_rule];
                  std::cout << "Fill rule set to custom fill rule: winding_number == "
                            << value << "\n";
                }
            }
          break;

        case SDLK_e:
          if (m_draw_fill != dont_draw_fill_path)
            {
              cycle_value(m_fill_by_mode, ev.key.keysym.mod & (KMOD_SHIFT|KMOD_CTRL|KMOD_ALT), fill_by_number_modes);
              std::cout << "Set to fill by " << m_fill_by_mode_labels[m_fill_by_mode] << "\n";
            }
          break;

        case SDLK_f:
          cycle_value(m_draw_fill,
                      ev.key.keysym.mod & (KMOD_SHIFT|KMOD_CTRL|KMOD_ALT),
                      number_fill_modes);
          std::cout << "Draw Fill by " << m_draw_fill_labels[m_draw_fill] << "\n";
          break;

        case SDLK_u:
          if (m_draw_fill != dont_draw_fill_path)
            {
              m_aa_fill_mode = !m_aa_fill_mode;
              std::cout << "Filling anti-alias mode set to: "
                        << on_off(m_aa_fill_mode) << "\n";
            }
          break;

        case SDLK_a:
          if (m_stroke_width > 0.0f)
            {
              m_aa_stroke_mode = !m_aa_stroke_mode;
              std::cout << on_off(m_aa_stroke_mode) << "\n";
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
          std::cout << "Painter::curve_flatness set to " << m_curve_flatness << "\n";
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
      GlyphRenderer renderer(distance_field_glyph);
      Glyph g;

      glyph_code = m_font->glyph_code(character_code);
      g = m_painter->glyph_cache().fetch_glyph(renderer, m_font.get(), glyph_code);
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
           << Path::contour_close_arc_degrees(90.0f)
           << vec2(200.0f, 200.0f)
           << vec2(400.0f, 200.0f)
           << vec2(400.0f, 400.0f)
           << vec2(200.0f, 400.0f)
           << Path::contour_close()
           << vec2(-50.0f, 100.0f)
           << vec2(0.0f, 200.0f)
           << vec2(100.0f, 300.0f)
           << vec2(150.0f, 325.0f)
           << vec2(150.0f, 100.0f)
           << Path::contour_close()
           << vec2(300.0f, 300.0f);
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

      for(unsigned int J = 0, endJ = stroked->number_joins(); J < endJ; ++J)
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
                                << "\t\t\t\tcontour_l  = " << segs[i].m_contour_length << "\n";
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

  if (m_color_stops.empty())
    {
      ColorStopSequence S;
      reference_counted_ptr<ColorStopSequenceOnAtlas> h;

      S.add(ColorStop(u8vec4(0, 255, 0, 255), 0.0f));
      S.add(ColorStop(u8vec4(0, 255, 255, 255), 0.33f));
      S.add(ColorStop(u8vec4(255, 255, 0, 255), 0.66f));
      S.add(ColorStop(u8vec4(255, 0, 0, 255), 1.0f));
      h = FASTUIDRAWnew ColorStopSequenceOnAtlas(S, m_painter->colorstop_atlas(), 8);
      m_color_stops.push_back(named_color_stop("Default ColorStop Sequence", h));
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
  m_painter->save();
  if (!m_draw_line_pen.packed())
    {
      PainterBrush br;
      br.color(m_draw_line_red.value(), m_draw_line_green.value(),
             m_draw_line_blue.value(), m_draw_line_alpha.value());
      m_draw_line_pen = m_painter->packed_value_pool().create_packed_brush(br);
    }

  if (m_paths[m_selected_path].m_from_glyph)
    {
      /* Glyphs have y-increasing upwards, rather than
       * downwards; so we reverse the y
       */
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
                           << Path::contour_close();
          m_clip_window_path.swap(clip_window_path);
          m_clip_window_path_dirty = false;
        }

      PainterBrush white;
      white.color(1.0f, 1.0f, 1.0f, 1.0f);
      PainterStrokeParams st;
      st.miter_limit(-1.0f);
      st.width(4.0f);
      m_painter->save();
      m_painter->clip_out_path(m_clip_window_path, Painter::nonzero_fill_rule);
      m_painter->stroke_path(PainterData(&white, &st), m_clip_window_path,
                             StrokingStyle()
                             .join_style(Painter::miter_clip_joins),
                             false);
      m_painter->restore();
      m_painter->clip_in_rect(Rect()
                              .min_point(clipping_xy())
                              .size(clipping_wh()));
    }

  CustomFillRuleFunction fill_rule_function(everything_filled);
  WindingValueFillRule value_fill_rule;
  CustomFillRuleBase *fill_rule(&fill_rule_function);

  if (m_draw_fill != dont_draw_fill_path)
    {
      PainterBrush fill_brush;

      fill_brush.color(m_fill_red.value(), m_fill_green.value(),
                       m_fill_blue.value(), m_fill_alpha.value());

      if (matrix_brush())
        {
          /* We want the effect that the brush is in the pixel-coordinates
           * so we make the transformation the same transformation
           * that is applied to painter.
           */
          float m;
          m = zoomer().transformation().scale();

          fill_brush.no_transformation();
          fill_brush.apply_translate(zoomer().transformation().translation());
          fill_brush.apply_shear(m, m);
          fill_brush.apply_shear(shear().x(), shear().y());
          fill_brush.apply_rotate(angle() * FASTUIDRAW_PI / 180.0f);
          fill_brush.apply_shear(shear2().x(), shear2().y());
        }
      else
        {
          fill_brush.no_transformation_matrix();
        }

      if (repeat_window())
        {
          fill_brush.repeat_window(repeat_xy(), repeat_wh(),
                                   repeat_window_spread_type_x(),
                                   repeat_window_spread_type_y());
        }
      else
        {
          fill_brush.no_repeat_window();
        }

      if (m_gradient_draw_mode == draw_linear_gradient)
        {
          fill_brush.linear_gradient(m_color_stops[m_active_color_stop].second,
                                     gradient_p0(), gradient_p1(), gradient_spread_type());
        }
      else if (m_gradient_draw_mode == draw_radial_gradient)
        {
          fill_brush.radial_gradient(m_color_stops[m_active_color_stop].second,
                                     gradient_p0(), gradient_r0(),
                                     gradient_p1(), gradient_r1(),
                                     gradient_spread_type());
        }
      else if (m_gradient_draw_mode == draw_sweep_gradient)
        {
          vec2 d;
          d = gradient_p1() - gradient_p0();
          fill_brush.sweep_gradient(m_color_stops[m_active_color_stop].second,
                                    gradient_p0(), t_atan2(d.y(), d.x()),
                                    Painter::y_increases_downwards,
                                    Painter::clockwise, sweep_repeat_factor(),
                                    gradient_spread_type());
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
          enum PainterBrush::mipmap_t mf;

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
          mf = (m_apply_mipmapping) ?
            PainterBrush::apply_mipmapping:
            PainterBrush::dont_apply_mipmapping;
          fill_brush.sub_image(m_image, m_image_offset, m_image_size, f, mf);
        }

      if (current_fill_rule() < Painter::number_fill_rule)
        {
          fill_rule_function = CustomFillRuleFunction(static_cast<enum Painter::fill_rule_t>(current_fill_rule()));
        }
      else if (current_fill_rule() != current_end_fill_rule())
        {
          int value;
          c_array<const int> wnd;

          wnd = path().tessellation()->filled()->subset(0).winding_numbers();
          value = wnd[current_fill_rule() - Painter::number_fill_rule];
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

      if (m_fill_by_mode == fill_by_clipping)
        {
          Rect R;

          path().approximate_bounding_box(&R);
          m_painter->save();
          m_painter->clip_in_path(path(), *fill_rule);
          m_painter->fill_rect(D, R);
          m_painter->restore();
        }
      else if (m_fill_by_mode == fill_by_filled_path)
        {
          m_painter->fill_path(D, path(), *fill_rule, m_aa_fill_mode);
        }
      else
        {
          const ShaderFilledPath *sf;

          sf = path().shader_filled_path().get();
          if (sf && current_fill_rule() < Painter::number_fill_rule)
            {
              m_painter->fill_path(D, *sf, static_cast<enum Painter::fill_rule_t>(current_fill_rule()));
            }
        }
    }

  if (m_draw_path_pts)
    {
      float inv_scale, r;

      inv_scale = 1.0f / zoomer().transformation().scale();
      r = 15.0f * inv_scale;
      if (!m_blue_pen.packed())
        {
          FASTUIDRAWassert(!m_red_pen.packed());
          FASTUIDRAWassert(!m_green_pen.packed());
          m_blue_pen = m_painter->packed_value_pool().create_packed_brush(PainterBrush().color(0.0, 0.0, 1.0, 1.0));
          m_red_pen = m_painter->packed_value_pool().create_packed_brush(PainterBrush().color(1.0, 0.0, 0.0, 1.0));
          m_green_pen = m_painter->packed_value_pool().create_packed_brush(PainterBrush().color(0.0, 1.0, 0.0, 1.0));
        }

      for (const vec2 &pt : m_paths[m_selected_path].m_pts)
        {
          fill_centered_rect(pt, r, PainterData(m_blue_pen));
        }

      for (const vec2 &pt : m_paths[m_selected_path].m_ctl_pts)
        {
          fill_centered_rect(pt, r, PainterData(m_red_pen));
        }

      for (const vec2 &pt : m_paths[m_selected_path].m_arc_center_pts)
        {
          fill_centered_rect(pt, r, PainterData(m_green_pen));
        }
    }

  if (!m_stroke_pen.packed())
    {
      PainterBrush br;
      br.color(m_stroke_red.value(), m_stroke_green.value(), m_stroke_blue.value(), m_stroke_alpha.value());
      m_stroke_pen = m_painter->packed_value_pool().create_packed_brush(br);
    }

  if (m_stroke_width > 0.0f)
    {
      PainterData::brush_value *stroke_pen;
      stroke_pen = (!drawing_wire_frame) ? &m_stroke_pen : &m_draw_line_pen;

      if (m_draw_fill == draw_fill_path_occludes_stroking)
        {
          m_painter->save();
          m_painter->clip_out_path(path(), *fill_rule);
        }

      if (is_dashed_stroking())
        {
          PainterDashedStrokeParams st;

          st.miter_limit(m_miter_limit);
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
                                        path(),
                                        StrokingStyle()
                                        .join_style(m_join_style)
                                        .cap_style(m_cap_style),
                                        m_aa_stroke_mode, m_stroking_mode);

        }
      else
        {
          PainterStrokeParams st;

          st.miter_limit(m_miter_limit);
          st.width(m_stroke_width);
          if (m_stroke_width_in_pixels)
            {
              st.stroking_units(PainterStrokeParams::pixel_stroking_units);
            }

          m_painter->stroke_path(PainterData(*stroke_pen, &st),
                                 path(),
                                 StrokingStyle()
                                 .join_style(m_join_style)
                                 .cap_style(m_cap_style),
                                 m_aa_stroke_mode, m_stroking_mode);
        }

      if (m_draw_fill == draw_fill_path_occludes_stroking)
        {
          m_painter->restore();
        }
    }

  if (m_draw_fill != dont_draw_fill_path && m_gradient_draw_mode != draw_no_gradient && !drawing_wire_frame)
    {
      float r0, r1;
      vec2 p0, p1;

      r0 = 15.0f;
      r1 = 30.0f;

      p0 = gradient_p0();
      p1 = gradient_p1();

      if (matrix_brush())
        {
          /* p0, p1 are suppose to be in screen coordinates,
           * avoid the drama of unapplying coordinates and just
           * change coordinate system temporarily to pixel
           * coordinates
           */
          m_painter->save();
          m_painter->transformation(m_pixel_matrix);
        }
      else
        {
          float inv_scale;

          inv_scale = 1.0f / zoomer().transformation().scale();
          r0 *= inv_scale;
          r1 *= inv_scale;
        }

      if (!m_black_pen.packed())
        {
          FASTUIDRAWassert(!m_white_pen.packed());
          m_white_pen = m_painter->packed_value_pool().create_packed_brush(PainterBrush().color(1.0, 1.0, 1.0, 1.0));
          m_black_pen = m_painter->packed_value_pool().create_packed_brush(PainterBrush().color(0.0, 0.0, 0.0, 1.0));
        }

      fill_centered_rect(p0, r1, PainterData(m_black_pen));
      fill_centered_rect(p0, r0, PainterData(m_white_pen));

      fill_centered_rect(p1, r1, PainterData(m_white_pen));
      fill_centered_rect(p1, r0, PainterData(m_black_pen));

      if (matrix_brush())
        {
          m_painter->restore();
        }
    }
  m_painter->restore();
}

void
painter_stroke_test::
draw_frame(void)
{
  ivec2 wh(dimensions());
  float us;
  PainterSurface::Viewport vwp;

  us = static_cast<float>(m_fps_timer.restart_us());

  update_cts_params();
  vwp.m_dimensions = wh;

  /* we must set the viewport of the surface
     OUTSIDE of Painter::begin()/Painter::end().
   */
  m_surface->viewport(vwp);
  m_painter->begin(m_surface, Painter::y_increases_downwards);

  m_painter->curve_flatness(m_curve_flatness);
  m_painter->save();

  /* draw grid using painter. */
  if (m_draw_grid && m_stroke_width_in_pixels && m_stroke_width > 0.0f)
    {
      if (m_grid_path_dirty && m_stroke_width > 0.0f)
        {
          Path grid_path;
          for(float x = 0, endx = wh.x(); x < endx; x += m_stroke_width)
            {
              grid_path << Path::contour_start(x, 0.0f)
                        << vec2(x, wh.y());
            }
          for(float y = 0, endy = wh.y(); y < endy; y += m_stroke_width)
            {
              grid_path << Path::contour_start(0.0f, y)
                        << vec2(wh.x(), y);
            }
          m_grid_path_dirty = false;
          m_grid_path.swap(grid_path);
        }

      PainterStrokeParams st;
      st.miter_limit(-1.0f);
      st.width(2.0f);

      PainterBrush stroke_pen;
      stroke_pen.color(1.0f, 1.0f, 1.0f, 1.0f);

      m_painter->stroke_path(PainterData(&stroke_pen, &st),
                             m_grid_path,
                             StrokingStyle()
                             .cap_style(Painter::flat_caps)
                             .join_style(Painter::no_joins),
                             false);
    }

  m_pixel_matrix = m_painter->transformation();

  /* apply zoomer() */
  m_painter->concat(zoomer().transformation().matrix3());

  /* apply shear */
  m_painter->shear(shear().x(), shear().y());

  /* apply rotation */
  m_painter->rotate(angle() * FASTUIDRAW_PI / 180.0f);

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
      ostr << "\nFPS = ";
      if (us > 0.0f)
        {
          ostr << 1000.0f * 1000.0f / us;
        }
      else
        {
          ostr << "NAN";
        }

      ostr << "\nms = " << us / 1000.0f
           << "\nDrawing Path: " << m_paths[m_selected_path].m_label;

      if (m_stroke_width > 0.0f)
        {
          ostr << "\n\t[a]AA-Stroking mode:" << on_off(m_aa_stroke_mode)
               << "\n\t[v]Stroke by: " << m_stroking_mode
               << "\n\tStroke Width: " << m_stroke_width;
          if (m_stroke_width_in_pixels)
            {
              ostr << "([p]in pixels)";
            }
          else
            {
              ostr << "([p]in item units)";
            }
          if (is_dashed_stroking())
            {
              ostr << "([d]dashed)";
            }
          else
            {
              ostr << "([d]non-dashed)";
            }

          ostr << "\n\t[c]CapStyle: " << Painter::label(m_cap_style)
               << "\n\t[j]JoinStyle: " << Painter::label(m_join_style);
        }

      if (m_draw_fill != dont_draw_fill_path)
        {
          bool print_fill_stats(true);

          if (m_fill_by_mode == fill_by_shader_filled_path)
            {
              if (!path().shader_filled_path())
                {
                  print_fill_stats = false;
                  ostr << "\n\nUnable to fill by " << m_fill_by_mode_labels[m_fill_by_mode]
                       << "\nbecause Path does not have\nShaderFilledPath\n";
                }
              else if (current_fill_rule() >= Painter::number_fill_rule)
                {
                  print_fill_stats = false;
                  ostr << "\n\nUnable to fill by " << m_fill_by_mode_labels[m_fill_by_mode]
                       << "\nbecause ShaderFilledPath\nonly supports the standard fill modes\n";
                }
            }

          if (print_fill_stats)
            {
              if (m_fill_by_mode == fill_by_filled_path)
                {
                  ostr << "\n\t[u]AA-Filling mode: " << on_off(m_aa_fill_mode);
                }

              ostr << "\n\t[f]Fill Mode: " << m_draw_fill_labels[m_draw_fill]
                   << "(via " << m_fill_by_mode_labels[m_fill_by_mode] << ")"
                   << "\n\t[r]Fill Rule: ";
              if (current_fill_rule() < Painter::number_fill_rule)
                {
                  ostr << Painter::label(static_cast<enum Painter::fill_rule_t>(current_fill_rule()));
                }
              else if (current_fill_rule() == current_end_fill_rule())
                {
                  ostr << "Custom (All Windings Filled)";
                }
              else
                {
                  c_array<const int> wnd;
                  int value;
                  wnd = path().tessellation()->filled()->subset(0).winding_numbers();
                  value = wnd[current_fill_rule() - Painter::number_fill_rule];
                  ostr << "Custom (Winding == " << value << ")";
                }
            }
        }
      fastuidraw::c_array<const unsigned int> stats(painter_stats());
      for (unsigned int i = 0; i < stats.size(); ++i)
        {
          enum Painter::query_stats_t st;

          st = static_cast<enum Painter::query_stats_t>(i);
          ostr << "\n" << Painter::label(st) << ": " << stats[i];
        }
      ostr << "\nMouse position:"
           << item_coordinates(mouse_position)
           << "\ncurve_flatness: " << m_curve_flatness
           << "\nView:\n\tzoom = " << zoomer().transformation().scale()
           << "\n\ttranslation = " << zoomer().transformation().translation()
           << "\n";

      PainterBrush brush;
      brush.color(0.0f, 1.0f, 1.0f, 1.0f);
      draw_text(ostr.str(), 32.0f, m_font.get(), PainterData(&brush));
    }

  c_array<const PainterSurface* const> surfaces;

  surfaces = m_painter->end();
  fastuidraw_glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
  fastuidraw_glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

  m_show_surface = t_min(m_show_surface, (int)surfaces.size());
  if (m_show_surface <= 0 || m_show_surface > surfaces.size())
    {
      m_surface->blit_surface(GL_NEAREST);
    }
  else
    {
      const gl::PainterSurfaceGL *S;
      PainterSurface::Viewport src, dest;

      src = m_surface->viewport();
      S = dynamic_cast<const gl::PainterSurfaceGL*>(surfaces[m_show_surface - 1]);

      dest.m_origin = src.m_origin;
      dest.m_dimensions = ivec2(src.m_dimensions.x(), src.m_dimensions.y() / 2);
      m_surface->blit_surface(src, dest, GL_LINEAR);

      dest.m_origin.y() +=  dest.m_dimensions.y();
      S->blit_surface(src, dest, GL_LINEAR);
    }

  if (m_last_shown_surface != m_show_surface)
    {
      if (m_show_surface > 0)
        {
          std::cout << "Show offscreen surface: " << m_show_surface - 1 << "\n";
        }
      else
        {
          std::cout << "Don't show offscreen surface\n";
        }
      m_last_shown_surface = m_show_surface;
    }
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
  m_font = FASTUIDRAWnew FontFreeType(gen, m_ft_lib);
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

  if (!m_image_file.value().empty())
    {
      ImageLoader image_data(m_image_file.value());
      if (image_data.non_empty())
        {
          if (m_use_atlas.value())
            {
              m_image = m_painter->image_atlas().create(image_data.width(),
                                                        image_data.height(),
                                                        image_data,
                                                        Image::on_atlas);
            }
          else
            {
              m_image = m_painter->image_atlas().create_non_atlas(image_data.width(),
                                                                  image_data.height(),
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

  m_curve_flatness = m_painter->curve_flatness();
  m_draw_timer.restart();
  m_fps_timer.restart();
}

int
main(int argc, char **argv)
{
  painter_stroke_test P;
  return P.main(argc, argv);
}
