/*!
 * \file painter_save_state.hpp
 * \brief file painter_save_state.hpp
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
#include <fastuidraw/painter/painter_state.hpp>

namespace fastuidraw
{
  class PainterPacker;

  template<typename T>
  class PainterSaveState;

/*!\addtogroup Painter
  @{
 */

  /*!
    (Private) base classed used for PainterSaveState
   */
  class PainterSaveStateBase
  {
  private:
    friend class PainterPacker;
    template<typename> friend class PainterSaveState;

    ~PainterSaveStateBase();
    PainterSaveStateBase(void);
    PainterSaveStateBase(const PainterSaveStateBase&);

    explicit
    PainterSaveStateBase(void*);

    void
    operator=(const PainterSaveStateBase&);

    const void*
    raw_state(void) const;

    void *m_d;
  };

  /*!
    A PainterSaveStateBase represents a handle to a
    portion of state of PainterPacker that is packed
    into PainterDrawCommand::m_store. If already on
    a store, the also location information to reuse
    the data. The object behind the handle is NOT
    thread safe including the reference counter.
    They cannot be used in multiple threads
    simutaneously!
   */
  template<typename T>
  class PainterSaveState:PainterSaveStateBase
  {
  private:
    typedef const T& (PainterSaveState::*unspecified_bool_type)(void) const;

  public:
    /*!
      Ctor, initializes handle to NULL, i.e.
      no underlying state object.
     */
    PainterSaveState(void)
    {}

    /*!
      Returns the state to which the handle points.
      If the handle is not-value, then asserts.
     */
    const T&
    state(void) const
    {
      const T *p;

      assert(this->m_d);
      p = reinterpret_cast<const T*>(this->raw_state());
      return *p;
    }

    /*!
      Used to allow using object as a boolean without
      accidentally converting to a boolean (since it
      returns a pointer to a function).
     */
    operator
    unspecified_bool_type(void) const
    {
      return this->m_d ? &PainterSaveState::state : 0;
    }

    /*!
      Comparison operator to underlying object
      storing state.
      \param rhs handle to which to compare
     */
    bool
    operator==(const PainterSaveState &rhs) const
    {
      return this->m_d == rhs.m_d;
    }

    /*!
      Comparison operator to underlying object
      storing state.
      \param rhs handle to which to compare
     */
    bool
    operator!=(const PainterSaveState &rhs) const
    {
      return this->m_d != rhs.m_d;
    }

    /*!
      Comparison operator to underlying object
      storing state.
      \param rhs handle to which to compare
     */
    bool
    operator<(const PainterSaveState &rhs) const
    {
      return this->m_d < rhs.m_d;
    }

  private:
    friend class PainterPacker;

    explicit
    PainterSaveState(void *d):
      PainterSaveStateBase(d)
    {}
  };

  namespace PainterState
  {
    /*!
      Convenience typedef to PainterSaveState with PainterBrushState
     */
    typedef PainterSaveState<PainterBrush> PainterBrushState;

    /*!
      Convenience typedef to PainterSaveState with ClipEquationsState
     */
    typedef PainterSaveState<ClipEquations> ClipEquationsState;

    /*!
      Convenience typedef to PainterSaveState with ItemMatrixState
     */
    typedef PainterSaveState<ItemMatrix> ItemMatrixState;

    /*!
      Convenience typedef to PainterSaveState with VertexShaderDataState
     */
    typedef PainterSaveState<VertexShaderData> VertexShaderDataState;

    /*!
      Convenience typedef to PainterSaveState with FragmentShaderDataState
     */
    typedef PainterSaveState<FragmentShaderData> FragmentShaderDataState;
  }

/*! @} */

}
