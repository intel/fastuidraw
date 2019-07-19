#ifndef FASTUIDRAW_DEMO_PANZOOMTRACKER_HPP
#define FASTUIDRAW_DEMO_PANZOOMTRACKER_HPP

#include <SDL.h>
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
  enum zoom_direction_t
    {
      zoom_direction_negative_y,
      zoom_direction_positive_y
    };

  PanZoomTracker(int32_t zoom_gesture_begin_time_ms=500,
                 float zoom_divider=40.0f):
    m_scale_zooming(1.0f),
    m_zoom_direction(zoom_direction_positive_y),
    m_zoom_gesture_begin_time(zoom_gesture_begin_time_ms),
    m_zoom_divider(zoom_divider),
    m_is_zooming(false),
    m_button_down(false)
  {}


  const ScaleTranslate<float>&
  transformation(void) const
  {
    return m_transformation;
  }

  void
  transformation(const ScaleTranslate<float> &v);

  /*!\fn
    Tell PanZoomTracker of a motion event
    \param pos position of event
    \param delta amunt of motion of event of event
   */
  void
  handle_motion(const fastuidraw::vec2 &pos,
                const fastuidraw::vec2 &delta);

  /*!\fn
    Tell PanZoomTracker of down (i.e begin gesture) event
    \param pos position of event
   */
  void
  handle_down(const fastuidraw::vec2 &pos);

  /*!\fn
    Tell PanZoomTracker of button up (i.e. end gesture) event
   */
  void
  handle_up(void);

  /*!
    Scale zooming factor
   */
  float m_scale_zooming;

  enum zoom_direction_t m_zoom_direction;

private:

  int32_t m_zoom_gesture_begin_time;
  float m_zoom_divider;

  fastuidraw::vec2 m_zoom_pivot;
  simple_time m_zoom_time;
  bool m_is_zooming, m_button_down;

  ScaleTranslate<float> m_transformation, m_start_gesture;
};


class PanZoomTrackerSDLEvent:public PanZoomTracker
{
public:
  PanZoomTrackerSDLEvent(int32_t zoom_gesture_begin_time_ms=500,
                         float zoom_divider=40.0f):
    PanZoomTracker(zoom_gesture_begin_time_ms,
                   zoom_divider),
    m_scale_event(1.0f, 1.0f),
    m_translate_event(0.0f, 0.0f)
  {}

  /*!\fn
    Maps to handle_drag, handle_button_down, handle_button_up
    from events on mouse button 0
   */
  void
  handle_event(const SDL_Event &ev);

  /*!\fn
    Amount by which to scale events
   */
  fastuidraw::vec2 m_scale_event, m_translate_event;
};

#endif
