#include <iostream>
#include <fastuidraw/gl_backend/image_gl.hpp>
#include <fastuidraw/gl_backend/gluniform.hpp>
#include <fastuidraw/gl_backend/opengl_trait.hpp>
#include "sdl_demo.hpp"
#include "ImageLoader.hpp"
#include "PanZoomTracker.hpp"

using namespace fastuidraw;

class image_test:public sdl_demo
{
public:
  image_test(void):
    sdl_demo("image-test"),
    m_image_src("./demo_data/images/1024x1024.png", "image", "Image file to use as source image", *this),

    m_slack(0, "slack", "image slack in color tiles", *this),

    m_log2_color_tile_size(5, "log2_color_tile_size",
                           "Specifies the log2 of the width and height of each color tile",
                           *this),

    m_log2_num_color_tiles_per_row_per_col(8, "log2_num_color_tiles_per_row_per_col",
                                           "Specifies the log2 of the number of color tiles "
                                           "in each row and column of each layer; note that "
                                           "then the total number of color tiles available "
                                           "is given as num_color_layers*pow(2, 2*log2_num_color_tiles_per_row_per_col)",
                                           *this),

    m_num_color_layers(1, "num_color_layers",
                       "Specifies the number of layers in the color texture; note that "
                       "then the total number of color tiles available "
                       "is given as num_color_layers*pow(2, 2*log2_num_color_tiles_per_row_per_col)",
                       *this),


    m_log2_index_tile_size(2, "log2_index_tile_size",
                           "Specifies the log2 of the width and height of each index tile",
                           *this),

    m_log2_num_index_tiles_per_row_per_col(6, "log2_num_index_tiles_per_row_per_col",
                                           "Specifies the log2 of the number of index tiles "
                                           "in each row and column of each layer; note that "
                                           "then the total number of index tiles available "
                                           "is given as num_index_layers*pow(2, 2*log2_num_index_tiles_per_row_per_col)",
                                           *this),

    m_num_index_layers(2, "num_index_layers",
                       "Specifies the number of layers in the index texture; note that "
                       "then the total number of index tiles available "
                       "is given as num_index_layers*pow(2, 2*log2_num_index_tiles_per_row_per_col)",
                       *this),

    m_color_boundary_mix_value(0.0f),
    m_filtered_lookup(0.0f),
    m_current_program(draw_image_on_atlas),
    m_current_layer(0),
    m_sampler(0),
    m_ibo(0)
  {
    std::cout << "Controls:\n"
              << "\ti: draw image\n"
              << "\ta: draw atlas\n"
              << "\tnumber keys(1-9): toggle k'th index tile boundary line(image drawing)\n"
              << "\t0: show color tile boundary line(image drawing)\n"
              << "\tf: toggle linear filtering (with slack=0 will have artifacts when linearly filtered)\n"
              << "\tn: draw next layer (atlas drawing)\n"
              << "\tp: draw previous layer (atlas drawing)\n"
              << "\tt: show transformation data\n"
              << "\tMouse Drag (left button): pan\n"
              << "\tHold Mouse (left button), then drag up/down: zoom out/in\n";
  }

  ~image_test()
  {
    if(m_sampler != 0)
      {
        glDeleteSamplers(1, &m_sampler);
      }
  }

protected:

  void
  init_gl(int w, int h)
  {
    build_images();
    build_programs();
    on_resize(w, h);
    set_attributes_indices();
    bind_textures();
  }

  void
  bind_textures(void)
  {
    switch(m_current_program)
      {
      case draw_image_on_atlas:
        glActiveTexture(GL_TEXTURE0 + color_atlas_texture_unit);
        glBindTexture(GL_TEXTURE_2D_ARRAY, m_atlas->color_texture());
        glBindSampler(0, m_sampler);
        glActiveTexture(GL_TEXTURE0 + index_atlas_texture_unit);
        glBindTexture(GL_TEXTURE_2D_ARRAY, m_atlas->index_texture());
        break;
      case draw_atlas:
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D_ARRAY, m_atlas->color_texture());
        glBindSampler(0, 0);
        break;
      }
  }

  void
  draw_frame(void)
  {
    if(m_program[m_current_program].m_pr)
      {
        glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        m_program[m_current_program].m_pr->use_program();
        glBindVertexArray(m_program[m_current_program].m_vao);

        gl::Uniform(m_program[m_current_program].m_pvm, m_pvm);
        gl::Uniform(m_program[m_current_program].m_scale,
                    m_program[m_current_program].m_zoomer.transformation().scale());
        gl::Uniform(m_program[m_current_program].m_translate,
                    m_program[m_current_program].m_zoomer.transformation().translation());

        if(m_program[m_current_program].m_layer != -1)
          {
            gl::Uniform(m_program[m_current_program].m_layer, m_current_layer);
          }

        if(m_program[m_current_program].m_index_boundary_mix != -1)
          {
            const_c_array<float> index_boundary_mix_values;
            index_boundary_mix_values = cast_c_array(m_index_boundary_mix_values);
            gl::Uniform(m_program[m_current_program].m_index_boundary_mix,
                        index_boundary_mix_values);
          }
        if(m_program[m_current_program].m_color_boundary_mix != -1)
          {
            gl::Uniform(m_program[m_current_program].m_color_boundary_mix, m_color_boundary_mix_value);
          }
        if(m_program[m_current_program].m_filtered_lookup != -1)
          {
            gl::Uniform(m_program[m_current_program].m_filtered_lookup, m_filtered_lookup);
          }

        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, 0);
      }
    else
      {
        vec3 random;
        for(int i=0; i<3; ++i)
          {
            random[i] = static_cast<float>(rand() % 255) / 255.0f;
          }

        glClearColor(random[0], random[1], random[2], 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
      }
  }

  void
  handle_event(const SDL_Event &ev)
  {
    int old_program(m_current_program);

    m_program[m_current_program].m_zoomer.handle_event(ev);
    switch(ev.type)
      {
      case SDL_QUIT:
        end_demo(0);
        break;

      case SDL_KEYUP:
        switch(ev.key.keysym.sym)
          {
          case SDLK_ESCAPE:
            end_demo(0);
            break;
          case SDLK_n:
            m_current_layer = std::min(m_current_layer + 1,
                                       m_atlas->color_store()->dimensions().z() - 1);
            break;
          case SDLK_p:
            m_current_layer = std::max(0, m_current_layer - 1);
            break;
          case SDLK_t:
            std::cout << "Tansformation = (sc="
                      << m_program[m_current_program].m_zoomer.transformation().scale()
                      << ", tr="
                      << m_program[m_current_program].m_zoomer.transformation().translation()
                      << ")\n";
            break;
          case SDLK_a:
            m_current_program = draw_atlas;
            break;
          case SDLK_i:
            m_current_program = draw_image_on_atlas;
            break;

          case SDLK_f:
            m_filtered_lookup = 1.0f - m_filtered_lookup;
            break;

          case SDLK_0:
            m_color_boundary_mix_value = 0.5f - m_color_boundary_mix_value;
            break;

          case SDLK_1:
          case SDLK_2:
          case SDLK_3:
          case SDLK_4:
          case SDLK_5:
          case SDLK_6:
          case SDLK_7:
          case SDLK_8:
          case SDLK_9:
            {
              int idx(ev.key.keysym.sym - SDLK_1);
              m_index_boundary_mix_values[idx] = 0.5f - m_index_boundary_mix_values[idx];
            }
            break;
          }

        if(old_program != m_current_program)
          {
            bind_textures();
            std::cout << "Current draw: "
                      << m_program[m_current_program].m_label
                      << " (id=" << m_current_program << ")\n";
          }
        break;

      case SDL_WINDOWEVENT:
        if(ev.window.event == SDL_WINDOWEVENT_RESIZED)
          {
            on_resize(ev.window.data1, ev.window.data2);
          }
        break;
      }
  }


private:

  void
  build_images(void)
  {
    glGenSamplers(1, &m_sampler);
    assert(m_sampler != 0);

    glSamplerParameteri(m_sampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glSamplerParameteri(m_sampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    gl::ImageAtlasGL::params params;
    params
      .log2_color_tile_size(m_log2_color_tile_size.m_value)
      .log2_num_color_tiles_per_row_per_col(m_log2_num_color_tiles_per_row_per_col.m_value)
      .num_color_layers(m_num_color_layers.m_value)
      .log2_index_tile_size(m_log2_index_tile_size.m_value)
      .log2_num_index_tiles_per_row_per_col(m_log2_num_index_tiles_per_row_per_col.m_value)
      .num_index_layers(m_num_index_layers.m_value)
      .delayed(false);

    m_atlas = FASTUIDRAWnew gl::ImageAtlasGL(params);

    std::vector<u8vec4> image_data;
    ivec2 image_size;

    image_size = load_image_to_array(m_image_src.m_value, image_data);
    if(image_size.x() == 0 || image_size.y() == 0)
      {
        image_size = ivec2(8, 8);
        image_data.resize(image_size.x() * image_size.y());
        for(int idx = 0, y = 0; y < image_size.y(); ++y)
          {
            for(int x = 0; x < image_size.x(); ++x, ++idx)
              {
                image_data[idx] = ((x+y) & 1) ?
                  u8vec4(255,     0, 255, 255) :
                  u8vec4(  0,   255,   0, 255);
              }
          }
      }

    m_image = Image::create(m_atlas, image_size.x(), image_size.y(),
                            cast_c_array(image_data), m_slack.m_value);

    std::cout << "Image \"" << m_image_src.m_value
              << " of size " << m_image->dimensions()
              << "\" requires " << m_image->number_index_lookups()
              << " index look ups\n"
              << "Image master tile at " << m_image->master_index_tile()
              << " of size " << m_image->master_index_tile_dims()
              << "\n";

    m_index_boundary_mix_values.resize(m_image->number_index_lookups() + 1, 0.0f);
  }

  void
  build_programs(void)
  {
    gl::Program::handle pr;

    {
      pr = FASTUIDRAWnew gl::Program(gl::Shader::shader_source()
                                    .add_source("layer_texture_blit.vert.glsl.resource_string",
                                                gl::Shader::from_resource),

                                    gl::Shader::shader_source()
                                    .add_source("detect_boundary.glsl.resource_string",
                                                gl::Shader::from_resource)
                                    .add_source("layer_texture_blit.frag.glsl.resource_string",
                                                gl::Shader::from_resource),
                                    gl::PreLinkActionArray()
                                    .add_binding("attrib_pos", 0),
                                    gl::ProgramInitializerArray()
                                    .add_sampler_initializer("image", 0)
                                    .add_uniform_initializer<float>("tile_size",
                                                                    float(m_atlas->color_tile_size())) );
      m_program[draw_atlas].set("draw_atlas", pr);
    }

    {
      gl::Shader::shader_source glsl_compute_coord;
      glsl_compute_coord = m_atlas->glsl_compute_coord_src("compute_atlas_coord", "indexAtlas");

      pr = FASTUIDRAWnew gl::Program(gl::Shader::shader_source()
                                    .add_source("atlas_image_blit.vert.glsl.resource_string", gl::Shader::from_resource),

                                    gl::Shader::shader_source()
                                    .add_macro("MAX_IMAGE_NUM_LOOKUPS", m_image->number_index_lookups())
                                    .add_source("detect_boundary.glsl.resource_string", gl::Shader::from_resource)
                                    .add_source("atlas_image_blit.frag.glsl.resource_string", gl::Shader::from_resource)
                                    .add_source(glsl_compute_coord),

                                    gl::PreLinkActionArray()
                                    .add_binding("attrib_pos", attrib_pos_vertex_attrib)
                                    .add_binding("attrib_image_shader_coord", index_coord_vertex_attrib),

                                    gl::ProgramInitializerArray()
                                    .add_sampler_initializer("imageAtlas", color_atlas_texture_unit)
                                    .add_sampler_initializer("indexAtlas", index_atlas_texture_unit)
                                    .add_uniform_initializer<float>("color_tile_size", float(m_atlas->color_tile_size() - 2 * m_image->slack()))
                                    .add_uniform_initializer<float>("index_tile_size", float(m_atlas->index_tile_size()))
                                    .add_uniform_initializer<int>("uniform_image_num_lookups", m_image->number_index_lookups())
                                    .add_uniform_initializer<int>("image_slack", m_image->slack())
                                    .add_uniform_initializer<vec3>("imageAtlasDims", vec3(m_atlas->color_store()->dimensions())) );

      m_program[draw_image_on_atlas].set("draw_image_on_atlas", pr);
    }

  }

  void
  on_resize(int w, int h)
  {
    float_orthogonal_projection_params proj(0, w, h, 0);
    m_pvm = float4x4(proj);
    glViewport(0, 0, w, h);
  }

  void
  set_attributes_indices(void)
  {


    glGenBuffers(1, &m_ibo);
    assert(m_ibo != 0);

    GLushort indices[]=
      {
        0, 1, 2,
        0, 2, 3
      };
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    {
      vec2 image_size(m_atlas->color_store()->dimensions().x(),
                      m_atlas->color_store()->dimensions().y());

      glBindVertexArray(m_program[draw_atlas].m_vao);
      vec2 draw_tex_attribs[]=
        {
          vec2(0, 0),
          vec2(0, image_size.y()),
          vec2(image_size.x(), image_size.y()),
          vec2(image_size.x(), 0)
        };
      glBindBuffer(GL_ARRAY_BUFFER, m_program[draw_atlas].m_vbo);
      glBufferData(GL_ARRAY_BUFFER, sizeof(draw_tex_attribs), draw_tex_attribs, GL_STATIC_DRAW);
      glEnableVertexAttribArray(0);
      glVertexAttribPointer(0,
                            gl::opengl_trait<vec2>::count,
                            gl::opengl_trait<vec2>::type,
                            GL_FALSE,
                            gl::opengl_trait<vec2>::stride,
                            NULL);
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ibo);
    }


    if(m_program[draw_image_on_atlas].m_vao)
      {
        glBindVertexArray(m_program[draw_image_on_atlas].m_vao);

        vec2 image_size(m_image->dimensions());
        vecN<vec2, 2> corner(gl::ImageAtlasGL::shader_coords(m_image));
        float layer(m_image->master_index_tile().z());
        float image_index_attribs[] =
          {
            0,              0,              corner[0].x(), corner[0].y(), layer,
            0,              image_size.y(), corner[0].x(), corner[1].y(), layer,
            image_size.x(), image_size.y(), corner[1].x(), corner[1].y(), layer,
            image_size.x(), 0,              corner[1].x(), corner[0].y(), layer
          };
        glBindBuffer(GL_ARRAY_BUFFER, m_program[draw_image_on_atlas].m_vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(image_index_attribs), image_index_attribs, GL_STATIC_DRAW);

        uint8_t *p(0);

        glEnableVertexAttribArray(attrib_pos_vertex_attrib);
        glVertexAttribPointer(attrib_pos_vertex_attrib,
                              2,
                              GL_FLOAT,
                              GL_FALSE,
                              sizeof(float) * 5,
                              p);
        glEnableVertexAttribArray(index_coord_vertex_attrib);
        glVertexAttribPointer(index_coord_vertex_attrib,
                              3,
                              GL_FLOAT,
                              GL_FALSE,
                              sizeof(float) * 5,
                              p + 2 * sizeof(float));
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ibo);
      }
  }

  enum
    {
      draw_image_on_atlas = 0,
      draw_atlas,

      number_draw_types
    };

  enum
    {
      color_atlas_texture_unit = 0,
      index_atlas_texture_unit
    };

  enum
    {
      attrib_pos_vertex_attrib = 0,
      index_coord_vertex_attrib
    };

  class per_program
  {
  public:
    per_program(void):
      m_vao(0),
      m_vbo(0)
    {}

    void
    set(const std::string &label, gl::Program::handle pr)
    {
      assert(pr);
      m_pr = pr;
      m_label = label;
      m_pvm = m_pr->uniform_location("pvm");
      m_scale = m_pr->uniform_location("scale");
      m_translate = m_pr->uniform_location("translate");
      m_layer = m_pr->uniform_location("layer");
      m_index_boundary_mix = m_pr->uniform_location("index_boundary_mix");
      m_color_boundary_mix = m_pr->uniform_location("color_boundary_mix");
      m_filtered_lookup = m_pr->uniform_location("filtered_lookup");

      glGenVertexArrays(1, &m_vao);
      assert(m_vao != 0);

      glGenBuffers(1, &m_vbo);
      assert(m_vbo != 0);
    }

    gl::Program::handle m_pr;
    GLint m_pvm;
    GLint m_scale;
    GLint m_translate;
    GLint m_layer;
    GLuint m_vao;
    GLuint m_vbo;
    GLint m_index_boundary_mix;
    GLint m_color_boundary_mix;
    GLint m_filtered_lookup;
    std::string m_label;
    PanZoomTrackerSDLEvent m_zoomer;
  };

  command_line_argument_value<std::string> m_image_src;
  command_line_argument_value<int> m_slack;
  command_line_argument_value<int> m_log2_color_tile_size, m_log2_num_color_tiles_per_row_per_col;
  command_line_argument_value<int> m_num_color_layers;
  command_line_argument_value<int> m_log2_index_tile_size, m_log2_num_index_tiles_per_row_per_col;
  command_line_argument_value<int> m_num_index_layers;

  float m_color_boundary_mix_value;
  std::vector<float> m_index_boundary_mix_values;
  float m_filtered_lookup;

  gl::ImageAtlasGL::handle m_atlas;
  Image::handle m_image;
  vecN<per_program, number_draw_types> m_program;
  int m_current_program;

  int m_current_layer;

  GLuint m_sampler;
  GLuint m_ibo;

  float4x4 m_pvm;
};


int
main(int argc, char **argv)
{
  image_test a;
  return a.main(argc, argv);
}
