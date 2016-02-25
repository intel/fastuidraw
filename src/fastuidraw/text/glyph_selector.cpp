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


#include <set>
#include <map>

#include <boost/thread.hpp>
#include <boost/tuple/tuple.hpp>
#include <boost/tuple/tuple_comparison.hpp>
#include <fastuidraw/text/glyph_selector.hpp>
#include "../private/util_private.hpp"

namespace
{
  class font_group:public fastuidraw::reference_counted<font_group>::non_concurrent
  {
  public:
    explicit
    font_group(font_group::const_handle p);

    enum fastuidraw::return_code
    add_font(fastuidraw::FontBase::const_handle h);

    std::pair<fastuidraw::FontBase::const_handle, uint32_t>
    fetch_glyph(uint32_t character_code, enum fastuidraw::glyph_type tp) const;

    const font_group::const_handle&
    parent(void) const
    {
      return m_parent;
    }

    fastuidraw::FontBase::const_handle
    first_font(void)
    {
      return m_font_set.empty() ?
        fastuidraw::FontBase::const_handle() :
        *m_font_set.begin();
    }

  private:
    std::set<fastuidraw::FontBase::const_handle> m_font_set;
    font_group::const_handle m_parent;
  };

  template<typename key_type>
  class font_group_map:public std::map<key_type, font_group::handle>
  {
  public:
    font_group::handle
    get_create(const key_type &key, font_group::const_handle parent)
    {
      typename std::map<key_type, font_group::handle>::iterator iter;
      font_group::handle return_value;

      iter = this->find(key);
      if(iter == this->end())
        {
          return_value = FASTUIDRAWnew font_group(parent);
          this->operator[](key) = return_value;
        }
      else
        {
          return_value = iter->second;
          assert(return_value->parent() == parent);
        }
      return return_value;
    }

    font_group::handle
    fetch_group(const key_type &key)
    {
      typename std::map<key_type, font_group::handle>::iterator iter;

      iter = this->find(key);
      if(iter != this->end())
        {
          return iter->second;
        }
      return font_group::handle();
    }
  };

  class bold_italic_key:
    public std::pair<bool, bool>
  {
  public:
    bold_italic_key(const fastuidraw::FontProperties &prop):
      std::pair<bool, bool>(prop.bold(), prop.italic())
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

  class style_family_bold_italic_key:
    public std::pair<std::string, family_bold_italic_key>
  {
  public:
    style_family_bold_italic_key(const fastuidraw::FontProperties &prop):
      std::pair<std::string, family_bold_italic_key>(prop.style(), prop)
    {}
  };

  class foundry_style_family_bold_italic_key:
    public std::pair<std::string, style_family_bold_italic_key>
  {
  public:
    foundry_style_family_bold_italic_key(const fastuidraw::FontProperties &prop):
      std::pair<std::string, style_family_bold_italic_key>(prop.foundry(), prop)
    {}
  };

  class GlyphSelectorPrivate
  {
  public:
    GlyphSelectorPrivate(fastuidraw::GlyphCache::handle h);

    font_group::handle
    fetch_font_group_no_lock(const fastuidraw::FontProperties &prop);

    std::pair<fastuidraw::FontBase::const_handle, uint32_t>
    fetch_glyph_helper(fastuidraw::FontBase::const_handle h,
                       uint32_t character_code, enum fastuidraw::glyph_type tp);

    fastuidraw::Glyph
    fetch_glyph_no_lock(fastuidraw::GlyphRender tp,
                        fastuidraw::FontBase::const_handle h,
                        uint32_t character_code);

    fastuidraw::Glyph
    fetch_glyph_no_lock(fastuidraw::GlyphRender tp, font_group::handle group,
                        uint32_t character_code);

    fastuidraw::Glyph
    fetch_glyph_no_merging_no_lock(fastuidraw::GlyphRender tp,
                                   fastuidraw::FontBase::const_handle h,
                                   uint32_t character_code);

    boost::mutex m_mutex;
    font_group::handle m_master_group;
    font_group_map<bold_italic_key> m_bold_italic_groups;
    font_group_map<family_bold_italic_key> m_family_bold_italic_groups;
    font_group_map<style_family_bold_italic_key> m_style_family_bold_italic_groups;
    font_group_map<foundry_style_family_bold_italic_key> m_foundry_style_family_bold_italic_groups;

    fastuidraw::GlyphCache::handle m_cache;
  };
}


///////////////////////////////////
// font_group methods
font_group::
font_group(const_handle parent):
  m_parent(parent)
{
}

enum fastuidraw::return_code
font_group::
add_font(fastuidraw::FontBase::const_handle h)
{
  std::pair<std::set<fastuidraw::FontBase::const_handle>::iterator, bool> R;
  R = m_font_set.insert(h);

  if(R.second)
    {
      return fastuidraw::routine_success;
    }
  else
    {
      return fastuidraw::routine_fail;
    }
}

std::pair<fastuidraw::FontBase::const_handle, uint32_t>
font_group::
fetch_glyph(uint32_t character_code, enum fastuidraw::glyph_type tp) const
{
  uint32_t r;

  for(std::set<fastuidraw::FontBase::const_handle>::const_iterator
        iter = m_font_set.begin(), end = m_font_set.end();
      iter != end; ++iter)
    {
      if((*iter)->can_create_rendering_data(tp))
        {
          r = (*iter)->glyph_code(character_code);
          if(r)
            {
              return std::pair<fastuidraw::FontBase::const_handle, uint32_t>(*iter, r);
            }
        }
    }

  if(m_parent)
    {
      return m_parent->fetch_glyph(character_code, tp);
    }

  return std::pair<fastuidraw::FontBase::const_handle, uint32_t>(fastuidraw::FontBase::const_handle(), -1);
}

////////////////////////////////////
// GlyphSelectorPrivate methods
GlyphSelectorPrivate::
GlyphSelectorPrivate(fastuidraw::GlyphCache::handle h):
  m_cache(h)
{
  m_master_group = FASTUIDRAWnew font_group(font_group::handle());
}

font_group::handle
GlyphSelectorPrivate::
fetch_font_group_no_lock(const fastuidraw::FontProperties &prop)
{
  font_group::handle return_value;

  return_value = m_foundry_style_family_bold_italic_groups.fetch_group(prop);
  if(return_value)
    {
      return return_value;
    }

  return_value = m_style_family_bold_italic_groups.fetch_group(prop);
  if(return_value)
    {
      return return_value;
    }

  return_value = m_family_bold_italic_groups.fetch_group(prop);
  if(return_value)
    {
      return return_value;
    }

  return_value = m_bold_italic_groups.fetch_group(prop);
  if(return_value)
    {
      return return_value;
    }

  return m_master_group;
}


std::pair<fastuidraw::FontBase::const_handle, uint32_t>
GlyphSelectorPrivate::
fetch_glyph_helper(fastuidraw::FontBase::const_handle h, uint32_t character_code,
                   enum fastuidraw::glyph_type tp)
{
  std::pair<fastuidraw::FontBase::const_handle, uint32_t> return_value;
  uint32_t r(0);

  if(h->can_create_rendering_data(tp))
    {
      r = h->glyph_code(character_code);
    }

  if(r)
    {
      return_value = std::pair<fastuidraw::FontBase::const_handle, uint32_t>(h, r);
    }
  else
    {
      font_group_map<foundry_style_family_bold_italic_key>::const_iterator iter;
      iter = m_foundry_style_family_bold_italic_groups.find(h->properties());
      if(iter != m_foundry_style_family_bold_italic_groups.end())
        {
          return_value = iter->second->fetch_glyph(character_code, tp);
        }
    }
  return return_value;
}

fastuidraw::Glyph
GlyphSelectorPrivate::
fetch_glyph_no_merging_no_lock(fastuidraw::GlyphRender tp,
                               fastuidraw::FontBase::const_handle h,
                               uint32_t character_code)
{
  if(!h || !tp.valid())
    {
      return fastuidraw::Glyph();
    }

  fastuidraw::Glyph return_value;
  uint32_t glyph_code(0);

  if(h->can_create_rendering_data(tp.m_type))
    {
      glyph_code = h->glyph_code(character_code);
    }

  if(glyph_code)
    {
      return_value = m_cache->fetch_glyph(tp, h, glyph_code);
    }

  return return_value;
}


fastuidraw::Glyph
GlyphSelectorPrivate::
fetch_glyph_no_lock(fastuidraw::GlyphRender tp, fastuidraw::FontBase::const_handle h,
                    uint32_t character_code)
{
  std::pair<fastuidraw::FontBase::const_handle, uint32_t> src;

  if(!h || !h->can_create_rendering_data(tp.m_type))
    {
      return fastuidraw::Glyph();
    }

  src = fetch_glyph_helper(h, character_code, tp.m_type);
  if(src.first)
    {
      return m_cache->fetch_glyph(tp, src.first, src.second);
    }
  else
    {
      return fastuidraw::Glyph();
    }
}

fastuidraw::Glyph
GlyphSelectorPrivate::
fetch_glyph_no_lock(fastuidraw::GlyphRender tp, font_group::handle group, uint32_t character_code)
{
  std::pair<fastuidraw::FontBase::const_handle, uint32_t> src;

  assert(group);
  src = group->fetch_glyph(character_code, tp.m_type);
  if(src.first)
    {
      return m_cache->fetch_glyph(tp, src.first, src.second);
    }
  else
    {
      return fastuidraw::Glyph();
    }
}

////////////////////////////////////////////////
// fastuidraw::GlyphSelector methods
fastuidraw::GlyphSelector::
GlyphSelector(GlyphCache::handle p)
{
  m_d = FASTUIDRAWnew GlyphSelectorPrivate(p);
}

fastuidraw::GlyphSelector::
~GlyphSelector()
{
  GlyphSelectorPrivate *d;
  d = reinterpret_cast<GlyphSelectorPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = NULL;
}

void
fastuidraw::GlyphSelector::
add_font(FontBase::const_handle h)
{
  GlyphSelectorPrivate *d;
  d = reinterpret_cast<GlyphSelectorPrivate*>(m_d);

  autolock_mutex m(d->m_mutex);

  enum return_code R;
  font_group::handle parent;

  parent = d->m_master_group;
  R = parent->add_font(h);
  if(R == routine_success)
    {
      parent = d->m_bold_italic_groups.get_create(h->properties(), parent);
      parent->add_font(h);

      parent = d->m_family_bold_italic_groups.get_create(h->properties(), parent);
      parent->add_font(h);

      parent = d->m_style_family_bold_italic_groups.get_create(h->properties(), parent);
      parent->add_font(h);

      parent = d->m_foundry_style_family_bold_italic_groups.get_create(h->properties(), parent);
      parent->add_font(h);
    }
}

fastuidraw::FontBase::const_handle
fastuidraw::GlyphSelector::
fetch_font(const FontProperties &prop)
{
  GlyphSelectorPrivate *d;
  d = reinterpret_cast<GlyphSelectorPrivate*>(m_d);

  autolock_mutex m(d->m_mutex);
  return d->fetch_font_group_no_lock(prop)->first_font();
}

fastuidraw::Glyph
fastuidraw::GlyphSelector::
fetch_glyph_no_merging(GlyphRender tp, FontBase::const_handle h, uint32_t character_code)
{
  GlyphSelectorPrivate *d;
  d = reinterpret_cast<GlyphSelectorPrivate*>(m_d);

  autolock_mutex m(d->m_mutex);
  return d->fetch_glyph_no_merging_no_lock(tp, h, character_code);
}

fastuidraw::Glyph
fastuidraw::GlyphSelector::
fetch_glyph(GlyphRender tp, FontBase::const_handle h, uint32_t character_code)
{
  GlyphSelectorPrivate *d;
  d = reinterpret_cast<GlyphSelectorPrivate*>(m_d);

  autolock_mutex m(d->m_mutex);
  return d->fetch_glyph_no_lock(tp, h, character_code);
}

fastuidraw::Glyph
fastuidraw::GlyphSelector::
fetch_glyph(GlyphRender tp, const FontProperties &props, uint32_t character_code)
{
  GlyphSelectorPrivate *d;
  d = reinterpret_cast<GlyphSelectorPrivate*>(m_d);

  autolock_mutex m(d->m_mutex);
  return d->fetch_glyph_no_lock(tp, d->fetch_font_group_no_lock(props), character_code);
}

fastuidraw::Glyph
fastuidraw::GlyphSelector::
fetch_glyph(GlyphRender tp, FontGroup group, uint32_t character_code)
{
  GlyphSelectorPrivate *d;
  d = reinterpret_cast<GlyphSelectorPrivate*>(m_d);

  font_group::handle p;
  p = font_group::handle(reinterpret_cast<font_group*>(group.m_d));
  if(!p)
    {
      p = d->m_master_group;
    }
  return d->fetch_glyph_no_lock(tp, p, character_code);
}

fastuidraw::GlyphSelector::FontGroup
fastuidraw::GlyphSelector::
fetch_group(const FontProperties &props)
{
  FontGroup return_value;
  font_group::handle h;

  GlyphSelectorPrivate *d;
  d = reinterpret_cast<GlyphSelectorPrivate*>(m_d);

  autolock_mutex m(d->m_mutex);
  h = d->fetch_font_group_no_lock(props);
  return_value.m_d = h.get();

  return return_value;
}
