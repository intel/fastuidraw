#include <sstream>
#include "cell.hpp"

namespace
{
  void
  bounce_move(float &v, float &q, float pmax, float sc)
  {
    const float delta(q * sc);

    v += delta;
    if(v < 0)
      {
        v = -v;
        q = -q;
      }
    else if(v > pmax)
      {
        v = pmax - (v - pmax);
        q = -q;
      }
  }

  void
  bounce_move(SkPoint &v, SkPoint &delta, SkSize pmax, float sc)
  {
    bounce_move(v.fX, delta.fX, pmax.width(), sc);
    bounce_move(v.fY, delta.fY, pmax.height(), sc);
  }
}

Cell::
Cell(PainterWidget *p, const CellParams &params):
  PainterWidget(p),
  m_first_frame(true),
  m_thousandths_degrees_rotation(0),
  m_thousandths_degrees_cell_rotation(0),
  m_pixels_per_ms(params.m_pixels_per_ms),
  m_degrees_per_s(params.m_degrees_per_s),
  m_background_brush(params.m_background_brush),
  m_image_brush(params.m_image_brush),
  m_rect_brush(params.m_rect_brush),
  m_text_brush(params.m_text_brush),
  m_item_location(SkPoint::Make(params.m_size.width() * SkScalar(0.5),
                                params.m_size.height() * SkScalar(0.5))),
  m_shared_state(params.m_state),
  m_timer_based_animation(params.m_timer_based_animation)
{
  std::ostringstream ostr;
  ostr << "Cell (" << params.m_table_pos.x() << ", "
       << params.m_table_pos.y() << ")"
       << "\n" << params.m_text
       << "\n" << params.m_image_name;

  std::istringstream istr(ostr.str());
  std::string line;
  SkScalar h(0);

  while(getline(istr, line))
    {
      m_text.push_back(pos_text_t(h, line));
      h += m_text_brush.getFontSpacing();
    }

  m_dimensions = params.m_size;
  m_table_pos.fX = m_dimensions.width() * SkScalar(params.m_table_pos.x());
  m_table_pos.fY = m_dimensions.height() * SkScalar(params.m_table_pos.y());
}

void
Cell::
pre_paint(void)
{
  if(!m_first_frame)
    {
      uint32_t ms;
      if(m_timer_based_animation)
        {
          ms = m_time.restart();
        }
      else
        {
          ms = 16;
        }

      if(m_shared_state->m_pause)
        {
          ms = 0;
        }

      m_thousandths_degrees_rotation += m_degrees_per_s * ms;
      bounce_move(m_item_location, m_pixels_per_ms,
                  m_dimensions, static_cast<float>(ms));

      if(m_thousandths_degrees_rotation >= 360 * 1000)
        {
          m_thousandths_degrees_rotation = m_thousandths_degrees_rotation % (360 * 1000);
        }

      if(m_shared_state->m_rotating)
        {
          m_thousandths_degrees_cell_rotation += m_degrees_per_s * ms;
          if(m_thousandths_degrees_rotation >= 360 * 1000)
            {
              m_thousandths_degrees_cell_rotation = m_thousandths_degrees_rotation % (360 * 1000);
            }
        }
      else
        {
          m_thousandths_degrees_cell_rotation = 0;
        }
    }
  else
    {
      m_first_frame = false;
    }

  m_item_rotation = static_cast<SkScalar>(m_thousandths_degrees_rotation) / SkScalar(1000);

  if(m_shared_state->m_rotating)
    {
      SkScalar r;

      r = static_cast<SkScalar>(m_thousandths_degrees_cell_rotation) / SkScalar(1000);
      m_parent_matrix_this.reset();
      m_parent_matrix_this.preTranslate(m_table_pos.x(), m_table_pos.y());
      m_parent_matrix_this.preTranslate(m_dimensions.width() * SkScalar(0.5), m_dimensions.height() * SkScalar(0.5));
      m_parent_matrix_this.preRotate(r);
      m_parent_matrix_this.preTranslate(-m_dimensions.width() * SkScalar(0.5), -m_dimensions.height() * SkScalar(0.5));
    }
  else
    {
      m_parent_matrix_this.reset();
      m_parent_matrix_this.preTranslate(m_table_pos.x(), m_table_pos.y());
    }
}

void
Cell::
paint_pre_children(SkCanvas *painter)
{
  painter->save();

  //draw background
  painter->drawRect(SkRect::MakeSize(m_dimensions), m_background_brush);

  //set transformation to rotate
  painter->translate(m_item_location.x(), m_item_location.y());
  painter->rotate(m_item_rotation);

  if(m_shared_state->m_draw_image)
    {
      if(m_image_brush)
        {
          SkScalar w, h;
          w = m_image_brush->width();
          h = m_image_brush->height();
          painter->drawImage(m_image_brush, w * SkScalar(-0.5), h * SkScalar(-0.5));
        }
      else
        {
          SkScalar w, h;
          w = m_dimensions.width() * SkScalar(0.25);
          h = m_dimensions.height() * SkScalar(0.25);
          painter->drawRect(SkRect::MakeXYWH(w * SkScalar(-0.5), h * SkScalar(-0.5),
                                             w, h),
                            m_rect_brush);
        }
    }

  if(m_shared_state->m_draw_text)
    {
      for(unsigned int i = 0, endi = m_text.size(); i < endi; ++i)
        {
          const char *txt;
          txt = m_text[i].second.c_str();
          painter->drawText(txt, strlen(txt), SkScalar(0), m_text[i].first, m_text_brush);
        }
    }

  painter->restore();

  if(m_shared_state->m_rotating && m_shared_state->m_path_paint.getStrokeWidth() > SkScalar(0))
    {
      painter->drawPath(m_shared_state->m_path, m_shared_state->m_path_paint);
    }
  m_shared_state->m_cells_drawn++;
}
