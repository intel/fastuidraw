/*!
 * \file glyph_atlas.hpp
 * \brief file glyph_atlas.hpp
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


#pragma once

#include <fastuidraw/util/reference_counted.hpp>
#include <fastuidraw/util/util.hpp>
#include <fastuidraw/util/vecN.hpp>
#include <fastuidraw/util/c_array.hpp>

namespace fastuidraw
{
/*!\addtogroup PainterBackend
 * @{
 */
  /*!
   * \brief
   * GlyphAtlasStoreBase represents an interface to an aray of
   * generic_data values.
   *
   * An example implementation in GL would be a buffer object that backs
   * a single usamplerBuffer. An implementation of the class does NOT need to be
   * thread safe because the ultimate user of the backing store
   * (GlyphCache) performs calls to the backing store behind its own mutex.
   */
  class GlyphAtlasBackingStoreBase:
    public reference_counted<GlyphAtlasBackingStoreBase>::default_base
  {
  public:
    /*!
     * Ctor.
     * \param psize number of generic_data elements that the
                    GlyphAtlasBackingStoreBase backs
     * \param presizable if true the object can be resized to be larger
     */
    GlyphAtlasBackingStoreBase(unsigned int psize, bool presizable);

    virtual
    ~GlyphAtlasBackingStoreBase();

    /*!
     * Returns the number of \ref generic_data backed by the store.
     */
    unsigned int
    size(void);

    /*!
     * To be implemented by a derived class to load
     * data into the store.
     * \param location to load data
     * \param pdata data to load
     */
    virtual
    void
    set_values(unsigned int location, c_array<const generic_data> pdata) = 0;

    /*!
     * To be implemented by a derived class to flush contents
     * to the backing store.
     */
    virtual
    void
    flush(void) = 0;

    /*!
     * Returns true if and only if this object can be
     * resized to a larger size.
     */
    bool
    resizeable(void) const;

    /*!
     * Resize the object to a larger size. The routine resizeable()
     * must return true, if not the function FASTUIDRAWasserts.
     * \param new_size new number of \ref generic_data for the store to back
     */
    void
    resize(unsigned int new_size);

  protected:

    /*!
     * To be implemented by a derived class to resize the
     * object. When called, the return value of size() is
     * the size before the resize completes.
     * \param new_size new number of \ref generic_data for the store to back
     */
    virtual
    void
    resize_implement(unsigned int new_size) = 0;

  private:
    void *m_d;
  };

  /*!
   * \brief
   * A GlyphAtlas is a common location to place glyph data of
   * an application. Ideally, all glyph data is placed into a
   * single GlyphAtlas. Methods of GlyphAtlas are thread
   * safe, locked behind a mutex of the GlyphAtlas.
   */
  class GlyphAtlas:
    public reference_counted<GlyphAtlas>::default_base
  {
  public:
    /*!
     * Ctor.
     * \param pstore GlyphAtlasBackingStoreBase to which to store  data
     */
    explicit
    GlyphAtlas(reference_counted_ptr<GlyphAtlasBackingStoreBase> pstore);

    virtual
    ~GlyphAtlas();

    /*!
     * Negative return value indicates failure.
     */
    int
    allocate_data(c_array<const generic_data> pdata);

    /*!
     * Deallocate data
     */
    void
    deallocate_data(int location, int count);

    /*!
     * Returns how much  data has been allocated
     */
    unsigned int
    data_allocated(void);

    /*!
     * Frees all allocated regions of this GlyphAtlas;
     */
    void
    clear(void);

    /*!
     * Returns the number of times that clear() has been called.
     */
    unsigned int
    number_times_cleared(void) const;

    /*!
     * Calls GlyphAtlasBackingStoreBase::flush() on
     * the  backing store (see store()).
     */
    void
    flush(void) const;

    /*!
     * Returns the  store for this GlyphAtlas.
     */
    const reference_counted_ptr<const GlyphAtlasBackingStoreBase>&
    store(void) const;

  private:
    void *m_d;
  };
/*! @} */
} //namespace fastuidraw
