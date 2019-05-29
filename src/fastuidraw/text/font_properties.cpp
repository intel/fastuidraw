/*!
 * \file font_properties.cpp
 * \brief file font_properties.cpp
 *
 * Copyright 2016 by Intel.
 *
 * Contact: kevin.rogovin@gmail.com
 *
 * This Source Code Form is subject to the
 * terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with
 * this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 *
 * \author Kevin Rogovin <kevin.rogovin@gmail.com>
 *
 */


#include <string>
#include <sstream>
#include <fastuidraw/util/fastuidraw_memory.hpp>
#include <fastuidraw/text/font_properties.hpp>
#include <private/util_private.hpp>

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

assign_swap_implement(fastuidraw::FontProperties)
setget_implement(fastuidraw::FontProperties,
                 FontPropertiesPrivate,
                 bool,
                 bold)
setget_implement(fastuidraw::FontProperties,
                 FontPropertiesPrivate,
                 bool,
                 italic)
setget_implement_string(fastuidraw::FontProperties,
                        FontPropertiesPrivate,
                        style)
setget_implement_string(fastuidraw::FontProperties,
                        FontPropertiesPrivate,
                        family)
setget_implement_string(fastuidraw::FontProperties,
                        FontPropertiesPrivate,
                        foundry)
setget_implement_string(fastuidraw::FontProperties,
                        FontPropertiesPrivate,
                        source_label)

fastuidraw::FontProperties&
fastuidraw::FontProperties::
source_label(c_string filename, int face_index)
{
  FontPropertiesPrivate *d;
  std::ostringstream str;

  d = static_cast<FontPropertiesPrivate*>(m_d);
  str << filename << ":" << face_index;
  d->m_source_label = str.str();

  return *this;
}
