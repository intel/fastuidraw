/*!
 * \file painter_item_shader.hpp
 * \brief file painter_item_shader.hpp
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
#include <fastuidraw/painter/shader/painter_shader.hpp>
#include <fastuidraw/painter/shader/painter_item_coverage_shader.hpp>

namespace fastuidraw
{

/*!\addtogroup Painter
 * @{
 */

  /*!
   * \brief
   * A PainterItemShader represents a shader to
   * draw an item (typically a vertex and fragment
   * shader pair).
   */
  class PainterItemShader:public PainterShader
  {
  public:
    /*!
     * Ctor for a PainterItemShader with no sub-shaders.
     * \param cvg coverage shader for the PainterItemShader;
     *            the coverage shader, if present is to use
     *            the exact same \ref PainterItemShaderData
     *            value but render to the coverage buffer.
     *            The \ref PainterItemShader can then use
     *            those coverage values in its shader code.
     */
    explicit
    PainterItemShader(const reference_counted_ptr<PainterItemCoverageShader> &cvg =
                      reference_counted_ptr<PainterItemCoverageShader>()):
      PainterShader(),
      m_coverage_shader(cvg)
    {}

    /*!
     * Ctor for creating a PainterItemShader which has multiple
     * sub-shaders. The purpose of sub-shaders is for the
     * case where multiple shaders almost same code and those
     * code differences can be realized by examining a sub-shader
     * ID.
     * \param num_sub_shaders number of sub-shaders
     * \param cvg coverage shader for the PainterItemShader;
     *            the coverage shader, if present is to use
     *            the exact same \ref PainterItemShaderData
     *            value but render to the coverage buffer.
     *            The \ref PainterItemShader can then use
     *            those coverage values in its shader code.
     */
    explicit
    PainterItemShader(unsigned int num_sub_shaders,
                      const reference_counted_ptr<PainterItemCoverageShader> &cvg =
                      reference_counted_ptr<PainterItemCoverageShader>()):
      PainterShader(num_sub_shaders),
      m_coverage_shader(cvg)
    {}

    /*!
     * Ctor to create a PainterItemShader realized as a sub-shader
     * of an existing PainterItemShader
     * \param parent parent PainterItemShader that has sub-shaders
     * \param sub_shader which sub-shader of the parent PainterItemShader
     * \param cvg coverage shader for the PainterItemShader;
     *            the coverage shader, if present is to use
     *            the exact same \ref PainterItemShaderData
     *            value but render to the coverage buffer.
     *            The \ref PainterItemShader can then use
     *            those coverage values in its shader code.
     */
    PainterItemShader(reference_counted_ptr<PainterItemShader> parent,
                      unsigned int sub_shader,
                      const reference_counted_ptr<PainterItemCoverageShader> &cvg =
                      reference_counted_ptr<PainterItemCoverageShader>()):
      PainterShader(parent, sub_shader),
      m_coverage_shader(cvg)
    {}

    /*!
     * The coverage shader used by this \ref PainterItemShader;
     * the coverage shader, if present is to use the exact same
     * \ref PainterItemShaderData value but renders to the
     * coverage buffer. The \ref PainterItemShader can then use
     * those coverage values in its shader code.
     */
    const reference_counted_ptr<PainterItemCoverageShader>&
    coverage_shader(void) const
    {
      return m_coverage_shader;
    }

  private:
    reference_counted_ptr<PainterItemCoverageShader> m_coverage_shader;
  };

/*! @} */
}
