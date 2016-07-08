#pragma once

#include <vector>
#include <string>
#include <utility>
#include <math.h>

#include "vec2.hpp"
#include "color.hpp"

#include "PainterWidget.hpp"
#include "simple_time.hpp"

class CellSharedState:boost::noncopyable
{
public:
  CellSharedState(void):
    m_draw_text(true),
    m_draw_image(true),
    m_rotating(false),
    m_pause(false),
    m_stroke_width(10.0),
    m_anti_alias_stroking(true)
  {}

  bool m_draw_text;
  bool m_draw_image;
  bool m_rotating;
  bool m_pause;
  int m_cells_drawn;
  double m_stroke_width;
  color_t m_line_color;
  bool m_anti_alias_stroking;
};

class CellParams
{
public:
  color_t m_background_brush;
  cairo_surface_t *m_image_brush;
  color_t m_rect_brush;
  color_t m_text_brush;
  double m_text_size;
  cairo_font_face_t *m_font;
  std::string m_text;
  std::string m_image_name;
  vec2 m_pixels_per_ms;
  int m_degrees_per_s;
  vec2 m_size;
  ivec2 m_table_pos;
  bool m_timer_based_animation;
  CellSharedState *m_state;
};

class Cell:public PainterWidget
{
public:
  Cell(PainterWidget *p, const CellParams &params);
  ~Cell() {}

protected:

  virtual
  void
  paint_pre_children(cairo_t *painter);

  virtual
  void
  pre_paint(void);

private:
  typedef std::pair<double, std::string> pos_text_t;

  bool m_first_frame;
  simple_time m_time;
  int m_thousandths_degrees_rotation;

  vec2 m_table_pos;

  vec2 m_pixels_per_ms;
  int m_degrees_per_s;

  color_t m_background_brush;
  cairo_surface_t *m_image_brush;
  color_t m_rect_brush;
  color_t m_text_brush;
  double m_text_size;
  cairo_font_face_t *m_font;

  vec2 m_item_location;
  double m_item_rotation;
  std::string m_text;
  CellSharedState *m_shared_state;
  bool m_timer_based_animation;
};
