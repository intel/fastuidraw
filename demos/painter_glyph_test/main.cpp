#include <iostream>
#include <sstream>
#include <fstream>
#include <fastuidraw/text/glyph_cache.hpp>
#include <fastuidraw/text/font_freetype.hpp>
#include <fastuidraw/text/glyph_selector.hpp>

#include "sdl_painter_demo.hpp"
#include "PanZoomTracker.hpp"
#include "text_helper.hpp"
#include "cycle_value.hpp"
#include "glyph_finder.hpp"

using namespace fastuidraw;

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

class GlyphDraws:fastuidraw::noncopyable
{
public:
  ~GlyphDraws();

  unsigned int
  size(void) const;

  const PainterAttributeData&
  data(unsigned int I) const;

  void
  init(unsigned int num_threads,
       const reference_counted_ptr<const FontFreeType> &font,
       const reference_counted_ptr<GlyphCache> &cache,
       const reference_counted_ptr<GlyphSelector> &selector,
       float pixel_size_formatting,
       GlyphRender renderer,
       size_t glyphs_per_painter_draw);

  void
  init(std::istream &istr,
       const reference_counted_ptr<const FontFreeType> &font,
       const reference_counted_ptr<GlyphSelector> &selector,
       float pixel_size_formatting,
       GlyphRender renderer,
       size_t glyphs_per_painter_draw);

  const GlyphFinder&
  glyph_finder(void) const
  {
    return m_glyph_finder;
  }

  c_array<const vec2>
  glyph_positions(void) const
  {
    return cast_c_array(m_glyph_positions);
  }

  c_array<const Glyph>
  glyphs(void) const
  {
    return cast_c_array(m_glyphs);
  }

  c_array<const range_type<float> >
  glyph_extents(void) const
  {
    return cast_c_array(m_glyph_extents);
  }

  c_array<const uint32_t>
  character_codes(void) const
  {
    return cast_c_array(m_character_codes);
  }

private:
  void
  set_data(float pixel_size, size_t glyphs_per_painter_draw);

  std::vector<PainterAttributeData*> m_data;
  std::vector<vec2> m_glyph_positions;
  std::vector<Glyph> m_glyphs;
  std::vector<range_type<float> > m_glyph_extents;
  std::vector<uint32_t> m_character_codes;
  GlyphFinder m_glyph_finder;
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
      draw_glyph_curvepair,
      draw_glyph_distance,

      number_draw_modes
    };

  enum return_code
  create_and_add_font(void);

  void
  ready_glyph_attribute_data(void);

  void
  init_glyph_draw(unsigned int I, GlyphRender renderer);

  float
  update_cts_params(void);

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

  reference_counted_ptr<const FontFreeType> m_font;

  vecN<GlyphDraws, number_draw_modes> m_draws;
  vecN<std::string, number_draw_modes> m_draw_labels;
  vecN<std::string, PainterEnums::number_join_styles> m_join_labels;

  bool m_use_anisotropic_anti_alias;
  bool m_stroke_glyphs, m_fill_glyphs;
  bool m_anti_alias_path_stroking, m_anti_alias_path_filling;
  bool m_pixel_width_stroking;
  bool m_draw_stats;
  float m_stroke_width;
  unsigned int m_current_drawer;
  unsigned int m_join_style;
  PanZoomTrackerSDLEvent m_zoomer;
  simple_time m_draw_timer;
};


////////////////////////////////////
// GlyphDraws methods
GlyphDraws::
~GlyphDraws()
{
  for(unsigned int i = 0, endi = m_data.size(); i < endi; ++i)
    {
      FASTUIDRAWdelete(m_data[i]);
    }
}

void
GlyphDraws::
init(unsigned int num_threads,
     const reference_counted_ptr<const FontFreeType> &font,
     const reference_counted_ptr<GlyphCache> &glyph_cache,
     const reference_counted_ptr<GlyphSelector> &glyph_selector,
     float pixel_size_formatting,
     GlyphRender renderer,
     size_t glyphs_per_painter_draw)
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
  reference_counted_ptr<FreeTypeFace> face;

  face = font->face_generator()->create_face(font->lib());
  div_scale_factor = static_cast<float>(face->face()->units_per_EM);
  scale_factor = pixel_size_formatting / div_scale_factor;

  /* Get all the glyphs */
  simple_time timer;
  std::vector<int> cnts;
  GlyphSetGenerator::generate(num_threads, renderer, font, face, m_glyphs, glyph_cache, cnts);
  std::cout << "Took " << timer.elapsed()
            << " ms to generate glyphs of type "
            << renderer << "\n";
  for(int i = 0; i < num_threads; ++i)
    {
      std::cout << "\tThread #" << i << " generated " << cnts[i] << " glyphs.\n";
    }

  for(Glyph g : m_glyphs)
    {
      FASTUIDRAWassert(g.valid());
      FASTUIDRAWassert(g.cache() == glyph_cache);

      tallest = std::max(tallest, g.layout().m_horizontal_layout_offset.y() + g.layout().m_size.y());
      negative_tallest = std::min(negative_tallest, g.layout().m_horizontal_layout_offset.y());
    }

  /* Try to get the character codes for each glyph */
  std::vector<bool> found_character_code(m_glyphs.size(), false);
  m_character_codes.resize(m_glyphs.size(), 0);

  for(int i = 0, endi = face->face()->num_charmaps; i < endi; ++i)
    {
      FT_Set_Charmap(face->face(), face->face()->charmaps[i]);
      for(character_code = FT_Get_First_Char(face->face(), &glyph_index);
          glyph_index != 0;
          character_code = FT_Get_Next_Char(face->face(), character_code, &glyph_index))
        {
          --glyph_index;
          FASTUIDRAWassert(glyph_index < static_cast<int>(m_glyphs.size()));
          if(!found_character_code[glyph_index])
            {
              m_character_codes[glyph_index] = character_code;
              found_character_code[glyph_index] = true;
            }
        }
    }

  tallest *= scale_factor;
  negative_tallest *= scale_factor;
  offset = tallest - negative_tallest;

  m_glyph_extents.resize(m_glyphs.size());
  m_glyph_positions.resize(m_glyphs.size());

  std::vector<LineData> lines;
  vec2 pen(0.0f, 0.0f);

  for(navigator_chars = 0, i = 0, endi = m_glyphs.size(), glyph_at_start = 0; i < endi; ++i)
    {
      Glyph g;
      GlyphLayoutData layout;
      float advance, nxt;

      g = m_glyphs[i];
      FASTUIDRAWassert(g.valid());

      layout = g.layout();
      advance = scale_factor * t_max(layout.m_advance.x(),
                                     t_max(0.0f, layout.m_horizontal_layout_offset.x()) + layout.m_size.x());

      m_glyph_positions[i].x() = pen.x();
      m_glyph_positions[i].y() = pen.y();

      m_glyph_extents[i].m_begin = pen.x() + scale_factor * layout.m_horizontal_layout_offset.x();
      m_glyph_extents[i].m_end = m_glyph_extents[i].m_begin + scale_factor * layout.m_size.x();

      pen.x() += advance;

      if(i + 1 < endi)
        {
          float pre_layout, nxt_adv;
          GlyphLayoutData nxtL(m_glyphs[i + 1].layout());

          pre_layout = t_max(0.0f, -nxtL.m_horizontal_layout_offset.x());
          pen.x() += scale_factor * pre_layout;
          nxt_adv = t_max(nxtL.m_advance.x(),
                          t_max(0.0f, nxtL.m_horizontal_layout_offset.x()) + nxtL.m_size.x());
          nxt = pen.x() + scale_factor * nxt_adv;
        }
      else
        {
          nxt = pen.x();
        }

      if(nxt >= line_length || i + 1 == endi)
        {
          std::ostringstream desc;
          desc << "[" << std::setw(5) << m_glyphs[glyph_at_start].layout().m_glyph_code
               << " - " << std::setw(5) << m_glyphs[i].layout().m_glyph_code << "]";
          navigator.push_back(std::make_pair(pen.y(), desc.str()));
          navigator_chars += navigator.back().second.length();

          LineData L;
          L.m_range.m_begin = glyph_at_start;
          L.m_range.m_end = i + 1;
          L.m_vertical_spread.m_begin = pen.y() - tallest;
          L.m_vertical_spread.m_end = pen.y() - negative_tallest;
          L.m_horizontal_spread.m_begin = 0.0f;
          L.m_horizontal_spread.m_end = pen.x();
          lines.push_back(L);
          glyph_at_start = i + 1;

          pen.x() = 0.0f;
          pen.y() += offset + 1.0f;
        }
    }

  m_glyph_finder.init(lines, cast_c_array(m_glyph_extents));

  m_character_codes.reserve(m_glyphs.size() + navigator_chars);
  m_glyph_positions.reserve(m_glyphs.size() + navigator_chars);
  m_glyphs.reserve(m_glyphs.size() + navigator_chars);

  std::vector<fastuidraw::Glyph> temp_glyphs;
  std::vector<fastuidraw::vec2> temp_positions;
  std::vector<uint32_t> temp_character_codes;
  for(nav_iter = navigator.begin(); nav_iter != navigator.end(); ++nav_iter)
    {
      std::istringstream stream(nav_iter->second);

      temp_glyphs.clear();
      temp_positions.clear();
      temp_character_codes.clear();
      create_formatted_text(stream, renderer, pixel_size_formatting,
                            font, glyph_selector, temp_glyphs,
                            temp_positions, temp_character_codes);

      FASTUIDRAWassert(temp_glyphs.size() == temp_positions.size());
      for(unsigned int c = 0; c < temp_glyphs.size(); ++c)
        {
          m_glyphs.push_back(temp_glyphs[c]);
          m_character_codes.push_back(temp_character_codes[c]);
          m_glyph_positions.push_back(vec2(line_length + temp_positions[c].x(), nav_iter->first) );
        }
    }
  set_data(pixel_size_formatting, glyphs_per_painter_draw);
}


void
GlyphDraws::
init(std::istream &istr,
     const reference_counted_ptr<const FontFreeType> &font,
     const reference_counted_ptr<GlyphSelector> &glyph_selector,
     float pixel_size_formatting,
     GlyphRender renderer,
     size_t glyphs_per_painter_draw)
{
  if(istr)
    {
      std::vector<LineData> lines;
      create_formatted_text(istr, renderer, pixel_size_formatting,
                            font, glyph_selector,
                            m_glyphs, m_glyph_positions,
                            m_character_codes, &lines, &m_glyph_extents);
      m_glyph_finder.init(lines, cast_c_array(m_glyph_extents));
    }
  set_data(pixel_size_formatting, glyphs_per_painter_draw);
}


void
GlyphDraws::
set_data(float pixel_size, size_t glyphs_per_painter_draw)
{
  c_array<const vec2> in_glyph_positions(cast_c_array(m_glyph_positions));
  c_array<const Glyph> in_glyphs(cast_c_array(m_glyphs));

  while(!in_glyphs.empty())
    {
      c_array<const Glyph> glyphs;
      c_array<const vec2> glyph_positions;
      unsigned int cnt;
      PainterAttributeData *data;

      cnt = t_min(in_glyphs.size(), glyphs_per_painter_draw);
      glyphs = in_glyphs.sub_array(0, cnt);
      glyph_positions = in_glyph_positions.sub_array(0, cnt);

      in_glyphs = in_glyphs.sub_array(cnt);
      in_glyph_positions = in_glyph_positions.sub_array(cnt);

      data = FASTUIDRAWnew PainterAttributeData();
      data->set_data(PainterAttributeDataFillerGlyphs(glyph_positions, glyphs, pixel_size));
      m_data.push_back(data);
    }
}

unsigned int
GlyphDraws::
size(void) const
{
  return m_data.size();
}

const PainterAttributeData&
GlyphDraws::
data(unsigned int I) const
{
  FASTUIDRAWassert(I < m_data.size());
  return *m_data[I];
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
  m_max_distance(96.0f, "max_distance",
                 "value to use for max distance in 64'ths of a pixel "
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
  m_use_anisotropic_anti_alias(false),
  m_stroke_glyphs(false),
  m_fill_glyphs(false),
  m_anti_alias_path_stroking(false),
  m_anti_alias_path_filling(false),
  m_pixel_width_stroking(true),
  m_draw_stats(false),
  m_stroke_width(1.0f),
  m_current_drawer(draw_glyph_curvepair),
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

  if(!m_font_file.m_value.empty())
    {
      reference_counted_ptr<FreeTypeFace::GeneratorBase> gen;
      gen = FASTUIDRAWnew FreeTypeFace::GeneratorMemory(m_font_file.m_value.c_str(), 0);
      if(gen->check_creation() == routine_success)
        {
          font = FASTUIDRAWnew FontFreeType(gen,
                                            FontFreeType::RenderParams()
                                            .distance_field_max_distance(m_max_distance.m_value)
                                            .distance_field_pixel_size(m_distance_pixel_size.m_value)
                                            .curve_pair_pixel_size(m_curve_pair_pixel_size.m_value),
                                            m_ft_lib);
        }
    }

  add_fonts_from_path(m_font_path.m_value, m_ft_lib, m_glyph_selector,
                      FontFreeType::RenderParams()
                      .distance_field_max_distance(m_max_distance.m_value)
                      .distance_field_pixel_size(m_distance_pixel_size.m_value)
                      .curve_pair_pixel_size(m_curve_pair_pixel_size.m_value));

  if(!font)
    {
      FontProperties props;
      props.style(m_font_style.m_value.c_str());
      props.family(m_font_family.m_value.c_str());
      props.bold(m_font_bold.m_value);
      props.italic(m_font_italic.m_value);

      font = m_glyph_selector->fetch_font(props);
    }

  m_font = font.dynamic_cast_ptr<const FontFreeType>();
  if(m_font)
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
derived_init(int w, int h)
{
  FASTUIDRAWunused(w);
  FASTUIDRAWunused(h);

  if(create_and_add_font() == routine_fail)
    {
      end_demo(-1);
      return;
    }

  //put into unit of per us
  m_change_stroke_width_rate.m_value /= (1000.0f * 1000.0f);

  ready_glyph_attribute_data();
  m_draw_timer.restart();
}

void
painter_glyph_test::
init_glyph_draw(unsigned int I, GlyphRender renderer)
{
  if(m_draw_glyph_set.m_value)
    {
      m_draws[I].init(m_realize_glyphs_thread_count.m_value,
                      m_font, m_glyph_cache, m_glyph_selector,
                      m_render_pixel_size.m_value, renderer,
                      m_glyphs_per_painter_draw.m_value);
    }
  else if(m_use_file.m_value)
    {
      std::ifstream istr(m_text.m_value.c_str(), std::ios::binary);
      m_draws[I].init(istr, m_font, m_glyph_selector,
                      m_render_pixel_size.m_value, renderer,
                      m_glyphs_per_painter_draw.m_value);
    }
  else
    {
      std::istringstream istr(m_text.m_value);
      m_draws[I].init(istr, m_font, m_glyph_selector,
                      m_render_pixel_size.m_value, renderer,
                      m_glyphs_per_painter_draw.m_value);
    }
}



void
painter_glyph_test::
ready_glyph_attribute_data(void)
{
  {
    GlyphRender renderer(distance_field_glyph);
    FASTUIDRAWassert(renderer.m_type == distance_field_glyph);
    init_glyph_draw(draw_glyph_distance, renderer);
    m_draw_labels[draw_glyph_distance] = "draw_glyph_distance";
  }

  {
    GlyphRender renderer(curve_pair_glyph);
    FASTUIDRAWassert(renderer.m_type == curve_pair_glyph);
    init_glyph_draw(draw_glyph_curvepair, renderer);
    m_draw_labels[draw_glyph_curvepair] = "draw_glyph_curvepair";
  }

  {
    GlyphRender renderer(m_coverage_pixel_size.m_value);
    FASTUIDRAWassert(renderer.m_type == coverage_glyph);
    init_glyph_draw(draw_glyph_coverage, renderer);
    m_draw_labels[draw_glyph_coverage] = "draw_glyph_coverage";
  }
}

void
painter_glyph_test::
draw_frame(void)
{
  float us;

  us = update_cts_params();

  glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
  m_painter->begin(m_surface);

  ivec2 wh(dimensions());
  float3x3 proj(float_orthogonal_projection_params(0, wh.x(), wh.y(), 0)), m;
  m = proj * m_zoomer.transformation().matrix3();
  m_painter->transformation(m);

  PainterBrush brush;
  brush.pen(1.0, 1.0, 1.0, 1.0);

  if(!m_fill_glyphs)
    {
      for(unsigned int S = 0, endS = m_draws[m_current_drawer].size(); S < endS; ++S)
        {
          m_painter->draw_glyphs(PainterData(&brush),
                                 m_draws[m_current_drawer].data(S),
                                 m_use_anisotropic_anti_alias);
        }
    }
  else
    {
      unsigned int src(m_current_drawer);
      c_array<const Glyph> glyphs(m_draws[src].glyphs());
      c_array<const vec2> glyph_positions(m_draws[src].glyph_positions());

      PainterBrush fill_brush;
      fill_brush.pen(1.0, 1.0, 1.0, 1.0);

      // reuse brush parameters across all glyphs
      PainterPackedValue<PainterBrush> pbr;
      pbr = m_painter->packed_value_pool().create_packed_value(fill_brush);

      for(unsigned int i = 0; i < glyphs.size(); ++i)
        {
          if(glyphs[i].valid())
            {
              m_painter->save();
              m_painter->translate(glyph_positions[i]);

              //make the scale of the path match how we scaled the text.
              float sc;
              sc = m_render_pixel_size.m_value / glyphs[i].layout().m_units_per_EM;

	      //we are drawing with y-coordinate increasing downwards
	      //which is the opposite coordinate system as the glyph's
	      //path, thus we also need to negate in the y-direction.
              m_painter->shear(sc, -sc);

              m_painter->fill_path(PainterData(pbr),
                                   glyphs[i].path(),
                                   PainterEnums::nonzero_fill_rule,
                                   m_anti_alias_path_filling);
              m_painter->restore();
            }
        }
    }

  if(m_stroke_glyphs)
    {
      unsigned int src;
      PainterBrush stroke_brush;
      stroke_brush.pen(0.0, 1.0, 1.0, 0.8);

      PainterStrokeParams st;
      st.miter_limit(5.0f);
      st.width(m_stroke_width);

      src = m_current_drawer;

      c_array<const Glyph> glyphs(m_draws[src].glyphs());
      c_array<const vec2> glyph_positions(m_draws[src].glyph_positions());

      // reuse stroke and brush parameters across all glyphs
      PainterPackedValue<PainterBrush> pbr;
      pbr = m_painter->packed_value_pool().create_packed_value(stroke_brush);

      PainterPackedValue<PainterItemShaderData> pst;
      pst = m_painter->packed_value_pool().create_packed_value(st);

      for(unsigned int i = 0; i < glyphs.size(); ++i)
        {
          if(glyphs[i].valid())
            {
              m_painter->save();
              m_painter->translate(glyph_positions[i]);

              //make the scale of the path match how we scaled the text.
              float sc;
              sc = m_render_pixel_size.m_value / glyphs[i].layout().m_units_per_EM;

	      //we are drawing with y-coordinate increasing downwards
	      //which is the opposite coordinate system as the glyph's
	      //path, thus we also need to negate in the y-direction.
              m_painter->shear(sc, -sc);

              if(m_pixel_width_stroking)
                {
                  m_painter->stroke_path_pixel_width(PainterData(pst, pbr),
                                                     glyphs[i].path(),
                                                     true, PainterEnums::flat_caps,
                                                     static_cast<enum PainterEnums::join_style>(m_join_style),
                                                     m_anti_alias_path_stroking);
                }
              else
                {
                  m_painter->stroke_path(PainterData(pst, pbr),
                                         glyphs[i].path(),
                                         true, PainterEnums::flat_caps,
                                         static_cast<enum PainterEnums::join_style>(m_join_style),
                                         m_anti_alias_path_stroking);
                }
              m_painter->restore();
            }
        }
    }

  if(m_draw_stats)
    {
      std::ostringstream ostr;

      ostr << "FPS = ";
      if(us > 0.0f)
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
           << m_painter->query_stat(PainterPacker::num_draws)
           << "\n";

      m_painter->transformation(proj);
      PainterBrush brush;
      brush.pen(0.0f, 1.0f, 1.0f, 1.0f);
      draw_text(ostr.str(), 32.0f, m_font, GlyphRender(distance_field_glyph), PainterData(&brush));
    }
  else
    {
      vec2 p;
      unsigned int G, src;
      ivec2 mouse_position;
      std::ostringstream ostr;

      src = m_current_drawer;

      SDL_GetMouseState(&mouse_position.x(), &mouse_position.y());
      p = m_zoomer.transformation().apply_inverse_to_point(vec2(mouse_position));
      G = m_draws[src].glyph_finder().glyph_source(p);
      if(G != GlyphFinder::glyph_not_found)
        {
          Glyph glyph;
          GlyphLayoutData layout;
          float ratio;
          vec2 wh, q;

          glyph = m_draws[src].glyphs()[G];
          layout = glyph.layout();
          ratio = m_render_pixel_size.m_value / layout.m_units_per_EM;

          q.x() = m_draws[src].glyph_positions()[G].x() + ratio * layout.m_horizontal_layout_offset.x();
          q.y() = m_draws[src].glyph_positions()[G].y() - ratio * layout.m_horizontal_layout_offset.y();
          wh.x() = ratio * layout.m_size.x();
          wh.y() = -ratio * layout.m_size.y();

          if(p.x() >= t_min(q.x(), q.x() + wh.x())
             && p.x() <= t_max(q.x(), q.x() + wh.x())
             && p.y() >= t_min(q.y(), q.y() + wh.y())
             && p.y() <= t_max(q.y(), q.y() + wh.y()))
            {
              ostr << "Glyph at " << p << " is:"
                   << "\n\tcharacter_code: " << m_draws[src].character_codes()[G]
                   << "\n\tglyph_code: " << layout.m_glyph_code
                   << "\n\tunits_per_EM: " << layout.m_units_per_EM
                   << "\n\tsize in EM: " << layout.m_size
                   << "\n\tsize normalized: " << layout.m_size * ratio
                   << "\n\tx-extents: [" << m_draws[src].glyph_extents()[G].m_begin
                   << ", " << m_draws[src].glyph_extents()[G].m_end << "]"
                   << "\n\tGlyphCoords in SCM=" << vec2(p.x() - q.x(), q.y() - p.y()) / ratio
                   << "\n\tGlyphCoords in pixel-size=" << vec2(p.x() - q.x(), q.y() - p.y())
                   << "\n";

              /* draw a box around the glyph(!).
               */
              PainterBrush brush;

              brush.pen(1.0f, 0.0f, 0.0f, 0.3f);
              m_painter->draw_rect(PainterData(&brush), q, wh, false);
            }
        }
      else
        {
          ostr << "No glyph at " << p << "\n";
        }

      m_painter->transformation(proj);
      PainterBrush brush;
      brush.pen(0.0f, 1.0f, 1.0f, 1.0f);
      draw_text(ostr.str(), 32.0f, m_font, GlyphRender(distance_field_glyph), PainterData(&brush));
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
  speed = return_value * m_change_stroke_width_rate.m_value;

  if(keyboard_state[SDL_SCANCODE_LSHIFT])
    {
      speed *= 0.1f;
    }
  if(keyboard_state[SDL_SCANCODE_RSHIFT])
    {
      speed *= 10.0f;
    }

  if(keyboard_state[SDL_SCANCODE_RIGHTBRACKET])
    {
      m_stroke_width += speed;
    }

  if(keyboard_state[SDL_SCANCODE_LEFTBRACKET] && m_stroke_width > 0.0f)
    {
      m_stroke_width -= speed;
      m_stroke_width = fastuidraw::t_max(m_stroke_width, 0.0f);
    }

  if(keyboard_state[SDL_SCANCODE_RIGHTBRACKET] || keyboard_state[SDL_SCANCODE_LEFTBRACKET])
    {
      std::cout << "Stroke width set to: " << m_stroke_width << "\n";
    }
  return return_value;
}

void
painter_glyph_test::
handle_event(const SDL_Event &ev)
{
  m_zoomer.handle_event(ev);
  switch(ev.type)
    {
    case SDL_QUIT:
      end_demo(0);
      break;

    case SDL_WINDOWEVENT:
      if(ev.window.event == SDL_WINDOWEVENT_RESIZED)
        {
          on_resize(ev.window.data1, ev.window.data2);
        }
      break;

    case SDL_KEYUP:
      switch(ev.key.keysym.sym)
        {
        case SDLK_ESCAPE:
          end_demo(0);
          break;

        case SDLK_a:
          if(!m_fill_glyphs)
            {
              m_use_anisotropic_anti_alias = !m_use_anisotropic_anti_alias;
              if(m_use_anisotropic_anti_alias)
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
          cycle_value(m_current_drawer, ev.key.keysym.mod & (KMOD_SHIFT|KMOD_CTRL|KMOD_ALT), number_draw_modes);
          std::cout << "Drawing " << m_draw_labels[m_current_drawer] << " glyphs\n";
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
            if(!m_stroke_glyphs)
              {
                std::cout << "not ";
              }
            std::cout << " stroke glyph paths\n";
          }
          break;

        case SDLK_j:
          if(m_stroke_glyphs)
            {
              cycle_value(m_join_style, ev.key.keysym.mod & (KMOD_SHIFT|KMOD_CTRL|KMOD_ALT), PainterEnums::number_join_styles);
              std::cout << "Join drawing mode set to: " << m_join_labels[m_join_style] << "\n";
            }
          break;

        case SDLK_w:
          if(m_stroke_glyphs)
            {
              m_anti_alias_path_stroking = !m_anti_alias_path_stroking;
              std::cout << "Anti-aliasing of path stroking set to ";
              if(m_anti_alias_path_stroking)
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
          if(m_stroke_glyphs)
            {
              m_pixel_width_stroking = !m_pixel_width_stroking;
              if(m_pixel_width_stroking)
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
          if(m_fill_glyphs)
            {
              std::cout << "Draw glyphs via path filling\n";
            }
          else
            {
              std::cout << "Draw glyphs with glyph renderer\n";
            }
          break;

        case SDLK_q:
          if(m_fill_glyphs)
            {
              m_anti_alias_path_filling = !m_anti_alias_path_filling;
              std::cout << "Anti-aliasing of path fill set to ";
              if(m_anti_alias_path_filling)
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
