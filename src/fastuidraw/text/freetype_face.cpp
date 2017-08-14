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
  class FreeTypeFacePrivate
  {
  public:
    FreeTypeFacePrivate(FT_Face pface,
                        const fastuidraw::reference_counted_ptr<fastuidraw::FreeTypeLib> &lib);
    ~FreeTypeFacePrivate();

    std::mutex m_mutex;
    FT_Face m_face;
    fastuidraw::reference_counted_ptr<fastuidraw::FreeTypeLib> m_lib;
  };

  typedef std::pair<std::string, int> GeneratorFilePrivate;
}

/////////////////////////////
// FreeTypeFacePrivate methods
FreeTypeFacePrivate::
FreeTypeFacePrivate(FT_Face pface,
                    const fastuidraw::reference_counted_ptr<fastuidraw::FreeTypeLib> &lib):
  m_face(pface),
  m_lib(lib)
{
  FASTUIDRAWassert(m_face);
  FASTUIDRAWassert(m_lib);
}

FreeTypeFacePrivate::
~FreeTypeFacePrivate()
{
  m_lib->lock();
  FT_Done_Face(m_face);
  m_lib->unlock();
}

//////////////////////////////////////////////////
// fastuidraw::FreeTypeFace::GeneratorBase methods
fastuidraw::reference_counted_ptr<fastuidraw::FreeTypeFace>
fastuidraw::FreeTypeFace::GeneratorBase::
create_face(reference_counted_ptr<FreeTypeLib> lib) const
{
  FT_Face face;
  reference_counted_ptr<FreeTypeFace> return_value;

  if(!lib)
    {
      lib = FASTUIDRAWnew FreeTypeLib();
    }

  lib->lock();
  face = create_face_implement(lib->lib());
  lib->unlock();

  if(face != nullptr)
    {
      return_value = FASTUIDRAWnew FreeTypeFace(face, lib);
    }
  return return_value;
}

//////////////////////////////////////////////////
// fastuidraw::FreeTypeFace::GeneratorFile methods
fastuidraw::FreeTypeFace::GeneratorFile::
GeneratorFile(const char *filename, int face_index)
{
  m_d = FASTUIDRAWnew GeneratorFilePrivate(filename, face_index);
}

fastuidraw::FreeTypeFace::GeneratorFile::
~GeneratorFile()
{
  GeneratorFilePrivate *d;
  d = static_cast<GeneratorFilePrivate*>(m_d);
  FASTUIDRAWdelete(d);
}

FT_Face
fastuidraw::FreeTypeFace::GeneratorFile::
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
// fastuidraw::FreeTypeFace methods
fastuidraw::FreeTypeFace::
FreeTypeFace(FT_Face pFace,
             const reference_counted_ptr<FreeTypeLib> &pLib)
{
  m_d = FASTUIDRAWnew FreeTypeFacePrivate(pFace, pLib);
}

fastuidraw::FreeTypeFace::
~FreeTypeFace()
{
  FreeTypeFacePrivate *d;
  d = static_cast<FreeTypeFacePrivate*>(m_d);
  FASTUIDRAWdelete(d);
}

void
fastuidraw::FreeTypeFace::
lock(void)
{
  FreeTypeFacePrivate *d;
  d = static_cast<FreeTypeFacePrivate*>(m_d);
  d->m_mutex.lock();
}

void
fastuidraw::FreeTypeFace::
unlock(void)
{
  FreeTypeFacePrivate *d;
  d = static_cast<FreeTypeFacePrivate*>(m_d);
  d->m_mutex.unlock();
}

bool
fastuidraw::FreeTypeFace::
try_lock(void)
{
  FreeTypeFacePrivate *d;
  d = static_cast<FreeTypeFacePrivate*>(m_d);
  return d->m_mutex.try_lock();
}

FT_Face
fastuidraw::FreeTypeFace::
face(void)
{
  FreeTypeFacePrivate *d;
  d = static_cast<FreeTypeFacePrivate*>(m_d);
  return d->m_face;
}

const fastuidraw::reference_counted_ptr<fastuidraw::FreeTypeLib>&
fastuidraw::FreeTypeFace::
lib(void) const
{
  FreeTypeFacePrivate *d;
  d = static_cast<FreeTypeFacePrivate*>(m_d);
  return d->m_lib;
}
