#pragma once

#include <fastuidraw/painter/painter_data.hpp>

namespace fastuidraw
{
/*!\addtogroup PainterPacking
  @{
 */

  /*!
    A PainterPackerData is the data parameters for drawing
    commans of PainterPacker.
   */
  class PainterPackerData:public PainterData
  {
  public:
    PainterPackerData(void)
    {}

    explicit
    PainterPackerData(const PainterData &obj):
      PainterData(obj)
    {}

    /*!
      value for the clip equations.
     */
    value<PainterClipEquations> m_clip;

    /*!
      value for the transformation matrix.
     */
    value<PainterItemMatrix> m_matrix;
  };

/*! @} */

}
