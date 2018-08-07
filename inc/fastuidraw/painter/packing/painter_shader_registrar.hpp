/*!
 * \file painter_shader_registrar.hpp
 * \brief file painter_shader_registrar.hpp
 *
 * Copyright 2018 by Intel.
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

#include <fastuidraw/painter/painter_shader_set.hpp>
#include <fastuidraw/util/mutex.hpp>

namespace fastuidraw
{
/*!\addtogroup PainterPacking
 * @{
 */

  /*!
   * \brief
   * A PainterShaderRegistrar is an interface that defines the assigning of
   * PainterShader::ID() to a \ref PainterShader. PainterShader objects
   * are registered to a unique PainterShaderRegistrar for their lifetime.
   */
  class PainterShaderRegistrar:
    public reference_counted<PainterShaderRegistrar>::default_base
  {
  public:
    /*!
     * Ctor.
     */
    PainterShaderRegistrar(void);

    virtual
    ~PainterShaderRegistrar();

    /*!
     * Registers an item shader for use; registring a shader more than
     * once to the SAME PainterShaderRegistrar has no effect. However,
     * registering a shader to multiple PainterShaderRegistrar objects
     * is an error.
     */
    void
    register_shader(const reference_counted_ptr<PainterItemShader> &shader);

    /*!
     * Registers a composite shader for use; registring a shader more than
     * once to the SAME PainterShaderRegistrar has no effect. However,
     * registering a shader to multiple PainterShaderRegistrar objects
     * is an error.
     */
    void
    register_shader(const reference_counted_ptr<PainterCompositeShader> &shader);

    /*!
     * Provided as a conveniance, equivalent to
     * \code
     * register_shader(p.non_aa_shader());
     * register_shader(p.aa_shader_pass1());
     * register_shader(p.aa_shader_pass2());
     * \endcode
     * \param p PainterStrokeShader hold shaders to register
     */
    void
    register_shader(const PainterStrokeShader &p);

    /*!
     * Provided as a conveniance, equivalent to
     * \code
     * register_shader(p.fill_shader());
     * \endcode
     * \param p PainterFillShader hold shaders to register
     */
    void
    register_shader(const PainterFillShader &p);

    /*!
     * Provided as a conveniance, equivalent to calling
     * register_shader(const PainterStrokeShader&) on each
     * PainterDashedStrokeShaderSet::shader(enum PainterEnums::dashed_cap_style) const.
     * \param p PainterDashedStrokeShaderSet hold shaders to register
     */
    void
    register_shader(const PainterDashedStrokeShaderSet &p);

    /*!
     * Register each of the reference_counted_ptr<PainterShader>
     * in a PainterGlyphShader.
     */
    void
    register_shader(const PainterGlyphShader &p);

    /*!
     * Register each of the reference_counted_ptr<PainterCompositeShader>
     * in a PainterCompositeShaderSet.
     */
    void
    register_shader(const PainterCompositeShaderSet &p);

    /*!
     * Register each of the shaders in a PainterShaderSet.
     */
    void
    register_shader(const PainterShaderSet &p);

  protected:

    /*!
     * Return the \ref Mutex used to make this object thread safe.
     */
    Mutex&
    mutex(void);

    /*!
     * To be implemented by a derived class to take into use
     * an item shader. Typically this means inserting the
     * the shader into a large uber shader. Returns
     * the PainterShader::Tag to be used by the backend
     * to identify the shader.  An implementation will never
     * be passed an object for which PainterShader::parent()
     * is non-nullptr. In addition, mutex() will be locked on
     * entry.
     * \param shader shader whose Tag is to be computed
     */
    virtual
    PainterShader::Tag
    absorb_item_shader(const reference_counted_ptr<PainterItemShader> &shader) = 0;

    /*!
     * To be implemented by a derived class to compute the PainterShader::group()
     * of a sub-shader. When called, the value of the shader's PainterShader::ID()
     * and PainterShader::registered_to() are already set correctly. In addition,
     * the value of PainterShader::group() is initialized to the same value as
     * that of the PainterItemShader::parent(). In addition, mutex() will be
     * locked on entry.
     * \param shader shader whose group is to be computed
     */
    virtual
    uint32_t
    compute_item_sub_shader_group(const reference_counted_ptr<PainterItemShader> &shader) = 0;

    /*!
     * To be implemented by a derived class to take into use
     * a composite shader. Typically this means inserting the
     * the composite shader into a large uber shader. Returns
     * the PainterShader::Tag to be used by the backend
     * to identify the shader. An implementation will never
     * be passed an object for which PainterShader::parent()
     * is non-nullptr. In addition, mutex() will be locked on
     * entry.
     * \param shader shader whose Tag is to be computed
     */
    virtual
    PainterShader::Tag
    absorb_composite_shader(const reference_counted_ptr<PainterCompositeShader> &shader) = 0;

    /*!
     * To be implemented by a derived class to compute the PainterShader::group()
     * of a sub-shader. When called, the value of the shader's PainterShader::ID()
     * and PainterShader::registered_to() are already set correctly. In addition,
     * the value of PainterShader::group() is initialized to the same value as
     * that of the PainterCompositeShader::parent(). In addition, mutex() will be
     * locked on entry.
     * \param shader shader whose group is to be computed
     */
    virtual
    uint32_t
    compute_composite_sub_shader_group(const reference_counted_ptr<PainterCompositeShader> &shader) = 0;

  private:
    void *m_d;
  };
/*! @} */

}
