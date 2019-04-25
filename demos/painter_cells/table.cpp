#include <algorithm>

#include "table.hpp"
#include "cell.hpp"
#include "random.hpp"


Table::
Table(const TableParams &params):
  CellGroup(nullptr),
  m_rotating(false),
  m_params(params),
  m_first_draw(true)
{
  m_dimensions = m_params.m_wh;
  m_clipped = false;
  m_params.m_cell_count.x() = std::max(1, m_params.m_cell_count.x());
  m_params.m_cell_count.y() = std::max(1, m_params.m_cell_count.y());

  if (m_params.m_text_colors.empty())
    {
      m_params.m_text_colors.push_back(vec4(1.0f, 1.0f, 1.0f, 1.0f));
    }

  if (m_params.m_background_colors.empty())
    {
      m_params.m_background_colors.push_back(vec4(0.0f, 0.0f, 1.0f, 1.0f));
    }

  if (m_params.m_texts.empty())
    {
      m_params.m_texts.push_back("Lonely Text");
    }

  if (m_params.m_images.empty())
    {
      m_params.m_images.push_back(named_image(reference_counted_ptr<const Image>(),
                                              "nullptr"));
    }

  if (m_params.m_rect_colors.empty())
    {
      m_params.m_rect_colors.push_back(vec4(1.0f, 1.0f, 1.0f, 1.0f));
    }

}

void
Table::
generate_children_in_group(const reference_counted_ptr<Painter> &painter,
                           CellGroup *g, int &J,
                           const ivec2 &xy,
                           int count_x, int count_y,
                           std::vector<PainterData::brush_value > &txt,
                           std::vector<PainterData::brush_value > &bg,
                           std::vector<PainterData::brush_value > &im)
{
  g->m_bb_min = (vec2(xy) ) * m_cell_sz;
  g->m_bb_max = (vec2(xy) + vec2(count_x, count_y) ) * m_cell_sz;

  if (count_x > m_params.m_max_cell_group_size || count_y > m_params.m_max_cell_group_size)
    {
      int cx1, cx2, cy1, cy2;

      if (count_x > m_params.m_max_cell_group_size)
        {
          cx1 = count_x / 2;
          cx2 = count_x - cx1;
        }
      else
        {
          cx1 = count_x;
          cx2 = 0;
        }

      if (count_y > m_params.m_max_cell_group_size)
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
      p = FASTUIDRAWnew CellGroup(g);
      generate_children_in_group(painter, p, J, xy, cx1, cy1, txt, bg, im);

      if (cx2 > 0)
        {
          p = FASTUIDRAWnew CellGroup(g);
          generate_children_in_group(painter, p, J, ivec2(xy.x() + cx1, xy.y()),
                                     cx2, cy1, txt, bg, im);
        }

      if (cy2 > 0)
        {
          p = FASTUIDRAWnew CellGroup(g);
          generate_children_in_group(painter, p, J, ivec2(xy.x(), xy.y() + cy1),
                                     cx1, cy2, txt, bg, im);
        }

      if (cx2 > 0 && cy2 > 0)
        {
          p = FASTUIDRAWnew CellGroup(g);
          generate_children_in_group(painter, p, J, ivec2(xy.x() + cx1, xy.y() + cy1),
                                     cx2, cy2, txt, bg, im);
        }
    }
  else
    {
      int x, y;
      vec2 pt;
      for(y = 0, pt.y() = float(xy.y()) * m_cell_sz.y(); y < count_y; ++y, pt.y() += m_cell_sz.y())
        {
          for(x = 0, pt.x() = float(xy.x()) * m_cell_sz.x(); x < count_x; ++x, pt.x() += m_cell_sz.x(), ++J)
            {
              int txtJ, bgJ, imJ;

              txtJ = J % txt.size();
              bgJ = J % bg.size();
              imJ = J % im.size();

              if (!txt[txtJ].packed())
                {
                  PainterBrush brush(m_params.m_text_colors[txtJ]);
                  txt[txtJ] = painter->packed_value_pool().create_packed_brush(brush);
                }

              if (!bg[bgJ].packed())
                {
                  PainterBrush brush(m_params.m_background_colors[bgJ]);
                  bg[bgJ] = painter->packed_value_pool().create_packed_brush(brush);
                }

              if (!im[imJ].packed())
                {
                  int image, color;

                  image = imJ % m_params.m_images.size();
                  color = imJ % m_params.m_rect_colors.size();

                  PainterBrush brush;
                  if (m_params.m_images[image].first)
                    {
                      brush.image(m_params.m_images[image].first,
                                  m_params.m_image_filter,
                                  m_params.m_image_mipmapping);
                    }

                  brush.color(m_params.m_rect_colors[color]);
                  im[imJ] = painter->packed_value_pool().create_packed_brush(brush);
                }

              CellParams params;
              params.m_font_database = m_params.m_font_database;
              params.m_glyph_cache = m_params.m_glyph_cache;
              params.m_font = m_params.m_font;
              params.m_background_brush = bg[bgJ];
              params.m_image_brush = im[imJ];
              params.m_image = m_params.m_images[imJ % m_params.m_images.size()].first.get();
              params.m_text_brush = txt[txtJ];
              params.m_text = m_params.m_texts[J % m_params.m_texts.size()];
              params.m_pixels_per_ms = random_value(m_params.m_min_speed, m_params.m_max_speed) / 1000.0f;
              params.m_degrees_per_s = (int)random_value(m_params.m_min_degrees_per_s, m_params.m_max_degrees_per_s);
              params.m_pixel_size = m_params.m_pixel_size;
              params.m_size = m_cell_sz;
              params.m_table_pos = ivec2(x, y) + xy;
              if (m_params.m_draw_image_name)
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
              cell = FASTUIDRAWnew Cell(g, params);
              cell->m_parent_matrix_this = float3x3(float2x2(), pt);
            }
        }
    }
}


void
Table::
paint_pre_children(const reference_counted_ptr<Painter> &painter)
{
  if (m_first_draw)
    {
      vec2 cell_loc;
      int x, y, J;
      std::vector<PainterData::brush_value > txt, bg, im;

      txt.resize(m_params.m_text_colors.size());
      bg.resize(m_params.m_background_colors.size());
      im.resize(m_params.m_images.size() * m_params.m_rect_colors.size());
      m_cell_sz = m_dimensions / vec2(m_params.m_cell_count);

      m_params.m_cell_state->m_path << vec2(0.0f, 0.0f)
                                    << vec2(m_cell_sz.x(), 0.0f)
                                    << vec2(m_cell_sz.x(), m_cell_sz.y())
                                    << vec2(0.0f, m_cell_sz.y())
                                    << Path::contour_close();


      m_grid_path << vec2(0.0f, 0.0f)
                  << vec2(m_params.m_wh.x(), 0.0f)
                  << vec2(m_params.m_wh.x(), m_params.m_wh.y())
                  << vec2(0.0f, m_params.m_wh.y())
                  << Path::contour_close();

      for(x = 1, cell_loc.x() = m_cell_sz.x(); x < m_params.m_cell_count.x(); ++x, cell_loc.x() += m_cell_sz.x())
        {
          m_grid_path << Path::contour_start(cell_loc.x(), 0.0f)
                      << vec2(cell_loc.x(), m_params.m_wh.y());

        }

      for(y = 1, cell_loc.y() = m_cell_sz.y(); y < m_params.m_cell_count.y(); ++y, cell_loc.y() += m_cell_sz.y())
        {
          m_grid_path << Path::contour_start(0.0f, cell_loc.y())
                      << vec2(m_params.m_wh.x(), cell_loc.y());
        }

      m_line_brush = painter->packed_value_pool().create_packed_brush(m_params.m_line_color);

      J = 0;
      generate_children_in_group(painter, this, J, ivec2(0, 0),
                                 m_params.m_cell_count.x(), m_params.m_cell_count.y(),
                                 txt, bg, im);
      m_first_draw = false;
      m_time.restart();
      m_thousandths_degrees_rotation = 0;
    }
  else
    {
      uint32_t ms;
      if (m_params.m_timer_based_animation)
        {
          ms = m_time.restart();
        }
      else
        {
          ms = 16;
        }

      if (m_params.m_cell_state->m_pause)
        {
          ms = 0;
        }

      m_thousandths_degrees_rotation += m_params.m_table_rotate_degrees_per_s * ms;
      if (m_thousandths_degrees_rotation >= 360 * 1000)
        {
          m_thousandths_degrees_rotation = m_thousandths_degrees_rotation % (360 * 1000);
        }

      if (!m_rotating)
        {
          m_thousandths_degrees_rotation = 0;
        }
    }

  m_rotation_radians =
    static_cast<float>(FASTUIDRAW_PI) * static_cast<float>(m_thousandths_degrees_rotation) / (1000.0f * 180.0f);
}

void
Table::
pre_paint()
{
  m_bb_min = m_params.m_zoomer->transformation().apply_inverse_to_point(m_bb_min);
  m_bb_max = m_params.m_zoomer->transformation().apply_inverse_to_point(m_bb_max);

  if (m_rotating)
    {
      m_parent_matrix_this.reset();
      m_parent_matrix_this.translate(m_dimensions * 0.5f);
      m_parent_matrix_this.rotate(m_rotation_radians);
      m_parent_matrix_this.translate(-m_dimensions * 0.5f);

      /*
        screen_pt = zoomer * parent_matrix_this * table_pt
        becomes:
        table_pt = inverse(parent_matrix_this) * inverse(zoomer) * screen_pt
       */

      float3x3 inverse;
      vec3 p0, p1, p2, p3;
      m_parent_matrix_this.inverse(inverse);

      p0 = inverse * vec3(m_bb_min.x(), m_bb_min.y(), 1.0f);
      p1 = inverse * vec3(m_bb_min.x(), m_bb_max.y(), 1.0f);
      p2 = inverse * vec3(m_bb_max.x(), m_bb_max.y(), 1.0f);
      p3 = inverse * vec3(m_bb_max.x(), m_bb_min.y(), 1.0f);

      for(int i = 0; i < 2; ++i)
        {
          m_bb_min[i] = std::min(std::min(p0[i], p1[i]), std::min(p2[i], p3[i]));
          m_bb_max[i] = std::max(std::max(p0[i], p1[i]), std::max(p2[i], p3[i]));
        }
    }
  else
    {
      m_parent_matrix_this.reset();
    }
  CellGroup::pre_paint();
}


void
Table::
paint_post_children(const reference_counted_ptr<Painter> &painter)
{
  if (!m_params.m_cell_state->m_rotating && m_params.m_cell_state->m_stroke_width > 0.0f)
    {
      PainterStrokeParams st;

      st.miter_limit(-1.0f);
      st.width(m_params.m_cell_state->m_stroke_width);

      painter->stroke_path(PainterData(m_line_brush, &st),
                           m_grid_path,
                           StrokingStyle()
                           .cap_style(Painter::flat_caps)
                           .join_style(Painter::rounded_joins),
                           m_params.m_cell_state->m_anti_alias_stroking);
    }
}
