#include <sstream>
#include "cell.hpp"

namespace
{
  void
  bounce_move(double &v, double &q, double pmax, double sc)
  {
    const double delta(q * sc);

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
  bounce_move(vec2 &v, vec2 &delta, const vec2 &pmax, double sc)
  {
    bounce_move(v.x(), delta.x(), pmax.x(), sc);
    bounce_move(v.y(), delta.y(), pmax.y(), sc);
  }
}

Cell::
Cell(PainterWidget *p, const CellParams &params):
  PainterWidget(p),
  m_first_frame(true),
  m_thousandths_degrees_rotation(0),
  m_pixels_per_ms(params.m_pixels_per_ms),
  m_degrees_per_s(params.m_degrees_per_s),
  m_background_brush(params.m_background_brush),
  m_image_brush(params.m_image_brush),
  m_rect_brush(params.m_rect_brush),
  m_text_brush(params.m_text_brush),
  m_text_size(params.m_text_size),
  m_font(params.m_font),
  m_item_location(params.m_size * 0.5),
  m_shared_state(params.m_state),
  m_timer_based_animation(params.m_timer_based_animation)
{
  if(m_font)
    {
      std::ostringstream ostr;
      double scale_factor = 1.0;
      ostr << "Cell (" << params.m_table_pos.x() << ", "
           << params.m_table_pos.y() << ")"
           << "\n" << params.m_text
           << "\n" << params.m_image_name;
      m_font->layout_glyphs(ostr.str(), scale_factor, m_glyph_run);
    }
  m_dimensions = params.m_size;
  m_table_pos = m_dimensions * vec2(params.m_table_pos);
}

void
Cell::
pre_paint(void)
{
  if(m_shared_state->m_pause)
    {
      m_time.restart();
      return;
    }

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
      m_thousandths_degrees_rotation += m_degrees_per_s * ms;
      bounce_move(m_item_location, m_pixels_per_ms,
                  m_dimensions, static_cast<double>(ms));

      if(m_thousandths_degrees_rotation >= 360 * 1000)
        {
          m_thousandths_degrees_rotation = m_thousandths_degrees_rotation % (360 * 1000);
        }
    }
  else
    {
      m_first_frame = false;
    }

  m_item_rotation = static_cast<double>(m_thousandths_degrees_rotation) / (1000.0f);
  m_item_rotation *= (M_PI / 180.0);

  if(m_shared_state->m_rotating)
    {
      cairo_matrix_init_identity(&m_parent_matrix_this);
      cairo_matrix_translate(&m_parent_matrix_this, m_table_pos);
      cairo_matrix_translate(&m_parent_matrix_this, m_dimensions * 0.5);
      cairo_matrix_rotate(&m_parent_matrix_this, m_item_rotation);
      cairo_matrix_translate(&m_parent_matrix_this, m_dimensions * -0.5);
    }
  else
    {
      cairo_matrix_init_identity(&m_parent_matrix_this);
      cairo_matrix_translate(&m_parent_matrix_this, m_table_pos);
    }
}

void
Cell::
paint_pre_children(cairo_t *painter)
{
  cairo_save(painter);

  //draw background
  cairo_set_source_rgba(painter, m_background_brush);
  cairo_rectangle(painter, 0.0, 0.0, m_dimensions.x(), m_dimensions.y());

  //set transformation to rotate
  cairo_translate(painter, m_item_location);
  cairo_rotate(painter, m_item_rotation);

  if(m_shared_state->m_draw_image)
    {
      if(m_image_brush)
        {
          double w, h;
          w = cairo_image_surface_get_width(m_image_brush);
          h = cairo_image_surface_get_width(m_image_brush);
          cairo_set_source_surface(painter, m_image_brush, 0.0, 0.0);
          cairo_rectangle(painter, -0.5 * w, -0.5 * h, w, h);
          cairo_fill(painter);
        }
      else
        {
          double w, h;
          w = m_dimensions.x() * 0.25;
          h = m_dimensions.y() * 0.25;
          cairo_set_source_rgba(painter, m_rect_brush);
          cairo_rectangle(painter, -0.5 * w, -0.5 * h, w, h);
          cairo_fill(painter);
        }
    }

  if(m_shared_state->m_draw_text && m_font)
    {
      cairo_set_font_face(painter, m_font->cairo_font());
      cairo_set_source_rgba(painter, m_text_brush);
      cairo_move_to(painter, 0.0, 0.0);
      cairo_show_glyphs(painter, &m_glyph_run[0], m_glyph_run.size());
    }

  cairo_restore(painter);

  if(m_shared_state->m_rotating && m_shared_state->m_stroke_width > 0.0)
    {
      cairo_set_source_rgba(painter, m_shared_state->m_line_color);
      cairo_set_line_width(painter, m_shared_state->m_stroke_width);
      cairo_set_line_join(painter, CAIRO_LINE_JOIN_MITER);
      cairo_set_dash(painter, NULL, 0, 0.0);
      cairo_rectangle(painter, 0.0, 0.0, m_dimensions.x(), m_dimensions.y());
      cairo_stroke(painter);
    }
  m_shared_state->m_cells_drawn++;
}
