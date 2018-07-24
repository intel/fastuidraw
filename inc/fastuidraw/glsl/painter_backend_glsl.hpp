/*!
 * \file painter_backend_glsl.hpp
 * \brief file painter_backend_glsl.hpp
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

#include <fastuidraw/glsl/painter_shader_registrar_glsl.hpp>
#include <fastuidraw/painter/packing/painter_backend.hpp>

namespace fastuidraw
{
  namespace glsl
  {
/*!\addtogroup GLSL
 * @{
 */

    /*!
     * PainterBackendGLSL is a partial implementation of PainterBackend that
     * actually does nothing: it just constructs a PainterShaderRegistrarGLSL
     * and uses that as the PainterShaderRegistrar in the ctor of PainterBackend.
     */
    class PainterBackendGLSL:
      public PainterBackend,
      public PainterShaderRegistrarGLSLTypes
    {
    public:/*!
       * \brief
       * A params gives parameters how to contruct
       * a PainterShaderRegistrarGLSL.
       *
       * These values influence the behavior of both
       * the PainterShaderRegistrarGLSL and the shaders it
       * constructs via PainterShaderRegistrarGLSL::construct_shader().
       */
      class ConfigurationGLSL
      {
      public:
        /*!
         * Ctor.
         */
        ConfigurationGLSL(void);

        /*!
         * Copy ctor.
         * \param obj value from which to copy
         */
        ConfigurationGLSL(const ConfigurationGLSL &obj);

        ~ConfigurationGLSL();

        /*!
         * Assignment operator
         * \param rhs value from which to copy
         */
        ConfigurationGLSL&
        operator=(const ConfigurationGLSL &rhs);

        /*!
         * Swap operation
         * \param obj object with which to swap
         */
        void
        swap(ConfigurationGLSL &obj);

        /*!
         * Specifies the alignment in units of generic_data for
         * packing of seperately accessible entries of generic data
         * in PainterDraw::m_store.
         */
        int
        alignment(void) const;

        /*!
         * Specify the value returned by alignment(void) const,
         * default value is 4
         * \param v value
         */
        ConfigurationGLSL&
        alignment(int v);

        /*!
         * Returns how the painter will perform blending.
         */
        enum blending_type_t
        blending_type(void) const;

        /*!
         * Specify the return value to blending_type() const.
         * Default value is \ref blending_dual_src
         * \param tp blend shader type
         */
        ConfigurationGLSL&
        blending_type(enum blending_type_t tp);

        /*!
         * Provided as a conveniance, returns the value of
         * blending_type(void) const as a \ref 
         * PainterBlendShader::shader_type.
         */
        enum PainterBlendShader::shader_type
        blend_type(void) const;

        /*!
         * If true, indicates that the PainterRegistrar supports
         * bindless texturing. Default value is false.
         */
        bool
        supports_bindless_texturing(void) const;

        /*!
         * Specify the return value to supports_bindless_texturing() const.
         * Default value is false.
         */
        ConfigurationGLSL&
        supports_bindless_texturing(bool);

        /*!
         * Sets how the default stroke shaders perform anti-aliasing.
         * If the value is \ref PainterStrokeShader::cover_then_draw, then
         * UberShaderParams::provide_auxiliary_image_buffer() must be true.
         */
        enum PainterStrokeShader::type_t
        default_stroke_shader_aa_type(void) const;

        /*!
         * Set the value returned by default_stroke_shader_aa_type(void) const.
         * Default value is \ref PainterStrokeShader::draws_solid_then_fuzz.
         */
        ConfigurationGLSL&
        default_stroke_shader_aa_type(enum PainterStrokeShader::type_t);

        /*!
         * The value to use for the default stroke shaders
         * for \ref PainterStrokeShader::aa_action_pass1().
         */
        const reference_counted_ptr<const PainterDraw::Action>&
        default_stroke_shader_aa_pass1_action(void) const;

        /*!
         * Set the value returned by default_stroke_shader_aa_pass1_action(void) const.
         * Default value is nullptr.
         */
        ConfigurationGLSL&
        default_stroke_shader_aa_pass1_action(const reference_counted_ptr<const PainterDraw::Action> &action);

        /*!
         * The value to use for the default stroke shaders
         * for \ref PainterStrokeShader::aa_action_pass2().
         */
        const reference_counted_ptr<const PainterDraw::Action>&
        default_stroke_shader_aa_pass2_action(void) const;

        /*!
         * Set the value returned by default_stroke_shader_aa_pass2_action(void) const.
         * Default value is nullptr.
         */
        ConfigurationGLSL&
        default_stroke_shader_aa_pass2_action(const reference_counted_ptr<const PainterDraw::Action> &action);

        /*!
         * Returns a PainterShaderSet derived from the current values
         * of this ConfigurationGLSL.
         */
        PainterShaderSet
        default_shaders(void) const;

      private:
        void *m_d;
      };

      PainterBackendGLSL(reference_counted_ptr<GlyphAtlas> glyph_atlas,
                         reference_counted_ptr<ImageAtlas> image_atlas,
                         reference_counted_ptr<ColorStopAtlas> colorstop_atlas,
                         const ConfigurationGLSL &config_glsl);
      void
      construct_shader(ShaderSource &out_vertex,
                       ShaderSource &out_fragment,
                       const UberShaderParams &contruct_params,
                       const ItemShaderFilter *item_shader_filter = nullptr,
                       c_string discard_macro_value = "discard");

      reference_counted_ptr<PainterShaderRegistrarGLSL>
      painter_shader_registrar_glsl(void)
      {
        const reference_counted_ptr<PainterShaderRegistrar> &reg(painter_shader_registrar());
        FASTUIDRAWassert(reg.dynamic_cast_ptr<PainterShaderRegistrarGLSL>());
        return reg.static_cast_ptr<PainterShaderRegistrarGLSL>();
      }

      const ConfigurationGLSL&
      configuration_glsl(void) const
      {
        return m_config_glsl;
      }

    private:
      ConfigurationGLSL m_config_glsl;
    };

/*! @} */
    
  }
}
