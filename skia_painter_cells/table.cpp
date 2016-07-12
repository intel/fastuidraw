#include "table.hpp"
#include "cell.hpp"
#include "random.hpp"


Table::
Table(const TableParams &params):
  CellGroup(NULL),
  m_rotating(false),
  m_params(params),
  m_first_draw(true)
{
  m_dimensions = m_params.m_wh;
  m_clipped = false;
  m_params.m_cell_count.fWidth = std::max(1, m_params.m_cell_count.width());
  m_params.m_cell_count.fHeight = std::max(1, m_params.m_cell_count.height());

  if(m_params.m_text_colors.empty())
    {
      m_params.m_text_colors.push_back(SkColorSetARGB(0xFF, 0xFF, 0xFF, 0xFF));
    }

  if(m_params.m_background_colors.empty())
    {
      m_params.m_background_colors.push_back(SkColorSetARGB(0xFF, 0, 0, 0xFF));
    }

  if(m_params.m_texts.empty())
    {
      m_params.m_texts.push_back("Lonely Text");
    }

  if(m_params.m_images.empty())
    {
      m_params.m_images.push_back(named_image(NULL, "NULL"));
    }

}

void
Table::
generate_children_in_group(CellGroup *g, int &J,
                           const SkIPoint &xy,
                           int count_x, int count_y)
{
  g->m_bb_min = SkPoint::Make(float(xy.x()) * m_cell_sz.width(), float(xy.y()) * m_cell_sz.height());
  g->m_bb_max = SkPoint::Make(float(xy.x() + count_x) * m_cell_sz.width(), float(xy.y() + count_y) * m_cell_sz.height());

  if(count_x > m_params.m_max_cell_group_size || count_y > m_params.m_max_cell_group_size)
    {
      int cx1, cx2, cy1, cy2;

      if(count_x > m_params.m_max_cell_group_size)
        {
          cx1 = count_x / 2;
          cx2 = count_x - cx1;
        }
      else
        {
          cx1 = count_x;
          cx2 = 0;
        }

      if(count_y > m_params.m_max_cell_group_size)
        {
          cy1 = count_y / 2;
          cy2 = count_y - cy1;
        }
      else
        {
          cy1 = count_y;
          cy2 = 0;
        }

      CellGroup *p;
      p = new CellGroup(g);
      generate_children_in_group(p, J, xy, cx1, cy1);

      if(cx2 > 0)
        {
          p = new CellGroup(g);
          generate_children_in_group(p, J,
                                     SkIPoint::Make(xy.x() + cx1, xy.y()),
                                     cx2, cy1);
        }

      if(cy2 > 0)
        {
          p = new CellGroup(g);
          generate_children_in_group(p, J,
                                     SkIPoint::Make(xy.x(), xy.y() + cy1),
                                     cx1, cy2);
        }

      if(cx2 > 0 && cy2 > 0)
        {
          p = new CellGroup(g);
          generate_children_in_group(p, J,
                                     SkIPoint::Make(xy.x() + cx1, xy.y() + cy1),
                                     cx2, cy2);
        }
    }
  else
    {
      int x, y;
      SkPoint pt;
      for(y = 0, pt.fY = SkScalar(xy.y()) * m_cell_sz.height(); y < count_y; ++y, pt.fY += m_cell_sz.height())
        {
          for(x = 0, pt.fX = SkScalar(xy.x()) * m_cell_sz.width(); x < count_x; ++x, pt.fX += m_cell_sz.width(), ++J)
            {
              int txtJ, bgJ, imJ;

              txtJ = J % m_params.m_text_colors.size();
              bgJ = J % m_params.m_background_colors.size();
              imJ = J % m_params.m_images.size();

              CellParams params;
              params.m_background_brush.setColor(m_params.m_background_colors[bgJ]);
              params.m_image_brush = m_params.m_images[imJ].first;
              params.m_rect_brush.setColor(SkColorSetARGB(190, 50, 200, 200));
              params.m_text_brush.setColor(m_params.m_text_colors[txtJ]);
              params.m_text_brush.setTextSize(m_params.m_pixel_size);
              params.m_text_brush.setFlags(SkPaint::kAntiAlias_Flag | SkPaint::kSubpixelText_Flag);
              if(m_params.m_font != NULL)
                {
                  params.m_text_brush.setTypeface(m_params.m_font);
                }

              params.m_text = m_params.m_texts[J % m_params.m_texts.size()];
              params.m_pixels_per_ms = random_value(m_params.m_min_speed, m_params.m_max_speed) * (SkScalar(1) / SkScalar(1000));
              params.m_degrees_per_s = (int)random_value(m_params.m_min_degrees_per_s, m_params.m_max_degrees_per_s);
              params.m_size = m_cell_sz;
              params.m_table_pos = SkIPoint::Make(x, y) + xy;
              if(m_params.m_draw_image_name)
                {
                  params.m_image_name = m_params.m_images[imJ].second;
                }
              else
                {
                  params.m_image_name = "";
                }
              params.m_state = m_params.m_cell_state;
              params.m_timer_based_animation = m_params.m_timer_based_animation;
              Cell *cell;
              cell = new Cell(g, params);
              cell->m_parent_matrix_this = SkMatrix::MakeTrans(pt.x(), pt.y());
            }
        }
    }
}

void
Table::
paint_pre_children(SkCanvas *painter)
{
  if(m_first_draw)
    {
      SkPoint cell_loc;
      int x, y, J;

      m_cell_sz.fWidth = m_dimensions.width() / SkScalar(m_params.m_cell_count.width());
      m_cell_sz.fHeight = m_dimensions.height() / SkScalar(m_params.m_cell_count.height());

      m_params.m_cell_state->m_path.moveTo(SkScalar(0), SkScalar(0));
      m_params.m_cell_state->m_path.lineTo(m_cell_sz.width(), SkScalar(0));
      m_params.m_cell_state->m_path.lineTo(m_cell_sz.width(), m_cell_sz.height());
      m_params.m_cell_state->m_path.lineTo(SkScalar(0), m_cell_sz.height());
      m_params.m_cell_state->m_path.close();

      m_outline_path.moveTo(SkScalar(0), SkScalar(0));
      m_outline_path.lineTo(m_params.m_wh.width(), SkScalar(0));
      m_outline_path.lineTo(m_params.m_wh.width(), m_params.m_wh.height());
      m_outline_path.lineTo(SkScalar(0), m_params.m_wh.height());
      m_outline_path.close();

      for(x = 1, cell_loc.fX = m_cell_sz.width(); x < m_params.m_cell_count.width(); ++x, cell_loc.fX += m_cell_sz.width())
        {
          m_grid_path.moveTo(cell_loc.x(), 0.0);
          m_grid_path.lineTo(cell_loc.x(), m_params.m_wh.height());
        }

      for(y = 1, cell_loc.fY = m_cell_sz.height(); y < m_params.m_cell_count.height(); ++y, cell_loc.fY += m_cell_sz.height())
        {
          m_grid_path.moveTo(0.0, cell_loc.y());
          m_grid_path.lineTo(m_params.m_wh.width(), cell_loc.y());
        }

      m_params.m_cell_state->m_path_paint.setColor(m_params.m_line_color);
      J = 0;
      generate_children_in_group(this, J, SkIPoint::Make(0, 0),
                                 m_params.m_cell_count.width(), m_params.m_cell_count.height());

      m_first_draw = false;
      m_time.restart();
      m_thousandths_degrees_rotation = 0;
    }
  else
    {
      uint32_t ms;
      if(m_params.m_timer_based_animation)
        {
          ms = m_time.restart();
        }
      else
        {
          ms = 16;
        }
      if(m_params.m_cell_state->m_pause)
        {
          ms = 0;
        }

      m_thousandths_degrees_rotation += m_params.m_table_rotate_degrees_per_s * ms;
      if(m_thousandths_degrees_rotation >= 360 * 1000)
        {
          m_thousandths_degrees_rotation = m_thousandths_degrees_rotation % (360 * 1000);
        }

      if(!m_rotating)
        {
          m_thousandths_degrees_rotation = 0;
        }
    }

  m_rotation_degrees = static_cast<float>(m_thousandths_degrees_rotation) / (1000.0f);
}

void
Table::
pre_paint()
{
  m_bb_min = m_params.m_zoomer->transformation().apply_inverse_to_point(m_bb_min);
  m_bb_max = m_params.m_zoomer->transformation().apply_inverse_to_point(m_bb_max);

  if(m_rotating)
    {
      m_parent_matrix_this.reset();
      m_parent_matrix_this.preTranslate(m_dimensions.width() * SkScalar(0.5), m_dimensions.height() * SkScalar(0.5));
      m_parent_matrix_this.preRotate(m_rotation_degrees);
      m_parent_matrix_this.preTranslate(m_dimensions.width() * SkScalar(-0.5), m_dimensions.height() * SkScalar(-0.5));

      /*
        screen_pt = zoomer * parent_matrix_this * table_pt
        becomes:
        table_pt = inverse(parent_matrix_this) * inverse(zoomer) * screen_pt
       */
      SkMatrix inverse;
      SkVector ps[4];
      bool invertible;

      invertible = m_parent_matrix_this.invert(&inverse);
      (void)invertible;
      assert(invertible);

      ps[0].set(m_bb_min.x(), m_bb_min.y());
      ps[1].set(m_bb_min.x(), m_bb_max.y());
      ps[2].set(m_bb_max.x(), m_bb_max.y());
      ps[3].set(m_bb_max.x(), m_bb_min.y());
      inverse.mapPoints(ps, 4);

      m_bb_min.fX = std::min(std::min(ps[0].x(), ps[1].x()), std::min(ps[2].x(), ps[3].x()));
      m_bb_min.fY = std::min(std::min(ps[0].y(), ps[1].y()), std::min(ps[2].y(), ps[3].y()));

      m_bb_max.fX = std::max(std::max(ps[0].x(), ps[1].x()), std::max(ps[2].x(), ps[3].x()));
      m_bb_max.fY = std::max(std::max(ps[0].y(), ps[1].y()), std::max(ps[2].y(), ps[3].y()));
    }
  else
    {
      m_parent_matrix_this.reset();
    }
  CellGroup::pre_paint();
}

void
Table::
paint_post_children(SkCanvas *painter)
{
  if(!m_params.m_cell_state->m_rotating && m_params.m_cell_state->m_path_paint.getStrokeWidth() > SkScalar(0))
    {
      m_params.m_cell_state->m_path_paint.setStrokeJoin(SkPaint::kRound_Join);
      painter->drawPath(m_outline_path, m_params.m_cell_state->m_path_paint);
      m_params.m_cell_state->m_path_paint.setStrokeJoin(SkPaint::kMiter_Join);

      painter->drawPath(m_grid_path, m_params.m_cell_state->m_path_paint);
    }
}
