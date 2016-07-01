#pragma once

#include <QPainterPath>
#include <QStaticText>
#include <QImage>
#include <QPainter>

#include "PainterWidget.hpp"
#include "cell_group.hpp"
#include "simple_time.hpp"
#include "PanZoomTracker.hpp"


typedef std::pair<QImage, std::string> named_image;
class CellSharedState;

class TableParams
{
public:
  QSizeF m_wh;
  QSize m_cell_count;
  float m_pixel_size;
  bool m_draw_image_name;
  int m_max_cell_group_size;
  int m_table_rotate_degrees_per_s;
  bool m_timer_based_animation;

  QColor m_line_color;
  std::vector<QColor> m_text_colors;
  std::vector<QColor> m_background_colors;
  std::vector<std::string> m_texts;
  std::vector<named_image> m_images;
  QPointF m_min_speed, m_max_speed;
  qreal m_min_degrees_per_s, m_max_degrees_per_s;
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
  paint_pre_children(QPainter *painter);

  virtual
  void
  paint_post_children(QPainter *painter);

  virtual
  void
  pre_paint(void);

private:
  void
  generate_children_in_group(QPainter *painter,
                             CellGroup *parent, int &J,
                             const QPoint &xy,
                             int count_x, int count_y,
                             std::vector<QColor> &txt,
                             std::vector<QColor> &bg,
                             std::vector<named_image> &im);
  TableParams m_params;
  QSizeF m_cell_sz;
  bool m_first_draw;
  QColor m_line_brush;
  QPainterPath m_horiz_grid_path, m_vert_grid_path, m_outline_path;

  simple_time m_time;
  int m_thousandths_degrees_rotation;
  qreal m_rotation_degrees;
};
