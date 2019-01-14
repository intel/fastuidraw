#include <string>
#include <algorithm>
#include <mutex>
#include <sstream>
#include <map>
#include <dirent.h>

#ifdef HAVE_FONT_CONFIG
#include <fontconfig/fontconfig.h>
#endif

#include "text_helper.hpp"
#include "cast_c_array.hpp"

namespace
{
  #ifdef HAVE_FONT_CONFIG
  class FontConfig
  {
  public:
    static
    FcConfig*
    get(void)
    {
      static FontConfig R;
      return R.m_fc;
    }

    static
    std::string
    get_string(FcPattern *pattern, const char *label, std::string default_value = std::string())
    {
      FcChar8 *value(nullptr);
      if (FcPatternGetString(pattern, label, 0, &value) == FcResultMatch)
        {
          return std::string((const char*)value);
        }
      else
        {
          return default_value;
        }
    }

    static
    int
    get_int(FcPattern *pattern, const char *label, int default_value = 0)
    {
      int value(0);
      if (FcPatternGetInteger(pattern, label, 0, &value) == FcResultMatch)
        {
          return value;
        }
      else
        {
          return default_value;
        }
    }

    static
    bool
    get_bool(FcPattern *pattern, const char *label, bool default_value = false)
    {
      FcBool value(0);
      if (FcPatternGetBool(pattern, label, 0, &value) == FcResultMatch)
        {
          return value;
        }
      else
        {
          return default_value;
        }
    }

    static
    fastuidraw::FontProperties
    get_font_properties(FcPattern *pattern)
    {
      return fastuidraw::FontProperties()
        .style(get_string(pattern, FC_STYLE).c_str())
        .family(get_string(pattern, FC_FAMILY).c_str())
        .foundry(get_string(pattern, FC_FOUNDRY).c_str())
        .source_label(get_string(pattern, FC_FILE).c_str(),
                      get_int(pattern, FC_INDEX))
        .bold(get_int(pattern, FC_WEIGHT) >= FC_WEIGHT_BOLD)
        .italic(get_int(pattern, FC_SLANT) >= FC_SLANT_ITALIC);
    }

  private:
    FontConfig(void)
    {
      m_fc = FcInitLoadConfigAndFonts();
    }

    ~FontConfig(void)
    {
      FcConfigDestroy(m_fc);
    }

    FcConfig* m_fc;
  };
  #endif
  /* The purpose of the DaaBufferHolder is to -DELAY-
   * the loading of data until the first time the data
   * is requested.
   */
  class DataBufferLoader:public fastuidraw::reference_counted<DataBufferLoader>::default_base
  {
  public:
    explicit
    DataBufferLoader(const std::string &pfilename):
      m_filename(pfilename)
    {}

    fastuidraw::reference_counted_ptr<fastuidraw::DataBufferBase>
    buffer(void)
    {
      fastuidraw::reference_counted_ptr<fastuidraw::DataBufferBase> R;

      m_mutex.lock();
      if (!m_buffer)
        {
          m_buffer = FASTUIDRAWnew fastuidraw::DataBuffer(m_filename.c_str());
        }
      R = m_buffer;
      m_mutex.unlock();

      return R;
    }

  private:
    std::string m_filename;
    std::mutex m_mutex;
    fastuidraw::reference_counted_ptr<fastuidraw::DataBufferBase> m_buffer;
  };

  class FreeTypeFontGenerator:public fastuidraw::FontDatabase::FontGeneratorBase
  {
  public:
    FreeTypeFontGenerator(fastuidraw::reference_counted_ptr<DataBufferLoader> buffer,
                          fastuidraw::reference_counted_ptr<fastuidraw::FreeTypeLib> lib,
                          int face_index,
                          const fastuidraw::FontProperties &props):
      m_buffer(buffer),
      m_lib(lib),
      m_face_index(face_index),
      m_props(props)
    {}

    virtual
    fastuidraw::reference_counted_ptr<const fastuidraw::FontBase>
    generate_font(void) const
    {
      fastuidraw::reference_counted_ptr<fastuidraw::FreeTypeFace::GeneratorBase> h;
      fastuidraw::reference_counted_ptr<fastuidraw::DataBufferBase> buffer;
      fastuidraw::reference_counted_ptr<const fastuidraw::FontBase> font;
      buffer = m_buffer->buffer();
      h = FASTUIDRAWnew fastuidraw::FreeTypeFace::GeneratorMemory(buffer, m_face_index);
      font = FASTUIDRAWnew fastuidraw::FontFreeType(h, m_props, m_lib);
      return font;
    }

    virtual
    const fastuidraw::FontProperties&
    font_properties(void) const
    {
      return m_props;
    }

  private:
    fastuidraw::reference_counted_ptr<DataBufferLoader> m_buffer;
    fastuidraw::reference_counted_ptr<fastuidraw::FreeTypeLib> m_lib;
    int m_face_index;
    fastuidraw::FontProperties m_props;
  };

  void
  preprocess_text(std::string &text)
  {
    /* we want to change '\t' into 4 spaces
     */
    std::string v;
    v.reserve(text.size() + 4 * std::count(text.begin(), text.end(), '\t'));
    for(std::string::const_iterator iter = text.begin(); iter != text.end(); ++iter)
      {
        if (*iter != '\t')
          {
            v.push_back(*iter);
          }
        else
          {
            v.push_back(' ');
          }
      }
    text.swap(v);
  }

  void
  add_fonts_from_file(const std::string &filename,
                      fastuidraw::reference_counted_ptr<fastuidraw::FreeTypeLib> lib,
                      fastuidraw::reference_counted_ptr<fastuidraw::FontDatabase> font_database)
  {
    FT_Error error_code;
    FT_Face face(nullptr);

    lib->lock();
    error_code = FT_New_Face(lib->lib(), filename.c_str(), 0, &face);
    lib->unlock();

    if (error_code == 0 && face != nullptr && (face->face_flags & FT_FACE_FLAG_SCALABLE) != 0)
      {
        fastuidraw::reference_counted_ptr<DataBufferLoader> buffer_loader;

        buffer_loader = FASTUIDRAWnew DataBufferLoader(filename);
        for(unsigned int i = 0, endi = face->num_faces; i < endi; ++i)
          {
            fastuidraw::reference_counted_ptr<const fastuidraw::FontDatabase::FontGeneratorBase> h;
            enum fastuidraw::return_code R;
            fastuidraw::FontProperties props;
            if (i != 0)
              {
                lib->lock();
                FT_Done_Face(face);
                FT_New_Face(lib->lib(), filename.c_str(), i, &face);
                lib->unlock();
              }
            fastuidraw::FontFreeType::compute_font_properties_from_face(face, props);
            props.source_label(filename.c_str(), i);

            h = FASTUIDRAWnew FreeTypeFontGenerator(buffer_loader, lib, i, props);
            R = font_database->add_font_generator(h);

            if (R != fastuidraw::routine_success)
              {
                std::cout << "Vanilla warning: unable to add font " << h->font_properties()
                          << " because it was already marked as added\n";
              }
            else
              {
                // std::cout << "Vanilla add font: " << props << "\n";
              }
          }
      }

    lib->lock();
    if (face != nullptr)
      {
        FT_Done_Face(face);
      }
    lib->unlock();
  }

}

/////////////////////////////
// GlyphSetGenerator methods
GlyphSetGenerator::
GlyphSetGenerator(fastuidraw::GlyphRenderer r,
                  fastuidraw::reference_counted_ptr<const fastuidraw::FontBase> f,
                  std::vector<fastuidraw::Glyph> &dst):
  m_render(r),
  m_font(f)
{
  dst.resize(f->number_glyphs());
  m_dst = fastuidraw::c_array<fastuidraw::Glyph>(&dst[0], dst.size());
  SDL_AtomicSet(&m_counter, 0);
}

int
GlyphSetGenerator::
execute(void *ptr)
{
  unsigned int idx, K;
  GlyphSetGenerator *p(static_cast<GlyphSetGenerator*>(ptr));

  for(idx = SDL_AtomicAdd(&p->m_counter, 1), K = 0;
      idx < p->m_dst.size();
      idx = SDL_AtomicAdd(&p->m_counter, 1), ++K)
    {
      p->m_dst[idx] = fastuidraw::Glyph::create_glyph(p->m_render, p->m_font, idx);
    }
  return K;
}

void
GlyphSetGenerator::
generate(unsigned int num_threads,
         fastuidraw::GlyphRenderer r,
         fastuidraw::reference_counted_ptr<const fastuidraw::FontBase> f,
         std::vector<fastuidraw::Glyph> &dst,
         fastuidraw::reference_counted_ptr<fastuidraw::GlyphCache> glyph_cache,
         std::vector<int> &cnts)
{
  GlyphSetGenerator generator(r, f, dst);
  std::vector<SDL_Thread*> threads;

  cnts.clear();
  cnts.resize(fastuidraw::t_max(1u, num_threads), 0);

  if (num_threads < 2)
    {
      cnts[0] = execute(&generator);
    }
  else
    {
      for(int i = 0; i < num_threads; ++i)
        {
          threads.push_back(SDL_CreateThread(execute, "", &generator));
        }

      for(int i = 0; i < num_threads; ++i)
        {
          SDL_WaitThread(threads[i], &cnts[i]);
        }
    }

  if (glyph_cache)
    {
      for(fastuidraw::Glyph glyph : dst)
        {
          if (glyph.valid())
            {
              enum fastuidraw::return_code R;

              R = glyph_cache->add_glyph(glyph, false);
              FASTUIDRAWassert(R == fastuidraw::routine_success);
              FASTUIDRAWunused(R);
            }
        }
    }
}

//////////////////////////////
// global methods
void
create_formatted_text(fastuidraw::GlyphSequence &out_sequence,
                      const std::vector<uint32_t> &glyph_codes,
                      const fastuidraw::FontBase *font,
                      const fastuidraw::vec2 &shift_by)
{
  fastuidraw::vec2 pen(shift_by);
  float pixel_size(out_sequence.pixel_size());
  enum fastuidraw::Painter::screen_orientation orientation(out_sequence.orientation());
  fastuidraw::GlyphMetrics layout;
  float ratio;

  for(uint32_t glyph_code : glyph_codes)
    {
      layout = out_sequence.glyph_cache()->fetch_glyph_metrics(font, glyph_code);
      out_sequence.add_glyph(fastuidraw::GlyphSource(glyph_code, font), pen);

      ratio = pixel_size / layout.units_per_EM();
      pen.x() += ratio * layout.advance().x();
    }
}

template<typename T>
static
void
create_formatted_textT(T &out_sequence,
                       std::istream &istr,
                       const fastuidraw::FontBase *font,
                       fastuidraw::reference_counted_ptr<fastuidraw::FontDatabase> font_database,
                       const fastuidraw::vec2 &starting_place)
{
  std::streampos current_position, end_position;
  float pixel_size(out_sequence.pixel_size());
  enum fastuidraw::Painter::screen_orientation orientation(out_sequence.orientation());
  unsigned int loc(0);
  fastuidraw::vec2 pen(starting_place);
  std::string line, original_line;
  float last_negative_tallest(0.0f);
  bool first_line(true);

  current_position = istr.tellg();
  istr.seekg(0, std::ios::end);
  end_position = istr.tellg();
  istr.seekg(current_position, std::ios::beg);

  std::vector<fastuidraw::GlyphSource> glyph_sources;
  std::vector<fastuidraw::vec2> sub_p;
  std::vector<fastuidraw::GlyphMetrics> metrics;

  while(getline(istr, line))
    {
      fastuidraw::c_array<uint32_t> sub_ch;
      fastuidraw::c_array<fastuidraw::range_type<float> > sub_extents;
      float tallest, negative_tallest, offset;
      bool empty_line;
      float pen_y_advance;

      empty_line = true;
      tallest = 0.0f;
      negative_tallest = 0.0f;

      original_line = line;
      preprocess_text(line);

      sub_p.resize(line.length());
      glyph_sources.resize(line.length());
      metrics.resize(line.length());

      font_database->create_glyph_sequence(font, line.begin(), line.end(), glyph_sources.begin());
      out_sequence.glyph_cache()->fetch_glyph_metrics(cast_c_array(glyph_sources), cast_c_array(metrics));
      for(unsigned int i = 0, endi = glyph_sources.size(); i < endi; ++i)
        {
          sub_p[i] = pen;
          if (glyph_sources[i].m_font)
            {
              float ratio;

              ratio = pixel_size / metrics[i].units_per_EM();

              empty_line = false;
              pen.x() += ratio * metrics[i].advance().x();

              tallest = std::max(tallest, ratio * (metrics[i].horizontal_layout_offset().y() + metrics[i].size().y()));
              negative_tallest = std::min(negative_tallest, ratio * metrics[i].horizontal_layout_offset().y());
            }
        }

      if (empty_line)
        {
          pen_y_advance = pixel_size + 1.0f;
          offset = 0.0f;
        }
      else
        {
          if (orientation == fastuidraw::Painter::y_increases_downwards)
            {
              float v;

              v = tallest - last_negative_tallest;
              offset = (first_line) ? 0 : v;
              pen_y_advance = (first_line) ? 0 : v;
            }
          else
            {
              pen_y_advance = tallest - negative_tallest;
              offset = (first_line) ? 0 : -negative_tallest;
            }
        }

      for(unsigned int i = 0; i < sub_p.size(); ++i)
        {
          sub_p[i].y() += offset;
        }

      if (orientation == fastuidraw::Painter::y_increases_downwards)
        {
          pen.y() += pen_y_advance + 1.0f;
        }
      else
        {
          pen.y() -= pen_y_advance + 1.0f;
        }

      pen.x() = starting_place.x();
      loc += line.length();
      last_negative_tallest = negative_tallest;
      first_line = false;

      out_sequence.add_glyphs(const_cast_c_array(glyph_sources),
                              const_cast_c_array(sub_p));
    }
}

void
create_formatted_text(fastuidraw::GlyphSequence &out_sequence,
                      std::istream &istr,
                      const fastuidraw::FontBase *font,
                      fastuidraw::reference_counted_ptr<fastuidraw::FontDatabase> font_database,
                      const fastuidraw::vec2 &starting_place)
{
  create_formatted_textT(out_sequence, istr, font, font_database, starting_place);
}

void
create_formatted_text(fastuidraw::GlyphRun &out_sequence,
                      std::istream &istr,
                      const fastuidraw::FontBase *font,
                      fastuidraw::reference_counted_ptr<fastuidraw::FontDatabase> font_database,
                      const fastuidraw::vec2 &starting_place)
{
  create_formatted_textT(out_sequence, istr, font, font_database, starting_place);
}

void
add_fonts_from_path(const std::string &filename,
                    fastuidraw::reference_counted_ptr<fastuidraw::FreeTypeLib> lib,
                    fastuidraw::reference_counted_ptr<fastuidraw::FontDatabase> font_database)
{
  DIR *dir;
  struct dirent *entry;

  dir = opendir(filename.c_str());
  if (!dir)
    {
      add_fonts_from_file(filename, lib, font_database);
      return;
    }

  for(entry = readdir(dir); entry != nullptr; entry = readdir(dir))
    {
      std::string file;
      file = entry->d_name;
      if (file != ".." && file != ".")
        {
          if (filename.empty() || filename.back() != '/')
            {
              add_fonts_from_path(filename + "/" + file, lib, font_database);
            }
          else
            {
              add_fonts_from_path(filename + file, lib, font_database);
            }
        }
    }
  closedir(dir);
}

fastuidraw::c_string
default_font(void)
{
  #ifdef _WIN32
    {
      return "C:/Windows/Fonts/arial.ttf";
    }
  #else
    {
      return "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf";
    }
  #endif
}

fastuidraw::c_string
default_font_path(void)
{
  #ifdef _WIN32
    {
      return "C:/Windows/Fonts";
    }
  #else
    {
      return "/usr/share/fonts/";
    }
  #endif
}

void
add_fonts_from_font_config(fastuidraw::reference_counted_ptr<fastuidraw::FreeTypeLib> lib,
                           fastuidraw::reference_counted_ptr<fastuidraw::FontDatabase> font_database)
{
  #ifdef HAVE_FONT_CONFIG
    {
      FcConfig *config = FontConfig::get();
      FcObjectSet *object_set;
      FcFontSet *font_set;
      FcPattern* pattern;
      std::map<std::string, fastuidraw::reference_counted_ptr<DataBufferLoader> > buffer_loaders;

      object_set = FcObjectSetBuild(FC_FOUNDRY, FC_FAMILY, FC_STYLE, FC_WEIGHT,
                                    FC_SLANT, FC_SCALABLE, FC_FILE, FC_INDEX,
                                    nullptr);
      pattern = FcPatternCreate();
      FcPatternAddBool(pattern, FC_SCALABLE, FcTrue);
      font_set = FcFontList(config, pattern, object_set);

      for (int i = 0; i < font_set->nfont; ++i)
        {
          std::string filename;

          filename = FontConfig::get_string(font_set->fonts[i], FC_FILE);
          if (filename != "")
            {
              fastuidraw::reference_counted_ptr<DataBufferLoader> b;
              fastuidraw::reference_counted_ptr<const fastuidraw::FontDatabase::FontGeneratorBase> g;
              std::map<std::string, fastuidraw::reference_counted_ptr<DataBufferLoader> >::const_iterator iter;
              int face_index;
              enum fastuidraw::return_code R;
              fastuidraw::FontProperties props(FontConfig::get_font_properties(font_set->fonts[i]));

              iter = buffer_loaders.find(filename);
              if (iter == buffer_loaders.end())
                {
                  b = FASTUIDRAWnew DataBufferLoader(filename);
                  buffer_loaders[filename] = b;
                }
              else
                {
                  b = iter->second;
                }

              face_index = FontConfig::get_int(font_set->fonts[i], FC_INDEX);
              g = FASTUIDRAWnew FreeTypeFontGenerator(b, lib, face_index, props);

              R = font_database->add_font_generator(g);
              if (R != fastuidraw::routine_success)
                {
                  std::cout << "FontConfig Warning: unable to add font " << props
                            << " because it was already marked as added\n";
                }
              else
                {
                  // std::cout << "FontConfig add font: " << props << "\n";
                }
            }
        }
      FcFontSetDestroy(font_set);
      FcPatternDestroy(pattern);
      FcObjectSetDestroy(object_set);
    }
  #endif
}

fastuidraw::reference_counted_ptr<const fastuidraw::FontBase>
select_font_font_config(int weight, int slant,
                        fastuidraw::c_string style,
                        fastuidraw::c_string family,
                        fastuidraw::c_string foundry,
                        fastuidraw::reference_counted_ptr<fastuidraw::FreeTypeLib> lib,
                        fastuidraw::reference_counted_ptr<fastuidraw::FontDatabase> font_database)
{
  #ifdef HAVE_FONT_CONFIG
    {
      FcConfig *config = FontConfig::get();
      FcPattern* pattern;

      pattern = FcPatternCreate();
      if (weight >= 0)
        {
          FcPatternAddInteger(pattern, FC_WEIGHT, weight);
        }

      if (slant >= 0)
        {
          FcPatternAddInteger(pattern, FC_SLANT, slant);
        }

      if (style)
        {
          FcPatternAddString(pattern, FC_STYLE, (const FcChar8*)style);
        }

      if (family)
        {
          FcPatternAddString(pattern, FC_FAMILY, (const FcChar8*)family);
        }

      if (foundry)
        {
          FcPatternAddString(pattern, FC_FOUNDRY, (const FcChar8*)foundry);
        }
      FcPatternAddBool(pattern, FC_SCALABLE, FcTrue);

      FcConfigSubstitute(config, pattern, FcMatchPattern);
      FcDefaultSubstitute(pattern);

      FcResult r;
      FcPattern *font_pattern = FcFontMatch(config, pattern, &r);
      fastuidraw::reference_counted_ptr<const fastuidraw::FontBase> font;

      if (font_pattern)
        {
          FcChar8* filename(nullptr);
          if (FcPatternGetString(font_pattern, FC_FILE, 0, &filename) == FcResultMatch)
            {
              int face_index;

              face_index = FontConfig::get_int(font_pattern, FC_INDEX);
              font = font_database->fetch_font((fastuidraw::c_string)filename, face_index);
              if (!font)
                {
                  fastuidraw::FontProperties props;
                  fastuidraw::reference_counted_ptr<FreeTypeFontGenerator> gen;
                  fastuidraw::reference_counted_ptr<DataBufferLoader> buffer_loader;

                  props = FontConfig::get_font_properties(font_pattern);
                  buffer_loader = FASTUIDRAWnew DataBufferLoader(std::string((const char*)filename));
                  gen = FASTUIDRAWnew FreeTypeFontGenerator(buffer_loader, lib, face_index, props);

                  font = font_database->fetch_or_generate_font(gen);
                }
            }
          FcPatternDestroy(font_pattern);
        }
      FcPatternDestroy(pattern);
      return font;
    }
  #else
    {
      fastuidraw::FontProperties props;
      uint32_t flags(0u);

      if (foundry)
        {
          props.foundry(foundry);
        }

      if (family)
        {
          props.family(family);
        }

      if (style)
        {
          props.style(style);
        }
      else
        {
          flags |= fastuidraw::FontDatabase::ignore_style;
        }

      if (weight >=0 && slant >= 0)
        {
          props
            .bold(weight >= 200)
            .italic(slant >= 100);
        }
      else
        {
          flags |= fastuidraw::FontDatabase::ignore_bold_italic;
        }

      return font_database->fetch_font(props, flags);
    }
  #endif
}
