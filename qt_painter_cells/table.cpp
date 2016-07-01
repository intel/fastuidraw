#include "table.hpp"
#include "cell.hpp"
#include "random.hpp"


Table::
Table(const TableParams &params):
  CellGroup(NULL),
  m_params(params),
  m_first_draw(true)
{
  m_dimensions = m_params.m_wh;
  m_clipped = false;
  m_params.m_cell_count.rwidth() = std::max(1, m_params.m_cell_count.width());
  m_params.m_cell_count.rheight() = std::max(1, m_params.m_cell_count.height());

  if(m_params.m_text_colors.empty())
    {
      m_params.m_text_colors.push_back(QColor(255, 255, 255, 255));
    }

  if(m_params.m_background_colors.empty())
    {
      m_params.m_background_colors.push_back(QColor(0, 0, 255, 255));
    }

  if(m_params.m_texts.empty())
    {
      m_params.m_texts.push_back("Lonely Text");
    }

  if(m_params.m_images.empty())
    {
      m_params.m_images.push_back(named_image(QImage(), "NULL"));
    }

}

void
Table::
generate_children_in_group(QPainter *painter,
                           CellGroup *g, int &J,
                           const QPoint &xy,
                           int count_x, int count_y,
                           std::vector<QColor> &txt,
                           std::vector<QColor> &bg,
                           std::vector<named_image> &im)
{
  g->m_bb_min.rx() = static_cast<qreal>(xy.x()) * m_cell_sz.width();
  g->m_bb_min.ry() = static_cast<qreal>(xy.y()) * m_cell_sz.height();

  g->m_bb_max.rx() = static_cast<qreal>(xy.x() + count_x) * m_cell_sz.width();
  g->m_bb_max.ry() = static_cast<qreal>(xy.y() + count_y) * m_cell_sz.height();

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
      generate_children_in_group(painter, p, J, xy, cx1, cy1, txt, bg, im);

      if(cx2 > 0)
        {
          p = new CellGroup(g);
          generate_children_in_group(painter, p, J, QPoint(xy.x() + cx1, xy.y()),
                                     cx2, cy1, txt, bg, im);
        }

      if(cy2 > 0)
        {
          p = new CellGroup(g);
          generate_children_in_group(painter, p, J, QPoint(xy.x(), xy.y() + cy1),
                                     cx1, cy2, txt, bg, im);
        }

      if(cx2 > 0 && cy2 > 0)
        {
          p = new CellGroup(g);
          generate_children_in_group(painter, p, J, QPoint(xy.x() + cx1, xy.y() + cy1),
                                     cx2, cy2, txt, bg, im);
        }
    }
  else
    {
      int x, y;
      QPointF pt;
      for(y = 0, pt.ry() = float(xy.y()) * m_cell_sz.height(); y < count_y; ++y, pt.ry() += m_cell_sz.height())
        {
          for(x = 0, pt.rx() = float(xy.x()) * m_cell_sz.width(); x < count_x; ++x, pt.rx() += m_cell_sz.width(), ++J)
            {
              int txtJ, bgJ, imJ;

              txtJ = J % txt.size();
              bgJ = J % bg.size();
              imJ = J % im.size();

              CellParams params;
              params.m_background_brush = m_params.m_background_colors[bgJ];
              params.m_image_brush = m_params.m_images[imJ].first;
              params.m_text_brush = m_params.m_text_colors[txtJ];
              params.m_text = m_params.m_texts[J % m_params.m_texts.size()];
              params.m_pixels_per_ms.rx() = random_value(m_params.m_min_speed.x(), m_params.m_max_speed.x()) / 1000.0f;
              params.m_pixels_per_ms.ry() = random_value(m_params.m_min_speed.y(), m_params.m_max_speed.y()) / 1000.0f;
              params.m_degrees_per_s = (int)random_value(m_params.m_min_degrees_per_s, m_params.m_max_degrees_per_s);
              params.m_pixel_size = m_params.m_pixel_size;
              params.m_size = m_cell_sz;
              params.m_table_pos = QPoint(x, y) + xy;
              if(m_params.m_draw_image_name)
                {
                  params.m_image_name = m_params.m_images[imJ].second;
                }
              else
                {
                  params.m_image_name = "";
                }
              params.m_line_brush = m_line_brush;
              params.m_state = m_params.m_cell_state;
              params.m_timer_based_animation = m_params.m_timer_based_animation;

              Cell *cell;
              cell = new Cell(g, params);
              cell->m_parent_matrix_this.translate(pt.x(), pt.y());
            }
        }
    }
}

void
Table::
paint_pre_children(QPainter *painter)
{
  if(m_first_draw)
    {
      QPointF cell_loc;
      int x, y, J;

      m_cell_sz.rwidth() = m_dimensions.width() / qreal(m_params.m_cell_count.width());
      m_cell_sz.rheight() = m_dimensions.height() / qreal(m_params.m_cell_count.height());

      m_params.m_cell_state->m_path.moveTo(0.0, 0.0);
      m_params.m_cell_state->m_path.lineTo(m_cell_sz.width(), 0.0);
      m_params.m_cell_state->m_path.lineTo(m_cell_sz.width(), m_cell_sz.height());
      m_params.m_cell_state->m_path.lineTo(0.0, m_cell_sz.height());
      m_params.m_cell_state->m_path.closeSubpath();

      m_outline_path.moveTo(0.0, 0.0);
      m_outline_path.lineTo(m_params.m_wh.width(), 0.0);
      m_outline_path.lineTo(m_params.m_wh.width(), m_params.m_wh.height());
      m_outline_path.lineTo(0.0, m_params.m_wh.height());
      m_outline_path.closeSubpath();

      for(x = 1, cell_loc.rx() = m_cell_sz.width(); x < m_params.m_cell_count.width(); ++x, cell_loc.rx() += m_cell_sz.width())
        {
          m_vert_grid_path.moveTo(cell_loc.x(), 0.0);
          m_vert_grid_path.lineTo(cell_loc.x(), m_params.m_wh.height());
          m_vert_grid_path.closeSubpath();
        }

      for(y = 1, cell_loc.ry() = m_cell_sz.height(); y < m_params.m_cell_count.height(); ++y, cell_loc.ry() += m_cell_sz.height())
        {
          m_horiz_grid_path.moveTo(0.0, cell_loc.y());
          m_horiz_grid_path.lineTo(m_params.m_wh.width(), cell_loc.y());
          m_horiz_grid_path.closeSubpath();
        }

      m_line_brush = m_params.m_line_color;
      J = 0;
      generate_children_in_group(painter, this, J, QPoint(0, 0),
                                 m_params.m_cell_count.width(), m_params.m_cell_count.height(),
                                 m_params.m_text_colors, m_params.m_background_colors,
                                 m_params.m_images);
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

      m_thousandths_degrees_rotation += m_params.m_table_rotate_degrees_per_s * ms;
      if(m_thousandths_degrees_rotation >= 360 * 1000)
        {
          m_thousandths_degrees_rotation = m_thousandths_degrees_rotation % (360 * 1000);
        }
    }

  m_rotation_degrees = static_cast<qreal>(m_thousandths_degrees_rotation) / 1000.0;
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
      m_parent_matrix_this.translate(m_dimensions.width() * 0.5f, m_dimensions.height() * 0.5f);
      m_parent_matrix_this.rotate(m_rotation_degrees);
      m_parent_matrix_this.translate(-m_dimensions.width() * 0.5f, -m_dimensions.height() * 0.5f);

      /*
        screen_pt = zoomer * parent_matrix_this * table_pt
        becomes:
        table_pt = inverse(parent_matrix_this) * inverse(zoomer) * screen_pt
       */

      QTransform inverse;
      QPointF p0, p1, p2, p3;
      inverse = m_parent_matrix_this.inverted();

      p0 = inverse.map(QPointF(m_bb_min.x(), m_bb_min.y()));
      p1 = inverse.map(QPointF(m_bb_min.x(), m_bb_max.y()));
      p2 = inverse.map(QPointF(m_bb_max.x(), m_bb_max.y()));
      p3 = inverse.map(QPointF(m_bb_max.x(), m_bb_min.y()));

      m_bb_min.rx() = std::min(std::min(p0.x(), p1.x()), std::min(p2.x(), p3.x()));
      m_bb_max.rx() = std::max(std::max(p0.x(), p1.x()), std::max(p2.x(), p3.x()));

      m_bb_min.ry() = std::min(std::min(p0.y(), p1.y()), std::min(p2.y(), p3.y()));
      m_bb_max.ry() = std::max(std::max(p0.y(), p1.y()), std::max(p2.y(), p3.y()));
    }
  else
    {
      m_parent_matrix_this.reset();
    }
  CellGroup::pre_paint();
}


void
Table::
paint_post_children(QPainter *painter)
{
  if(!m_params.m_cell_state->m_rotating && m_params.m_cell_state->m_stroke_width > 0.0)
    {
      QPen pen(m_line_brush);

      pen.setWidthF(m_params.m_cell_state->m_stroke_width);
      pen.setStyle(Qt::SolidLine);
      pen.setJoinStyle(Qt::RoundJoin);
      pen.setCapStyle(Qt::FlatCap);
      painter->strokePath(m_horiz_grid_path, pen);
      painter->strokePath(m_vert_grid_path, pen);
      painter->strokePath(m_outline_path, pen);
    }
}
