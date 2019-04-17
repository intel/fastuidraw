#include <vector>

#include <fastuidraw/text/glyph_cache.hpp>
#include <fastuidraw/text/font_database.hpp>
#include <fastuidraw/painter/painter.hpp>
#include <fastuidraw/gl_backend/painter_engine_gl.hpp>
#include <fastuidraw/gl_backend/painter_surface_gl.hpp>
#include <fastuidraw/text/font_freetype.hpp>

#include "cast_c_array.hpp"
#include "sdl_demo.hpp"

class sdl_painter_demo:public sdl_demo
{
public:
  enum pixel_count_t
    {
      frame_number_pixels,
      frame_number_pixels_that_neighbor_helper,
      total_number_pixels,
      total_number_pixels_that_neighbor_helper,
    };

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

  virtual
  void
  pre_draw_frame(void);

  virtual
  void
  post_draw_frame(void);

  uint64_t
  pixel_count(enum pixel_count_t tp)
  {
    return m_pixel_counts[tp];
  }

  bool
  pixel_counter_active(void)
  {
    return m_pixel_counter_stack.value() >= 0;
  }

  fastuidraw::c_array<const unsigned int>
  painter_stats(void) const
  {
    return cast_c_array(m_painter_stats);
  }

  unsigned int
  painter_stat(enum fastuidraw::Painter::query_stats_t t)
  {
    return (t < m_painter_stats.size()) ?
      m_painter_stats[t] :
      0u;
  }

protected:
  void
  draw_text(const std::string &text, float pixel_size,
            const fastuidraw::FontBase *font,
            fastuidraw::GlyphRenderer renderer,
            const fastuidraw::PainterData &draw,
            enum fastuidraw::Painter::screen_orientation orientation
            = fastuidraw::Painter::y_increases_downwards);
  void
  draw_text(const std::string &text, float pixel_size,
            const fastuidraw::FontBase *font,
            const fastuidraw::PainterData &draw,
            enum fastuidraw::Painter::screen_orientation orientation
            = fastuidraw::Painter::y_increases_downwards)
  {
    draw_text(text, pixel_size, font, fastuidraw::GlyphRenderer(),
              draw, orientation);
  }

  fastuidraw::reference_counted_ptr<fastuidraw::ImageAtlas> m_image_atlas;
  fastuidraw::reference_counted_ptr<fastuidraw::GlyphAtlas> m_glyph_atlas;
  fastuidraw::reference_counted_ptr<fastuidraw::ColorStopAtlas> m_colorstop_atlas;
  fastuidraw::reference_counted_ptr<fastuidraw::gl::PainterSurfaceGL> m_surface;
  fastuidraw::reference_counted_ptr<fastuidraw::gl::PainterEngineGL> m_backend;
  fastuidraw::reference_counted_ptr<fastuidraw::Painter> m_painter;
  fastuidraw::reference_counted_ptr<fastuidraw::GlyphCache> m_glyph_cache;
  fastuidraw::reference_counted_ptr<fastuidraw::FontDatabase> m_font_database;
  fastuidraw::reference_counted_ptr<fastuidraw::FreeTypeLib> m_ft_lib;

private:
  typedef enum fastuidraw::gl::PainterEngineGL::data_store_backing_t data_store_backing_t;
  typedef enum fastuidraw::glsl::PainterShaderRegistrarGLSL::clipping_type_t clipping_type_t;
  typedef enum fastuidraw::glsl::PainterShaderRegistrarGLSL::fbf_blending_type_t fbf_blending_type_t;
  typedef enum fastuidraw::PainterBlendShader::shader_type shader_blend_type;
  enum glyph_backing_store_t
    {
      glyph_backing_store_texture_buffer,
      glyph_backing_store_texture_array,
      glyph_backing_store_ssbo,
      glyph_backing_store_auto,
    };

  enum painter_optimal_t
    {
      painter_no_optimal,
      painter_optimal_performance,
      painter_optimal_rendering,
    };

  fastuidraw::gl::GlyphAtlasGL::params m_glyph_atlas_params;
  fastuidraw::gl::ColorStopAtlasGL::params m_colorstop_atlas_params;
  fastuidraw::gl::ImageAtlasGL::params m_image_atlas_params;
  fastuidraw::gl::PainterEngineGL::ConfigurationGL m_painter_params;

  /* Image atlas parameters */
  command_separator m_image_atlas_options;
  command_line_argument_value<int> m_log2_color_tile_size, m_log2_num_color_tiles_per_row_per_col;
  command_line_argument_value<int> m_num_color_layers;
  command_line_argument_value<int> m_log2_index_tile_size, m_log2_num_index_tiles_per_row_per_col;
  command_line_argument_value<int> m_num_index_layers;

  /* Glyph atlas parameters */
  command_separator m_glyph_atlas_options;
  command_line_argument_value<int> m_glyph_atlas_size;
  command_line_argument_value<bool> m_glyph_atlas_delayed_upload;
  enumerated_command_line_argument_value<enum glyph_backing_store_t> m_glyph_backing_store_type;
  command_line_argument_value<int> m_glyph_backing_texture_log2_w, m_glyph_backing_texture_log2_h;

  /* ColorStop atlas parameters */
  command_separator m_colorstop_atlas_options;
  command_line_argument_value<int> m_color_stop_atlas_width;
  command_line_argument_value<int> m_color_stop_atlas_layers;

  /* Painter params */
  command_separator m_painter_options;
  command_line_argument_value<int> m_painter_attributes_per_buffer;
  command_line_argument_value<int> m_painter_indices_per_buffer;
  command_line_argument_value<int> m_painter_number_pools;
  command_line_argument_value<bool> m_painter_break_on_shader_change;
  command_line_argument_value<bool> m_uber_vert_use_switch;
  command_line_argument_value<bool> m_uber_frag_use_switch;
  command_line_argument_value<bool> m_use_uber_item_shader;
  command_line_argument_value<bool> m_uber_blend_use_switch;
  command_line_argument_value<bool> m_separate_program_for_discard;
  command_line_argument_value<bool> m_allow_bindless_texture_from_surface;

  /* Painter params that can be overridden by properties of GL context */
  command_separator m_painter_options_affected_by_context;
  enumerated_command_line_argument_value<clipping_type_t> m_use_hw_clip_planes;
  command_line_argument_value<int> m_painter_data_blocks_per_buffer;
  enumerated_command_line_argument_value<data_store_backing_t> m_data_store_backing;
  command_line_argument_value<bool> m_assign_layout_to_vertex_shader_inputs;
  command_line_argument_value<bool> m_assign_layout_to_varyings;
  command_line_argument_value<bool> m_assign_binding_points;
  command_line_argument_value<bool> m_support_dual_src_blend_shaders;
  enumerated_command_line_argument_value<shader_blend_type> m_preferred_blend_type;
  enumerated_command_line_argument_value<fbf_blending_type_t> m_fbf_blending_type;
  enumerated_command_line_argument_value<enum painter_optimal_t> m_painter_optimal;

  command_separator m_demo_options;
  command_line_argument_value<bool> m_print_painter_config;
  command_line_argument_value<bool> m_print_painter_shader_ids;

  /* if we are to record pixel counts only */
  command_line_argument_value<int> m_pixel_counter_stack;

  /* glyph generation options */
  command_line_argument_value<int> m_distance_field_pixel_size;
  command_line_argument_value<float> m_distance_field_max_distance;
  command_line_argument_value<unsigned int> m_restricted_rays_max_recursion;
  command_line_argument_value<unsigned int> m_restricted_rays_split_thresh;
  command_line_argument_value<float> m_restricted_rays_expected_min_render_size;
  command_line_argument_value<unsigned int> m_banded_rays_max_recursion;
  command_line_argument_value<float> m_banded_rays_average_number_curves_thresh;

  std::list<GLuint> m_pixel_counter_buffers;
  unsigned int m_num_pixel_counter_buffers;
  unsigned int m_pixel_counter_buffer_binding_index;
  fastuidraw::vecN<uint64_t, 4> m_pixel_counts;
  std::vector<unsigned int> m_painter_stats;
};
