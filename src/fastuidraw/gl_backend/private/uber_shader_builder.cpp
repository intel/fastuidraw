#include "uber_shader_builder.hpp"
#include "../../private/util_private.hpp"

namespace
{
  unsigned int
  number_data_blocks(unsigned int alignment, unsigned int sz)
  {
    unsigned int number_blocks;
    number_blocks = sz / alignment;
    if(number_blocks * alignment < sz)
      {
        ++number_blocks;
      }
    return number_blocks;
  }

  const char*
  float_varying_label(unsigned int t)
  {
    using namespace fastuidraw::glsl;
    switch(t)
      {
      case varying_list::interpolation_smooth:
        return "fastuidraw_varying_float_smooth";
      case varying_list::interpolation_flat:
        return "fastuidraw_varying_float_flat";
      case varying_list::interpolation_noperspective:
        return "fastuidraw_varying_float_noperspective";
      }
    assert(!"Invalid varying_list::interpolation_qualifier_t");
    return "";
  }

  const char*
  int_varying_label(void)
  {
    return "fastuidraw_varying_int";
  }

  const char*
  uint_varying_label(void)
  {
    return "fastuidraw_varying_uint";
  }

  void
  stream_alias_varyings(fastuidraw::glsl::ShaderSource &vert,
                        fastuidraw::const_c_array<const char*> p,
                        const std::string &s, bool define)
  {
    for(unsigned int i = 0; i < p.size(); ++i)
      {
        std::ostringstream str;
        str << s << i;
        if(define)
          {
            vert.add_macro(p[i], str.str().c_str());
          }
        else
          {
            vert.remove_macro(p[i]);
          }
      }
  }

  void
  stream_alias_varyings(fastuidraw::glsl::ShaderSource &shader,
                        const fastuidraw::glsl::varying_list &p,
                        bool define)
  {
    using namespace fastuidraw::glsl;

    stream_alias_varyings(shader, p.uints(), uint_varying_label(), define);
    stream_alias_varyings(shader, p.ints(), int_varying_label(), define);

    for(unsigned int i = 0; i < varying_list::interpolation_number_types; ++i)
      {
        enum varying_list::interpolation_qualifier_t q;
        q = static_cast<enum varying_list::interpolation_qualifier_t>(i);
        stream_alias_varyings(shader, p.floats(q), float_varying_label(q), define);
      }
  }

  void
  stream_declare_varyings_type(std::ostream &str, unsigned int cnt,
                               const std::string &qualifier,
                               const std::string &type,
                               const std::string &name)
  {
    for(unsigned int i = 0; i < cnt; ++i)
      {
        str << qualifier << " fastuidraw_varying " << type << " " << name << i << ";\n";
      }
  }

  void
  pre_stream_varyings(fastuidraw::glsl::ShaderSource &dst,
                      const fastuidraw::reference_counted_ptr<fastuidraw::glsl::PainterItemShaderGLSL> &sh)
  {
    stream_alias_varyings(dst, sh->varyings(), true);
  }

  void
  post_stream_varyings(fastuidraw::glsl::ShaderSource &dst,
                       const fastuidraw::reference_counted_ptr<fastuidraw::glsl::PainterItemShaderGLSL> &sh)
  {
    stream_alias_varyings(dst, sh->varyings(), false);
  }

  template<typename T>
  class UberShaderStreamer
  {
  public:
    typedef fastuidraw::glsl::ShaderSource ShaderSource;
    typedef fastuidraw::reference_counted_ptr<T> ref_type;
    typedef fastuidraw::const_c_array<ref_type> array_type;
    typedef const ShaderSource& (T::*get_src_type)(void) const;
    typedef void (*pre_post_stream_type)(ShaderSource &dst, const ref_type &sh);

    static
    void
    stream_nothing(ShaderSource &, const ref_type &)
    {}

    static
    void
    stream_uber(bool use_switch, ShaderSource &dst, array_type shaders,
                get_src_type get_src,
                pre_post_stream_type pre_stream,
                pre_post_stream_type post_stream,
                const std::string &return_type,
                const std::string &uber_func_with_args,
                const std::string &shader_main,
                const std::string &shader_args, //of the form ", arg1, arg2,..,argN" or empty string
                const std::string &shader_id);

    static
    void
    stream_uber(bool use_switch, ShaderSource &dst, array_type shaders,
                get_src_type get_src,
                const std::string &return_type,
                const std::string &uber_func_with_args,
                const std::string &shader_main,
                const std::string &shader_args,
                const std::string &shader_id)
    {
      stream_uber(use_switch, dst, shaders, get_src,
                  &stream_nothing, &stream_nothing,
                  return_type, uber_func_with_args,
                  shader_main, shader_args, shader_id);
    }
  };

}

template<typename T>
void
UberShaderStreamer<T>::
stream_uber(bool use_switch, ShaderSource &dst, array_type shaders,
            get_src_type get_src,
            pre_post_stream_type pre_stream, pre_post_stream_type post_stream,
            const std::string &return_type,
            const std::string &uber_func_with_args,
            const std::string &shader_main,
            const std::string &shader_args, //of the form ", arg1, arg2,..,argN" or empty string
            const std::string &shader_id)
{
  /* first stream all of the item_shaders with predefined macros. */
  for(unsigned int i = 0; i < shaders.size(); ++i)
    {
      pre_stream(dst, shaders[i]);

      std::ostringstream str;
      str << shader_main << shaders[i]->ID();
      dst
        .add_macro(shader_main.c_str(), str.str().c_str())
        .add_source((shaders[i].get()->*get_src)())
        .remove_macro(shader_main.c_str());

      post_stream(dst, shaders[i]);
    }

  std::ostringstream str;
  bool has_sub_shaders(false), has_return_value(return_type != "void");
  std::string tab;

  str << return_type << "\n"
      << uber_func_with_args << "\n"
      << "{\n";

  if(has_return_value)
    {
      str << "    " << return_type << " p;\n";
    }

  for(unsigned int i = 0; i < shaders.size(); ++i)
    {
      if(shaders[i]->number_sub_shaders() > 1)
        {
          unsigned int start, end;
          start = shaders[i]->ID();
          end = start + shaders[i]->number_sub_shaders();
          if(has_sub_shaders)
            {
              str << "    else";
            }
          else
            {
              str << "    ";
            }

          str << "if(" << shader_id << " >= uint(" << start
              << ") && " << shader_id << " < uint(" << end << "))\n"
              << "    {\n"
              << "        ";
          if(has_return_value)
            {
              str << "p = ";
            }
          str << shader_main << shaders[i]->ID()
              << "(" << shader_id << " - uint(" << start << ")" << shader_args << ");\n"
              << "    }\n";
          has_sub_shaders = true;
        }
    }

  if(has_sub_shaders && use_switch)
    {
      str << "    else\n"
          << "    {\n";
      tab = "        ";
    }
  else
    {
      tab = "    ";
    }

  if(use_switch)
    {
      str << tab << "switch(" << shader_id << ")\n"
          << tab << "{\n";
    }

  for(unsigned int i = 0; i < shaders.size(); ++i)
    {
      if(shaders[i]->number_sub_shaders() == 1)
        {
          if(use_switch)
            {
              str << tab << "case uint(" << shaders[i]->ID() << "):\n";
              str << tab << "    {\n"
                  << tab << "        ";
            }
          else
            {
              if(i != 0)
                {
                  str << tab << "else if";
                }
              else
                {
                  str << tab << "if";
                }
              str << "(" << shader_id << " == uint(" << shaders[i]->ID() << "))\n";
              str << tab << "{\n"
                  << tab << "    ";
            }

          if(has_return_value)
            {
              str << "p = ";
            }

          str << shader_main << shaders[i]->ID()
              << "(uint(0)" << shader_args << ");\n";

          if(use_switch)
            {
              str << tab << "    }\n"
                  << tab << "    break;\n\n";
            }
          else
            {
              str << tab << "}\n";
            }
        }
    }

  if(use_switch)
    {
      str << tab << "}\n";
    }

  if(has_sub_shaders && use_switch)
    {
      str << "    }\n";
    }

  if(has_return_value)
    {
      str << "    return p;\n";
    }

  str << "}\n";
  dst.add_source(str.str().c_str(), ShaderSource::from_string);
}


namespace fastuidraw { namespace glsl { namespace detail { namespace shader_builder {

void
add_enums(unsigned int alignment, ShaderSource &src)
{
  using namespace fastuidraw::PainterPacking;
  using namespace fastuidraw::PainterEnums;

  /* fp32 can store a 24-bit integer exactly,
     however, the operation of converting from
     uint to normalized fp32 may lose a bit,
     so 23-bits it is.
     TODO: go through the requirements of IEEE754,
     what a compiler of a driver might do and
     what a GPU does to see how many bits we
     really have.
  */
  uint32_t z_bits_supported;
  z_bits_supported = std::min(23u, static_cast<uint32_t>(z_num_bits));

  src
    .add_macro("fastuidraw_half_max_z", FASTUIDRAW_MAX_VALUE_FROM_NUM_BITS(z_bits_supported - 1))
    .add_macro("fastuidraw_max_z", FASTUIDRAW_MAX_VALUE_FROM_NUM_BITS(z_bits_supported))
    .add_macro("fastuidraw_shader_image_mask", PainterBrush::image_mask)
    .add_macro("fastuidraw_shader_image_filter_bit0", PainterBrush::image_filter_bit0)
    .add_macro("fastuidraw_shader_image_filter_num_bits", PainterBrush::image_filter_num_bits)
    .add_macro("fastuidraw_shader_image_filter_nearest", PainterBrush::image_filter_nearest)
    .add_macro("fastuidraw_shader_image_filter_linear", PainterBrush::image_filter_linear)
    .add_macro("fastuidraw_shader_image_filter_cubic", PainterBrush::image_filter_cubic)
    .add_macro("fastuidraw_shader_linear_gradient_mask", PainterBrush::gradient_mask)
    .add_macro("fastuidraw_shader_radial_gradient_mask", PainterBrush::radial_gradient_mask)
    .add_macro("fastuidraw_shader_gradient_repeat_mask", PainterBrush::gradient_repeat_mask)
    .add_macro("fastuidraw_shader_repeat_window_mask", PainterBrush::repeat_window_mask)
    .add_macro("fastuidraw_shader_transformation_translation_mask", PainterBrush::transformation_translation_mask)
    .add_macro("fastuidraw_shader_transformation_matrix_mask", PainterBrush::transformation_matrix_mask)
    .add_macro("fastuidraw_image_number_index_lookup_bit0", PainterBrush::image_number_index_lookups_bit0)
    .add_macro("fastuidraw_image_number_index_lookup_num_bits", PainterBrush::image_number_index_lookups_num_bits)
    .add_macro("fastuidraw_image_slack_bit0", PainterBrush::image_slack_bit0)
    .add_macro("fastuidraw_image_slack_num_bits", PainterBrush::image_slack_num_bits)
    .add_macro("fastuidraw_image_master_index_x_bit0",     Brush::image_atlas_location_x_bit0)
    .add_macro("fastuidraw_image_master_index_x_num_bits", Brush::image_atlas_location_x_num_bits)
    .add_macro("fastuidraw_image_master_index_y_bit0",     Brush::image_atlas_location_y_bit0)
    .add_macro("fastuidraw_image_master_index_y_num_bits", Brush::image_atlas_location_y_num_bits)
    .add_macro("fastuidraw_image_master_index_z_bit0",     Brush::image_atlas_location_z_bit0)
    .add_macro("fastuidraw_image_master_index_z_num_bits", Brush::image_atlas_location_z_num_bits)
    .add_macro("fastuidraw_image_size_x_bit0",     Brush::image_size_x_bit0)
    .add_macro("fastuidraw_image_size_x_num_bits", Brush::image_size_x_num_bits)
    .add_macro("fastuidraw_image_size_y_bit0",     Brush::image_size_y_bit0)
    .add_macro("fastuidraw_image_size_y_num_bits", Brush::image_size_y_num_bits)
    .add_macro("fastuidraw_color_stop_x_bit0",     Brush::gradient_color_stop_x_bit0)
    .add_macro("fastuidraw_color_stop_x_num_bits", Brush::gradient_color_stop_x_num_bits)
    .add_macro("fastuidraw_color_stop_y_bit0",     Brush::gradient_color_stop_y_bit0)
    .add_macro("fastuidraw_color_stop_y_num_bits", Brush::gradient_color_stop_y_num_bits)

    .add_macro("fastuidraw_shader_pen_num_blocks", number_data_blocks(alignment, Brush::pen_data_size))
    .add_macro("fastuidraw_shader_image_num_blocks", number_data_blocks(alignment, Brush::image_data_size))
    .add_macro("fastuidraw_shader_linear_gradient_num_blocks", number_data_blocks(alignment, Brush::linear_gradient_data_size))
    .add_macro("fastuidraw_shader_radial_gradient_num_blocks", number_data_blocks(alignment, Brush::radial_gradient_data_size))
    .add_macro("fastuidraw_shader_repeat_window_num_blocks", number_data_blocks(alignment, Brush::repeat_window_data_size))
    .add_macro("fastuidraw_shader_transformation_matrix_num_blocks", number_data_blocks(alignment, Brush::transformation_matrix_data_size))
    .add_macro("fastuidraw_shader_transformation_translation_num_blocks", number_data_blocks(alignment, Brush::transformation_translation_data_size))

    .add_macro("fastuidraw_z_bit0", z_bit0)
    .add_macro("fastuidraw_z_num_bits", z_num_bits)
    .add_macro("fastuidraw_blend_shader_bit0", blend_shader_bit0)
    .add_macro("fastuidraw_blend_shader_num_bits", blend_shader_num_bits)

    .add_macro("fastuidraw_stroke_edge_point", StrokedPath::edge_point)
    .add_macro("fastuidraw_stroke_start_edge_point", StrokedPath::start_edge_point)
    .add_macro("fastuidraw_stroke_end_edge_point", StrokedPath::end_edge_point)
    .add_macro("fastuidraw_stroke_number_edge_point_types", StrokedPath::number_edge_point_types)
    .add_macro("fastuidraw_stroke_start_contour_point", StrokedPath::start_contour_point)
    .add_macro("fastuidraw_stroke_end_contour_point", StrokedPath::end_contour_point)
    .add_macro("fastuidraw_stroke_rounded_join_point", StrokedPath::rounded_join_point)
    .add_macro("fastuidraw_stroke_miter_join_point", StrokedPath::miter_join_point)
    .add_macro("fastuidraw_stroke_rounded_cap_point", StrokedPath::rounded_cap_point)
    .add_macro("fastuidraw_stroke_square_cap_point", StrokedPath::square_cap_point)
    .add_macro("fastuidraw_stroke_point_type_mask", StrokedPath::point_type_mask)
    .add_macro("fastuidraw_stroke_sin_sign_mask", StrokedPath::sin_sign_mask)
    .add_macro("fastuidraw_stroke_normal0_y_sign_mask", StrokedPath::normal0_y_sign_mask)
    .add_macro("fastuidraw_stroke_normal1_y_sign_mask", StrokedPath::normal1_y_sign_mask)

    .add_macro("fastuidraw_stroke_dashed_no_caps_close", PainterEnums::dashed_no_caps_closed)
    .add_macro("fastuidraw_stroke_dashed_rounded_caps_closed", PainterEnums::dashed_rounded_caps_closed)
    .add_macro("fastuidraw_stroke_dashed_square_caps_closed", PainterEnums::dashed_square_caps_closed)
    .add_macro("fastuidraw_stroke_dashed_no_caps", PainterEnums::dashed_no_caps)
    .add_macro("fastuidraw_stroke_dashed_rounded_caps", PainterEnums::dashed_rounded_caps)
    .add_macro("fastuidraw_stroke_dashed_square_caps", PainterEnums::dashed_square_caps)
    .add_macro("fastuidraw_stroke_no_dashes", PainterEnums::number_dashed_cap_styles);
}

void
add_texture_size_constants(ShaderSource &src,
                           const gl::PainterBackendGL::params &P)
{
  ivec2 glyph_atlas_size;
  unsigned int image_atlas_size, colorstop_atlas_size;

  glyph_atlas_size = ivec2(P.glyph_atlas()->param_values().texel_store_dimensions());
  image_atlas_size = 1 << (P.image_atlas()->param_values().log2_color_tile_size()
                           + P.image_atlas()->param_values().log2_num_color_tiles_per_row_per_col());
  colorstop_atlas_size = P.colorstop_atlas()->param_values().width();

  src
    .add_macro("fastuidraw_glyphTexelStore_size_x", glyph_atlas_size.x())
    .add_macro("fastuidraw_glyphTexelStore_size_y", glyph_atlas_size.y())
    .add_macro("fastuidraw_glyphTexelStore_size", "ivec2(fastuidraw_glyphTexelStore_size_x, fastuidraw_glyphTexelStore_size_y)")
    .add_macro("fastuidraw_imageAtlas_size", image_atlas_size)
    .add_macro("fastuidraw_colorStopAtlas_size", colorstop_atlas_size)
    .add_macro("fastuidraw_glyphTexelStore_size_reciprocal_x", "(1.0 / float(fastuidraw_glyphTexelStore_size_x) )")
    .add_macro("fastuidraw_glyphTexelStore_size_reciprocal_y", "(1.0 / float(fastuidraw_glyphTexelStore_size_y) )")
    .add_macro("fastuidraw_glyphTexelStore_size_reciprocal",
               "vec2(fastuidraw_glyphTexelStore_size_reciprocal_x, fastuidraw_glyphTexelStore_size_reciprocal_y)")
    .add_macro("fastuidraw_imageAtlas_size_reciprocal", "(1.0 / float(fastuidraw_imageAtlas_size) )")
    .add_macro("fastuidraw_colorStopAtlas_size_reciprocal", "(1.0 / float(fastuidraw_colorStopAtlas_size) )");
}

void
stream_declare_varyings(std::ostream &str,
                        size_t uint_count, size_t int_count,
                        const_c_array<size_t> float_counts)
{
  stream_declare_varyings_type(str, uint_count, "flat", "uint", uint_varying_label());
  stream_declare_varyings_type(str, int_count, "flat", "int", int_varying_label());

  stream_declare_varyings_type(str, float_counts[varying_list::interpolation_smooth],
                               "", "float", float_varying_label(varying_list::interpolation_smooth));

  stream_declare_varyings_type(str, float_counts[varying_list::interpolation_flat],
                               "flat", "float", float_varying_label(varying_list::interpolation_flat));

  stream_declare_varyings_type(str, float_counts[varying_list::interpolation_noperspective],
                               "noperspective", "float", float_varying_label(varying_list::interpolation_noperspective));
}

void
stream_unpack_code(unsigned int alignment,
                   ShaderSource &str)
{
  using namespace fastuidraw::PainterPacking;
  using namespace fastuidraw::PainterPacking::Brush;

  {
    shader_unpack_value_set<pen_data_size> labels;
    labels
      .set(pen_red_offset, ".r")
      .set(pen_green_offset, ".g")
      .set(pen_blue_offset, ".b")
      .set(pen_alpha_offset, ".a")
      .stream_unpack_function(alignment, str, "fastuidraw_read_pen_color", "vec4");
  }

  {
    /* Matrics in GLSL are [column][row], that is why
       one sees the transposing to the loads
    */
    shader_unpack_value_set<transformation_matrix_data_size> labels;
    labels
      .set(transformation_matrix_m00_offset, "[0][0]")
      .set(transformation_matrix_m10_offset, "[0][1]")
      .set(transformation_matrix_m01_offset, "[1][0]")
      .set(transformation_matrix_m11_offset, "[1][1]")
      .stream_unpack_function(alignment, str,
                              "fastuidraw_read_brush_transformation_matrix",
                              "mat2");
  }

  {
    shader_unpack_value_set<transformation_translation_data_size> labels;
    labels
      .set(transformation_translation_x_offset, ".x")
      .set(transformation_translation_y_offset, ".y")
      .stream_unpack_function(alignment, str,
                              "fastuidraw_read_brush_transformation_translation",
                              "vec2");
  }

  {
    shader_unpack_value_set<repeat_window_data_size> labels;
    labels
      .set(repeat_window_x_offset, ".xy.x")
      .set(repeat_window_y_offset, ".xy.y")
      .set(repeat_window_width_offset, ".wh.x")
      .set(repeat_window_height_offset, ".wh.y")
      .stream_unpack_function(alignment, str,
                              "fastuidraw_read_brush_repeat_window",
                              "fastuidraw_brush_repeat_window");
  }

  {
    shader_unpack_value_set<image_data_size> labels;
    labels
      .set(image_atlas_location_xyz_offset, ".image_atlas_location_xyz", shader_unpack_value::uint_type)
      .set(image_size_xy_offset, ".image_size_xy", shader_unpack_value::uint_type)
      .set(image_start_xy_offset, ".image_start_xy", shader_unpack_value::uint_type)
      .stream_unpack_function(alignment, str,
                              "fastuidraw_read_brush_image_raw_data",
                              "fastuidraw_brush_image_data_raw");
  }

  {
    shader_unpack_value_set<linear_gradient_data_size> labels;
    labels
      .set(gradient_p0_x_offset, ".p0.x")
      .set(gradient_p0_y_offset, ".p0.y")
      .set(gradient_p1_x_offset, ".p1.x")
      .set(gradient_p1_y_offset, ".p1.y")
      .set(gradient_color_stop_xy_offset, ".color_stop_sequence_xy", shader_unpack_value::uint_type)
      .set(gradient_color_stop_length_offset, ".color_stop_sequence_length", shader_unpack_value::uint_type)
      .stream_unpack_function(alignment, str,
                              "fastuidraw_read_brush_linear_gradient_data",
                              "fastuidraw_brush_gradient_raw");
  }

  {
    shader_unpack_value_set<radial_gradient_data_size> labels;
    labels
      .set(gradient_p0_x_offset, ".p0.x")
      .set(gradient_p0_y_offset, ".p0.y")
      .set(gradient_p1_x_offset, ".p1.x")
      .set(gradient_p1_y_offset, ".p1.y")
      .set(gradient_color_stop_xy_offset, ".color_stop_sequence_xy", shader_unpack_value::uint_type)
      .set(gradient_color_stop_length_offset, ".color_stop_sequence_length", shader_unpack_value::uint_type)
      .set(gradient_start_radius_offset, ".r0")
      .set(gradient_end_radius_offset, ".r1")
      .stream_unpack_function(alignment, str,
                              "fastuidraw_read_brush_radial_gradient_data",
                              "fastuidraw_brush_gradient_raw");
  }

  {
    shader_unpack_value_set<header_size> labels;
    labels
      .set(clip_equations_offset, ".clipping_location", shader_unpack_value::uint_type)
      .set(item_matrix_offset, ".item_matrix_location", shader_unpack_value::uint_type)
      .set(brush_shader_data_offset, ".brush_shader_data_location", shader_unpack_value::uint_type)
      .set(item_shader_data_offset, ".item_shader_data_location", shader_unpack_value::uint_type)
      .set(blend_shader_data_offset, ".blend_shader_data_location", shader_unpack_value::uint_type)
      .set(item_shader_offset, ".item_shader", shader_unpack_value::uint_type)
      .set(brush_shader_offset, ".brush_shader", shader_unpack_value::uint_type)
      .set(z_blend_shader_offset, ".z_blend_shader_raw", shader_unpack_value::uint_type)
      .stream_unpack_function(alignment, str,
                              "fastuidraw_read_header",
                              "fastuidraw_shader_header", false);
  }

  {
    shader_unpack_value_set<clip_equations_data_size> labels;
    labels
      .set(clip0_coeff_x, ".clip0.x")
      .set(clip0_coeff_y, ".clip0.y")
      .set(clip0_coeff_w, ".clip0.z")

      .set(clip1_coeff_x, ".clip1.x")
      .set(clip1_coeff_y, ".clip1.y")
      .set(clip1_coeff_w, ".clip1.z")

      .set(clip2_coeff_x, ".clip2.x")
      .set(clip2_coeff_y, ".clip2.y")
      .set(clip2_coeff_w, ".clip2.z")

      .set(clip3_coeff_x, ".clip3.x")
      .set(clip3_coeff_y, ".clip3.y")
      .set(clip3_coeff_w, ".clip3.z")

      .stream_unpack_function(alignment, str,
                              "fastuidraw_read_clipping",
                              "fastuidraw_clipping_data", false);
  }

  {
    /* Matrics in GLSL are [column][row], that is why
       one sees the transposing to the loads
    */
    shader_unpack_value_set<item_matrix_data_size> labels;
    labels
      .set(item_matrix_m00_offset, "[0][0]")
      .set(item_matrix_m10_offset, "[0][1]")
      .set(item_matrix_m20_offset, "[0][2]")
      .set(item_matrix_m01_offset, "[1][0]")
      .set(item_matrix_m11_offset, "[1][1]")
      .set(item_matrix_m21_offset, "[1][2]")
      .set(item_matrix_m02_offset, "[2][0]")
      .set(item_matrix_m12_offset, "[2][1]")
      .set(item_matrix_m22_offset, "[2][2]")
      .stream_unpack_function(alignment, str,
                              "fastuidraw_read_item_matrix", "mat3", false);
  }

  {
    shader_unpack_value_set<PainterStrokeParams::stroke_data_size> labels;
    labels
      .set(PainterStrokeParams::stroke_width_offset, ".width")
      .set(PainterStrokeParams::stroke_miter_limit_offset, ".miter_limit")
      .stream_unpack_function(alignment, str,
                              "fastuidraw_read_stroking_params",
                              "fastuidraw_stroking_params",
                              true);
  }
}


void
stream_uber_vert_shader(bool use_switch,
                        ShaderSource &vert,
                        const_c_array<reference_counted_ptr<PainterItemShaderGLSL> > item_shaders)
{
  UberShaderStreamer<PainterItemShaderGLSL>::stream_uber(use_switch, vert, item_shaders,
                                                         &PainterItemShaderGLSL::vertex_src,
                                                         &pre_stream_varyings, &post_stream_varyings,
                                                         "vec4", "fastuidraw_run_vert_shader(in fastuidraw_shader_header h, out uint add_z)",
                                                         "fastuidraw_gl_vert_main",
                                                         ", fastuidraw_primary_attribute, fastuidraw_secondary_attribute, "
                                                         "fastuidraw_uint_attribute, h.item_shader_data_location, add_z",
                                                         "h.item_shader");
}

void
stream_uber_frag_shader(bool use_switch,
                        ShaderSource &frag,
                        const_c_array<reference_counted_ptr<PainterItemShaderGLSL> > item_shaders)
{
  UberShaderStreamer<PainterItemShaderGLSL>::stream_uber(use_switch, frag, item_shaders,
                                                         &PainterItemShaderGLSL::fragment_src,
                                                         &pre_stream_varyings, &post_stream_varyings,
                                                         "vec4",
                                                         "fastuidraw_run_frag_shader(in uint frag_shader, "
                                                         "in uint frag_shader_data_location)",
                                                         "fastuidraw_gl_frag_main", ", frag_shader_data_location",
                                                         "frag_shader");
}

void
stream_uber_blend_shader(bool use_switch,
                         ShaderSource &frag,
                         const_c_array<reference_counted_ptr<PainterBlendShaderGLSL> > shaders,
                         enum PainterBlendShader::shader_type tp)
{
  std::string sub_func_name, func_name, sub_func_args;

  switch(tp)
    {
    default:
      assert("Unknown blend_code_type!");
      //fall through
    case PainterBlendShader::single_src:
      func_name = "fastuidraw_run_blend_shader(in uint blend_shader, in uint blend_shader_data_location, in vec4 in_src, out vec4 out_src)";
      sub_func_name = "fastuidraw_gl_compute_blend_value";
      sub_func_args = ", blend_shader_data_location, in_src, out_src";
      break;

    case PainterBlendShader::dual_src:
      func_name = "fastuidraw_run_blend_shader(in uint blend_shader, in uint blend_shader_data_location, in vec4 color0, out vec4 src0, out vec4 src1)";
      sub_func_name = "fastuidraw_gl_compute_blend_factors";
      sub_func_args = ", blend_shader_data_location, color0, src0, src1";
      break;

    case PainterBlendShader::framebuffer_fetch:
      func_name = "fastuidraw_run_blend_shader(in uint blend_shader, in uint blend_shader_data_location, in vec4 in_src, in vec4 in_fb, out vec4 out_src)";
      sub_func_name = "fastuidraw_gl_compute_post_blended_value";
      sub_func_args = ", blend_shader_data_location, in_src, in_fb, out_src";
      break;
    }
  UberShaderStreamer<PainterBlendShaderGLSL>::stream_uber(use_switch, frag, shaders,
                                                          &PainterBlendShaderGLSL::blend_src,
                                                          "void", func_name,
                                                          sub_func_name, sub_func_args, "blend_shader");
}



}}}}
