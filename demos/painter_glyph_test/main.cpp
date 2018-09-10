#include <iostream>
#include <sstream>
#include <fstream>
#include <fastuidraw/text/glyph_cache.hpp>
#include <fastuidraw/text/glyph_generate_params.hpp>
#include <fastuidraw/text/font_freetype.hpp>
#include <fastuidraw/text/glyph_selector.hpp>

#include "sdl_painter_demo.hpp"
#include "PanZoomTracker.hpp"
#include "text_helper.hpp"
#include "cycle_value.hpp"
#include "generic_hierarchy.hpp"
#include "command_line_list.hpp"

using namespace fastuidraw;

std::ostream&
operator<<(std::ostream &str, GlyphRender R)
{
  if (R.valid())
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

        default:
          str << "Unkown";
        }
    }
  else
    {
      str << "Auto Adjust";
    }
  return str;
}

class GlyphDrawsShared:fastuidraw::noncopyable
{
public:
  GlyphDrawsShared(void):
    m_glyph_sequence(nullptr),
    m_empty_glyph_sequence(nullptr),
    m_hierarchy(nullptr)
  {}

  ~GlyphDrawsShared();

  void
  query_glyph_interesection(const BoundingBox<float> bbox,
                            std::vector<unsigned int> *output)
  {
    if (m_hierarchy)
      {
        m_hierarchy->query(bbox, output);
      }
  }

  unsigned int
  query_glyph_at(const vec2 &p, BoundingBox<float> *out_bb)
  {
    if (m_hierarchy)
      {
        return m_hierarchy->query(p, out_bb);
      }
    return GenericHierarchy::not_found;
  }

  c_array<const vec2>
  glyph_positions(void) const
  {
    FASTUIDRAWassert(m_glyph_sequence);
    return m_glyph_sequence->glyph_positions();
  }

  GlyphSequence&
  glyph_sequence(void)
  {
    return *m_glyph_sequence;
  }

  c_array<const Glyph>
  glyphs(GlyphRender render,
         const reference_counted_ptr<Painter> &painter);

  GlyphRender
  draw_glyphs(GlyphRender render,
              const reference_counted_ptr<Painter> &painter,
              const PainterData &data,
              bool anisotropic_ant_ialias) const;

  void
  init(const reference_counted_ptr<const FontFreeType> &font,
       const reference_counted_ptr<GlyphCache> &glyph_cache,
       const reference_counted_ptr<GlyphSelector> &selector,
       float pixel_size_formatting,
       enum PainterEnums::screen_orientation screen_orientation);

  void
  init(const std::vector<uint32_t> &glyph_codes,
       const reference_counted_ptr<const FontFreeType> &font,
       const reference_counted_ptr<GlyphCache> &glyph_cache,
       float pixel_size_formatting,
       enum PainterEnums::screen_orientation screen_orientation);

  void
  init(std::istream &istr,
       const reference_counted_ptr<const FontFreeType> &font,
       const reference_counted_ptr<GlyphCache> &glyph_cache,
       const reference_counted_ptr<GlyphSelector> &selector,
       float pixel_size_formatting,
       enum PainterEnums::screen_orientation screen_orientation);

  void
  post_finalize(size_t glyphs_per_painter_draw)
  {
    make_hierarchy();
    make_sub_sequences(glyphs_per_painter_draw);
  }

  void
  realize_all_glyphs(GlyphRender renderer,
                     const reference_counted_ptr<const FontFreeType> &font,
                     int num_threads);

private:
  void
  make_hierarchy(void);

  void
  make_sub_sequences(size_t glyphs_per_painter_draw);

  GlyphSequence *m_glyph_sequence, *m_empty_glyph_sequence;
  GenericHierarchy *m_hierarchy;
  std::vector<GlyphSequence*> m_sub_glyph_sequence;
};

class painter_glyph_test:public sdl_painter_demo
{
public:
  painter_glyph_test(void);
  ~painter_glyph_test();

protected:

  virtual
  void
  derived_init(int w, int h);

  virtual
  void
  draw_frame(void);

  virtual
  void
  handle_event(const SDL_Event&);

private:

  enum
    {
      draw_glyph_coverage,
      draw_glyph_distance,
      draw_glyph_curvepair,

      draw_glyph_auto
    };

  typedef enum PainterEnums::screen_orientation screen_orientation;

  enum return_code
  create_and_add_font(void);

  void
  realize_all_glyphs(GlyphRender renderer);

  void
  ready_glyph_attribute_data(void);

  void
  ready_atlas_drawer(void);

  float
  update_cts_params(void);

  void
  stroke_glyph(const PainterData &d, Glyph G);

  void
  fill_glyph(const PainterData &d, Glyph G);

  void
  draw_glyphs(float us);

  void
  draw_atlas(float us);

  command_line_argument_value<std::string> m_font_path;
  command_line_argument_value<std::string> m_font_style, m_font_family;
  command_line_argument_value<bool> m_font_bold, m_font_italic;
  command_line_argument_value<std::string> m_font_file;
  command_line_argument_value<int> m_coverage_pixel_size;
  command_line_argument_value<int> m_distance_pixel_size;
  command_line_argument_value<float> m_max_distance;
  command_line_argument_value<int> m_curve_pair_pixel_size;
  command_line_argument_value<std::string> m_text;
  command_line_argument_value<bool> m_use_file;
  command_line_argument_value<bool> m_draw_glyph_set;
  command_line_argument_value<int> m_realize_glyphs_thread_count;
  command_line_argument_value<float> m_render_pixel_size;
  command_line_argument_value<float> m_change_stroke_width_rate;
  command_line_argument_value<int> m_glyphs_per_painter_draw;
  command_line_list<uint32_t> m_explicit_glyph_codes;
  enumerated_command_line_argument_value<screen_orientation> m_screen_orientation;

  reference_counted_ptr<const FontFreeType> m_font;

  vecN<std::string, PainterEnums::number_join_styles> m_join_labels;
  GlyphDrawsShared m_draw_shared;

  /* The last entry value is left as invalid to represent
   * to auto-choose how to render the glyphs.
   */
  vecN<GlyphRender, draw_glyph_auto + 1> m_draws;
  bool m_draw_glyphs;
  unsigned int m_glyph_texel_page;

  bool m_use_anisotropic_anti_alias;
  bool m_stroke_glyphs, m_fill_glyphs;
  bool m_anti_alias_path_stroking, m_anti_alias_path_filling;
  bool m_pixel_width_stroking;
  bool m_draw_stats;
  float m_stroke_width;
  unsigned int m_current_drawer;
  unsigned int m_join_style;

  reference_counted_ptr<PainterItemShader> m_atlas_shader;
  vecN<PainterAttribute, 4> m_draw_atlas_attributes;
  vecN<PainterIndex, 6> m_draw_atlas_indices;

  PanZoomTrackerSDLEvent m_zoomer;
  PanZoomTrackerSDLEvent m_atlas_zoomer;
  simple_time m_draw_timer;
};

///////////////////////////////////
// GlyphDrawsShared methods
GlyphDrawsShared::
~GlyphDrawsShared()
{
  if (m_glyph_sequence)
    {
      FASTUIDRAWdelete(m_glyph_sequence);
    }

  if (m_empty_glyph_sequence)
    {
      FASTUIDRAWdelete(m_empty_glyph_sequence);
    }

  if (m_hierarchy)
    {
      FASTUIDRAWdelete(m_hierarchy);
    }

  for (GlyphSequence *p : m_sub_glyph_sequence)
    {
      FASTUIDRAWdelete(p);
    }
}

void
GlyphDrawsShared::
make_hierarchy(void)
{
  c_array<const GlyphSource> glyph_sources(m_glyph_sequence->glyph_sources());
  c_array<const vec2> in_glyph_positions(m_glyph_sequence->glyph_positions());
  enum PainterEnums::screen_orientation orientation(m_glyph_sequence->orientation());
  GlyphCache *glyph_cache(m_glyph_sequence->glyph_cache().get());
  float pixel_size(m_glyph_sequence->pixel_size());
  GlyphMetrics metrics;
  BoundingBox<float> bbox;
  std::vector<BoundingBox<float> > glyph_bboxes;
  simple_time timer;

  std::cout << "Creating GlyphHierarch..." << std::flush;
  glyph_bboxes.resize(glyph_sources.size());
  for(unsigned int i = 0 ; i < glyph_sources.size(); ++i)
    {
      if(glyph_sources[i].m_font)
        {
          vec2 p, min_bb, max_bb;
          float ratio;

          metrics = glyph_cache->fetch_glyph_metrics(glyph_sources[i].m_font, glyph_sources[i].m_glyph_code);
          min_bb = metrics.horizontal_layout_offset();
          max_bb = min_bb + metrics.size();

          ratio = pixel_size / metrics.units_per_EM();
          min_bb *= ratio;
          max_bb *= ratio;

          if (orientation == PainterEnums::y_increases_downwards)
            {
              min_bb.y() = -min_bb.y();
              max_bb.y() = -max_bb.y();
            }

          p = in_glyph_positions[i];
          glyph_bboxes[i].union_point(p + min_bb);
          glyph_bboxes[i].union_point(p + max_bb);
          bbox.union_box(glyph_bboxes[i]);
        }
    }

  m_hierarchy = FASTUIDRAWnew GenericHierarchy(bbox);
  for(unsigned int i = 0 ; i < glyph_sources.size(); ++i)
    {
      m_hierarchy->add(glyph_bboxes[i], i);
    }
  std::cout << "took " << timer.restart() << " ms\n";
}

void
GlyphDrawsShared::
init(const reference_counted_ptr<const FontFreeType> &font,
     const reference_counted_ptr<GlyphCache> &glyph_cache,
     const reference_counted_ptr<GlyphSelector> &glyph_selector,
     float pixel_size_formatting,
     enum PainterEnums::screen_orientation screen_orientation)
{
  float tallest(0.0f), negative_tallest(0.0f), offset;
  unsigned int i, endi, glyph_at_start, navigator_chars;
  std::list< std::pair<float, std::string> > navigator;
  std::list< std::pair<float, std::string> >::iterator nav_iter;
  float line_length(800);
  unsigned int num_glyphs;
  float y_advance_sign;
  std::vector<GlyphMetrics> metrics;
  simple_time timer;

  y_advance_sign = (screen_orientation == PainterEnums::y_increases_downwards) ? 1.0f : -1.0f;
  num_glyphs = font->number_glyphs();

  m_glyph_sequence = FASTUIDRAWnew GlyphSequence(pixel_size_formatting,
                                                 screen_orientation, glyph_cache);
  m_empty_glyph_sequence = FASTUIDRAWnew GlyphSequence(pixel_size_formatting,
                                                       screen_orientation, glyph_cache);

  std::cout << "Formatting glyphs ..." << std::flush;
  metrics.resize(num_glyphs);
  for(unsigned int i = 0; i < num_glyphs; ++i)
    {
      float scale_factor;

      metrics[i] = glyph_cache->fetch_glyph_metrics(font, i);
      scale_factor = pixel_size_formatting / metrics[i].units_per_EM();

      tallest = scale_factor * t_max(tallest, metrics[i].horizontal_layout_offset().y() + metrics[i].size().y());
      negative_tallest = scale_factor * t_min(negative_tallest, metrics[i].horizontal_layout_offset().y());
    }
  offset = tallest - negative_tallest;

  vec2 pen(0.0f, 0.0f);
  for(navigator_chars = 0, i = 0, endi = metrics.size(), glyph_at_start = 0; i < endi; ++i)
    {
      GlyphMetrics metric(metrics[i]);
      float advance, nxt, scale_factor;

      scale_factor = pixel_size_formatting / metric.units_per_EM();
      advance = scale_factor * t_max(metric.advance().x(),
                                     t_max(0.0f, metric.horizontal_layout_offset().x()) + metric.size().x());

      m_glyph_sequence->add_glyph(GlyphSource(i, font), pen);
      pen.x() += advance;

      if (i + 1 < endi)
        {
          float pre_layout, nxt_adv;
          GlyphMetrics nxtL(metrics[i + 1]);

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
          desc << "[" << std::setw(5) << metrics[glyph_at_start].glyph_code()
               << " - " << std::setw(5) << metrics[i].glyph_code() << "]";
          navigator.push_back(std::make_pair(pen.y(), desc.str()));
          navigator_chars += navigator.back().second.length();

          glyph_at_start = i + 1;

          pen.x() = 0.0f;
          pen.y() += (offset + 1.0f) * y_advance_sign;
        }
    }
  std::cout << "took " << timer.restart() << " ms\n";

  std::cout << "Formatting navigation text..." << std::flush;
  for(nav_iter = navigator.begin(); nav_iter != navigator.end(); ++nav_iter)
    {
      std::istringstream stream(nav_iter->second);

      create_formatted_text(*m_glyph_sequence, stream, font,
                            glyph_selector,
                            vec2(line_length, nav_iter->first));
    }
  std::cout << "took " << timer.restart() << " ms\n";
}

void
GlyphDrawsShared::
init(const std::vector<uint32_t> &glyph_codes,
     const reference_counted_ptr<const FontFreeType> &font,
     const reference_counted_ptr<GlyphCache> &glyph_cache,
     float pixel_size_formatting,
     enum PainterEnums::screen_orientation screen_orientation)
{
  simple_time timer;

  std::cout << "Formatting glyphs ..." << std::flush;
  m_glyph_sequence = FASTUIDRAWnew GlyphSequence(pixel_size_formatting,
                                                 screen_orientation, glyph_cache);
  m_empty_glyph_sequence = FASTUIDRAWnew GlyphSequence(pixel_size_formatting,
                                                       screen_orientation, glyph_cache);
  create_formatted_text(*m_glyph_sequence, glyph_codes, font);

  std::cout << "took " << timer.restart() << " ms\n";
}

void
GlyphDrawsShared::
init(std::istream &istr,
     const reference_counted_ptr<const FontFreeType> &font,
     const reference_counted_ptr<GlyphCache> &glyph_cache,
     const reference_counted_ptr<GlyphSelector> &glyph_selector,
     float pixel_size_formatting,
     enum PainterEnums::screen_orientation screen_orientation)
{
  m_glyph_sequence = FASTUIDRAWnew GlyphSequence(pixel_size_formatting,
                                                 screen_orientation, glyph_cache);
  m_empty_glyph_sequence = FASTUIDRAWnew GlyphSequence(pixel_size_formatting,
                                                       screen_orientation, glyph_cache);
  if (istr)
    {
      simple_time timer;

      std::cout << "Formatting glyphs ..." << std::flush;
      create_formatted_text(*m_glyph_sequence, istr, font, glyph_selector);
      std::cout << "took " << timer.restart() << " ms\n";
    }
}

void
GlyphDrawsShared::
make_sub_sequences(size_t glyphs_per_painter_draw)
{
  c_array<const vec2> in_positions(glyph_sequence().glyph_positions());
  c_array<const GlyphSource> in_glyphs(glyph_sequence().glyph_sources());
  simple_time timer;

  std::cout << "Creating sub-glyph sequences..." << std::flush;
  while (!in_glyphs.empty())
    {
      c_array<const GlyphSource> glyphs;
      c_array<const vec2> glyph_positions;
      unsigned int cnt;
      GlyphSequence *data;

      cnt = t_min(in_glyphs.size(), glyphs_per_painter_draw);
      glyphs = in_glyphs.sub_array(0, cnt);
      glyph_positions = in_positions.sub_array(0, cnt);

      data = FASTUIDRAWnew GlyphSequence(glyph_sequence().pixel_size(),
                                         glyph_sequence().orientation(),
                                         glyph_sequence().glyph_cache(),
                                         glyph_sequence().layout());
      data->add_glyphs(glyphs, glyph_positions);

      in_glyphs = in_glyphs.sub_array(cnt);
      in_positions = in_positions.sub_array(cnt);
      m_sub_glyph_sequence.push_back(data);
    }
  std::cout << "took " << timer.restart() << " ms\n";
}

GlyphRender
GlyphDrawsShared::
draw_glyphs(GlyphRender render,
            const reference_counted_ptr<Painter> &painter,
            const PainterData &draw,
            bool anisotropic_anti_alias) const
{
  for (GlyphSequence *p : m_sub_glyph_sequence)
    {
      if (render.valid())
        {
          painter->draw_glyphs(draw,
                               p->painter_attribute_data(render),
                               anisotropic_anti_alias);
        }
      else
        {
          render = painter->draw_glyphs(draw, *p, anisotropic_anti_alias);
        }
    }

  return render;
}

c_array<const Glyph>
GlyphDrawsShared::
glyphs(GlyphRender render,
       const reference_counted_ptr<Painter> &painter)
{
  if (!render.valid())
    {
      render = painter->draw_glyphs(PainterData(), *m_empty_glyph_sequence);
    }
  return m_glyph_sequence->glyph_sequence(render);
}

/////////////////////////////////////
// painter_glyph_test methods
painter_glyph_test::
painter_glyph_test(void):
  m_font_path(default_font_path(), "font_path", "Specifies path in which to search for fonts", *this),
  m_font_style("Book", "font_style", "Specifies the font style", *this),
  m_font_family("DejaVu Sans", "font_family", "Specifies the font family name", *this),
  m_font_bold(false, "font_bold", "if true select a bold font", *this),
  m_font_italic(false, "font_italic", "if true select an italic font", *this),
  m_font_file("", "font_file",
              "If non-empty gives the name of a font by filename "
              "thus bypassing the glyph selection process with glyph_selector",
              *this),
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
  m_render_pixel_size(24.0f, "render_pixel_size", "pixel size at which to display glyphs", *this),
  m_change_stroke_width_rate(10.0f, "change_stroke_width_rate",
                             "rate of change in pixels/sec for changing stroke width "
                             "when changing stroke when key is down",
                             *this),
  m_glyphs_per_painter_draw(10000, "glyphs_per_painter_draw",
                            "Number of glyphs to draw per Painter::draw_text call",
                            *this),
  m_explicit_glyph_codes("add_glyph_code",
                         "Add an explicit glyph code to render, if the list "
                         "is non-empty, takes precendence over text",
                         *this),
  m_screen_orientation(PainterEnums::y_increases_downwards,
                      enumerated_string_type<screen_orientation>()
                      .add_entry("y_downwards",
                                 PainterEnums::y_increases_downwards,
                                 "Make coordinate system so that y-coordinate "
                                 "increases downwards (i.e. top of the window "
                                 "has y-coordinate is 0")
                      .add_entry("y_upwards",
                                 PainterEnums::y_increases_upwards,
                                 "Make coordinate system so that y-coordinate "
                                 "increases upwards (i.e. bottom of the window "
                                 "has y-coordinate is 0"),
                      "y_orientation",
                      "Determine y-coordiante convention",
                      *this),
  m_draw_glyphs(true),
  m_glyph_texel_page(0),
  m_use_anisotropic_anti_alias(false),
  m_stroke_glyphs(false),
  m_fill_glyphs(false),
  m_anti_alias_path_stroking(false),
  m_anti_alias_path_filling(false),
  m_pixel_width_stroking(true),
  m_draw_stats(false),
  m_stroke_width(1.0f),
  m_current_drawer(draw_glyph_auto),
  m_join_style(PainterEnums::miter_joins)
{
  std::cout << "Controls:\n"
            << "\td: cycle drawing mode: draw coverage glyph, draw distance glyphs "
            << "[hold shift, control or mode to reverse cycle]\n"
            << "\ta: Toggle using anistropic anti-alias glyph rendering\n"
            << "\td: Cycle though text renderer\n"
            << "\tf: Toggle rendering text as filled path\n"
            << "\tq: Toggle anti-aliasing filled path rendering\n"
            << "\tw: Toggle anti-aliasing stroked path rendering\n"
            << "\tp: Toggle pixel width stroking\n"
            << "\tz: reset zoom factor to 1.0\n"
            << "\tg: toggle drawing glyphs or glyph texel atlas\n"
            << "\tn: show next page in glyph atlas\n"
            << "\tctrl-n: show previous page in glyph atlas\n"
            << "\ts: toggle stroking glyph path\n"
            << "\tj: cycle through join styles for stroking\n"
            << "\tl: draw Painter stats\n"
            << "\t[: decrease stroke width(hold left-shift for slower rate and right shift for faster)\n"
            << "\t]: increase stroke width(hold left-shift for slower rate and right shift for faster)\n"
            << "\tMouse Drag (left button): pan\n"
            << "\tHold Mouse (left button), then drag up/down: zoom out/in\n";

  m_join_labels[PainterEnums::no_joins] = "no_joins";
  m_join_labels[PainterEnums::rounded_joins] = "rounded_joins";
  m_join_labels[PainterEnums::bevel_joins] = "bevel_joins";
  m_join_labels[PainterEnums::miter_clip_joins] = "miter_clip_joins";
  m_join_labels[PainterEnums::miter_bevel_joins] = "miter_bevel_joins";
  m_join_labels[PainterEnums::miter_joins] = "miter_joins";
}

painter_glyph_test::
~painter_glyph_test()
{
}

enum return_code
painter_glyph_test::
create_and_add_font(void)
{
  reference_counted_ptr<const FontBase> font;

  GlyphGenerateParams::distance_field_max_distance(m_max_distance.value());
  GlyphGenerateParams::distance_field_pixel_size(m_distance_pixel_size.value());
  GlyphGenerateParams::curve_pair_pixel_size(m_curve_pair_pixel_size.value());

  if (!m_font_file.value().empty())
    {
      reference_counted_ptr<FreeTypeFace::GeneratorBase> gen;
      gen = FASTUIDRAWnew FreeTypeFace::GeneratorMemory(m_font_file.value().c_str(), 0);
      if (gen->check_creation() == routine_success)
        {
          font = FASTUIDRAWnew FontFreeType(gen, m_ft_lib);
        }
    }

  add_fonts_from_path(m_font_path.value(), m_ft_lib, m_glyph_selector);

  if (!font)
    {
      FontProperties props;
      props.style(m_font_style.value().c_str());
      props.family(m_font_family.value().c_str());
      props.bold(m_font_bold.value());
      props.italic(m_font_italic.value());

      font = m_glyph_selector->fetch_font(props);
    }

  m_font = font.dynamic_cast_ptr<const FontFreeType>();
  if (m_font)
    {
      std::cout << "Chose font: \"" << m_font->properties() << "\"\n";
    }
  else
    {
      std::cout << "\n-----------------------------------------------------"
                << "\nWarning: unable to create font\n"
                << "-----------------------------------------------------\n";
    }

  return routine_success;
}

void
painter_glyph_test::
ready_atlas_drawer(void)
{
  m_atlas_shader =
    FASTUIDRAWnew glsl::PainterItemShaderGLSL(false,
                                              glsl::ShaderSource()
                                              .add_source("show_atlas.vert.glsl.resource_string", glsl::ShaderSource::from_resource),
                                              glsl::ShaderSource()
                                              .add_source("show_atlas.frag.glsl.resource_string", glsl::ShaderSource::from_resource),
                                              glsl::varying_list()
                                              .add_float_varying("show_atlas_tex_coords_x")
                                              .add_float_varying("show_atlas_tex_coords_y")
                                              .add_float_varying("show_atlas_tex_coords_layer"));
  m_painter->painter_shader_registrar()->register_shader(m_atlas_shader);

  ivec3 dims(m_glyph_atlas->texel_store()->dimensions());

  m_draw_atlas_attributes[0].m_attrib0 = uvec4(0u, 0u, 0u, 0u);
  m_draw_atlas_attributes[1].m_attrib0 = uvec4(dims.x(), 0u, 0u, 0u);
  m_draw_atlas_attributes[2].m_attrib0 = uvec4(dims.x(), dims.y(), 0u, 0u);
  m_draw_atlas_attributes[3].m_attrib0 = uvec4(0, dims.y(), 0u, 0u);

  for (int i = 0; i < 4; ++i)
    {
      m_draw_atlas_attributes[i].m_attrib1 = uvec4(0u, 0u, 0u, 0u);
      m_draw_atlas_attributes[i].m_attrib2 = uvec4(0u, 0u, 0u, 0u);
    }

  m_draw_atlas_indices[0] = 0;
  m_draw_atlas_indices[1] = 1;
  m_draw_atlas_indices[2] = 2;
  m_draw_atlas_indices[3] = 0;
  m_draw_atlas_indices[4] = 2;
  m_draw_atlas_indices[5] = 3;
}

void
painter_glyph_test::
derived_init(int w, int h)
{
  FASTUIDRAWunused(w);
  FASTUIDRAWunused(h);

  if (create_and_add_font() == routine_fail)
    {
      end_demo(-1);
      return;
    }

  //put into unit of per us
  m_change_stroke_width_rate.value() /= (1000.0f * 1000.0f);

  ready_glyph_attribute_data();
  ready_atlas_drawer();
  m_draw_timer.restart();

  if (m_screen_orientation.value() == PainterEnums::y_increases_upwards)
    {
      m_zoomer.m_zoom_direction = PanZoomTracker::zoom_direction_negative_y;
      m_zoomer.m_scale_event.y() = -1.0f;
      m_zoomer.m_translate_event.y() = static_cast<float>(h);
      m_zoomer.transformation(ScaleTranslate<float>(vec2(0.0f, h - m_render_pixel_size.value())));
    }
  else
    {
      m_zoomer.transformation(ScaleTranslate<float>(vec2(0.0f, m_render_pixel_size.value())));
    }
}

void
painter_glyph_test::
realize_all_glyphs(GlyphRender renderer)
{
  unsigned int num_threads(m_realize_glyphs_thread_count.value());
  simple_time timer;
  std::vector<int> cnts;
  std::vector<Glyph> glyphs;

  std::cout << "Generating " << renderer << " glyphs ..." << std::flush;
  GlyphSetGenerator::generate(num_threads, renderer, m_font, glyphs, m_glyph_cache, cnts);
  std::cout << "took " << timer.restart()
            << " ms to generate " << glyphs.size()
            << " glyphs of type " << renderer << "\n";
  for(int i = 0; i < num_threads; ++i)
    {
      std::cout << "\tThread #" << i << " generated " << cnts[i] << " glyphs.\n";
    }

  std::cout << "Uploading to atlas ..." << std::flush;
  for (auto &g : glyphs)
    {
      g.upload_to_atlas();
    }
  std::cout << "took " << timer.restart() << " ms\n";

  unsigned int texels_allocated(m_glyph_atlas->number_texels_allocated());
  ivec3 texel_store_dims(m_glyph_atlas->texel_store()->dimensions());
  int num_texels_total(texel_store_dims.x() * texel_store_dims.y() * texel_store_dims.z());
  float fract_allocated(float(texels_allocated) / float(num_texels_total));
  unsigned int num_blocks(m_glyph_atlas->geometry_data_allocated());
  unsigned int block_alignment(m_glyph_atlas->geometry_store()->alignment());
  unsigned int block_size(sizeof(generic_data) * block_alignment);

  std::cout << "Number texel nodes = " << m_glyph_atlas->number_nodes()
            << ", bytes used = " << m_glyph_atlas->bytes_used_by_nodes() << "\n"
            << "Texels allocate = " << texels_allocated << " of "
            << num_texels_total << " (" << 100.0f * fract_allocated
            << "%)\nBytes geometry data allocated = "
            << num_blocks * block_size
            << "\n";
}

void
painter_glyph_test::
ready_glyph_attribute_data(void)
{
  std::vector<uint32_t> explicit_glyph_codes(m_explicit_glyph_codes.begin(),
                                             m_explicit_glyph_codes.end());

  m_draws[draw_glyph_curvepair] = GlyphRender(curve_pair_glyph);
  m_draws[draw_glyph_distance] = GlyphRender(distance_field_glyph);
  m_draws[draw_glyph_coverage] = GlyphRender(m_coverage_pixel_size.value());

  if (m_draw_glyph_set.value())
    {
      for (unsigned int i = 0; i < draw_glyph_auto; ++i)
        {
          realize_all_glyphs(m_draws[i]);
        }
      m_draw_shared.init(m_font,
                         m_glyph_cache, m_glyph_selector,
                         m_render_pixel_size.value(),
                         m_screen_orientation.value());
    }
  else if (!explicit_glyph_codes.empty())
    {
      m_draw_shared.init(explicit_glyph_codes,
                         m_font, m_glyph_cache,
                         m_render_pixel_size.value(),
                         m_screen_orientation.value());
    }
  else if (m_use_file.value())
    {
      std::ifstream istr(m_text.value().c_str(), std::ios::binary);
      m_draw_shared.init(istr, m_font,
                         m_glyph_cache, m_glyph_selector,
                         m_render_pixel_size.value(),
                         m_screen_orientation.value());
    }
  else
    {
      std::istringstream istr(m_text.value());
      m_draw_shared.init(istr, m_font,
                         m_glyph_cache, m_glyph_selector,
                         m_render_pixel_size.value(),
                         m_screen_orientation.value());
    }
  m_draw_shared.post_finalize(m_glyphs_per_painter_draw.value());

  for (unsigned int i = 0; i < draw_glyph_auto; ++i)
    {
      simple_time timer;

      std::cout << "Generating and realizing glyphs of type " << m_draws[i] << "...";
      m_draw_shared.glyph_sequence().glyph_sequence(m_draws[i], false);
      std::cout << "took " << timer.restart() << "ms.\n";

      std::cout << "Uploading (or confirming upload) glyphs of type " << m_draws[i] << "...";
      m_draw_shared.glyph_sequence().glyph_sequence(m_draws[i], true);
      std::cout << "took " << timer.restart() << "ms.\n";
    }
}

void
painter_glyph_test::
stroke_glyph(const PainterData &d, Glyph G)
{
  m_painter->stroke_path(d, G.path(),
                         true, PainterEnums::flat_caps,
                         static_cast<enum PainterEnums::join_style>(m_join_style),
                         m_anti_alias_path_stroking);
}

void
painter_glyph_test::
fill_glyph(const PainterData &d, Glyph G)
{
  m_painter->fill_path(d, G.path(),
                       PainterEnums::nonzero_fill_rule,
                       m_anti_alias_path_filling);
}

void
painter_glyph_test::
draw_frame(void)
{
  float us;
  us = update_cts_params();

  if (m_draw_glyphs)
    {
      draw_glyphs(us);
    }
  else
    {
      draw_atlas(us);
    }
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
  m_surface->blit_surface(GL_NEAREST);
}

void
painter_glyph_test::
draw_atlas(float us)
{
  PainterBrush brush;

  for (int i = 0; i < 4; ++i)
    {
      m_draw_atlas_attributes[i].m_attrib0.z() = m_glyph_texel_page;
    }

  m_painter->begin(m_surface, m_screen_orientation.value());
  m_painter->save();
  m_atlas_zoomer.transformation().concat_to_painter(m_painter);
  m_painter->draw_generic(m_atlas_shader, PainterData(&brush),
                          m_draw_atlas_attributes, m_draw_atlas_indices, 0);
  m_painter->restore();


  std::ostringstream ostr;
  /* start with an eol so that top line is visible */
  ostr << "\nShowing Texel Page " << m_glyph_texel_page
       << " / " << m_glyph_atlas->texel_store()->dimensions().z()
       << "\nFPS = ";
  if (us > 0.0f)
    {
      ostr << 1000.0f * 1000.0f / us;
    }
  else
    {
      ostr << "NAN";
    }

  ostr << "\nms = " << us / 1000.0f
       << "\nAttribs: "
       << m_painter->query_stat(PainterPacker::num_attributes)
       << "\nIndices: "
       << m_painter->query_stat(PainterPacker::num_indices)
       << "\nGenericData: "
       << m_painter->query_stat(PainterPacker::num_generic_datas)
       << "\nNumber Headers: "
       << m_painter->query_stat(PainterPacker::num_headers)
       << "\nNumber Draws: "
       << m_painter->query_stat(PainterPacker::num_draws);
  if (m_screen_orientation.value() == PainterEnums::y_increases_upwards)
    {
      m_painter->translate(vec2(0.0f, dimensions().y()));
    }
  brush.pen(0.0f, 1.0f, 1.0f, 1.0f);
  draw_text(ostr.str(), 32.0f, m_font, GlyphRender(distance_field_glyph),
            PainterData(&brush), m_screen_orientation.value());
  m_painter->end();
}

void
painter_glyph_test::
draw_glyphs(float us)
{
  std::vector<unsigned int> glyphs_visible;
  GlyphRender render;
  c_array<const vec2> glyph_positions(m_draw_shared.glyph_positions());
  PainterBrush glyph_brush;

  glyph_brush.pen(1.0, 1.0, 1.0, 1.0);

  m_painter->begin(m_surface, m_screen_orientation.value());
  m_painter->save();
  m_zoomer.transformation().concat_to_painter(m_painter);

  c_array<const Glyph> glyphs(m_draw_shared.glyphs(m_draws[m_current_drawer], m_painter));

  if (m_fill_glyphs || m_stroke_glyphs)
    {
      vec2 p0, p1;
      BoundingBox<float> screen;

      p0 = m_zoomer.transformation().apply_inverse_to_point(vec2(0.0f, 0.0f));
      p1 = m_zoomer.transformation().apply_inverse_to_point(vec2(dimensions()));

      screen
        .union_point(p0)
        .union_point(p1);

      m_draw_shared.query_glyph_interesection(screen, &glyphs_visible);
    }

  if (!m_fill_glyphs)
    {
      m_draw_shared.draw_glyphs(m_draws[m_current_drawer],
                                m_painter, PainterData(&glyph_brush),
                                m_use_anisotropic_anti_alias);
    }
  else
    {
      unsigned int src(m_current_drawer);

      // reuse brush parameters across all glyphs
      PainterPackedValue<PainterBrush> pbr;
      pbr = m_painter->packed_value_pool().create_packed_value(glyph_brush);

      for(unsigned int i : glyphs_visible)
        {
          if (glyphs[i].valid())
            {
              m_painter->save();
              m_painter->translate(glyph_positions[i]);

              //make the scale of the path match how we scaled the text.
              float sc, ysign;
              sc = m_render_pixel_size.value() / glyphs[i].metrics().units_per_EM();

              /* when drawing with y-coordinate increasing downwards
               * which is the opposite coordinate system as the glyph's
               * path, thus we also need to negate in the y-direction.
               */
              ysign = (m_screen_orientation.value() == PainterEnums::y_increases_upwards) ? 1.0f : -1.0f;
              m_painter->shear(sc, sc * ysign);
              fill_glyph(PainterData(pbr), glyphs[i]);
              m_painter->restore();
            }
        }
    }

  if (m_stroke_glyphs)
    {
      unsigned int src;
      PainterBrush stroke_brush;
      stroke_brush.pen(0.0, 1.0, 1.0, 0.8);

      PainterStrokeParams st;
      st.miter_limit(5.0f);
      st.width(m_stroke_width);
      if (m_pixel_width_stroking)
        {
          st.stroking_units(PainterStrokeParams::pixel_stroking_units);
        }

      src = m_current_drawer;

      // reuse stroke and brush parameters across all glyphs
      PainterPackedValue<PainterBrush> pbr;
      pbr = m_painter->packed_value_pool().create_packed_value(stroke_brush);

      PainterPackedValue<PainterItemShaderData> pst;
      pst = m_painter->packed_value_pool().create_packed_value(st);

      for(unsigned int i : glyphs_visible)
        {
          if (glyphs[i].valid())
            {
              m_painter->save();
              m_painter->translate(glyph_positions[i]);

              //make the scale of the path match how we scaled the text.
              float sc, ysign;
              sc = m_render_pixel_size.value() / glyphs[i].metrics().units_per_EM();

              /* when drawing with y-coordinate increasing downwards
               * which is the opposite coordinate system as the glyph's
               * path, thus we also need to negate in the y-direction.
               */
              ysign = (m_screen_orientation.value() == PainterEnums::y_increases_upwards) ? 1.0f : -1.0f;
              m_painter->shear(sc, sc * ysign);

              stroke_glyph(PainterData(pst, pbr), glyphs[i]);
              m_painter->restore();
            }
        }
    }

  if (m_draw_stats)
    {
      std::ostringstream ostr;

      /* start with an eol so that top line is visible */
      ostr << "\nFPS = ";
      if (us > 0.0f)
        {
          ostr << 1000.0f * 1000.0f / us;
        }
      else
        {
          ostr << "NAN";
        }

      ostr << "\nms = " << us / 1000.0f
           << "\nAttribs: "
           << m_painter->query_stat(PainterPacker::num_attributes)
           << "\nIndices: "
           << m_painter->query_stat(PainterPacker::num_indices)
           << "\nGenericData: "
           << m_painter->query_stat(PainterPacker::num_generic_datas)
           << "\nNumber Headers: "
           << m_painter->query_stat(PainterPacker::num_headers)
           << "\nNumber Draws: "
           << m_painter->query_stat(PainterPacker::num_draws);

      m_painter->restore();
      if (m_screen_orientation.value() == PainterEnums::y_increases_upwards)
        {
          m_painter->translate(vec2(0.0f, dimensions().y()));
        }

      PainterBrush brush;

      brush.pen(0.0f, 1.0f, 1.0f, 1.0f);
      draw_text(ostr.str(), 32.0f, m_font, GlyphRender(distance_field_glyph),
                PainterData(&brush), m_screen_orientation.value());
    }
  else
    {
      vec2 p;
      unsigned int G, src;
      ivec2 mouse_position;
      BoundingBox<float> glyph_bb;
      std::ostringstream ostr;

      src = m_current_drawer;

      SDL_GetMouseState(&mouse_position.x(), &mouse_position.y());
      if (m_screen_orientation.value() == PainterEnums::y_increases_upwards)
        {
          mouse_position.y() = dimensions().y() - mouse_position.y();
        }
      p = m_zoomer.transformation().apply_inverse_to_point(vec2(mouse_position));
      G = m_draw_shared.query_glyph_at(p, &glyph_bb);
      if (G != GenericHierarchy::not_found)
        {
          Glyph glyph;
          GlyphMetrics metrics;
          float ratio;

          glyph = glyphs[G];
          metrics = glyph.metrics();
          ratio = m_render_pixel_size.value() / metrics.units_per_EM();

          /* start with an eol so that top line is visible */
          ostr << "\nGlyph at " << p << " is:"
               << "\n\tglyph_code: " << metrics.glyph_code()
               << "\n\trenderer: ";

          if (!m_fill_glyphs)
            {
              ostr << glyph.renderer();
            }
          else
            {
              ostr << " path-fill";
            }

          ostr << "\n\tunits_per_EM: " << metrics.units_per_EM()
               << "\n\tsize in EM: " << metrics.size()
               << "\n\tsize normalized: " << metrics.size() * ratio
               << "\n\tHorizontal Offset = " << metrics.horizontal_layout_offset();

          /* draw a box around the glyph(!).*/
          PainterBrush brush;

          brush.pen(1.0f, 0.0f, 0.0f, 0.3f);
          m_painter->draw_rect(PainterData(&brush), glyph_bb.min_point(), glyph_bb.size());
        }
      else
        {
          ostr << "\nNo glyph at " << p << "\n";
        }

      m_painter->restore();
      if (m_screen_orientation.value() == PainterEnums::y_increases_upwards)
        {
          m_painter->translate(vec2(0.0f, dimensions().y()));
        }

      PainterBrush brush;

      brush.pen(0.0f, 1.0f, 1.0f, 1.0f);
      draw_text(ostr.str(), 32.0f, m_font, GlyphRender(distance_field_glyph),
                PainterData(&brush), m_screen_orientation.value());
    }

  m_painter->end();
}

float
painter_glyph_test::
update_cts_params(void)
{
  float return_value;
  const Uint8 *keyboard_state = SDL_GetKeyboardState(nullptr);
  FASTUIDRAWassert(keyboard_state != nullptr);

  float speed;
  return_value = static_cast<float>(m_draw_timer.restart_us());
  speed = return_value * m_change_stroke_width_rate.value();

  if (keyboard_state[SDL_SCANCODE_LSHIFT])
    {
      speed *= 0.1f;
    }
  if (keyboard_state[SDL_SCANCODE_RSHIFT])
    {
      speed *= 10.0f;
    }

  if (keyboard_state[SDL_SCANCODE_RIGHTBRACKET])
    {
      m_stroke_width += speed;
    }

  if (keyboard_state[SDL_SCANCODE_LEFTBRACKET] && m_stroke_width > 0.0f)
    {
      m_stroke_width -= speed;
      m_stroke_width = fastuidraw::t_max(m_stroke_width, 0.0f);
    }

  if (keyboard_state[SDL_SCANCODE_RIGHTBRACKET] || keyboard_state[SDL_SCANCODE_LEFTBRACKET])
    {
      std::cout << "Stroke width set to: " << m_stroke_width << "\n";
    }
  return return_value;
}

void
painter_glyph_test::
handle_event(const SDL_Event &ev)
{
  if (m_draw_glyphs)
    {
      m_zoomer.handle_event(ev);
    }
  else
    {
      m_atlas_zoomer.handle_event(ev);
    }
  switch(ev.type)
    {
    case SDL_QUIT:
      end_demo(0);
      break;

    case SDL_WINDOWEVENT:
      if (ev.window.event == SDL_WINDOWEVENT_RESIZED)
        {
          on_resize(ev.window.data1, ev.window.data2);
          if (m_screen_orientation.value() == PainterEnums::y_increases_upwards)
            {
              m_zoomer.m_translate_event.y() = static_cast<float>(ev.window.data2);
            }
        }
      break;

    case SDL_KEYUP:
      switch(ev.key.keysym.sym)
        {
        case SDLK_ESCAPE:
          end_demo(0);
          break;

        case SDLK_g:
          m_draw_glyphs = !m_draw_glyphs;
          if (m_draw_glyphs)
            {
              std::cout << "Set to draw glyphs.\n";
            }
          else
            {
              std::cout << "Set to draw glyph atlas.\n";
            }
          break;

        case SDLK_n:
          if (!m_draw_glyphs)
            {
              cycle_value(m_glyph_texel_page, ev.key.keysym.mod & (KMOD_SHIFT|KMOD_CTRL|KMOD_ALT),
                          m_glyph_atlas->texel_store()->dimensions().z());
            }
          break;

        case SDLK_a:
          if (!m_fill_glyphs)
            {
              m_use_anisotropic_anti_alias = !m_use_anisotropic_anti_alias;
              if (m_use_anisotropic_anti_alias)
                {
                  std::cout << "Using Anistropic anti-alias filtering\n";
                }
              else
                {
                  std::cout << "Using Istropic anti-alias filtering\n";
                }
            }
          break;

        case SDLK_d:
          cycle_value(m_current_drawer, ev.key.keysym.mod & (KMOD_SHIFT|KMOD_CTRL|KMOD_ALT), m_draws.size());
          std::cout << "Drawing " << m_draws[m_current_drawer] << " glyphs\n";
          break;

        case SDLK_z:
          {
            vec2 p, fixed_point(dimensions() / 2);
            ScaleTranslate<float> tr;
            tr = m_zoomer.transformation();
            p = fixed_point - (fixed_point - tr.translation()) / tr.scale();
            m_zoomer.transformation(ScaleTranslate<float>(p));
          }
          break;

        case SDLK_s:
          {
            m_stroke_glyphs = !m_stroke_glyphs;
            std::cout << "Set to ";
            if (!m_stroke_glyphs)
              {
                std::cout << "not ";
              }
            std::cout << " stroke glyph paths\n";
          }
          break;

        case SDLK_j:
          if (m_stroke_glyphs)
            {
              cycle_value(m_join_style, ev.key.keysym.mod & (KMOD_SHIFT|KMOD_CTRL|KMOD_ALT), PainterEnums::number_join_styles);
              std::cout << "Join drawing mode set to: " << m_join_labels[m_join_style] << "\n";
            }
          break;

        case SDLK_w:
          if (m_stroke_glyphs)
            {
              m_anti_alias_path_stroking = !m_anti_alias_path_stroking;
              std::cout << "Anti-aliasing of path stroking set to ";
              if (m_anti_alias_path_stroking)
                {
                  std::cout << "ON\n";
                }
              else
                {
                  std::cout << "OFF\n";
                }
            }
          break;

        case SDLK_p:
          if (m_stroke_glyphs)
            {
              m_pixel_width_stroking = !m_pixel_width_stroking;
              if (m_pixel_width_stroking)
                {
                  std::cout << "Set to stroke with pixel width stroking\n";
                }
              else
                {
                  std::cout << "Set to stroke with local coordinate width stroking\n";
                }
            }
          break;

        case SDLK_l:
          m_draw_stats = !m_draw_stats;
          break;

        case SDLK_f:
          m_fill_glyphs = !m_fill_glyphs;
          if (m_fill_glyphs)
            {
              std::cout << "Draw glyphs via path filling\n";
            }
          else
            {
              std::cout << "Draw glyphs with glyph renderer\n";
            }
          break;

        case SDLK_q:
          if (m_fill_glyphs)
            {
              m_anti_alias_path_filling = !m_anti_alias_path_filling;
              std::cout << "Anti-aliasing of path fill set to ";
              if (m_anti_alias_path_filling)
                {
                  std::cout << "ON\n";
                }
              else
                {
                  std::cout << "OFF\n";
                }
            }
          break;
        }
      break;
    }
}

int
main(int argc, char **argv)
{
  painter_glyph_test G;
  return G.main(argc, argv);
}
