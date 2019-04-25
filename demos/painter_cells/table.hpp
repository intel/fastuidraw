#pragma once

#include <fastuidraw/text/font_database.hpp>
#include <fastuidraw/text/glyph_cache.hpp>
#include <fastuidraw/text/font.hpp>
#include <fastuidraw/painter/painter.hpp>

#include "ostream_utility.hpp"
#include "PainterWidget.hpp"
#include "cell_group.hpp"
#include "simple_time.hpp"
#include "PanZoomTracker.hpp"

using namespace fastuidraw;

typedef std::pair<reference_counted_ptr<const Image>, std::string> named_image;
class CellSharedState;

class TableParams
{
public:
  vec2 m_wh;
  ivec2 m_cell_count;
  reference_counted_ptr<FontDatabase> m_font_database;
  reference_counted_ptr<GlyphCache> m_glyph_cache;
  reference_counted_ptr<const FontBase> m_font;
  float m_pixel_size;
  bool m_draw_image_name;
  int m_max_cell_group_size;
  int m_table_rotate_degrees_per_s;
  bool m_timer_based_animation;

  vec4 m_line_color;
  std::vector<vec4> m_text_colors;
  std::vector<vec4> m_background_colors;
  std::vector<vec4> m_rect_colors;
  std::vector<std::string> m_texts;
  std::vector<named_image> m_images;
  enum PainterBrush::image_filter m_image_filter;
  enum PainterBrush::mipmap_t m_image_mipmapping;
  vec2 m_min_speed, m_max_speed;
  float m_min_degrees_per_s, m_max_degrees_per_s;
  CellSharedState *m_cell_state;
  const PanZoomTracker *m_zoomer;
};

class Table:public CellGroup
{
public:
  explicit
  Table(const TableParams &params);

  ~Table() {}

  bool m_rotating;

protected:

  virtual
  void
  paint_pre_children(const reference_counted_ptr<Painter> &painter);

  virtual
  void
  paint_post_children(const reference_counted_ptr<Painter> &painter);

  virtual
  void
  pre_paint(void);

private:
  void
  generate_children_in_group(const reference_counted_ptr<Painter> &painter,
                             CellGroup *parent, int &J,
                             const ivec2 &xy,
                             int count_x, int count_y,
                             std::vector<PainterData::brush_value > &txt,
                             std::vector<PainterData::brush_value > &bg,
                             std::vector<PainterData::brush_value > &im);

  TableParams m_params;
  vec2 m_cell_sz;
  bool m_first_draw;
  PainterAttributeData m_lines;
  PainterData::brush_value m_line_brush;
  Path m_grid_path;

  simple_time m_time;
  int m_thousandths_degrees_rotation;
  float m_rotation_radians;
};
