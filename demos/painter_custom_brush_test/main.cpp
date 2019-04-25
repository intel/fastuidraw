#include <fastuidraw/painter/painter.hpp>
#include <fastuidraw/text/glyph_cache.hpp>
#include <fastuidraw/text/font_freetype.hpp>
#include <fastuidraw/text/font_database.hpp>
#include <fastuidraw/glsl/painter_custom_brush_shader_glsl.hpp>

#include "sdl_painter_demo.hpp"
#include "PanZoomTracker.hpp"
#include "text_helper.hpp"
#include "read_path.hpp"
#include "ImageLoader.hpp"
#include "colorstop_command_line.hpp"
#include "command_line_list.hpp"
#include "cycle_value.hpp"

using namespace fastuidraw;

c_string
on_off(bool v)
{
  return v ? "ON" : "OFF";
}

class ExampleCustomBrushData:public PainterBrushShaderData
{
public:
  ExampleCustomBrushData(void):
    m_width(100.0f),
    m_height(100.0f)
  {
  }

  ExampleCustomBrushData&
  width(float v)
  {
    m_width = v;
    return *this;
  }

  ExampleCustomBrushData&
  height(float v)
  {
    m_height = v;
    return *this;
  }

  void
  pack_data(fastuidraw::c_array<fastuidraw::generic_data> dst) const override
  {
    dst[0].f = 1.0f / m_width;
    dst[1].f = 1.0f / m_height;
  }

  unsigned int
  data_size(void) const override
  {
    return FASTUIDRAW_ROUND_UP_MULTIPLE_OF4(2);
  }

  float m_width, m_height;
};

class painter_custom_brush_test:public sdl_painter_demo
{
public:
  painter_custom_brush_test(void);

protected:

  void
  derived_init(int w, int h);

  void
  draw_frame(void);

  void
  handle_event(const SDL_Event &ev);

private:
  enum
    {
      custom_red_green_brush,
      custom_green_blue_brush,

      number_custom_brushes
    };

  command_line_argument_value<std::string> m_image_file;
  command_line_argument_value<bool> m_use_atlas;

  vecN<reference_counted_ptr<PainterCustomBrushShader>, number_custom_brushes> m_brush_shader;
  reference_counted_ptr<const Image> m_image;
  unsigned int m_current_brush;
};

painter_custom_brush_test::
painter_custom_brush_test(void):
  m_image_file("", "image", "if a valid file name, apply an image to drawing the fill", *this),
  m_use_atlas(true, "use_atlas",
              "If false, each image is realized as a texture; if "
              "GL_ARB_bindless_texture or GL_NV_bindless_texture "
              "is supported, the Image objects are realized as bindless "
              "texture, thus avoding draw breaks; if both of these "
              "extensions is not present, then images are realized as "
              "bound textures which means that a draw break will be present "
              "whenever the image changes, harming performance.",
              *this),
  m_current_brush(number_custom_brushes)
{
  std::cout << "\tp: cycle through brushes\n";
}

void
painter_custom_brush_test::
derived_init(int w, int h)
{
  FASTUIDRAWunused(w);
  FASTUIDRAWunused(h);
  if (!m_image_file.value().empty())
    {
      ImageLoader image_data(m_image_file.value());
      if (image_data.non_empty())
        {
          if (m_use_atlas.value())
            {
              m_image = m_painter->image_atlas().create(image_data.width(),
                                                        image_data.height(),
                                                        image_data,
                                                        Image::on_atlas);
            }
          else
            {
              m_image = m_painter->image_atlas().create_non_atlas(image_data.width(),
                                                                  image_data.height(),
                                                                  image_data);
            }
        }
    }

  glsl::varying_list varyings;
  c_string macros[number_custom_brushes] =
    {
      [0] = "RED_GREEN",
      [1] = "GREEN_BLUE"
    };
  varyings
    .add_float("brush_p_x")
    .add_float("brush_p_y");

  for (int i = 0; i < number_custom_brushes; ++i)
    {
      glsl::ShaderSource vert_src, frag_src;

      vert_src
        .add_macro(macros[i])
        .add_source("custom_brush_example.vert.glsl.resource_string", glsl::ShaderSource::from_resource)
        .remove_macro(macros[i]);

      frag_src
        .add_macro(macros[i])
        .add_source("custom_brush_example.frag.glsl.resource_string", glsl::ShaderSource::from_resource)
        .remove_macro(macros[i]);

      m_brush_shader[i] = FASTUIDRAWnew glsl::PainterCustomBrushShaderGLSL(1, vert_src, frag_src, varyings);
      m_painter->painter_shader_registrar().register_shader(m_brush_shader[i]);
    }
}

void
painter_custom_brush_test::
draw_frame(void)
{
  m_painter->begin(m_surface, Painter::y_increases_downwards);

  PainterBrush brush;
  ExampleCustomBrushData custom_brush_data;
  PainterData data;
  vec2 dims(dimensions());

  if (number_custom_brushes == m_current_brush)
    {
      if (m_image)
        {
          brush
            .image(m_image)
            .repeat_window(vec2(0.0f, 0.0f),
                           vec2(m_image->dimensions()));
        }
      else
        {
          brush.color(1.0f, 0.5f, 0.5f, 0.75f);
        }
      data.set(&brush);
    }
  else
    {
      custom_brush_data
        .width(0.25f * dims.x())
        .height(0.35f * dims.y());

      PainterCustomBrush br(m_brush_shader[m_current_brush].get(),
                            &custom_brush_data);

      data.set(br);
    }

  m_painter->fill_rect(data,
                       Rect()
                       .width(dims.x())
                       .height(dims.y()),
                       false);
  m_painter->end();
  fastuidraw_glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
  fastuidraw_glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
  m_surface->blit_surface(GL_NEAREST);
}

void
painter_custom_brush_test::
handle_event(const SDL_Event &ev)
{
  switch(ev.type)
    {
    case SDL_QUIT:
      end_demo(0);
      break;

    case SDL_WINDOWEVENT:
      if (ev.window.event == SDL_WINDOWEVENT_RESIZED)
        {
          on_resize(ev.window.data1, ev.window.data2);
        }
      break;

    case SDL_KEYUP:
      switch(ev.key.keysym.sym)
        {
        case SDLK_ESCAPE:
          end_demo(0);
          break;
        case SDLK_p:
          cycle_value(m_current_brush,
                      ev.key.keysym.mod & (KMOD_SHIFT|KMOD_CTRL|KMOD_ALT),
                      number_custom_brushes + 1);
          std::cout << "Set to brush #" << m_current_brush << "\n";
          break;
        }
      break;
    } //switch
}

int
main(int argc, char **argv)
{
  painter_custom_brush_test P;
  return P.main(argc, argv);
}
