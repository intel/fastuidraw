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
  class PainterSaveStatePool;
  class PainterPacker;

  template<typename T>
  class PainterSaveState;

/*!\addtogroup Painter
  @{
 */

  /*!
    (Private) base classe used for PainterSaveState
   */
  class PainterSaveStateBase
  {

    /*!
      Returns the alignment packing for PainterSaveState
      object (see PainterPacker::Configuration::alignment());
      If the PainterSaveState represents a NULL handle then
      returns 0.
     */
    unsigned int
    alignment_packing(void) const;

  private:
    friend class PainterPacker;
    friend class PainterSaveStatePool;
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
    A PainterSaveStateBase represents a handle to a portion of state of
    PainterPacker that is packed into PainterDrawCommand::m_store. If already
    on a store, the also location information to reuse the data. The object
    behind the handle is NOT thread safe (including the reference counter!)
    They cannot be used in multiple threads simutaneously. A fixed
    PainterSaveState can be used by different Painter (and PainterPacker)
    objects subject to the condition that the data store alignment (see
    PainterPacker::Configuration::alignment()) is the same for each of these
    objects.
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
    friend class PainterSaveStatePool;

    explicit
    PainterSaveState(void *d):
      PainterSaveStateBase(d)
    {}
  };

  /*!
    A PainterSaveStatePool can be used to create PainterSaveState
    objects. Just like PainterSaveState, PainterSaveStatePool is
    NOT thread safe, as such it is not a safe operation to use the
    same PainterSaveStatePool object from multiple threads at the
    same time. A fixed PainterSaveStatePool can create PainterSaveState
    objects used by different Painter (and PainterPacker) objects subject
    to the condition that the data store alignment (see
    PainterPacker::Configuration::alignment()) is the same for each of
    these objects.
   */
  class PainterSaveStatePool:noncopyable
  {
  public:
    /*!
      Ctor.
      \param alignment the alignment to create packed data, see
                       PainterPacker::Configuration::alignment()
     */
    explicit
    PainterSaveStatePool(int painter_alignment);

    ~PainterSaveStatePool();

    /*!
      Create and return a PainerSaveState object for
      the value of a PainterBrush object.
      \param value data to pack into returned PainterSaveState
     */
    PainterSaveState<PainterBrush>
    create_state(const PainterBrush &value);

    /*!
      Create and return a PainerSaveState object for
      the value of a PainterState::ClipEquations object.
      \param value data to pack into returned PainterSaveState
     */
    PainterSaveState<PainterState::ClipEquations>
    create_state(const PainterState::ClipEquations &value);

    /*!
      Create and return a PainerSaveState object for
      the value of a PainterState::ItemMatrix object.
      \param value data to pack into returned PainterSaveState
     */
    PainterSaveState<PainterState::ItemMatrix>
    create_state(const PainterState::ItemMatrix &value);

    /*!
      Create and return a PainerSaveState object for
      the value of a PainterState::VertexShaderData object.
      \param value data to pack into returned PainterSaveState
     */
    PainterSaveState<PainterState::VertexShaderData>
    create_state(const PainterState::VertexShaderData &value);

    /*!
      Create and return a PainerSaveState object for
      the value of a  object.
      \param value data to pack into returned PainterSaveState
     */
    PainterSaveState<PainterState::FragmentShaderData>
    create_state(const PainterState::FragmentShaderData &value);

  private:
    void *m_d;
  };

  namespace PainterState
  {
    /*!
      Convenience typedef to PainterSaveState with PainterBrush
     */
    typedef PainterSaveState<PainterBrush> PainterBrushState;

    /*!
      Convenience typedef to PainterSaveState with ClipEquations
     */
    typedef PainterSaveState<ClipEquations> ClipEquationsState;

    /*!
      Convenience typedef to PainterSaveState with ItemMatrix
     */
    typedef PainterSaveState<ItemMatrix> ItemMatrixState;

    /*!
      Convenience typedef to PainterSaveState with VertexShaderData
     */
    typedef PainterSaveState<VertexShaderData> VertexShaderDataState;

    /*!
      Convenience typedef to PainterSaveState with FragmentShaderData
     */
    typedef PainterSaveState<FragmentShaderData> FragmentShaderDataState;
  }

/*! @} */

}
