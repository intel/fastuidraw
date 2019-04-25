#pragma once

#include <fastuidraw/text/font_database.hpp>
#include <fastuidraw/text/glyph_cache.hpp>
#include <fastuidraw/text/font.hpp>
#include <fastuidraw/painter/painter.hpp>
#include <fastuidraw/util/util.hpp>

#include "ostream_utility.hpp"
#include "PainterWidget.hpp"
#include "simple_time.hpp"

using namespace fastuidraw;

class CellSharedState:noncopyable
{
public:
  CellSharedState(void):
    m_draw_text(true),
    m_draw_image(true),
    m_rotating(false),
    m_stroke_width(10.0f),
    m_pause(false),
    m_anti_alias_stroking(true),
    m_cells_drawn(0),
    m_draw_transparent(false),
    m_rect_blend_mode(Painter::blend_porter_duff_src_over)
  {}

  bool m_draw_text;
  bool m_draw_image;
  bool m_rotating;
  Path m_path;
  float m_stroke_width;
  bool m_pause;
  bool m_draw_transparent;
  bool m_anti_alias_stroking;

  int m_cells_drawn;
  enum Painter::blend_mode_t m_rect_blend_mode;
  enum glyph_type m_glyph_render;
};

class CellParams
{
public:
  reference_counted_ptr<FontDatabase> m_font_database;
  reference_counted_ptr<GlyphCache> m_glyph_cache;
  reference_counted_ptr<const FontBase> m_font;
  PainterData::brush_value m_background_brush;
  PainterData::brush_value m_image_brush;
  PainterData::brush_value m_text_brush;
  PainterData::brush_value m_line_brush;
  const Image *m_image;
  ivec2 m_rect_dims;
  std::string m_text;
  std::string m_image_name;
  vec2 m_pixels_per_ms;
  int m_degrees_per_s;
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
  paint_pre_children(const reference_counted_ptr<Painter> &painter);

  virtual
  void
  pre_paint(void);

private:

  bool m_first_frame;
  simple_time m_time;
  int m_thousandths_degrees_rotation;
  int m_thousandths_degrees_cell_rotation;

  vec2 m_table_pos;
  vec2 m_rect_dims;

  vec2 m_pixels_per_ms;
  int m_degrees_per_s;

  PainterData::brush_value m_background_brush;
  PainterData::brush_value m_image_brush;
  PainterData::brush_value m_text_brush;
  PainterData::brush_value m_line_brush;

  vec2 m_item_location;
  float m_item_rotation;
  GlyphSequence m_text;
  CellSharedState *m_shared_state;
  bool m_timer_based_animation;
};
