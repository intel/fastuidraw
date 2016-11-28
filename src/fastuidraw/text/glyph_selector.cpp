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

#include <fastuidraw/text/glyph_selector.hpp>
#include "../private/util_private.hpp"

namespace
{
  typedef std::pair<fastuidraw::reference_counted_ptr<const fastuidraw::FontBase>, uint32_t> glyph_source;

  class font_group:public fastuidraw::reference_counted<font_group>::non_concurrent
  {
  public:
    explicit
    font_group(fastuidraw::reference_counted_ptr<const font_group> p);

    enum fastuidraw::return_code
    add_font(fastuidraw::reference_counted_ptr<const fastuidraw::FontBase> h);

    glyph_source
    fetch_glyph(uint32_t character_code, enum fastuidraw::glyph_type tp) const;

    const fastuidraw::reference_counted_ptr<const font_group>&
    parent(void) const
    {
      return m_parent;
    }

    fastuidraw::reference_counted_ptr<const fastuidraw::FontBase>
    first_font(void)
    {
      return m_font_set.empty() ?
        fastuidraw::reference_counted_ptr<const fastuidraw::FontBase>() :
        *m_font_set.begin();
    }

  private:
    std::set<fastuidraw::reference_counted_ptr<const fastuidraw::FontBase> > m_font_set;
    fastuidraw::reference_counted_ptr<const font_group> m_parent;
  };

  template<typename key_type>
  class font_group_map:
    public std::map<key_type, fastuidraw::reference_counted_ptr<font_group> >
  {
  public:
    typedef std::map<key_type, fastuidraw::reference_counted_ptr<font_group> > base_class;

    fastuidraw::reference_counted_ptr<font_group>
    get_create(const key_type &key, fastuidraw::reference_counted_ptr<const font_group> parent)
    {
      typename base_class::iterator iter;
      fastuidraw::reference_counted_ptr<font_group> return_value;

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

    fastuidraw::reference_counted_ptr<font_group>
    fetch_group(const key_type &key)
    {
      typename base_class::iterator iter;

      iter = this->find(key);
      if(iter != this->end())
        {
          return iter->second;
        }
      return fastuidraw::reference_counted_ptr<font_group>();
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
    GlyphSelectorPrivate(fastuidraw::reference_counted_ptr<fastuidraw::GlyphCache> h);

    fastuidraw::reference_counted_ptr<font_group>
    fetch_font_group_no_lock(const fastuidraw::FontProperties &prop);

    glyph_source
    fetch_glyph_helper(fastuidraw::reference_counted_ptr<const fastuidraw::FontBase> h,
                       uint32_t character_code, enum fastuidraw::glyph_type tp);

    fastuidraw::Glyph
    fetch_glyph_no_lock(fastuidraw::GlyphRender tp,
                        fastuidraw::reference_counted_ptr<const fastuidraw::FontBase> h,
                        uint32_t character_code);

    fastuidraw::Glyph
    fetch_glyph_no_lock(fastuidraw::GlyphRender tp,
                        fastuidraw::reference_counted_ptr<font_group> group,
                        uint32_t character_code);

    fastuidraw::Glyph
    fetch_glyph_no_merging_no_lock(fastuidraw::GlyphRender tp,
                                   fastuidraw::reference_counted_ptr<const fastuidraw::FontBase> h,
                                   uint32_t character_code);

    fastuidraw::mutex m_mutex;
    fastuidraw::reference_counted_ptr<font_group> m_master_group;
    font_group_map<bold_italic_key> m_bold_italic_groups;
    font_group_map<family_bold_italic_key> m_family_bold_italic_groups;
    font_group_map<style_family_bold_italic_key> m_style_family_bold_italic_groups;
    font_group_map<foundry_style_family_bold_italic_key> m_foundry_style_family_bold_italic_groups;

    fastuidraw::reference_counted_ptr<fastuidraw::GlyphCache> m_cache;
  };
}


///////////////////////////////////
// font_group methods
font_group::
font_group(fastuidraw::reference_counted_ptr<const font_group> parent):
  m_parent(parent)
{
}

enum fastuidraw::return_code
font_group::
add_font(fastuidraw::reference_counted_ptr<const fastuidraw::FontBase> h)
{
  std::pair<std::set<fastuidraw::reference_counted_ptr<const fastuidraw::FontBase> >::iterator, bool> R;
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

glyph_source
font_group::
fetch_glyph(uint32_t character_code, enum fastuidraw::glyph_type tp) const
{
  uint32_t r;

  for(std::set<fastuidraw::reference_counted_ptr<const fastuidraw::FontBase> >::const_iterator
        iter = m_font_set.begin(), end = m_font_set.end();
      iter != end; ++iter)
    {
      if((*iter)->can_create_rendering_data(tp))
        {
          r = (*iter)->glyph_code(character_code);
          if(r)
            {
              return glyph_source(*iter, r);
            }
        }
    }

  if(m_parent)
    {
      return m_parent->fetch_glyph(character_code, tp);
    }

  return glyph_source(fastuidraw::reference_counted_ptr<const fastuidraw::FontBase>(), -1);
}

////////////////////////////////////
// GlyphSelectorPrivate methods
GlyphSelectorPrivate::
GlyphSelectorPrivate(fastuidraw::reference_counted_ptr<fastuidraw::GlyphCache> h):
  m_cache(h)
{
  m_master_group = FASTUIDRAWnew font_group(fastuidraw::reference_counted_ptr<font_group>());
}

fastuidraw::reference_counted_ptr<font_group>
GlyphSelectorPrivate::
fetch_font_group_no_lock(const fastuidraw::FontProperties &prop)
{
  fastuidraw::reference_counted_ptr<font_group> return_value;

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


glyph_source
GlyphSelectorPrivate::
fetch_glyph_helper(fastuidraw::reference_counted_ptr<const fastuidraw::FontBase> h, uint32_t character_code,
                   enum fastuidraw::glyph_type tp)
{
  glyph_source return_value;
  uint32_t r(0);

  if(h->can_create_rendering_data(tp))
    {
      r = h->glyph_code(character_code);
    }

  if(r)
    {
      return_value = glyph_source(h, r);
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
                               fastuidraw::reference_counted_ptr<const fastuidraw::FontBase> h,
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
fetch_glyph_no_lock(fastuidraw::GlyphRender tp,
                    fastuidraw::reference_counted_ptr<const fastuidraw::FontBase> h,
                    uint32_t character_code)
{
  glyph_source src;

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
fetch_glyph_no_lock(fastuidraw::GlyphRender tp,
                    fastuidraw::reference_counted_ptr<font_group> group,
                    uint32_t character_code)
{
  glyph_source src;

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
GlyphSelector(reference_counted_ptr<GlyphCache> p)
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
add_font(reference_counted_ptr<const FontBase> h)
{
  if(!h)
    {
      return;
    }

  GlyphSelectorPrivate *d;
  d = reinterpret_cast<GlyphSelectorPrivate*>(m_d);

  autolock_mutex m(d->m_mutex);

  enum return_code R;
  reference_counted_ptr<font_group> parent;

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

fastuidraw::reference_counted_ptr<const fastuidraw::FontBase>
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
fetch_glyph_no_merging_no_lock(GlyphRender tp,
                               reference_counted_ptr<const FontBase> h,
                               uint32_t character_code)
{
  GlyphSelectorPrivate *d;
  d = reinterpret_cast<GlyphSelectorPrivate*>(m_d);
  return d->fetch_glyph_no_merging_no_lock(tp, h, character_code);
}

fastuidraw::Glyph
fastuidraw::GlyphSelector::
fetch_glyph_no_lock(GlyphRender tp,
                    reference_counted_ptr<const FontBase> h,
                    uint32_t character_code)
{
  GlyphSelectorPrivate *d;
  d = reinterpret_cast<GlyphSelectorPrivate*>(m_d);
  return d->fetch_glyph_no_lock(tp, h, character_code);
}

void
fastuidraw::GlyphSelector::
lock_mutex(void)
{
  GlyphSelectorPrivate *d;
  d = reinterpret_cast<GlyphSelectorPrivate*>(m_d);
  d->m_mutex.lock();
}

void
fastuidraw::GlyphSelector::
unlock_mutex(void)
{
  GlyphSelectorPrivate *d;
  d = reinterpret_cast<GlyphSelectorPrivate*>(m_d);
  d->m_mutex.unlock();
}

fastuidraw::Glyph
fastuidraw::GlyphSelector::
fetch_glyph_no_lock(GlyphRender tp, FontGroup group, uint32_t character_code)
{
  GlyphSelectorPrivate *d;
  d = reinterpret_cast<GlyphSelectorPrivate*>(m_d);

  reference_counted_ptr<font_group> p;
  p = reference_counted_ptr<font_group>(reinterpret_cast<font_group*>(group.m_d));
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
  reference_counted_ptr<font_group> h;

  GlyphSelectorPrivate *d;
  d = reinterpret_cast<GlyphSelectorPrivate*>(m_d);

  autolock_mutex m(d->m_mutex);
  h = d->fetch_font_group_no_lock(props);
  return_value.m_d = h.get();

  return return_value;
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
fetch_glyph(GlyphRender tp, FontGroup h, uint32_t character_code)
{
  Glyph G;
  lock_mutex();
  G = fetch_glyph_no_lock(tp, h, character_code);
  unlock_mutex();
  return G;
}

fastuidraw::Glyph
fastuidraw::GlyphSelector::
fetch_glyph(GlyphRender tp, reference_counted_ptr<const FontBase> h, uint32_t character_code)
{
  Glyph G;
  lock_mutex();
  G = fetch_glyph_no_lock(tp, h, character_code);
  unlock_mutex();
  return G;
}

fastuidraw::Glyph
fastuidraw::GlyphSelector::
fetch_glyph_no_merging(GlyphRender tp, reference_counted_ptr<const FontBase> h, uint32_t character_code)
{
  Glyph G;
  lock_mutex();
  G = fetch_glyph_no_merging_no_lock(tp, h, character_code);
  unlock_mutex();
  return G;
}
