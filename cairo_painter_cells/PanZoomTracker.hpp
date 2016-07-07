#pragma once

#include <SDL.h>
#include <stdint.h>

#include "vec2.hpp"
#include "ScaleTranslate.hpp"
#include "simple_time.hpp"

/*!\class PanZoomTracker
  A PanZoomTracker implements the following gesture:
    - panning while dragging
    - hold button down (long) time, then up or left zoom out,
      down or right zoom in.
 */
class PanZoomTracker
{
public:
  PanZoomTracker(int32_t zoom_gesture_begin_time_ms=500,
                 double zoom_divider=double(40)):
    m_scale_zooming(double(1)),
    m_zoom_gesture_begin_time(zoom_gesture_begin_time_ms),
    m_zoom_divider(zoom_divider),
    m_is_zooming(false),
    m_button_down(false)
  {}

  const ScaleTranslate&
  transformation(void) const
  {
    return m_transformation;
  }

  void
  transformation(const ScaleTranslate &v);

  /*!
    Tell PanZoomTracker of a motion event
    \param pos position of event
    \param delta amunt of motion of event of event
   */
  void
  handle_motion(const vec2 &pos,
                const vec2 &delta);

  /*!
    Tell PanZoomTracker of down (i.e begin gesture) event
    \param pos position of event
   */
  void
  handle_down(const vec2 &pos);

  /*!
    Tell PanZoomTracker of button up (i.e. end gesture) event
   */
  void
  handle_up(void);

  /*!\var
    Scale zooming factor
   */
  double m_scale_zooming;

private:

  int32_t m_zoom_gesture_begin_time;
  double m_zoom_divider;

  vec2 m_zoom_pivot;
  simple_time m_zoom_time;
  bool m_is_zooming, m_button_down;

  ScaleTranslate m_transformation, m_start_gesture;
};


class PanZoomTrackerSDLEvent:public PanZoomTracker
{
public:
  PanZoomTrackerSDLEvent(int32_t zoom_gesture_begin_time_ms=500,
                         double zoom_divider=40.0f):
    PanZoomTracker(zoom_gesture_begin_time_ms,
                   zoom_divider),
    m_scale_event(1, 1),
    m_translate_event(0, 0)
  {
  }

  /*!
    Maps to handle_drag, handle_button_down, handle_button_up
    from events on mouse button 0
   */
  void
  handle_event(const SDL_Event &ev);

  /*!
    Amount by which to scale events
   */
  vec2 m_scale_event, m_translate_event;
};
