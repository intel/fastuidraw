#pragma once

#include <QPainterPath>
#include <QStaticText>
#include <QImage>
#include <QPainter>
#include <QTextLayout>
#include <QGlyphRun>
#include <QList>

#include "PainterWidget.hpp"
#include "simple_time.hpp"

class CellSharedState:boost::noncopyable
{
public:
  CellSharedState(void):
    m_draw_text(true),
    m_draw_image(true),
    m_rotating(false),
    m_use_glyph_run(true),
    m_stroke_width(10.0f),
    m_pause(false),
    m_anti_alias_stroking(true),
    m_cells_drawn(0)
  {}

  bool m_draw_text;
  bool m_draw_image;
  bool m_rotating;
  bool m_use_glyph_run;
  QPainterPath m_path;
  float m_stroke_width;
  QFont m_font;
  bool m_pause;
  bool m_anti_alias_stroking;

  int m_cells_drawn;
};

class CellParams
{
public:
  QColor m_background_brush;
  QImage m_image_brush;
  QColor m_text_brush;
  QColor m_line_brush;
  std::string m_text;
  std::string m_image_name;
  QPointF m_pixels_per_ms;
  int m_degrees_per_s;
  qreal m_pixel_size;
  QSizeF m_size;
  QPoint m_table_pos;
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
  paint_pre_children(QPainter *painter);

private:

  void
  pre_paint(void);

  void
  update(void);

  bool m_first_frame;
  simple_time m_time;
  int m_thousandths_degrees_rotation;

  QPointF m_table_pos;

  QPointF m_pixels_per_ms;
  int m_degrees_per_s;

  QColor m_background_brush;
  const QImage m_image_brush;
  QColor m_text_brush;
  QColor m_line_brush;

  QPointF m_item_location;
  qreal m_item_rotation;
  QString m_text_as_string;
  QList<QGlyphRun> m_text_as_glyph_run;
  CellSharedState *m_shared_state;
  bool m_timer_based_animation;
};
