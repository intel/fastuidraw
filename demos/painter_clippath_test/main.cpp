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
      rect_zoomer,

      number_zoomers
    };

  enum
    {
      separate_clipping,
      path1_then_path2,
      path2_then_path1,

      number_combine_clip_modes
    };

  enum
    {
      draw_rounded_rect,
      rounded_rect_clips,

      number_rounded_rect_modes
    };

  void
  draw_element(const Path &path, unsigned int clip_mode, const vec4 &pen_color,
               const float3x3 &matrix);

  void
  draw_combined(const Path &path1, unsigned int clip_mode1, const float3x3 &matrix1,
                const Path &path2, unsigned int clip_mode2, const float3x3 &matrix2,
                const vec4 &pen_color);

  enum return_code
  load_path(Path &out_path, const std::string &file);

  void
  make_paths(void);

  command_line_argument_value<std::string> m_path1_file;
  command_line_argument_value<std::string> m_path2_file;
  command_line_argument_value<float> m_rect_width;
  command_line_argument_value<float> m_rect_height;
  command_line_argument_value<float> m_rect_min_radii_x;
  command_line_argument_value<float> m_rect_min_radii_y;
  command_line_argument_value<float> m_rect_max_radii_x;
  command_line_argument_value<float> m_rect_max_radii_y;

  Path m_path1, m_path2;
  RoundedRect m_rect;

  unsigned int m_path1_clip_mode, m_path2_clip_mode;
  unsigned int m_combine_clip_mode, m_rounded_rect_mode;
  unsigned int m_active_zoomer;
  vecN<PanZoomTrackerSDLEvent, number_zoomers> m_zoomers;
  vecN<std::string, number_clip_modes> m_clip_labels;
  vecN<std::string, number_zoomers> m_zoomer_labels;
  vecN<std::string, number_combine_clip_modes> m_combine_clip_labels;
  vecN<std::string, number_rounded_rect_modes> m_rounded_rect_mode_labels;
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
  m_rect_width(100.0f, "rect_width", "Rounded rectangle width", *this),
  m_rect_height(50.0f, "rect_height", "Rounded rectangle height", *this),
  m_rect_min_radii_x(10.0f, "rect_min_radii_x", "Rounded rectangle min-radii-x", *this),
  m_rect_min_radii_y(5.0f, "rect_min_radii_y", "Rounded rectangle min-radii-y", *this),
  m_rect_max_radii_x(10.0f, "rect_max_radii_x", "Rounded rectangle max-radii-x", *this),
  m_rect_max_radii_y(5.0f, "rect_max_radii_y", "Rounded rectangle max-radii-y", *this),
  m_path1_clip_mode(no_clip),
  m_path2_clip_mode(no_clip),
  m_combine_clip_mode(separate_clipping),
  m_rounded_rect_mode(draw_rounded_rect),
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

  m_combine_clip_labels[separate_clipping] = "separate_clipping";
  m_combine_clip_labels[path1_then_path2] = "path1_then_path2";
  m_combine_clip_labels[path2_then_path1] = "path2_then_path1";

  m_rounded_rect_mode_labels[draw_rounded_rect] = "draw_rounded_rect";
  m_rounded_rect_mode_labels[rounded_rect_clips] = "rounded_rect_clips";
}

void
painter_clip_test::
handle_event(const SDL_Event &ev)
{
  if (m_active_zoomer != view_zoomer)
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
      if (ev.window.event == SDL_WINDOWEVENT_RESIZED)
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
        case SDLK_c:
          cycle_value(m_combine_clip_mode, ev.key.keysym.mod & (KMOD_SHIFT|KMOD_CTRL|KMOD_ALT), number_combine_clip_modes);
          std::cout << "Combine clip mode set to: " << m_combine_clip_labels[m_combine_clip_mode] << "\n";
          break;
        case SDLK_r:
          cycle_value(m_rounded_rect_mode, ev.key.keysym.mod & (KMOD_SHIFT|KMOD_CTRL|KMOD_ALT), number_rounded_rect_modes);
          std::cout << "Rounded rect mode set to: " << m_rounded_rect_mode_labels[m_rounded_rect_mode] << "\n";
        }
      break;
    };
}


enum return_code
painter_clip_test::
load_path(Path &out_path, const std::string &filename)
{
  if (!filename.empty())
    {
      std::ifstream path_file(filename.c_str());
      if (path_file)
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
  if (routine_fail == load_path(m_path1, m_path1_file.value()))
    {
      m_path1 << vec2(100.0f, 100.0f)
              << vec2(0.0f, 0.0f)
              << vec2(100.0f,  0.0f)
              << Path::contour_end();
    }

  if (routine_fail == load_path(m_path2, m_path2_file.value()))
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

  m_rect.m_min_point = vec2(0.0f, 0.0f);
  m_rect.m_max_point = vec2(m_rect_width.value(), m_rect_height.value());
  m_rect.m_min_corner_radii = vec2(m_rect_min_radii_x.value(), m_rect_min_radii_y.value());
  m_rect.m_max_corner_radii = vec2(m_rect_max_radii_x.value(), m_rect_max_radii_y.value());
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
      m_painter->clipInPath(path, Painter::nonzero_fill_rule);
      break;
    case clip_out:
      m_painter->clipOutPath(path, Painter::nonzero_fill_rule);
      break;
    }

  vec2 p0, p1, sz;
  p0 = path.tessellation()->bounding_box_min();
  p1 = path.tessellation()->bounding_box_max();
  sz = p1 - p0;

  m_painter->draw_rect(PainterData(&brush), p0 - 0.5f * sz, 2.0f * sz);

  m_painter->restore();
}

void
painter_clip_test::
draw_combined(const Path &path1, unsigned int clip_mode1, const float3x3 &matrix1,
              const Path &path2, unsigned int clip_mode2, const float3x3 &matrix2,
              const vec4 &pen_color)
{
  PainterItemMatrix M(m_painter->transformation());
  PainterBrush brush;

  brush.pen(pen_color);
  m_painter->save();
  m_painter->concat(matrix1);
  switch (clip_mode1)
    {
    default:
      break;
    case clip_in:
      m_painter->clipInPath(path1, Painter::nonzero_fill_rule);
      break;
    case clip_out:
      m_painter->clipOutPath(path1, Painter::nonzero_fill_rule);
      break;
    }

  m_painter->transformation(M);
  m_painter->concat(matrix2);
  switch (clip_mode2)
    {
    default:
      break;
    case clip_in:
      m_painter->clipInPath(path2, Painter::nonzero_fill_rule);
      break;
    case clip_out:
      m_painter->clipOutPath(path2, Painter::nonzero_fill_rule);
      break;
    }

  m_painter->transformation(M);
  m_painter->concat(matrix1);

  vec2 p0, p1, sz;
  p0 = path1.tessellation()->bounding_box_min();
  p1 = path1.tessellation()->bounding_box_max();
  sz = p1 - p0;

  m_painter->draw_rect(PainterData(&brush), p0 - 0.5f * sz, 2.0f * sz);
  m_painter->restore();
}

void
painter_clip_test::
draw_frame(void)
{
  m_painter->begin(m_surface, Painter::y_increases_downwards);
  m_zoomers[view_zoomer].transformation().concat_to_painter(m_painter);

  PainterItemMatrix M(m_painter->transformation());
  m_zoomers[rect_zoomer].transformation().concat_to_painter(m_painter);
  switch(m_rounded_rect_mode)
    {
    case draw_rounded_rect:
      {
        PainterBrush brush;
        brush.pen(vec4(1.0f, 1.0f, 0.0f, 1.0f));
        m_painter->draw_rounded_rect(m_painter->default_shaders().fill_shader(),
                                     PainterData(&brush), m_rect);
      }
      break;

    case rounded_rect_clips:
      m_painter->clipInRoundedRect(m_rect);
      break;
    }
  m_painter->transformation(M);

  switch(m_combine_clip_mode)
    {
    case separate_clipping:
      draw_element(m_path1, m_path1_clip_mode, vec4(1.0f, 0.0f, 0.0f, 0.5f),
                   m_zoomers[path1_zoomer].transformation().matrix3());
      draw_element(m_path2, m_path2_clip_mode, vec4(0.0f, 1.0f, 0.0f, 0.5f),
                   m_zoomers[path2_zoomer].transformation().matrix3());
      break;

    case path1_then_path2:
      draw_combined(m_path1, m_path1_clip_mode, m_zoomers[path1_zoomer].transformation().matrix3(),
                    m_path2, m_path2_clip_mode, m_zoomers[path2_zoomer].transformation().matrix3(),
                    vec4(0.0f, 1.0f, 1.0f, 0.5f));
      break;

    case path2_then_path1:
      draw_combined(m_path2, m_path2_clip_mode, m_zoomers[path2_zoomer].transformation().matrix3(),
                    m_path1, m_path1_clip_mode, m_zoomers[path1_zoomer].transformation().matrix3(),
                    vec4(1.0f, 0.0f, 1.0f, 0.5f));
      break;
    }

  m_painter->end();
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
  m_surface->blit_surface(GL_NEAREST);

}

int
main(int argc, char **argv)
{
  painter_clip_test P;
  return P.main(argc, argv);
}
