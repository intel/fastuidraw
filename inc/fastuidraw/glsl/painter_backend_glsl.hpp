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
    public:
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

    };

/*! @} */
    
  }
}
