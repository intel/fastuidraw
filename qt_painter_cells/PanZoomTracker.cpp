#include <QMouseEvent>
#include <iostream>
#include "PanZoomTracker.hpp"

/////////////////////////////////
// PanZoomTracker methods
void
PanZoomTracker::
handle_down(const QPointF &pos)
{
  m_zoom_time.restart();
  m_button_down = true;
  m_zoom_pivot = pos;
  m_start_gesture = m_transformation;
}

void
PanZoomTracker::
handle_up(void)
{
  m_is_zooming = false;
  m_button_down = false;
}


void
PanZoomTracker::
handle_motion(const QPointF &pos, const QPointF &delta)
{
  if(!m_button_down)
    {
      return;
    }

  if(m_zoom_time.elapsed() > m_zoom_gesture_begin_time)
    {
      m_is_zooming = true;
    }

  qreal zdivide(m_scale_zooming * m_zoom_divider);

  if(!m_is_zooming)
    {
      qreal zdx(pos.x() - m_zoom_pivot.x());
      qreal zdy(pos.y() - m_zoom_pivot.y());

      m_transformation.translation(m_transformation.translation() + delta);

      if(qAbs(zdx) > zdivide or qAbs(zdy) > zdivide)
        {
          m_zoom_time.restart();
          m_zoom_pivot = pos;
          m_start_gesture = m_transformation;
        }
    }
  else
    {
      qreal zoom_factor(pos.y() - m_zoom_pivot.y());
      ScaleTranslate R;

      zoom_factor /= zdivide;
      if(zoom_factor < 0.0)
        {
          zoom_factor = -1.0/std::min(-1.0, zoom_factor);
        }
      else
        {
          zoom_factor = std::max(1.0, zoom_factor);
        }
      R.scale(zoom_factor);
      R.translation( (1.0 - zoom_factor) * m_zoom_pivot);
      m_transformation = R * m_start_gesture;
    }
}


void
PanZoomTracker::
transformation(const ScaleTranslate &v)
{
  m_transformation = v;
  if(m_button_down)
    {
      m_start_gesture = m_transformation;
    }
}

/////////////////////////////////
// PanZoomTrackerEvent methods
void
PanZoomTrackerEvent::
handle_event(QEvent *ev)
{
  switch(ev->type())
    {
    default:
      break;

    case QEvent::MouseButtonPress:
    case QEvent::MouseButtonRelease:
    case QEvent::MouseMove:
      QMouseEvent *qev(static_cast<QMouseEvent*>(ev));
      handle_mouse_event(*qev);
      ev->accept();
      break;
    }
}

void
PanZoomTrackerEvent::
handle_mouse_event(QMouseEvent &ev)
{
  switch(ev.type())
    {
    default:
      break;

    case QEvent::MouseButtonPress:
      if(ev.button() == Qt::LeftButton)
        {
          QPointF pt;
          m_pt = ev.posF();
          pt.rx() = m_scale_event.x() * m_pt.x() + m_translate_event.x();
          pt.ry() = m_scale_event.y() * m_pt.y() + m_translate_event.y();
          handle_down(pt);
        }
      break;

    case QEvent::MouseButtonRelease:
      if(ev.button() == Qt::LeftButton)
        {
          handle_up();
        }
      break;

    case QEvent::MouseMove:
      if(ev.buttons() & Qt::LeftButton)
        {
          QPointF delta, pt;
          delta = ev.posF() - m_pt;
          m_pt = ev.posF();
          pt.rx() = m_scale_event.x() * m_pt.x() + m_translate_event.x();
          pt.ry() = m_scale_event.y() * m_pt.y() + m_translate_event.y();
          delta.rx() *= m_scale_event.x();
          delta.ry() *= m_scale_event.y();
          handle_motion(pt, delta);
        }
      break;
    }
}
