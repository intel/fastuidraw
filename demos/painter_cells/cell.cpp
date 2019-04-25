#include <sstream>
#include "cell.hpp"
#include "text_helper.hpp"

namespace
{
  void
  bounce_move(float &v, float &q, float pmax, float sc)
  {
    const float delta(q * sc);

    v += delta;
    if (v < 0)
      {
        v = -v;
        q = -q;
      }
    else if (v > pmax)
      {
        v = pmax - (v - pmax);
        q = -q;
      }
  }

  void
  bounce_move(vec2 &v, vec2 &delta, vec2 pmax, float sc)
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
  m_thousandths_degrees_cell_rotation(0),
  m_pixels_per_ms(params.m_pixels_per_ms),
  m_degrees_per_s(params.m_degrees_per_s),
  m_background_brush(params.m_background_brush),
  m_image_brush(params.m_image_brush),
  m_text_brush(params.m_text_brush),
  m_line_brush(params.m_line_brush),
  m_item_location(params.m_size * 0.5f),
  m_text(params.m_pixel_size,
         fastuidraw::Painter::y_increases_downwards,
         *params.m_glyph_cache),
  m_shared_state(params.m_state),
  m_timer_based_animation(params.m_timer_based_animation)
{
  std::ostringstream ostr;
  ostr << "Cell" << params.m_table_pos
       << "\n" << params.m_text
       << "\n" << params.m_image_name;

  std::istringstream str(ostr.str());
  create_formatted_text(m_text, str, params.m_font.get(),
                        params.m_font_database);

  m_dimensions = params.m_size;
  m_table_pos = m_dimensions * vec2(params.m_table_pos);

  if (params.m_image)
    {
      m_rect_dims = vec2(params.m_image->dimensions());
    }
  else
    {
      m_rect_dims = vec2(m_dimensions) * 0.25f;
    }
}

void
Cell::
pre_paint(void)
{
  if (!m_first_frame)
    {
      uint32_t ms;
      if (m_timer_based_animation)
        {
          ms = m_time.restart();
        }
      else
        {
          ms = 16;
        }

      if (m_shared_state->m_pause)
        {
          ms = 0;
        }

      m_thousandths_degrees_rotation += m_degrees_per_s * ms;
      bounce_move(m_item_location, m_pixels_per_ms,
                  m_dimensions, static_cast<float>(ms));

      if (m_thousandths_degrees_rotation >= 360 * 1000)
        {
          m_thousandths_degrees_rotation = m_thousandths_degrees_rotation % (360 * 1000);
        }

      if (m_shared_state->m_rotating)
        {
          m_thousandths_degrees_cell_rotation += m_degrees_per_s * ms;
          if (m_thousandths_degrees_rotation >= 360 * 1000)
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

  m_item_rotation =
    static_cast<float>(FASTUIDRAW_PI) * static_cast<float>(m_thousandths_degrees_rotation) / (1000.0f * 180.0f);

  if (m_shared_state->m_rotating)
    {
      float r;

      r = static_cast<float>(FASTUIDRAW_PI) * static_cast<float>(m_thousandths_degrees_cell_rotation) / (1000.0f * 180.0f);
      m_parent_matrix_this.reset();
      m_parent_matrix_this.translate(m_dimensions * 0.5f + m_table_pos);
      m_parent_matrix_this.rotate(r);
      m_parent_matrix_this.translate(-m_dimensions * 0.5f);
    }
  else
    {
      m_parent_matrix_this = float3x3(float2x2(), m_table_pos);
    }
  m_draw_transparent = m_shared_state->m_draw_transparent;
}

void
Cell::
paint_pre_children(const reference_counted_ptr<Painter> &painter)
{
  painter->save();
  painter->fill_rect(PainterData(m_background_brush),
                     Rect().size(m_dimensions),
                     false);

  painter->translate(m_item_location);
  painter->rotate(m_item_rotation);

  if (m_shared_state->m_draw_image)
    {
      vec2 wh;
      wh = m_rect_dims;
      painter->save();
      painter->translate(-wh * 0.5f);
      painter->blend_shader(m_shared_state->m_rect_blend_mode);
      painter->fill_rect(PainterData(m_image_brush),
                         Rect().size(wh),
                         false);
      painter->restore();
    }

  if (m_shared_state->m_draw_text)
    {
      painter->draw_glyphs(PainterData(m_text_brush), m_text,
                           GlyphRenderer(m_shared_state->m_glyph_render));
    }

  painter->restore();

  if (m_shared_state->m_rotating && m_shared_state->m_stroke_width > 0.0f)
    {
      PainterStrokeParams st;

      st.miter_limit(-1.0f);
      st.width(m_shared_state->m_stroke_width);

      painter->stroke_path(PainterData(m_line_brush, &st),
                           m_shared_state->m_path,
                           StrokingStyle()
                           .join_style(Painter::miter_clip_joins),
                           m_shared_state->m_anti_alias_stroking);
    }
  m_shared_state->m_cells_drawn++;
}
