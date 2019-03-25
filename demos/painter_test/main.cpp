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
    fastuidraw::c_string program_labels[gl::PainterBackendGL::number_program_types] =
      {
        [gl::PainterBackendGL::program_all] = "program_all",
        [gl::PainterBackendGL::program_without_discard] = "program_without_discard",
        [gl::PainterBackendGL::program_with_discard] = "program_with_discard",
      };

    fastuidraw::c_string blend_labels[PainterBlendShader::number_types] =
      {
        [PainterBlendShader::single_src] = "single_src",
        [PainterBlendShader::dual_src] = "single_src",
        [PainterBlendShader::framebuffer_fetch] = "framebuffer_fetch",
      };

    for (int pr = 0; pr < gl::PainterBackendGL::number_program_types; ++pr)
      {
        for (int b = 0; b < PainterBlendShader::number_types; ++b)
          {
            reference_counted_ptr<gl::Program> program;
            program = m_backend->program(static_cast<enum gl::PainterBackendGL::program_type_t>(pr),
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
  std::cout << std::setw(40) << "header_size = " << PainterHeader::header_size << "\n"
            << std::setw(40) << "clip_equations_data_size = " << PainterClipEquations::clip_data_size << "\n"
            << std::setw(40) << "item_matrix_data_size = " << PainterItemMatrix::matrix_data_size << "\n"
            << std::setw(40) << "image_data_size = " << PainterBrush::image_data_size << "\n"
            << std::setw(40) << "linear_gradient_data_size = " << PainterBrush::linear_gradient_data_size << "\n"
            << std::setw(40) << "radial_gradient_data_size = " << PainterBrush::radial_gradient_data_size << "\n"
            << std::setw(40) << "repeat_window_data_size = " << PainterBrush::repeat_window_data_size << "\n"
            << std::setw(40) << "transformation_matrix_data_size = " << PainterBrush::transformation_matrix_data_size << "\n"
            << std::setw(40) << "transformation_translation_data_size = " << PainterBrush::transformation_translation_data_size << "\n"
            << "\n"

            << std::setw(40) << "image_slack_bit0 = " << PainterBrush::image_slack_bit0 << "\n"
            << std::setw(40) << "image_slack_num_bits = " << PainterBrush::image_slack_num_bits << "\n"
            << std::setw(40) << "image_number_index_lookups_bit0 = " << PainterBrush::image_number_index_lookups_bit0 << "\n"
            << std::setw(40) << "image_number_index_lookups_num_bits = " << PainterBrush::image_number_index_lookups_num_bits << "\n"
            << "\n"

            << std::setw(40) << "image_mask = " << bitset(PainterBrush::image_mask) << "\n"
            << std::setw(40) << "gradient_type_mask = " << bitset(PainterBrush::gradient_type_mask) << "\n"
            << std::setw(40) << "gradient_spread_type_mask = " << bitset(PainterBrush::gradient_spread_type_mask) << "\n"
            << std::setw(40) << "repeat_window_mask = " << bitset(PainterBrush::repeat_window_mask) << "\n"
            << std::setw(40) << "transformation_translation_mask = " << bitset(PainterBrush::transformation_translation_mask) << "\n"
            << std::setw(40) << "transformation_matrix_mask = " << bitset(PainterBrush::transformation_matrix_mask) << "\n"

            << std::setw(40) << "stroked_number_offset_types = " << StrokedPoint::number_offset_types << "\n"
            << std::setw(40) << "stroked_offset_type_bit0 = " << StrokedPoint::offset_type_bit0 << "\n"
            << std::setw(40) << "stroked_offset_type_num_bits = " << StrokedPoint::offset_type_num_bits << "\n"
            << std::setw(40) << "stroked_boundary_bit = " << StrokedPoint::boundary_bit << "\n"
            << std::setw(40) << "stroked_depth_bit0 = " << StrokedPoint::depth_bit0 << "\n"
            << std::setw(40) << "stroked_depth_num_bits = " << StrokedPoint::depth_num_bits << "\n"
            << std::setw(40) << "stroked_join_bit = " << StrokedPoint::join_bit << "\n"
            << std::setw(40) << "stroked_number_common_bits = " << StrokedPoint::number_common_bits << "\n"
            << std::setw(40) << "stroked_normal0_y_sign_bit = " << StrokedPoint::normal0_y_sign_bit << "\n"
            << std::setw(40) << "stroked_normal1_y_sign_bit = " << StrokedPoint::normal1_y_sign_bit << "\n"
            << std::setw(40) << "stroked_sin_sign_bit = " << StrokedPoint::sin_sign_bit << "\n"
            << std::setw(40) << "stroked_adjustable_cap_ending_bit = " << StrokedPoint::adjustable_cap_ending_bit << "\n"
            << std::setw(40) << "stroked_bevel_edge_bit = " << StrokedPoint::bevel_edge_bit << "\n";

  painter_test P;
  return P.main(argc, argv);

}
