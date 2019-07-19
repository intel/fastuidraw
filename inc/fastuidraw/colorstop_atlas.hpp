/*!
 * \file colorstop_atlas.hpp
 * \brief file colorstop_atlas.hpp
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


#ifndef FASTUIDRAW_COLORSTOP_ATLAS_HPP
#define FASTUIDRAW_COLORSTOP_ATLAS_HPP

#include <fastuidraw/util/reference_counted.hpp>
#include <fastuidraw/colorstop.hpp>

namespace fastuidraw
{
/*!\addtogroup PainterBackend
 * @{
 */

  /*!
   * \brief
   * Represents the interface for the backing store for the texels
   * of a sequence of color stops. The expectation is that linear
   * filtering acting on the underlying backing store is all that
   * is needed for correct color interpolation from a gradient
   * interpolate. For example in GL, this can be GL_TEXTURE_1D_ARRAY
   * with both minification and magnification filters set as GL_LINEAR.
   * An implementation of the class does NOT need to be thread safe
   * because the user of the backing store (ColorStopAtlas) performs
   * calls to the backing store behind its own mutex.
   */
  class ColorStopBackingStore:
    public reference_counted<ColorStopBackingStore>::concurrent
  {
  public:
    virtual
    ~ColorStopBackingStore();

    /*!
     * To be implemented by a derived class to set color data into the
     * backing store.
     * \param x horizontal position
     * \param l length of data
     * \param w width of data
     * \param data RGBA8 values
     */
    virtual
    void
    set_data(int x, int l, int w,
             c_array<const u8vec4> data)=0;

    /*!
     * To be implemented by a derived class to flush set_data() to
     * the backing store.
     */
    virtual
    void
    flush(void)
    {}

    /*!
     * Returns the dimensions of the backing store (as passed in the
     * ctor).
     */
    ivec2
    dimensions(void) const;

    /*!
     * Returns the product of dimensions().x()
     * against dimensions().y()
     */
    int
    width_times_height(void) const;

    /*!
     * Resize the object by increasing the number of layers.
     */
    void
    resize(int new_num_layers);

  protected:
    /*!
     * Ctor.
     * \param wl provides the dimensions of the ColorStopBackingStore
     */
    explicit
    ColorStopBackingStore(ivec2 wl);

    /*!
     * Ctor
     * \param w width of backing store
     * \param num_layers number of layers of backing store
     */
    ColorStopBackingStore(int w, int num_layers);

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
   * A ColorStopAtlas is a common location to all color stop data of an
   * application. Ideally, all color stop sequences are placed into a single
   * ColorStopAtlas (changes of ColorStopAtlas force draw-call breaks).
   */
  class ColorStopAtlas:
    public reference_counted<ColorStopAtlas>::concurrent
  {
  public:
    /*!
     * Ctor.
     * \param pbacking_store handle to the ColorStopBackingStore
     *        object to which the atlas will store color stops
     */
    explicit
    ColorStopAtlas(reference_counted_ptr<ColorStopBackingStore> pbacking_store);

    virtual
    ~ColorStopAtlas();

    /*!
     * Create a \ref ColorStopSequence onto this \ref ColorStopAtlas.
     * \param color_stops source color stops to use
     * \param pwidth specifies number of texels to occupy on the ColorStopAtlas.
     *               The discretization of the color stop values is specified by
     *               the width. Additionally, the width is clamped to \ref
     *               max_width().
     */
    reference_counted_ptr<ColorStopSequence>
    create(const ColorStopArray &color_stops, unsigned int pwidth);

    /*!
     * Returns the width of the ColorStopBackingStore
     * of the atlas.
     */
    unsigned int
    max_width(void) const;

    /*!
     * Returns a handle to the backing store of the atlas.
     */
    reference_counted_ptr<const ColorStopBackingStore>
    backing_store(void) const;

    /*!
     * Increments an internal counter. If this internal
     * counter is greater than zero, then the reurning
     * of interval to the free store for later use is
     * -delayed- until the counter reaches zero again
     * (see unlock_resources()). The use case is
     * for buffered painting where the GPU calls are delayed
     * for later (to batch commands) and an Image may go
     * out of scope before the GPU commands are sent to
     * the GPU. By delaying the return of intervals to the
     * freestore, the color stop data is valid still for
     * rendering even if the owning ColorStopSequence
     * has been deleted.
     */
    void
    lock_resources(void);

    /*!
     * Decrements an internal counter. If this internal
     * counter reaches zero, those intervals from those
     * ColorStopSequence objects that were deleted
     * while the counter was non-zero, are then returned
     * to the interval free store. See lock_resources()
     * for more details.
     */
    void
    unlock_resources(void);

    /*!
     * Calls ColorStopBackingStore::flush() on
     * the backing store (see backing_store()).
     */
    void
    flush(void) const;

  private:
    friend class ColorStopSequence;

    /*!
     * Allocate and set on the atlas a sequence of color values
     * to be stored continuously in a common layer. Returns
     * the offset into the layer in ivec2::x() and the which
     * layer in ivec2::y().
     * \param data data to place on atlas
     */
    ivec2
    allocate(c_array<const u8vec4> data);

    /*!
     * Mark a region to be free on the atlas
     * \param location .x() gives the offset into the layer to
     *                 mark as free and .y() gives the layer
     * \param width number of elements to mark as free
     */
    void
    deallocate(ivec2 location, int width);

    /*!
     * Returns the total number of color stops that are available
     * in the atlas without resizing the ColorStopBackingStore
     * of the ColorStopAtlas.
     */
    int
    total_available(void) const;

    void *m_d;
  };

/*! @} */
}

#endif
