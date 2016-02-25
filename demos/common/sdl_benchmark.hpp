#pragma once

#include <fastuidraw/util/vecN.hpp>
#include "sdl_demo.hpp"
#include "simple_time.hpp"

class sdl_benchmark:
  private sdl_demo
{
public:
  sdl_benchmark(const std::string &about_text, bool allow_offscreen_fbo=true);

  virtual
  ~sdl_benchmark();

  int
  main(int argc, char **argv)
  {
    return sdl_demo::main(argc, argv);
  }

protected:

  virtual
  void
  init_gl(int, int);

  virtual
  void
  draw_frame(void);

  virtual
  void
  benchmark_init(int width, int height)=0;

  virtual
  void
  benchmark_draw_frame(int frame, uint32_t time_ms)=0;

  void
  end_benchmark(int return_code)
  {
    sdl_demo::end_demo(return_code);
  }

  fastuidraw::ivec2
  dimensions(void)
  {
    return sdl_demo::dimensions();
  }

private:

  enum render_to_fbo_t
    {
      no_fbo,
      blit_fbo,
      no_blit_fbo,
    };

  void
  draw_fbo_contents(void);

  void
  create_and_bind_fbo(void);

  void
  unbind_and_delete_fbo(void);

  command_line_register m_avoid_allow_fbo;

  command_separator m_common_options;
  command_line_argument_value<int> m_num_frames;
  enumerated_command_line_argument_value<enum render_to_fbo_t> m_render_to_fbo;
  command_line_argument_value<bool> m_read_pixel;
  command_line_argument_value<int> m_fbo_width, m_fbo_height;
  command_line_argument_value<bool> m_dry_run;
  command_line_argument_value<int> m_swap_buffer_extra;
  command_line_argument_value<bool> m_print_each_time;

  command_separator m_benchmark_label;

  fastuidraw::ivec2 m_screen_size;
  GLuint m_fbo, m_color, m_depth_stencil;
  int m_frame;
  uint32_t m_to_create;
  simple_time m_time;
  simple_time m_last_frame_time;
};
