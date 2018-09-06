#include <string>
#include <algorithm>
#include <mutex>
#include <sstream>
#include <dirent.h>
#include "text_helper.hpp"

namespace
{
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

  class FreeTypeFontGenerator:public fastuidraw::GlyphSelector::FontGeneratorBase
  {
  public:
    FreeTypeFontGenerator(fastuidraw::reference_counted_ptr<DataBufferLoader> buffer,
                          fastuidraw::reference_counted_ptr<fastuidraw::FreeTypeLib> lib,
                          fastuidraw::FontFreeType::RenderParams render_params,
                          int face_index,
                          const fastuidraw::FontProperties &props):
      m_buffer(buffer),
      m_lib(lib),
      m_render_params(render_params),
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
      font = FASTUIDRAWnew fastuidraw::FontFreeType(h, m_props, m_render_params, m_lib);
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
    fastuidraw::FontFreeType::RenderParams m_render_params;
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
                      fastuidraw::reference_counted_ptr<fastuidraw::GlyphSelector> glyph_selector,
                      fastuidraw::FontFreeType::RenderParams render_params)
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
            fastuidraw::reference_counted_ptr<fastuidraw::GlyphSelector::FontGeneratorBase> h;
            std::ostringstream source_label;
            fastuidraw::FontProperties props;
            if (i != 0)
              {
                lib->lock();
                FT_Done_Face(face);
                FT_New_Face(lib->lib(), filename.c_str(), i, &face);
                lib->unlock();
              }
            fastuidraw::FontFreeType::compute_font_properties_from_face(face, props);
            source_label << filename << ":" << i;
            props.source_label(source_label.str().c_str());

            h = FASTUIDRAWnew FreeTypeFontGenerator(buffer_loader, lib, render_params, i, props);
            glyph_selector->add_font_generator(h);

            //std::cout << "add font: " << props << "\n";
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
GlyphSetGenerator(fastuidraw::GlyphRender r,
                  fastuidraw::reference_counted_ptr<const fastuidraw::FontFreeType> f,
                  fastuidraw::reference_counted_ptr<fastuidraw::FreeTypeFace> face,
                  std::vector<fastuidraw::Glyph> &dst):
  m_render(r),
  m_font(f)
{
  dst.resize(face->face()->num_glyphs);
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
         fastuidraw::GlyphRender r,
         fastuidraw::reference_counted_ptr<const fastuidraw::FontFreeType> f,
         fastuidraw::reference_counted_ptr<fastuidraw::FreeTypeFace> face,
         std::vector<fastuidraw::Glyph> &dst,
         fastuidraw::reference_counted_ptr<fastuidraw::GlyphCache> glyph_cache,
         std::vector<int> &cnts)
{
  GlyphSetGenerator generator(r, f, face, dst);
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
          glyph_cache->add_glyph(glyph);
        }
    }
}

//////////////////////////////
// global methods
void
create_formatted_text(fastuidraw::GlyphSequence &out_sequence,
                      const std::vector<uint32_t> &glyph_codes,
                      fastuidraw::reference_counted_ptr<const fastuidraw::FontBase> font,
                      const fastuidraw::vec2 &shift_by)
{
  fastuidraw::vec2 pen(shift_by);
  float pixel_size(out_sequence.pixel_size());
  enum fastuidraw::PainterEnums::screen_orientation orientation(out_sequence.orientation());
  fastuidraw::GlyphLayoutData layout;
  float ratio;

  for(uint32_t glyph_code : glyph_codes)
    {
      font->compute_layout_data(glyph_code, layout);
      out_sequence.add_glyph(fastuidraw::GlyphSource(glyph_code, font), pen);

      ratio = pixel_size / layout.units_per_EM();
      pen.x() += ratio * layout.advance().x();
    }

}


void
create_formatted_text(fastuidraw::GlyphSequence &out_sequence,
                      std::istream &istr,
                      fastuidraw::reference_counted_ptr<const fastuidraw::FontBase> font,
                      fastuidraw::reference_counted_ptr<fastuidraw::GlyphSelector> glyph_selector,
                      const fastuidraw::vec2 &starting_place)
{
  std::streampos current_position, end_position;
  float pixel_size(out_sequence.pixel_size());
  enum fastuidraw::PainterEnums::screen_orientation orientation(out_sequence.orientation());
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
  fastuidraw::GlyphLayoutData layout;

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

      glyph_selector->create_glyph_sequence(font, line.begin(), line.end(), glyph_sources.begin());
      for(unsigned int i = 0, endi = glyph_sources.size(); i < endi; ++i)
        {
          sub_p[i] = pen;
          if (glyph_sources[i].m_font)
            {
              float ratio;

              glyph_sources[i].m_font->compute_layout_data(glyph_sources[i].m_glyph_code, layout);
              ratio = pixel_size / layout.units_per_EM();

              empty_line = false;
              pen.x() += ratio * layout.advance().x();

              tallest = std::max(tallest, ratio * (layout.horizontal_layout_offset().y() + layout.size().y()));
              negative_tallest = std::min(negative_tallest, ratio * layout.horizontal_layout_offset().y());
            }
        }

      if (empty_line)
        {
          pen_y_advance = pixel_size + 1.0f;
          offset = (first_line) ? 0 : pen_y_advance;
        }
      else
        {
          if (orientation == fastuidraw::PainterEnums::y_increases_downwards)
            {
              pen_y_advance = tallest - last_negative_tallest;
              offset = (first_line) ? 0 : pen_y_advance;
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

      if (orientation == fastuidraw::PainterEnums::y_increases_downwards)
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

      out_sequence.add_glyphs(cast_c_array(glyph_sources), cast_c_array(sub_p));
    }
}

void
add_fonts_from_path(const std::string &filename,
                    fastuidraw::reference_counted_ptr<fastuidraw::FreeTypeLib> lib,
                    fastuidraw::reference_counted_ptr<fastuidraw::GlyphSelector> glyph_selector,
                    fastuidraw::FontFreeType::RenderParams render_params)
{
  DIR *dir;
  struct dirent *entry;

  dir = opendir(filename.c_str());
  if (!dir)
    {
      add_fonts_from_file(filename, lib, glyph_selector, render_params);
      return;
    }

  for(entry = readdir(dir); entry != nullptr; entry = readdir(dir))
    {
      std::string file;
      file = entry->d_name;
      if (file != ".." && file != ".")
        {
          add_fonts_from_path(filename + "/" + file, lib, glyph_selector, render_params);
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
