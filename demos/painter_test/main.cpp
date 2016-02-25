#include <iostream>
#include <sstream>
#include <fstream>
#include <bitset>

#include <fastuidraw/util/util.hpp>
#include <fastuidraw/painter/painter.hpp>
#include <fastuidraw/painter/packing/painter_packing_enums.hpp>
#include <fastuidraw/painter/packing/painter_packing_brush.hpp>

#include "sdl_painter_demo.hpp"

using namespace fastuidraw;

typedef std::bitset<32> bitset;

class painter_test:public sdl_painter_demo
{
public:
  painter_test(void)
  {}

protected:
  void
  derived_init(int, int)
  {
    gl::PainterShaderGL::handle v0, v1;
    v0 = FASTUIDRAWnew gl::PainterShaderGL(gl::Shader::shader_source()
                                          .add_source("vert0.glsl.resource_string",
                                                      gl::Shader::from_resource));

    v1 = FASTUIDRAWnew gl::PainterShaderGL(gl::Shader::shader_source()
                                          .add_source("vert1.glsl.resource_string",
                                                      gl::Shader::from_resource));


    m_backend->register_vert_shader(v0);
    m_backend->register_vert_shader(v1);


    if(m_backend->program()->link_success())
      {
        std::ofstream file("painter.program.glsl");
        std::cout << "Link success\n";
        file << m_backend->program()->log();
        m_backend->program()->use_program();
      }
    else
      {
        std::cout << "Link Failed\n";
      }
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
  std::cout << std::setw(40) << "header_size = " << PainterPacking::header_size << "\n"
            << std::setw(40) << "clip_equations_data_size = " << PainterPacking::clip_equations_data_size << "\n"
            << std::setw(40) << "item_matrix_data_size = " << PainterPacking::item_matrix_data_size << "\n"
            << std::setw(40) << "image_data_size = " << PainterPacking::Brush::image_data_size << "\n"
            << std::setw(40) << "linear_gradient_data_size = " << PainterPacking::Brush::linear_gradient_data_size << "\n"
            << std::setw(40) << "radial_gradient_data_size = " << PainterPacking::Brush::radial_gradient_data_size << "\n"
            << std::setw(40) << "repeat_window_data_size = " << PainterPacking::Brush::repeat_window_data_size << "\n"
            << std::setw(40) << "transformation_matrix_data_size = " << PainterPacking::Brush::transformation_matrix_data_size << "\n"
            << std::setw(40) << "transformation_translation_data_size = " << PainterPacking::Brush::transformation_translation_data_size << "\n"
            << "\n"

            << std::setw(40) << "image_slack_max = " << PainterBrush::image_slack_max << "\n"
            << std::setw(40) << "image_number_index_lookups_max = " << PainterBrush::image_number_index_lookups_max << "\n"
            << "\n"

            << std::setw(40) << "image_mask = " << bitset(PainterBrush::image_mask) << "\n"
            << std::setw(40) << "gradient_mask = " << bitset(PainterBrush::gradient_mask) << "\n"
            << std::setw(40) << "radial_gradient_mask = " << bitset(PainterBrush::radial_gradient_mask) << "\n"
            << std::setw(40) << "gradient_repeat_mask = " << bitset(PainterBrush::gradient_repeat_mask) << "\n"
            << std::setw(40) << "repeat_window_mask = " << bitset(PainterBrush::repeat_window_mask) << "\n"
            << std::setw(40) << "transformation_translation_mask = " << bitset(PainterBrush::transformation_translation_mask) << "\n"
            << std::setw(40) << "transformation_matrix_mask = " << bitset(PainterBrush::transformation_matrix_mask) << "\n"
            << std::setw(40) << "image_number_index_lookups_mask = " << bitset(PainterBrush::image_number_index_lookups_mask) << "\n"
            << std::setw(40) << "image_slack_mask = " << bitset(PainterBrush::image_slack_mask) << "\n";

  painter_test P;
  return P.main(argc, argv);

}
