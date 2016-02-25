/*!
 * \file colorstop_atlas.hpp
 * \brief file colorstop_atlas.hpp
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
#include <fastuidraw/colorstop.hpp>

namespace fastuidraw
{
/*!\addtogroup Core
  @{
 */

  /*!
    Represents the interface for the backing store for the texels
    of a sequence of color stops. The expectation is that linear
    filtering acting on the underlying backing store is all that
    is needed for correct color interpolation from a gradient
    interpolate. For example in GL, this can be GL_TEXTURE_1D_ARRAY
    with both minification and magnification filters set as GL_LINEAR.
    An implementation of the class does NOT need to be thread safe
    because the user of the backing store (ColorStopAtlas) performs
    calls to the backing store behind its own mutex.
   */
  class ColorStopBackingStore:
    public reference_counted<ColorStopBackingStore>::default_base
  {
  public:
    /*!
      Ctor.
      \param wl provides the dimensions of the ColorStopBackingStore
      \param presizable if true the object can be resized to be larger
     */
    ColorStopBackingStore(ivec2 wl, bool presizable);

    /*!
      Ctor
      \param w width of backing store
      \param num_layers number of layers of backing store
      \param presizable if true the object can be resized to be larger
     */
    ColorStopBackingStore(int w, int num_layers, bool presizable);

    virtual
    ~ColorStopBackingStore();

    /*!
      To be implemented by a derived class to set color data into the
      backing store.
      \param x horizontal position
      \param l length of data
      \param w width of data
      \param data RGBA8 values
     */
    virtual
    void
    set_data(int x, int l,
             int w,
             const_c_array<u8vec4> data)=0;

    /*!
      To be implemented by a derived class to flush set_data() to
      the backing store.
     */
    virtual
    void
    flush(void)
    {}

    /*!
      Returns the dimensions of the backing store (as passed in the
      ctor).
     */
    ivec2
    dimensions(void) const;

    /*!
      Returns the product of dimensions().x()
      against dimensions().y()
     */
    int
    width_times_height(void) const;

    /*!
      Returns true if and only if this object can be
      resized to a larger size.
     */
    bool
    resizeable(void) const;

    /*!
      Resize the object by increasing the number of layers.
      The routine resizeable() must return true, if not
      the function asserts.
     */
    void
    resize(int new_num_layers);

  protected:
    /*!
      To be implemented by a derived class to resize the
      object. The resize changes ONLY the number of layers
      of the object and only increases the value as well.
      When called, the return value of dimensions() is
      the size before the resize completes.
      \param new_num_layers new number of layers to which
                            to resize the underlying store.
     */
    virtual
    void
    resize_implement(int new_num_layers) = 0;

  private:
    void *m_d;
  };

  /*!
    A ColorStopAtlas is a common location to all color stop data of an
    application. Ideally, all color stop sequences are placed into a single
    ColorStopAtlas (changes of ColorStopAtlas force draw-call breaks).
   */
  class ColorStopAtlas:
    public reference_counted<ColorStopAtlas>::default_base
  {
  public:
    /*!
      Ctor.
      \param pbacking_store handle to the ColorStopBackingStore
             object to which the atlas will store color stops
     */
    explicit
    ColorStopAtlas(ColorStopBackingStore::handle pbacking_store);

    virtual
    ~ColorStopAtlas();

    /*!
      Allocate and set on the atlas a sequence of color values
      to be stored continuously in a common layer. Returns
      the offset into the layer in ivec2::x() and the which
      layer in ivec2::y().
      \param data data to place on atlas
     */
    ivec2
    allocate(const_c_array<u8vec4> data);

    /*!
      Mark a region to be free on the atlas
      \param location .x() gives the offset into the layer to
                      mark as free and .y() gives the layer
      \param width number of elements to mark as free
     */
    void
    deallocate(ivec2 location, int width);

    /*!
      Returns the total number of color stops that are available
      in the atlas without resizing the ColorStopBackingStore
      of the ColorStopAtlas.
     */
    int
    total_available(void) const;

    /*!
      Returns the largest size possible to allocate on the atlas
      via allocate(). Note that this value is much smaller
      than total_available() because an allocation needs
      to all be within the same layer and continuous within the
      layer.
     */
    int
    largest_allocation_possible(void) const;

    /*!
      Returns the width of the ColorStopBackingStore
      of the atlas.
     */
    int
    max_width(void) const;

    /*!
      Returns a handle to the backing store of the atlas.
     */
    ColorStopBackingStore::const_handle
    backing_store(void) const;

    /*!
      Calls ColorStopBackingStore::flush() on
      the backing store (see backing_store()).
     */
    void
    flush(void) const;

  private:
    void *m_d;
  };

  /*!
    A ColorStopSequenceOnAtlas is a ColorStopSequence on a ColorStopAtlas.
    A ColorStopAtlas is backed by a 1D texture array with linear filtering.
    The values of ColorStop::m_place are discretized. Values in between the
    ColorStop 's of a ColorStopSequence are interpolated.
   */
  class ColorStopSequenceOnAtlas:
    public reference_counted<ColorStopSequenceOnAtlas>::default_base
  {
  public:
    /*!
      Ctor.
      \param color_stops source color stops to use
      \param atlas ColorStopAtlas on to which to place
      \param pwidth specifies number of texels to occupy on the ColorStopAtlas.
                    The discretization of the color stop values is specified by
                    the width. Additionally the width is clamped to
                    ColorStopAtlas::max_width().
     */
    ColorStopSequenceOnAtlas(const ColorStopSequence &color_stops,
                             ColorStopAtlas::handle atlas,
                             int pwidth);

    ~ColorStopSequenceOnAtlas();

    /*!
      Returns the location in the backing store to
      the logical start of the ColorStopSequenceOnAtlas.
      A ColorStopSequenceOnAtlas is added to an atlas
      so that the first and last texel are repeated, thus
      allowing for implementation to use linear texture
      filtering to implement color interpolation quickly
      in a shader.
     */
    ivec2
    texel_location(void) const;

    /*!
      Returns the number of texels NOT including repeating the
      boundary texels used in the backing store.
     */
    int
    width(void) const;

    /*!
      Returns the atlas on which the object resides
     */
    ColorStopAtlas::const_handle
    atlas(void) const;

  private:
    void *m_d;
  };

/*! @} */
}
