#include <string>
#include <algorithm>
#include <dirent.h>
#include "text_helper.hpp"

namespace
{
  void
  preprocess_text(std::string &text)
  {
    /* we want to change '\t' into 4 spaces
     */
    std::string v;
    v.reserve(text.size() + 4 * std::count(text.begin(), text.end(), '\t'));
    for(std::string::const_iterator iter = text.begin(); iter != text.end(); ++iter)
      {
        if(*iter != '\t')
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
    std::vector<fastuidraw::reference_counted_ptr<fastuidraw::FreeTypeFace::GeneratorBase> > h;
    FT_Error error_code;
    FT_Face face(nullptr);

    lib->lock();
    error_code = FT_New_Face(lib->lib(), filename.c_str(), 0, &face);
    if(error_code == 0 && face != nullptr && (face->face_flags & FT_FACE_FLAG_SCALABLE) != 0)
      {
	for(unsigned int i = 0, endi = face->num_faces; i < endi; ++i)
	  {
	    h.push_back(FASTUIDRAWnew fastuidraw::FreeTypeFace::GeneratorFile(filename.c_str(), i));
 	  }
      }
    if(face != nullptr)
      {
	FT_Done_Face(face);
      }
    lib->unlock();

    for(unsigned int i = 0, endi = h.size(); i < endi; ++i)
      {
	fastuidraw::reference_counted_ptr<fastuidraw::FontFreeType> f;
	f = FASTUIDRAWnew fastuidraw::FontFreeType(h[i], render_params, lib);
        glyph_selector->add_font(f);
      }
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

  if(num_threads < 2)
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

  if(glyph_cache)
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
create_formatted_text(std::istream &istr, fastuidraw::GlyphRender renderer,
                      float pixel_size,
                      fastuidraw::reference_counted_ptr<const fastuidraw::FontBase> font,
                      fastuidraw::reference_counted_ptr<fastuidraw::GlyphSelector> glyph_selector,
                      std::vector<fastuidraw::Glyph> &glyphs,
                      std::vector<fastuidraw::vec2> &positions,
                      std::vector<uint32_t> &character_codes,
                      std::vector<LineData> *line_data,
                      std::vector<fastuidraw::range_type<float> > *glyph_extents,
		      enum fastuidraw::PainterEnums::glyph_orientation orientation)
{
  std::streampos current_position, end_position;
  unsigned int loc(0);
  fastuidraw::vec2 pen(0.0f, 0.0f);
  std::string line, original_line;
  float last_negative_tallest(0.0f);

  current_position = istr.tellg();
  istr.seekg(0, std::ios::end);
  end_position = istr.tellg();
  istr.seekg(current_position, std::ios::beg);

  glyphs.resize(end_position - current_position);
  positions.resize(end_position - current_position);
  character_codes.resize(end_position - current_position);

  if(glyph_extents)
    {
      glyph_extents->resize(end_position - current_position);
    }

  if(line_data)
    {
      line_data->clear();
    }

  fastuidraw::c_array<fastuidraw::Glyph> glyphs_ptr(cast_c_array(glyphs));
  fastuidraw::c_array<fastuidraw::vec2> pos_ptr(cast_c_array(positions));
  fastuidraw::c_array<uint32_t> char_codes_ptr(cast_c_array(character_codes));
  fastuidraw::c_array<fastuidraw::range_type<float> > extents_ptr;

  if(glyph_extents)
    {
      extents_ptr = cast_c_array(*glyph_extents);
    }

  while(getline(istr, line))
    {
      fastuidraw::c_array<fastuidraw::Glyph> sub_g;
      fastuidraw::c_array<fastuidraw::vec2> sub_p;
      fastuidraw::c_array<uint32_t> sub_ch;
      fastuidraw::c_array<fastuidraw::range_type<float> > sub_extents;
      float tallest, negative_tallest, offset;
      bool empty_line;
      LineData L;
      float pen_y_advance;

      empty_line = true;
      tallest = 0.0f;
      negative_tallest = 0.0f;

      original_line = line;
      preprocess_text(line);

      sub_g = glyphs_ptr.sub_array(loc, line.length());
      sub_p = pos_ptr.sub_array(loc, line.length());
      sub_ch = char_codes_ptr.sub_array(loc, line.length());
      if(glyph_extents)
        {
          sub_extents = extents_ptr.sub_array(loc, line.length());
        }

      glyph_selector->create_glyph_sequence(renderer, font, line.begin(), line.end(), sub_g.begin());
      for(unsigned int i = 0, endi = sub_g.size(); i < endi; ++i)
        {
          fastuidraw::Glyph g;

          g = sub_g[i];
          sub_p[i] = pen;
          sub_ch[i] = static_cast<uint32_t>(line[i]);
          if(g.valid())
            {
              float ratio;
              ratio = pixel_size / g.layout().m_units_per_EM;

              if(glyph_extents)
                {
                  sub_extents[i].m_begin = pen.x() + ratio * g.layout().m_horizontal_layout_offset.x();
                  sub_extents[i].m_end = sub_extents[i].m_begin + ratio * g.layout().m_size.x();
                }

              empty_line = false;
              pen.x() += ratio * g.layout().m_advance.x();

              tallest = std::max(tallest, ratio * (g.layout().m_horizontal_layout_offset.y() + g.layout().m_size.y()));
              negative_tallest = std::min(negative_tallest, ratio * g.layout().m_horizontal_layout_offset.y());
            }
        }

      if(empty_line)
        {
          offset = pixel_size + 1.0f;
	  pen_y_advance = offset;
        }
      else
        {
	  if(orientation == fastuidraw::PainterEnums::y_increases_downwards)
	    {
	      offset = (tallest - last_negative_tallest);
	      pen_y_advance = offset;
	    }
	  else
	    {
	      offset = -negative_tallest;
	      pen_y_advance = tallest - negative_tallest;
	    }
        }

      for(unsigned int i = 0; i < sub_p.size(); ++i)
        {
          sub_p[i].y() += offset;
        }

      L.m_range.m_begin = loc;
      L.m_range.m_end = loc + line.length();
      L.m_horizontal_spread.m_begin = 0.0f;
      L.m_horizontal_spread.m_end = pen.x();
      L.m_vertical_spread.m_begin = pen.y() + offset - tallest;
      L.m_vertical_spread.m_end = pen.y() + offset - negative_tallest;

      pen.x() = 0.0f;
      pen.y() += pen_y_advance + 1.0f;
      loc += line.length();
      last_negative_tallest = negative_tallest;

      if(line_data && !empty_line)
        {
          line_data->push_back(L);
        }
    }

  glyphs.resize(loc);
  positions.resize(loc);
  character_codes.resize(loc);
  if(glyph_extents)
    {
      glyph_extents->resize(loc);
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
  if(!dir)
    {
      add_fonts_from_file(filename, lib, glyph_selector, render_params);
      return;
    }

  for(entry = readdir(dir); entry != nullptr; entry = readdir(dir))
    {
      std::string file;
      file = entry->d_name;
      if(file != ".." && file != ".")
        {
          add_fonts_from_path(filename + "/" + file, lib, glyph_selector, render_params);
        }
    }
  closedir(dir);
}

const char*
default_font(void)
{
  #ifdef __WIN32
    {
      return "C:/Windows/Fonts/arial.ttf";
    }
  #else
    {
      return "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf";
    }
  #endif
}

const char*
default_font_path(void)
{
  #ifdef __WIN32
    {
      return "C:/Windows/Fonts";
    }
  #else
    {
      return "/usr/share/fonts/truetype";
    }
  #endif
}
