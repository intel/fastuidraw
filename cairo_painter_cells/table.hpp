#pragma once

#include <utility>
#include <string>
#include <vector>

#include <cairo.h>

#include "vec2.hpp"
#include "color.hpp"
#include "PainterWidget.hpp"
#include "cell_group.hpp"
#include "simple_time.hpp"
#include "PanZoomTracker.hpp"
#include "text.hpp"

typedef std::pair<cairo_surface_t*, std::string> named_image;
class CellSharedState;

class TableParams
{
public:
  vec2 m_wh;
  ivec2 m_cell_count;
  double m_pixel_size;
  bool m_draw_image_name;
  int m_max_cell_group_size;
  int m_table_rotate_degrees_per_s;
  bool m_timer_based_animation;
  color_t m_line_color;

  ft_cairo_font *m_font;
  std::vector<color_t> m_text_colors;
  std::vector<color_t> m_background_colors;
  std::vector<std::string> m_texts;
  std::vector<named_image> m_images;
  vec2 m_min_speed, m_max_speed;
  double m_min_degrees_per_s, m_max_degrees_per_s;
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
  paint_pre_children(cairo_t *painter);

  virtual
  void
  paint_post_children(cairo_t *painter);

  virtual
  void
  pre_paint(void);

private:

  void
  generate_children_in_group(CellGroup *parent, int &J,
                             const ivec2 &xy,
                             int count_x, int count_y);

  TableParams m_params;
  vec2 m_cell_sz;
  bool m_first_draw;

  simple_time m_time;
  int m_thousandths_degrees_rotation;
  float m_rotation_degrees;
};
