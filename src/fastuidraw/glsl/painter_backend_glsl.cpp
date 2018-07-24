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


///////////////////////////////////////////////
// fastuidraw::glsl::PainterBackendGLSL methods
fastuidraw::glsl::PainterBackendGLSL::
PainterBackendGLSL(reference_counted_ptr<GlyphAtlas> glyph_atlas,
                   reference_counted_ptr<ImageAtlas> image_atlas,
                   reference_counted_ptr<ColorStopAtlas> colorstop_atlas,
                   const ConfigurationGLSL &config_glsl):
  fastuidraw::PainterBackend(glyph_atlas, image_atlas, colorstop_atlas,
                             FASTUIDRAWnew PainterShaderRegistrarGLSL(config_glsl),
                             ConfigurationBase()
                             .alignment(config_glsl.alignment())
                             .blend_type(config_glsl.blend_type())
                             .supports_bindless_texturing(config_glsl.supports_bindless_texturing()),
                             config_glsl.default_shaders())
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
  PainterBackend *backend(this);
  painter_shader_registrar_glsl()->construct_shader(backend, out_vertex, out_fragment,
                                                    contruct_params, item_shader_filter,
                                                    discard_macro_value);
}

