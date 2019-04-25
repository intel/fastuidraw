#include <iostream>
#include <sstream>
#include <fstream>
#include <fastuidraw/text/glyph_cache.hpp>
#include <fastuidraw/text/glyph_generate_params.hpp>
#include <fastuidraw/text/font_freetype.hpp>
#include <fastuidraw/text/font_database.hpp>
#include <fastuidraw/text/glyph_render_data_restricted_rays.hpp>

#include "sdl_painter_demo.hpp"
#include "PanZoomTracker.hpp"
#include "text_helper.hpp"
#include "path_util.hpp"
#include "cycle_value.hpp"
#include "generic_hierarchy.hpp"
#include "command_line_list.hpp"
#include "print_utils.hpp"
#include "simple_time_circular_array.hpp"

using namespace fastuidraw;

const char*
on_off(bool v)
{
  return v ? "on" : "off";
}

std::ostream&
operator<<(std::ostream &str, GlyphRenderer R)
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

        case restricted_rays_glyph:
          str << "RestrictedRays";
          break;

        case banded_rays_glyph:
          str << "BandedRays";
          break;

        default:
          str << "Unknown";
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

  GlyphSequence&
  glyph_sequence(void)
  {
    return *m_glyph_sequence;
  }

  void
  realize_glyphs(GlyphRenderer R);

  GlyphRenderer
  draw_glyphs(GlyphRenderer render,
              const reference_counted_ptr<Painter> &painter,
              const PainterData &data) const;

  void
  init(const reference_counted_ptr<const FontBase> &font,
       float line_length, GlyphCache &glyph_cache,
       const reference_counted_ptr<FontDatabase> &selector,
       float format_size_formatting,
       enum Painter::screen_orientation screen_orientation);

  void
  init(const std::vector<uint32_t> &glyph_codes,
       const reference_counted_ptr<const FontBase> &font,
       GlyphCache &glyph_cache, float format_size_formatting,
       enum Painter::screen_orientation screen_orientation);

  void
  init(std::istream &istr,
       const reference_counted_ptr<const FontBase> &font,
       GlyphCache &glyph_cache,
       const reference_counted_ptr<FontDatabase> &selector,
       float format_size_formatting,
       enum Painter::screen_orientation screen_orientation);

  void
  post_finalize(void)
  {
    make_hierarchy();
  }

  void
  realize_all_glyphs(GlyphRenderer renderer,
                     const reference_counted_ptr<const FontBase> &font,
                     int num_threads);

private:
  void
  make_hierarchy(void);

  GlyphSequence *m_glyph_sequence;
  GenericHierarchy *m_hierarchy;
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
      draw_glyph_restricted_rays,
      draw_glyph_banded_rays,

      draw_glyph_auto
    };

  typedef enum Painter::screen_orientation screen_orientation;

  enum return_code
  create_and_add_font(void);

  void
  realize_all_glyphs(GlyphRenderer renderer);

  void
  ready_glyph_data(int w, int h);

  float
  update_cts_params(void);

  void
  stroke_glyph(const PainterData &d, GlyphMetrics G, GlyphRenderer R);

  void
  fill_glyph(const PainterData &d, GlyphMetrics G, GlyphRenderer R);

  void
  draw_glyphs(float us);

  vec2
  item_coordinates(ivec2 src);

  command_line_argument_value<std::string> m_font_path;
  command_line_argument_value<std::string> m_font_foundry, m_font_style, m_font_family;
  command_line_argument_value<bool> m_font_bold, m_font_italic;
  command_line_argument_value<bool> m_font_ignore_style, m_font_ignore_bold_italic;
  command_line_argument_value<bool> m_use_font_config;
  command_line_list<std::string> m_font_langs;
  command_line_argument_value<int> m_font_weight, m_font_slant;
  command_line_argument_value<bool> m_font_exact_match;
  command_line_argument_value<std::string> m_font_file;
  command_line_argument_value<int> m_coverage_pixel_size;
  command_line_argument_value<std::string> m_text;
  command_line_argument_value<bool> m_use_file;
  command_line_argument_value<bool> m_draw_glyph_set;
  command_line_argument_value<int> m_realize_glyphs_thread_count;
  command_line_argument_value<float> m_render_format_size;
  command_line_argument_value<float> m_bg_red, m_bg_green, m_bg_blue;
  command_line_argument_value<float> m_fg_red, m_fg_green, m_fg_blue;
  command_line_argument_value<float> m_change_stroke_width_rate;
  command_line_list<uint32_t> m_explicit_glyph_codes;
  enumerated_command_line_argument_value<screen_orientation> m_screen_orientation;

  reference_counted_ptr<const FontBase> m_font, m_default_font;
  GlyphDrawsShared m_draw_shared;

  /* The last entry value is left as invalid to represent
   * to auto-choose how to render the glyphs.
   */
  vecN<GlyphRenderer, draw_glyph_auto + 1> m_draws;
  unsigned int m_glyph_texel_page;

  bool m_stroke_glyphs, m_fill_glyphs, m_draw_path_pts;
  bool m_anti_alias_path_stroking, m_anti_alias_path_filling;
  enum Painter::stroking_method_t m_stroking_method;
  bool m_pixel_width_stroking;
  bool m_draw_stats, m_draw_restricted_rays_box_slack;
  float m_stroke_width;
  unsigned int m_current_drawer;
  enum Painter::join_style m_join_style;
  vec2 m_shear, m_shear2;
  float m_angle;
  float m_restricted_rays_box_slack;

  PanZoomTrackerSDLEvent m_zoomer;
  simple_time m_draw_timer;
  simple_time_circular_array<128> m_draw_times;
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

  if (m_hierarchy)
    {
      FASTUIDRAWdelete(m_hierarchy);
    }
}

void
GlyphDrawsShared::
make_hierarchy(void)
{
  enum Painter::screen_orientation orientation(m_glyph_sequence->orientation());
  float format_size(m_glyph_sequence->format_size());
  GlyphMetrics metrics;
  vec2 p;
  BoundingBox<float> bbox;
  std::vector<BoundingBox<float> > glyph_bboxes;
  simple_time timer;

  std::cout << "Creating GlyphHierarch..." << std::flush;
  glyph_bboxes.resize(m_glyph_sequence->number_glyphs());
  for(unsigned int i = 0, endi = m_glyph_sequence->number_glyphs(); i < endi; ++i)
    {
      m_glyph_sequence->added_glyph(i, &metrics, &p);
      if(metrics.valid())
        {
          vec2 min_bb, max_bb;
          float ratio;

          min_bb = metrics.horizontal_layout_offset();
          max_bb = min_bb + metrics.size();

          ratio = format_size / metrics.units_per_EM();
          min_bb *= ratio;
          max_bb *= ratio;

          if (orientation == Painter::y_increases_downwards)
            {
              min_bb.y() = -min_bb.y();
              max_bb.y() = -max_bb.y();
            }

          glyph_bboxes[i].union_point(p + min_bb);
          glyph_bboxes[i].union_point(p + max_bb);
          bbox.union_box(glyph_bboxes[i]);
        }
    }

  m_hierarchy = FASTUIDRAWnew GenericHierarchy(bbox);
  for(unsigned int i = 0, endi = m_glyph_sequence->number_glyphs(); i < endi; ++i)
    {
      m_hierarchy->add(glyph_bboxes[i], i);
    }
  std::cout << "took " << timer.restart() << " ms\n";
}

void
GlyphDrawsShared::
init(const reference_counted_ptr<const FontBase> &pfont,
     float line_length,
     GlyphCache &glyph_cache,
     const reference_counted_ptr<FontDatabase> &font_database,
     float format_size_formatting,
     enum Painter::screen_orientation screen_orientation)
{
  float scale_factor, offset;
  unsigned int i, endi, glyph_at_start, navigator_chars;
  std::list< std::pair<float, std::string> > navigator;
  std::list< std::pair<float, std::string> >::iterator nav_iter;
  unsigned int num_glyphs;
  float y_advance_sign;
  std::vector<GlyphMetrics> metrics;
  simple_time timer;
  reference_counted_ptr<FreeTypeFace> face;
  reference_counted_ptr<const FontFreeType> font;

  font = pfont.dynamic_cast_ptr<const FontFreeType>();
  face = font->face_generator()->create_face();
  scale_factor = format_size_formatting / static_cast<float>(face->face()->units_per_EM);
  y_advance_sign = (screen_orientation == Painter::y_increases_downwards) ? 1.0f : -1.0f;
  offset = scale_factor * static_cast<float>(face->face()->height);
  num_glyphs = font->number_glyphs();

  m_glyph_sequence = FASTUIDRAWnew GlyphSequence(format_size_formatting,
                                                 screen_orientation, glyph_cache);

  std::cout << "Formatting glyphs ..." << std::flush;
  metrics.resize(num_glyphs);
  for(unsigned int i = 0; i < num_glyphs; ++i)
    {
      metrics[i] = glyph_cache.fetch_glyph_metrics(font.get(), i);
    }

  vec2 pen(0.0f, 0.0f);
  for(navigator_chars = 0, i = 0, endi = metrics.size(), glyph_at_start = 0; i < endi; ++i)
    {
      GlyphMetrics metric(metrics[i]);
      float advance, nxt;

      advance = scale_factor * t_max(metric.advance().x(),
                                     t_max(0.0f, metric.horizontal_layout_offset().x()) + metric.size().x());
      advance += 1.0; //a little additional slack between glyphs.

      m_glyph_sequence->add_glyph(GlyphSource(i, font.get()), pen);
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

      create_formatted_text(*m_glyph_sequence, stream, font.get(),
                            font_database,
                            vec2(line_length, nav_iter->first));
    }
  std::cout << "took " << timer.restart() << " ms\n";
}

void
GlyphDrawsShared::
init(const std::vector<uint32_t> &glyph_codes,
     const reference_counted_ptr<const FontBase> &font,
     GlyphCache &glyph_cache,
     float format_size_formatting,
     enum Painter::screen_orientation screen_orientation)
{
  simple_time timer;

  std::cout << "Formatting glyphs ..." << std::flush;
  m_glyph_sequence = FASTUIDRAWnew GlyphSequence(format_size_formatting,
                                                 screen_orientation, glyph_cache);
  create_formatted_text(*m_glyph_sequence, glyph_codes, font.get());

  std::cout << "took " << timer.restart() << " ms\n";
}

void
GlyphDrawsShared::
init(std::istream &istr,
     const reference_counted_ptr<const FontBase> &font,
     GlyphCache &glyph_cache,
     const reference_counted_ptr<FontDatabase> &font_database,
     float format_size_formatting,
     enum Painter::screen_orientation screen_orientation)
{
  m_glyph_sequence = FASTUIDRAWnew GlyphSequence(format_size_formatting,
                                                 screen_orientation, glyph_cache);
  if (istr)
    {
      simple_time timer;

      std::cout << "Formatting glyphs ..." << std::flush;
      create_formatted_text(*m_glyph_sequence, istr, font.get(), font_database);
      std::cout << "took " << timer.restart() << " ms\n";
    }
}

void
GlyphDrawsShared::
realize_glyphs(GlyphRenderer R)
{
  unsigned int num;

  num = m_glyph_sequence->number_subsets();
  for (unsigned int i = 0; i < num; ++i)
    {
      GlyphSequence::Subset S(m_glyph_sequence->subset(i));
      c_array<const PainterAttribute> out_attributes;
      c_array<const PainterIndex> out_indices;

      S.attributes_and_indices(R, &out_attributes, &out_indices);
    }
}

GlyphRenderer
GlyphDrawsShared::
draw_glyphs(GlyphRenderer render,
            const reference_counted_ptr<Painter> &painter,
            const PainterData &draw) const
{
  return painter->draw_glyphs(draw, *m_glyph_sequence, render);
}

/////////////////////////////////////
// painter_glyph_test methods
painter_glyph_test::
painter_glyph_test(void):
  m_font_path(default_font_path(), "font_path", "Specifies path in which to search for fonts", *this),
  m_font_foundry("", "font_foundry", "Specifies the font foundry", *this),
  m_font_style("Book", "font_style", "Specifies the font style", *this),
  m_font_family("DejaVu Sans", "font_family", "Specifies the font family name", *this),
  m_font_bold(false, "font_bold", "if true select a bold font", *this),
  m_font_italic(false, "font_italic", "if true select an italic font", *this),
  m_font_ignore_style(false, "font_ignore_style", "if true, when selecting a font ignore style value", *this),
  m_font_ignore_bold_italic(false, "font_ignore_bold_italic", "if true, when selecting a font ignore bold and italic values", *this),
  m_use_font_config(false, "use_font_config", "If true, use font config to select font", *this),
  m_font_langs("add_font_lang",
               "Add a language requirement when choosing the font with FontConfig. "
               "Languages are encoded as strings as defined by RFC-3066. ",
               *this),
  m_font_weight(-1, "font_weight", "Only has effect if value is non-negative and use_font_config is true. "
                "Gives the value for FC_WEIGHT to pass for fontconfig for font selection",
                *this),
  m_font_slant(-1, "font_slant", "Only has effect if value is non-negative and use_font_config is true. "
               "Gives the value for FC_SLANT to pass for fontconfig for font selection",
               *this),
  m_font_exact_match(false, "font_exact_match", "if true, require an exact match when selecting a font ignore style value", *this),
  m_font_file("", "font_file",
              "If non-empty gives the name of a font by filename "
              "thus bypassing the glyph selection process with font_database",
              *this),
  m_coverage_pixel_size(24, "coverage_pixel_size", "Pixel size at which to create coverage glyphs", *this),
  m_text("Hello World!", "text", "text to draw to the screen", *this),
  m_use_file(false, "use_file", "if true the value for text gives a filename to display", *this),
  m_draw_glyph_set(false, "draw_glyph_set", "if true, display all glyphs of font instead of text", *this),
  m_realize_glyphs_thread_count(1, "realize_glyphs_thread_count",
                                "If draw_glyph_set is true, gives the number of threads to use "
                                "to create the glyph data",
                                *this),
  m_render_format_size(24.0f, "render_format_size", "format size at which to display glyphs", *this),
  m_bg_red(0.0f, "bg_red", "Background Red", *this),
  m_bg_green(0.0f, "bg_green", "Background Green", *this),
  m_bg_blue(0.0f, "bg_blue", "Background Blue", *this),
  m_fg_red(1.0f, "fg_red", "Foreground Red", *this),
  m_fg_green(1.0f, "fg_green", "Foreground Green", *this),
  m_fg_blue(1.0f, "fg_blue", "Foreground Blue", *this),

  m_change_stroke_width_rate(10.0f, "change_stroke_width_rate",
                             "rate of change in pixels/sec for changing stroke width "
                             "when changing stroke when key is down",
                             *this),
  m_explicit_glyph_codes("add_glyph_code",
                         "Add an explicit glyph code to render, if the list "
                         "is non-empty, takes precendence over text",
                         *this),
  m_screen_orientation(Painter::y_increases_downwards,
                      enumerated_string_type<screen_orientation>()
                      .add_entry("y_downwards",
                                 Painter::y_increases_downwards,
                                 "Make coordinate system so that y-coordinate "
                                 "increases downwards (i.e. top of the window "
                                 "has y-coordinate is 0")
                      .add_entry("y_upwards",
                                 Painter::y_increases_upwards,
                                 "Make coordinate system so that y-coordinate "
                                 "increases upwards (i.e. bottom of the window "
                                 "has y-coordinate is 0"),
                      "y_orientation",
                      "Determine y-coordiante convention",
                      *this),
  m_glyph_texel_page(0),
  m_stroke_glyphs(false),
  m_fill_glyphs(false),
  m_draw_path_pts(false),
  m_anti_alias_path_stroking(false),
  m_anti_alias_path_filling(false),
  m_stroking_method(Painter::stroking_method_fastest),
  m_pixel_width_stroking(false),
  m_draw_stats(false),
  m_draw_restricted_rays_box_slack(false),
  m_stroke_width(1.0f),
  m_current_drawer(draw_glyph_banded_rays),
  m_shear(1.0f, 1.0f),
  m_shear2(1.0f, 1.0f),
  m_angle(0.0f),
  m_join_style(Painter::miter_joins)
{
  std::cout << "Controls:\n"
            << "\tl: toggle showing glyph stats and FPS\n"
            << "\tctrl-l: toggle drawing restircted rays box slack at cursor\n"
            << "\td: cycle drawing mode: draw coverage glyph, draw distance glyphs "
            << "[hold shift, control or mode to reverse cycle]\n"
            << "\td: Cycle though text renderer\n"
            << "\tf: Toggle rendering text as filled path\n"
            << "\tq: Cycle through anti-aliasing filled path rendering modes\n"
            << "\tw: Cycle through anti-aliasing stroked path rendering modes\n"
            << "\tx: Cycle through stroking path realization mode\n"
            << "\tp: Toggle pixel width stroking\n"
            << "\tctrl-p: toggle showing points (blue), control pts(blue) and arc-center(green) of glyphs"
            << "\tz: reset zoom factor to 1.0\n"
            << "\ts: toggle stroking glyph path\n"
            << "\tj: cycle through join styles for stroking\n"
            << "\tl: draw Painter stats\n"
            << "\t[: decrease stroke width(hold left-shift for slower rate and right shift for faster)\n"
            << "\t]: increase stroke width(hold left-shift for slower rate and right shift for faster)\n"
            << "\t6: x-shear (hold ctrl to decrease, hold enter for shear2)\n"
            << "\t7: y-shear (hold ctrl to decrease, hold enter for shear2)\n"
            << "\t0: Rotate left\n"
            << "\t9: Rotate right\n"
            << "\tMouse Drag (left button): pan\n"
            << "\tHold Mouse (left button), then drag up/down: zoom out/in\n";
}

painter_glyph_test::
~painter_glyph_test()
{
}

enum return_code
painter_glyph_test::
create_and_add_font(void)
{
  if (!m_font_file.value().empty())
    {
      reference_counted_ptr<FreeTypeFace::GeneratorBase> gen;
      gen = FASTUIDRAWnew FreeTypeFace::GeneratorMemory(m_font_file.value().c_str(), 0);
      if (gen->check_creation() == routine_success)
        {
          m_font = FASTUIDRAWnew FontFreeType(gen, m_ft_lib);
        }
    }

  if (m_use_font_config.value())
    {
      add_fonts_from_font_config(m_ft_lib, m_font_database);
    }
  else
    {
      add_fonts_from_path(m_font_path.value(), m_ft_lib, m_font_database);
    }

  if (!m_font)
    {
      if (m_use_font_config.value())
        {
          m_font = select_font_font_config(m_font_ignore_bold_italic.value() ? -1 : m_font_weight.value(),
                                           m_font_ignore_bold_italic.value() ? -1 : m_font_slant.value(),
                                           m_font_ignore_style.value() ? nullptr : m_font_style.value().c_str(),
                                           m_font_family.value().empty() ? nullptr : m_font_family.value().c_str(),
                                           m_font_foundry.value().empty() ? nullptr : m_font_foundry.value().c_str(),
                                           m_font_langs,
                                           m_ft_lib, m_font_database);
        }
      else
        {
          FontProperties props;
          uint32_t flags(0);

          props
            .style(m_font_style.value().c_str())
            .family(m_font_family.value().c_str())
            .foundry(m_font_foundry.value().c_str())
            .bold(m_font_bold.value())
            .italic(m_font_italic.value());

          if (m_font_ignore_style.value())
            {
              flags |= FontDatabase::ignore_style;
            }

          if (m_font_ignore_bold_italic.value())
            {
              flags |= FontDatabase::ignore_bold_italic;
            }

          if (m_font_exact_match.value())
            {
              flags |= FontDatabase::exact_match;
            }

          m_font = m_font_database->fetch_font(props, flags);
        }
    }

  m_default_font = m_font_database->fetch_font(FontProperties()
                                               .style("Book")
                                               .family("DejaVu Sans")
                                               .foundry("")
                                               .bold(false)
                                               .italic(false),
                                               0u);
  if (m_font)
    {
      std::cout << "Chose font: \"" << m_font->properties() << "\"\n";
    }
  else
    {
      std::cout << "\n-----------------------------------------------------\n"
                << "Warning: unable to create font, either choose a font\n"
                << "file explicitly with font_file or select a path with\n"
                << "fonts via font_path\n"
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

  if (create_and_add_font() == routine_fail)
    {
      end_demo(-1);
      return;
    }

  //put into unit of per us
  m_change_stroke_width_rate.value() /= (1000.0f * 1000.0f);

  ready_glyph_data(w, h);
  m_draw_timer.restart();

  if (m_screen_orientation.value() == Painter::y_increases_upwards)
    {
      m_zoomer.m_zoom_direction = PanZoomTracker::zoom_direction_negative_y;
      m_zoomer.m_scale_event.y() = -1.0f;
      m_zoomer.m_translate_event.y() = static_cast<float>(h);
      m_zoomer.transformation(ScaleTranslate<float>(vec2(0.0f, h - m_render_format_size.value())));
    }
  else
    {
      m_zoomer.transformation(ScaleTranslate<float>(vec2(0.0f, m_render_format_size.value())));
    }
}

void
painter_glyph_test::
realize_all_glyphs(GlyphRenderer renderer)
{
  unsigned int num_threads(m_realize_glyphs_thread_count.value());
  simple_time timer;
  std::vector<int> cnts;
  std::vector<Glyph> glyphs;

  std::cout << "Generating " << renderer << " glyphs ..." << std::flush;
  GlyphSetGenerator::generate(num_threads, renderer, m_font, glyphs, m_painter->glyph_cache(), cnts);
  std::cout << "took " << timer.restart()
            << " ms to generate " << glyphs.size()
            << " glyphs of type " << renderer << "\n";
  for(int i = 0; i < num_threads; ++i)
    {
      std::cout << "\tThread #" << i << " generated " << cnts[i] << " glyphs.\n";
    }

  unsigned int pre_num_elements(m_painter->glyph_atlas().data_allocated());

  std::cout << "Uploading to atlas ..." << std::flush;
  for (auto &g : glyphs)
    {
      g.upload_to_atlas();
    }
  std::cout << "took " << timer.restart() << " ms\n";

  unsigned int num_elements(m_painter->glyph_atlas().data_allocated());
  unsigned int num_used(num_elements - pre_num_elements);

  std::cout << "Used additional " << PrintBytes(num_used * sizeof(generic_data))
            << " for glyph data\nTotal bytes glyph data total allocated = "
            << PrintBytes(num_elements * sizeof(generic_data))
            << "\n--------------------------------------\n\n";
}

void
painter_glyph_test::
ready_glyph_data(int w, int h)
{
  std::vector<uint32_t> explicit_glyph_codes(m_explicit_glyph_codes.begin(),
                                             m_explicit_glyph_codes.end());

  m_draws[draw_glyph_distance] = GlyphRenderer(distance_field_glyph);
  m_draws[draw_glyph_restricted_rays] = GlyphRenderer(restricted_rays_glyph);
  m_draws[draw_glyph_banded_rays] = GlyphRenderer(banded_rays_glyph);
  m_draws[draw_glyph_coverage] = GlyphRenderer(m_coverage_pixel_size.value());

  if (m_draw_glyph_set.value())
    {
      for (unsigned int i = 0; i < draw_glyph_auto; ++i)
        {
          realize_all_glyphs(m_draws[i]);
        }
      m_draw_shared.init(m_font, w,
                         m_painter->glyph_cache(), m_font_database,
                         m_render_format_size.value(),
                         m_screen_orientation.value());
    }
  else if (!explicit_glyph_codes.empty())
    {
      m_draw_shared.init(explicit_glyph_codes,
                         m_font, m_painter->glyph_cache(),
                         m_render_format_size.value(),
                         m_screen_orientation.value());
    }
  else if (m_use_file.value())
    {
      std::ifstream istr(m_text.value().c_str(), std::ios::binary);
      m_draw_shared.init(istr, m_font,
                         m_painter->glyph_cache(), m_font_database,
                         m_render_format_size.value(),
                         m_screen_orientation.value());
    }
  else
    {
      std::istringstream istr(m_text.value());
      m_draw_shared.init(istr, m_font,
                         m_painter->glyph_cache(), m_font_database,
                         m_render_format_size.value(),
                         m_screen_orientation.value());
    }
  m_draw_shared.post_finalize();

  for (unsigned int i = 0; i < draw_glyph_auto; ++i)
    {
      simple_time timer;
      unsigned int pre_num_elements(m_painter->glyph_atlas().data_allocated());

      std::cout << "Generating, realizing and uploading to atlas glyphs of type " << m_draws[i] << "...";
      m_draw_shared.realize_glyphs(m_draws[i]);

      if (!m_draw_glyph_set.value())
        {
          unsigned int num_elements(m_painter->glyph_atlas().data_allocated());
          unsigned int num_used(num_elements - pre_num_elements);
          std::cout << "Used " << PrintBytes(num_used * sizeof(generic_data))
                    << " glyph data, total used = "
                    << PrintBytes(num_elements * sizeof(generic_data)) << ", ";
        }
      std::cout << "took " << timer.restart() << "ms.\n" << std::flush;
    }

  float sz;
  sz = GlyphGenerateParams::restricted_rays_minimum_render_size();
  m_restricted_rays_box_slack = m_render_format_size.value() / sz;
}

vec2
painter_glyph_test::
item_coordinates(ivec2 scr)
{
  vec2 p(scr);

  p = m_zoomer.transformation().apply_inverse_to_point(p);
  p /= m_shear;

  float s, c, a;
  float2x2 tr;
  a = -m_angle * FASTUIDRAW_PI / 180.0f;
  s = t_sin(a);
  c = t_cos(a);

  tr(0, 0) = c;
  tr(1, 0) = s;
  tr(0, 1) = -s;
  tr(1, 1) = c;

  p = tr * p;
  p /= m_shear2;

  return p;
}

void
painter_glyph_test::
stroke_glyph(const PainterData &d, GlyphMetrics M, GlyphRenderer R)
{
  Glyph G;

  FASTUIDRAWassert(R.valid());
  G = m_painter->glyph_cache().fetch_glyph(R, M.font().get(), M.glyph_code());
  m_painter->stroke_path(d, G.path(),
                         StrokingStyle()
                         .join_style(static_cast<enum Painter::join_style>(m_join_style)),
                         m_anti_alias_path_stroking, m_stroking_method);
}

void
painter_glyph_test::
fill_glyph(const PainterData &d, GlyphMetrics M, GlyphRenderer R)
{
  Glyph G;

  FASTUIDRAWassert(R.valid());
  G = m_painter->glyph_cache().fetch_glyph(R, M.font().get(), M.glyph_code());
  m_painter->fill_path(d, G.path(),
                       Painter::nonzero_fill_rule,
                       m_anti_alias_path_filling);
}

void
painter_glyph_test::
draw_frame(void)
{
  float us;
  us = update_cts_params();

  m_surface->clear_color(vec4(m_bg_red.value(),
                              m_bg_green.value(),
                              m_bg_blue.value(),
                              1.0f));
  draw_glyphs(us);
  m_draw_times.advance();

  fastuidraw_glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
  fastuidraw_glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
  m_surface->blit_surface(GL_NEAREST);
}

void
painter_glyph_test::
draw_glyphs(float us)
{
  std::vector<unsigned int> glyphs_visible;
  GlyphRenderer render;
  PainterBrush glyph_brush;

  glyph_brush.color(m_fg_red.value(), m_fg_green.value(),
                  m_fg_blue.value(), 1.0f);

  m_painter->begin(m_surface, m_screen_orientation.value());
  m_painter->save();
  m_zoomer.transformation().concat_to_painter(m_painter);
  m_painter->shear(m_shear.x(), m_shear.y());
  m_painter->rotate(m_angle * FASTUIDRAW_PI / 180.0f);
  m_painter->shear(m_shear2.x(), m_shear2.y());

  if (m_fill_glyphs || m_stroke_glyphs || m_draw_path_pts)
    {
      vec2 p0, p1, p2, p3;
      BoundingBox<float> screen;
      ivec2 dims(dimensions());

      p0 = item_coordinates(ivec2(0, 0));
      p1 = item_coordinates(dims);
      p2 = item_coordinates(ivec2(0, dims.y()));
      p3 = item_coordinates(ivec2(dims.x(), dims.y()));

      screen
        .union_point(p0)
        .union_point(p1)
        .union_point(p2)
        .union_point(p3);

      m_draw_shared.query_glyph_interesection(screen, &glyphs_visible);
    }

  if (!m_fill_glyphs)
    {
      render = m_draw_shared.draw_glyphs(m_draws[m_current_drawer], m_painter,
                                         PainterData(&glyph_brush));
    }
  else
    {
      render = m_painter->compute_glyph_renderer(m_draw_shared.glyph_sequence().format_size());
    }

  if (m_fill_glyphs)
    {
      unsigned int src(m_current_drawer);

      // reuse brush parameters across all glyphs
      PainterData::brush_value pbr;
      pbr = m_painter->packed_value_pool().create_packed_brush(glyph_brush);

      for(unsigned int i : glyphs_visible)
        {
          GlyphMetrics metrics;
          vec2 position;

          m_draw_shared.glyph_sequence().added_glyph(i, &metrics, &position);
          if (metrics.valid())
            {
              m_painter->save();
              m_painter->translate(position);

              //make the scale of the path match how we scaled the text.
              float sc, ysign;
              sc = m_render_format_size.value() / metrics.units_per_EM();

              /* when drawing with y-coordinate increasing downwards
               * which is the opposite coordinate system as the glyph's
               * path, thus we also need to negate in the y-direction.
               */
              ysign = (m_screen_orientation.value() == Painter::y_increases_upwards) ? 1.0f : -1.0f;
              m_painter->shear(sc, sc * ysign);
              fill_glyph(PainterData(pbr), metrics, render);
              m_painter->restore();
            }
        }
    }

  if (m_stroke_glyphs)
    {
      unsigned int src;
      PainterBrush stroke_brush;
      stroke_brush.color(1.0, 0.0, 0.0, 0.8);

      PainterStrokeParams st;
      st.miter_limit(5.0f);
      st.width(m_stroke_width);
      if (m_pixel_width_stroking)
        {
          st.stroking_units(PainterStrokeParams::pixel_stroking_units);
        }

      src = m_current_drawer;

      // reuse stroke and brush parameters across all glyphs
      PainterData::brush_value pbr;
      pbr = m_painter->packed_value_pool().create_packed_brush(stroke_brush);

      PainterPackedValue<PainterItemShaderData> pst;
      pst = m_painter->packed_value_pool().create_packed_value(st);

      for(unsigned int i : glyphs_visible)
        {
          GlyphMetrics metrics;
          vec2 position;

          m_draw_shared.glyph_sequence().added_glyph(i, &metrics, &position);
          if (metrics.valid())
            {
              m_painter->save();
              m_painter->translate(position);

              //make the scale of the path match how we scaled the text.
              float sc, ysign;
              sc = m_render_format_size.value() / metrics.units_per_EM();

              /* when drawing with y-coordinate increasing downwards
               * which is the opposite coordinate system as the glyph's
               * path, thus we also need to negate in the y-direction.
               */
              ysign = (m_screen_orientation.value() == Painter::y_increases_upwards) ? 1.0f : -1.0f;
              m_painter->shear(sc, sc * ysign);

              stroke_glyph(PainterData(pst, pbr), metrics, render);
              m_painter->restore();
            }
        }
    }

  if (m_draw_path_pts)
    {
      vecN<PainterBrush, 3> brs;
      vecN<PainterData::brush_value, 3 > pbrs;

      brs[0].color(1.0f, 0.0f, 0.0f, 0.5f);
      brs[1].color(0.0f, 1.0f, 0.0f, 0.5f);
      brs[2].color(0.0f, 0.0f, 1.0f, 0.5f);
      for (int i = 0; i < 3; ++i)
        {
          pbrs[i] = m_painter->packed_value_pool().create_packed_brush(brs[i]);
        }

      for(unsigned int i : glyphs_visible)
        {
          GlyphMetrics metrics;
          vec2 position;
          float inv_scale;

          inv_scale = 1.0f / m_zoomer.transformation().scale();

          m_draw_shared.glyph_sequence().added_glyph(i, &metrics, &position);
          if (metrics.valid())
            {
              std::vector<vec2> pts, ctl_pts, arc_center_pts;
              std::string descr;
              Glyph G;
              Rect R;
              vec2 sz_bb, r;
              float rad;

              G = m_painter->glyph_cache().fetch_glyph(render, metrics.font().get(), metrics.glyph_code());
              extract_path_info(G.path(), &pts, &ctl_pts, &arc_center_pts, &descr);
              G.path().approximate_bounding_box(&R);
              sz_bb = R.size();

              m_painter->save();
              m_painter->translate(position);

              //make the scale of the path match how we scaled the text.
              float sc, ysign;
              Rect rect;
              sc = m_render_format_size.value() / metrics.units_per_EM();


              /* when drawing with y-coordinate increasing downwards
               * which is the opposite coordinate system as the glyph's
               * path, thus we also need to negate in the y-direction.
               */
              ysign = (m_screen_orientation.value() == Painter::y_increases_upwards) ? 1.0f : -1.0f;
              m_painter->shear(sc, sc * ysign);

              rad = 8.0f * inv_scale / sc;
              rad = t_min(rad, 0.02f * t_max(sz_bb.x(), sz_bb.y()));
              r = vec2(rad, rad);
              for (const vec2 &pt : pts)
                {
                  rect
                    .min_point(pt - r)
                    .max_point(pt + r);
                  m_painter->fill_rect(PainterData(pbrs[2]), rect);
                }

              for (const vec2 &pt : ctl_pts)
                {
                  rect
                    .min_point(pt - r)
                    .max_point(pt + r);
                  m_painter->fill_rect(PainterData(pbrs[0]), rect);
                }

              for (const vec2 &pt : arc_center_pts)
                {
                  rect
                    .min_point(pt - r)
                    .max_point(pt + r);
                  m_painter->fill_rect(PainterData(pbrs[1]), rect);
                }

              m_painter->restore();
            }
        }
    }

  if (m_draw_restricted_rays_box_slack && m_restricted_rays_box_slack > 0.0f)
    {
      PainterBrush brush;
      vec2 p;
      ivec2 mouse_position;

      SDL_GetMouseState(&mouse_position.x(), &mouse_position.y());
      if (m_screen_orientation.value() == Painter::y_increases_upwards)
        {
          mouse_position.y() = dimensions().y() - mouse_position.y();
        }

      p = item_coordinates(mouse_position);
      brush.color(1.0f, 1.0f, 0.0f, 0.3f);
      m_painter->fill_rect(PainterData(&brush),
                           Rect()
                           .min_point(p)
                           .size(m_restricted_rays_box_slack, m_restricted_rays_box_slack));
    }

  if (m_draw_stats)
    {
      std::ostringstream ostr;
      uint32_t glyph_atlas_size_bytes, glyph_atlas_size_kb(0u), glyph_atlas_size_mb(0u);

      glyph_atlas_size_bytes = 4u * m_painter->glyph_atlas().data_allocated();
      if (glyph_atlas_size_bytes > 1000u)
        {
          glyph_atlas_size_kb = glyph_atlas_size_bytes / 1000u;
          glyph_atlas_size_bytes %= 1000u;
        }
      if (glyph_atlas_size_kb > 1000u)
        {
          glyph_atlas_size_mb = glyph_atlas_size_kb / 1000u;
          glyph_atlas_size_kb %= 1000u;
        }

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
      ostr << "\nms = " << us / 1000.0f;

      float avg_us;
      int num_frames;

      avg_us = static_cast<float>(m_draw_times.oldest_elapsed_us(&num_frames));
      if (num_frames > 0)
        {
          avg_us /= static_cast<float>(num_frames);
          ostr << "\nFPS(over " << num_frames << " frames) = ";
          if (avg_us > 0.0f)
            {
              ostr << 1000.0f * 1000.0f / avg_us;
            }
          else
            {
              ostr << "NAN";
            }
          ostr << "\nms(over " << num_frames << " frames) = " << avg_us / 1000.0f;
        }

      if (m_fill_glyphs)
        {
          ostr << "\nFilling Glyphs (anti-aliasing = "
               << on_off(m_anti_alias_path_filling) << ")";
        }
      else
        {
          ostr << "\n" << m_draws[m_current_drawer] << " glyphs";
        }

      if (m_stroke_glyphs)
        {
          ostr << "\nStroking Glyphs\n\tanti-aliasing = "
               << on_off(m_anti_alias_path_stroking)
               << "\n\tstroking_method = "
               << on_off(m_stroking_method)
               << "\n\tpixel_width_stroking = "
               << on_off(m_pixel_width_stroking);
        }

      fastuidraw::c_array<const unsigned int> stats(painter_stats());
      for (unsigned int i = 0; i < stats.size(); ++i)
        {
          enum Painter::query_stats_t st;

          st = static_cast<enum Painter::query_stats_t>(i);
          ostr << "\n" << Painter::label(st) << ": " << stats[i];
        }
      ostr << "\nGlyph Atlas size: ";
      if (glyph_atlas_size_mb > 0u)
        {
          ostr << glyph_atlas_size_mb << ".";
        }
      if (glyph_atlas_size_kb > 0u)
        {
          ostr << std::setfill('0') << std::setw(3) << glyph_atlas_size_kb << ".";
        }
      ostr << std::setfill('0') << std::setw(3) << glyph_atlas_size_bytes << " Bytes";

      m_painter->restore();
      if (m_screen_orientation.value() == Painter::y_increases_upwards)
        {
          m_painter->translate(vec2(0.0f, dimensions().y()));
        }

      PainterBrush brush;

      brush.color(0.0f, 1.0f, 1.0f, 1.0f);
      draw_text(ostr.str(), 32.0f, m_default_font.get(), GlyphRenderer(restricted_rays_glyph),
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
      if (m_screen_orientation.value() == Painter::y_increases_upwards)
        {
          mouse_position.y() = dimensions().y() - mouse_position.y();
        }
      p = item_coordinates(mouse_position);
      G = m_draw_shared.query_glyph_at(p, &glyph_bb);
      if (G != GenericHierarchy::not_found)
        {
          GlyphMetrics metrics;
          float ratio, ysign;
          vec2 glyph_position, glyph_coord;

          m_draw_shared.glyph_sequence().added_glyph(G, &metrics, &glyph_position);
          ratio = m_render_format_size.value() / metrics.units_per_EM();

          ysign = (m_screen_orientation.value() == Painter::y_increases_upwards) ? 1.0f : -1.0f;
          glyph_coord = (p - glyph_position) / vec2(ratio, ratio * ysign);

          /* start with an eol so that top line is visible */
          ostr << "\nGlyph at " << p << " is:"
               << "\n\tglyph_code: " << metrics.glyph_code()
               << "\n\tfont: " << metrics.font()->properties()
               << "\n\trenderer: ";

          if (!m_fill_glyphs)
            {
              Glyph glyph;
              c_array<const GlyphRenderCostInfo> render_costs;

              glyph = m_painter->glyph_cache().fetch_glyph(render, metrics.font().get(), metrics.glyph_code());
              render_costs = glyph.render_cost();
              ostr << render;

              for (const GlyphRenderCostInfo &info : render_costs)
                {
                  ostr << "\n\tRenderCost(" << info.m_label << "): " << info.m_value;
                }
            }
          else
            {
              ostr << " path-fill";
            }

          ostr << "\n\tglyph_coord: " << glyph_coord
               << "\n\tunits_per_EM: " << metrics.units_per_EM()
               << "\n\tsize in EM: " << metrics.size()
               << "\n\tsize normalized: " << metrics.size() * ratio
               << "\n\tHorizontal Offset = " << metrics.horizontal_layout_offset();

          /* draw a box around the glyph(!).*/
          PainterBrush brush;

          brush.color(1.0f, 0.0f, 0.0f, 0.3f);
          m_painter->fill_rect(PainterData(&brush),
                               Rect()
                               .min_point(glyph_bb.min_point())
                               .max_point(glyph_bb.max_point()));
        }
      else
        {
          ostr << "\nNo glyph at " << p << "\n";
        }

      m_painter->restore();
      if (m_screen_orientation.value() == Painter::y_increases_upwards)
        {
          m_painter->translate(vec2(0.0f, dimensions().y()));
        }

      PainterBrush brush;

      brush.color(0.0f, 1.0f, 1.0f, 1.0f);
      draw_text(ostr.str(), 32.0f, m_default_font.get(), GlyphRenderer(distance_field_glyph),
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

  float speed, speed_shear;
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

  speed_shear = 0.05f * speed;
  if (keyboard_state[SDL_SCANCODE_LCTRL] || keyboard_state[SDL_SCANCODE_RCTRL])
    {
      speed_shear = -speed_shear;
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
  /**/
  vec2 *pshear(&m_shear);
  c_string shear_txt = "";
  if (keyboard_state[SDL_SCANCODE_RETURN])
    {
      pshear = &m_shear2;
      shear_txt = "2";
    }

  if (keyboard_state[SDL_SCANCODE_6])
    {
      pshear->x() += speed_shear;
      std::cout << "Shear" << shear_txt << " set to: " << *pshear << "\n";
    }
  if (keyboard_state[SDL_SCANCODE_7])
    {
      pshear->y() += speed_shear;
      std::cout << "Shear " << shear_txt << " set to: " << *pshear << "\n";
    }

  if (keyboard_state[SDL_SCANCODE_9])
    {
      m_angle += speed * 0.5f;
      std::cout << "Angle set to: " << m_angle << "\n";
    }
  if (keyboard_state[SDL_SCANCODE_0])
    {
      m_angle -= speed * 0.5f;
      std::cout << "Angle set to: " << m_angle << "\n";
    }
  /**/

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
      if (ev.window.event == SDL_WINDOWEVENT_RESIZED)
        {
          on_resize(ev.window.data1, ev.window.data2);
          if (m_screen_orientation.value() == Painter::y_increases_upwards)
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

        case SDLK_d:
          cycle_value(m_current_drawer, ev.key.keysym.mod & (KMOD_SHIFT|KMOD_CTRL|KMOD_ALT), m_draws.size());
          std::cout << "Drawing " << m_draws[m_current_drawer] << " glyphs\n";
          break;

        case SDLK_c:
          std::cout << "Clear Cache\n";
          m_painter->glyph_cache().clear_atlas();
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
              cycle_value(m_join_style, ev.key.keysym.mod & (KMOD_SHIFT|KMOD_CTRL|KMOD_ALT), Painter::number_join_styles);
              std::cout << "Join drawing mode set to: " << Painter::label(m_join_style) << "\n";
            }
          break;

        case SDLK_w:
          if (m_stroke_glyphs)
            {
              m_anti_alias_path_stroking = !m_anti_alias_path_stroking;
              std::cout << "Anti-aliasing of path stroking set to "
                        << on_off(m_anti_alias_path_stroking)
                        << "\n";
            }
          break;

        case SDLK_x:
          if (m_stroke_glyphs)
            {
              int v(m_stroking_method);

              cycle_value(v, ev.key.keysym.mod & (KMOD_SHIFT | KMOD_ALT),
                          Painter::number_stroking_methods);
              m_stroking_method = static_cast<enum Painter::stroking_method_t>(v);
              std::cout << "Anti-aliasing of path stroking set to "
                        << Painter::label(m_stroking_method)
                        << "\n";
            }
          break;

        case SDLK_p:
          if (ev.key.keysym.mod & KMOD_CTRL)
            {
              m_draw_path_pts = !m_draw_path_pts;
              if (m_draw_path_pts)
                {
                  std::cout << "Draw Path Points\n";
                }
              else
                {
                  std::cout << "Do not draw Path Points\n";
                }
            }
          else if (m_stroke_glyphs)
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
          if (ev.key.keysym.mod & KMOD_CTRL)
            {
              m_draw_restricted_rays_box_slack = !m_draw_restricted_rays_box_slack;
            }
          else
            {
              m_draw_stats = !m_draw_stats;
            }
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
              std::cout << "Anti-aliasing of path fill set to "
                        << on_off(m_anti_alias_path_filling)
                        << "\n";
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
