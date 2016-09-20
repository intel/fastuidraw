/*!
 * \file painter_fill_shader.hpp
 * \brief file painter_fill_shader.hpp
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


#include <fastuidraw/painter/painter_item_shader.hpp>
#include <fastuidraw/painter/painter_enums.hpp>

namespace fastuidraw
{
/*!\addtogroup Painter
  @{
 */

  /*!
    A WindingSelectorChunkBase provides an interface to select
    what chunk (and if attributes are shared across chunks)
    for drawing a filled path.
   */
  class WindingSelectorChunkBase:
    public reference_counted<WindingSelectorChunkBase>::default_base
  {
  public:
    /*!
      To be implemented by a derived class to return true
      if and only if the same attribute chunk is to be
      used regardless of fill rule or winding number
      requested.
     */
    virtual
    bool
    common_attribute_data(void) const = 0;

    /*!
      To be implemented by a derived clas to return
      the chunk (i.e. the argument to feed
      PainterAttributeData::index_chunk()) to fetch
      the data for filling a path with a given fill
      rule.
      \param fill_rule fill rule
     */
    virtual
    unsigned int
    chunk_from_fill_rule(enum PainterEnums::fill_rule_t fill_rule) const = 0;

    /*!
      To be implemented by a derived clas to return
      the chunk (i.e. the argument to feed
      PainterAttributeData::index_chunk()) to fetch
      the data for filling the component of a path
      with a specified winding number.
      \param winding_number winding number
     */
    virtual
    unsigned int
    chunk_from_winding_number(int winding_number) const = 0;

    /*!
      To be implemented by a derived class to return
      true if the named chunk can be returned by
      chunk_from_winding_number() and if true,
      to also report to what winding number
     */
    virtual
    bool
    winding_number_from_chunk(unsigned int chunk, int &winding_number) const = 0;
  };

  /*!
    A PainterFillShader holds the shader for
    drawing filled paths.
   */
  class PainterFillShader
  {
  public:
    /*!
      Ctor
     */
    PainterFillShader(void);

    /*!
      Copy ctor.
     */
    PainterFillShader(const PainterFillShader &obj);

    ~PainterFillShader();

    /*!
      Assignment operator.
     */
    PainterFillShader&
    operator=(const PainterFillShader &rhs);

    /*!
      Returns the PainterItemShader to use.
     */
    const reference_counted_ptr<PainterItemShader>&
    item_shader(void) const;

    /*!
      Set the value returned by item_shader(void) const.
      \param sh value to use
     */
    PainterFillShader&
    item_shader(const reference_counted_ptr<PainterItemShader> &sh);

    /*!
      Returns a reference to the WindingSelectorChunkBase
      to be used with the PainterFillShader
     */
    const reference_counted_ptr<WindingSelectorChunkBase>&
    chunk_selector(void) const;

    /*!
      Set the value returned by chunk_selector(void) const.
      \param ch value to use
     */
    PainterFillShader&
    chunk_selector(const reference_counted_ptr<WindingSelectorChunkBase> &ch);

  private:
    void *m_d;
  };

/*! @} */
}
