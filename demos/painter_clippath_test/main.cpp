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
      view_transformer,
      path1_transformer,
      path2_transformer,
      rect_transformer,

      number_transformers
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
      fill_rounded_rect,
      rounded_rect_clips,

      number_rounded_rect_modes
    };

  enum anti_alias_mode_t
    {
      no_anti_alias,
      by_anti_alias_auto,
      by_anti_alias_simple,
      by_anti_alias_hq,
      by_anti_alias_fastest,

      number_anti_alias_modes
    };

  class Transformer
  {
  public:
    Transformer(void):
      m_shear(1.0f, 1.0f),
      m_angle(0.0f)
    {}

    void
    concat_to_painter(const reference_counted_ptr<Painter> &ref) const
    {
      const float conv(M_PI / 180.0f);
      m_zoomer.transformation().concat_to_painter(ref);
      ref->shear(m_shear.x(), m_shear.y());
      ref->rotate(m_angle * conv);
    }

    PanZoomTrackerSDLEvent m_zoomer;
    vec2 m_shear;
    float m_angle;
  };

  void
  draw_element(const Path &path, unsigned int clip_mode, const vec4 &pen_color,
               const Transformer &matrix);

  void
  draw_combined(const Path &path1, unsigned int clip_mode1, const Transformer &matrix1,
                const Path &path2, unsigned int clip_mode2, const Transformer &matrix2,
                const vec4 &pen_color);

  enum return_code
  load_path(Path &out_path, const std::string &file);

  void
  make_paths(void);

  void
  update_cts_params(void);

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
  unsigned int m_active_transformer;
  unsigned int m_aa_mode;
  vecN<Transformer, number_transformers> m_transformers;
  vecN<std::string, number_clip_modes> m_clip_labels;
  vecN<std::string, number_transformers> m_transformer_labels;
  vecN<std::string, number_combine_clip_modes> m_combine_clip_labels;
  vecN<std::string, number_rounded_rect_modes> m_rounded_rect_mode_labels;
  vecN<std::string, number_anti_alias_modes> m_anti_alias_mode_labels;
  vecN<enum Painter::shader_anti_alias_t, number_anti_alias_modes> m_shader_anti_alias_mode_values;
  simple_time m_draw_timer;
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
  m_rounded_rect_mode(fill_rounded_rect),
  m_active_transformer(view_transformer),
  m_aa_mode(by_anti_alias_auto)
{
  std::cout << "Controls:\n"
            << "\t1: cycle through clip modes for path1\n"
            << "\t2: cycle through clip modes for path2\n"
            << "\ts: cycle through active transformer controls\n"
            << "\tc: change combine clip mode\n"
            << "\tr: change rounded rect mode\n"
            << "\tu: change anti-alias mode\n"
            << "\t6: x-shear (hold ctrl to decrease)\n"
            << "\t7: y-shear (hold ctrl to decrease)\n"
            << "\t0: Rotate left\n"
            << "\t9: Rotate right\n";

  m_clip_labels[clip_in] = "clip_in";
  m_clip_labels[clip_out] = "clip_out";
  m_clip_labels[no_clip] = "no_clip";

  m_transformer_labels[view_transformer] = "view_transformer";
  m_transformer_labels[path1_transformer] = "path1_transformer";
  m_transformer_labels[path2_transformer] = "path2_transformer";
  m_transformer_labels[rect_transformer] = "rect_transformer";

  m_combine_clip_labels[separate_clipping] = "separate_clipping";
  m_combine_clip_labels[path1_then_path2] = "path1_then_path2";
  m_combine_clip_labels[path2_then_path1] = "path2_then_path1";

  m_rounded_rect_mode_labels[fill_rounded_rect] = "fill_rounded_rect";
  m_rounded_rect_mode_labels[rounded_rect_clips] = "rounded_rect_clips";

  m_anti_alias_mode_labels[no_anti_alias] = "no_anti_alias";
  m_anti_alias_mode_labels[by_anti_alias_auto] = "by_anti_alias_auto";
  m_anti_alias_mode_labels[by_anti_alias_simple] = "by_anti_alias_simple";
  m_anti_alias_mode_labels[by_anti_alias_hq] = "by_anti_alias_hq";
  m_anti_alias_mode_labels[by_anti_alias_fastest] = "by_anti_alias_fastest";

  m_shader_anti_alias_mode_values[no_anti_alias] = Painter::shader_anti_alias_none;
  m_shader_anti_alias_mode_values[by_anti_alias_auto] = Painter::shader_anti_alias_auto;
  m_shader_anti_alias_mode_values[by_anti_alias_simple] = Painter::shader_anti_alias_simple;
  m_shader_anti_alias_mode_values[by_anti_alias_hq] = Painter::shader_anti_alias_high_quality;
  m_shader_anti_alias_mode_values[by_anti_alias_fastest] = Painter::shader_anti_alias_fastest;
}

void
painter_clip_test::
handle_event(const SDL_Event &ev)
{
  if (m_active_transformer != view_transformer)
    {
      /* invert y-direction for the other transformers */
      ScaleTranslate<float> inv;

      inv = m_transformers[view_transformer].m_zoomer.transformation().inverse();
      m_transformers[m_active_transformer].m_zoomer.m_scale_event = vec2(inv.scale(), inv.scale());
      m_transformers[m_active_transformer].m_zoomer.m_scale_zooming = inv.scale();
      m_transformers[m_active_transformer].m_zoomer.m_translate_event = inv.translation();
    }

  m_transformers[m_active_transformer].m_zoomer.handle_event(ev);
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
          cycle_value(m_active_transformer, ev.key.keysym.mod & (KMOD_SHIFT|KMOD_CTRL|KMOD_ALT), number_transformers);
          std::cout << "Active zoomer set to: " << m_transformer_labels[m_active_transformer] << "\n";
          break;
        case SDLK_c:
          cycle_value(m_combine_clip_mode, ev.key.keysym.mod & (KMOD_SHIFT|KMOD_CTRL|KMOD_ALT), number_combine_clip_modes);
          std::cout << "Combine clip mode set to: " << m_combine_clip_labels[m_combine_clip_mode] << "\n";
          break;
        case SDLK_r:
          cycle_value(m_rounded_rect_mode, ev.key.keysym.mod & (KMOD_SHIFT|KMOD_CTRL|KMOD_ALT), number_rounded_rect_modes);
          std::cout << "Rounded rect mode set to: " << m_rounded_rect_mode_labels[m_rounded_rect_mode] << "\n";
          break;
        case SDLK_u:
          cycle_value(m_aa_mode,
                      ev.key.keysym.mod & (KMOD_SHIFT|KMOD_CTRL|KMOD_ALT),
                      number_anti_alias_modes);
          std::cout << "RoundedRect drawing anti-alias mode set to: "
                    << m_anti_alias_mode_labels[m_aa_mode]
                    << "\n";
          break;
        }
      break;
    };
}

void
painter_clip_test::
update_cts_params(void)
{
  const Uint8 *keyboard_state = SDL_GetKeyboardState(nullptr);
  FASTUIDRAWassert(keyboard_state != nullptr);

  float speed = static_cast<float>(m_draw_timer.restart_us()), speed_shear;
  if (m_active_transformer == view_transformer)
    {
      return;
    }

  speed /= 1000.0f;
  if (keyboard_state[SDL_SCANCODE_LSHIFT])
    {
      speed *= 0.1f;
    }
  if (keyboard_state[SDL_SCANCODE_RSHIFT])
    {
      speed *= 10.0f;
    }

  speed_shear = 0.01f * speed;
  if (keyboard_state[SDL_SCANCODE_LCTRL] || keyboard_state[SDL_SCANCODE_RCTRL])
    {
      speed_shear = -speed_shear;
    }

  speed_shear = 0.01f * speed;
  if (keyboard_state[SDL_SCANCODE_LCTRL] || keyboard_state[SDL_SCANCODE_RCTRL])
    {
      speed_shear = -speed_shear;
    }

  vec2 *pshear(&m_transformers[m_active_transformer].m_shear);
  float *pangle(&m_transformers[m_active_transformer].m_angle);

  if (keyboard_state[SDL_SCANCODE_6])
    {
      pshear->x() += speed_shear;
      std::cout << "Shear set to: " << *pshear << "\n";
    }
  if (keyboard_state[SDL_SCANCODE_7])
    {
      pshear->y() += speed_shear;
      std::cout << "Shear set to: " << *pshear << "\n";
    }

  if (keyboard_state[SDL_SCANCODE_9])
    {
      *pangle += speed * 0.1f;
      std::cout << "Angle set to: " << *pangle << "\n";
    }
  if (keyboard_state[SDL_SCANCODE_0])
    {
      *pangle -= speed * 0.1f;
      std::cout << "Angle set to: " << *pangle << "\n";
    }
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
              << Path::contour_close();
    }

  if (routine_fail == load_path(m_path2, m_path2_file.value()))
    {
      m_path2 << vec2(100.0f, 0.0f)
              << vec2(0.0f, 100.0f)
              << vec2(100.0f, 100.0f)
              << Path::contour_close();
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
  m_draw_timer.restart();
}

void
painter_clip_test::
draw_element(const Path &path, unsigned int clip_mode, const vec4 &pen_color,
             const Transformer &matrix)
{
  PainterBrush brush;

  m_painter->save();
  matrix.concat_to_painter(m_painter);
  brush.pen(pen_color);
  switch(clip_mode)
    {
    default:
      break;

    case clip_in:
      m_painter->clip_in_path(path, Painter::nonzero_fill_rule);
      break;
    case clip_out:
      m_painter->clip_out_path(path, Painter::nonzero_fill_rule);
      break;
    }

  vec2 p0, p1, sz;
  p0 = path.tessellation()->bounding_box_min();
  p1 = path.tessellation()->bounding_box_max();
  sz = p1 - p0;

  m_painter->fill_rect(PainterData(&brush), p0 - 0.5f * sz, 2.0f * sz);

  m_painter->restore();
}

void
painter_clip_test::
draw_combined(const Path &path1, unsigned int clip_mode1, const Transformer &matrix1,
              const Path &path2, unsigned int clip_mode2, const Transformer &matrix2,
              const vec4 &pen_color)
{
  PainterItemMatrix M(m_painter->transformation());
  PainterBrush brush;

  brush.pen(pen_color);
  m_painter->save();
  matrix1.concat_to_painter(m_painter);
  switch (clip_mode1)
    {
    default:
      break;
    case clip_in:
      m_painter->clip_in_path(path1, Painter::nonzero_fill_rule);
      break;
    case clip_out:
      m_painter->clip_out_path(path1, Painter::nonzero_fill_rule);
      break;
    }

  m_painter->transformation(M);
  matrix2.concat_to_painter(m_painter);
  switch (clip_mode2)
    {
    default:
      break;
    case clip_in:
      m_painter->clip_in_path(path2, Painter::nonzero_fill_rule);
      break;
    case clip_out:
      m_painter->clip_out_path(path2, Painter::nonzero_fill_rule);
      break;
    }

  m_painter->transformation(M);
  matrix1.concat_to_painter(m_painter);

  vec2 p0, p1, sz;
  p0 = path1.tessellation()->bounding_box_min();
  p1 = path1.tessellation()->bounding_box_max();
  sz = p1 - p0;

  m_painter->fill_rect(PainterData(&brush), p0 - 0.5f * sz, 2.0f * sz);
  m_painter->restore();
}

void
painter_clip_test::
draw_frame(void)
{
  update_cts_params();
  m_painter->begin(m_surface, Painter::y_increases_downwards);
  m_transformers[view_transformer].concat_to_painter(m_painter);

  PainterItemMatrix M(m_painter->transformation());
  m_transformers[rect_transformer].concat_to_painter(m_painter);
  switch(m_rounded_rect_mode)
    {
    case fill_rounded_rect:
      {
        PainterBrush brush;
        brush.pen(vec4(1.0f, 1.0f, 0.0f, 1.0f));
        m_painter->fill_rounded_rect(m_painter->default_shaders().fill_shader(),
                                     PainterData(&brush), m_rect,
                                     m_shader_anti_alias_mode_values[m_aa_mode]);
      }
      break;

    case rounded_rect_clips:
      m_painter->clip_in_rounded_rect(m_rect);
      break;
    }
  m_painter->transformation(M);

  switch(m_combine_clip_mode)
    {
    case separate_clipping:
      draw_element(m_path1, m_path1_clip_mode, vec4(1.0f, 0.0f, 0.0f, 0.5f),
                   m_transformers[path1_transformer]);
      draw_element(m_path2, m_path2_clip_mode, vec4(0.0f, 1.0f, 0.0f, 0.5f),
                   m_transformers[path2_transformer]);
      break;

    case path1_then_path2:
      draw_combined(m_path1, m_path1_clip_mode, m_transformers[path1_transformer],
                    m_path2, m_path2_clip_mode, m_transformers[path2_transformer],
                    vec4(0.0f, 1.0f, 1.0f, 0.5f));
      break;

    case path2_then_path1:
      draw_combined(m_path2, m_path2_clip_mode, m_transformers[path2_transformer],
                    m_path1, m_path1_clip_mode, m_transformers[path1_transformer],
                    vec4(1.0f, 0.0f, 1.0f, 0.5f));
      break;
    }

  m_painter->end();
  fastuidraw_glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
  fastuidraw_glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
  m_surface->blit_surface(GL_NEAREST);

}

int
main(int argc, char **argv)
{
  painter_clip_test P;
  return P.main(argc, argv);
}
