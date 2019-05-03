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

#include <fastuidraw/painter/shader/painter_shader_set.hpp>
#include <fastuidraw/util/mutex.hpp>

namespace fastuidraw
{
/*!\addtogroup PainterBackend
 * @{
 */

  /*!
   * \brief
   * A PainterShaderRegistrar is an interface that defines the assigning of
   * PainterShader::ID() to a \ref PainterShader. PainterShader objects
   * are registered to a unique PainterShaderRegistrar for their lifetime.
   */
  class PainterShaderRegistrar:
    public reference_counted<PainterShaderRegistrar>::concurrent
  {
  public:
    /*!
     * Ctor.
     */
    PainterShaderRegistrar(void);

    virtual
    ~PainterShaderRegistrar();

    /*!
     * Registers an item shader for use; registering a shader more than
     * once to the SAME PainterShaderRegistrar has no effect. However,
     * registering a shader to multiple PainterShaderRegistrar objects
     * is an error.
     */
    void
    register_shader(PainterItemShader *shader);

    /*!
     * Registers an item shader for use; registering a shader more than
     * once to the SAME PainterShaderRegistrar has no effect. However,
     * registering a shader to multiple PainterShaderRegistrar objects
     * is an error.
     */
    void
    register_shader(PainterItemCoverageShader *shader);

    /*!
     * Registers a blend shader for use; registering a shader more than
     * once to the SAME PainterShaderRegistrar has no effect. However,
     * registering a shader to multiple PainterShaderRegistrar objects
     * is an error.
     */
    void
    register_shader(PainterBlendShader *shader);

    /*!
     * Registers a brush shader for use; registering a shader more than
     * once to the SAME PainterShaderRegistrar has no effect. However,
     * registering a shader to multiple PainterShaderRegistrar objects
     * is an error.
     */
    void
    register_shader(PainterBrushShader *shader);

    /*!
     * Registers each of the sub-shaders of a \ref PainterImageBrushShader.
     */
    void
    register_shader(const PainterImageBrushShader *shader);

    /*!
     * Provided as a conveniance to allow registering shaders
     * through reference counted pointers.
     */
    template<typename T>
    void
    register_shader(const reference_counted_ptr<T> &shader)
    {
      register_shader(shader.get());
    }

    /*!
     * Register each \ref PainterItemShader in a \ref
     * PainterStrokeShader
     */
    void
    register_shader(const PainterStrokeShader &p);

    /*!
     * Register each \ref PainterItemShader in a \ref
     * PainterFillShader
     */
    void
    register_shader(const PainterFillShader &p);

    /*!
     * Register each \ref PainterStrokeShader in a \ref
     * PainterDashedStrokeShaderSet
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
     * Register each of the \ref PainterBlendShader
     * in a \ref PainterBlendShaderSet.
     */
    void
    register_shader(const PainterBlendShaderSet &p);

    /*!
     * Register each of the \ref PainterBrushShader
     * in a \ref PainterBrushShaderSet.
     */
    void
    register_shader(const PainterBrushShaderSet &p);

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
     * To be implemented by a derived class to return true
     * if and only if a \ref PainterBlendShader objects
     * with a given \ref PainterBlendShader::shader_type
     * value for \ref PainterBlendShader::type() is supported.
     * The return values must be unchanging for the lifetime
     * of the \ref PainterShaderRegistrar object.
     */
    virtual
    bool
    blend_type_supported(enum PainterBlendShader::shader_type) const = 0;

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
    absorb_item_coverage_shader(const reference_counted_ptr<PainterItemCoverageShader> &shader) = 0;

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
    compute_item_coverage_sub_shader_group(const reference_counted_ptr<PainterItemCoverageShader> &shader) = 0;

    /*!
     * To be implemented by a derived class to take into use
     * a blend shader. Typically this means inserting the
     * the blend shader into a large uber shader. Returns
     * the PainterShader::Tag to be used by the backend
     * to identify the shader. An implementation will never
     * be passed an object for which PainterShader::parent()
     * is non-nullptr. In addition, mutex() will be locked on
     * entry.
     * \param shader shader whose Tag is to be computed
     */
    virtual
    PainterShader::Tag
    absorb_blend_shader(const reference_counted_ptr<PainterBlendShader> &shader) = 0;

    /*!
     * To be implemented by a derived class to compute the PainterShader::group()
     * of a sub-shader. When called, the value of the shader's PainterShader::ID()
     * and PainterShader::registered_to() are already set correctly. In addition,
     * the value of PainterShader::group() is initialized to the same value as
     * that of the PainterBlendShader::parent(). In addition, mutex() will be
     * locked on entry.
     * \param shader shader whose group is to be computed
     */
    virtual
    uint32_t
    compute_blend_sub_shader_group(const reference_counted_ptr<PainterBlendShader> &shader) = 0;

    /*!
     * To be implemented by a derived class to take into use
     * a blend shader. Typically this means inserting the
     * the blend shader into a large uber shader. Returns
     * the PainterShader::Tag to be used by the backend
     * to identify the shader. An implementation will never
     * be passed an object for which PainterShader::parent()
     * is non-nullptr. In addition, mutex() will be locked on
     * entry.
     * \param shader shader whose Tag is to be computed
     */
    virtual
    PainterShader::Tag
    absorb_custom_brush_shader(const reference_counted_ptr<PainterBrushShader> &shader) = 0;

    /*!
     * To be implemented by a derived class to compute the PainterShader::group()
     * of a sub-shader. When called, the value of the shader's PainterShader::ID()
     * and PainterShader::registered_to() are already set correctly. In addition,
     * the value of PainterShader::group() is initialized to the same value as
     * that of the PainterBrushShader::parent(). In addition, mutex() will be
     * locked on entry.
     * \param shader shader whose group is to be computed
     */
    virtual
    uint32_t
    compute_custom_brush_sub_shader_group(const reference_counted_ptr<PainterBrushShader> &shader) = 0;

  private:
    void *m_d;
  };
/*! @} */

}
