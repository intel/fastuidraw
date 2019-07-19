/*!
 * \file freetype_face.hpp
 * \brief file freetype_face.hpp
 *
 * Copyright 2017 by Intel.
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


#ifndef FASTUIDRAW_FREETYPE_FACE_HPP
#define FASTUIDRAW_FREETYPE_FACE_HPP

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H

#include <fastuidraw/util/data_buffer.hpp>
#include <fastuidraw/text/freetype_lib.hpp>

namespace fastuidraw
{
/*!\addtogroup Glyph
 * @{
 */
  /*!\brief
   * A FreeTypeFace wraps an FT_Face object of the FreeType library
   * together with a mutex in a reference counted object.
   *
   * The threading model for the FreeType appears to be:
   * - Create an FT_Library object
   * - When creating or releasing FT_Face objects, lock a mutex
   *   around the FT_Library when doing so
   * - If an FT_Face is accessed from multiple threads, the FT_Face
   *   (but not the FT_Library) needs to be mutex locked
   */
  class FreeTypeFace:public reference_counted<FreeTypeFace>::concurrent
  {
  public:
    /*!\brief
     * A Generator provides an interface to create FreeTypeFace
     * objects.
     */
    class GeneratorBase:public reference_counted<GeneratorBase>::concurrent
    {
    public:
      virtual
      ~GeneratorBase()
      {}

      /*!
       * Public interface to create a FreeTypeFace object.
       * \param lib FreeTypeLib with which to create the FT_Face;
       *            if lib is nullptr, then lib will be sustituted
       *            with a newly created FreeTypeLib object that
       *            only the returned FreeTypeFace will use.
       */
      virtual
      reference_counted_ptr<FreeTypeFace>
      create_face(reference_counted_ptr<FreeTypeLib> lib
                  = reference_counted_ptr<FreeTypeLib>()) const;

      /*!
       * Checks if the GeneratorBase object can create a face
       * (by calling create_face_implement()). Returns routine_fail
       * if the object is unable to create a face.
       * \param lib FreeTypeLib with which to test face creation;
       *            if lib is nullptr, then lib will be sustituted
       *            with a newly created FreeTypeLib object that
       *            only the returned FreeTypeFace will use.
       */
      enum return_code
      check_creation(reference_counted_ptr<FreeTypeLib> lib
                     = reference_counted_ptr<FreeTypeLib>()) const;

    protected:
      /*!
       * To be implemented by a derived class to create a
       * FT_Face using a given (and locked by the caller)
       * FT_Library object.
       * \param lib FT_Libray with which to create the FT_Face
       */
      virtual
      FT_Face
      create_face_implement(FT_Library lib) const = 0;
    };

    /*!
     * \brief Implementation of GeneratorBase to create a FreeTypeFace
     *        from a face index / file pair via lib FreeType's FT_New_Face
     */
    class GeneratorFile:public GeneratorBase
    {
    public:
      /*!
       * Ctor.
       * \param filename name of file from which to source the created
       *                 FT_Face objects
       * \param face_index face index of file
       */
      GeneratorFile(c_string filename, int face_index);
      ~GeneratorFile();

    protected:
      virtual
      FT_Face
      create_face_implement(FT_Library lib) const;

    private:
      void *m_d;
    };

    /*!
     * \brief Implementation of GeneratorBase to create a FreeTypeFace
     *        from a face index / file pair via lib FreeType's
     *       FT_New_Memory_Face
     */
    class GeneratorMemory:public GeneratorBase
    {
    public:
      /*!
       * Ctor.
       * \param src holder of data; modifying the data after creating
       *            a GeneratorMemory that uses it is undefined and
       *            crashing behavior.
       * \param face_index face index of data
       */
      GeneratorMemory(const reference_counted_ptr<const DataBufferBase> &src,
                      int face_index);

      /*!
       * Ctor. Provided as a convenience, a DataBuffer object is created
       * from the named file and used as the memory source.
       * \param filename name of file from which to source the created
       *                 FT_Face objects
       * \param face_index face index of file
       */
      GeneratorMemory(c_string filename, int face_index);

      ~GeneratorMemory();

    protected:
      virtual
      FT_Face
      create_face_implement(FT_Library lib) const;

    private:
      void *m_d;
    };

    /*!
     * Ctor.
     * \param pFace the underlying FT_Face; the created FreeTypeFace
     *        takes ownership of pFace and pFace will be deleted when
     *        the created FreeTypeFace is deleted.
     * \param pLib the FreeTypeLib that was used to create pFace
     */
    FreeTypeFace(FT_Face pFace,
                 const reference_counted_ptr<FreeTypeLib> &pLib);

    ~FreeTypeFace();

    /*!
     * Returns the FT_Face object about which
     * this object wraps.
     */
    FT_Face
    face(void);

    /*!
     * Returns the FreeTypeLib that was used to
     * create the FT_Face face().
     */
    const reference_counted_ptr<FreeTypeLib>&
    lib(void) const;

    /*!
     * Aquire the lock of the mutex used to access/use the FT_Face
     * return by face() safely across multiple threads.
     */
    void
    lock(void);

    /*!
     * Release the lock of the mutex used to access/use the FT_Face
     * return by face() safely across multiple threads.
     */
    void
    unlock(void);

    /*!
     * Try to aquire the lock of the mutex. Return true on success.
     */
    bool
    try_lock(void);

  private:
    void *m_d;
  };
/*! @} */
}

#endif
