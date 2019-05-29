/*!
 * \file gl_context_properties.cpp
 * \brief file gl_context_properties.cpp
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
#include <set>

#include <fastuidraw/util/fastuidraw_memory.hpp>
#include <fastuidraw/gl_backend/gl_context_properties.hpp>
#include <fastuidraw/gl_backend/ngl_header.hpp>
#include <fastuidraw/gl_backend/gl_get.hpp>

namespace
{
  class ContextPropertiesPrivate
  {
  public:
    explicit
    ContextPropertiesPrivate(bool b);

    void
    make_version_ready(void);

    void
    make_extensions_ready(void);

    bool m_version_ready, m_extensions_ready;
    bool m_is_es;
    fastuidraw::vecN<int, 2> m_version;
    std::set<std::string> m_extensions;
  };
}

//////////////////////////////////////
// ContextPropertiesPrivate
ContextPropertiesPrivate::
ContextPropertiesPrivate(bool make_ready):
  m_version_ready(false),
  m_extensions_ready(false)
{
  #ifdef FASTUIDRAW_GL_USE_GLES
    {
      m_is_es = true;
    }
  #else
    {
      m_is_es = false;
    }
  #endif

  if (make_ready)
    {
      make_version_ready();
      make_extensions_ready();
    }
}

void
ContextPropertiesPrivate::
make_version_ready(void)
{
  if (m_version_ready)
    {
      return;
    }

  m_version_ready = true;
  m_version[0] = fastuidraw::gl::context_get<GLint>(GL_MAJOR_VERSION);
  m_version[1] = fastuidraw::gl::context_get<GLint>(GL_MINOR_VERSION);
}

void
ContextPropertiesPrivate::
make_extensions_ready(void)
{
  if (m_extensions_ready)
    {
      return;
    }

  m_extensions_ready = true;

  int cnt;
  cnt = fastuidraw::gl::context_get<GLint>(GL_NUM_EXTENSIONS);
  for(int i = 0; i < cnt; ++i)
    {
      fastuidraw::c_string ext;
      ext = reinterpret_cast<fastuidraw::c_string>(fastuidraw_glGetStringi(GL_EXTENSIONS, i));
      m_extensions.insert(ext);
    }
}

/////////////////////////////////////////////
// fastuidraw::gl::ContextProperties methods
fastuidraw::gl::ContextProperties::
ContextProperties(bool make_ready)
{
  m_d = FASTUIDRAWnew ContextPropertiesPrivate(make_ready);
}

fastuidraw::gl::ContextProperties::
~ContextProperties()
{
  ContextPropertiesPrivate *d;
  d = static_cast<ContextPropertiesPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = nullptr;
}

fastuidraw::vecN<int, 2>
fastuidraw::gl::ContextProperties::
version(void) const
{
  ContextPropertiesPrivate *d;
  d = static_cast<ContextPropertiesPrivate*>(m_d);
  d->make_version_ready();
  return d->m_version;
}

bool
fastuidraw::gl::ContextProperties::
is_es(void) const
{
  ContextPropertiesPrivate *d;
  d = static_cast<ContextPropertiesPrivate*>(m_d);
  return d->m_is_es;
}

bool
fastuidraw::gl::ContextProperties::
has_extension(fastuidraw::c_string ext) const
{
  ContextPropertiesPrivate *d;
  d = static_cast<ContextPropertiesPrivate*>(m_d);
  d->make_extensions_ready();
  return ext && d->m_extensions.find(std::string(ext)) != d->m_extensions.end();
}
