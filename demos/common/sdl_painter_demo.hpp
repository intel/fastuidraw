#include <fastuidraw/text/glyph_cache.hpp>
#include <fastuidraw/text/glyph_selector.hpp>
#include <fastuidraw/painter/painter.hpp>
#include <fastuidraw/painter/painter_attribute_data_filler_glyphs.hpp>
#include <fastuidraw/gl_backend/painter_backend_gl.hpp>
#include <fastuidraw/text/font_freetype.hpp>

#include "sdl_demo.hpp"

class sdl_painter_demo:public sdl_demo
{
public:
  explicit
  sdl_painter_demo(const std::string &about_text=std::string(),
                   bool default_value_for_print_painter_config = false);

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

protected:
  void
  draw_text(const std::string &text, float pixel_size,
            fastuidraw::reference_counted_ptr<const fastuidraw::FontBase> font,
            fastuidraw::GlyphRender renderer,
            const fastuidraw::PainterData &draw,
	    enum fastuidraw::PainterEnums::glyph_orientation orientation
	    = fastuidraw::PainterEnums::y_increases_downwards);

  fastuidraw::reference_counted_ptr<fastuidraw::gl::ImageAtlasGL> m_image_atlas;
  fastuidraw::reference_counted_ptr<fastuidraw::gl::GlyphAtlasGL> m_glyph_atlas;
  fastuidraw::reference_counted_ptr<fastuidraw::gl::ColorStopAtlasGL> m_colorstop_atlas;
  fastuidraw::reference_counted_ptr<fastuidraw::gl::PainterBackendGL::SurfaceGL> m_surface;
  fastuidraw::reference_counted_ptr<fastuidraw::gl::PainterBackendGL> m_backend;
  fastuidraw::reference_counted_ptr<fastuidraw::Painter> m_painter;
  fastuidraw::reference_counted_ptr<fastuidraw::GlyphCache> m_glyph_cache;
  fastuidraw::reference_counted_ptr<fastuidraw::GlyphSelector> m_glyph_selector;
  fastuidraw::reference_counted_ptr<fastuidraw::FreeTypeLib> m_ft_lib;

private:
  typedef enum fastuidraw::gl::PainterBackendGL::data_store_backing_t data_store_backing_t;
  typedef enum fastuidraw::glsl::PainterBackendGLSL::auxiliary_buffer_t auxiliary_buffer_t;
  typedef enum fastuidraw::glsl::PainterBackendGLSL::clipping_type_t clipping_type_t;
  enum glyph_geometry_backing_store_t
    {
      glyph_geometry_backing_store_texture_buffer,
      glyph_geometry_backing_store_texture_array,
      glyph_geometry_backing_store_auto,
    };

  fastuidraw::gl::GlyphAtlasGL::params m_glyph_atlas_params;
  fastuidraw::gl::ColorStopAtlasGL::params m_colorstop_atlas_params;
  fastuidraw::gl::ImageAtlasGL::params m_image_atlas_params;
  fastuidraw::PainterBackend::ConfigurationBase m_painter_base_params;
  fastuidraw::gl::PainterBackendGL::ConfigurationGL m_painter_params;

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
  command_line_argument_value<int> m_painter_number_pools;
  command_line_argument_value<bool> m_painter_break_on_shader_change;
  command_line_argument_value<bool> m_uber_vert_use_switch;
  command_line_argument_value<bool> m_uber_frag_use_switch;
  command_line_argument_value<bool> m_uber_blend_use_switch;
  command_line_argument_value<bool> m_unpack_header_and_brush_in_frag_shader;
  command_line_argument_value<bool> m_separate_program_for_discard;
  command_line_argument_value<unsigned int> m_painter_msaa;

  /* Painter params that can be overridden by properties of GL context
   */
  command_separator m_painter_options_affected_by_context;
  enumerated_command_line_argument_value<auxiliary_buffer_t> m_provide_auxiliary_image_buffer;
  enumerated_command_line_argument_value<clipping_type_t> m_use_hw_clip_planes;
  command_line_argument_value<int> m_painter_alignment;
  command_line_argument_value<int> m_painter_data_blocks_per_buffer;
  enumerated_command_line_argument_value<data_store_backing_t> m_data_store_backing;
  command_line_argument_value<bool> m_assign_layout_to_vertex_shader_inputs;
  command_line_argument_value<bool> m_assign_layout_to_varyings;
  command_line_argument_value<bool> m_assign_binding_points;
  enumerated_command_line_argument_value<enum fastuidraw::PainterBlendShader::shader_type> m_blend_type;

  command_separator m_demo_options;
  command_line_argument_value<bool> m_print_painter_config;
  command_line_argument_value<bool> m_print_painter_shader_ids;
};
