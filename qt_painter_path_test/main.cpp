#include <iostream>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <bitset>
#include <math.h>


#include "qt_demo.hpp"
#include "simple_time.hpp"
#include "PanZoomTracker.hpp"
#include "cycle_value.hpp"

const char*
on_off(bool v)
{
  return v ? "ON" : "OFF";
}

template<typename T>
class labeled_style
{
public:
  labeled_style(T v, const std::string &L):
    m_value(v),
    m_label(L)
  {}

  T m_value;
  std::string m_label;
};

template<typename T>
labeled_style<T>
make_label(T v, const std::string &L)
{
  return labeled_style<T>(v, L);
}

class painter_stroke_test:public qt_demo
{
public:
  painter_stroke_test(void);

protected:

  void
  derived_init(int w, int h);

  void
  paint(QPainter *p);

  void
  handle_event(QEvent *ev);

private:

  enum
    {
      shift_key_down,
      ctrl_key_down,
      key_6_down,
      key_7_down,
      key_9_down,
      key_0_down,
      return_key_down,
      left_bracket_key_down,
      right_bracket_key_down,

      number_keys_tracked
    };

  void
  construct_path(void);

  void
  construct_dash_patterns(void);

  void
  update_cts_params(void);

  command_line_argument_value<float> m_change_stroke_width_rate;
  command_line_argument_value<std::string> m_path_file;

  QPainterPath m_path;
  std::vector<QVector<qreal> > m_dash_patterns;

  PanZoomTrackerSDLEvent m_zoomer;
  labeled_style<enum Qt::PenCapStyle> m_cap_labels[3];
  labeled_style<enum Qt::PenJoinStyle> m_join_labels[3];

  unsigned int m_join_style;
  unsigned int m_cap_style;
  /* m_dash pattern:
      0 -> undashed stroking
      [1, m_dash_patterns.size()] -> dashed stroking
   */
  unsigned int m_dash;

  bool
  is_dashed_stroking(void)
  {
    return m_dash != 0;
  }

  unsigned int
  dash_pattern(void)
  {
    return m_dash != 0 ?
      m_dash - 1:
      0;
  }

  qreal m_stroke_width;
  bool m_stroke_aa;

  QPointF m_shear, m_shear2;
  qreal m_angle;

  simple_time m_draw_timer;
  std::vector<bool> m_key_downs;
};

//////////////////////////////////////
// painter_stroke_test methods
painter_stroke_test::
painter_stroke_test(void):
  sdl_painter_demo("painter-stroke-test"),
  m_change_stroke_width_rate(10.0f, "change_stroke_width_rate",
                             "rate of change in pixels/sec for changing stroke width "
                             "when changing stroke when key is down",
                             *this),
  m_path_file("", "path_file",
              "if non-empty read the geometry of the path from the specified file, "
              "otherwise use a default path",
              *this),
  m_join_style(0),
  m_cap_style(0),
  m_dash(0),
  m_stroke_width(1.0),
  m_stroke_aa(true),
  m_shear(1.0, 1.0),
  m_shear2(1.0, 1.0),
  m_angle(0.0),
  m_key_downs(number_keys_tracked, false)
{
  std::cout << "Controls:\n"
            << "\ta: toggle anti-aliased stroking\n"
            << "\tj: cycle through join styles for stroking\n"
            << "\tc: cycle through cap style for stroking\n"
            << "\td: cycle through dash patterns\n"
            << "\t[: decrease stroke width(hold left-shift for slower rate and right shift for faster)\n"
            << "\t]: increase stroke width(hold left-shift for slower rate and right shift for faster)\n"
            << "\tq: reset shear to 1.0\n"
            << "\t6: x-shear (hold ctrl to decrease, hold enter for shear2)\n"
            << "\t7: y-shear (hold ctrl to decrease, hold enter for shear2)\n"
            << "\t0: Rotate left\n"
            << "\t9: Rotate right\n"
            << "\tLeft Mouse Drag: pan\n"
            << "\tHold Left Mouse, then drag up/down: zoom out/in\n";

  m_join_labels[0] = make_label(Qt::RoundJoin, "rounded_joins");
  m_join_labels[1] = make_label(Qt::BevelJoin, "bevel_joins");
  m_join_labels[2] = make_label(Qt::MiterJoin, "miter_joins");

  m_cap_labels[0] = make_label(Qt::FlatCap, "no_caps");
  m_cap_labels[1] = make_label(Qt::RoundCap, "rounded_caps");
  m_cap_labels[2] = make_label(Qt::SquareCap, "square_caps");
}


void
painter_stroke_test::
update_cts_params(void)
{
  float speed = static_cast<float>(m_draw_timer.restart()), speed_stroke, speed_shear;

  if(m_key_downs[shift_key_down])
    {
      speed *= 0.1f;
    }

  //right shift??
  //if(m_key_downs[SDL_SCANCODE_RSHIFT])
  //  {
  //    speed *= 10.0f;
  //  }

  speed_shear = 0.01f * speed;
  if(m_key_downs[ctrl_key_down])
    {
      speed_shear = -speed_shear;
    }

  vec2 *pshear(&m_shear);
  const char *shear_txt = "";
  if(m_key_downs[return_key_down])
    {
      pshear = &m_shear2;
      shear_txt = "2";
    }

  if(m_key_downs[key_6_down])
    {
      pshear->x() += speed_shear;
      std::cout << "Shear" << shear_txt << " set to: " << *pshear << "\n";
    }
  if(m_key_downs[key_7_down])
    {
      pshear->y() += speed_shear;
      std::cout << "Shear " << shear_txt << " set to: " << *pshear << "\n";
    }

  if(m_key_downs[key_9_down])
    {
      m_angle += speed * 0.1f;
      std::cout << "Angle set to: " << m_angle << "\n";
    }
  if(m_key_downs[key_0_down])
    {
      m_angle -= speed * 0.1f;
      std::cout << "Angle set to: " << m_angle << "\n";
    }

  speed_stroke = speed * m_change_stroke_width_rate.m_value;

  if(m_key_downs[right_bracket_key_down])
    {
      m_stroke_width += speed_stroke;
    }

  if(m_key_downs[left_bracket_key_down])
    {
      m_stroke_width -= speed_stroke;
      if(m_stroke_width < 0.0)
        {
          m_stroke_width = 0.0;
        }
    }

  if(m_key_downs[right_bracket_key_down] || m_key_downs[left_bracket_key_down])
    {
      std::cout << "Stroke width set to: " << m_stroke_width << "\n";
    }
}

void
painter_stroke_test::
handle_event(QEvent *ev)
{
  m_zoomer.handle_event(ev);
  switch(ev->type)
    {
    default:
      break;

    case QEvent::KeyRelease:
    case QEvent::KeyPress:
      {
        QKeyEvent *kev(static_cast<QKeyEvent*>(ev));
        switch(kev->key())
          {
          default:
            break;

          case Qt::Key_Escape:
            if(ev->type() == QEvent::KeyRelease)
              {
                end_demo(0);
              }
            break;

          case Qt::Key_Q:
            m_shear = m_shear2 = vec2(1.0f, 1.0f);
            break;

          case Qt::Key_J:
            if(ev->type() == QEvent::KeyRelease)
              {
                cycle_value(m_join_style, ev.key.keysym.mod & (KMOD_SHIFT|KMOD_CTRL|KMOD_ALT), m_join_labels.size());
                std::cout << "Join drawing mode set to: " << m_join_labels[m_join_style] << "\n";
              }
            break;

          case Qt::Key_C:
            if(ev->type() == QEvent::KeyRelease)
              {
                cycle_value(m_cap_style, ev.key.keysym.mod & (KMOD_SHIFT|KMOD_CTRL|KMOD_ALT), m_cap_labels.size());
                std::cout << "Cap drawing mode set to: " << m_cap_labels[m_cap_style] << "\n";
              }
            break;

          case Qt::Key_D:
            if(ev->type() == QEvent::KeyRelease)
              {
                cycle_value(m_dash, ev.key.keysym.mod & (KMOD_SHIFT|KMOD_CTRL|KMOD_ALT), m_dash_patterns.size() + 1);
                if(is_dashed_stroking())
                  {
                    unsigned int P;
                    P = dash_pattern();
                    std::cout << "Set to stroke dashed with pattern: {";
                    for(unsigned int i = 0; i < m_dash_patterns[P].size(); ++i)
                      {
                        if(i != 0)
                          {
                            std::cout << ", ";
                          }
                        std::cout << "Draw(" << m_dash_patterns[P][i].m_draw_length
                                  << "), Space(" << m_dash_patterns[P][i].m_space_length
                                  << ")";
                      }
                  }
                else
                  {
                    std::cout << "Set to stroke non-dashed\n";
                  }
              }
            break;

          } //switch (kev->key())
      }
      break;

    } //switch(ev->type()
}

void
painter_stroke_test::
construct_path(void)
{
  if(!m_path_file.m_value.empty())
    {
      std::ifstream path_file(m_path_file.m_value.c_str());
      if(path_file)
        {
          std::stringstream buffer;
          buffer << path_file.rdbuf();
          read_path(m_path, buffer.str());
          return;
        }
    }

  m_path << vec2(300.0f, 300.0f)
         << Path::contour_end()
         << vec2(50.0f, 35.0f)
         << Path::control_point(60.0f, 50.0f)
         << vec2(70.0f, 35.0f)
         << Path::arc_degrees(180.0, vec2(70.0f, -100.0f))
         << Path::control_point(60.0f, -150.0f)
         << Path::control_point(30.0f, -50.0f)
         << vec2(0.0f, -100.0f)
         << Path::contour_end_arc_degrees(90.0f)
         << vec2(200.0f, 200.0f)
         << vec2(400.0f, 200.0f)
         << vec2(400.0f, 400.0f)
         << vec2(200.0f, 400.0f)
         << Path::contour_end()
         << vec2(-50.0f, 100.0f)
         << vec2(0.0f, 200.0f)
         << vec2(100.0f, 300.0f)
         << vec2(150.0f, 325.0f)
         << vec2(150.0f, 100.0f)
         << Path::contour_end();
}



void
painter_stroke_test::
construct_dash_patterns(void)
{
  m_dash_patterns.push_back(std::vector<PainterDashedStrokeParams::DashPatternElement>());
  m_dash_patterns.back().push_back(PainterDashedStrokeParams::DashPatternElement(20.0f, 10.0f));
  m_dash_patterns.back().push_back(PainterDashedStrokeParams::DashPatternElement(15.0f, 10.0f));
  m_dash_patterns.back().push_back(PainterDashedStrokeParams::DashPatternElement(10.0f, 10.0f));
  m_dash_patterns.back().push_back(PainterDashedStrokeParams::DashPatternElement( 5.0f, 10.0f));
}

void
painter_stroke_test::
paint(QPainter *painter)
{
  ivec2 wh(dimensions());

  update_cts_params();

  /* apply m_zoomer
   */
  m_painter->concat(m_zoomer.transformation().matrix3());

  /* apply shear
   */
  m_painter->shear(m_shear.x(), m_shear.y());

  /* apply rotation
   */
  m_painter->rotate(m_angle * M_PI / 180.0f);

  /* apply shear2
   */
  m_painter->shear(m_shear2.x(), m_shear2.y());

  if(!m_transparent_blue_pen)
    {
      m_transparent_blue_pen = m_painter->packed_value_pool().create_packed_value(PainterBrush().pen(0.0f, 0.0f, 1.0f, 0.5f));
    }

  if(m_stroke_width > 0.0f)
    {
      if(is_dashed_stroking())
        {
          PainterDashedStrokeParams st;
          if(m_have_miter_limit)
            {
              st.miter_limit(m_miter_limit);
            }
          else
            {
              st.miter_limit(-1.0f);
            }
          st.width(m_stroke_width);

          unsigned int D(dash_pattern());
          const_c_array<PainterDashedStrokeParams::DashPatternElement> dash_ptr(&m_dash_patterns[D][0], m_dash_patterns[D].size());
          st.dash_pattern(dash_ptr);

          if(m_stroke_width_in_pixels)
            {
              m_painter->stroke_dashed_path_pixel_width(PainterData(m_transparent_blue_pen, &st),
                                                        m_path, dashed_stroking_is_closed(),
                                                        static_cast<enum PainterEnums::cap_style>(m_cap_style),
                                                        static_cast<enum PainterEnums::join_style>(m_join_style),
                                                        m_stroke_aa);
            }
          else
            {
              m_painter->stroke_dashed_path(PainterData(m_transparent_blue_pen, &st),
                                            m_path, dashed_stroking_is_closed(),
                                            static_cast<enum PainterEnums::cap_style>(m_cap_style),
                                            static_cast<enum PainterEnums::join_style>(m_join_style),
                                            m_stroke_aa);
            }
        }
      else
        {
          PainterStrokeParams st;
          if(m_have_miter_limit)
            {
              st.miter_limit(m_miter_limit);
            }
          else
            {
              st.miter_limit(-1.0f);
            }
          st.width(m_stroke_width);

          if(m_stroke_width_in_pixels)
            {
              m_painter->stroke_path_pixel_width(PainterData(m_transparent_blue_pen, &st),
                                                 m_path,
                                                 static_cast<enum PainterEnums::cap_style>(m_cap_style),
                                                 static_cast<enum PainterEnums::join_style>(m_join_style),
                                                 m_stroke_aa);
            }
          else
            {
              m_painter->stroke_path(PainterData(m_transparent_blue_pen, &st),
                                     m_path,
                                     static_cast<enum PainterEnums::cap_style>(m_cap_style),
                                     static_cast<enum PainterEnums::join_style>(m_join_style),
                                     m_stroke_aa);
            }
        }
    }
}

void
painter_stroke_test::
derived_init(int w, int h)
{
  //put into unit of per ms.
  m_window_change_rate.m_value /= 1000.0f;
  m_change_stroke_width_rate.m_value /= 1000.0f;
  m_change_miter_limit_rate.m_value /= 1000.0f;

  construct_path();
  construct_dash_patterns();

  /* set transformation to center and contain path.
   */
  vec2 delta, dsp(w, h), ratio, mid;
  float mm;
  QRectF bounding_rect;
  bounding_rect = m_path.boundingRect();

  delta = p1 - p0;
  ratio = delta / dsp;
  mm = fastuidraw::t_max(0.00001f, fastuidraw::t_max(ratio.x(), ratio.y()) );
  mid = 0.5 * (p1 + p0);

  ScaleTranslate<float> sc, tr1, tr2;
  tr1.translation(-mid);
  sc.scale( 1.0f / mm);
  tr2.translation(dsp * 0.5f);
  m_zoomer.transformation(tr2 * sc * tr1);

  m_draw_timer.restart();
}

int
main(int argc, char **argv)
{
  painter_stroke_test P;
  return P.main(argc, argv);
}
