#include "PanZoomTracker.hpp"

/////////////////////////////////
// PanZoomTracker methods
void
PanZoomTracker::
handle_down(const fastuidraw::vec2 &pos)
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
handle_motion(const fastuidraw::vec2 &pos, const fastuidraw::vec2 &delta)
{
  if (!m_button_down)
    {
      return;
    }

  if (m_zoom_time.elapsed() > m_zoom_gesture_begin_time)
    {
      m_is_zooming = true;
    }

  float zdivide(m_scale_zooming * m_zoom_divider);

  if (!m_is_zooming)
    {
      float zdx(pos.x() - m_zoom_pivot.x());
      float zdy(pos.y() - m_zoom_pivot.y());

      m_transformation.translation( m_transformation.translation() + delta);

      if (fastuidraw::t_abs(zdx) > zdivide or fastuidraw::t_abs(zdy) > zdivide)
        {
          m_zoom_time.restart();
          m_zoom_pivot = pos;
          m_start_gesture = m_transformation;
        }
    }
  else
    {
      float zoom_factor(pos.y() - m_zoom_pivot.y());
      ScaleTranslate<float> R;

      if (m_zoom_direction == zoom_direction_negative_y)
	{
	  zoom_factor = -zoom_factor;
	}

      zoom_factor /= zdivide;
      if (zoom_factor < 0.0f)
        {
          zoom_factor = -1.0f/fastuidraw::t_min(-1.0f, zoom_factor);
        }
      else
        {
          zoom_factor = fastuidraw::t_max(1.0f, zoom_factor);
        }
      R.scale(zoom_factor);
      R.translation( (1.0f - zoom_factor) * m_zoom_pivot);
      m_transformation = R * m_start_gesture;
    }
}


void
PanZoomTracker::
transformation(const ScaleTranslate<float> &v)
{
  m_transformation = v;
  if (m_button_down)
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
      if (ev.button.button == 1)
        {
          handle_down(m_scale_event * fastuidraw::vec2(ev.button.x, ev.button.y) + m_translate_event);
        }
      break;

    case SDL_MOUSEBUTTONUP:
      if (ev.button.button == 1)
        {
          handle_up();
        }
      break;

    case SDL_MOUSEMOTION:
      handle_motion(m_scale_event * fastuidraw::vec2(ev.motion.x, ev.motion.y) + m_translate_event,
                    m_scale_event * fastuidraw::vec2(ev.motion.xrel, ev.motion.yrel));
      break;
    }
}
