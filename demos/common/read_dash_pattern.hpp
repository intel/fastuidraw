#ifndef FASTUIDRAW_DEMO_READ_DASH_PATTERN_HPP
#define FASTUIDRAW_DEMO_READ_DASH_PATTERN_HPP

#include <istream>
#include <vector>
#include <fastuidraw/painter/shader_data/painter_dashed_stroke_params.hpp>

void
read_dash_pattern(std::vector<fastuidraw::PainterDashedStrokeParams::DashPatternElement> &pattern_out,
                  std::istream &input_stream);

#endif
