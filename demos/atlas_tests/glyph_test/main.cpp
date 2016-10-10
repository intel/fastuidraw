#include <iostream>
#include <sstream>
#include <fstream>
#include <fastuidraw/glsl/shader_code.hpp>
#include <fastuidraw/text/glyph_cache.hpp>
#include <fastuidraw/text/freetype_font.hpp>
#include <fastuidraw/text/glyph_selector.hpp>
#include <fastuidraw/gl_backend/ngl_header.hpp>
#include <fastuidraw/gl_backend/glyph_atlas_gl.hpp>
#include <fastuidraw/gl_backend/gluniform.hpp>
#include <fastuidraw/gl_backend/opengl_trait.hpp>
#include <fastuidraw/gl_backend/gl_context_properties.hpp>
#include <fastuidraw/gl_backend/gl_program.hpp>
#include "sdl_demo.hpp"
#include "ImageLoader.hpp"
#include "PanZoomTracker.hpp"
#include "text_helper.hpp"
#include "cycle_value.hpp"

using namespace fastuidraw;

#define NUMBER_AA_MODES 5


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
                               std::vector<vec2> &positions,
                               std::vector<uint32_t> &character_codes);

  void
  change_glyph_renderer(GlyphRender renderer, c_array<Glyph> glyphs,
                        const_c_array<uint32_t> character_codes);

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
      geometry_backing_store_auto,
    };

  class per_program:boost::noncopyable
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

  class per_draw:boost::noncopyable
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
    init_draw_text(const_c_array<Glyph> glyphs, const_c_array<vec2> glyph_positions,
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
  FT_Library m_library;
  FT_Face m_face;

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
  assert(m_program->link_success());

  m_pvm_loc = m_program->uniform_location("pvm");
  assert(m_pvm_loc != -1);

  m_scale_loc = m_program->uniform_location("scale");
  assert(m_scale_loc != -1);

  m_translate_loc = m_program->uniform_location("translate");
  assert(m_translate_loc != -1);

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
  m_zoomer(NULL)
{}

glyph_test::per_draw::
~per_draw()
{
  if(m_vao != 0)
    {
      glDeleteVertexArrays(1, &m_vao);
    }

  if(m_vbo != 0)
    {
      glDeleteBuffers(1, &m_vbo);
    }

  if(m_ibo != 0)
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
  assert(m_vao != 0);
  glBindVertexArray(m_vao);

  glGenBuffers(1, &m_vbo);
  assert(m_vbo != 0);
  glBindBuffer(GL_ARRAY_BUFFER, m_vbo);

  glGenBuffers(1, &m_ibo);
  assert(m_ibo != 0);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ibo);
}

void
glyph_test::per_draw::
init_draw_text(const_c_array<Glyph> glyphs, const_c_array<vec2> glyph_positions, float
               scale_factor)
{
  std::vector<attribs_per_glyph> attribs;
  std::vector<vecN<GLuint, 6> > indices;
  unsigned int i, endi, glyph_count;

  assert(glyphs.size() == glyph_positions.size());

  /* generate attribute data from glyphs and glyph_posisions.
   */
  attribs.resize(glyphs.size());
  indices.resize(glyphs.size());
  for(i = 0, endi = glyphs.size(), glyph_count = 0; i < endi; ++i)
    {
      if(glyphs[i].valid())
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

  if(attribs.empty())
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


  if(indices.empty())
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

  if(m_programs[which_program].m_layer_loc != -1)
    {
      gl::Uniform(m_programs[which_program].m_layer_loc, layer);
    }

  if(m_programs[which_program].m_aa_mode_loc != -1)
    {
      gl::Uniform(m_programs[which_program].m_aa_mode_loc, aa_mode);
    }

  if(m_programs[which_program].m_fg_color_loc != -1)
    {
      gl::Uniform(m_programs[which_program].m_fg_color_loc,
                  vec3(q->m_fg_red.m_value, q->m_fg_green.m_value, q->m_fg_blue.m_value));
    }

  glDrawElements(GL_TRIANGLES, m_index_count, GL_UNSIGNED_INT, NULL);
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
  assert(R == routine_success);
  FASTUIDRAWunused(R);

  GlyphLocation atlas_location(G.atlas_location());
  GlyphLocation secondary_atlas_location(G.secondary_atlas_location());

  float layer(atlas_location.layer());
  float layer2(secondary_atlas_location.layer());
  vec2 tex_size(atlas_location.size());
  vec2 atlas_loc(atlas_location.location());
  vec2 secondary_atlas_loc(secondary_atlas_location.location());
  vec2 t_bl(atlas_loc), t_tr(t_bl + tex_size);
  vec2 t2_bl(secondary_atlas_loc), t2_tr(t2_bl + tex_size);
  vec2 glyph_size(SCALE * G.layout().m_size);
  vec2 p_bl, p_tr;

  p_bl.x() = p.x() + SCALE * G.layout().m_horizontal_layout_offset.x();
  p_tr.x() = p_bl.x() + glyph_size.x();

  p_bl.y() = p.y() - SCALE * G.layout().m_horizontal_layout_offset.y();
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

  if(layer2 != -1)
    {
      std::cout << "Needs secondary: glyph_code = " << G.layout().m_glyph_code
                << "\n\tglyph_size=" << glyph_size << " at " << p_bl << ":" << p_tr
                << "\n\tfrom location=" << p
                << "\n\ttex_size=" << tex_size << " at " << t_bl << ":" << layer
                << " and " << t2_bl << ":" << layer2
                << "\n\tglyph_offset=" << G.layout().m_horizontal_layout_offset
                << "\n\toriginal_size=" << G.layout().m_size
                << "\n\tadvance=" << G.layout().m_advance
                << "\n\toffset = " << G.geometry_offset()
                << "\n";
    }

}

/////////////////////////////////////
// glyph_test methods
glyph_test::
glyph_test(void):
  m_font_file("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", "font", "font to use", *this),
  m_font_index(0, "font_index", "face index into font file to use if font file has multiple fonts", *this),
  m_coverage_pixel_size(24, "coverage_pixel_size", "Pixel size at which to create coverage glyphs", *this),
  m_distance_pixel_size(48, "distance_pixel_size", "Pixel size at which to create distance field glyphs", *this),
  m_max_distance(96.0f, "max_distance",
                 "value to use for max distance in 64'ths of a pixel "
                 "when generating distance field glyphs", *this),
  m_curve_pair_pixel_size(48, "curvepair_pixel_size", "Pixel size at which to create distance curve pair glyphs", *this),
  m_text("Hello World!", "text", "text to draw to the screen", *this),
  m_use_file(false, "use_file", "if true the value for text gives a filename to display", *this),
  m_draw_glyph_set(false, "draw_glyph_set", "if true, display all glyphs of font instead of text", *this),
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
  m_library(NULL),
  m_face(NULL),
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
  if(m_face != NULL)
    {
      FT_Done_Face(m_face);
    }

  if(m_library != NULL)
    {
      FT_Done_FreeType(m_library);
    }
}


enum return_code
glyph_test::
create_and_add_font(void)
{
  int error_code;

  error_code = FT_Init_FreeType(&m_library);
  if(error_code != 0)
    {
      m_library = NULL;
      std::cerr << "Failed to init FreeType library\n";
      return routine_fail;
    }

  error_code = FT_New_Face(m_library, m_font_file.m_value.c_str(),
                           m_font_index.m_value, &m_face);

  if(error_code != 0 || m_face == NULL || (m_face->face_flags & FT_FACE_FLAG_SCALABLE) == 0)
    {
      if(m_face != NULL)
        {
          FT_Done_Face(m_face);
          m_face = NULL;
        }
      FT_Done_FreeType(m_library);
      m_library = NULL;
      std::cerr << "Failed to load scalable font at index " << m_font_index.m_value
                << "from file \"" << m_font_file.m_value << "\"\n";
      return routine_fail;
    }

  m_font = FASTUIDRAWnew FontFreeType(m_face,
                                     FontFreeType::RenderParams()
                                     .distance_field_max_distance(m_max_distance.m_value)
                                     .distance_field_pixel_size(m_distance_pixel_size.m_value)
                                     .curve_pair_pixel_size(m_curve_pair_pixel_size.m_value));
  m_glyph_selector->add_font(m_font);

  return routine_success;
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
  ivec3 texel_dims(m_texel_store_width.m_value, m_texel_store_height.m_value,
                   m_texel_store_num_layers.m_value);

  set_pvm(w, h);

  gl::GlyphAtlasGL::params glyph_atlas_options;
  glyph_atlas_options
    .texel_store_dimensions(texel_dims)
    .number_floats(m_geometry_store_size.m_value)
    .alignment(m_geometry_store_alignment.m_value)
    .delayed(m_atlas_delayed_upload.m_value);

  switch(m_geometry_backing_store_type.m_value.m_value)
    {
    case geometry_backing_store_texture_buffer:
      glyph_atlas_options.use_texture_buffer_geometry_store();
      break;

    case geometry_backing_store_texture_array:
      glyph_atlas_options.use_texture_2d_array_geometry_store(m_geometry_backing_texture_log2_w.m_value,
                                                              m_geometry_backing_texture_log2_h.m_value);
      break;

    default:
      glyph_atlas_options.use_optimal_geometry_store_backing();
      switch(glyph_atlas_options.glyph_geometry_backing_store_type())
        {
        case fastuidraw::glsl::PainterBackendGLSL::glyph_geometry_tbo:
          {
            std::cout << "Glyph Geometry Store: auto selected buffer\n";
          }
          break;

        case fastuidraw::glsl::PainterBackendGLSL::glyph_geometry_texture_array:
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
  m_glyph_selector = FASTUIDRAWnew GlyphSelector(m_glyph_cache);

  if(create_and_add_font() == routine_fail)
    {
      end_demo(-1);
      return;
    }

  ready_program();
  ready_attributes_indices();
}

void
glyph_test::
ready_program(void)
{
  vecN<reference_counted_ptr<gl::Program>, number_texel_store_modes> pr;
  vecN<std::string, number_texel_store_modes> macros;
  std::string glyph_geom_mode;
  ivec2 geom_log2_dims(0, 0);

  macros[texel_store_uint] = "USE_UINT_TEXEL_FETCH";
  macros[texel_store_float] = "USE_FLOAT_TEXEL_FETCH";

  if(m_glyph_atlas->geometry_texture_binding_point() == GL_TEXTURE_BUFFER)
    {
      glyph_geom_mode = "GLYPH_GEOMETRY_USE_TEXTURE_BUFFER";
    }
  else
    {
      glyph_geom_mode = "GLYPH_GEOMETRY_USE_TEXTURE_2D_ARRAY";
      geom_log2_dims = m_glyph_atlas->geometry_texture_as_2d_array_log2_dims();
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

      #ifdef FASTUIDRAW_GL_USE_GLES
        {
          /* Use the latest shader version.
           */
          gl::ContextProperties ctx;
          std::string version;

          if(ctx.version() >= ivec2(3, 2))
            {
              version = "320 es";
            }
          else if(ctx.version() >= ivec2(3,1))
            {
              version = "310 es";
            }
          else
            {
              version = "300 es";
            }

          vert
            .specify_version(version.c_str())
            .specify_extension("GL_OES_texture_buffer", glsl::ShaderSource::enable_extension)
            .specify_extension("GL_EXT_texture_buffer", glsl::ShaderSource::enable_extension);
          frag
            .specify_version(version.c_str())
            .specify_extension("GL_OES_texture_buffer", glsl::ShaderSource::enable_extension)
            .specify_extension("GL_EXT_texture_buffer", glsl::ShaderSource::enable_extension);
        }
      #else
        {
          vert.specify_version("330");
          frag.specify_version("330");
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
change_glyph_renderer(GlyphRender renderer, c_array<Glyph> glyphs, const_c_array<uint32_t> character_codes)
{
  for(unsigned int i = 0; i < glyphs.size(); ++i)
    {
      if(glyphs[i].valid())
        {
          glyphs[i] = m_glyph_selector->fetch_glyph_no_merging(renderer, glyphs[i].layout().m_font, character_codes[i]);
        }
    }
}

void
glyph_test::
compute_glyphs_and_positions(fastuidraw::GlyphRender renderer, float pixel_size_formatting,
                             std::vector<Glyph> &glyphs, std::vector<vec2> &positions,
                             std::vector<uint32_t> &character_codes)
{
  if(m_draw_glyph_set.m_value)
    {
      float max_height(0.0f);
      unsigned int i, endi, glyph_at_start, navigator_chars;
      float scale_factor, div_scale_factor;
      std::list< std::pair<float, std::string> > navigator;
      std::list< std::pair<float, std::string> >::iterator nav_iter;
      float line_length(800);
      FT_ULong character_code;
      FT_UInt  glyph_index;

      switch(renderer.m_type)
        {
        case distance_field_glyph:
          div_scale_factor = m_font->render_params().distance_field_pixel_size();
          break;
        case curve_pair_glyph:
          div_scale_factor = m_font->render_params().curve_pair_pixel_size();
          break;

        default:
          div_scale_factor = renderer.m_pixel_size;
        }

      scale_factor = pixel_size_formatting / div_scale_factor;

      for(character_code = FT_Get_First_Char(m_face, &glyph_index); glyph_index != 0;
          character_code = FT_Get_Next_Char(m_face, character_code, &glyph_index))
        {
          Glyph g;
          g = m_glyph_selector->fetch_glyph_no_merging(renderer, m_font, character_code);

          assert(g.valid());
          assert(g.layout().m_glyph_code == uint32_t(glyph_index));
          max_height = std::max(max_height, g.layout().m_size.y());
          glyphs.push_back(g);
          character_codes.push_back(character_code);
        }

      positions.resize(glyphs.size());

      vec2 pen(0.0f, scale_factor * max_height);
      for(navigator_chars = 0, i = 0, endi = glyphs.size(), glyph_at_start = 0; i < endi; ++i)
        {
          Glyph g;
          float advance;

          g = glyphs[i];
          advance = scale_factor * std::max(g.layout().m_advance.x(), g.layout().m_size.x());

          positions[i] = pen;
          pen.x() += advance;
          if(pen.x() >= line_length)
            {
              std::ostringstream desc;
              desc << "[" << std::setw(5) << glyph_at_start << " - "
                   << std::setw(5) << i << "]";
              navigator.push_back(std::make_pair(pen.y(), desc.str()));
              navigator_chars += navigator.back().second.length();

              pen.y() += scale_factor * max_height;
              pen.x() = 0.0f;
              positions[i] = pen;
              pen.x() += advance;
              glyph_at_start = i + 1;
            }
        }

      character_codes.reserve(glyphs.size() + navigator_chars);
      positions.reserve(glyphs.size() + navigator_chars);
      glyphs.reserve(glyphs.size() + navigator_chars);

      std::vector<fastuidraw::Glyph> temp_glyphs;
      std::vector<fastuidraw::vec2> temp_positions;
      std::vector<uint32_t> temp_character_codes;
      for(nav_iter = navigator.begin(); nav_iter != navigator.end(); ++nav_iter)
        {
          std::istringstream stream(nav_iter->second);

          temp_glyphs.clear();
          temp_positions.clear();
          create_formatted_text(stream, renderer, pixel_size_formatting,
                                m_font, m_glyph_selector, temp_glyphs,
                                temp_positions, temp_character_codes);

          assert(temp_glyphs.size() == temp_positions.size());
          for(unsigned int c = 0; c < temp_glyphs.size(); ++c)
            {
              glyphs.push_back(temp_glyphs[c]);
              character_codes.push_back(temp_character_codes[c]);
              positions.push_back( vec2(line_length + temp_positions[c].x(), nav_iter->first) );
            }
        }
    }
  else if(m_use_file.m_value)
    {
      std::ifstream istr(m_text.m_value.c_str(), std::ios::binary);
      if(istr)
        {
          create_formatted_text(istr, renderer, pixel_size_formatting,
                                m_font, m_glyph_selector, glyphs, positions,
                                character_codes);
        }
    }
  else
    {
      std::istringstream istr(m_text.m_value);
      create_formatted_text(istr, renderer, pixel_size_formatting,
                            m_font, m_glyph_selector, glyphs, positions,
                            character_codes);
    }
}



void
glyph_test::
ready_attributes_indices(void)
{
  std::vector<vec2> glyph_positions;
  std::vector<Glyph> glyphs;
  std::vector<uint32_t> character_codes;

  {
    GlyphRender renderer(m_coverage_pixel_size.m_value);
    float format_pixel_size, scale_factor;

    format_pixel_size = m_render_pixel_size.m_value;
    scale_factor = format_pixel_size / static_cast<float>(m_coverage_pixel_size.m_value);

    compute_glyphs_and_positions(renderer, format_pixel_size, glyphs, glyph_positions, character_codes);
    m_drawers[draw_glyph_coverage].init_draw_text(cast_c_array(glyphs), cast_c_array(glyph_positions), scale_factor);
  }

  {
    GlyphRender renderer(distance_field_glyph);
    float format_pixel_size, scale_factor;

    format_pixel_size = m_render_pixel_size.m_value;
    scale_factor = format_pixel_size / static_cast<float>(m_font->render_params().distance_field_pixel_size());
    change_glyph_renderer(renderer, cast_c_array(glyphs), cast_c_array(character_codes));
    m_drawers[draw_glyph_distance].init_draw_text(cast_c_array(glyphs), cast_c_array(glyph_positions), scale_factor);
  }

  {
    GlyphRender renderer(curve_pair_glyph);
    float format_pixel_size, scale_factor;

    format_pixel_size = m_render_pixel_size.m_value;
    scale_factor = format_pixel_size / static_cast<float>(m_font->render_params().curve_pair_pixel_size());
    change_glyph_renderer(renderer, cast_c_array(glyphs), cast_c_array(character_codes));
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
  glClearColor(m_bg_red.m_value, m_bg_green.m_value, m_bg_blue.m_value, 0.0f);
  glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glDisable(GL_DEPTH_TEST);

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D_ARRAY, m_glyph_atlas->texel_texture(texel_store_uint == m_texel_access_mode));

  glActiveTexture(GL_TEXTURE1);
  glBindTexture(m_glyph_atlas->geometry_texture_binding_point(), m_glyph_atlas->geometry_texture());

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
      if(ev.window.event == SDL_WINDOWEVENT_RESIZED)
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
          if(m_current_drawer == draw_glyph_atlas)
            {
              cycle_value(m_current_layer, false, m_glyph_atlas->texel_store()->dimensions().z());
              std::cout << "Drawing atlas layer #" << m_current_layer << "\n";
            }
          break;

        case SDLK_p:
          if(m_current_drawer == draw_glyph_atlas)
            {
              cycle_value(m_current_layer, true, m_glyph_atlas->texel_store()->dimensions().z());
              std::cout << "Drawing atlas layer #" << m_current_layer << "\n";
            }
          break;

        case SDLK_a:
          if(m_current_drawer == draw_glyph_curvepair || m_current_drawer == draw_glyph_distance)
            {
              cycle_value(m_aa_mode, ev.key.keysym.mod & (KMOD_SHIFT|KMOD_CTRL|KMOD_ALT), NUMBER_AA_MODES);
              std::cout << "AA-mode set to: " << m_aa_mode << "\n";
            }
          break;

        case SDLK_t:
          {
            const char *labels[number_texel_store_modes] =
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
