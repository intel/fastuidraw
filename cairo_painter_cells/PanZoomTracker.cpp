#include <algorithm>
#include "PanZoomTracker.hpp"

/////////////////////////////////
// PanZoomTracker methods
void
PanZoomTracker::
handle_down(const vec2 &pos)
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
handle_motion(const vec2 &pos, const vec2 &delta)
{
  if(!m_button_down)
    {
      return;
    }

  if(m_zoom_time.elapsed() > m_zoom_gesture_begin_time)
    {
      m_is_zooming = true;
    }

  double zdivide(m_scale_zooming * m_zoom_divider);

  if(!m_is_zooming)
    {
      vec2 zdxdy(pos - m_zoom_pivot);

      m_transformation.translation(m_transformation.translation() + delta);

      if(fabs(zdxdy.m_x) > zdivide or fabs(zdxdy.m_y) > zdivide)
        {
          m_zoom_time.restart();
          m_zoom_pivot = pos;
          m_start_gesture = m_transformation;
        }
    }
  else
    {
      double zoom_factor(pos.y() - m_zoom_pivot.y());
      ScaleTranslate R;

      zoom_factor /= zdivide;
      if(zoom_factor < double(0))
        {
          zoom_factor = double(-1) / std::min(double(-1), zoom_factor);
        }
      else
        {
          zoom_factor = std::max(double(1), zoom_factor);
        }
      R.scale(zoom_factor);
      R.translation(m_zoom_pivot * (double(1) - zoom_factor));
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
          vec2 p;
          p = vec2(ev.button.x, ev.button.y) * m_scale_event + m_translate_event;
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
      vec2 p, m;
      p = vec2(ev.button.x, ev.button.y) * m_scale_event + m_translate_event;
      m = vec2(ev.button.x, ev.button.y) * m_scale_event;
      handle_motion(p, m);
      break;
    }
}
