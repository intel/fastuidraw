/*!
 * \file font_database.cpp
 * \brief file font_database.cpp
 *
 * Copyright 2016 by Intel.
 *
 * Contact: kevin.rogovin@intel.com
 *
 * This Source Code Form is subject to the
 * terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with
 * this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 *
 * \author Kevin Rogovin <kevin.rogovin@intel.com>
 *
 */

#include <set>
#include <map>
#include <vector>
#include <string>
#include <mutex>
#include <atomic>

#include <fastuidraw/text/font_database.hpp>
#include "../private/util_private.hpp"

namespace
{
  class AbstractFont
  {
  public:
    typedef AbstractFont* pointer_type;

    AbstractFont(const fastuidraw::reference_counted_ptr<const fastuidraw::FontDatabase::FontGeneratorBase> &g):
      m_generator(g)
    {
      FASTUIDRAWassert(g);
    }

    AbstractFont(const fastuidraw::reference_counted_ptr<const fastuidraw::FontBase> &f):
      m_font(f)
    {
      FASTUIDRAWassert(f);
    }

    const fastuidraw::reference_counted_ptr<const fastuidraw::FontBase>&
    font(void);

    bool
    font_ready(void) const
    {
      return m_font;
    }

  private:
    fastuidraw::reference_counted_ptr<const fastuidraw::FontBase> m_font;
    fastuidraw::reference_counted_ptr<const fastuidraw::FontDatabase::FontGeneratorBase> m_generator;
  };

  class font_group:public fastuidraw::reference_counted<font_group>::non_concurrent
  {
  public:
    typedef fastuidraw::FontBase FontBase;
    typedef fastuidraw::FontDatabase FontDatabase;
    typedef FontDatabase::FontGeneratorBase FontGeneratorBase;

    explicit
    font_group(fastuidraw::reference_counted_ptr<font_group> p);

    void
    add_font(AbstractFont *h)
    {
      FASTUIDRAWassert(h);
      m_fonts.push_back(h);
    }

    fastuidraw::GlyphSource
    fetch_glyph(uint32_t character_code, bool skip_parent);

    const fastuidraw::reference_counted_ptr<font_group>&
    parent(void) const
    {
      return m_parent;
    }

    AbstractFont*
    first_font(void)
    {
      return m_fonts.empty() ? nullptr : m_fonts.front();
    }

    fastuidraw::c_array<const AbstractFont::pointer_type>
    fonts(void) const
    {
      return fastuidraw::make_c_array(m_fonts);
    }

  private:
    std::vector<AbstractFont*> m_fonts;
    fastuidraw::reference_counted_ptr<font_group> m_parent;
  };

  class font_group_map_base
  {
  public:
    virtual
    ~font_group_map_base()
    {}

    virtual
    fastuidraw::reference_counted_ptr<font_group>
    get_create(const fastuidraw::FontProperties &props, fastuidraw::reference_counted_ptr<font_group> parent) = 0;

    virtual
    fastuidraw::reference_counted_ptr<font_group>
    fetch_group(const fastuidraw::FontProperties &props) = 0;
  };

  template<typename key_type>
  class font_group_map:
    public font_group_map_base,
    private std::map<key_type, fastuidraw::reference_counted_ptr<font_group> >
  {
  public:
    virtual
    fastuidraw::reference_counted_ptr<font_group>
    get_create(const fastuidraw::FontProperties &props, fastuidraw::reference_counted_ptr<font_group> parent)
    {
      return get_create_key(key_type(props), parent);
    }

    virtual
    fastuidraw::reference_counted_ptr<font_group>
    fetch_group(const fastuidraw::FontProperties &props)
    {
      return fetch_group_key(key_type(props));
    }

  private:
    typedef std::map<key_type, fastuidraw::reference_counted_ptr<font_group> > base_class;

    fastuidraw::reference_counted_ptr<font_group>
    get_create_key(const key_type &key, fastuidraw::reference_counted_ptr<font_group> parent)
    {
      typename base_class::iterator iter;
      fastuidraw::reference_counted_ptr<font_group> return_value;

      iter = this->find(key);
      if (iter == this->end())
        {
          return_value = FASTUIDRAWnew font_group(parent);
          this->operator[](key) = return_value;
        }
      else
        {
          return_value = iter->second;
          FASTUIDRAWassert(return_value->parent() == parent);
        }
      return return_value;
    }

    fastuidraw::reference_counted_ptr<font_group>
    fetch_group_key(const key_type &key)
    {
      typename base_class::iterator iter;

      iter = this->find(key);
      if (iter != this->end())
        {
          return iter->second;
        }
      return fastuidraw::reference_counted_ptr<font_group>();
    }
  };

  class style_key
  {
  public:
    style_key(const fastuidraw::FontProperties &prop):
      m_style(prop.style())
    {
    }

    bool
    operator<(const style_key &rhs) const
    {
      return m_style < rhs.m_style;
    }

    std::string m_style;
  };

  class bold_italic_key
  {
  public:
    bold_italic_key(const fastuidraw::FontProperties &prop):
      m_bold_italic(prop.bold(), prop.italic())
    {
    }

    bool
    operator<(const bold_italic_key &rhs) const
    {
      return m_bold_italic < rhs.m_bold_italic;
    }

    std::pair<bool, bool> m_bold_italic;
  };

  class style_bold_italic_key
  {
  public:
    style_bold_italic_key(const fastuidraw::FontProperties &prop):
      m_style(prop.style()),
      m_bold_italic(prop.bold(), prop.italic())
    {
    }

    bool
    operator<(const style_bold_italic_key &rhs) const
    {
      if (m_style.empty() || rhs.m_style.empty())
        {
          return m_bold_italic < rhs.m_bold_italic;
        }
      else
        {
          return m_style < rhs.m_style;
        }
    }

    std::string m_style;
    std::pair<bool, bool> m_bold_italic;
  };

  class family_style_bold_italic_key:
    public std::pair<std::string, style_bold_italic_key>
  {
  public:
    family_style_bold_italic_key(const fastuidraw::FontProperties &prop):
      std::pair<std::string, style_bold_italic_key>(prop.family(), prop)
    {}
  };

  class foundry_family_style_bold_italic_key:
    public std::pair<std::string, family_style_bold_italic_key>
  {
  public:
    foundry_family_style_bold_italic_key(const fastuidraw::FontProperties &prop):
      std::pair<std::string, family_style_bold_italic_key>(prop.foundry(), prop)
    {}
  };

  class family_bold_italic_key:
    public std::pair<std::string, bold_italic_key>
  {
  public:
    family_bold_italic_key(const fastuidraw::FontProperties &prop):
      std::pair<std::string, bold_italic_key>(prop.family(), prop)
    {}
  };

  class foundry_family_bold_italic_key:
    public std::pair<std::string, family_bold_italic_key>
  {
  public:
    foundry_family_bold_italic_key(const fastuidraw::FontProperties &prop):
      std::pair<std::string, family_bold_italic_key>(prop.foundry(), prop)
    {}
  };

  class family_style_key:
    public std::pair<std::string, style_key>
  {
  public:
    family_style_key(const fastuidraw::FontProperties &prop):
      std::pair<std::string, style_key>(prop.family(), prop)
    {}
  };

  class foundry_family_style_key:
    public std::pair<std::string, family_style_key>
  {
  public:
    foundry_family_style_key(const fastuidraw::FontProperties &prop):
      std::pair<std::string, family_style_key>(prop.foundry(), prop)
    {}
  };

  class family_key
  {
  public:
    family_key(const fastuidraw::FontProperties &prop):
      m_family(prop.family())
    {}

    bool
    operator<(const family_key &rhs) const
    {
      return m_family < rhs.m_family;
    }

    std::string m_family;
  };

  class foundry_family_key:
    public std::pair<std::string, family_key>
  {
  public:
    foundry_family_key(const fastuidraw::FontProperties &prop):
      std::pair<std::string, family_key>(prop.foundry(), prop)
    {}
  };

  class FontDatabasePrivate
  {
  public:
    FontDatabasePrivate(void);
    ~FontDatabasePrivate();

    fastuidraw::reference_counted_ptr<font_group>
    fetch_font_group_no_lock(const fastuidraw::FontProperties &prop,
                             uint32_t selection_strategy);

    fastuidraw::GlyphSource
    fetch_glyph(fastuidraw::reference_counted_ptr<const fastuidraw::FontBase> h,
                uint32_t character_code, uint32_t selection_strategy);

    fastuidraw::GlyphSource
    fetch_glyph(fastuidraw::reference_counted_ptr<font_group> group,
                uint32_t character_code, uint32_t selection_strategy);

    fastuidraw::GlyphSource
    fetch_glyph_no_merging(fastuidraw::reference_counted_ptr<const fastuidraw::FontBase> h,
                           uint32_t character_code);

    AbstractFont*
    add_or_fetch_font_no_lock(const fastuidraw::FontProperties &props, AbstractFont *h);

    enum fastuidraw::return_code
    add_font_no_lock(const fastuidraw::FontProperties &props, AbstractFont *h);

    std::mutex m_mutex;
    std::map<std::string, AbstractFont*> m_fonts;
    fastuidraw::reference_counted_ptr<font_group> m_master_group;
    font_group_map<style_key> m_style_groups;
    font_group_map<bold_italic_key> m_bold_italic_groups;

    font_group_map<style_bold_italic_key> m_style_bold_italic_groups;
    font_group_map<family_style_bold_italic_key> m_family_style_bold_italic_groups;
    font_group_map<foundry_family_style_bold_italic_key> m_foundry_family_style_bold_italic_groups;

    font_group_map<family_bold_italic_key> m_family_bold_italic_groups;
    font_group_map<foundry_family_bold_italic_key> m_foundry_family_bold_italic_groups;

    font_group_map<family_style_key> m_family_style_groups;
    font_group_map<foundry_family_style_key> m_foundry_family_style_groups;

    font_group_map<family_key> m_family_groups;
    font_group_map<foundry_family_key> m_foundry_family_groups;

    /* The various groupings depending on the fetching pattern */
    std::vector<font_group_map_base*> m_style_bold_italic_hunter;
    std::vector<font_group_map_base*> m_style_hunter;
    std::vector<font_group_map_base*> m_bold_italic_hunter;
    std::vector<font_group_map_base*> m_vanilla_hunter;
  };
}


///////////////////////////////////
// font_group methods
font_group::
font_group(fastuidraw::reference_counted_ptr<font_group> parent):
  m_parent(parent)
{
}

fastuidraw::GlyphSource
font_group::
fetch_glyph(uint32_t character_code, bool skip_parent)
{
  uint32_t r;

  for(const auto &abs : m_fonts)
    {
      if (abs->font_ready())
        {
          const auto &font(abs->font());
          r = font->glyph_code(character_code);
          if (r)
            {
              return fastuidraw::GlyphSource(font, r);
            }
        }
    }

  for(const auto &abs : m_fonts)
    {
      const auto &font(abs->font());
      r = font->glyph_code(character_code);
      if (r)
        {
          return fastuidraw::GlyphSource(font, r);
        }
    }

  if (m_parent && !skip_parent)
    {
      return m_parent->fetch_glyph(character_code, false);
    }

  return fastuidraw::GlyphSource();
}

////////////////////////////////////
// FontDatabasePrivate methods
FontDatabasePrivate::
FontDatabasePrivate(void)
{
  m_master_group = FASTUIDRAWnew font_group(fastuidraw::reference_counted_ptr<font_group>());

  m_style_bold_italic_hunter.push_back(&m_foundry_family_style_bold_italic_groups);
  m_style_bold_italic_hunter.push_back(&m_family_style_bold_italic_groups);
  m_style_bold_italic_hunter.push_back(&m_style_bold_italic_groups);
  m_style_bold_italic_hunter.push_back(&m_style_groups);

  m_style_hunter.push_back(&m_foundry_family_style_groups);
  m_style_hunter.push_back(&m_family_style_groups);
  m_style_hunter.push_back(&m_style_groups);

  m_bold_italic_hunter.push_back(&m_foundry_family_bold_italic_groups);
  m_bold_italic_hunter.push_back(&m_family_bold_italic_groups);
  m_bold_italic_hunter.push_back(&m_bold_italic_groups);

  m_vanilla_hunter.push_back(&m_foundry_family_groups);
  m_vanilla_hunter.push_back(&m_family_groups);
}

FontDatabasePrivate::
~FontDatabasePrivate()
{
  for (const auto &p : m_fonts)
    {
      FASTUIDRAWdelete(p.second);
    }
}

fastuidraw::reference_counted_ptr<font_group>
FontDatabasePrivate::
fetch_font_group_no_lock(const fastuidraw::FontProperties &prop,
                         uint32_t selection_strategy)
{
  using namespace fastuidraw;
  const std::vector<font_group_map_base*> *src(nullptr);
  reference_counted_ptr<font_group> return_value;

  /* such a hassle, we need to check the bits of selection_strategy
   * to decide if we pay attention to style, bold/italic
   */
  if ((selection_strategy & FontDatabase::ignore_style) == 0)
    {
      if ((selection_strategy & FontDatabase::ignore_bold_italic) == 0)
        {
          src = &m_style_bold_italic_hunter;
        }
      else
        {
          src = &m_style_hunter;
        }
    }
  else
    {
      if ((selection_strategy & FontDatabase::ignore_bold_italic) == 0)
        {
          src = &m_bold_italic_hunter;
        }
      else
        {
          src = &m_vanilla_hunter;
        }
    }

  FASTUIDRAWassert(src && !src->empty());

  return_value = src->front()->fetch_group(prop);
  if (return_value || (selection_strategy & FontDatabase::exact_match) != 0u)
    {
      return return_value;
    }

  for (unsigned int i = 1, endi = src->size(); i < endi; ++i)
    {
      return_value = (*src)[i]->fetch_group(prop);
      if (return_value)
        {
          return return_value;
        }
    }

  return m_master_group;
}

fastuidraw::GlyphSource
FontDatabasePrivate::
fetch_glyph(fastuidraw::reference_counted_ptr<font_group> group,
            uint32_t character_code, uint32_t selection_strategy)
{
  FASTUIDRAWassert(group);
  return group->fetch_glyph(character_code, selection_strategy);
}

fastuidraw::GlyphSource
FontDatabasePrivate::
fetch_glyph(fastuidraw::reference_counted_ptr<const fastuidraw::FontBase> h,
            uint32_t character_code, uint32_t selection_strategy)
{
  using namespace fastuidraw;

  GlyphSource return_value;
  uint32_t r(0);

  if (!h)
    {
      return return_value;
    }

  r = h->glyph_code(character_code);
  if (r)
    {
      return_value = GlyphSource(h, r);
    }
  else
    {
      reference_counted_ptr<font_group> g;
      g = fetch_font_group_no_lock(h->properties(), selection_strategy);
      if (g)
        {
          return_value = g->fetch_glyph(character_code,
                                        (selection_strategy & FontDatabase::exact_match) != 0);
        }
    }
  return return_value;
}

fastuidraw::GlyphSource
FontDatabasePrivate::
fetch_glyph_no_merging(fastuidraw::reference_counted_ptr<const fastuidraw::FontBase> h,
                       uint32_t character_code)
{
  if (!h)
    {
      return fastuidraw::GlyphSource();
    }

  fastuidraw::GlyphSource return_value;
  uint32_t glyph_code(0);

  glyph_code = h->glyph_code(character_code);
  if (glyph_code)
    {
      return_value = fastuidraw::GlyphSource(h, glyph_code);
    }

  return return_value;
}

enum fastuidraw::return_code
FontDatabasePrivate::
add_font_no_lock(const fastuidraw::FontProperties &props, AbstractFont *h)
{
  AbstractFont *a;
  a = add_or_fetch_font_no_lock(props, h);

  return (a == h) ?
    fastuidraw::routine_success :
    fastuidraw::routine_fail;
}

AbstractFont*
FontDatabasePrivate::
add_or_fetch_font_no_lock(const fastuidraw::FontProperties &props, AbstractFont *h)
{
  using namespace fastuidraw;

  reference_counted_ptr<font_group> parent;
  std::string fnt_source;
  std::map<std::string, AbstractFont*>::const_iterator iter;

  fnt_source = props.source_label();
  iter = m_fonts.find(fnt_source);
  if (iter != m_fonts.end())
    {
      FASTUIDRAWdelete(h);
      return iter->second;
    }
  m_fonts[fnt_source] = h;
  m_master_group->add_font(h);

  /* keys with just (bold, italic) */
  parent = m_bold_italic_groups.get_create(props, m_master_group);
  parent->add_font(h);

  parent = m_family_bold_italic_groups.get_create(props, parent);
  parent->add_font(h);

  parent = m_foundry_family_bold_italic_groups.get_create(props, parent);
  parent->add_font(h);

  /* keys with just style */
  parent = m_style_groups.get_create(props, m_master_group);
  parent->add_font(h);

  parent = m_family_style_groups.get_create(props, parent);
  parent->add_font(h);

  parent = m_foundry_family_style_groups.get_create(props, parent);
  parent->add_font(h);

  /* keys with style and (bold, italic) */
  parent = m_style_bold_italic_groups.get_create(props, m_master_group);
  parent->add_font(h);

  parent = m_family_style_bold_italic_groups.get_create(props, parent);
  parent->add_font(h);

  parent = m_foundry_family_style_bold_italic_groups.get_create(props, parent);
  parent->add_font(h);

  /* keys without style and without (bold, italic) */
  parent = m_family_groups.get_create(props, m_master_group);
  parent->add_font(h);

  parent = m_foundry_family_groups.get_create(props, parent);
  parent->add_font(h);

  return h;
}

/////////////////////////////////////////
// AbstractFont methods
const fastuidraw::reference_counted_ptr<const fastuidraw::FontBase>&
AbstractFont::
font(void)
{
  if (!m_font)
    {
      m_font = m_generator->generate_font();
      m_generator.clear();
    }
  return m_font;
}

////////////////////////////////////////////////
// fastuidraw::FontDatabase methods
fastuidraw::FontDatabase::
FontDatabase(void)
{
  m_d = FASTUIDRAWnew FontDatabasePrivate();
}

fastuidraw::FontDatabase::
~FontDatabase()
{
  FontDatabasePrivate *d;
  d = static_cast<FontDatabasePrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = nullptr;
}

enum fastuidraw::return_code
fastuidraw::FontDatabase::
add_font(const reference_counted_ptr<const FontBase> &h)
{
  if (!h)
    {
      return routine_fail;
    }
  else
    {
      FontDatabasePrivate *d;

      d = static_cast<FontDatabasePrivate*>(m_d);
      std::lock_guard<std::mutex> m(d->m_mutex);

      return d->add_font_no_lock(h->properties(), FASTUIDRAWnew AbstractFont(h));
    }
}

enum fastuidraw::return_code
fastuidraw::FontDatabase::
add_font_generator(const reference_counted_ptr<const FontGeneratorBase> &h)
{
  if (!h)
    {
      return routine_fail;
    }
  else
    {
      FontDatabasePrivate *d;

      d = static_cast<FontDatabasePrivate*>(m_d);
      std::lock_guard<std::mutex> m(d->m_mutex);

      return d->add_font_no_lock(h->font_properties(), FASTUIDRAWnew AbstractFont(h));
    }
}

fastuidraw::reference_counted_ptr<const fastuidraw::FontBase>
fastuidraw::FontDatabase::
fetch_or_generate_font(const reference_counted_ptr<const FontGeneratorBase> &h)
{
  FontDatabasePrivate *d;
  AbstractFont *a;
  reference_counted_ptr<const FontBase> return_value;

  if (!h)
    {
      return return_value;
    }

  a = FASTUIDRAWnew AbstractFont(h);
  d = static_cast<FontDatabasePrivate*>(m_d);
  std::lock_guard<std::mutex> m(d->m_mutex);

  a = d->add_or_fetch_font_no_lock(h->font_properties(), a);
  return a->font();
}

fastuidraw::reference_counted_ptr<const fastuidraw::FontBase>
fastuidraw::FontDatabase::
fetch_font(c_string source_label)
{
  FontDatabasePrivate *d;
  d = static_cast<FontDatabasePrivate*>(m_d);

  std::lock_guard<std::mutex> m(d->m_mutex);
  reference_counted_ptr<const FontBase> return_value;
  std::map<std::string, AbstractFont*>::const_iterator iter;

  iter = d->m_fonts.find(source_label);
  if (iter != d->m_fonts.end())
    {
      return_value = iter->second->font();
    }

  return return_value;
}

fastuidraw::FontDatabase::FontGroup
fastuidraw::FontDatabase::
parent_group(FontGroup G)
{
  font_group *d;
  FontGroup return_value;

  d = static_cast<font_group*>(G.m_d);
  if (d)
    {
      d = d->parent().get();
    }

  return_value.m_d = d;
  return return_value;
}

unsigned int
fastuidraw::FontDatabase::
number_fonts(FontGroup G)
{
  unsigned int return_value(0);
  font_group *g;
  FontDatabasePrivate *d;

  d = static_cast<FontDatabasePrivate*>(m_d);
  g = static_cast<font_group*>(G.m_d);
  if (g)
    {
      std::lock_guard<std::mutex> m(d->m_mutex);
      return_value = g->fonts().size();
    }
  return return_value;
}

fastuidraw::reference_counted_ptr<const fastuidraw::FontBase>
fastuidraw::FontDatabase::
fetch_font(FontGroup G, unsigned int N)
{
  reference_counted_ptr<const FontBase> return_value;
  font_group *g;
  FontDatabasePrivate *d;

  d = static_cast<FontDatabasePrivate*>(m_d);
  g = static_cast<font_group*>(G.m_d);

  if (g && g->fonts().size() < N)
    {
      std::lock_guard<std::mutex> m(d->m_mutex);
      return_value = g->fonts()[N]->font();
    }

  return return_value;
}

fastuidraw::reference_counted_ptr<const fastuidraw::FontBase>
fastuidraw::FontDatabase::
fetch_font(const FontProperties &prop, uint32_t selection_strategy)
{
  FontDatabasePrivate *d;
  d = static_cast<FontDatabasePrivate*>(m_d);

  std::lock_guard<std::mutex> m(d->m_mutex);
  return d->fetch_font_group_no_lock(prop, selection_strategy)->first_font()->font();
}

fastuidraw::GlyphSource
fastuidraw::FontDatabase::
fetch_glyph_no_merging_no_lock(reference_counted_ptr<const FontBase> h,
                               uint32_t character_code)
{
  FontDatabasePrivate *d;
  d = static_cast<FontDatabasePrivate*>(m_d);
  return d->fetch_glyph_no_merging(h, character_code);
}

fastuidraw::GlyphSource
fastuidraw::FontDatabase::
fetch_glyph_no_lock(reference_counted_ptr<const FontBase> h,
                    uint32_t character_code, uint32_t selection_strategy)
{
  FontDatabasePrivate *d;
  d = static_cast<FontDatabasePrivate*>(m_d);
  return d->fetch_glyph(h, character_code, selection_strategy);
}

void
fastuidraw::FontDatabase::
lock_mutex(void)
{
  FontDatabasePrivate *d;
  d = static_cast<FontDatabasePrivate*>(m_d);
  d->m_mutex.lock();
}

void
fastuidraw::FontDatabase::
unlock_mutex(void)
{
  FontDatabasePrivate *d;
  d = static_cast<FontDatabasePrivate*>(m_d);
  d->m_mutex.unlock();
}

fastuidraw::GlyphSource
fastuidraw::FontDatabase::
fetch_glyph_no_lock(FontGroup group,
                    uint32_t character_code, uint32_t selection_strategy)
{
  FontDatabasePrivate *d;
  d = static_cast<FontDatabasePrivate*>(m_d);

  reference_counted_ptr<font_group> p;
  p = reference_counted_ptr<font_group>(static_cast<font_group*>(group.m_d));
  if (!p)
    {
      p = d->m_master_group;
    }
  return d->fetch_glyph(p, character_code, selection_strategy);
}

fastuidraw::FontDatabase::FontGroup
fastuidraw::FontDatabase::
fetch_group(const FontProperties &props, uint32_t selection_strategy)
{
  FontGroup return_value;
  reference_counted_ptr<font_group> h;

  FontDatabasePrivate *d;
  d = static_cast<FontDatabasePrivate*>(m_d);

  std::lock_guard<std::mutex> m(d->m_mutex);
  h = d->fetch_font_group_no_lock(props, selection_strategy);
  return_value.m_d = h.get();

  return return_value;
}

fastuidraw::FontDatabase::FontGroup
fastuidraw::FontDatabase::
root_group(void)
{
  FontGroup return_value;
  FontDatabasePrivate *d;

  d = static_cast<FontDatabasePrivate*>(m_d);
  return_value.m_d = d->m_master_group.get();
  return return_value;
}

fastuidraw::GlyphSource
fastuidraw::FontDatabase::
fetch_glyph(const FontProperties &props, uint32_t character_code, uint32_t selection_strategy)
{
  FontDatabasePrivate *d;
  reference_counted_ptr<font_group> g;

  d = static_cast<FontDatabasePrivate*>(m_d);

  std::lock_guard<std::mutex> m(d->m_mutex);
  g = d->fetch_font_group_no_lock(props, selection_strategy);
  return d->fetch_glyph(g, character_code, (selection_strategy & exact_match) != 0u);
}

fastuidraw::GlyphSource
fastuidraw::FontDatabase::
fetch_glyph(FontGroup h, uint32_t character_code, uint32_t selection_strategy)
{
  GlyphSource G;
  lock_mutex();
  G = fetch_glyph_no_lock(h, character_code, selection_strategy);
  unlock_mutex();
  return G;
}

fastuidraw::GlyphSource
fastuidraw::FontDatabase::
fetch_glyph(reference_counted_ptr<const FontBase> h,
            uint32_t character_code, uint32_t selection_strategy)
{
  GlyphSource G;
  lock_mutex();
  G = fetch_glyph_no_lock(h, character_code, selection_strategy);
  unlock_mutex();
  return G;
}

fastuidraw::GlyphSource
fastuidraw::FontDatabase::
fetch_glyph_no_merging(reference_counted_ptr<const FontBase> h,
                       uint32_t character_code)
{
  GlyphSource G;
  lock_mutex();
  G = fetch_glyph_no_merging_no_lock(h, character_code);
  unlock_mutex();
  return G;
}
