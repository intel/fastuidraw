/*!
 * \file painter_draw.hpp
 * \brief file painter_draw.hpp
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


#ifndef FASTUIDRAW_PAINTER_DRAW_HPP
#define FASTUIDRAW_PAINTER_DRAW_HPP

#include <fastuidraw/util/reference_counted.hpp>
#include <fastuidraw/util/vecN.hpp>
#include <fastuidraw/util/matrix.hpp>
#include <fastuidraw/util/c_array.hpp>
#include <fastuidraw/util/gpu_dirty_state.hpp>
#include <fastuidraw/painter/attribute_data/painter_attribute.hpp>
#include <fastuidraw/painter/backend/painter_shader_group.hpp>
#include <fastuidraw/painter/backend/painter_surface.hpp>
#include <fastuidraw/painter/backend/painter_draw_break_action.hpp>

namespace fastuidraw
{
/*!\addtogroup PainterBackend
 * @{
 */

  /*!
   * \brief
   * Store for attributes, indices of items and shared data of
   * items for items to draw. Indices (stored in \ref m_indices)
   * are -ALWAYS- in groups of three where each group is a single
   * triangle and each index is an index into \ref m_attributes.
   * The PainterDraw object is NOT thread safe, neither is its
   * reference count. A PainterDraw object is used \ref Painter
   * to send attributer and index data to a \ref PainterBackend.
   */
  class PainterDraw:
    public reference_counted<PainterDraw>::non_concurrent
  {
  public:
    /*!
     * \brief
     * A delayed action is an action that is to be called
     * just before the buffers of a \ref PainterDraw are to
     * be unmapped. Typically, this is used to write values
     * using information that is ready after the original
     * values are written by \ref Painter. A fixed \ref
     * DelayedAction object may only be added to one \ref
     * PainterDraw object, but a single \ref PainterDraw
     * can have many \ref DelayedAction objects added to it.
     */
    class DelayedAction:public reference_counted<DelayedAction>::non_concurrent
    {
    public:
      /*!
       * Ctor.
       */
      DelayedAction(void);

      ~DelayedAction();

      /*!
       * Perform the action of this DelayedAction object
       * and remove it from the list of delayed actions of
       * the PainterDraw.
       */
      void
      perform_action(void);

    protected:
      /*!
       * To be implemented by a derived class to execute its delayed action.
       * \param h handle to PainterDraw on which the action has
       *          been placed
       */
      virtual
      void
      action(const reference_counted_ptr<const PainterDraw> &h) = 0;

    private:
      friend class PainterDraw;
      void *m_d;
    };

    /*!
     * Location to which to place attribute data,
     * the store is understood to be write only.
     */
    c_array<PainterAttribute> m_attributes;

    /*!
     * Location to which to place the attribute data
     * storing the header _locations_ in \ref m_store.
     * The size of \ref m_header_attributes must be the
     * same as the size of \ref m_attributes, the store
     * is understood to be write only.
     */
    c_array<uint32_t> m_header_attributes;

    /*!
     * Location to which to place index data. Values
     * are indices into m_attributes,
     * the store is understood to be write only.
     */
    c_array<PainterIndex> m_indices;

    /*!
     * Generic store for data that is shared between
     * vertices within an item and possibly between
     * items. The store is understood to be write
     * only.
     */
    c_array<uvec4> m_store;

    /*!
     * Ctor, a derived class will set \ref m_attributes,
     * \ref m_header_attributes, \ref m_indices and
     * \ref m_store.
     */
    PainterDraw(void);

    ~PainterDraw();

    /*!
     * Called to indicate a change in value to the
     * painter header that this PainterDraw needs
     * to record. The most common case is to insert API
     * state changes (or just break a draw) for when a
     * \ref PainterBackend cannot accomodate a Painter
     * state change without changing the 3D API state.
     * \param render_type the render target type of the rendering
     * \param old_groups PainterShaderGroup before state change
     * \param new_groups PainterShaderGroup after state change
     * \param indices_written total number of indices written to m_indices -before- the change
     * \returns true if the \ref PainterShaderGroup resulted in a draw break
     */
    virtual
    bool
    draw_break(enum PainterSurface::render_type_t render_type,
               const PainterShaderGroup &old_groups,
               const PainterShaderGroup &new_groups,
               unsigned int indices_written) = 0;

    /*!
     * Called to execute an action (and thus also cause a draw-call break).
     * Implementations are to assume that \ref PainterDrawBreakAction reference
     * is non-null. Implementations are to return true if the draw_break triggers
     * a break in the draw call.
     * \param action action to execute
     * \param indices_written total number of indices written to \ref m_indices
     *                        -before- the break
     */
    virtual
    bool
    draw_break(const reference_counted_ptr<const PainterDrawBreakAction> &action,
               unsigned int indices_written) = 0;

    /*!
     * Adds a delayed action to the action list.
     * \param h handle to action to add.
     */
    void
    add_action(const reference_counted_ptr<DelayedAction> &h) const;

    /*!
     * Signals this PainterDraw to be unmapped.
     * Actual unmapping is delayed until all actions that
     * have been added with add_action() have been
     * called.
     * \param attributes_written number of elements written to \ref
     *                           m_attributes and \ref m_header_attributes.
     * \param indices_written number of elements written to \ref m_indices
     * \param data_store_written number of elements written to \ref m_store
     */
    void
    unmap(unsigned int attributes_written,
          unsigned int indices_written,
          unsigned int data_store_written);

    /*!
     * Returns true if and only if this PainterDraw
     * is unmapped.
     */
    bool
    unmapped(void) const;

    /*!
     * To be implemented by a derived class to draw the
     * contents. Must be performed after unmap() is called.
     * In addition, may only be called within a
     * PainterBackend::on_pre_draw() /
     * PainterBackend::on_post_draw() pair of the \ref
     * PainterBackend whose PainterBackend::map_draw()
     * created this object.
     */
    virtual
    void
    draw(void) const = 0;

  protected:

    /*!
     * To be implemented by a derived class to unmap
     * the arrays m_store, m_attributes
     * and m_indices. Once unmapped, the store
     * can no longer be written to.
     * \param attributes_written only the range [0,floats_written) of
     *                           m_attributes must be uploaded to
     *                           3D API
     * \param indices_written only the range [0,uints_written) of
     *                        m_indices specify indices to use.
     * \param data_store_written only the range [0,data_store_written) of
     *                           m_store must be uploaded to 3D API
     */
    virtual
    void
    unmap_implement(unsigned int attributes_written,
                    unsigned int indices_written,
                    unsigned int data_store_written) = 0;

  private:

    void
    complete_unmapping(void);

    void *m_d;
  };
/*! @} */

}

#endif
