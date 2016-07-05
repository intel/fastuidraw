#pragma once

#include <vector>
#include <string>
#include <utility>

#include "SkCanvas.h"
#include "SkPaint.h"
#include "SkPath.h"
#include "SkImage.h"

#include "PainterWidget.hpp"
#include "simple_time.hpp"


class CellSharedState:boost::noncopyable
{
public:
  CellSharedState(void):
    m_draw_text(true),
    m_draw_image(true),
    m_rotating(false),
    m_pause(false)
  {
    m_path_paint.setStyle(SkPaint::kStroke_Style);
    m_path_paint.setStrokeWidth(SkScalar(10)); //use getStrokeWidth to fetch value)
    m_path_paint.setStrokeJoin(SkPaint::kMiter_Join);
    m_path_paint.setStrokeCap(SkPaint::kButt_Cap);
    //miter limit?
    m_path_paint.setStrokeMiter(SkScalar(3));
  }

  bool m_draw_text;
  bool m_draw_image;
  bool m_rotating;
  SkPath m_path;
  SkPaint m_path_paint;
  bool m_pause;
  int m_cells_drawn;
};

class CellParams
{
public:
  SkPaint m_background_brush;
  SkImage *m_image_brush;
  SkPaint m_rect_brush;
  SkPaint m_text_brush;
  std::string m_text;
  std::string m_image_name;
  SkPoint m_pixels_per_ms;
  int m_degrees_per_s;
  SkSize m_size;
  SkIPoint m_table_pos;
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
  paint_pre_children(SkCanvas *painter);

  virtual
  void
  pre_paint(void);

private:
  typedef std::pair<SkScalar, std::string> pos_text_t;

  bool m_first_frame;
  simple_time m_time;
  int m_thousandths_degrees_rotation;

  SkPoint m_table_pos;

  SkPoint m_pixels_per_ms;
  int m_degrees_per_s;

  SkPaint m_background_brush;
  SkImage *m_image_brush;
  SkPaint m_rect_brush;
  SkPaint m_text_brush;

  SkPoint m_item_location;
  float m_item_rotation;
  std::vector<pos_text_t> m_text;
  CellSharedState *m_shared_state;
  bool m_timer_based_animation;
};
