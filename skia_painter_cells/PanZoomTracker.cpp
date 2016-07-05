#include <algorithm>
#include "PanZoomTracker.hpp"

/////////////////////////////////
// PanZoomTracker methods
void
PanZoomTracker::
handle_down(const SkPoint &pos)
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
handle_motion(const SkPoint &pos, const SkPoint &delta)
{
  if(!m_button_down)
    {
      return;
    }

  if(m_zoom_time.elapsed() > m_zoom_gesture_begin_time)
    {
      m_is_zooming = true;
    }

  SkScalar zdivide(m_scale_zooming * m_zoom_divider);

  if(!m_is_zooming)
    {
      SkScalar zdx(pos.x() - m_zoom_pivot.x());
      SkScalar zdy(pos.y() - m_zoom_pivot.y());

      m_transformation.translation( m_transformation.translation() + delta);

      if(SkScalarAbs(zdx) > zdivide or SkScalarAbs(zdy) > zdivide)
        {
          m_zoom_time.restart();
          m_zoom_pivot = pos;
          m_start_gesture = m_transformation;
        }
    }
  else
    {
      SkScalar zoom_factor(pos.y() - m_zoom_pivot.y());
      ScaleTranslate R;

      zoom_factor /= zdivide;
      if(zoom_factor < SkScalar(0))
        {
          zoom_factor = SkScalar(-1) / std::min(SkScalar(-1), zoom_factor);
        }
      else
        {
          zoom_factor = std::max(SkScalar(1), zoom_factor);
        }
      R.scale(zoom_factor);
      R.translation(m_zoom_pivot * (SkScalar(1) - zoom_factor));
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
// PanZoomTrackerSDLEvent methods
void
PanZoomTrackerSDLEvent::
handle_event(const SDL_Event &ev)
{
  switch(ev.type)
    {
    case SDL_MOUSEBUTTONDOWN:
      if(ev.button.button == 1)
        {
          SkPoint p;
          p.fX = SkScalar(ev.button.x) * m_scale_event.x() + m_translate_event.x();
          p.fY = SkScalar(ev.button.y) * m_scale_event.y() + m_translate_event.y();
          handle_down(p);
        }
      break;

    case SDL_MOUSEBUTTONUP:
      if(ev.button.button == 1)
        {
          handle_up();
        }
      break;

    case SDL_MOUSEMOTION:
      SkPoint p, m;
      p.fX = SkScalar(ev.button.x) * m_scale_event.x() + m_translate_event.x();
      p.fY = SkScalar(ev.button.y) * m_scale_event.y() + m_translate_event.y();
      m.fX = SkScalar(ev.motion.xrel) * m_scale_event.x();
      m.fY = SkScalar(ev.motion.yrel) * m_scale_event.y();
      handle_motion(p, m);
      break;
    }
}
