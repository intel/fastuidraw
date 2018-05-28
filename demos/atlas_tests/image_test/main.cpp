#include <iostream>
#include <dirent.h>
#include <fastuidraw/glsl/shader_code.hpp>
#include <fastuidraw/gl_backend/image_gl.hpp>
#include <fastuidraw/gl_backend/gluniform.hpp>
#include <fastuidraw/gl_backend/opengl_trait.hpp>
#include <fastuidraw/gl_backend/gl_program.hpp>
#include <fastuidraw/gl_backend/gl_get.hpp>
#include "sdl_demo.hpp"
#include "ImageLoader.hpp"
#include "PanZoomTracker.hpp"
#include "cycle_value.hpp"

using namespace fastuidraw;


class command_line_list:
  public command_line_argument,
  public std::set<std::string>
{
public:
  command_line_list(const std::string &nm,
                    const std::string &desc,
                    command_line_register &p):
    command_line_argument(p),
    m_name(nm)
  {
    std::ostringstream ostr;
    ostr << "\n\t" << m_name << " value"
         << format_description_string(m_name, desc);
    m_description = tabs_to_spaces(ostr.str());
  }

  virtual
  int
  check_arg(const std::vector<std::string> &argv, int location)
  {
    int argc(argv.size());
    if (location + 1 < argc && argv[location] == m_name)
      {
        insert(argv[location+1]);
        std::cout << "\n\t" << m_name << " \""
                  << argv[location+1] << "\" ";
        return 2;
      }
    return 0;
  }

  virtual
  void
  print_command_line_description(std::ostream &ostr) const
  {
    ostr << "[" << m_name << " value] ";
  }

  virtual
  void
  print_detailed_description(std::ostream &ostr) const
  {
    ostr << m_description;
  }

private:
  std::string m_name, m_description;
};


class image_test:public sdl_demo
{
public:
  image_test(void):
    sdl_demo("image-test"),
    m_images("add_image", "Add an image or images to be shown, directory values recurse into files", *this),
    m_print_loaded_image_list(false, "print_loaded_image_list", "If true, print to stdout what images are loaded", *this),
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
    m_current_image(0),
    m_current_layer(0),
    m_sampler(0),
    m_ibo(0)
  {
    std::cout << "Controls:\n"
              << "\ti: cycle what image to draw\n"
              << "\ta: toggle between drawing image and drawing atlas\n"
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
    if (m_sampler != 0)
      {
        glDeleteSamplers(1, &m_sampler);
      }
  }

protected:

  void
  add_single_image(const std::string &filename)
  {
    ImageLoader image_data(filename);

    if (image_data.non_empty())
      {
        reference_counted_ptr<Image> p;
        p = Image::create(m_atlas, image_data.width(), image_data.height(),
                          image_data, m_slack.m_value);

        m_image_handles.push_back(p);
        m_image_names.push_back(filename);

        if (m_print_loaded_image_list.m_value)
          {
            std::cout << "Image \"" << filename
                      << " of size " << m_image_handles.back()->dimensions()
                      << "\" requires " << m_image_handles.back()->number_index_lookups()
                      << " index look ups, "
                      << "master tile at " << m_image_handles.back()->master_index_tile()
                      << " of size " << m_image_handles.back()->master_index_tile_dims()
                      << "\n";
          }
      }
  }

  void
  add_images(const std::string &filename)
  {
    DIR *dir;
    struct dirent *entry;

    dir = opendir(filename.c_str());
    if (!dir)
      {
      add_single_image(filename);
      return;
    }

    for(entry = readdir(dir); entry != nullptr; entry = readdir(dir))
      {
        std::string file;
        file = entry->d_name;
        if (file != ".." && file != ".")
          {
            add_images(filename + "/" + file);
          }
      }
    closedir(dir);
  }

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
    if (m_program[m_current_program].m_pr)
      {
        glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        m_program[m_current_program].m_pr->use_program();
        glBindVertexArray(m_program[m_current_program].m_vao);

        gl::Uniform(m_program[m_current_program].m_pvm, m_pvm);
        gl::Uniform(m_program[m_current_program].m_scale,
                    m_program[m_current_program].m_zoomer.transformation().scale());
        gl::Uniform(m_program[m_current_program].m_translate,
                    m_program[m_current_program].m_zoomer.transformation().translation());

        if (m_program[m_current_program].m_layer != -1)
          {
            gl::Uniform(m_program[m_current_program].m_layer, m_current_layer);
          }

        if (m_program[m_current_program].m_index_boundary_mix != -1)
          {
            c_array<const float> index_boundary_mix_values;
            index_boundary_mix_values = cast_c_array(m_index_boundary_mix_values);
            gl::Uniform(m_program[m_current_program].m_index_boundary_mix,
                        index_boundary_mix_values);
          }
        if (m_program[m_current_program].m_color_boundary_mix != -1)
          {
            gl::Uniform(m_program[m_current_program].m_color_boundary_mix, m_color_boundary_mix_value);
          }
        if (m_program[m_current_program].m_filtered_lookup != -1)
          {
            gl::Uniform(m_program[m_current_program].m_filtered_lookup, m_filtered_lookup);
          }
        if (m_program[m_current_program].m_uniform_image_num_lookups != -1)
          {
            unsigned int num_lookups(m_image_handles[m_current_image]->number_index_lookups());
            gl::Uniform(m_program[m_current_program].m_uniform_image_num_lookups, num_lookups);
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
    unsigned int old_program(m_current_program);

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
            {
              cycle_value(m_current_program, ev.key.keysym.mod & (KMOD_SHIFT|KMOD_CTRL|KMOD_ALT), number_draw_types);
              switch(m_current_program)
                {
                case draw_image_on_atlas:
                  std::cout << "Set to draw image \"" << m_image_names[m_current_image] << "\"\n";
                  break;
                case draw_atlas:
                  std::cout << "Set to draw atlas\n";
                  break;
                }
            }
            break;
          case SDLK_i:
            if (m_current_program == draw_image_on_atlas)
              {
                cycle_value(m_current_image, ev.key.keysym.mod & (KMOD_SHIFT|KMOD_CTRL|KMOD_ALT), m_image_handles.size());
                std::cout << "Set to draw image \"" << m_image_names[m_current_image] << "\"\n";

                vec2 image_size(m_image_handles[m_current_image]->dimensions());
                vecN<vec2, 2> corner(shader_coords(m_image_handles[m_current_image]));
                float layer(m_image_handles[m_current_image]->master_index_tile().z());
                float image_index_attribs[] =
                  {
                    0,              0,              corner[0].x(), corner[0].y(), layer,
                    0,              image_size.y(), corner[0].x(), corner[1].y(), layer,
                    image_size.x(), image_size.y(), corner[1].x(), corner[1].y(), layer,
                    image_size.x(), 0,              corner[1].x(), corner[0].y(), layer
                  };
                glBindBuffer(GL_ARRAY_BUFFER, m_program[draw_image_on_atlas].m_vbo);
                glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(image_index_attribs), image_index_attribs);
              }
            break;

          case SDLK_f:
            if (m_current_program == draw_image_on_atlas)
              {
                m_filtered_lookup = 1.0f - m_filtered_lookup;
                if (m_filtered_lookup > 0.5)
                  {
                    std::cout << "Filter set to bilinear filtering.\n";
                  }
                else
                  {
                    std::cout << "Filter set to nearest filtering.\n";
                  }
              }
            break;

          case SDLK_0:
            m_color_boundary_mix_value = 0.5f - m_color_boundary_mix_value;
            if (m_color_boundary_mix_value > 0.25f)
              {
                std::cout << "Set to show tile boundaries.\n";
              }
            else
              {
                std::cout << "Set to hide tile boundaries.\n";
              }
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
            if (m_current_program == draw_image_on_atlas)
              {
                unsigned int idx(ev.key.keysym.sym - SDLK_1);
                if (idx < m_index_boundary_mix_values.size())
                  {
                    m_index_boundary_mix_values[idx] = 0.5f - m_index_boundary_mix_values[idx];
                    if (m_index_boundary_mix_values[idx] > 0.25f)
                      {
                        std::cout << "Set to show level " << idx << " tile boundaries.\n";
                      }
                    else
                      {
                        std::cout << "Set to hide level " << idx << " tile boundaries.\n";
                      }
                  }
              }
            break;
          }

        if (old_program != m_current_program)
          {
            bind_textures();
            std::cout << "Current draw: "
                      << m_program[m_current_program].m_label
                      << " (id=" << m_current_program << ")\n";
          }
        break;

      case SDL_WINDOWEVENT:
        if (ev.window.event == SDL_WINDOWEVENT_RESIZED)
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
    FASTUIDRAWassert(m_sampler != 0);

    glSamplerParameteri(m_sampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glSamplerParameteri(m_sampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    gl::ImageAtlasGL::params params;
    int max_layers(0);

    max_layers = fastuidraw::gl::context_get<GLint>(GL_MAX_ARRAY_TEXTURE_LAYERS);
    if (max_layers < m_num_color_layers.m_value)
      {
	std::cout << "num_color_layers exceeds max number texture layers (" << max_layers
		  << "), num_color_layers set to that value.\n";
	m_num_color_layers.m_value = max_layers;
      }

    params
      .log2_color_tile_size(m_log2_color_tile_size.m_value)
      .log2_num_color_tiles_per_row_per_col(m_log2_num_color_tiles_per_row_per_col.m_value)
      .num_color_layers(m_num_color_layers.m_value)
      .log2_index_tile_size(m_log2_index_tile_size.m_value)
      .log2_num_index_tiles_per_row_per_col(m_log2_num_index_tiles_per_row_per_col.m_value)
      .num_index_layers(m_num_index_layers.m_value)
      .delayed(false);

    m_atlas = FASTUIDRAWnew gl::ImageAtlasGL(params);
    m_slack.m_value = std::max(0, m_slack.m_value);

    for(command_line_list::iterator iter = m_images.begin(); iter != m_images.end(); ++iter)
      {
        add_images(*iter);
      }

    if (m_image_handles.empty())
      {
        std::vector<u8vec4> image_data;
        ivec2 image_size;

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
        m_image_handles.push_back(Image::create(m_atlas, image_size.x(), image_size.y(),
                                                cast_c_array(image_data), m_slack.m_value));
        m_image_names.push_back("Simple Checkerboard");
      }

  }

  void
  build_programs(void)
  {
    reference_counted_ptr<gl::Program> pr;

    {
      pr = FASTUIDRAWnew gl::Program(glsl::ShaderSource()
                                     .specify_version(gl::Shader::default_shader_version())
                                     .add_source("layer_texture_blit.vert.glsl.resource_string",
                                                 glsl::ShaderSource::from_resource),
                                     glsl::ShaderSource()
                                     .specify_version(gl::Shader::default_shader_version())
                                     .add_source("detect_boundary.glsl.resource_string",
                                                 glsl::ShaderSource::from_resource)
                                     .add_source("layer_texture_blit.frag.glsl.resource_string",
                                                glsl::ShaderSource::from_resource),
                                     gl::PreLinkActionArray()
                                     .add_binding("attrib_pos", 0),
                                     gl::ProgramInitializerArray()
                                     .add_sampler_initializer("image", 0)
                                     .add_uniform_initializer<float>("tile_size",
                                                                     float(m_atlas->color_tile_size())) );
      m_program[draw_atlas].set("draw_atlas", pr);
    }

    {
      unsigned int max_num_look_ups(1);
      for(std::vector<reference_counted_ptr<Image> >::iterator iter = m_image_handles.begin(),
            end = m_image_handles.end(); iter != end; ++iter)
        {
          max_num_look_ups = std::max(max_num_look_ups, (*iter)->number_index_lookups());
        }
      m_index_boundary_mix_values.resize(max_num_look_ups + 1, 0.0f);

      glsl::ShaderSource glsl_compute_coord;
      glsl_compute_coord = glsl::code::image_atlas_compute_coord("compute_atlas_coord", "indexAtlas",
                                                                 m_atlas->index_tile_size(),
                                                                 m_atlas->color_tile_size());

      pr = FASTUIDRAWnew gl::Program(glsl::ShaderSource()
                                     .specify_version(gl::Shader::default_shader_version())
                                     .add_source("atlas_image_blit.vert.glsl.resource_string",
                                                 glsl::ShaderSource::from_resource),
                                     glsl::ShaderSource()
                                     .specify_version(gl::Shader::default_shader_version())
                                     .add_macro("MAX_IMAGE_NUM_LOOKUPS", max_num_look_ups)
                                     .add_source("detect_boundary.glsl.resource_string",
                                                 glsl::ShaderSource::from_resource)
                                     .add_source("atlas_image_blit.frag.glsl.resource_string",
                                                 glsl::ShaderSource::from_resource)
                                     .add_source(glsl_compute_coord),
                                     gl::PreLinkActionArray()
                                     .add_binding("attrib_pos", attrib_pos_vertex_attrib)
                                     .add_binding("attrib_image_shader_coord", index_coord_vertex_attrib),
                                     gl::ProgramInitializerArray()
                                     .add_sampler_initializer("imageAtlas", color_atlas_texture_unit)
                                     .add_sampler_initializer("indexAtlas", index_atlas_texture_unit)
                                     .add_uniform_initializer<float>("color_tile_size",
                                                                     float(m_atlas->color_tile_size() - 2 * m_slack.m_value))
                                     .add_uniform_initializer<float>("index_tile_size", float(m_atlas->index_tile_size()))
                                     .add_uniform_initializer<unsigned int>("uniform_image_num_lookups",
                                                                            m_image_handles.front()->number_index_lookups())
                                     .add_uniform_initializer<unsigned int>("image_slack", m_slack.m_value)
                                     .add_uniform_initializer<vec3>("imageAtlasDims",
                                                                    vec3(m_atlas->color_store()->dimensions())) );

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
    FASTUIDRAWassert(m_ibo != 0);

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
                            nullptr);
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ibo);
    }


    if (m_program[draw_image_on_atlas].m_vao)
      {
        glBindVertexArray(m_program[draw_image_on_atlas].m_vao);

        vec2 image_size(m_image_handles.front()->dimensions());
        vecN<vec2, 2> corner(shader_coords(m_image_handles.front()));
        float layer(m_image_handles.front()->master_index_tile().z());
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

  static
  vecN<vec2, 2>
  shader_coords(reference_counted_ptr<Image> image)
  {
    FASTUIDRAWassert(image->number_index_lookups() > 0);
    ivec2 master_index_tile(image->master_index_tile());
    vec2 wh(image->master_index_tile_dims());
    float f(image->atlas()->index_tile_size());
    vec2 fmaster_index_tile(master_index_tile);
    vec2 c0(f * fmaster_index_tile);
    return vecN<vec2, 2>(c0, c0 + wh);
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
    set(const std::string &label, reference_counted_ptr<gl::Program> pr)
    {
      FASTUIDRAWassert(pr);
      m_pr = pr;
      m_label = label;
      m_pvm = m_pr->uniform_location("pvm");
      m_scale = m_pr->uniform_location("scale");
      m_translate = m_pr->uniform_location("translate");
      m_layer = m_pr->uniform_location("layer");
      m_index_boundary_mix = m_pr->uniform_location("index_boundary_mix");
      m_color_boundary_mix = m_pr->uniform_location("color_boundary_mix");
      m_filtered_lookup = m_pr->uniform_location("filtered_lookup");
      m_uniform_image_num_lookups = m_pr->uniform_location("uniform_image_num_lookups");

      glGenVertexArrays(1, &m_vao);
      FASTUIDRAWassert(m_vao != 0);

      glGenBuffers(1, &m_vbo);
      FASTUIDRAWassert(m_vbo != 0);
    }

    reference_counted_ptr<gl::Program> m_pr;
    GLint m_pvm;
    GLint m_scale;
    GLint m_translate;
    GLint m_layer;
    GLuint m_vao;
    GLuint m_vbo;
    GLint m_index_boundary_mix;
    GLint m_color_boundary_mix;
    GLint m_filtered_lookup;
    GLint m_uniform_image_num_lookups;
    std::string m_label;
    PanZoomTrackerSDLEvent m_zoomer;
  };

  command_line_list m_images;
  command_line_argument_value<bool> m_print_loaded_image_list;
  command_line_argument_value<int> m_slack;
  command_line_argument_value<int> m_log2_color_tile_size, m_log2_num_color_tiles_per_row_per_col;
  command_line_argument_value<int> m_num_color_layers;
  command_line_argument_value<int> m_log2_index_tile_size, m_log2_num_index_tiles_per_row_per_col;
  command_line_argument_value<int> m_num_index_layers;

  float m_color_boundary_mix_value;
  std::vector<float> m_index_boundary_mix_values;
  float m_filtered_lookup;

  reference_counted_ptr<gl::ImageAtlasGL> m_atlas;
  std::vector<reference_counted_ptr<Image> > m_image_handles;
  std::vector<std::string> m_image_names;
  vecN<per_program, number_draw_types> m_program;
  unsigned int m_current_program;
  unsigned int m_current_image;

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
