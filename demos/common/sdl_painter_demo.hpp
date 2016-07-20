#include <fastuidraw/text/glyph_cache.hpp>
#include <fastuidraw/text/glyph_selector.hpp>
#include <fastuidraw/painter/painter.hpp>
#include <fastuidraw/gl_backend/painter_backend_gl.hpp>
#include <fastuidraw/text/freetype_font.hpp>

#include "sdl_demo.hpp"

class sdl_painter_demo:public sdl_demo
{
public:
  explicit
  sdl_painter_demo(const std::string &about_text=std::string());

  ~sdl_painter_demo();

protected:
  void
  init_gl(int, int);

  virtual
  void
  derived_init(int, int)
  {}

  void
  on_resize(int, int);

  const fastuidraw::gl::PainterBackendGL::params&
  painter_params(void)
  {
    return m_painter_params;
  }

protected:
  void
  draw_text(const std::string &text, float pixel_size,
            fastuidraw::FontBase::const_handle font,
            fastuidraw::GlyphRender renderer,
            const fastuidraw::PainterData &draw);

  fastuidraw::gl::ImageAtlasGL::handle m_image_atlas;
  fastuidraw::gl::GlyphAtlasGL::handle m_glyph_atlas;
  fastuidraw::gl::ColorStopAtlasGL::handle m_colorstop_atlas;
  fastuidraw::gl::PainterBackendGL::handle m_backend;
  fastuidraw::Painter::handle m_painter;
  fastuidraw::GlyphCache::handle m_glyph_cache;
  fastuidraw::GlyphSelector::handle m_glyph_selector;
  fastuidraw::FreetypeLib::handle m_ft_lib;

private:
  typedef enum fastuidraw::gl::PainterBackendGL::data_store_backing_t data_store_backing_t;
  enum glyph_geometry_backing_store_t
    {
      glyph_geometry_backing_store_texture_buffer,
      glyph_geometry_backing_store_texture_array,
      glyph_geometry_backing_store_auto,
    };

  fastuidraw::gl::GlyphAtlasGL::params m_glyph_atlas_params;
  fastuidraw::gl::ColorStopAtlasGL::params m_colorstop_atlas_params;
  fastuidraw::gl::ImageAtlasGL::params m_image_atlas_params;
  fastuidraw::gl::PainterBackendGL::params m_painter_params;

  /* Image atlas parameters
   */
  command_separator m_image_atlas_options;
  command_line_argument_value<int> m_log2_color_tile_size, m_log2_num_color_tiles_per_row_per_col;
  command_line_argument_value<int> m_num_color_layers;
  command_line_argument_value<int> m_log2_index_tile_size, m_log2_num_index_tiles_per_row_per_col;
  command_line_argument_value<int> m_num_index_layers;
  command_line_argument_value<bool> m_image_atlas_delayed_upload;

  /* Glyph atlas parameters
   */
  command_separator m_glyph_atlas_options;
  command_line_argument_value<int> m_texel_store_width, m_texel_store_height;
  command_line_argument_value<int> m_texel_store_num_layers, m_geometry_store_size;
  command_line_argument_value<int> m_geometry_store_alignment;
  command_line_argument_value<bool> m_glyph_atlas_delayed_upload;
  enumerated_command_line_argument_value<enum glyph_geometry_backing_store_t> m_glyph_geometry_backing_store_type;
  command_line_argument_value<int> m_glyph_geometry_backing_texture_log2_w, m_glyph_geometry_backing_texture_log2_h;

  /* ColorStop atlas parameters
   */
  command_separator m_colorstop_atlas_options;
  command_line_argument_value<int> m_color_stop_atlas_width;
  command_line_argument_value<bool> m_color_stop_atlas_use_optimal_width;
  command_line_argument_value<int> m_color_stop_atlas_layers;
  command_line_argument_value<bool> m_color_stop_atlas_delayed_upload;

  /* Painter params
   */
  command_separator m_painter_options;
  command_line_argument_value<int> m_painter_attributes_per_buffer;
  command_line_argument_value<int> m_painter_indices_per_buffer;
  command_line_argument_value<int> m_painter_data_blocks_per_buffer;
  command_line_argument_value<int> m_painter_alignment;
  command_line_argument_value<int> m_painter_number_pools;
  command_line_argument_value<bool> m_painter_break_on_vertex_shader_change;
  command_line_argument_value<bool> m_painter_break_on_fragment_shader_change;
  command_line_argument_value<bool> m_use_hw_clip_planes;
  command_line_argument_value<bool> m_uber_vert_use_switch;
  command_line_argument_value<bool> m_uber_frag_use_switch;
  command_line_argument_value<bool> m_uber_blend_use_switch;
  command_line_argument_value<bool> m_unpack_header_and_brush_in_frag_shader;
  enumerated_command_line_argument_value<data_store_backing_t> m_data_store_backing;

  command_separator m_demo_options;
};
