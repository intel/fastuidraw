#include <assert.h>
#include "color.hpp"
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
  m_params.m_cell_count.x() = std::max(1, m_params.m_cell_count.x());
  m_params.m_cell_count.y() = std::max(1, m_params.m_cell_count.y());

  if(m_params.m_text_colors.empty())
    {
      m_params.m_text_colors.push_back(color_t(1.0, 1.0, 1.0, 1.0));
    }

  if(m_params.m_background_colors.empty())
    {
      m_params.m_background_colors.push_back(color_t(1.0, 0.0, 0.0, 1.0));
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
                           const ivec2 &xy,
                           int count_x, int count_y)
{
  g->m_bb_min = vec2(xy) * m_cell_sz;
  g->m_bb_max = vec2(xy + ivec2(count_x, count_y)) * m_cell_sz;

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
                                     ivec2(xy.x() + cx1, xy.y()),
                                     cx2, cy1);
        }

      if(cy2 > 0)
        {
          p = new CellGroup(g);
          generate_children_in_group(p, J,
                                     ivec2(xy.x(), xy.y() + cy1),
                                     cx1, cy2);
        }

      if(cx2 > 0 && cy2 > 0)
        {
          p = new CellGroup(g);
          generate_children_in_group(p, J,
                                     ivec2(xy.x() + cx1, xy.y() + cy1),
                                     cx2, cy2);
        }
    }
  else
    {
      int x, y;
      vec2 pt;
      for(y = 0, pt.y() = double(xy.y()) * m_cell_sz.y(); y < count_y; ++y, pt.y() += m_cell_sz.y())
        {
          for(x = 0, pt.x() =double (xy.x()) * m_cell_sz.x(); x < count_x; ++x, pt.x() += m_cell_sz.x(), ++J)
            {
              int txtJ, bgJ, imJ;

              txtJ = J % m_params.m_text_colors.size();
              bgJ = J % m_params.m_background_colors.size();
              imJ = J % m_params.m_images.size();

              CellParams params;
              params.m_background_brush = m_params.m_background_colors[bgJ];
              params.m_image_brush = m_params.m_images[imJ].first;
              params.m_rect_brush = color_t(0.2, 0.7, 0.7, 0.6);
              params.m_text_brush = m_params.m_text_colors[txtJ];
              params.m_text_size = m_params.m_pixel_size;
              params.m_font = m_params.m_font;

              params.m_text = m_params.m_texts[J % m_params.m_texts.size()];
              params.m_pixels_per_ms = random_value(m_params.m_min_speed, m_params.m_max_speed) * (1.0 / 1000.0);
              params.m_degrees_per_s = (int)random_value(m_params.m_min_degrees_per_s, m_params.m_max_degrees_per_s);
              params.m_size = m_cell_sz;
              params.m_table_pos = ivec2(x, y) + xy;
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
              cairo_matrix_init_identity(&cell->m_parent_matrix_this);
              cairo_matrix_translate(&cell->m_parent_matrix_this, pt);
            }
        }
    }
}

void
Table::
paint_pre_children(cairo_t *)
{
  if(m_first_draw)
    {
      vec2 cell_loc;
      int J;

      m_cell_sz = m_dimensions / vec2(m_params.m_cell_count);
      m_params.m_cell_state->m_line_color = m_params.m_line_color;
      J = 0;
      generate_children_in_group(this, J, ivec2(0, 0),
                                 m_params.m_cell_count.x(), m_params.m_cell_count.y());

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

  m_rotation_degrees = static_cast<float>(m_thousandths_degrees_rotation) / (1000.0);
}

void
Table::
pre_paint()
{
  m_bb_min = m_params.m_zoomer->transformation().apply_inverse_to_point(m_bb_min);
  m_bb_max = m_params.m_zoomer->transformation().apply_inverse_to_point(m_bb_max);

  if(m_rotating)
    {
      cairo_matrix_init_identity(&m_parent_matrix_this);
      cairo_matrix_translate(&m_parent_matrix_this, m_dimensions * 0.5);
      cairo_matrix_rotate(&m_parent_matrix_this, m_rotation_degrees * M_PI / 180.0);
      cairo_matrix_translate(&m_parent_matrix_this, m_dimensions * -0.5);

      /*
        screen_pt = zoomer * parent_matrix_this * table_pt
        becomes:
        table_pt = inverse(parent_matrix_this) * inverse(zoomer) * screen_pt
       */
      cairo_matrix_t inverse(m_parent_matrix_this);
      vec2 ps[4];
      cairo_status_t invertible;

      invertible = cairo_matrix_invert(&inverse);
      (void)invertible;
      assert(invertible == CAIRO_STATUS_SUCCESS);

      ps[0] = vec2(m_bb_min.x(), m_bb_min.y());
      ps[1] = vec2(m_bb_min.x(), m_bb_max.y());
      ps[2] = vec2(m_bb_max.x(), m_bb_max.y());
      ps[3] = vec2(m_bb_max.x(), m_bb_min.y());
      for(int i = 0; i < 4; ++i)
        {
          ps[i] = inverse * ps[i];
        }

      m_bb_min.x() = std::min(std::min(ps[0].x(), ps[1].x()), std::min(ps[2].x(), ps[3].x()));
      m_bb_min.y() = std::min(std::min(ps[0].y(), ps[1].y()), std::min(ps[2].y(), ps[3].y()));

      m_bb_max.x() = std::max(std::max(ps[0].x(), ps[1].x()), std::max(ps[2].x(), ps[3].x()));
      m_bb_max.y() = std::max(std::max(ps[0].y(), ps[1].y()), std::max(ps[2].y(), ps[3].y()));
    }
  else
    {
      cairo_matrix_init_identity(&m_parent_matrix_this);
    }
  CellGroup::pre_paint();
}

void
Table::
paint_post_children(cairo_t *painter)
{
  if(!m_params.m_cell_state->m_rotating && m_params.m_cell_state->m_stroke_width > 0.0)
    {
      cairo_set_source_rgba(painter, m_params.m_cell_state->m_line_color);
      cairo_set_line_width(painter, m_params.m_cell_state->m_stroke_width);
      cairo_set_dash(painter, NULL, 0, 0.0);

      //stroke the outline with a rounded join.
      cairo_set_line_join(painter, CAIRO_LINE_JOIN_ROUND);
      cairo_rectangle(painter, 0.0, 0.0, m_params.m_wh.x(), m_params.m_wh.y());
      cairo_stroke(painter);

      //stroke grids.
      int x, y;
      vec2 cell_loc;

      cairo_set_line_cap(painter, CAIRO_LINE_CAP_BUTT);
      cairo_set_line_join(painter, CAIRO_LINE_JOIN_MITER);
      for(x = 1, cell_loc.x() = m_cell_sz.x(); x < m_params.m_cell_count.x(); ++x, cell_loc.x() += m_cell_sz.x())
        {
          cairo_move_to(painter, cell_loc.x(), 0.0);
          cairo_line_to(painter, cell_loc.x(), m_params.m_wh.y());
        }

      for(y = 1, cell_loc.y() = m_cell_sz.y(); y < m_params.m_cell_count.y(); ++y, cell_loc.y() += m_cell_sz.y())
        {
          cairo_move_to(painter, 0.0, cell_loc.y());
          cairo_line_to(painter, m_params.m_wh.x(), cell_loc.y());
        }
      cairo_stroke(painter);
    }
}
