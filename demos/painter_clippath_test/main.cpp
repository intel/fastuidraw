#include <iostream>
#include <sstream>
#include <fstream>

#include "sdl_painter_demo.hpp"
#include "simple_time.hpp"
#include "PanZoomTracker.hpp"
#include "read_path.hpp"
#include "ImageLoader.hpp"
#include "colorstop_command_line.hpp"
#include "cycle_value.hpp"

using namespace fastuidraw;





class painter_clip_test:public sdl_painter_demo
{
public:
  painter_clip_test(void);

protected:
  void
  derived_init(int, int);

  void
  draw_frame(void);

  void
  handle_event(const SDL_Event &ev);

private:

  enum
    {
      clip_in,
      clip_out,
      no_clip,

      number_clip_modes
    };

  enum
    {
      view_zoomer,
      path1_zoomer,
      path2_zoomer,

      number_zoomers
    };

  void
  draw_element(const Path &path, unsigned int clip_mode, const vec4 &pen_color,
               const float3x3 &matrix);

  enum return_code
  load_path(Path &out_path, const std::string &file);

  void
  make_paths(void);

  command_line_argument_value<std::string> m_path1_file;
  command_line_argument_value<std::string> m_path2_file;

  Path m_path1, m_path2;

  unsigned int m_path1_clip_mode, m_path2_clip_mode;
  unsigned int m_active_zoomer;
  vecN<PanZoomTrackerSDLEvent, number_zoomers> m_zoomers;
  vecN<std::string, number_clip_modes> m_clip_labels;
  vecN<std::string, number_zoomers> m_zoomer_labels;
};

painter_clip_test::
painter_clip_test():
  m_path1_file("", "path1_file",
              "if non-empty read the geometry of the path1 from the specified file, "
              "otherwise use a default path",
              *this),
  m_path2_file("", "path2_file",
              "if non-empty read the geometry of the path1 from the specified file, "
              "otherwise use a default path",
               *this),
  m_path1_clip_mode(no_clip),
  m_path2_clip_mode(no_clip),
  m_active_zoomer(view_zoomer)
{
  std::cout << "Controls:\n"
            << "\t1: cycle through clip modes for path1\n"
            << "\t2: cycle through clip modes for path2\n"
            << "\ts: cycle through active zoomer controls\n";

  m_clip_labels[clip_in] = "clip_in";
  m_clip_labels[clip_out] = "clip_out";
  m_clip_labels[no_clip] = "no_clip";

  m_zoomer_labels[view_zoomer] = "view_zoomer";
  m_zoomer_labels[path1_zoomer] = "path1_zoomer";
  m_zoomer_labels[path2_zoomer] = "path2_zoomer";
}

void
painter_clip_test::
handle_event(const SDL_Event &ev)
{
  if(m_active_zoomer != view_zoomer)
    {
      ScaleTranslate<float> inv;

      inv = m_zoomers[view_zoomer].transformation().inverse();
      m_zoomers[m_active_zoomer].m_scale_event = vec2(inv.scale(), inv.scale());
      m_zoomers[m_active_zoomer].m_scale_zooming = inv.scale();
      m_zoomers[m_active_zoomer].m_translate_event = inv.translation();
    }

  m_zoomers[m_active_zoomer].handle_event(ev);
  switch(ev.type)
    {
    case SDL_WINDOWEVENT:
      if(ev.window.event == SDL_WINDOWEVENT_RESIZED)
        {
          on_resize(ev.window.data1, ev.window.data2);
        }
      break;

    case SDL_QUIT:
        end_demo(0);
        break;

    case SDL_KEYUP:
      switch(ev.key.keysym.sym)
        {
        case SDLK_ESCAPE:
          end_demo(0);
          break;
        case SDLK_1:
          cycle_value(m_path1_clip_mode, ev.key.keysym.mod & (KMOD_SHIFT|KMOD_CTRL|KMOD_ALT), number_clip_modes);
          std::cout << "Path1 clip mode set to: " << m_clip_labels[m_path1_clip_mode] << "\n";
          break;
        case SDLK_2:
          cycle_value(m_path2_clip_mode, ev.key.keysym.mod & (KMOD_SHIFT|KMOD_CTRL|KMOD_ALT), number_clip_modes);
          std::cout << "Path2 clip mode set to: " << m_clip_labels[m_path2_clip_mode] << "\n";
          break;
        case SDLK_s:
          cycle_value(m_active_zoomer, ev.key.keysym.mod & (KMOD_SHIFT|KMOD_CTRL|KMOD_ALT), number_zoomers);
          std::cout << "Active zoomer set to: " << m_zoomer_labels[m_active_zoomer] << "\n";
          break;
        }
      break;
    };
}


enum return_code
painter_clip_test::
load_path(Path &out_path, const std::string &filename)
{
  if(!filename.empty())
    {
      std::ifstream path_file(filename.c_str());
      if(path_file)
        {
          std::stringstream buffer;
          buffer << path_file.rdbuf();
          read_path(out_path, buffer.str());
          return routine_success;
        }
    }
  return routine_fail;
}


void
painter_clip_test::
make_paths(void)
{
  if(routine_fail == load_path(m_path1, m_path1_file.m_value))
    {
      m_path1 << vec2(100.0f, 100.0f)
              << vec2(0.0f, 0.0f)
              << vec2(100.0f,  0.0f)
              << Path::contour_end();
    }

  if(routine_fail == load_path(m_path2, m_path2_file.m_value))
    {
      m_path2 << vec2(100.0f, 0.0f)
              << vec2(0.0f, 100.0f)
              << vec2(100.0f, 100.0f)
              << Path::contour_end();
    }


}


void
painter_clip_test::
derived_init(int, int)
{
  make_paths();
}



void
painter_clip_test::
draw_element(const Path &path, unsigned int clip_mode, const vec4 &pen_color,
             const float3x3 &matrix)
{
  PainterBrush brush;

  m_painter->save();
  m_painter->concat(matrix);
  brush.pen(pen_color);
  switch(clip_mode)
    {
    default:
      break;

    case clip_in:
      m_painter->clipInPath(path, PainterEnums::nonzero_fill_rule);
      break;
    case clip_out:
      m_painter->clipOutPath(path, PainterEnums::nonzero_fill_rule);
      break;
    }

  vec2 p0, p1, sz;
  p0 = path.tessellation()->bounding_box_min();
  p1 = path.tessellation()->bounding_box_max();
  sz = p1 - p0;

  m_painter->draw_rect(PainterData(&brush), p0 - 0.5f * sz, 2.0f * sz, false);

  m_painter->restore();
}

void
painter_clip_test::
draw_frame(void)
{
  glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  m_painter->begin();

  ivec2 wh(dimensions());
  float3x3 proj(float_orthogonal_projection_params(0, wh.x(), wh.y(), 0)), m;
  m = proj * m_zoomers[view_zoomer].transformation().matrix3();
  m_painter->transformation(m);

  draw_element(m_path1, m_path1_clip_mode, vec4(1.0f, 0.0f, 0.0f, 1.0f),
               m_zoomers[path1_zoomer].transformation().matrix3());

  draw_element(m_path2, m_path2_clip_mode, vec4(0.0f, 1.0f, 0.0f, 1.0f),
               m_zoomers[path2_zoomer].transformation().matrix3());

  m_painter->end();

}

int
main(int argc, char **argv)
{
  painter_clip_test P;
  return P.main(argc, argv);
}
