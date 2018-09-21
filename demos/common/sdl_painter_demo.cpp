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

      case fastuidraw::gl::PainterBackendGL::data_store_ssbo:
        str << "ssbo";
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
      case fastuidraw::gl::PainterBackendGL::clipping_via_gl_clip_distance:
        str << "on";
        break;

      case fastuidraw::gl::PainterBackendGL::clipping_via_discard:
        str << "off";
        break;

      case fastuidraw::gl::PainterBackendGL::clipping_via_skip_color_write:
        str << "emulate_skip_color_write";
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
             enum_wrapper<enum fastuidraw::glsl::PainterShaderRegistrarGLSL::compositing_type_t> v)
  {
    switch(v.m_v)
      {
      case fastuidraw::glsl::PainterShaderRegistrarGLSL::compositing_single_src:
        str << "single_src";
        break;

      case fastuidraw::glsl::PainterShaderRegistrarGLSL::compositing_dual_src:
        str << "dual_src";
        break;

      case fastuidraw::glsl::PainterShaderRegistrarGLSL::compositing_framebuffer_fetch:
        str << "framebuffer_fetch";
        break;

      case fastuidraw::glsl::PainterShaderRegistrarGLSL::compositing_interlock:
        str << "interlock";
        break;

      default:
        str << "invalid value";
      }

    return str;
  }

  std::ostream&
  operator<<(std::ostream &str,
             enum_wrapper<enum fastuidraw::glsl::PainterShaderRegistrarGLSL::auxiliary_buffer_t> v)
  {
    switch(v.m_v)
      {
      case fastuidraw::glsl::PainterShaderRegistrarGLSL::no_auxiliary_buffer:
        str << "no_auxiliary_buffer";
        break;

      case fastuidraw::glsl::PainterShaderRegistrarGLSL::auxiliary_buffer_atomic:
        str << "auxiliary_buffer_atomic";
        break;

      case fastuidraw::glsl::PainterShaderRegistrarGLSL::auxiliary_buffer_interlock:
        str << "auxiliary_buffer_interlock";
        break;

      case fastuidraw::glsl::PainterShaderRegistrarGLSL::auxiliary_buffer_interlock_main_only:
        str << "auxiliary_buffer_interlock_main_only";
        break;

      case fastuidraw::glsl::PainterShaderRegistrarGLSL::auxiliary_buffer_framebuffer_fetch:
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

  GLuint
  ready_pixel_counter_ssbo(unsigned int binding_index)
  {
    GLuint return_value(0);
    uint32_t zero[2] = {0, 0};

    glGenBuffers(1, &return_value);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, return_value);
    glBufferData(GL_SHADER_STORAGE_BUFFER, 2 * sizeof(uint32_t), zero, GL_STREAM_READ);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, binding_index, return_value);

    return return_value;
  }

  void
  update_pixel_counts(GLuint bo, fastuidraw::vecN<uint64_t, 4> &dst)
  {
    const uint32_t *p;

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, bo);
    p = (const uint32_t*)glMapBufferRange(GL_SHADER_STORAGE_BUFFER,
                                          0, 2 * sizeof(uint32_t), GL_MAP_READ_BIT);
    dst[sdl_painter_demo::frame_number_pixels] = p[0];
    dst[sdl_painter_demo::frame_number_pixels_that_neighbor_helper] = p[1];
    glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    glDeleteBuffers(1, &bo);

    dst[sdl_painter_demo::total_number_pixels] += dst[sdl_painter_demo::frame_number_pixels];
    dst[sdl_painter_demo::total_number_pixels_that_neighbor_helper] += dst[sdl_painter_demo::frame_number_pixels_that_neighbor_helper];
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
  m_glyph_atlas_delayed_upload(m_glyph_atlas_params.delayed(),
                               "glyph_atlas_delayed_upload",
                               "if true delay uploading of data to GL from glyph atlas until atlas flush",
                               *this),
  m_glyph_geometry_backing_store_type(glyph_geometry_backing_store_auto,
                                      enumerated_string_type<enum glyph_geometry_backing_store_t>()
                                      .add_entry("texture_buffer",
                                                 glyph_geometry_backing_store_texture_buffer,
                                                 "use a texture buffer, feature is core in GL but for GLES requires version 3.2, "
                                                 "for GLES version pre-3.2, requires the extension GL_OES_texture_buffer or the "
                                                 "extension GL_EXT_texture_buffer")
                                      .add_entry("texture_array",
                                                 glyph_geometry_backing_store_texture_array,
                                                 "use a 2D texture array to store the glyph geometry data, "
                                                 "GL and GLES have feature in core")
                                      .add_entry("storage_buffer",
                                                 glyph_geometry_backing_store_ssbo,
                                                 "use a shader storage buffer, feature is core starting in GLES 3.1 and available "
                                                 "in GL starting at version 4.2 or via the extension GL_ARB_shader_storage_buffer")
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
  m_use_uber_item_shader(m_painter_params.use_uber_item_shader(),
                         "painter_use_uber_item_shader",
                         "If true, use an uber-shader for all item shaders",
                         *this),
  m_uber_composite_use_switch(m_painter_params.composite_shader_use_switch(),
                          "painter_uber_composite_use_switch",
                          "If true, use a switch statement in uber composite shader dispatch",
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
                                             fastuidraw::glsl::PainterShaderRegistrarGLSL::no_auxiliary_buffer,
                                             "No auxiliary buffer provided")
                                  .add_entry("auxiliary_buffer_atomic",
                                             fastuidraw::glsl::PainterShaderRegistrarGLSL::auxiliary_buffer_atomic,
                                             "Auxiliary buffer through atomic ops; this can be quite poor "
                                             "performing because atomics can be quite slow AND (worse) "
                                             "a draw break appears to issue a memory barrier within each "
                                             "path stroking and after each path stroking. Requires only "
                                             "GL 4.2 (or GL_ARB_shader_image_load_store extension) for GL "
                                             "and GLES 3.1 for GLES")
                                  .add_entry("auxiliary_buffer_interlock",
                                             fastuidraw::glsl::PainterShaderRegistrarGLSL::auxiliary_buffer_interlock,
                                             "Auxiliary buffer with interlock; this is high performant option "
                                             "as it does NOT use atomic ops and does not force any draw call "
                                             "breaks to issue a memory barrier; requires GL_INTEL_fragment_shader_ordering "
                                             "and GL 4.2 (or GL_ARB_shader_image_load_store extension); if requirements "
                                             "are not satisfied will try to fall back to auxiliary_buffer_interlock_main_only "
                                             "and if those are not satisfied will fall back to auxiliary_buffer_atomic and "
                                             "if those requirement are not satsified, then no_auxiliary_buffer")
                                  .add_entry("auxiliary_buffer_interlock_main_only",
                                             fastuidraw::glsl::PainterShaderRegistrarGLSL::auxiliary_buffer_interlock_main_only,
                                             "Auxiliary buffer with interlock; this is high performant option "
                                             "as it does NOT use atomic ops and does not force any draw call "
                                             "breaks to issue a memory barrier; requires GL_ARB_fragment_shader_interlock "
                                             "OR GL_NV_fragment_shader_interlock together with GL 4.2 (or "
                                             "GL_ARB_shader_image_load_store) for GL and GLES 3.1 for GLES; if requirements "
                                             "are not satisfied will try to fall back to auxiliary_buffer_interlock "
                                             "and if those are not satisfied will fall back to auxiliary_buffer_atomic and "
                                             "if those requirement are not satsified, then no_auxiliary_buffer")
                                  .add_entry("auxiliary_buffer_framebuffer_fetch",
                                             fastuidraw::glsl::PainterShaderRegistrarGLSL::auxiliary_buffer_framebuffer_fetch,
                                             "Auxilary buffer via framebuffer fetch; this is high performant option "
                                             "as it does NOT use atomic ops and does not force any draw call "
                                             "breaks to issue a memory barrier; requires GL_EXT_shader_framebuffer_fetch; "
                                             "if requirement is not satisfied will try to fall back to auxiliary_buffer_interlock "
                                             "which if not satisfied will fall back to auxiliary_buffer_interlock_main_only "
                                             "and if those are not satisfied will fall back to auxiliary_buffer_atomic and "
                                             "if those requirement are not satsified, then no_auxiliary_buffer"),
                                  "provide_auxiliary_image_buffer",
                                  "Specifies if and how to provide auxiliary image buffer; "
                                  "will remove rendering artifacts on shader-based anti-aliased "
                                  "transparent path stroking",
                                  *this),
  m_use_hw_clip_planes(m_painter_params.clipping_type(),
                       enumerated_string_type<clipping_type_t>()
                       .add_entry("on", fastuidraw::gl::PainterBackendGL::clipping_via_gl_clip_distance,
                                  "Use HW clip planes via gl_ClipDistance for clipping")
                       .add_entry_alias("true", fastuidraw::gl::PainterBackendGL::clipping_via_gl_clip_distance)
                       .add_entry("off", fastuidraw::gl::PainterBackendGL::clipping_via_discard,
                                  "Use discard in fragment shader for clipping")
                       .add_entry_alias("false", fastuidraw::gl::PainterBackendGL::clipping_via_discard)
                       .add_entry("emulate_skip_color_write",
                                  fastuidraw::gl::PainterBackendGL::clipping_via_skip_color_write,
                                  "Emulate by (virtually) skipping color writes, painter_composite_type "
                                  "must be framebuffer_fetch"),
                       "painter_use_hw_clip_planes",
                       "",
                       *this),
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
                                  "of a texture buffer object usually")
                       .add_entry("ssbo",
                                  fastuidraw::gl::PainterBackendGL::data_store_ssbo,
                                  "use a shader storage buffer object to back the data store. "
                                  "A shader storage buffer can have a very large maximum size"),
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
  m_composite_type(m_painter_params.compositing_type(),
               enumerated_string_type<compositing_type_t>()
               .add_entry("framebuffer_fetch",
                          fastuidraw::glsl::PainterShaderRegistrarGLSL::compositing_framebuffer_fetch,
                          "use a framebuffer fetch (if available) to perform compositing, "
                          "thus all compositing operations are part of uber-shader giving "
                          "more flexibility for composite types (namely W3C support) and "
                          "composite mode changes do not induce pipeline state changes")
               .add_entry("interlock",
                          fastuidraw::glsl::PainterShaderRegistrarGLSL::compositing_interlock,
                          "use image-load store together with interlock (if both available) "
                          "to perform compositing, tus all compositing operations are part of "
                          "uber-shader giving more flexibility for composite types (namely "
                          "W3C support) and composite mode changes do not induce pipeline "
                          "state changes")
               .add_entry("dual_src",
                          fastuidraw::glsl::PainterShaderRegistrarGLSL::compositing_dual_src,
                          "use a dual source compositing (if available) to perform compositing, "
                          "which has far less flexibility for compositing than framebuffer-fetch "
                          "but has far few pipeline states (there are 3 composite mode pipeline states "
                          "and hald of the Porter-Duff composite modes are in one composite mode pipeline state")
               .add_entry("single_src",
                          fastuidraw::glsl::PainterShaderRegistrarGLSL::compositing_single_src,
                          "use single source compositing to perform compositing, "
                          "which is even less flexible than dual_src compositing and "
                          "every Porter-Duff composite mode is a different pipeline state"),
               "painter_composite_type",
               "specifies how the painter will perform compositing",
               *this),
  m_painter_optimal(true, "painter_optimal_auto",
                    "If set to true, override all painter options and "
                    "query the GL/GLES context to configure the options",
                    *this),
  m_demo_options("Demo Options", *this),
  m_print_painter_config(default_value_for_print_painter,
                         "print_painter_config",
                         "Print PainterBackendGL config", *this),
  m_print_painter_shader_ids(default_value_for_print_painter,
                             "print_painter_shader_ids",
                             "Print PainterBackendGL shader IDs", *this),
  m_pixel_counter_stack(-1, "pixel_counter_latency",
                        "If non-negative, will add code to the painter ubder- shader "
                        "to count number of helper and non-helper pixels. The value "
                        "is how many frames to wait before reading the values from the "
                        "atomic buffers that are updated", *this),
  m_num_pixel_counter_buffers(0),
  m_pixel_counts(0, 0, 0, 0)
{}

sdl_painter_demo::
~sdl_painter_demo()
{
  for (GLuint bo : m_pixel_counter_buffers)
    {
      glDeleteBuffers(1, &bo);
    }
}

void
sdl_painter_demo::
init_gl(int w, int h)
{
  int max_layers(0);

  max_layers = fastuidraw::gl::context_get<GLint>(GL_MAX_ARRAY_TEXTURE_LAYERS);
  if (max_layers < m_num_color_layers.value())
    {
      std::cout << "num_color_layers exceeds max number texture layers (" << max_layers
                << "), num_color_layers set to that value.\n";
      m_num_color_layers.value() = max_layers;
    }

  if (max_layers < m_color_stop_atlas_layers.value())
    {
      std::cout << "atlas_layers exceeds max number texture layers (" << max_layers
                << "), atlas_layers set to that value.\n";
      m_color_stop_atlas_layers.value() = max_layers;
    }

  m_image_atlas_params
    .log2_color_tile_size(m_log2_color_tile_size.value())
    .log2_num_color_tiles_per_row_per_col(m_log2_num_color_tiles_per_row_per_col.value())
    .num_color_layers(m_num_color_layers.value())
    .log2_index_tile_size(m_log2_index_tile_size.value())
    .log2_num_index_tiles_per_row_per_col(m_log2_num_index_tiles_per_row_per_col.value())
    .num_index_layers(m_num_index_layers.value())
    .delayed(m_image_atlas_delayed_upload.value());
  m_image_atlas = FASTUIDRAWnew fastuidraw::gl::ImageAtlasGL(m_image_atlas_params);

  fastuidraw::ivec3 texel_dims(m_texel_store_width.value(),
                               m_texel_store_height.value(),
                               m_texel_store_num_layers.value());

  m_glyph_atlas_params
    .texel_store_dimensions(texel_dims)
    .number_floats(m_geometry_store_size.value())
    .delayed(m_glyph_atlas_delayed_upload.value());

  switch(m_glyph_geometry_backing_store_type.value())
    {
    case glyph_geometry_backing_store_texture_buffer:
      m_glyph_atlas_params.use_texture_buffer_geometry_store();
      break;

    case glyph_geometry_backing_store_texture_array:
      m_glyph_atlas_params.use_texture_2d_array_geometry_store(m_glyph_geometry_backing_texture_log2_w.value(),
                                                               m_glyph_geometry_backing_texture_log2_h.value());
      break;

    case glyph_geometry_backing_store_ssbo:
      m_glyph_atlas_params.use_storage_buffer_geometry_store();
      break;

    default:
      m_glyph_atlas_params.use_optimal_geometry_store_backing();
      switch(m_glyph_atlas_params.glyph_geometry_backing_store_type())
        {
        case fastuidraw::glsl::PainterShaderRegistrarGLSL::glyph_geometry_tbo:
          {
            std::cout << "Glyph Geometry Store: auto selected texture buffer\n";
          }
          break;

        case fastuidraw::glsl::PainterShaderRegistrarGLSL::glyph_geometry_ssbo:
          {
            std::cout << "Glyph Geometry Store: auto selected storage buffer\n";
          }
          break;

        case fastuidraw::glsl::PainterShaderRegistrarGLSL::glyph_geometry_texture_array:
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
    .width(m_color_stop_atlas_width.value())
    .num_layers(m_color_stop_atlas_layers.value())
    .delayed(m_color_stop_atlas_delayed_upload.value());

  if (!m_color_stop_atlas_width.set_by_command_line())
    {
      m_colorstop_atlas_params.optimal_width();
      std::cout << "Colorstop Atlas optimal width selected to be "
                << m_colorstop_atlas_params.width() << "\n";
    }

  m_colorstop_atlas = FASTUIDRAWnew fastuidraw::gl::ColorStopAtlasGL(m_colorstop_atlas_params);
  if (m_painter_optimal.value())
    {
      m_painter_params.configure_from_context(m_painter_msaa.value() > 1);
    }

#define APPLY_PARAM(X, Y) do { if (Y.set_by_command_line()) { std::cout << "Apply: "#X": " << Y.value() << "\n"; m_painter_params.X(Y.value()); } } while (0)

  APPLY_PARAM(attributes_per_buffer, m_painter_attributes_per_buffer);
  APPLY_PARAM(indices_per_buffer, m_painter_indices_per_buffer);
  APPLY_PARAM(data_blocks_per_store_buffer, m_painter_data_blocks_per_buffer);
  APPLY_PARAM(number_pools, m_painter_number_pools);
  APPLY_PARAM(break_on_shader_change, m_painter_break_on_shader_change);
  APPLY_PARAM(clipping_type, m_use_hw_clip_planes);
  APPLY_PARAM(vert_shader_use_switch, m_uber_vert_use_switch);
  APPLY_PARAM(frag_shader_use_switch, m_uber_frag_use_switch);
  APPLY_PARAM(composite_shader_use_switch, m_uber_composite_use_switch);
  APPLY_PARAM(unpack_header_and_brush_in_frag_shader, m_unpack_header_and_brush_in_frag_shader);
  APPLY_PARAM(data_store_backing, m_data_store_backing);
  APPLY_PARAM(assign_layout_to_vertex_shader_inputs, m_assign_layout_to_vertex_shader_inputs);
  APPLY_PARAM(assign_layout_to_varyings, m_assign_layout_to_varyings);
  APPLY_PARAM(assign_binding_points, m_assign_binding_points);
  APPLY_PARAM(separate_program_for_discard, m_separate_program_for_discard);
  APPLY_PARAM(provide_auxiliary_image_buffer, m_provide_auxiliary_image_buffer);
  APPLY_PARAM(compositing_type, m_composite_type);
  APPLY_PARAM(use_uber_item_shader, m_use_uber_item_shader);

#undef APPLY_PARAM

  if (!m_painter_optimal.value() || m_provide_auxiliary_image_buffer.set_by_command_line())
    {
      bool use_cover_then_draw;

      use_cover_then_draw = (m_painter_params.provide_auxiliary_image_buffer()
                             != fastuidraw::glsl::PainterShaderRegistrarGLSL::no_auxiliary_buffer);
      m_painter_params
        .default_stroke_shader_aa_type(use_cover_then_draw?
                                       fastuidraw::PainterStrokeShader::cover_then_draw :
                                       fastuidraw::PainterStrokeShader::draws_solid_then_fuzz);
    }


  if (m_painter_msaa.value() > 1)
    {
      if (m_provide_auxiliary_image_buffer.value() != fastuidraw::glsl::PainterShaderRegistrarGLSL::no_auxiliary_buffer)
        {
          std::cout << "Auxilary buffer cannot be used with painter_msaa "
                    << "(and there is little reason since it is used only for shader-based anti-aliasing)\n"
                    << std::flush;
          m_provide_auxiliary_image_buffer.value() = fastuidraw::glsl::PainterShaderRegistrarGLSL::no_auxiliary_buffer;
        }

      if (m_composite_type.value() == fastuidraw::glsl::PainterShaderRegistrarGLSL::compositing_interlock)
        {
          std::cout << "WARNING: using interlock with painter_msaa is not possible, falling back to "
                    << ") framebuffer_fetch\n"
                    << std::flush;
        }

      if (m_composite_type.value() == fastuidraw::glsl::PainterShaderRegistrarGLSL::compositing_framebuffer_fetch)
        {
          std::cout << "WARNING: using framebuffer fetch with painter_msaa makes all fragment shading happen "
                    << "per sample (which is terribly expensive)\n"
                    << std::flush;
        }
    }

  m_painter_params
    .image_atlas(m_image_atlas)
    .glyph_atlas(m_glyph_atlas)
    .colorstop_atlas(m_colorstop_atlas);

  if (m_pixel_counter_stack.value() >= 0)
    {
      fastuidraw::c_string version;
      #ifdef FASTUIDRAW_GL_USE_GLES
        {
          version = "310 es";
        }
      #else
        {
          version = "450";
        }
      #endif
      m_painter_params.glsl_version_override(version);
    }

  m_backend = fastuidraw::gl::PainterBackendGL::create(m_painter_params);

  if (m_pixel_counter_stack.value() >= 0)
    {
      fastuidraw::c_string code;
      fastuidraw::reference_counted_ptr<fastuidraw::glsl::PainterShaderRegistrarGLSL> R;

      m_pixel_counter_buffer_binding_index = fastuidraw::gl::context_get<GLint>(GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS) - 1;
      code =
        "layout(binding = PIXEL_COUNTER_BINDING) buffer pixel_counter_buffer\n"
        "{\n"
        "\tuint num_pixels;\n"
        "\tuint num_neighbor_helper_pixels;\n"
        "};\n"
        "void real_main(void);\n"
        "void main(void)\n"
        "{\n"
        "\tfloat f;\n"
        "\tf = float(gl_HelperInvocation);\n"
        "\tatomicAdd(num_pixels, 1u);\n"
        "\tif(abs(dFdxFine(f)) > 0.0 || abs(dFdyFine(f)) > 0.0)\n"
        "\t\tatomicAdd(num_neighbor_helper_pixels, 1u);\n"
        "\treal_main();\n"
        "}\n";

      R = m_backend->painter_shader_registrar().static_cast_ptr<fastuidraw::glsl::PainterShaderRegistrarGLSL>();
      R->add_fragment_shader_util(fastuidraw::glsl::ShaderSource()
                                  .add_macro("PIXEL_COUNTER_BINDING", m_pixel_counter_buffer_binding_index)
                                  .add_source(code, fastuidraw::glsl::ShaderSource::from_string)
                                  .add_macro("main", "real_main"));
    }

  m_painter = FASTUIDRAWnew fastuidraw::Painter(m_backend);
  m_glyph_cache = FASTUIDRAWnew fastuidraw::GlyphCache(m_painter->glyph_atlas());
  m_glyph_selector = FASTUIDRAWnew fastuidraw::GlyphSelector();
  m_ft_lib = FASTUIDRAWnew fastuidraw::FreeTypeLib();

  if (m_print_painter_config.value())
    {
      std::cout << "\nPainterBackendGL configuration:\n";

      #define LAZY_PARAM(X, Y) do {                                     \
        std::cout << std::setw(40) << Y.name() << ": " << std::setw(8)	\
                  << m_backend->configuration_gl().X()                  \
                  << "  (requested " << m_painter_params.X() << ")\n";  \
      } while(0)

      #define LAZY_PARAM_ENUM(X, Y) do {				\
        std::cout << std::setw(40) << Y.name() <<": " << std::setw(8)	\
                  << make_enum_wrapper(m_backend->configuration_gl().X()) \
                  << "  (requested " << make_enum_wrapper(m_painter_params.X()) \
                  << ")\n";                                             \
      } while(0)

      LAZY_PARAM(attributes_per_buffer, m_painter_attributes_per_buffer);
      LAZY_PARAM(indices_per_buffer, m_painter_indices_per_buffer);
      LAZY_PARAM(data_blocks_per_store_buffer, m_painter_data_blocks_per_buffer);
      LAZY_PARAM(number_pools, m_painter_number_pools);
      LAZY_PARAM_ENUM(break_on_shader_change, m_painter_break_on_shader_change);
      LAZY_PARAM_ENUM(clipping_type, m_use_hw_clip_planes);
      LAZY_PARAM_ENUM(vert_shader_use_switch, m_uber_vert_use_switch);
      LAZY_PARAM_ENUM(frag_shader_use_switch, m_uber_frag_use_switch);
      LAZY_PARAM(use_uber_item_shader, m_use_uber_item_shader);
      LAZY_PARAM_ENUM(composite_shader_use_switch, m_uber_composite_use_switch);
      LAZY_PARAM_ENUM(unpack_header_and_brush_in_frag_shader, m_unpack_header_and_brush_in_frag_shader);
      LAZY_PARAM_ENUM(data_store_backing, m_data_store_backing);
      LAZY_PARAM_ENUM(assign_layout_to_vertex_shader_inputs, m_assign_layout_to_vertex_shader_inputs);
      LAZY_PARAM_ENUM(assign_layout_to_varyings, m_assign_layout_to_varyings);
      LAZY_PARAM_ENUM(assign_binding_points, m_assign_binding_points);
      LAZY_PARAM_ENUM(separate_program_for_discard, m_separate_program_for_discard);
      LAZY_PARAM_ENUM(provide_auxiliary_image_buffer, m_provide_auxiliary_image_buffer);
      LAZY_PARAM_ENUM(compositing_type, m_composite_type);
      std::cout << std::setw(40) << "default_stroke_shader_aa_type:"
		<< std::setw(8) << make_enum_wrapper(m_backend->configuration_gl().default_stroke_shader_aa_type())
		<< " (requested " << make_enum_wrapper(m_painter_params.default_stroke_shader_aa_type())
		<< ")\n"
                << std::setw(40) << "geometry_backing_store_type:"
                << std::setw(8) << m_glyph_atlas->param_values().glyph_geometry_backing_store_type()
                << "\n";

      #undef LAZY_PARAM
      #undef LAZY_ENUM_PARAM
    }

  if (m_print_painter_shader_ids.value())
    {
      const fastuidraw::PainterShaderSet &sh(m_painter->default_shaders());
      std::cout << "Default shader IDs:\n";

      std::cout << "\tGlyph Shaders:\n";
      print_glyph_shader_ids(sh.glyph_shader());

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
    .msaa(m_painter_msaa.value());

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
        .msaa(m_painter_msaa.value());
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
          enum fastuidraw::PainterEnums::screen_orientation orientation)
{
  std::istringstream str(text);
  fastuidraw::GlyphSequence sequence(pixel_size, orientation,
                                     m_glyph_cache);

  create_formatted_text(sequence, str, font, m_glyph_selector);
  if (renderer.valid())
    {
      m_painter->draw_glyphs(draw, sequence.painter_attribute_data(renderer));
    }
  else
    {
      m_painter->draw_glyphs(draw, sequence);
    }
}

void
sdl_painter_demo::
pre_draw_frame(void)
{
  if (m_pixel_counter_stack.value() >= 0)
    {
      GLuint bo;

      bo = ready_pixel_counter_ssbo(m_pixel_counter_buffer_binding_index);
      m_pixel_counter_buffers.push_back(bo);
      ++m_num_pixel_counter_buffers;
    }
}

void
sdl_painter_demo::
post_draw_frame(void)
{
  if (m_pixel_counter_stack.value() >= 0
      && m_num_pixel_counter_buffers > m_pixel_counter_stack.value())
    {
      GLuint bo;

      bo = m_pixel_counter_buffers.front();
      update_pixel_counts(bo, m_pixel_counts);

      m_pixel_counter_buffers.pop_front();
      --m_num_pixel_counter_buffers;
    }
}
