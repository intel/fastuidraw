#include <fastuidraw/gl_backend/gl_get.hpp>
#include "sdl_painter_demo.hpp"
#include "text_helper.hpp"

namespace
{
  template<typename T>
  class enum_wrapper
  {
  public:
    explicit
    enum_wrapper(T v):
      m_v(v)
    {}

    T m_v;
  };

  template<typename T>
  enum_wrapper<T>
  make_enum_wrapper(T v)
  {
    return enum_wrapper<T>(v);
  }

  std::ostream&
  operator<<(std::ostream &str, enum_wrapper<bool> b)
  {
    if (b.m_v)
      {
        str << "true";
      }
    else
      {
        str << "false";
      }
    return str;
  }

  std::ostream&
  operator<<(std::ostream &str,
             enum_wrapper<enum fastuidraw::gl::PainterBackendGL::data_store_backing_t> v)
  {
    switch(v.m_v)
      {
      case fastuidraw::gl::PainterBackendGL::data_store_tbo:
        str << "tbo";
        break;

      case fastuidraw::gl::PainterBackendGL::data_store_ubo:
        str << "ubo";
        break;

      default:
        str << "invalid value";
      }

    return str;
  }

  std::ostream&
  operator<<(std::ostream &str,
             enum_wrapper<enum fastuidraw::gl::PainterBackendGL::clipping_type_t> v)
  {
    switch(v.m_v)
      {
      case fastuidraw::gl::PainterBackendGL::clipping_via_discard:
        str << "clipping_via_discard";
        break;

      case fastuidraw::gl::PainterBackendGL::clipping_via_clip_distance:
        str << "clipping_via_clip_distance";
        break;

      default:
        str << "invalid value";
      }

    return str;
  }

  std::ostream&
  operator<<(std::ostream &str,
             enum_wrapper<enum fastuidraw::PainterStrokeShader::type_t> v)
  {
    switch(v.m_v)
      {
      case fastuidraw::PainterStrokeShader::draws_solid_then_fuzz:
        str << "draws_solid_then_fuzz";
        break;

      case fastuidraw::PainterStrokeShader::cover_then_draw:
        str << "cover_then_draw";
        break;

      default:
        str << "invalid value";
      }

    return str;
  }

  std::ostream&
  operator<<(std::ostream &str,
             enum_wrapper<enum fastuidraw::PainterBlendShader::shader_type> v)
  {
    switch(v.m_v)
      {
      case fastuidraw::PainterBlendShader::single_src:
        str << "single_src";
        break;

      case fastuidraw::PainterBlendShader::dual_src:
        str << "dual_src";
        break;

      case fastuidraw::PainterBlendShader::framebuffer_fetch:
        str << "framebuffer_fetch";
        break;

      default:
        str << "invalid value";
      }

    return str;
  }

  std::ostream&
  operator<<(std::ostream &str,
             enum_wrapper<enum fastuidraw::glsl::PainterBackendGLSL::auxiliary_buffer_t> v)
  {
    switch(v.m_v)
      {
      case fastuidraw::glsl::PainterBackendGLSL::no_auxiliary_buffer:
        str << "no_auxiliary_buffer";
        break;

      case fastuidraw::glsl::PainterBackendGLSL::auxiliary_buffer_atomic:
        str << "auxiliary_buffer_atomic";
        break;

      case fastuidraw::glsl::PainterBackendGLSL::auxiliary_buffer_interlock:
        str << "auxiliary_buffer_interlock";
        break;

      case fastuidraw::glsl::PainterBackendGLSL::auxiliary_buffer_interlock_main_only:
        str << "auxiliary_buffer_interlock_main_only";
        break;

      case fastuidraw::glsl::PainterBackendGLSL::auxiliary_buffer_framebuffer_fetch:
        str << "auxiliary_buffer_framebuffer_fetch";
        break;

      default:
        str << "invalid value";
      }

    return str;
  }

  std::ostream&
  operator<<(std::ostream &ostr, const fastuidraw::PainterShader::Tag &tag)
  {
    ostr << "(ID=" << tag.m_ID << ", group=" << tag.m_group << ")";
    return ostr;
  }

  void
  print_glyph_shader_ids(const fastuidraw::PainterGlyphShader &sh)
  {
    for(unsigned int i = 0; i < sh.shader_count(); ++i)
      {
        enum fastuidraw::glyph_type tp;
        tp = static_cast<enum fastuidraw::glyph_type>(i);
        std::cout << "\t\t#" << i << ": " << sh.shader(tp)->tag() << "\n";
      }
  }

  void
  print_stroke_shader_ids(const fastuidraw::PainterStrokeShader &sh,
                          const std::string &prefix = "\t\t")
  {
    std::cout << prefix << "aa_shader_pass1: " << sh.aa_shader_pass1()->tag() << "\n"
              << prefix << "aa_shader_pass2: " << sh.aa_shader_pass2()->tag() << "\n"
              << prefix << "non_aa_shader: " << sh.non_aa_shader()->tag() << "\n";
  }

  void
  print_dashed_stroke_shader_ids(const fastuidraw::PainterDashedStrokeShaderSet &sh)
  {
    std::cout << "\t\tflat_caps:\n";
    print_stroke_shader_ids(sh.shader(fastuidraw::PainterEnums::flat_caps), "\t\t\t");

    std::cout << "\t\trounded_caps:\n";
    print_stroke_shader_ids(sh.shader(fastuidraw::PainterEnums::rounded_caps), "\t\t\t");

    std::cout << "\t\tsquare_caps:\n";
    print_stroke_shader_ids(sh.shader(fastuidraw::PainterEnums::square_caps), "\t\t\t");
  }
}

sdl_painter_demo::
sdl_painter_demo(const std::string &about_text,
                 bool default_value_for_print_painter):
  sdl_demo(about_text),

  m_image_atlas_options("Image Atlas Options", *this),
  m_log2_color_tile_size(m_image_atlas_params.log2_color_tile_size(), "log2_color_tile_size",
                         "Specifies the log2 of the width and height of each color tile",
                         *this),
  m_log2_num_color_tiles_per_row_per_col(m_image_atlas_params.log2_num_color_tiles_per_row_per_col(),
                                         "log2_num_color_tiles_per_row_per_col",
                                         "Specifies the log2 of the number of color tiles "
                                         "in each row and column of each layer; note that "
                                         "then the total number of color tiles available "
                                         "is given as num_color_layers*pow(2, 2*log2_num_color_tiles_per_row_per_col)",
                                         *this),
  m_num_color_layers(m_image_atlas_params.num_color_layers(), "num_color_layers",
                     "Specifies the number of layers in the color texture; note that "
                     "then the total number of color tiles available "
                     "is given as num_color_layers*pow(2, 2*log2_num_color_tiles_per_row_per_col)",
                     *this),
  m_log2_index_tile_size(m_image_atlas_params.log2_index_tile_size(), "log2_index_tile_size",
                         "Specifies the log2 of the width and height of each index tile",
                         *this),
  m_log2_num_index_tiles_per_row_per_col(m_image_atlas_params.log2_num_index_tiles_per_row_per_col(),
                                         "log2_num_index_tiles_per_row_per_col",
                                         "Specifies the log2 of the number of index tiles "
                                         "in each row and column of each layer; note that "
                                         "then the total number of index tiles available "
                                         "is given as num_index_layers*pow(2, 2*log2_num_index_tiles_per_row_per_col)",
                                         *this),
  m_num_index_layers(m_image_atlas_params.num_index_layers(), "num_index_layers",
                     "Specifies the number of layers in the index texture; note that "
                     "then the total number of index tiles available "
                     "is given as num_index_layers*pow(2, 2*log2_num_index_tiles_per_row_per_col)",
                     *this),
  m_image_atlas_delayed_upload(m_image_atlas_params.delayed(),
                               "image_atlas_delayed_upload",
                               "if true delay uploading of data to GL from image atlas until atlas flush",
                               *this),

  m_glyph_atlas_options("Glyph Atlas options", *this),
  m_texel_store_width(m_glyph_atlas_params.texel_store_dimensions().x(),
                      "texel_store_width", "width of texel store", *this),
  m_texel_store_height(m_glyph_atlas_params.texel_store_dimensions().y(),
                       "texel_store_height", "height of texel store", *this),
  m_texel_store_num_layers(m_glyph_atlas_params.texel_store_dimensions().z(),
                           "texel_store_num_layers", "number of layers of texel store", *this),
  m_geometry_store_size(m_glyph_atlas_params.number_floats(),
                        "geometry_store_size", "size of geometry store in floats", *this),
  m_geometry_store_alignment(m_glyph_atlas_params.alignment(),
                              "geometry_store_alignment",
                              "alignment of the geometry store, must be one of 1, 2, 3 or 4", *this),
  m_glyph_atlas_delayed_upload(m_glyph_atlas_params.delayed(),
                               "glyph_atlas_delayed_upload",
                               "if true delay uploading of data to GL from glyph atlas until atlas flush",
                               *this),
  m_glyph_geometry_backing_store_type(glyph_geometry_backing_store_auto,
                                      enumerated_string_type<enum glyph_geometry_backing_store_t>()
                                      .add_entry("buffer",
                                                 glyph_geometry_backing_store_texture_buffer,
                                                 "use a texture buffer, feature is core in GL but for GLES requires version 3.2, "
                                                 "for GLES version pre-3.2, requires the extension GL_OES_texture_buffer or the "
                                                 "extension GL_EXT_texture_buffer")
                                      .add_entry("texture_array",
                                                 glyph_geometry_backing_store_texture_array,
                                                 "use a 2D texture array to store the glyph geometry data, "
                                                 "GL and GLES have feature in core")
                                      .add_entry("auto",
                                                 glyph_geometry_backing_store_auto,
                                                 "query context and decide optimal value"),
                                      "geometry_backing_store_type",
                                      "Determines how the glyph geometry store is backed.",
                                      *this),
  m_glyph_geometry_backing_texture_log2_w(10, "glyph_geometry_backing_texture_log2_w",
                                          "If glyph_geometry_backing_store_type is set to texture_array, then "
                                          "this gives the log2 of the width of the texture array", *this),
  m_glyph_geometry_backing_texture_log2_h(10, "glyph_geometry_backing_texture_log2_h",
                                          "If glyph_geometry_backing_store_type is set to texture_array, then "
                                          "this gives the log2 of the height of the texture array", *this),
  m_colorstop_atlas_options("ColorStop Atlas options", *this),
  m_color_stop_atlas_width(m_colorstop_atlas_params.width(),
                           "colorstop_atlas_width",
                           "width for color stop atlas", *this),
  m_color_stop_atlas_use_optimal_width(false, "colorstop_atlas_use_optimal_width",
                                       "if true ignore the value of colorstop_atlas_layers "
                                       "and query the GL context for the optimal width for "
                                       "the colorstop atlas",
                                       *this),
  m_color_stop_atlas_layers(m_colorstop_atlas_params.num_layers(),
                            "colorstop_atlas_layers",
                            "number of layers for the color stop atlas",
                            *this),
  m_color_stop_atlas_delayed_upload(m_colorstop_atlas_params.delayed(),
                                    "color_stop_atlas_delayed_upload",
                                    "if true delay uploading of data to GL from color stop atlas until atlas flush",
                                    *this),

  m_painter_options("PainterBackendGL Options", *this),
  m_painter_attributes_per_buffer(m_painter_params.attributes_per_buffer(),
                                  "painter_verts_per_buffer",
                                  "Number of vertices a single API draw can hold",
                                  *this),
  m_painter_indices_per_buffer(m_painter_params.indices_per_buffer(),
                               "painter_indices_per_buffer",
                               "Number of indices a single API draw can hold",
                               *this),
  m_painter_number_pools(m_painter_params.number_pools(), "painter_number_pools",
                         "Number of GL object pools used by the painter", *this),
  m_painter_break_on_shader_change(m_painter_params.break_on_shader_change(),
                                   "painter_break_on_shader_change",
                                   "If true, different shadings are placed into different "
                                   "entries of a call to glMultiDrawElements", *this),
  m_uber_vert_use_switch(m_painter_params.vert_shader_use_switch(),
                         "painter_uber_vert_use_switch",
                         "If true, use a switch statement in uber vertex shader dispatch",
                         *this),
  m_uber_frag_use_switch(m_painter_params.frag_shader_use_switch(),
                         "painter_uber_frag_use_switch",
                         "If true, use a switch statement in uber fragment shader dispatch",
                         *this),
  m_uber_blend_use_switch(m_painter_params.blend_shader_use_switch(),
                          "painter_uber_blend_use_switch",
                          "If true, use a switch statement in uber blend shader dispatch",
                          *this),
  m_unpack_header_and_brush_in_frag_shader(m_painter_params.unpack_header_and_brush_in_frag_shader(),
                                           "painter_unpack_header_and_brush_in_frag_shader",
                                           "if true, unpack the brush and frag-shader specific data from "
                                           "the header in the fragment shader instead of the vertex shader",
                                           *this),
  m_separate_program_for_discard(m_painter_params.separate_program_for_discard(),
                                 "separate_program_for_discard",
                                 "if true, there are two GLSL programs active when drawing: "
                                 "one for those item shaders that have discard and one for "
                                 "those that do not",
                                 *this),
  m_painter_msaa(1, "painter_msaa",
                 "If greater than one, use MSAA for the backing store of the SurfaceGL "
                 "to which the Painter will draw, the value indicates the number of samples "
                 "to use for the backing store",
                 *this),

  m_painter_options_affected_by_context("PainterBackendGL Options that can be overridden "
                                        "by version and extension supported by GL/GLES context",
                                        *this),
  m_provide_auxiliary_image_buffer(m_painter_params.provide_auxiliary_image_buffer(),
                                  enumerated_string_type<auxiliary_buffer_t>()
                                  .add_entry("no_auxiliary_buffer",
                                             fastuidraw::glsl::PainterBackendGLSL::no_auxiliary_buffer,
                                             "No auxiliary buffer provided")
                                  .add_entry("auxiliary_buffer_atomic",
                                             fastuidraw::glsl::PainterBackendGLSL::auxiliary_buffer_atomic,
                                             "Auxiliary buffer through atomic ops; this can be quite poor "
                                             "performing because atomics can be quite slow AND (worse) "
                                             "a draw break appears to issue a memory barrier within each "
                                             "path stroking and after each path stroking. Requires only "
                                             "GL 4.2 (or GL_ARB_shader_image_load_store extension) for GL "
                                             "and GLES 3.1 for GLES")
                                  .add_entry("auxiliary_buffer_interlock",
                                             fastuidraw::glsl::PainterBackendGLSL::auxiliary_buffer_interlock,
                                             "Auxiliary buffer with interlock; this is high performant option "
                                             "as it does NOT use atomic ops and does not force any draw call "
                                             "breaks to issue a memory barrier; requires GL_INTEL_fragment_shader_ordering "
                                             "and GL 4.2 (or GL_ARB_shader_image_load_store extension); if requirements "
                                             "are not satisfied will try to fall back to auxiliary_buffer_interlock_main_only "
                                             "and if those are not satisfied will fall back to auxiliary_buffer_atomic and "
                                             "if those requirement are not satsified, then no_auxiliary_buffer")
                                  .add_entry("auxiliary_buffer_interlock_main_only",
                                             fastuidraw::glsl::PainterBackendGLSL::auxiliary_buffer_interlock_main_only,
                                             "Auxiliary buffer with interlock; this is high performant option "
                                             "as it does NOT use atomic ops and does not force any draw call "
                                             "breaks to issue a memory barrier; requires GL_ARB_fragment_shader_interlock "
                                             "OR GL_NV_fragment_shader_interlock together with GL 4.2 (or "
                                             "GL_ARB_shader_image_load_store) for GL and GLES 3.1 for GLES; if requirements "
                                             "are not satisfied will try to fall back to auxiliary_buffer_interlock "
                                             "and if those are not satisfied will fall back to auxiliary_buffer_atomic and "
                                             "if those requirement are not satsified, then no_auxiliary_buffer")
                                  .add_entry("auxiliary_buffer_framebuffer_fetch",
                                             fastuidraw::glsl::PainterBackendGLSL::auxiliary_buffer_framebuffer_fetch,
                                             "Auxilary buffer via framebuffer fetch; this is high performant option "
                                             "as it does NOT use atomic ops and does not force any draw call "
                                             "breaks to issue a memory barrier; requires GL_EXT_shader_framebuffer_fetch; "
                                             "if requirement is not satisfied will try to fall back to auxiliary_buffer_interlock "
                                             "which if not satisfied will fall back to auxiliary_buffer_interlock_main_only "
                                             "and if those are not satisfied will fall back to auxiliary_buffer_atomic and "
                                             "if those requirement are not satsified, then no_auxiliary_buffer"),
                                  "provide_auxiliary_image_buffer",
                                  "Spcifies if and how to provide auxiliary image buffer; "
                                  "will remove rendering artifacts on shader-based anti-aliased "
                                  "transparent path stroking",
                                  *this),
  m_use_hw_clip_planes(true,
                       "painter_use_hw_clip_planes",
                       "If true, use HW clip planes (i.e. gl_ClipDistance) for clipping",
                       *this),
  m_painter_alignment(m_painter_base_params.alignment(), "painter_alignment",
                       "Alignment for data store of painter, must be 1, 2, 3 or 4", *this),
  m_painter_data_blocks_per_buffer(m_painter_params.data_blocks_per_store_buffer(),
                                   "painter_blocks_per_buffer",
                                   "Number of data blocks a single API draw can hold",
                                   *this),
  m_data_store_backing(m_painter_params.data_store_backing(),
                       enumerated_string_type<data_store_backing_t>()
                       .add_entry("tbo",
                                  fastuidraw::gl::PainterBackendGL::data_store_tbo,
                                  "use a texture buffer (if available) to back the data store. "
                                  "A texture buffer can have a very large maximum size")
                       .add_entry("ubo",
                                  fastuidraw::gl::PainterBackendGL::data_store_ubo,
                                  "use a uniform buffer object to back the data store. "
                                  "A uniform buffer object's maximum size is much smaller than that "
                                  "of a texture buffer object usually"),
                       "painter_data_store_backing_type",
                       "specifies how the data store buffer is backed",
                       *this),
  m_assign_layout_to_vertex_shader_inputs(m_painter_params.assign_layout_to_vertex_shader_inputs(),
                                          "painter_assign_layout_to_vertex_shader_inputs",
                                          "If true, use layout(location=) in GLSL shader for vertex shader inputs",
                                          *this),
  m_assign_layout_to_varyings(m_painter_params.assign_layout_to_varyings(),
                              "painter_assign_layout_to_varyings",
                              "If true, use layout(location=) in GLSL shader for varyings", *this),
  m_assign_binding_points(m_painter_params.assign_binding_points(),
                          "painter_assign_binding_points",
                          "If true, use layout(binding=) in GLSL shader on samplers and buffers", *this),
  m_blend_type(m_painter_params.blend_type(),
	       enumerated_string_type<enum fastuidraw::PainterBlendShader::shader_type>()
	       .add_entry("framebuffer_fetch",
			  fastuidraw::PainterBlendShader::framebuffer_fetch,
			  "use a framebuffer fetch (if available) to perform blending, "
			  "thus all blending operations are part of uber-shader giving "
			  "more flexibility for blend types (namely W3C support) and "
			  "blend mode changes do not induce pipeline state changes")
	       .add_entry("dual_src",
			  fastuidraw::PainterBlendShader::dual_src,
			  "use a dual source blending (if available) to perform blending, "
			  "which has far less flexibility for blending than framebuffer-fetch "
			  "but has far few pipeline states (there are 3 blend mode pipeline states "
			  "and hald of the Porter-Duff blend modes are in one blend mode pipeline state")
	       .add_entry("single_src",
			  fastuidraw::PainterBlendShader::single_src,
			  "use single source blending to perform blending, "
			  "which is even less flexible than dual_src blending and "
			  "every Porter-Duff blend mode is a different pipeline state"),
	       "painter_blend_type",
	       "specifies how the painter will perform blending",
	       *this),
  m_demo_options("Demo Options", *this),
  m_print_painter_config(default_value_for_print_painter,
                         "print_painter_config",
                         "Print PainterBackendGL config", *this),
  m_print_painter_shader_ids(default_value_for_print_painter,
                         "print_painter_shader_ids",
                         "Print PainterBackendGL shader IDs", *this)
{}

sdl_painter_demo::
~sdl_painter_demo()
{}

void
sdl_painter_demo::
init_gl(int w, int h)
{
  int max_layers(0);

  max_layers = fastuidraw::gl::context_get<GLint>(GL_MAX_ARRAY_TEXTURE_LAYERS);
  if (max_layers < m_num_color_layers.m_value)
    {
      std::cout << "num_color_layers exceeds max number texture layers (" << max_layers
		<< "), num_color_layers set to that value.\n";
      m_num_color_layers.m_value = max_layers;
    }

  if (max_layers < m_color_stop_atlas_layers.m_value)
    {   
      std::cout << "atlas_layers exceeds max number texture layers (" << max_layers
		<< "), atlas_layers set to that value.\n";
      m_color_stop_atlas_layers.m_value = max_layers;
    }

  m_image_atlas_params
    .log2_color_tile_size(m_log2_color_tile_size.m_value)
    .log2_num_color_tiles_per_row_per_col(m_log2_num_color_tiles_per_row_per_col.m_value)
    .num_color_layers(m_num_color_layers.m_value)
    .log2_index_tile_size(m_log2_index_tile_size.m_value)
    .log2_num_index_tiles_per_row_per_col(m_log2_num_index_tiles_per_row_per_col.m_value)
    .num_index_layers(m_num_index_layers.m_value)
    .delayed(m_image_atlas_delayed_upload.m_value);
  m_image_atlas = FASTUIDRAWnew fastuidraw::gl::ImageAtlasGL(m_image_atlas_params);

  fastuidraw::ivec3 texel_dims(m_texel_store_width.m_value, m_texel_store_height.m_value, m_texel_store_num_layers.m_value);
  m_glyph_atlas_params
    .texel_store_dimensions(texel_dims)
    .number_floats(m_geometry_store_size.m_value)
    .alignment(m_geometry_store_alignment.m_value)
    .delayed(m_glyph_atlas_delayed_upload.m_value);

  switch(m_glyph_geometry_backing_store_type.m_value.m_value)
    {
    case glyph_geometry_backing_store_texture_buffer:
      m_glyph_atlas_params.use_texture_buffer_geometry_store();
      break;

    case glyph_geometry_backing_store_texture_array:
      m_glyph_atlas_params.use_texture_2d_array_geometry_store(m_glyph_geometry_backing_texture_log2_w.m_value,
                                                               m_glyph_geometry_backing_texture_log2_h.m_value);
      break;

    default:
      m_glyph_atlas_params.use_optimal_geometry_store_backing();
      switch(m_glyph_atlas_params.glyph_geometry_backing_store_type())
        {
        case fastuidraw::glsl::PainterBackendGLSL::glyph_geometry_tbo:
          {
            std::cout << "Glyph Geometry Store: auto selected buffer\n";
          }
          break;

        case fastuidraw::glsl::PainterBackendGLSL::glyph_geometry_texture_array:
          {
            fastuidraw::ivec2 log2_dims(m_glyph_atlas_params.texture_2d_array_geometry_store_log2_dims());
            std::cout << "Glyph Geometry Store: auto selected texture with dimensions: (2^"
                      << log2_dims.x() << ", 2^" << log2_dims.y() << ") = "
                      << fastuidraw::ivec2(1 << log2_dims.x(), 1 << log2_dims.y())
                      << "\n";
          }
          break;
        }
    }
  m_glyph_atlas = FASTUIDRAWnew fastuidraw::gl::GlyphAtlasGL(m_glyph_atlas_params);

  m_colorstop_atlas_params
    .width(m_color_stop_atlas_width.m_value)
    .num_layers(m_color_stop_atlas_layers.m_value)
    .delayed(m_color_stop_atlas_delayed_upload.m_value);

  if (m_color_stop_atlas_use_optimal_width.m_value)
    {
      m_colorstop_atlas_params.optimal_width();
      std::cout << "Colorstop Atlas optimal width selected to be "
                << m_colorstop_atlas_params.width() << "\n";
    }

  m_colorstop_atlas = FASTUIDRAWnew fastuidraw::gl::ColorStopAtlasGL(m_colorstop_atlas_params);

  if (m_painter_msaa.m_value > 1)
    {
      if (m_provide_auxiliary_image_buffer.m_value.m_value != fastuidraw::glsl::PainterBackendGLSL::no_auxiliary_buffer)
        {
          std::cout << "Auxilary buffer cannot be used with painter_msaa "
                    << "(and there is little reason since it is used only for shader-based anti-aliasing)\n"
                    << std::flush;
          m_provide_auxiliary_image_buffer.m_value.m_value = fastuidraw::glsl::PainterBackendGLSL::no_auxiliary_buffer;
        }

      if (m_blend_type.m_value.m_value == fastuidraw::PainterBlendShader::framebuffer_fetch)
        {
          std::cout << "WARNING: using framebuffer fetch with painter_msaa makes all fragment shading happen per sample (which is terribly expensive)\n"
                    << std::flush;
        }
    }

  m_painter_base_params.alignment(m_painter_alignment.m_value);
  m_painter_params
    .image_atlas(m_image_atlas)
    .glyph_atlas(m_glyph_atlas)
    .colorstop_atlas(m_colorstop_atlas)
    .attributes_per_buffer(m_painter_attributes_per_buffer.m_value)
    .indices_per_buffer(m_painter_indices_per_buffer.m_value)
    .data_blocks_per_store_buffer(m_painter_data_blocks_per_buffer.m_value)
    .number_pools(m_painter_number_pools.m_value)
    .break_on_shader_change(m_painter_break_on_shader_change.m_value)
    .clipping_type(m_use_hw_clip_planes.m_value ?
                   fastuidraw::gl::PainterBackendGL::clipping_via_clip_distance :
                   fastuidraw::gl::PainterBackendGL::clipping_via_discard)
    .vert_shader_use_switch(m_uber_vert_use_switch.m_value)
    .frag_shader_use_switch(m_uber_frag_use_switch.m_value)
    .blend_shader_use_switch(m_uber_blend_use_switch.m_value)
    .unpack_header_and_brush_in_frag_shader(m_unpack_header_and_brush_in_frag_shader.m_value)
    .data_store_backing(m_data_store_backing.m_value.m_value)
    .assign_layout_to_vertex_shader_inputs(m_assign_layout_to_vertex_shader_inputs.m_value)
    .assign_layout_to_varyings(m_assign_layout_to_varyings.m_value)
    .assign_binding_points(m_assign_binding_points.m_value)
    .separate_program_for_discard(m_separate_program_for_discard.m_value)
    .provide_auxiliary_image_buffer(m_provide_auxiliary_image_buffer.m_value.m_value)
    .default_stroke_shader_aa_type(m_provide_auxiliary_image_buffer.m_value.m_value != fastuidraw::glsl::PainterBackendGLSL::no_auxiliary_buffer?
                                   fastuidraw::PainterStrokeShader::cover_then_draw :
                                   fastuidraw::PainterStrokeShader::draws_solid_then_fuzz)
    .blend_type(m_blend_type.m_value.m_value);

  m_backend = FASTUIDRAWnew fastuidraw::gl::PainterBackendGL(m_painter_params, m_painter_base_params);
  m_painter = FASTUIDRAWnew fastuidraw::Painter(m_backend);
  m_glyph_cache = FASTUIDRAWnew fastuidraw::GlyphCache(m_painter->glyph_atlas());
  m_glyph_selector = FASTUIDRAWnew fastuidraw::GlyphSelector(m_glyph_cache);
  m_ft_lib = FASTUIDRAWnew fastuidraw::FreeTypeLib();

  if (m_print_painter_config.m_value)
    {
      std::cout << "\nPainterBackendGL configuration:\n";

      #define LAZY(X) do {                                              \
        std::cout << std::setw(40) << #X": " << std::setw(8)            \
                  << m_backend->configuration_gl().X()                  \
                  << "  (requested " << m_painter_params.X() << ")\n";  \
      } while(0)

      #define LAZY_ENUM(X) do {                                               \
        std::cout << std::setw(40) << #X": " << std::setw(8)            \
                  << make_enum_wrapper(m_backend->configuration_gl().X()) \
                  << "  (requested " << make_enum_wrapper(m_painter_params.X()) \
                  << ")\n";                                             \
      } while(0)

      LAZY(attributes_per_buffer);
      LAZY(indices_per_buffer);
      LAZY(number_pools);
      LAZY_ENUM(break_on_shader_change);
      LAZY_ENUM(vert_shader_use_switch);
      LAZY_ENUM(frag_shader_use_switch);
      LAZY_ENUM(blend_shader_use_switch);
      LAZY_ENUM(unpack_header_and_brush_in_frag_shader);
      LAZY_ENUM(separate_program_for_discard);
      LAZY(data_blocks_per_store_buffer);
      LAZY_ENUM(data_store_backing);
      LAZY_ENUM(clipping_type);
      LAZY_ENUM(assign_layout_to_vertex_shader_inputs);
      LAZY_ENUM(assign_layout_to_varyings);
      LAZY_ENUM(assign_binding_points);
      LAZY_ENUM(default_stroke_shader_aa_type);
      LAZY_ENUM(blend_type);
      LAZY_ENUM(provide_auxiliary_image_buffer);
      std::cout << std::setw(40) << "alignment: " << std::setw(8) << m_backend->configuration_base().alignment()
                << "  (requested " << m_painter_base_params.alignment()
                << ")\n\n\n";

      #undef LAZY
      #undef LAZY_ENUM
    }

  if (m_print_painter_shader_ids.m_value)
    {
      const fastuidraw::PainterShaderSet &sh(m_painter->default_shaders());
      std::cout << "Default shader IDs:\n";

      std::cout << "\tGlyph Shaders:\n";
      print_glyph_shader_ids(sh.glyph_shader());

      std::cout << "\tAnisoptropic Glyph shaders\n";
      print_glyph_shader_ids(sh.glyph_shader_anisotropic());

      std::cout << "\tSolid StrokeShaders:\n";
      print_stroke_shader_ids(sh.stroke_shader());

      std::cout << "\tDashed Stroke Shader:\n";
      print_dashed_stroke_shader_ids(sh.dashed_stroke_shader());

      std::cout << "\tFill Shader:" << sh.fill_shader().item_shader()->tag() << "\n";
    }

  m_painter_params = m_backend->configuration_gl();

  fastuidraw::gl::PainterBackendGL::SurfaceGL::Properties props;
  props
    .dimensions(fastuidraw::ivec2(w, h))
    .msaa(m_painter_msaa.m_value);

  m_surface = FASTUIDRAWnew fastuidraw::gl::PainterBackendGL::SurfaceGL(props);
  derived_init(w, h);
}

void
sdl_painter_demo::
on_resize(int w, int h)
{
  fastuidraw::ivec2 wh(w, h);
  if (wh != m_surface->dimensions())
    {
      fastuidraw::gl::PainterBackendGL::SurfaceGL::Properties props;
      fastuidraw::PainterBackend::Surface::Viewport vwp(0, 0, w, h);

      props
        .dimensions(fastuidraw::ivec2(w, h))
        .msaa(m_painter_msaa.m_value);
      m_surface = FASTUIDRAWnew fastuidraw::gl::PainterBackendGL::SurfaceGL(props);
      m_surface->viewport(vwp);
    }
}

void
sdl_painter_demo::
draw_text(const std::string &text, float pixel_size,
          fastuidraw::reference_counted_ptr<const fastuidraw::FontBase> font,
          fastuidraw::GlyphRender renderer,
          const fastuidraw::PainterData &draw,
	  enum fastuidraw::PainterEnums::glyph_orientation orientation)
{
  std::istringstream str(text);
  std::vector<fastuidraw::Glyph> glyphs;
  std::vector<fastuidraw::vec2> positions;
  std::vector<uint32_t> chars;
  fastuidraw::PainterAttributeData P;

  create_formatted_text(str, renderer, pixel_size, font,
                        m_glyph_selector, glyphs, positions, chars,
			nullptr, nullptr, orientation);

  fastuidraw::PainterAttributeDataFillerGlyphs filler(cast_c_array(positions),
						      cast_c_array(glyphs), pixel_size,
						      orientation);
  P.set_data(filler);
  m_painter->draw_glyphs(draw, P);
}
