#pragma once

#include "ostream_utility.hpp"
#include "PainterWidget.hpp"
#include "simple_time.hpp"

using namespace fastuidraw;

class CellSharedState:boost::noncopyable
{
public:
  CellSharedState(void):
    m_draw_text(true),
    m_draw_image(true),
    m_rotating(false),
    m_stroke_width(10.0f),
    m_pause(false),
    m_anti_alias_stroking(true),
    m_cells_drawn(0)
  {}

  bool m_draw_text;
  bool m_draw_image;
  bool m_rotating;
  Path m_path;
  float m_stroke_width;
  bool m_pause;
  bool m_anti_alias_stroking;

  int m_cells_drawn;
};

class CellParams
{
public:
  GlyphSelector::handle m_glyph_selector;
  FontBase::const_handle m_font;
  PainterState::PainterBrushState m_background_brush;
  PainterState::PainterBrushState m_image_brush;
  PainterState::PainterBrushState m_text_brush;
  PainterState::PainterBrushState m_line_brush;
  std::string m_text;
  std::string m_image_name;
  vec2 m_pixels_per_ms;
  int m_degrees_per_s;
  GlyphRender m_text_render;
  float m_pixel_size;
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
  paint_pre_children(const Painter::handle &painter);

private:

  void
  pre_paint(void);

  bool m_first_frame;
  simple_time m_time;
  int m_thousandths_degrees_rotation;

  vec2 m_table_pos;

  vec2 m_pixels_per_ms;
  int m_degrees_per_s;

  PainterState::PainterBrushState m_background_brush;
  PainterState::PainterBrushState m_image_brush;
  PainterState::PainterBrushState m_text_brush;
  PainterState::PainterBrushState m_line_brush;

  vec2 m_item_location;
  float m_item_rotation;
  PainterAttributeData m_text;
  CellSharedState *m_shared_state;
  bool m_timer_based_animation;
};
