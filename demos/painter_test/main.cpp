#include <iostream>
#include <sstream>
#include <fstream>
#include <bitset>

#include <fastuidraw/util/util.hpp>
#include <fastuidraw/painter/painter.hpp>
#include <fastuidraw/painter/backend/painter_header.hpp>
#include <fastuidraw/painter/attribute_data/stroked_point.hpp>
#include <fastuidraw/painter/attribute_data/arc_stroked_point.hpp>

#include "sdl_painter_demo.hpp"

using namespace fastuidraw;

typedef std::bitset<32> bitset;

c_string
shader_extension(GLenum shader_type)
{
  switch (shader_type)
    {
    case GL_FRAGMENT_SHADER: return "frag";
    case GL_VERTEX_SHADER: return "vert";
    case GL_GEOMETRY_SHADER: return "geom";
    case GL_TESS_CONTROL_SHADER: return "tesc";
    case GL_TESS_EVALUATION_SHADER: return "tese";
    case GL_COMPUTE_SHADER: return "comp";
    default: return "unknown";
    }
}

c_string
label_sampler_type(GLenum type)
{
#define EASY(X) case X: return #X

#ifndef FASTUIDRAW_GL_USE_GLES
#define SUFFIX_1D(X)                             \
  EASY(X##1D_ARRAY);                             \
  EASY(X##1D);                                   \
  EASY(X##2D_RECT);
#else
#define SUFFIX_1D(X)  ;
#endif

#define SUFFIX_MS(X)                             \
  EASY(X##2D_MULTISAMPLE);                       \
  EASY(X##2D_MULTISAMPLE_ARRAY);

#define SUFFIX(X)                                \
  SUFFIX_1D(X);                                  \
  EASY(X##2D);                                   \
  EASY(X##3D);                                   \
  EASY(X##CUBE);                                 \
  EASY(X##2D_ARRAY);                             \
  EASY(X##BUFFER);

#define PREFIX(X)                                      \
  SUFFIX(GL_##X##_);                                   \
  SUFFIX(GL_INT_##X##_);                               \
  SUFFIX(GL_UNSIGNED_INT_##X##_);                      \

#define PREFIX_MS(X)                                      \
  SUFFIX_MS(GL_##X##_);                                   \
  SUFFIX_MS(GL_INT_##X##_);                               \
  SUFFIX_MS(GL_UNSIGNED_INT_##X##_);                      \

  switch(type)
    {
      PREFIX(SAMPLER);
      PREFIX_MS(SAMPLER);
      PREFIX(IMAGE);
#ifndef FASTUIDRAW_GL_USE_GLES
      PREFIX_MS(IMAGE);
#endif
    }

  return nullptr;

#undef PREFIX_MS
#undef SUFFIX_MS
#undef PREFIX
#undef SUFFIX
#undef SUFFIX_1D
#undef EASY
}

class painter_test:public sdl_painter_demo
{
public:
  painter_test(void):
    sdl_painter_demo("", true)
  {}

protected:

  void
  log_helper(const reference_counted_ptr<gl::Program> &pr,
             c_string prefix_1, c_string prefix_2, GLenum shader_type)
  {
    unsigned int cnt;
    c_string label;

    cnt = pr->num_shaders(shader_type);
    if (cnt == 0)
      {
        return;
      }

    label = gl::Shader::gl_shader_type_label(shader_type);
    std::cout << label << "'s written to:\n";

    for(unsigned int i = 0; i < cnt; ++i)
      {
        std::ostringstream name, name_glsl, name_log;

        name << "painter." << prefix_1 << ".";
        if (prefix_2)
          {
            name << prefix_2 << ".";
          }
        name << i << "." << shader_extension(shader_type);
        name_glsl << name.str() << ".glsl";
        name_log << name.str() << ".log";

        std::ofstream file(name_glsl.str().c_str());
        file << pr->shader_src_code(shader_type, i);
        std::cout << "\tSource #" << i << ":" << name.str() << "\n";

        std::ofstream file_log(name_log.str().c_str());
        file_log << pr->shader_compile_log(shader_type, i);
        std::cout << "\tCompile Log #" << i << ":" << name_log.str() << "\n";
      }
  }

  void
  log_program(const reference_counted_ptr<gl::Program> &pr,
              c_string prefix_1, c_string prefix_2)
  {
    if (!pr)
      {
        return;
      }
    else
      {
        std::ostringstream name;

        name << "painter." << prefix_1;
        if (prefix_2)
          {
            name << "." << prefix_2;
          }
        name << ".program.log";

        std::ofstream file(name.str().c_str());

        file << pr->log();

        /* query the program for the values of all texture types */
        if (pr->link_success())
          {
            pr->use_program();
            gl::Program::block_info default_block(pr->default_uniform_block());
            for (unsigned int v = 0, endv = default_block.number_variables(); v < endv; ++v)
              {
                gl::Program::shader_variable_info sh(default_block.variable(v));
                const char *sampler_type;

                sampler_type = label_sampler_type(sh.glsl_type());
                if (sampler_type)
                  {
                    file << sh.name() << " of type " << sampler_type << "\n";
                    for (unsigned c = 0, endc = sh.count(); c < endc; ++c)
                      {
                        GLint value;
                        fastuidraw_glGetUniformiv(pr->name(), sh.location(c), &value);
                        file << "\t[" << c << "] bound at " << value << " binding point\n";
                      }
                  }
              }
          }

        std::cout << "\n\nProgram Log and contents written to "
                  << name.str() << "\n";
      }

    log_helper(pr, prefix_1, prefix_2, GL_VERTEX_SHADER);
    log_helper(pr, prefix_1, prefix_2, GL_FRAGMENT_SHADER);
    log_helper(pr, prefix_1, prefix_2, GL_GEOMETRY_SHADER);
    log_helper(pr, prefix_1, prefix_2, GL_TESS_EVALUATION_SHADER);
    log_helper(pr, prefix_1, prefix_2, GL_TESS_CONTROL_SHADER);

    if (pr->link_success())
      {
        std::cout << "Link success\n";
        pr->use_program();
      }
    else
      {
        std::cout << "Link Failed\n";
      }
  }

  void
  derived_init(int, int)
  {
    fastuidraw::c_string program_labels[gl::PainterEngineGL::number_program_types] =
      {
        [gl::PainterEngineGL::program_all] = "program_all",
        [gl::PainterEngineGL::program_without_discard] = "program_without_discard",
        [gl::PainterEngineGL::program_with_discard] = "program_with_discard",
      };

    fastuidraw::c_string blend_labels[PainterBlendShader::number_types] =
      {
        [PainterBlendShader::single_src] = "single_src",
        [PainterBlendShader::dual_src] = "dual_src",
        [PainterBlendShader::framebuffer_fetch] = "framebuffer_fetch",
      };

    for (int pr = 0; pr < gl::PainterEngineGL::number_program_types; ++pr)
      {
        for (int b = 0; b < PainterBlendShader::number_types; ++b)
          {
            reference_counted_ptr<gl::Program> program;
            program = m_backend->program(static_cast<enum gl::PainterEngineGL::program_type_t>(pr),
                                         static_cast<enum PainterBlendShader::shader_type>(b));
            log_program(program, program_labels[pr], blend_labels[b]);
          }
      }
    log_program(m_backend->program_deferred_coverage_buffer(), "deferred_coverage_buffer", nullptr);

    std::cout << "\nUseful command to see shader after pre-processor:\n"
              << "\tsed 's/#version/@version/g' file.glsl | sed 's/#extension/@extension/g'"
              << " | cpp | grep -v \"#\" | sed '/^\\s*$/d'"
              << " | sed 's/@version/#version/g' | sed 's/@extension/#extension/g'\n";
  }

  void
  draw_frame(void)
  {
    end_demo(0);
  }

  void
  handle_event(const SDL_Event &ev)
  {
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
          }
        break;
      };
  }

private:

};

int
main(int argc, char **argv)
{
  FASTUIDRAWassert(StrokedPoint::number_offset_types < FASTUIDRAW_MAX_VALUE_FROM_NUM_BITS(StrokedPoint::offset_type_num_bits));
  std::cout << std::setw(45) << "header_size = " << PainterHeader::header_size << "\n"
            << std::setw(45) << "clip_equations_data_size = " << PainterClipEquations::clip_data_size << "\n"
            << std::setw(45) << "item_matrix_data_size = " << PainterItemMatrix::matrix_data_size << "\n"
            << std::setw(45) << "brush image_data_size = " << PainterBrush::image_data_size << "\n"
            << std::setw(45) << "brush linear_gradient_data_size = " << PainterBrush::linear_gradient_data_size << "\n"
            << std::setw(45) << "brush radial_gradient_data_size = " << PainterBrush::radial_gradient_data_size << "\n"
            << std::setw(45) << "brush repeat_window_data_size = " << PainterBrush::repeat_window_data_size << "\n"
            << std::setw(45) << "brush transformation_matrix_data_size = " << PainterBrush::transformation_matrix_data_size << "\n"
            << std::setw(45) << "brush transformation_translation_data_size = " << PainterBrush::transformation_translation_data_size << "\n"
            << "\n"

            << std::setw(45) << "brush image_mask = " << bitset(PainterBrush::image_mask) << "\n"
            << std::setw(45) << "brush image_mipmap_mask = " << bitset(PainterBrush::image_mipmap_mask) << "\n"
            << std::setw(45) << "brush gradient_type_mask = " << bitset(PainterBrush::gradient_type_mask) << "\n"
            << std::setw(45) << "brush gradient_spread_type_mask = " << bitset(PainterBrush::gradient_spread_type_mask) << "\n"
            << std::setw(45) << "brush repeat_window_mask = " << bitset(PainterBrush::repeat_window_mask) << "\n"
            << std::setw(45) << "brush repeat_window_x_spread_type_mask = " << bitset(PainterBrush::repeat_window_x_spread_type_mask) << "\n"
            << std::setw(45) << "brush repeat_window_y_spread_type_mask = " << bitset(PainterBrush::repeat_window_y_spread_type_mask) << "\n"
            << std::setw(45) << "brush transformation_translation_mask = " << bitset(PainterBrush::transformation_translation_mask) << "\n"
            << std::setw(45) << "brush transformation_matrix_mask = " << bitset(PainterBrush::transformation_matrix_mask) << "\n"
            << std::setw(45) << "brush image_type_mask = " << bitset(PainterBrush::image_type_mask) << "\n"
            << std::setw(45) << "brush image_format_mask = " << bitset(PainterBrush::image_format_mask) << "\n"
            << std::setw(45) << "brush number_feature_bits = " << PainterBrush::number_feature_bits << "\n"

            << std::setw(45) << "stroked_number_offset_types = " << StrokedPoint::number_offset_types << "\n"
            << std::setw(45) << "stroked_offset_type_bit0 = " << StrokedPoint::offset_type_bit0 << "\n"
            << std::setw(45) << "stroked_offset_type_num_bits = " << StrokedPoint::offset_type_num_bits << "\n"
            << std::setw(45) << "stroked_boundary_bit = " << StrokedPoint::boundary_bit << "\n"
            << std::setw(45) << "stroked_depth_bit0 = " << StrokedPoint::depth_bit0 << "\n"
            << std::setw(45) << "stroked_depth_num_bits = " << StrokedPoint::depth_num_bits << "\n"
            << std::setw(45) << "stroked_join_bit = " << StrokedPoint::join_bit << "\n"
            << std::setw(45) << "stroked_number_common_bits = " << StrokedPoint::number_common_bits << "\n"
            << std::setw(45) << "stroked_normal0_y_sign_bit = " << StrokedPoint::normal0_y_sign_bit << "\n"
            << std::setw(45) << "stroked_normal1_y_sign_bit = " << StrokedPoint::normal1_y_sign_bit << "\n"
            << std::setw(45) << "stroked_sin_sign_bit = " << StrokedPoint::sin_sign_bit << "\n"
            << std::setw(45) << "stroked_adjustable_cap_ending_bit = " << StrokedPoint::adjustable_cap_ending_bit << "\n"
            << std::setw(45) << "stroked_bevel_edge_bit = " << StrokedPoint::bevel_edge_bit << "\n";

  painter_test P;
  return P.main(argc, argv);

}
