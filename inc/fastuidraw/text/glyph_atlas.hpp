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
#include <fastuidraw/text/glyph_location.hpp>

namespace fastuidraw
{
/*!\addtogroup Text
 * @{
 */

  /*!
   * \brief
   * GlyphAtlasTexelBackingStoreBase represents an interface for a backing
   * store for one-channel 8-bit data (for glyphs essentially).
   *
   * The values stored can be coverage values, distance values or index values.
   * Index values are to be fetched unfiltered and other values filtered
   * (but NO mipmap filtering). An implementation of the class does NOT
   * need to be thread safe because the user of the backing store (GlyphAtlas)
   * performs calls to the backing store behind its own mutex.
   */
  class GlyphAtlasTexelBackingStoreBase:
    public reference_counted<GlyphAtlasTexelBackingStoreBase>::default_base
  {
  public:
    enum
      {
        /*!
         * log2 of \ref max_size
         */
        log2_max_size = 10,

        /*!
         * The maximum allowed size in each dimension for a backing store.
         */
        max_size = 1 << log2_max_size
      };

    /*!
     * Ctor.
     * \param whl provides the dimensions of the GlyphAtlasBackingStoreBase
     * \param presizable if true the object can be resized to be larger;
     *                   the size of any dimension can be no more than
     *                   \ref max_size
     */
    GlyphAtlasTexelBackingStoreBase(ivec3 whl, bool presizable);

    /*!
     * Ctor.
     * \param w width of the backing store, must be no more than \ref max_size
     * \param h height of the backing store, must be no more than \ref max_size
     * \param l number of layers of the backing store, must be no more than \ref max_size
     * \param presizable if true the object can be resized to be larger
     */
    GlyphAtlasTexelBackingStoreBase(int w, int h, int l, bool presizable);

    virtual
    ~GlyphAtlasTexelBackingStoreBase();

    /*!
     * To be implemented by a derived class to set color data into the backing store.
     * \param x horizontal position
     * \param y vertical position
     * \param l layer of position
     * \param w width of data
     * \param h height of data
     * \param data 8-bit values
     */
    virtual
    void
    set_data(int x, int y, int l, int w, int h,
             c_array<const uint8_t> data)=0;

    /*!
     * To be implemented by a derived class
     * to flush set_data() to the backing
     * store.
     */
    virtual
    void
    flush(void) = 0;

    /*!
     * Returns the dimensions of the backing store
     * (as passed in the ctor).
     */
    ivec3
    dimensions(void) const;

    /*!
     * Returns true if and only if this object can be
     * resized to a larger size.
     */
    bool
    resizeable(void) const;

    /*!
     * Resize the object by increasing the number of layers.
     * The routine resizeable() must return true, if not
     * the function FASTUIDRAWasserts.
     * \param new_num_layers new number of layers for the backing
     *                       store to have, must be no greater than
     *                       \ref max_size
     */
    void
    resize(int new_num_layers);

  private:

    /*!
     * To be implemented by a derived class to resize the
     * object. The resize changes ONLY the number of layers
     * of the object and only increases the value as well.
     * When called, the return value of dimensions() is
     * the size before the resize completes.
     * \param new_num_layers new number of layers to which
     *                       to resize the underlying store.
     */
    virtual
    void
    resize_implement(int new_num_layers) = 0;

  private:
    void *m_d;
  };

  /*!
   * \brief
   * GlyphAtlasGeometryStoreBase represents an interface to an aray of
   * generic_data values.
   *
   * An example implementation in GL would be a buffer object that used
   * to back simultaneously a samplerBuffer, usamplerBuffer and an
   * isamplerBuffer. An implementation of the class does NOT need to be
   * thread safe because the user of the backing store (GlyphAtlas)
   * performs calls to the backing store behind its own mutex.
   */
  class GlyphAtlasGeometryBackingStoreBase:
    public reference_counted<GlyphAtlasGeometryBackingStoreBase>::default_base
  {
  public:
    /*!
     * Ctor.
     * \param palignment Specifies the alignment in units of generic_data for
     *                   packing of seperately accessible entries in the backing
     *                   store.
     * \param psize number of blocks, where each block is palignment generic_data
     *              in size, that GlyphAtlasGeometryBackingStoreBase backs
     * \param presizable if true the object can be resized to be larger
     */
    GlyphAtlasGeometryBackingStoreBase(unsigned int palignment, unsigned int psize,
                                       bool presizable);

    virtual
    ~GlyphAtlasGeometryBackingStoreBase();

    /*!
     * Returns the number of blocks, where each block is
     * alignment() generic_data values in size, the store
     * holds.
     */
    unsigned int
    size(void);

    /*!
     * Set at ctor. Provides the alignment of the store.
     */
    unsigned int
    alignment(void) const;

    /*!
     * To be implemented by a derived class to load
     * data into the store.
     * \param location given in units of blocks, where each block
     *        is alignment() generic_data values.
     * \param pdata data to load, must be a multiple of alignment().
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
     * \param new_size new size of object in number of blocks.
     */
    void
    resize(unsigned int new_size);

  protected:

    /*!
     * To be implemented by a derived class to resize the
     * object. When called, the return value of size() is
     * the size before the resize completes.
     * \param new_size new size in number of blocks
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
     * \brief
     * A Padding object holds how much of the data allocated
     * by \ref GlyphAtlas::allocate() is for padding.
     */
    class Padding
    {
    public:
      Padding(void):
        m_left(0),
        m_right(0),
        m_top(0),
        m_bottom(0)
      {}

      /*!
       * Padding to the left
       */
      unsigned int m_left;

      /*!
       * Padding to the right
       */
      unsigned int m_right;

      /*!
       * Padding to the top (y=0 is the top of a glyph)
       */
      unsigned int m_top;

      /*!
       * Padding to the bottom
       */
      unsigned int m_bottom;
    };

    /*!
     * Ctor.
     * \param ptexel_store GlyphAtlasTexelBackingStoreBase to which to store texel data
     * \param pgeometry_store GlyphAtlasGeometryBackingStoreBase to which to store geometry data
     */
    GlyphAtlas(reference_counted_ptr<GlyphAtlasTexelBackingStoreBase> ptexel_store,
               reference_counted_ptr<GlyphAtlasGeometryBackingStoreBase> pgeometry_store);

    virtual
    ~GlyphAtlas();

    /*!
     * Allocate a rectangular region. If allocation is not possible,
     * return a GlyphLocation where GlyphLocation::valid() is false.
     * \param size size of region to allocate
     * \param data data to which to set the region allocated
     * \param padding amount of padding the passed data has
     */
    GlyphLocation
    allocate(ivec2 size, c_array<const uint8_t> data, const Padding &padding);

    /*!
     * Free a region previously allocated by allocate().
     * \param G region to free as returned by allocate().
     */
    void
    deallocate(GlyphLocation G);

    /*!
     * Returns the number of texels allocated.
     */
    unsigned int
    number_texels_allocated(void);

    /*!
     * Returns the number of bytes used by the bookkeeping tree
     */
    unsigned int
    bytes_used_by_nodes(void);

    /*!
     * Returns the number of nodes of the bookkeeping tree
     */
    unsigned int
    number_nodes(void);

    /*!
     * Negative return value indicates failure.
     * Size of pdata must be a multiple of geometry_store()->alignment().
     */
    int
    allocate_geometry_data(c_array<const generic_data> pdata);

    /*!
     * Location and count are in units of geometry_store()->alignment().
     */
    void
    deallocate_geometry_data(int location, int count);

    /*!
     * Returns how much geometry data has been allocated in
     * units of geometry_store()->alignment().
     */
    unsigned int
    geometry_data_allocated(void);

    /*!
     * Frees all allocated regions of this GlyphAtlas;
     */
    void
    clear(void);

    /*!
     * Calls GlyphAtlasTexelBackingStoreBase::flush() on
     * the texel backing store (see texel_store())
     * and GlyphAtlasGeometryBackingStoreBase::flush() on
     * the geometry backing store (see geometry_store()).
     */
    void
    flush(void) const;

    /*!
     * Returns the texel store for this GlyphAtlas.
     */
    reference_counted_ptr<const GlyphAtlasTexelBackingStoreBase>
    texel_store(void) const;

    /*!
     * Returns the geometry store for this GlyphAtlas.
     */
    reference_counted_ptr<const GlyphAtlasGeometryBackingStoreBase>
    geometry_store(void) const;

  private:
    void *m_d;
  };
/*! @} */
} //namespace fastuidraw
