#pragma once

#include <utility>
#include <string>
#include <vector>

#include "SkSize.h"
#include "SkPoint.h"
#include "SkImage.h"
#include "SkPath.h"
#include "SkColor.h"
#include "SkTypeface.h"

#include "PainterWidget.hpp"
#include "cell_group.hpp"
#include "simple_time.hpp"
#include "PanZoomTracker.hpp"


typedef std::pair<sk_sp<SkImage>, std::string> named_image;
class CellSharedState;

class TableParams
{
public:
  SkSize m_wh;
  SkISize m_cell_count;
  SkScalar m_pixel_size;
  bool m_draw_image_name;
  int m_max_cell_group_size;
  int m_table_rotate_degrees_per_s;
  bool m_timer_based_animation;
  SkColor m_line_color;

  sk_sp<SkTypeface> m_font;
  std::vector<SkColor> m_text_colors;
  std::vector<SkColor> m_background_colors;
  std::vector<std::string> m_texts;
  std::vector<named_image> m_images;
  SkPoint m_min_speed, m_max_speed;
  SkScalar m_min_degrees_per_s, m_max_degrees_per_s;
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
  paint_pre_children(SkCanvas *painter);

  virtual
  void
  paint_post_children(SkCanvas *painter);

  virtual
  void
  pre_paint(void);

private:

  void
  generate_children_in_group(CellGroup *parent, int &J,
                             const SkIPoint &xy,
                             int count_x, int count_y);

  TableParams m_params;
  SkSize m_cell_sz;
  bool m_first_draw;
  SkPath m_grid_path, m_outline_path;

  simple_time m_time;
  int m_thousandths_degrees_rotation;
  float m_rotation_degrees;
};
