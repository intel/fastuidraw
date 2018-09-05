/*!
 * \file glyph_selector.cpp
 * \brief file glyph_selector.cpp
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

#include <mutex>
#include <string>
#include <set>
#include <map>

#include <fastuidraw/text/glyph_selector.hpp>
#include "../private/util_private.hpp"

namespace
{
  class font_group:public fastuidraw::reference_counted<font_group>::non_concurrent
  {
  public:
    typedef fastuidraw::FontBase FontBase;
    typedef fastuidraw::GlyphSelector GlyphSelector;
    typedef GlyphSelector::FontGeneratorBase FontGeneratorBase;

    explicit
    font_group(fastuidraw::reference_counted_ptr<font_group> p);

    enum fastuidraw::return_code
    add_font(fastuidraw::reference_counted_ptr<const FontBase> h);

    enum fastuidraw::return_code
    add_font(fastuidraw::reference_counted_ptr<const FontGeneratorBase> h);

    fastuidraw::GlyphSelector::GlyphSource
    fetch_glyph(uint32_t character_code, bool skip_parent);

    const fastuidraw::reference_counted_ptr<font_group>&
    parent(void) const
    {
      return m_parent;
    }

    fastuidraw::reference_counted_ptr<const fastuidraw::FontBase>
    first_font(void);

    fastuidraw::c_array<const fastuidraw::reference_counted_ptr<const FontBase> >
    fonts(void) const
    {
      return fastuidraw::make_c_array(m_fonts);
    }

    fastuidraw::c_array<const fastuidraw::reference_counted_ptr<const FontGeneratorBase> >
    font_generators(void) const
    {
      return fastuidraw::make_c_array(m_gens_unused);
    }

  private:
    fastuidraw::reference_counted_ptr<const fastuidraw::FontBase>
    use_unused_generator(void);

    std::set<fastuidraw::reference_counted_ptr<const FontGeneratorBase> > m_gen_set;
    std::set<fastuidraw::reference_counted_ptr<const FontBase> > m_font_set;

    std::vector<fastuidraw::reference_counted_ptr<const FontGeneratorBase> > m_gens_unused;
    std::vector<fastuidraw::reference_counted_ptr<const FontBase> > m_fonts;
    fastuidraw::reference_counted_ptr<font_group> m_parent;
  };

  template<typename key_type>
  class font_group_map:
    public std::map<key_type, fastuidraw::reference_counted_ptr<font_group> >
  {
  public:
    typedef std::map<key_type, fastuidraw::reference_counted_ptr<font_group> > base_class;

    fastuidraw::reference_counted_ptr<font_group>
    get_create(const key_type &key, fastuidraw::reference_counted_ptr<font_group> parent)
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
    fetch_group(const key_type &key)
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

  class GlyphSelectorPrivate
  {
  public:
    GlyphSelectorPrivate(void);

    fastuidraw::reference_counted_ptr<font_group>
    fetch_font_group_no_lock(const fastuidraw::FontProperties &prop,
                             bool exact_match);

    fastuidraw::GlyphSelector::GlyphSource
    fetch_glyph(fastuidraw::reference_counted_ptr<const fastuidraw::FontBase> h,
                uint32_t character_code, bool exact_match);

    fastuidraw::GlyphSelector::GlyphSource
    fetch_glyph(fastuidraw::reference_counted_ptr<font_group> group,
                uint32_t character_code, bool exact_match);

    fastuidraw::GlyphSelector::GlyphSource
    fetch_glyph_no_merging(fastuidraw::reference_counted_ptr<const fastuidraw::FontBase> h,
                           uint32_t character_code);

    template<typename T>
    void
    add_font_no_lock(const fastuidraw::FontProperties &props, const T &h);

    std::mutex m_mutex;
    fastuidraw::reference_counted_ptr<font_group> m_master_group;
    font_group_map<style_bold_italic_key> m_style_bold_italic_groups;
    font_group_map<family_style_bold_italic_key> m_family_style_bold_italic_groups;
    font_group_map<foundry_family_style_bold_italic_key> m_foundry_family_style_bold_italic_groups;
  };
}


///////////////////////////////////
// font_group methods
font_group::
font_group(fastuidraw::reference_counted_ptr<font_group> parent):
  m_parent(parent)
{
}

enum fastuidraw::return_code
font_group::
add_font(fastuidraw::reference_counted_ptr<const FontBase> h)
{
  std::pair<std::set<fastuidraw::reference_counted_ptr<const FontBase> >::iterator, bool> R;
  R = m_font_set.insert(h);

  if (R.second)
    {
      m_fonts.push_back(h);
      return fastuidraw::routine_success;
    }
  else
    {
      return fastuidraw::routine_fail;
    }
}

enum fastuidraw::return_code
font_group::
add_font(fastuidraw::reference_counted_ptr<const FontGeneratorBase> h)
{
  std::pair<std::set<fastuidraw::reference_counted_ptr<const FontGeneratorBase> >::iterator, bool> R;
  R = m_gen_set.insert(h);

  if (R.second)
    {
      m_gens_unused.push_back(h);
      return fastuidraw::routine_success;
    }
  else
    {
      return fastuidraw::routine_fail;
    }
}

fastuidraw::reference_counted_ptr<const fastuidraw::FontBase>
font_group::
first_font(void)
{
  while(m_fonts.empty() && !m_gens_unused.empty())
    {
      use_unused_generator();
    }

  return m_fonts.empty() ?
    fastuidraw::reference_counted_ptr<const fastuidraw::FontBase>() :
    m_fonts.front();
}

fastuidraw::reference_counted_ptr<const fastuidraw::FontBase>
font_group::
use_unused_generator(void)
{
  fastuidraw::reference_counted_ptr<const FontGeneratorBase> g;
  fastuidraw::reference_counted_ptr<const FontBase> f;

  FASTUIDRAWassert(!m_gens_unused.empty());
  g = m_gens_unused.back();
  m_gens_unused.pop_back();

  f = g->generate_font();
  if (f)
    {
      m_font_set.insert(f);
      m_fonts.push_back(f);
    }

  return f;
}

fastuidraw::GlyphSelector::GlyphSource
font_group::
fetch_glyph(uint32_t character_code, bool skip_parent)
{
  uint32_t r;

  for(const auto &font : m_fonts)
    {
      r = font->glyph_code(character_code);
      if (r)
        {
          return fastuidraw::GlyphSelector::GlyphSource(font, r);
        }
    }

  while(!m_gens_unused.empty())
    {
      fastuidraw::reference_counted_ptr<const FontBase> f;

      f = use_unused_generator();
      if (f)
        {
          r = f->glyph_code(character_code);
          if (r)
            {
              return fastuidraw::GlyphSelector::GlyphSource(f, r);
            }
        }
    }

  if (m_parent && !skip_parent)
    {
      return m_parent->fetch_glyph(character_code, false);
    }

  return fastuidraw::GlyphSelector::GlyphSource();
}

////////////////////////////////////
// GlyphSelectorPrivate methods
GlyphSelectorPrivate::
GlyphSelectorPrivate(void)
{
  m_master_group = FASTUIDRAWnew font_group(fastuidraw::reference_counted_ptr<font_group>());
}

fastuidraw::reference_counted_ptr<font_group>
GlyphSelectorPrivate::
fetch_font_group_no_lock(const fastuidraw::FontProperties &prop,
                         bool exact_match)
{
  fastuidraw::reference_counted_ptr<font_group> return_value;

  return_value = m_foundry_family_style_bold_italic_groups.fetch_group(prop);
  if (return_value || exact_match)
    {
      return return_value;
    }

  return_value = m_family_style_bold_italic_groups.fetch_group(prop);
  if (return_value)
    {
      return return_value;
    }

  return_value = m_style_bold_italic_groups.fetch_group(prop);
  if (return_value)
    {
      return return_value;
    }

  return m_master_group;
}

fastuidraw::GlyphSelector::GlyphSource
GlyphSelectorPrivate::
fetch_glyph(fastuidraw::reference_counted_ptr<font_group> group,
            uint32_t character_code, bool exact_match)
{
  FASTUIDRAWassert(group);
  return group->fetch_glyph(character_code, exact_match);
}

fastuidraw::GlyphSelector::GlyphSource
GlyphSelectorPrivate::
fetch_glyph(fastuidraw::reference_counted_ptr<const fastuidraw::FontBase> h,
            uint32_t character_code, bool exact_match)
{
  fastuidraw::GlyphSelector::GlyphSource return_value;
  uint32_t r(0);

  if (!h)
    {
      return return_value;
    }

  r = h->glyph_code(character_code);
  if (r)
    {
      return_value = fastuidraw::GlyphSelector::GlyphSource(h, r);
    }
  else
    {
      font_group_map<foundry_family_style_bold_italic_key>::const_iterator iter;
      iter = m_foundry_family_style_bold_italic_groups.find(h->properties());
      if (iter != m_foundry_family_style_bold_italic_groups.end())
        {
          return_value = iter->second->fetch_glyph(character_code, exact_match);
        }
    }
  return return_value;
}

fastuidraw::GlyphSelector::GlyphSource
GlyphSelectorPrivate::
fetch_glyph_no_merging(fastuidraw::reference_counted_ptr<const fastuidraw::FontBase> h,
                       uint32_t character_code)
{
  if (!h)
    {
      return fastuidraw::GlyphSelector::GlyphSource();
    }

  fastuidraw::GlyphSelector::GlyphSource return_value;
  uint32_t glyph_code(0);

  glyph_code = h->glyph_code(character_code);
  if (glyph_code)
    {
      return_value = fastuidraw::GlyphSelector::GlyphSource(h, glyph_code);
    }

  return return_value;
}

template<typename T>
void
GlyphSelectorPrivate::
add_font_no_lock(const fastuidraw::FontProperties &props, const T &h)
{
  enum fastuidraw::return_code R;

  fastuidraw::reference_counted_ptr<font_group> parent;
  parent = m_master_group;
  R = parent->add_font(h);
  if (R == fastuidraw::routine_success)
    {
      parent = m_style_bold_italic_groups.get_create(props, parent);
      parent->add_font(h);

      parent = m_family_style_bold_italic_groups.get_create(props, parent);
      parent->add_font(h);

      parent = m_foundry_family_style_bold_italic_groups.get_create(props, parent);
      parent->add_font(h);
    }
}

///////////////////////////////////////////////
// fastuidraw::GlyphSelector::FontGroup methods
fastuidraw::c_array<const fastuidraw::reference_counted_ptr<const fastuidraw::FontBase> >
fastuidraw::GlyphSelector::FontGroup::
loaded_fonts(void) const
{
  font_group *d;
  c_array<const reference_counted_ptr<const FontBase> > return_value;

  d = static_cast<font_group*>(m_d);
  if (d)
    {
      return_value = d->fonts();
    }

  return return_value;
}

fastuidraw::c_array<const fastuidraw::reference_counted_ptr<const fastuidraw::GlyphSelector::FontGeneratorBase> >
fastuidraw::GlyphSelector::FontGroup::
font_generators(void) const
{
  font_group *d;
  c_array<const reference_counted_ptr<const FontGeneratorBase> > return_value;

  d = static_cast<font_group*>(m_d);
  if (d)
    {
      return_value = d->font_generators();
    }

  return return_value;
}

////////////////////////////////////////////////
// fastuidraw::GlyphSelector methods
fastuidraw::GlyphSelector::
GlyphSelector(void)
{
  m_d = FASTUIDRAWnew GlyphSelectorPrivate();
}

fastuidraw::GlyphSelector::
~GlyphSelector()
{
  GlyphSelectorPrivate *d;
  d = static_cast<GlyphSelectorPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = nullptr;
}

void
fastuidraw::GlyphSelector::
add_font(reference_counted_ptr<const FontBase> h)
{
  if (!h)
    {
      return;
    }

  GlyphSelectorPrivate *d;
  d = static_cast<GlyphSelectorPrivate*>(m_d);

  std::lock_guard<std::mutex> m(d->m_mutex);
  d->add_font_no_lock(h->properties(), h);
}

void
fastuidraw::GlyphSelector::
add_font_generator(reference_counted_ptr<const FontGeneratorBase> h)
{
  if (!h)
    {
      return;
    }

  GlyphSelectorPrivate *d;
  d = static_cast<GlyphSelectorPrivate*>(m_d);

  std::lock_guard<std::mutex> m(d->m_mutex);
  d->add_font_no_lock(h->font_properties(), h);
}

fastuidraw::reference_counted_ptr<const fastuidraw::FontBase>
fastuidraw::GlyphSelector::
fetch_font(const FontProperties &prop, bool exact_match)
{
  GlyphSelectorPrivate *d;
  d = static_cast<GlyphSelectorPrivate*>(m_d);

  std::lock_guard<std::mutex> m(d->m_mutex);
  return d->fetch_font_group_no_lock(prop, exact_match)->first_font();
}

fastuidraw::GlyphSelector::GlyphSource
fastuidraw::GlyphSelector::
fetch_glyph_no_merging_no_lock(reference_counted_ptr<const FontBase> h,
                               uint32_t character_code)
{
  GlyphSelectorPrivate *d;
  d = static_cast<GlyphSelectorPrivate*>(m_d);
  return d->fetch_glyph_no_merging(h, character_code);
}

fastuidraw::GlyphSelector::GlyphSource
fastuidraw::GlyphSelector::
fetch_glyph_no_lock(reference_counted_ptr<const FontBase> h,
                    uint32_t character_code, bool exact_match)
{
  GlyphSelectorPrivate *d;
  d = static_cast<GlyphSelectorPrivate*>(m_d);
  return d->fetch_glyph(h, character_code, exact_match);
}

void
fastuidraw::GlyphSelector::
lock_mutex(void)
{
  GlyphSelectorPrivate *d;
  d = static_cast<GlyphSelectorPrivate*>(m_d);
  d->m_mutex.lock();
}

void
fastuidraw::GlyphSelector::
unlock_mutex(void)
{
  GlyphSelectorPrivate *d;
  d = static_cast<GlyphSelectorPrivate*>(m_d);
  d->m_mutex.unlock();
}

fastuidraw::GlyphSelector::GlyphSource
fastuidraw::GlyphSelector::
fetch_glyph_no_lock(FontGroup group,
                    uint32_t character_code, bool exact_match)
{
  GlyphSelectorPrivate *d;
  d = static_cast<GlyphSelectorPrivate*>(m_d);

  reference_counted_ptr<font_group> p;
  p = reference_counted_ptr<font_group>(static_cast<font_group*>(group.m_d));
  if (!p)
    {
      p = d->m_master_group;
    }
  return d->fetch_glyph(p, character_code, exact_match);
}

fastuidraw::GlyphSelector::FontGroup
fastuidraw::GlyphSelector::
fetch_group(const FontProperties &props, bool exact_match)
{
  FontGroup return_value;
  reference_counted_ptr<font_group> h;

  GlyphSelectorPrivate *d;
  d = static_cast<GlyphSelectorPrivate*>(m_d);

  std::lock_guard<std::mutex> m(d->m_mutex);
  h = d->fetch_font_group_no_lock(props, exact_match);
  return_value.m_d = h.get();

  return return_value;
}

fastuidraw::GlyphSelector::GlyphSource
fastuidraw::GlyphSelector::
fetch_glyph(const FontProperties &props, uint32_t character_code, bool exact_match)
{
  GlyphSelectorPrivate *d;
  reference_counted_ptr<font_group> g;

  d = static_cast<GlyphSelectorPrivate*>(m_d);

  std::lock_guard<std::mutex> m(d->m_mutex);
  g = d->fetch_font_group_no_lock(props, exact_match);
  return d->fetch_glyph(g, character_code, exact_match);
}

fastuidraw::GlyphSelector::GlyphSource
fastuidraw::GlyphSelector::
fetch_glyph(FontGroup h, uint32_t character_code, bool exact_match)
{
  GlyphSource G;
  lock_mutex();
  G = fetch_glyph(h, character_code, exact_match);
  unlock_mutex();
  return G;
}

fastuidraw::GlyphSelector::GlyphSource
fastuidraw::GlyphSelector::
fetch_glyph(reference_counted_ptr<const FontBase> h,
            uint32_t character_code, bool exact_match)
{
  GlyphSource G;
  lock_mutex();
  G = fetch_glyph(h, character_code, exact_match);
  unlock_mutex();
  return G;
}

fastuidraw::GlyphSelector::GlyphSource
fastuidraw::GlyphSelector::
fetch_glyph_no_merging(reference_counted_ptr<const FontBase> h,
                       uint32_t character_code)
{
  GlyphSource G;
  lock_mutex();
  G = fetch_glyph_no_merging(h, character_code);
  unlock_mutex();
  return G;
}
