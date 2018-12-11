#include <fastuidraw/gl_backend/colorstop_atlas_gl.hpp>
#include <fastuidraw/gl_backend/opengl_trait.hpp>
#include <fastuidraw/gl_backend/gl_get.hpp>
#include <cstdlib>
#include "sdl_demo.hpp"
#include "colorstop_command_line.hpp"

using namespace fastuidraw;
typedef color_stop_arguments::hoard colorstop_data_hoard;


class gradient_test:public sdl_demo
{
public:
  gradient_test(void):
    sdl_demo("gradient-test"),
    m_color_stop_atlas_width(1024, "atlas_width",
                             "width for color stop atlas", *this),
    m_color_stop_atlas_layers(1024, "atlas_layers",
                              "number of layers for the color stop atlas",
                              *this),
    m_color_stop_args(*this),
    m_stress(false, "stress_color_stop_atlas",
             "If true create and delete multiple color stops "
             "to test ColorStopAtlas allocation and deletion", *this),
    m_active_color_stop(0),
    m_ibo(0),
    m_bo(0),
    m_vao(0),
    m_pts_bo(0),
    m_pts_vao(0),
    m_p0(-1.0f, -1.0f),
    m_p1(1.0f, 1.0f),
    m_draw_gradient_points(true)
  {
    std::cout << "Controls:\n"
              << "\tn: next color stop sequence\n"
              << "\tp: previous color stop sequence\n"
              << "\td: toggle drawing gradient points\n"
              << "\tLeft Mouse Button: set p0(starting position bottom left) {drawn black with white inside} of linear gradient\n"
              << "\tRight Mouse Button: set p1(starting position top right) {drawn white with black inside} of linear gradient\n";
  }

  ~gradient_test()
  {
    if (m_vao != 0)
      {
        fastuidraw_glDeleteVertexArrays(1, &m_vao);
      }

    if (m_bo != 0)
      {
        fastuidraw_glDeleteBuffers(1, &m_bo);
      }

    if (m_ibo != 0)
      {
        fastuidraw_glDeleteBuffers(1, &m_ibo);
      }

    if (m_pts_bo != 0)
      {
        fastuidraw_glDeleteBuffers(1, &m_pts_bo);
      }

    if (m_pts_vao != 0)
      {
        fastuidraw_glDeleteVertexArrays(1, &m_pts_vao);
      }

  }

protected:

  void
  init_gl(int w, int h)
  {
    (void)w;
    (void)h;

    create_colorstops_and_atlas();
    set_attributes_indices();
    build_programs();
  }

  void
  draw_frame(void)
  {
    fastuidraw_glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    {
      fastuidraw_glBindVertexArray(m_vao);
      fastuidraw_glBindTexture(m_atlas->texture_bind_target(), m_atlas->texture());
      const reference_counted_ptr<ColorStopSequenceOnAtlas> cst(m_color_stops[m_active_color_stop].second);
      m_program->use_program();
      gl::Uniform(m_p0_loc, m_p0);
      gl::Uniform(m_p1_loc, m_p1);
      gl::Uniform(m_atlasLayer_loc, float(cst->texel_location().y()));
      gl::Uniform(m_gradientStart_loc, float(cst->texel_location().x()));
      gl::Uniform(m_gradientLength_loc, float(cst->width()));
      fastuidraw_glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, 0);
    }


    if (m_draw_gradient_points)
      {
        float width(20.0f);

        fastuidraw_glBindVertexArray(m_pts_vao);
        m_draw_pts->use_program();

        #ifndef FASTUIDRAW_GL_USE_GLES
          {
            fastuidraw_glEnable(GL_PROGRAM_POINT_SIZE);
          }
        #endif

        fastuidraw_glDisable(GL_DEPTH_TEST);

        draw_pt(m_p0, width, vec4(0.0f, 0.0f, 0.0f, 1.0f));
        draw_pt(m_p0, width * 0.5f, vec4(1.0f, 1.0f, 1.0f, 1.0f));

        draw_pt(m_p1, width, vec4(1.0f, 1.0f, 1.0f, 1.0f));
        draw_pt(m_p1, width * 0.5f, vec4(0.0f, 0.0f, 0.0f, 1.0f));
      }
  }


  void
  draw_pt(const vec2 &pt, float size, const vec4 &color)
  {
    gl::Uniform(m_pts_pos_loc, vec3(pt.x(), pt.y(), size));
    gl::Uniform(m_pts_color_loc, color);
    fastuidraw_glDrawArrays(GL_POINTS, 0, 1);
  }

  void
  handle_event(const SDL_Event &ev)
  {
    switch(ev.type)
      {
      case SDL_WINDOWEVENT:
        if (ev.window.event == SDL_WINDOWEVENT_RESIZED)
          {
            fastuidraw_glViewport(0, 0, ev.window.data1, ev.window.data2);
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

          case SDLK_n:
            ++m_active_color_stop;
            if (m_active_color_stop >= m_color_stops.size())
              {
                m_active_color_stop = 0;
              }
            std::cout << "Active ColorStop: "
                      << m_color_stops[m_active_color_stop].first << "\n";
            break;

          case SDLK_p:
            m_active_color_stop = (m_active_color_stop == 0) ?
              (m_color_stops.size() - 1):
              (m_active_color_stop - 1);
            std::cout << "Active ColorStop: "
                      << m_color_stops[m_active_color_stop].first << "\n";
            break;

          case SDLK_d:
            m_draw_gradient_points = !m_draw_gradient_points;
            break;
          }
        break;

      case SDL_MOUSEMOTION:
        {
          ivec2 c(ev.motion.x + ev.motion.xrel,
                  ev.motion.y + ev.motion.yrel);

          if (ev.motion.state & SDL_BUTTON(SDL_BUTTON_LEFT))
            {
              m_p0 = get_normalized_device_coords(c);
            }
          else if (ev.motion.state & SDL_BUTTON(SDL_BUTTON_RIGHT))
            {
              m_p1 = get_normalized_device_coords(c);
            }
        }
        break;

      case SDL_MOUSEBUTTONDOWN:
        {
          ivec2 c(ev.button.x, ev.button.y);
          if (ev.button.button == SDL_BUTTON_LEFT)
            {
              m_p0 = get_normalized_device_coords(c);
            }
          else if (ev.button.button == SDL_BUTTON_RIGHT)
            {
              m_p1 = get_normalized_device_coords(c);
            }
        }
        break;

      }
  }

private:

  vec2
  get_normalized_device_coords(const ivec2 &c)
  {
    vec2 r01;

    r01 = vec2(c) / vec2(dimensions());
    r01.y() = 1.0f - r01.y();

    return 2.0f * r01 - vec2(1.0f, 1.0f);
  }

  void
  create_colorstops_and_atlas(void)
  {
    gl::ColorStopAtlasGL::params params;
    int max_layers(0);

    max_layers = fastuidraw::gl::context_get<GLint>(GL_MAX_ARRAY_TEXTURE_LAYERS);
    if (max_layers < m_color_stop_atlas_layers.value())
      {
        std::cout << "atlas_layers exceeds max number texture layers (" << max_layers
                  << ") atlas_layers set to that value.\n";
        m_color_stop_atlas_layers.value() = max_layers;
      }

    params
      .width(m_color_stop_atlas_width.value())
      .num_layers(m_color_stop_atlas_layers.value())
      .delayed(false);

    m_atlas = FASTUIDRAWnew gl::ColorStopAtlasGL(params);

    if (m_color_stop_args.values().empty())
      {
        c_array<const ColorStop> src;

        m_color_stop_args.fetch("default-32px").m_stops.add(ColorStop(u8vec4(255, 255, 255, 255), 0.00f));
        m_color_stop_args.fetch("default-32px").m_stops.add(ColorStop(u8vec4(0,     0, 255, 255), 0.25f));
        m_color_stop_args.fetch("default-32px").m_stops.add(ColorStop(u8vec4(0,   255,   0, 255), 0.75f));
        m_color_stop_args.fetch("default-32px").m_stops.add(ColorStop(u8vec4(255,   0, 255, 255), 1.0f));
        m_color_stop_args.fetch("default-32px").m_discretization = 32;

        src = m_color_stop_args.fetch("default-32px").m_stops.values();
        m_color_stop_args.fetch("default-16px").m_stops.add(src.begin(), src.end());
        m_color_stop_args.fetch("default-16px").m_discretization = 16;
        m_color_stop_args.fetch("default-8px").m_stops.add(src.begin(), src.end());
        m_color_stop_args.fetch("default-8px").m_discretization = 8;

        m_color_stop_args.fetch("default2-32px").m_stops.add(ColorStop(u8vec4(0,   255, 255, 255), 0.00f));
        m_color_stop_args.fetch("default2-32px").m_stops.add(ColorStop(u8vec4(0,     0, 255, 255), 0.25f));
        m_color_stop_args.fetch("default2-32px").m_stops.add(ColorStop(u8vec4(255,   0,   0, 255), 0.75f));
        m_color_stop_args.fetch("default2-32px").m_stops.add(ColorStop(u8vec4(  0, 255,   0, 255), 1.0f));
        m_color_stop_args.fetch("default2-32px").m_discretization = 32;

        src = m_color_stop_args.fetch("default2-32px").m_stops.values();
        m_color_stop_args.fetch("default2-16px").m_stops.add(src.begin(), src.end());
        m_color_stop_args.fetch("default2-16px").m_discretization = 16;
        m_color_stop_args.fetch("default2-8px").m_stops.add(src.begin(), src.end());
        m_color_stop_args.fetch("default2-8px").m_discretization = 8;
      }


    for(colorstop_data_hoard::const_iterator
          iter = m_color_stop_args.values().begin(),
          end = m_color_stop_args.values().end();
        iter != end; ++iter)
      {
        reference_counted_ptr<ColorStopSequenceOnAtlas> h;
        reference_counted_ptr<ColorStopSequenceOnAtlas> temp1, temp2;

        if (m_stress.value())
          {
            int sz1, sz2;
            sz1 = std::max(1, m_color_stop_atlas_width.value() / 2);
            sz2 = std::max(1, sz1 / 2);
            temp1 = FASTUIDRAWnew ColorStopSequenceOnAtlas(iter->second->m_stops, m_atlas, sz1);
            temp2 = FASTUIDRAWnew ColorStopSequenceOnAtlas(iter->second->m_stops,  m_atlas, sz2);
          }


        h =  FASTUIDRAWnew ColorStopSequenceOnAtlas(iter->second->m_stops, m_atlas,
                                                   iter->second->m_discretization);

        m_color_stops.push_back(named_color_stop(iter->first, h));

      }
  }

  void
  set_attributes_indices(void)
  {
    fastuidraw_glGenVertexArrays(1, &m_pts_vao);
    FASTUIDRAWassert(m_pts_vao != 0);
    fastuidraw_glBindVertexArray(m_pts_vao);

    float value[] = { 1.0f };

    fastuidraw_glGenBuffers(1, &m_pts_bo);
    fastuidraw_glBindBuffer(GL_ARRAY_BUFFER, m_pts_bo);
    fastuidraw_glBufferData(GL_ARRAY_BUFFER, sizeof(value), value, GL_STATIC_DRAW);
    fastuidraw_glEnableVertexAttribArray(0);
    fastuidraw_glVertexAttribPointer(0,
                                     gl::opengl_trait<float>::count,
                                     gl::opengl_trait<float>::type,
                                     GL_FALSE,
                                     gl::opengl_trait<float>::stride,
                                     nullptr);

    fastuidraw_glGenVertexArrays(1, &m_vao);
    FASTUIDRAWassert(m_vao != 0);
    fastuidraw_glBindVertexArray(m_vao);

    fastuidraw_glGenBuffers(1, &m_ibo);
    FASTUIDRAWassert(m_ibo != 0);

    GLushort indices[]=
      {
        0, 1, 2,
        0, 2, 3
      };
    fastuidraw_glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ibo);
    fastuidraw_glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    fastuidraw_glGenBuffers(1, &m_bo);
    FASTUIDRAWassert(m_bo !=0);

    vec2 positions[]=
      {
        vec2(-1.0f, -1.0f),
        vec2(-1.0f, +1.0f),
        vec2(+1.0f, +1.0f),
        vec2(+1.0f, -1.0f)
      };

    fastuidraw_glBindBuffer(GL_ARRAY_BUFFER, m_bo);
    fastuidraw_glBufferData(GL_ARRAY_BUFFER, sizeof(positions), positions, GL_STATIC_DRAW);

    fastuidraw_glEnableVertexAttribArray(0);
    fastuidraw_glVertexAttribPointer(0,
                                     gl::opengl_trait<vec2>::count,
                                     gl::opengl_trait<vec2>::type,
                                     GL_FALSE,
                                     gl::opengl_trait<vec2>::stride,
                                     nullptr);
  }

  void
  build_programs(void)
  {
    m_draw_pts = FASTUIDRAWnew gl::Program(glsl::ShaderSource()
                                           .specify_version(gl::Shader::default_shader_version())
                                           .add_source("draw_pt.vert.glsl.resource_string",
                                                       glsl::ShaderSource::from_resource),

                                           glsl::ShaderSource()
                                           .specify_version(gl::Shader::default_shader_version())
                                           .add_source("draw_pt.frag.glsl.resource_string",
                                                       glsl::ShaderSource::from_resource),

                                           gl::PreLinkActionArray()
                                           .add_binding("attrib_fake", 0));

    m_pts_color_loc = m_draw_pts->uniform_location("color");
    m_pts_pos_loc = m_draw_pts->uniform_location("pos_size");

    m_program = FASTUIDRAWnew gl::Program(glsl::ShaderSource()
                                          .specify_version(gl::Shader::default_shader_version())
                                          .add_source("linear_gradient.vert.glsl.resource_string",
                                                      glsl::ShaderSource::from_resource),

                                          glsl::ShaderSource()
                                          .specify_version(gl::Shader::default_shader_version())
                                          .add_source("linear_gradient.frag.glsl.resource_string",
                                                      glsl::ShaderSource::from_resource),

                                          gl::PreLinkActionArray()
                                          .add_binding("attrib_pos", 0),

                                          gl::ProgramInitializerArray()
                                          .add_sampler_initializer("gradientAtlas", 0)
                                          .add_uniform_initializer<float>("gradientAtlasWidth",
                                                                          m_atlas->backing_store()->dimensions().x())
                                         );
#define GETLOC(x) do { \
      m_##x##_loc = m_program->uniform_location(#x);    \
      FASTUIDRAWassert( m_##x##_loc != -1);                       \
    } while(0)

    GETLOC(p0);
    GETLOC(p1);
    GETLOC(atlasLayer);
    GETLOC(gradientStart);
    GETLOC(gradientLength);


  }


  typedef std::pair<std::string, reference_counted_ptr<ColorStopSequenceOnAtlas> > named_color_stop;

  command_line_argument_value<int> m_color_stop_atlas_width;
  command_line_argument_value<int> m_color_stop_atlas_layers;
  color_stop_arguments m_color_stop_args;
  command_line_argument_value<bool> m_stress;
  reference_counted_ptr<gl::ColorStopAtlasGL> m_atlas;
  std::vector<named_color_stop> m_color_stops;

  unsigned int m_active_color_stop;
  GLuint m_ibo, m_bo, m_vao;
  reference_counted_ptr<gl::Program> m_program;

  GLuint m_pts_bo, m_pts_vao;
  GLint m_pts_color_loc, m_pts_pos_loc;
  reference_counted_ptr<gl::Program> m_draw_pts;


  vec2 m_p0, m_p1;
  bool m_draw_gradient_points;

  GLint m_p0_loc, m_p1_loc;
  GLint m_atlasLayer_loc;
  GLint m_gradientStart_loc, m_gradientLength_loc;

};


int
main(int argc, char **argv)
{
  gradient_test G;
  return G.main(argc, argv);
}
