/*!
 * \file painter_backend_glsl.cpp
 * \brief file painter_backend_glsl.cpp
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

#include <fastuidraw/glsl/painter_backend_glsl.hpp>
#include "private/backend_shaders.hpp"
#include "../private/util_private.hpp"

namespace
{
  class ConfigurationGLSLPrivate
  {
  public:
    ConfigurationGLSLPrivate(void):
      m_alignment(4),
      m_default_stroke_shader_aa_type(fastuidraw::PainterStrokeShader::draws_solid_then_fuzz)
    {}

    int m_alignment;
    enum fastuidraw::glsl::PainterShaderRegistrarGLSL::blending_type_t m_blending_type;
    bool m_supports_bindless_texturing;
    enum fastuidraw::PainterStrokeShader::type_t m_default_stroke_shader_aa_type;
    fastuidraw::reference_counted_ptr<const fastuidraw::PainterDraw::Action> m_default_stroke_shader_aa_pass1_action;
    fastuidraw::reference_counted_ptr<const fastuidraw::PainterDraw::Action> m_default_stroke_shader_aa_pass2_action;
  };
}

/////////////////////////////////////////////////////////////////
//fastuidraw::glsl::PainterBackendGLSL::ConfigurationGLSL methods
fastuidraw::glsl::PainterBackendGLSL::ConfigurationGLSL::
ConfigurationGLSL(void)
{
  m_d = FASTUIDRAWnew ConfigurationGLSLPrivate();
}

fastuidraw::glsl::PainterBackendGLSL::ConfigurationGLSL::
ConfigurationGLSL(const ConfigurationGLSL &obj)
{
  ConfigurationGLSLPrivate *d;
  d = static_cast<ConfigurationGLSLPrivate*>(obj.m_d);
  m_d = FASTUIDRAWnew ConfigurationGLSLPrivate(*d);
}

fastuidraw::glsl::PainterBackendGLSL::ConfigurationGLSL::
~ConfigurationGLSL()
{
  ConfigurationGLSLPrivate *d;
  d = static_cast<ConfigurationGLSLPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = nullptr;
}

enum fastuidraw::PainterBlendShader::shader_type
fastuidraw::glsl::PainterBackendGLSL::ConfigurationGLSL::
blend_type(void) const
{
  return detail::shader_blend_type(blending_type());
}

fastuidraw::PainterShaderSet
fastuidraw::glsl::PainterBackendGLSL::ConfigurationGLSL::
default_shaders(void) const
{
  ConfigurationGLSLPrivate *d;
  d = static_cast<ConfigurationGLSLPrivate*>(m_d);

  detail::ShaderSetCreator S(detail::shader_blend_type(d->m_blending_type),
                             d->m_default_stroke_shader_aa_type,
                             d->m_default_stroke_shader_aa_pass1_action,
                             d->m_default_stroke_shader_aa_pass2_action);
  return S.create_shader_set();                                 
}

assign_swap_implement(fastuidraw::glsl::PainterBackendGLSL::ConfigurationGLSL)

setget_implement(fastuidraw::glsl::PainterBackendGLSL::ConfigurationGLSL,
                 ConfigurationGLSLPrivate,
                 int, alignment)

setget_implement(fastuidraw::glsl::PainterBackendGLSL::ConfigurationGLSL,
                 ConfigurationGLSLPrivate,
                 enum fastuidraw::glsl::PainterShaderRegistrarGLSL::blending_type_t, blending_type)

setget_implement(fastuidraw::glsl::PainterBackendGLSL::ConfigurationGLSL,
                 ConfigurationGLSLPrivate,
                 bool, supports_bindless_texturing)

setget_implement(fastuidraw::glsl::PainterBackendGLSL::ConfigurationGLSL,
                 ConfigurationGLSLPrivate,
                 enum fastuidraw::PainterStrokeShader::type_t, default_stroke_shader_aa_type)

setget_implement(fastuidraw::glsl::PainterBackendGLSL::ConfigurationGLSL,
                 ConfigurationGLSLPrivate,
                 const fastuidraw::reference_counted_ptr<const fastuidraw::PainterDraw::Action>&,
                 default_stroke_shader_aa_pass1_action)

setget_implement(fastuidraw::glsl::PainterBackendGLSL::ConfigurationGLSL,
                 ConfigurationGLSLPrivate,
                 const fastuidraw::reference_counted_ptr<const fastuidraw::PainterDraw::Action>&,
                 default_stroke_shader_aa_pass2_action)

///////////////////////////////////////////////
// fastuidraw::glsl::PainterBackendGLSL methods
fastuidraw::glsl::PainterBackendGLSL::
PainterBackendGLSL(reference_counted_ptr<GlyphAtlas> glyph_atlas,
                   reference_counted_ptr<ImageAtlas> image_atlas,
                   reference_counted_ptr<ColorStopAtlas> colorstop_atlas,
                   const ConfigurationGLSL &pconfig_glsl):
  fastuidraw::PainterBackend(glyph_atlas, image_atlas, colorstop_atlas,
                             FASTUIDRAWnew PainterShaderRegistrarGLSL(),
                             ConfigurationBase()
                             .alignment(pconfig_glsl.alignment())
                             .blend_type(pconfig_glsl.blend_type())
                             .supports_bindless_texturing(pconfig_glsl.supports_bindless_texturing()),
                             pconfig_glsl.default_shaders()),
  m_config_glsl(pconfig_glsl)
{
}

void
fastuidraw::glsl::PainterBackendGLSL::
construct_shader(ShaderSource &out_vertex,
                 ShaderSource &out_fragment,
                 const UberShaderParams &contruct_params,
                 const ItemShaderFilter *item_shader_filter,
                 c_string discard_macro_value)
{
  BackendConstants backend_constants(this);

  FASTUIDRAWassert(m_config_glsl.blending_type() == contruct_params.blending_type());
  FASTUIDRAWassert(m_config_glsl.supports_bindless_texturing() == contruct_params.supports_bindless_texturing());
  painter_shader_registrar_glsl()->construct_shader(backend_constants, out_vertex, out_fragment,
                                                    contruct_params, item_shader_filter,
                                                    discard_macro_value);
}

