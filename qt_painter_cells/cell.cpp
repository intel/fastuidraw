#include <sstream>
#include "cell.hpp"


namespace
{
  void
  bounce_move(qreal &v, qreal &q, qreal pmax, qreal sc)
  {
    const qreal delta(q * sc);

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
  bounce_move(QPointF &v, QPointF &delta, QSizeF pmax, qreal sc)
  {
    bounce_move(v.rx(), delta.rx(), pmax.width(), sc);
    bounce_move(v.ry(), delta.ry(), pmax.height(), sc);
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
  m_text_brush(params.m_text_brush),
  m_line_brush(params.m_line_brush),
  m_item_location(params.m_size.width() * 0.5, params.m_size.height() * 0.5),
  m_shared_state(params.m_state),
  m_timer_based_animation(params.m_timer_based_animation)
{
  std::ostringstream ostr;
  ostr << "Cell(" << params.m_table_pos.x() << ", "
       << params.m_table_pos.y() << ")\n"
       << params.m_text
       << "\n" << params.m_image_name;

  m_text_as_string = QString(ostr.str().c_str());

  std::istringstream istr(ostr.str());
  std::string string_line;
  qreal height = 0.0;
  QFontMetrics metrics(m_shared_state->m_font);
  while(getline(istr, string_line))
    {
      /*
        this is awful, we need to make a list of GlyphRun objects
        per line of text, putting multi-line text into a single
        TextLayout and extacting the QGlyphRun does not draw the
        stuff on multiple lines as it interprets the EOL (\n) as
        an unprintable character.
      */
      QTextLayout text;
      QList<QGlyphRun> txt;
      text.setFont(m_shared_state->m_font);
      text.setText(QString(string_line.c_str()));

      text.beginLayout();
      while (1)
        {
          QTextLine line = text.createLine();
          if (!line.isValid())
            break;

          line.setPosition(QPointF(0, height));
          height += metrics.lineSpacing();
        }
      text.endLayout();
      txt = text.glyphRuns();
      m_text_as_glyph_run += txt;
      string_line.clear();
    }

  m_dimensions = params.m_size;
  m_table_pos.rx() = m_dimensions.width() * qreal(params.m_table_pos.x());
  m_table_pos.ry() = m_dimensions.height() * qreal(params.m_table_pos.y());
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
                  m_dimensions, static_cast<qreal>(ms));

      if(m_thousandths_degrees_rotation >= 360 * 1000)
        {
          m_thousandths_degrees_rotation = m_thousandths_degrees_rotation % (360 * 1000);
        }
    }
  else
    {
      m_first_frame = false;
    }

  m_item_rotation = static_cast<qreal>(m_thousandths_degrees_rotation) / (1000.0);

  if(m_shared_state->m_rotating)
    {
      QTransform M;
      M.translate(m_table_pos.x(), m_table_pos.y());
      M.translate(m_dimensions.width() * 0.5, m_dimensions.height() * 0.5);
      M.rotate(m_item_rotation);
      M.translate(-m_dimensions.width() * 0.5, -m_dimensions.height() * 0.5);
      m_parent_matrix_this = M;
    }
  else
    {
      m_parent_matrix_this.reset();
      m_parent_matrix_this.translate(m_table_pos.x(), m_table_pos.y());
    }
}

void
Cell::
paint_pre_children(QPainter *painter)
{
  painter->save();
  painter->fillRect(QRectF(QPointF(0.0f, 0.0f), m_dimensions),
                    m_background_brush);
  painter->translate(m_item_location);
  painter->rotate(m_item_rotation);

  if(!m_image_brush.isNull() && m_shared_state->m_draw_image)
    {
      QSizeF sz(m_image_brush.size());
      painter->translate(-0.5 * sz.width(), -0.5 * sz.height());
      painter->drawImage(QPointF(0.0, 0.0), m_image_brush);
      painter->translate(0.5 * sz.width(), 0.5 * sz.height());
    }

  if(m_shared_state->m_draw_text)
    {
      QPen pen(m_text_brush);
      painter->setPen(pen);

      if(m_shared_state->m_use_glyph_run)
        {
          for(QList<QGlyphRun>::const_iterator iter = m_text_as_glyph_run.begin(),
                end = m_text_as_glyph_run.end(); iter != end; ++iter)
            {
              painter->drawGlyphRun(QPointF(0.0, 0.0), *iter);
            }
        }
      else
        {
          QRectF textRect(QPointF(0.0, 0.0), m_dimensions);
          painter->drawText(textRect, Qt::AlignLeft | Qt::TextDontClip | Qt::TextExpandTabs,
                            m_text_as_string);
        }
    }
  painter->restore();

  if(m_shared_state->m_rotating && m_shared_state->m_stroke_width > 0.0)
    {
      QPen pen(m_line_brush);
      painter->setRenderHint(QPainter::Antialiasing, m_shared_state->m_anti_alias_stroking);
      painter->setRenderHint(QPainter::HighQualityAntialiasing, m_shared_state->m_anti_alias_stroking);
      pen.setWidthF(m_shared_state->m_stroke_width);
      pen.setStyle(Qt::SolidLine);
      pen.setJoinStyle(Qt::MiterJoin);
      pen.setCapStyle(Qt::FlatCap);
      painter->strokePath(m_shared_state->m_path, pen);
      painter->setRenderHint(QPainter::Antialiasing, true);
      painter->setRenderHint(QPainter::HighQualityAntialiasing, false);
    }
  m_shared_state->m_cells_drawn++;
}
