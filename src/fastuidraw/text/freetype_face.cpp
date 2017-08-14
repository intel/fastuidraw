/*!
 * \file freetype_face.cpp
 * \brief file freetype_face.cpp
 *
 * Copyright 2017 by Intel.
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
#include <utility>
#include <fastuidraw/text/freetype_face.hpp>

namespace
{
  class FreetypeFacePrivate
  {
  public:
    FreetypeFacePrivate(FT_Face pface,
                        const fastuidraw::reference_counted_ptr<fastuidraw::FreetypeLib> &lib);
    ~FreetypeFacePrivate();

    std::mutex m_mutex;
    FT_Face m_face;
    fastuidraw::reference_counted_ptr<fastuidraw::FreetypeLib> m_lib;
  };

  typedef std::pair<std::string, int> GeneratorFilePrivate;
}

/////////////////////////////
// FreetypeFacePrivate methods
FreetypeFacePrivate::
FreetypeFacePrivate(FT_Face pface,
                    const fastuidraw::reference_counted_ptr<fastuidraw::FreetypeLib> &lib):
  m_face(pface),
  m_lib(lib)
{
  FASTUIDRAWassert(m_face);
  FASTUIDRAWassert(m_lib);
}

FreetypeFacePrivate::
~FreetypeFacePrivate()
{
  m_lib->lock();
  FT_Done_Face(m_face);
  m_lib->unlock();
}

//////////////////////////////////////////////////
// fastuidraw::FreetypeFace::GeneratorBase methods
fastuidraw::reference_counted_ptr<fastuidraw::FreetypeFace>
fastuidraw::FreetypeFace::GeneratorBase::
create_face(reference_counted_ptr<FreetypeLib> lib) const
{
  FT_Face face;
  reference_counted_ptr<FreetypeFace> return_value;

  if(!lib)
    {
      lib = FASTUIDRAWnew FreetypeLib();
    }

  lib->lock();
  face = create_face_implement(lib->lib());
  lib->unlock();

  if(face != nullptr)
    {
      return_value = FASTUIDRAWnew FreetypeFace(face, lib);
    }
  return return_value;
}

//////////////////////////////////////////////////
// fastuidraw::FreetypeFace::GeneratorFile methods
fastuidraw::FreetypeFace::GeneratorFile::
GeneratorFile(const char *filename, int face_index)
{
  m_d = FASTUIDRAWnew GeneratorFilePrivate(filename, face_index);
}

fastuidraw::FreetypeFace::GeneratorFile::
~GeneratorFile()
{
  GeneratorFilePrivate *d;
  d = static_cast<GeneratorFilePrivate*>(m_d);
  FASTUIDRAWdelete(d);
}

FT_Face
fastuidraw::FreetypeFace::GeneratorFile::
create_face_implement(FT_Library lib) const
{
  int error_code;
  FT_Face face(nullptr);
  GeneratorFilePrivate *d;

  d = static_cast<GeneratorFilePrivate*>(m_d);
  error_code = FT_New_Face(lib, d->first.c_str(), d->second, &face);
  if(error_code != 0 && face != nullptr)
    {
      FT_Done_Face(face);
      face = nullptr;
    }
  return face;
}

/////////////////////////////
// fastuidraw::FreetypeFace methods
fastuidraw::FreetypeFace::
FreetypeFace(FT_Face pFace,
             const reference_counted_ptr<FreetypeLib> &pLib)
{
  m_d = FASTUIDRAWnew FreetypeFacePrivate(pFace, pLib);
}

fastuidraw::FreetypeFace::
~FreetypeFace()
{
  FreetypeFacePrivate *d;
  d = static_cast<FreetypeFacePrivate*>(m_d);
  FASTUIDRAWdelete(d);
}

void
fastuidraw::FreetypeFace::
lock(void)
{
  FreetypeFacePrivate *d;
  d = static_cast<FreetypeFacePrivate*>(m_d);
  d->m_mutex.lock();
}

void
fastuidraw::FreetypeFace::
unlock(void)
{
  FreetypeFacePrivate *d;
  d = static_cast<FreetypeFacePrivate*>(m_d);
  d->m_mutex.unlock();
}

bool
fastuidraw::FreetypeFace::
try_lock(void)
{
  FreetypeFacePrivate *d;
  d = static_cast<FreetypeFacePrivate*>(m_d);
  return d->m_mutex.try_lock();
}

FT_Face
fastuidraw::FreetypeFace::
face(void)
{
  FreetypeFacePrivate *d;
  d = static_cast<FreetypeFacePrivate*>(m_d);
  return d->m_face;
}

const fastuidraw::reference_counted_ptr<fastuidraw::FreetypeLib>&
fastuidraw::FreetypeFace::
lib(void) const
{
  FreetypeFacePrivate *d;
  d = static_cast<FreetypeFacePrivate*>(m_d);
  return d->m_lib;
}
