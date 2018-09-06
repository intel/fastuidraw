#include <iostream>
#include <sstream>
#include <fstream>
#include <fastuidraw/glsl/shader_code.hpp>
#include <fastuidraw/text/glyph_cache.hpp>
#include <fastuidraw/text/font_freetype.hpp>
#include <fastuidraw/text/glyph_selector.hpp>
#include <fastuidraw/text/glyph_generate_params.hpp>
#include <fastuidraw/gl_backend/ngl_header.hpp>
#include <fastuidraw/gl_backend/glyph_atlas_gl.hpp>
#include <fastuidraw/gl_backend/gluniform.hpp>
#include <fastuidraw/gl_backend/opengl_trait.hpp>
#include <fastuidraw/gl_backend/gl_context_properties.hpp>
#include <fastuidraw/gl_backend/gl_program.hpp>
#include <fastuidraw/util/util.hpp>
#include "sdl_demo.hpp"
#include "ImageLoader.hpp"
#include "PanZoomTracker.hpp"
#include "text_helper.hpp"
#include "cycle_value.hpp"

using namespace fastuidraw;

#define NUMBER_AA_MODES 5

std::ostream&
operator<<(std::ostream &str, GlyphRender R)
{
  switch(R.m_type)
    {
    case coverage_glyph:
      str << "Coverage(" << R.m_pixel_size << ")";
      break;
    case distance_field_glyph:
      str << "Distance";
      break;
    case curve_pair_glyph:
      str << "CurvePair";
      break;
    }
  return str;
}

class glyph_test:public sdl_demo
{
public:
  glyph_test(void);
  ~glyph_test();

protected:

  virtual
  void
  init_gl(int w, int h);

  virtual
  void
  draw_frame(void);

  virtual
  void
  handle_event(const SDL_Event&);

private:

  void
  set_pvm(int w, int h);

  enum return_code
  create_and_add_font(void);

  void
  ready_program(void);

  void
  ready_attributes_indices(void);


  void
  compute_glyphs_and_positions(GlyphRender renderer, float pixel_size_formatting,
                               std::vector<Glyph> &glyphs,
                               std::vector<vec2> &positions);

  void
  compute_glyphs_and_positions_glyph_set(GlyphRender renderer, float pixel_size_formatting,
                                         std::vector<Glyph> &glyphs,
                                         std::vector<vec2> &positions);

  enum
    {
      draw_glyph_coverage,
      draw_glyph_curvepair,
      draw_glyph_distance,
      draw_glyph_atlas,

      number_draw_modes
    };

  enum
    {
      texel_store_uint,
      texel_store_float,

      number_texel_store_modes,
    };

  enum geometry_backing_store_t
    {
      geometry_backing_store_texture_buffer,
      geometry_backing_store_texture_array,
      geometry_backing_store_ssbo,
      geometry_backing_store_auto,
    };

  class per_program:fastuidraw::noncopyable
  {
  public:
    void
    set(reference_counted_ptr<gl::Program> pr);

    reference_counted_ptr<gl::Program> m_program;
    GLint m_pvm_loc, m_scale_loc, m_translate_loc;
    GLint m_layer_loc;
    GLint m_aa_mode_loc;
    GLint m_fg_color_loc;
  };

  class per_draw:fastuidraw::noncopyable
  {
  public:
    per_draw(void);
    ~per_draw();

    void
    set(vecN<reference_counted_ptr<gl::Program>, 2> prs,
        const std::string &l, PanZoomTrackerSDLEvent *zoomer);

    void
    init_and_bind_vao_vbo_ibo(void);

    void
    init_draw_text(c_array<const Glyph> glyphs, c_array<const vec2> glyph_positions,
                   float scale_factor);

    void
    draw(glyph_test *q, int which_program, const float4x4 &pvm, int layer, unsigned int aa_mode);

    std::string m_label;
    GLuint m_vao;
    GLuint m_vbo, m_ibo;
    int m_index_count;
    PanZoomTrackerSDLEvent *m_zoomer;

    vecN<per_program, number_texel_store_modes> m_programs;
  };

  class single_attribute
  {
  public:
    vec2 m_pos;
    vec3 m_tex_coord_layer;
    unsigned int m_geometry_offset;
    vec3 m_secondary_tex_coord_layer;
  };

  class attribs_per_glyph
  {
  public:
    vecN<single_attribute, 4> m_data;

    void
    pack_data(float SCALE, unsigned int index,
              Glyph g, vec2 p,
              vecN<GLuint, 6> &indices);
  };

  command_line_argument_value<std::string> m_font_file;
  command_line_argument_value<int> m_font_index;
  command_line_argument_value<int> m_coverage_pixel_size;
  command_line_argument_value<int> m_distance_pixel_size;
  command_line_argument_value<float> m_max_distance;
  command_line_argument_value<int> m_curve_pair_pixel_size;
  command_line_argument_value<std::string> m_text;
  command_line_argument_value<bool> m_use_file;
  command_line_argument_value<bool> m_draw_glyph_set;
  command_line_argument_value<int> m_realize_glyphs_thread_count;
  command_line_argument_value<int> m_texel_store_width, m_texel_store_height;
  command_line_argument_value<int> m_texel_store_num_layers, m_geometry_store_size;
  command_line_argument_value<int> m_geometry_store_alignment;
  command_line_argument_value<bool> m_atlas_delayed_upload;
  enumerated_command_line_argument_value<enum geometry_backing_store_t> m_geometry_backing_store_type;
  command_line_argument_value<int> m_geometry_backing_texture_log2_w, m_geometry_backing_texture_log2_h;
  command_line_argument_value<float> m_render_pixel_size;
  command_line_argument_value<float> m_bg_red, m_bg_green, m_bg_blue;
  command_line_argument_value<float> m_fg_red, m_fg_green, m_fg_blue;

  reference_counted_ptr<gl::GlyphAtlasGL> m_glyph_atlas;
  reference_counted_ptr<GlyphCache> m_glyph_cache;
  reference_counted_ptr<GlyphSelector> m_glyph_selector;
  reference_counted_ptr<const FontFreeType> m_font;
  reference_counted_ptr<FreeTypeFace> m_face;

  unsigned int m_current_drawer;
  unsigned int m_texel_access_mode;
  unsigned int m_aa_mode;
  vecN<per_draw, number_draw_modes> m_drawers;

  unsigned int m_current_layer;
  PanZoomTrackerSDLEvent m_zoomer_atlas, m_zoomer_text;

  float4x4 m_pvm;
};

/////////////////////////////
// glyph_test::per_program methods
void
glyph_test::per_program::
set(reference_counted_ptr<gl::Program> pr)
{
  m_program = pr;
  FASTUIDRAWassert(m_program->link_success());

  m_pvm_loc = m_program->uniform_location("pvm");
  FASTUIDRAWassert(m_pvm_loc != -1);

  m_scale_loc = m_program->uniform_location("scale");
  FASTUIDRAWassert(m_scale_loc != -1);

  m_translate_loc = m_program->uniform_location("translate");
  FASTUIDRAWassert(m_translate_loc != -1);

  m_layer_loc = m_program->uniform_location("layer");
  m_aa_mode_loc = m_program->uniform_location("aa_mode");
  m_fg_color_loc = m_program->uniform_location("fg_color");
}


////////////////////////////////////
// glyph_test::per_draw methods
glyph_test::per_draw::
per_draw(void):
  m_vao(0),
  m_vbo(0),
  m_ibo(0),
  m_index_count(0),
  m_zoomer(nullptr)
{}

glyph_test::per_draw::
~per_draw()
{
  if (m_vao != 0)
    {
      glDeleteVertexArrays(1, &m_vao);
    }

  if (m_vbo != 0)
    {
      glDeleteBuffers(1, &m_vbo);
    }

  if (m_ibo != 0)
    {
      glDeleteBuffers(1, &m_ibo);
    }
}

void
glyph_test::per_draw::
set(vecN<reference_counted_ptr<gl::Program>, 2> prs,
    const std::string &l, PanZoomTrackerSDLEvent *zoomer)
{
  m_label = l;
  m_zoomer = zoomer;
  m_programs[0].set(prs[0]);
  m_programs[1].set(prs[1]);
}

void
glyph_test::per_draw::
init_and_bind_vao_vbo_ibo(void)
{
  glGenVertexArrays(1, &m_vao);
  FASTUIDRAWassert(m_vao != 0);
  glBindVertexArray(m_vao);

  glGenBuffers(1, &m_vbo);
  FASTUIDRAWassert(m_vbo != 0);
  glBindBuffer(GL_ARRAY_BUFFER, m_vbo);

  glGenBuffers(1, &m_ibo);
  FASTUIDRAWassert(m_ibo != 0);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ibo);
}

void
glyph_test::per_draw::
init_draw_text(c_array<const Glyph> glyphs, c_array<const vec2> glyph_positions,
               float scale_factor)
{
  std::vector<attribs_per_glyph> attribs;
  std::vector<vecN<GLuint, 6> > indices;
  unsigned int i, endi, glyph_count;

  FASTUIDRAWassert(glyphs.size() == glyph_positions.size());

  /* generate attribute data from glyphs and glyph_posisions.
   */
  attribs.resize(glyphs.size());
  indices.resize(glyphs.size());
  for(i = 0, endi = glyphs.size(), glyph_count = 0; i < endi; ++i)
    {
      if (glyphs[i].valid())
        {
          attribs[glyph_count].pack_data(scale_factor, i, glyphs[i], glyph_positions[i], indices[glyph_count]);
          ++glyph_count;
        }
    }
  attribs.resize(glyph_count);
  indices.resize(glyph_count);

  /* setup VAO:
   */
  init_and_bind_vao_vbo_ibo();

  if (attribs.empty())
    {
      attribs_per_glyph J;
      glBufferData(GL_ARRAY_BUFFER, sizeof(J), &J, GL_STATIC_DRAW);
    }
  else
    {
      glBufferData(GL_ARRAY_BUFFER, sizeof(attribs_per_glyph) * attribs.size(), &attribs[0], GL_STATIC_DRAW);
    }

  glEnableVertexAttribArray(0);
  gl::VertexAttribPointer(0, gl::opengl_trait_values<vec2>(sizeof(single_attribute), offsetof(single_attribute, m_pos)));

  glEnableVertexAttribArray(1);
  gl::VertexAttribPointer(1, gl::opengl_trait_values<vec3>(sizeof(single_attribute), offsetof(single_attribute, m_tex_coord_layer)));

  glEnableVertexAttribArray(2);
  gl::VertexAttribIPointer(2, gl::opengl_trait_values<unsigned int>(sizeof(single_attribute), offsetof(single_attribute, m_geometry_offset)));

  glEnableVertexAttribArray(3);
  gl::VertexAttribPointer(3, gl::opengl_trait_values<vec3>(sizeof(single_attribute), offsetof(single_attribute, m_secondary_tex_coord_layer)));


  if (indices.empty())
    {
      vecN<GLuint, 6> I(0);
      glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(I), &I, GL_STATIC_DRAW);
    }
  else
    {
      glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(vecN<GLuint, 6>) * indices.size(), &indices[0], GL_STATIC_DRAW);
    }

  m_index_count = 6 * indices.size();
}

void
glyph_test::per_draw::
draw(glyph_test *q, int which_program, const float4x4 &pvm, int layer, unsigned int aa_mode)
{
  m_programs[which_program].m_program->use_program();
  glBindVertexArray(m_vao);
  gl::Uniform(m_programs[which_program].m_pvm_loc, pvm);
  gl::Uniform(m_programs[which_program].m_translate_loc, m_zoomer->transformation().translation());
  gl::Uniform(m_programs[which_program].m_scale_loc, m_zoomer->transformation().scale());

  if (m_programs[which_program].m_layer_loc != -1)
    {
      gl::Uniform(m_programs[which_program].m_layer_loc, layer);
    }

  if (m_programs[which_program].m_aa_mode_loc != -1)
    {
      gl::Uniform(m_programs[which_program].m_aa_mode_loc, aa_mode);
    }

  if (m_programs[which_program].m_fg_color_loc != -1)
    {
      gl::Uniform(m_programs[which_program].m_fg_color_loc,
                  vec3(q->m_fg_red.value(), q->m_fg_green.value(), q->m_fg_blue.value()));
    }

  glDrawElements(GL_TRIANGLES, m_index_count, GL_UNSIGNED_INT, nullptr);
}

//////////////////////////////////////////
// glyph_test::attribs_per_glyph methods
void
glyph_test::attribs_per_glyph::
pack_data(float SCALE, unsigned int i, Glyph G, vec2 p, vecN<GLuint, 6> &indices)
{
  indices[0] = 4 * i;
  indices[1] = 4 * i + 1;
  indices[2] = 4 * i + 2;

  indices[3] = 4 * i;
  indices[4] = 4 * i + 2;
  indices[5] = 4 * i + 3;

  enum return_code R;
  R = G.upload_to_atlas();
  FASTUIDRAWassert(R == routine_success);
  FASTUIDRAWunused(R);

  c_array<const GlyphLocation> atlas_locations(G.atlas_locations());
  GlyphLocation atlas_location;
  GlyphLocation secondary_atlas_location;

  if (atlas_locations.size() >= 1)
    {
      atlas_location = atlas_locations[0];
    }

  if (atlas_locations.size() >= 2)
    {
      secondary_atlas_location = atlas_locations[1];
    }

  float layer(atlas_location.layer());
  float layer2(secondary_atlas_location.layer());
  vec2 tex_size(atlas_location.size());
  vec2 atlas_loc(atlas_location.location());
  vec2 secondary_atlas_loc(secondary_atlas_location.location());
  vec2 t_bl(atlas_loc), t_tr(t_bl + tex_size);
  vec2 t2_bl(secondary_atlas_loc), t2_tr(t2_bl + tex_size);
  vec2 glyph_size(SCALE * G.layout().size());
  vec2 p_bl, p_tr;

  p_bl.x() = p.x() + SCALE * G.layout().horizontal_layout_offset().x();
  p_tr.x() = p_bl.x() + glyph_size.x();

  p_bl.y() = p.y() - SCALE * G.layout().horizontal_layout_offset().y();
  p_tr.y() = p_bl.y() - glyph_size.y();

  m_data[0].m_pos             = vec2(p_bl.x(), p_bl.y());
  m_data[0].m_tex_coord_layer = vec3(t_bl.x(), t_bl.y(), layer);
  m_data[0].m_geometry_offset = G.geometry_offset();
  m_data[0].m_secondary_tex_coord_layer = vec3(t2_bl.x(), t2_bl.y(), layer2);

  m_data[1].m_pos             = vec2(p_tr.x(), p_bl.y());
  m_data[1].m_tex_coord_layer = vec3(t_tr.x(), t_bl.y(), layer);
  m_data[1].m_geometry_offset = G.geometry_offset();
  m_data[1].m_secondary_tex_coord_layer = vec3(t2_tr.x(), t2_bl.y(), layer2);

  m_data[2].m_pos             = vec2(p_tr.x(), p_tr.y());
  m_data[2].m_tex_coord_layer = vec3(t_tr.x(), t_tr.y(), layer);
  m_data[2].m_geometry_offset = G.geometry_offset();
  m_data[2].m_secondary_tex_coord_layer = vec3(t2_tr.x(), t2_tr.y(), layer2);

  m_data[3].m_pos             = vec2(p_bl.x(), p_tr.y());
  m_data[3].m_tex_coord_layer = vec3(t_bl.x(), t_tr.y(), layer);
  m_data[3].m_geometry_offset = G.geometry_offset();
  m_data[3].m_secondary_tex_coord_layer = vec3(t2_bl.x(), t2_tr.y(), layer2);

  if (layer2 != -1)
    {
      std::cout << "Needs secondary: glyph_code = " << G.layout().glyph_code()
                << "\n\tglyph_size=" << glyph_size << " at " << p_bl << ":" << p_tr
                << "\n\tfrom location=" << p
                << "\n\ttex_size=" << tex_size << " at " << t_bl << ":" << layer
                << " and " << t2_bl << ":" << layer2
                << "\n\tglyph_offset=" << G.layout().horizontal_layout_offset()
                << "\n\toriginal_size=" << G.layout().size()
                << "\n\tadvance=" << G.layout().advance()
                << "\n\toffset = " << G.geometry_offset()
                << "\n";
    }

}

/////////////////////////////////////
// glyph_test methods
glyph_test::
glyph_test(void):
  m_font_file(default_font(), "font", "font to use", *this),
  m_font_index(0, "font_index", "face index into font file to use if font file has multiple fonts", *this),
  m_coverage_pixel_size(24, "coverage_pixel_size", "Pixel size at which to create coverage glyphs", *this),
  m_distance_pixel_size(48, "distance_pixel_size", "Pixel size at which to create distance field glyphs", *this),
  m_max_distance(GlyphGenerateParams::distance_field_max_distance(),
                 "max_distance",
                 "value to use for max distance in pixels "
                 "when generating distance field glyphs", *this),
  m_curve_pair_pixel_size(48, "curvepair_pixel_size", "Pixel size at which to create distance curve pair glyphs", *this),
  m_text("Hello World!", "text", "text to draw to the screen", *this),
  m_use_file(false, "use_file", "if true the value for text gives a filename to display", *this),
  m_draw_glyph_set(false, "draw_glyph_set", "if true, display all glyphs of font instead of text", *this),
  m_realize_glyphs_thread_count(1, "realize_glyphs_thread_count",
                                "If draw_glyph_set is true, gives the number of threads to use "
                                "to create the glyph data",
                                *this),
  m_texel_store_width(1024, "texel_store_width", "width of texel store", *this),
  m_texel_store_height(1024, "texel_store_height", "height of texel store", *this),
  m_texel_store_num_layers(16, "texel_store_num_layers", "number of layers of texel store", *this),
  m_geometry_store_size(1024 * 1024, "geometry_store_size", "size of geometry store in floats", *this),
  m_geometry_store_alignment(4, "geometry_store_alignment",
                              "alignment of the geometry store, must be one of 1, 2, 3 or 4", *this),
  m_atlas_delayed_upload(false,
                         "atlas_delayed_upload",
                         "if true delay uploading of data to GL from glyph atlas until atlas flush",
                         *this),
  m_geometry_backing_store_type(geometry_backing_store_auto,
                                enumerated_string_type<enum geometry_backing_store_t>()
                                .add_entry("buffer",
                                           geometry_backing_store_texture_buffer,
                                           "use a texture buffer, feature is core in GL but for GLES requires version 3.2, "
                                           "for GLES version pre-3.2, requires the extension GL_OES_texture_buffer or the "
                                           "extension GL_EXT_texture_buffer")
                                .add_entry("texture_array",
                                           geometry_backing_store_texture_array,
                                           "use a 2D texture array to store the glyph geometry data, "
                                           "GL and GLES have feature in core")
                                .add_entry("ssbo",
                                           geometry_backing_store_ssbo,
                                           "use an SSBO, requires GLES 3.1 or GL 4.3 or the extension "
                                           "GL_ARB_shader_storage_buffer_object")
                                .add_entry("auto",
                                           geometry_backing_store_auto,
                                           "query context and decide optimal value"),
                                "geometry_backing_store_type",
                                "Determines how the glyph geometry store is backed.",
                                *this),
  m_geometry_backing_texture_log2_w(10, "geometry_backing_texture_log2_w",
                                    "If geometry_backing_store_type is set to texture_array, then "
                                    "this gives the log2 of the width of the texture array", *this),
  m_geometry_backing_texture_log2_h(10, "geometry_backing_texture_log2_h",
                                    "If geometry_backing_store_type is set to texture_array, then "
                                    "this gives the log2 of the height of the texture array", *this),
  m_render_pixel_size(24.0f, "render_pixel_size", "pixel size at which to display glyphs", *this),
  m_bg_red(1.0f, "bg_red", "Background Red", *this),
  m_bg_green(1.0f, "bg_green", "Background Green", *this),
  m_bg_blue(1.0f, "bg_blue", "Background Blue", *this),
  m_fg_red(0.0f, "fg_red", "Foreground Red", *this),
  m_fg_green(0.0f, "fg_green", "Foreground Green", *this),
  m_fg_blue(0.0f, "fg_blue", "Foreground Blue", *this),
  m_current_drawer(draw_glyph_curvepair),
  m_texel_access_mode(texel_store_uint),
  m_aa_mode(0),
  m_current_layer(0)
{
  std::cout << "Controls:\n"
            << "\td:cycle drawing mode: draw coverage glyph, draw distance glyphs, draw atlas [hold shift, control or mode to reverse cycle]\n"
            << "\tn:when drawing glyph atlas, goto next layer\n"
            << "\tp:when drawing glyph atlas, goto previous layer\n"
            << "\tt:toggle between accessing texel store as uint or not\n"
            << "\ta:when drawing curve pair or distance field glyphs, cycle anti-alias mode\n"
            << "\tz:reset zoom factor to 1.0\n"
            << "\tMouse Drag (left button): pan\n"
            << "\tHold Mouse (left button), then drag up/down: zoom out/in\n";
}

glyph_test::
~glyph_test()
{
}


enum return_code
glyph_test::
create_and_add_font(void)
{
  reference_counted_ptr<FreeTypeFace::GeneratorFile> gen;

  gen = FASTUIDRAWnew FreeTypeFace::GeneratorFile(m_font_file.value().c_str(), m_font_index.value());
  m_face = gen->create_face();

  GlyphGenerateParams::distance_field_max_distance(m_max_distance.value());
  GlyphGenerateParams::distance_field_pixel_size(m_distance_pixel_size.value());
  GlyphGenerateParams::curve_pair_pixel_size(m_curve_pair_pixel_size.value());

  if (m_face)
    {
      m_font = FASTUIDRAWnew FontFreeType(gen);
      m_glyph_selector->add_font(m_font);
      return routine_success;
    }
  else
    {
      std::cout << "\n-----------------------------------------------------"
                << "\nWarning: unable to create font from file \""
                << m_font_file.value() << "\"\n"
                << "-----------------------------------------------------\n";
      return routine_fail;
    }
}

void
glyph_test::
set_pvm(int w, int h)
{
  float_orthogonal_projection_params proj(0, w, h, 0);
  m_pvm = float4x4(proj);
}

void
glyph_test::
init_gl(int w, int h)
{
  ivec3 texel_dims(m_texel_store_width.value(), m_texel_store_height.value(),
                   m_texel_store_num_layers.value());

  set_pvm(w, h);

  gl::GlyphAtlasGL::params glyph_atlas_options;
  glyph_atlas_options
    .texel_store_dimensions(texel_dims)
    .number_floats(m_geometry_store_size.value())
    .alignment(m_geometry_store_alignment.value())
    .delayed(m_atlas_delayed_upload.value());

  switch(m_geometry_backing_store_type.value())
    {
    case geometry_backing_store_texture_buffer:
      glyph_atlas_options.use_texture_buffer_geometry_store();
      break;

    case geometry_backing_store_texture_array:
      glyph_atlas_options.use_texture_2d_array_geometry_store(m_geometry_backing_texture_log2_w.value(),
                                                              m_geometry_backing_texture_log2_h.value());
      break;

    case geometry_backing_store_ssbo:
      glyph_atlas_options.use_storage_buffer_geometry_store();
      break;

    default:
      glyph_atlas_options.use_optimal_geometry_store_backing();
      switch(glyph_atlas_options.glyph_geometry_backing_store_type())
        {
        case fastuidraw::glsl::PainterShaderRegistrarGLSL::glyph_geometry_tbo:
          {
            std::cout << "Glyph Geometry Store: auto selected texture buffer (tbo)\n";
          }
          break;

        case fastuidraw::glsl::PainterShaderRegistrarGLSL::glyph_geometry_ssbo:
          {
            std::cout << "Glyph Geometry Store: auto selected shader storage buffer (ssbo)\n";
          }
          break;

        case fastuidraw::glsl::PainterShaderRegistrarGLSL::glyph_geometry_texture_array:
          {
            fastuidraw::ivec2 log2_dims(glyph_atlas_options.texture_2d_array_geometry_store_log2_dims());
            std::cout << "Glyph Geometry Store: auto selected texture with dimensions: (2^"
                      << log2_dims.x() << ", 2^" << log2_dims.y() << ") = "
                      << fastuidraw::ivec2(1 << log2_dims.x(), 1 << log2_dims.y())
                      << "\n";
          }
          break;
        }
    }

  m_glyph_atlas = FASTUIDRAWnew gl::GlyphAtlasGL(glyph_atlas_options);
  m_glyph_cache = FASTUIDRAWnew GlyphCache(m_glyph_atlas);
  m_glyph_selector = FASTUIDRAWnew GlyphSelector();

  if (create_and_add_font() == routine_fail)
    {
      end_demo(-1);
      return;
    }

  ready_program();
  ready_attributes_indices();
  m_zoomer_text.transformation(ScaleTranslate<float>(vec2(0.0f, m_render_pixel_size.value())));
}

void
glyph_test::
ready_program(void)
{
  vecN<reference_counted_ptr<gl::Program>, number_texel_store_modes> pr;
  vecN<std::string, number_texel_store_modes> macros;
  std::string glyph_geom_mode;
  ivec2 geom_log2_dims(0, 0);
  bool need_ssbo(false);

  macros[texel_store_uint] = "USE_UINT_TEXEL_FETCH";
  macros[texel_store_float] = "USE_FLOAT_TEXEL_FETCH";

  switch(m_glyph_atlas->geometry_binding_point())
    {
    case GL_TEXTURE_BUFFER:
      glyph_geom_mode = "GLYPH_GEOMETRY_USE_TEXTURE_BUFFER";
      break;

    case GL_TEXTURE_2D_ARRAY:
      glyph_geom_mode = "GLYPH_GEOMETRY_USE_TEXTURE_2D_ARRAY";
      geom_log2_dims = m_glyph_atlas->geometry_texture_as_2d_array_log2_dims();
      break;

    case GL_SHADER_STORAGE_BUFFER:
      glyph_geom_mode = "GLYPH_GEOMETRY_USE_SSBO";
      need_ssbo = true;
      break;
    }

  for(int i = 0; i < number_texel_store_modes; ++i)
    {
      pr[i] = FASTUIDRAWnew gl::Program(glsl::ShaderSource()
                                        .specify_version(gl::Shader::default_shader_version())
                                        .add_source("glyph.vert.glsl.resource_string",
                                                    glsl::ShaderSource::from_resource),
                                        glsl::ShaderSource()
                                        .specify_version(gl::Shader::default_shader_version())
                                        .add_macro(macros[i].c_str())
                                        .add_source("gles_prec.frag.glsl.resource_string",
                                                    glsl::ShaderSource::from_resource)
                                        .add_source("coverage_glyph.frag.glsl.resource_string",
                                                    glsl::ShaderSource::from_resource),
                                        gl::PreLinkActionArray()
                                        .add_binding("attrib_p", 0)
                                        .add_binding("attrib_tex_coord_layer", 1),
                                        gl::ProgramInitializerArray()
                                        .add_sampler_initializer("glyph_texel_store", 0));
    }
  m_drawers[draw_glyph_coverage].set(pr, "Coverage Text", &m_zoomer_text);



  for(int i = 0; i < number_texel_store_modes; ++i)
    {
      pr[i] = FASTUIDRAWnew gl::Program(glsl::ShaderSource()
                                        .specify_version(gl::Shader::default_shader_version())
                                        .add_source("glyph.vert.glsl.resource_string",
                                                    glsl::ShaderSource::from_resource),
                                        glsl::ShaderSource()
                                        .specify_version(gl::Shader::default_shader_version())
                                        .add_macro(macros[i].c_str())
                                        .add_source("gles_prec.frag.glsl.resource_string",
                                                    glsl::ShaderSource::from_resource)
                                        .add_source("perform_aa.frag.glsl.resource_string",
                                                    glsl::ShaderSource::from_resource)
                                        .add_source("distance_glyph.frag.glsl.resource_string",
                                                    glsl::ShaderSource::from_resource),
                                        gl::PreLinkActionArray()
                                        .add_binding("attrib_p", 0)
                                        .add_binding("attrib_tex_coord_layer", 1),
                                        gl::ProgramInitializerArray()
                                        .add_sampler_initializer("glyph_texel_store", 0));
    }
  m_drawers[draw_glyph_distance].set(pr, "Distance Text", &m_zoomer_text);

  glsl::ShaderSource curve_pair_func;
  curve_pair_func = glsl::code::curvepair_compute_pseudo_distance(m_glyph_atlas->geometry_store()->alignment(),
                                                                  "curvepair_pseudo_distance",
                                                                  "fetch_glyph_geometry_data",
                                                                  true);
  for(int i = 0; i < number_texel_store_modes; ++i)
    {
      glsl::ShaderSource vert, frag;
      gl::ContextProperties ctx;

      #ifdef FASTUIDRAW_GL_USE_GLES
        {
          c_string version;

          if (ctx.version() >= ivec2(3, 2))
            {
              version = "320 es";
            }
          else if (ctx.version() >= ivec2(3,1))
            {
              version = "310 es";
            }
          else
            {
              version = "300 es";
            }

          vert
            .specify_version(version)
            .specify_extension("GL_OES_texture_buffer", glsl::ShaderSource::enable_extension)
            .specify_extension("GL_EXT_texture_buffer", glsl::ShaderSource::enable_extension);
          frag
            .specify_version(version)
            .specify_extension("GL_OES_texture_buffer", glsl::ShaderSource::enable_extension)
            .specify_extension("GL_EXT_texture_buffer", glsl::ShaderSource::enable_extension);
        }
      #else
        {
          c_string version("330");
          if (need_ssbo)
            {
              if (ctx.version() >= ivec2(4, 3))
                {
                  version = "430";
                }
              else if(ctx.has_extension("GL_ARB_shader_storage_buffer_object"))
                {
                  frag.specify_extension("GL_ARB_shader_storage_buffer_object", glsl::ShaderSource::require_extension);
                  vert.specify_extension("GL_ARB_shader_storage_buffer_object", glsl::ShaderSource::require_extension);
                }
            }
          vert.specify_version(version);
          frag.specify_version(version);
        }
      #endif

      vert
        .add_macro(glyph_geom_mode.c_str())
        .add_source("glyph.vert.glsl.resource_string", glsl::ShaderSource::from_resource);

      frag
        .add_macro(macros[i].c_str())
        .add_macro(glyph_geom_mode.c_str())
        .add_macro("GLYPH_GEOM_WIDTH_LOG2", geom_log2_dims.x())
        .add_macro("GLYPH_GEOM_HEIGHT_LOG2", geom_log2_dims.y())
        .add_source("gles_prec.frag.glsl.resource_string", glsl::ShaderSource::from_resource)
        .add_source("perform_aa.frag.glsl.resource_string", glsl::ShaderSource::from_resource)
        .add_source("curvepair_glyph.frag.glsl.resource_string", glsl::ShaderSource::from_resource)
        .add_source(curve_pair_func);


      pr[i] = FASTUIDRAWnew gl::Program(vert, frag,
                                        gl::PreLinkActionArray()
                                        .add_binding("attrib_p", 0)
                                        .add_binding("attrib_tex_coord_layer", 1)
                                        .add_binding("attrib_geometry_data_location", 2)
                                        .add_binding("attrib_secondary_tex_coord_layer", 3),
                                        gl::ProgramInitializerArray()
                                        .add_sampler_initializer("glyph_texel_store", 0)
                                        .add_sampler_initializer("glyph_geometry_data_store", 1) );
    }
  m_drawers[draw_glyph_curvepair].set(pr, "CurvePair Text", &m_zoomer_text);

  for(int i = 0; i < number_texel_store_modes; ++i)
    {
      pr[i] = FASTUIDRAWnew gl::Program(glsl::ShaderSource()
                                        .specify_version(gl::Shader::default_shader_version())
                                        .add_source("glyph_atlas.vert.glsl.resource_string",
                                                    glsl::ShaderSource::from_resource),
                                        glsl::ShaderSource()
                                        .specify_version(gl::Shader::default_shader_version())
                                        .add_macro(macros[i].c_str())
                                        .add_source("gles_prec.frag.glsl.resource_string",
                                                    glsl::ShaderSource::from_resource)
                                        .add_source("glyph_atlas.frag.glsl.resource_string",
                                                    glsl::ShaderSource::from_resource),
                                        gl::PreLinkActionArray()
                                        .add_binding("attrib_p", 0),
                                        gl::ProgramInitializerArray()
                                        .add_sampler_initializer("glyph_texel_store", 0));
    }
  m_drawers[draw_glyph_atlas].set(pr, "Atlas", &m_zoomer_atlas);
}

void
glyph_test::
compute_glyphs_and_positions_glyph_set(fastuidraw::GlyphRender renderer, float pixel_size_formatting,
                                       std::vector<Glyph> &glyphs, std::vector<vec2> &positions)
{
  float tallest(0.0f), negative_tallest(0.0f), offset;
  unsigned int i, endi, glyph_at_start, navigator_chars;
  float scale_factor, div_scale_factor;
  std::list< std::pair<float, std::string> > navigator;
  std::list< std::pair<float, std::string> >::iterator nav_iter;
  float line_length(800);
  FT_ULong character_code;
  FT_UInt  glyph_index;
  unsigned int num_glyphs;

  div_scale_factor = static_cast<float>(m_face->face()->units_per_EM);
  scale_factor = pixel_size_formatting / div_scale_factor;

  /* Get all the glyphs */
  simple_time timer;
  std::vector<int> cnts;
  GlyphSetGenerator::generate(m_realize_glyphs_thread_count.value(), renderer,
                              m_font, m_face, glyphs, m_glyph_cache, cnts);
  std::cout << "Took " << timer.elapsed()
            << " ms to generate glyphs of type "
            << renderer << "\n";
  for(int i = 0; i < m_realize_glyphs_thread_count.value(); ++i)
    {
      std::cout << "\tThread #" << i << " generated " << cnts[i] << " glyphs.\n";
    }

  for(Glyph g : glyphs)
    {
      FASTUIDRAWassert(g.valid());
      FASTUIDRAWassert(g.cache() == m_glyph_cache);

      tallest = std::max(tallest, g.layout().horizontal_layout_offset().y() + g.layout().size().y());
      negative_tallest = std::min(negative_tallest, g.layout().horizontal_layout_offset().y());
    }

  tallest *= scale_factor;
  negative_tallest *= scale_factor;
  offset = tallest - negative_tallest;

  positions.resize(glyphs.size());

  vec2 pen(0.0f, 0.0f);
  for(navigator_chars = 0, i = 0, endi = glyphs.size(), glyph_at_start = 0; i < endi; ++i)
    {
      Glyph g;
      GlyphLayoutData layout;
      float advance, nxt;

      g = glyphs[i];
      FASTUIDRAWassert(g.valid());

      layout = g.layout();
      advance = scale_factor * t_max(layout.advance().x(),
                                     t_max(0.0f, layout.horizontal_layout_offset().x()) + layout.size().x());

      positions[i].x() = pen.x();
      positions[i].y() = pen.y();
      pen.x() += advance;

      if (i + 1 < endi)
        {
          float pre_layout, nxt_adv;
          GlyphLayoutData nxtL(glyphs[i + 1].layout());

          pre_layout = t_max(0.0f, -nxtL.horizontal_layout_offset().x());
          pen.x() += scale_factor * pre_layout;
          nxt_adv = t_max(nxtL.advance().x(),
                          t_max(0.0f, nxtL.horizontal_layout_offset().x()) + nxtL.size().x());
          nxt = pen.x() + scale_factor * nxt_adv;
        }
      else
        {
          nxt = pen.x();
        }

      if (nxt >= line_length || i + 1 == endi)
        {
          std::ostringstream desc;
          desc << "[" << std::setw(5) << glyphs[glyph_at_start].layout().glyph_code()
               << " - " << std::setw(5) << glyphs[i].layout().glyph_code() << "]";
          navigator.push_back(std::make_pair(pen.y(), desc.str()));
          navigator_chars += navigator.back().second.length();
          glyph_at_start = i + 1;

          pen.x() = 0.0f;
          pen.y() += offset + 1.0f;
        }
    }

  positions.reserve(glyphs.size() + navigator_chars);
  glyphs.reserve(glyphs.size() + navigator_chars);

  for(nav_iter = navigator.begin(); nav_iter != navigator.end(); ++nav_iter)
    {
      std::istringstream stream(nav_iter->second);
      GlyphSequence seq(pixel_size_formatting,
                        PainterEnums::y_increases_downwards,
                        m_glyph_cache);

      create_formatted_text(seq, stream, m_font, m_glyph_selector,
                            vec2(line_length, nav_iter->first));

      c_array<const Glyph> src_glyphs(seq.glyph_sequence(renderer));
      c_array<const vec2> src_pos(seq.glyph_positions());

      for(unsigned int c = 0; c < src_glyphs.size(); ++c)
        {
          glyphs.push_back(src_glyphs[c]);
          positions.push_back(src_pos[c]);
        }
    }
}

void
glyph_test::
compute_glyphs_and_positions(fastuidraw::GlyphRender renderer, float pixel_size_formatting,
                             std::vector<Glyph> &glyphs, std::vector<vec2> &positions)
{
  if (m_draw_glyph_set.value())
    {
      compute_glyphs_and_positions_glyph_set(renderer, pixel_size_formatting,
                                             glyphs, positions);
    }
  else
    {
      GlyphSequence seq(pixel_size_formatting,
                        PainterEnums::y_increases_downwards,
                        m_glyph_cache);

      if (m_use_file.value())
        {
          std::ifstream istr(m_text.value().c_str(), std::ios::binary);
          if (istr)
            {
              create_formatted_text(seq, istr, m_font, m_glyph_selector);
            }
        }
      else
        {
          std::istringstream istr(m_text.value());
          create_formatted_text(seq, istr, m_font, m_glyph_selector);
        }

      c_array<const Glyph> src_glyphs(seq.glyph_sequence(renderer));
      c_array<const vec2> src_pos(seq.glyph_positions());

      glyphs.resize(src_glyphs.size());
      std::copy(src_glyphs.begin(), src_glyphs.end(), glyphs.begin());

      positions.resize(src_pos.size());
      std::copy(src_pos.begin(), src_pos.end(), positions.begin());
    }
}



void
glyph_test::
ready_attributes_indices(void)
{
  std::vector<vec2> glyph_positions;
  std::vector<Glyph> glyphs;

  {
    GlyphRender renderer(m_coverage_pixel_size.value());
    float format_pixel_size, scale_factor;

    format_pixel_size = m_render_pixel_size.value();
    scale_factor = format_pixel_size / static_cast<float>(m_face->face()->units_per_EM);

    glyph_positions.clear();
    glyphs.clear();

    compute_glyphs_and_positions(renderer, format_pixel_size, glyphs, glyph_positions);
    m_drawers[draw_glyph_coverage].init_draw_text(cast_c_array(glyphs), cast_c_array(glyph_positions), scale_factor);
  }

  {
    GlyphRender renderer(distance_field_glyph);
    float format_pixel_size, scale_factor;

    format_pixel_size = m_render_pixel_size.value();
    scale_factor = format_pixel_size / static_cast<float>(m_face->face()->units_per_EM);

    glyph_positions.clear();
    glyphs.clear();

    compute_glyphs_and_positions(renderer, format_pixel_size, glyphs, glyph_positions);
    m_drawers[draw_glyph_distance].init_draw_text(cast_c_array(glyphs), cast_c_array(glyph_positions), scale_factor);
  }

  {
    GlyphRender renderer(curve_pair_glyph);
    float format_pixel_size, scale_factor;

    format_pixel_size = m_render_pixel_size.value();
    scale_factor = format_pixel_size / static_cast<float>(m_face->face()->units_per_EM);

    glyph_positions.clear();
    glyphs.clear();

    compute_glyphs_and_positions(renderer, format_pixel_size, glyphs, glyph_positions);
    m_drawers[draw_glyph_curvepair].init_draw_text(cast_c_array(glyphs), cast_c_array(glyph_positions), scale_factor);
  }


  {
    float w, h;

    w = static_cast<float>(m_glyph_atlas->texel_store()->dimensions().x());
    h = static_cast<float>(m_glyph_atlas->texel_store()->dimensions().y());

    vec2 pts[4] =
      {
        vec2(0.0f, 0.0f),
        vec2(0.0f, h),
        vec2(w, h),
        vec2(w, 0.0f)
      };

    m_drawers[draw_glyph_atlas].init_and_bind_vao_vbo_ibo();
    glBufferData(GL_ARRAY_BUFFER, sizeof(pts), pts, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    gl::VertexAttribPointer(0, gl::opengl_trait_values<vec2>());

    GLuint inds[6] =
      {
        0, 1, 2,
        0, 2, 3
      };
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(inds), inds, GL_STATIC_DRAW);
    m_drawers[draw_glyph_atlas].m_index_count = 6;
  }

}

void
glyph_test::
draw_frame(void)
{
  glClearColor(m_bg_red.value(), m_bg_green.value(), m_bg_blue.value(), 0.0f);
  glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glDisable(GL_DEPTH_TEST);

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D_ARRAY, m_glyph_atlas->texel_texture(texel_store_uint == m_texel_access_mode));

  if (m_glyph_atlas->geometry_binding_point() != GL_SHADER_STORAGE_BUFFER)
    {
      glActiveTexture(GL_TEXTURE1);
      glBindTexture(m_glyph_atlas->geometry_binding_point(), m_glyph_atlas->geometry_backing());
    }
  else
    {
      glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_glyph_atlas->geometry_backing());
    }

  m_drawers[m_current_drawer].draw(this, m_texel_access_mode, m_pvm, m_current_layer, m_aa_mode);

}

void
glyph_test::
handle_event(const SDL_Event &ev)
{
  m_drawers[m_current_drawer].m_zoomer->handle_event(ev);
  switch(ev.type)
    {
    case SDL_QUIT:
      end_demo(0);
      break;

    case SDL_WINDOWEVENT:
      if (ev.window.event == SDL_WINDOWEVENT_RESIZED)
        {
          int w, h;
          w = ev.window.data1;
          h = ev.window.data2;
          set_pvm(w, h);
          glViewport(0, 0, w, h);
        }
      break;

    case SDL_KEYUP:
      switch(ev.key.keysym.sym)
        {
        case SDLK_ESCAPE:
          end_demo(0);
          break;
        case SDLK_d:
          cycle_value(m_current_drawer, ev.key.keysym.mod & (KMOD_SHIFT|KMOD_CTRL|KMOD_ALT), number_draw_modes);
          std::cout << "Drawing " << m_drawers[m_current_drawer].m_label << "\n";
          break;
        case SDLK_n:
          if (m_current_drawer == draw_glyph_atlas)
            {
              cycle_value(m_current_layer, false, m_glyph_atlas->texel_store()->dimensions().z());
              std::cout << "Drawing atlas layer #" << m_current_layer << "\n";
            }
          break;

        case SDLK_p:
          if (m_current_drawer == draw_glyph_atlas)
            {
              cycle_value(m_current_layer, true, m_glyph_atlas->texel_store()->dimensions().z());
              std::cout << "Drawing atlas layer #" << m_current_layer << "\n";
            }
          break;

        case SDLK_a:
          if (m_current_drawer == draw_glyph_curvepair || m_current_drawer == draw_glyph_distance)
            {
              cycle_value(m_aa_mode, ev.key.keysym.mod & (KMOD_SHIFT|KMOD_CTRL|KMOD_ALT), NUMBER_AA_MODES);
              std::cout << "AA-mode set to: " << m_aa_mode << "\n";
            }
          break;

        case SDLK_t:
          {
            c_string labels[number_texel_store_modes] =
              {
                /*[texel_store_uint]=*/ "texel_store_uint",
                /*[texel_store_float]=*/ "texel_store_float"
              };
            cycle_value(m_texel_access_mode, false, number_texel_store_modes);
            std::cout << "Texel store access mode set to " << labels[m_texel_access_mode] << "\n";
          }
          break;

        case SDLK_z:
          {
            vec2 p, fixed_point(dimensions() / 2);
            ScaleTranslate<float> tr;
            tr = m_drawers[m_current_drawer].m_zoomer->transformation();
            p = fixed_point - (fixed_point - tr.translation()) / tr.scale();
            m_drawers[m_current_drawer].m_zoomer->transformation(ScaleTranslate<float>(p));
          }
          break;
        }
      break;
    }
}

int
main(int argc, char **argv)
{
  glyph_test G;
  return G.main(argc, argv);
}
