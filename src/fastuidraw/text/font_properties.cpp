/*!
 * \file font_properties.cpp
 * \brief file font_properties.cpp
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


#include <string>
#include <fastuidraw/util/fastuidraw_memory.hpp>
#include <fastuidraw/text/font_properties.hpp>

namespace
{
  class FontPropertiesPrivate
  {
  public:
    FontPropertiesPrivate(void):
      m_bold(false),
      m_italic(false)
    {}

    bool m_bold;
    bool m_italic;
    std::string m_style;
    std::string m_family;
    std::string m_foundry;
    std::string m_source_label;
  };
};

////////////////////////////////////////
// fastuidraw::FontProperties methods
fastuidraw::FontProperties::
FontProperties(void)
{
  m_d = FASTUIDRAWnew FontPropertiesPrivate();
}

fastuidraw::FontProperties::
FontProperties(const fastuidraw::FontProperties &obj)
{
  FontPropertiesPrivate *obj_d;
  obj_d = static_cast<FontPropertiesPrivate*>(obj.m_d);
  m_d = FASTUIDRAWnew FontPropertiesPrivate(*obj_d);
}

fastuidraw::FontProperties::
~FontProperties()
{
  FontPropertiesPrivate *d;
  d = static_cast<FontPropertiesPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = nullptr;
}

fastuidraw::FontProperties&
fastuidraw::FontProperties::
operator=(const FontProperties &obj)
{
  FontPropertiesPrivate *obj_d, *d;
  d = static_cast<FontPropertiesPrivate*>(m_d);
  obj_d = static_cast<FontPropertiesPrivate*>(obj.m_d);
  *d = *obj_d;
  return *this;
}

bool
fastuidraw::FontProperties::
bold(void) const
{
  FontPropertiesPrivate *d;
  d = static_cast<FontPropertiesPrivate*>(m_d);
  return d->m_bold;
}

fastuidraw::FontProperties&
fastuidraw::FontProperties::
bold(bool b)
{
  FontPropertiesPrivate *d;
  d = static_cast<FontPropertiesPrivate*>(m_d);
  d->m_bold = b;
  return *this;
}

bool
fastuidraw::FontProperties::
italic(void) const
{
  FontPropertiesPrivate *d;
  d = static_cast<FontPropertiesPrivate*>(m_d);
  return d->m_italic;
}

fastuidraw::FontProperties&
fastuidraw::FontProperties::
italic(bool b)
{
  FontPropertiesPrivate *d;
  d = static_cast<FontPropertiesPrivate*>(m_d);
  d->m_italic = b;
  return *this;
}

const char*
fastuidraw::FontProperties::
style(void) const
{
  FontPropertiesPrivate *d;
  d = static_cast<FontPropertiesPrivate*>(m_d);
  return d->m_style.c_str();
}

fastuidraw::FontProperties&
fastuidraw::FontProperties::
style(const char* b)
{
  FontPropertiesPrivate *d;
  d = static_cast<FontPropertiesPrivate*>(m_d);
  d->m_style = b ? b : "";
  return *this;
}

const char*
fastuidraw::FontProperties::
family(void) const
{
  FontPropertiesPrivate *d;
  d = static_cast<FontPropertiesPrivate*>(m_d);
  return d->m_family.c_str();
}

fastuidraw::FontProperties&
fastuidraw::FontProperties::
family(const char* b)
{
  FontPropertiesPrivate *d;
  d = static_cast<FontPropertiesPrivate*>(m_d);
  d->m_family = b ? b : "";
  return *this;
}

const char*
fastuidraw::FontProperties::
foundry(void) const
{
  FontPropertiesPrivate *d;
  d = static_cast<FontPropertiesPrivate*>(m_d);
  return d->m_foundry.c_str();
}

fastuidraw::FontProperties&
fastuidraw::FontProperties::
foundry(const char* b)
{
  FontPropertiesPrivate *d;
  d = static_cast<FontPropertiesPrivate*>(m_d);
  d->m_foundry = b ? b : "";
  return *this;
}

const char*
fastuidraw::FontProperties::
source_label(void) const
{
  FontPropertiesPrivate *d;
  d = static_cast<FontPropertiesPrivate*>(m_d);
  return d->m_source_label.c_str();
}

fastuidraw::FontProperties&
fastuidraw::FontProperties::
source_label(const char* b)
{
  FontPropertiesPrivate *d;
  d = static_cast<FontPropertiesPrivate*>(m_d);
  d->m_source_label = b ? b : "";
  return *this;
}
