#include "read_dash_pattern.hpp"

float
read_dash_pattern(std::vector<fastuidraw::PainterDashedStrokeParams::DashPatternElement> &pattern_out,
                  std::istream &input_stream)
{
  float return_value(0.0f);
  pattern_out.clear();
  while(input_stream)
    {
      fastuidraw::PainterDashedStrokeParams::DashPatternElement dsh;

      input_stream >> dsh.m_draw_length;
      input_stream >> dsh.m_space_length;
      if(dsh.m_draw_length > 0.0f || dsh.m_space_length > 0.0f)
        {
          pattern_out.push_back(dsh);
          return_value += dsh.m_draw_length;
        }
    }
  return return_value;
}
