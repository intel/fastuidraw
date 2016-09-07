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
  stream_varyings_as_local_variables_array(fastuidraw::glsl::ShaderSource &vert,
                                           fastuidraw::const_c_array<const char*> p,
                                           const char *type)
  {
    std::ostringstream ostr;
    for(unsigned int i = 0, endi = p.size(); i < endi; ++i)
      {
        ostr << type << " " << p[i] << ";\n";
      }
    vert.add_source(ostr.str().c_str(), fastuidraw::glsl::ShaderSource::from_string);
  }

  unsigned int
  compute_special_index(size_t sz)
  {
    if(sz % 4 == 1)
      {
        return sz - 1;
      }
    else
      {
        return sz;
      }
  }

  void
  stream_alias_varyings_array(const char *append_to_name,
                              fastuidraw::glsl::ShaderSource &vert,
                              fastuidraw::const_c_array<const char*> p,
                              const std::string &s, bool define,
                              unsigned int special_index)
  {
    const char *ext = "xyzw";
    for(unsigned int i = 0; i < p.size(); ++i)
      {
        if(define)
          {
            std::ostringstream str;
            str << s << append_to_name << i / 4;
            if(i != special_index)
              {
                str << "." << ext[i % 4];
              }
            vert.add_macro(p[i], str.str().c_str());
          }
        else
          {
            vert.remove_macro(p[i]);
          }
      }
  }

  unsigned int
  stream_declare_varyings_type(const char *append_to_name, unsigned int location,
                               std::ostream &str, unsigned int cnt,
                               const char *qualifier,
                               const char *types[],
                               const char *name)
  {
    unsigned int extra_over_4;
    unsigned int return_value;

    /* pack the data into vec4 types an duse macros
       to do the rest.
     */
    return_value = cnt / 4;
    for(unsigned int i = 0; i < return_value; ++i, ++location)
      {
        str << "FASTUIDRAW_LAYOUT_VARYING(" << location << ") "
            << qualifier << " fastuidraw_varying"
            << " " << types[3] << " "
            << name << append_to_name << i << ";\n\n";
      }

    extra_over_4 = cnt % 4;
    if(extra_over_4 > 0)
      {
        str << "FASTUIDRAW_LAYOUT_VARYING(" << location << ") "
            << qualifier << " fastuidraw_varying"
            << " " << types[extra_over_4 - 1] << " "
            << name << append_to_name << return_value << ";\n\n";
        ++return_value;
      }

    return return_value;
  }

  unsigned int
  stream_declare_varyings(const char *append_to_name, std::ostream &str,
                          size_t uint_count, size_t int_count,
                          fastuidraw::const_c_array<size_t> float_counts,
                          unsigned int start_slot)
  {
    unsigned int number_slots(0);
    const char *uint_labels[]=
      {
        "uint",
        "uvec2",
        "uvec3",
        "uvec4",
      };
    const char *int_labels[]=
      {
        "int",
        "ivec2",
        "ivec3",
        "ivec4",
      };
    const char *float_labels[]=
      {
        "float",
        "vec2",
        "vec3",
        "vec4",
      };

    number_slots +=
      stream_declare_varyings_type(append_to_name, start_slot + number_slots, str,
                                   uint_count, "flat", uint_labels, uint_varying_label());

    number_slots +=
      stream_declare_varyings_type(append_to_name, start_slot + number_slots, str,
                                   int_count, "flat", int_labels, int_varying_label());

    number_slots +=
      stream_declare_varyings_type(append_to_name, start_slot + number_slots, str,
                                   float_counts[fastuidraw::glsl::varying_list::interpolation_smooth],
                                   "", float_labels,
                                   float_varying_label(fastuidraw::glsl::varying_list::interpolation_smooth));

    number_slots +=
      stream_declare_varyings_type(append_to_name, start_slot + number_slots, str,
                                   float_counts[fastuidraw::glsl::varying_list::interpolation_flat],
                                   "flat", float_labels,
                                   float_varying_label(fastuidraw::glsl::varying_list::interpolation_flat));

    number_slots +=
      stream_declare_varyings_type(append_to_name, start_slot + number_slots, str,
                                   float_counts[fastuidraw::glsl::varying_list::interpolation_noperspective],
                                   "noperspective", float_labels,
                                   float_varying_label(fastuidraw::glsl::varying_list::interpolation_noperspective));
    return number_slots;
  }


  void
  pre_stream_varyings(fastuidraw::glsl::ShaderSource &dst,
                      const fastuidraw::reference_counted_ptr<fastuidraw::glsl::PainterItemShaderGLSL> &sh,
                      const fastuidraw::glsl::detail::DeclareVaryingsStringDatum &datum)
  {
    fastuidraw::glsl::detail::stream_alias_varyings("_shader", dst, sh->varyings(), true, datum);
  }

  void
  post_stream_varyings(fastuidraw::glsl::ShaderSource &dst,
                       const fastuidraw::reference_counted_ptr<fastuidraw::glsl::PainterItemShaderGLSL> &sh,
                       const fastuidraw::glsl::detail::DeclareVaryingsStringDatum &datum)
  {
    fastuidraw::glsl::detail::stream_alias_varyings("_shader", dst, sh->varyings(), false, datum);
  }

  template<typename T>
  class UberShaderStreamer
  {
  public:
    typedef fastuidraw::glsl::ShaderSource ShaderSource;
    typedef fastuidraw::reference_counted_ptr<T> ref_type;
    typedef fastuidraw::const_c_array<ref_type> array_type;
    typedef const ShaderSource& (T::*get_src_type)(void) const;
    typedef fastuidraw::glsl::detail::DeclareVaryingsStringDatum Datum;
    typedef void (*pre_post_stream_type)(ShaderSource &dst, const ref_type &sh, const Datum &datum);

    static
    void
    stream_nothing(ShaderSource &, const ref_type &,
                   const Datum &)
    {}

    static
    void
    stream_uber(bool use_switch, ShaderSource &dst, array_type shaders,
                get_src_type get_src,
                pre_post_stream_type pre_stream,
                pre_post_stream_type post_stream,
                const Datum &datum,
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
                  fastuidraw::glsl::detail::DeclareVaryingsStringDatum(),
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
            const Datum &datum,
            const std::string &return_type,
            const std::string &uber_func_with_args,
            const std::string &shader_main,
            const std::string &shader_args, //of the form ", arg1, arg2,..,argN" or empty string
            const std::string &shader_id)
{
  /* first stream all of the item_shaders with predefined macros. */
  for(unsigned int i = 0; i < shaders.size(); ++i)
    {
      pre_stream(dst, shaders[i], datum);

      std::ostringstream str;
      str << shader_main << shaders[i]->ID();
      dst
        .add_macro(shader_main.c_str(), str.str().c_str())
        .add_source((shaders[i].get()->*get_src)())
        .remove_macro(shader_main.c_str());

      post_stream(dst, shaders[i], datum);
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
              str << "    else ";
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


namespace fastuidraw { namespace glsl { namespace detail {

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

    .add_macro("fastuidraw_stroke_offset_edge", StrokedPath::offset_edge)
    .add_macro("fastuidraw_stroke_offset_rounded_join", StrokedPath::offset_rounded_join)
    .add_macro("fastuidraw_stroke_offset_miter_join", StrokedPath::offset_miter_join)
    .add_macro("fastuidraw_stroke_offset_rounded_cap", StrokedPath::offset_rounded_cap)
    .add_macro("fastuidraw_stroke_offset_square_cap", StrokedPath::offset_square_cap)
    .add_macro("fastuidraw_stroke_offset_cap_join", StrokedPath::offset_cap_join)
    .add_macro("fastuidraw_stroke_offset_type_bit0", StrokedPath::offset_type_bit0)
    .add_macro("fastuidraw_stroke_offset_type_num_bits", StrokedPath::offset_type_num_bits)
    .add_macro("fastuidraw_stroke_sin_sign_mask", StrokedPath::sin_sign_mask)
    .add_macro("fastuidraw_stroke_normal0_y_sign_mask", StrokedPath::normal0_y_sign_mask)
    .add_macro("fastuidraw_stroke_normal1_y_sign_mask", StrokedPath::normal1_y_sign_mask)
    .add_macro("fastuidraw_stroke_boundary_bit0", StrokedPath::boundary_bit0)
    .add_macro("fastuidraw_stroke_boundary_num_bits", StrokedPath::boundary_num_bits)
    .add_macro("fastuidraw_stroke_depth_bit0", StrokedPath::depth_bit0)
    .add_macro("fastuidraw_stroke_depth_num_bits", StrokedPath::depth_num_bits)
    .add_macro("fastuidraw_stroke_undashed", PainterEnums::number_cap_styles)
    .add_macro("fastuidraw_stroke_dashed_no_caps", PainterEnums::no_caps)
    .add_macro("fastuidraw_stroke_dashed_rounded_caps", PainterEnums::rounded_caps)
    .add_macro("fastuidraw_stroke_dashed_square_caps", PainterEnums::square_caps)

    .add_macro("fastuidraw_data_store_alignment", alignment)
    .add_macro("fastuidraw_data_store_block_offset1", number_data_blocks(alignment, 1))
    .add_macro("fastuidraw_data_store_block_offset2", number_data_blocks(alignment, 2))
    .add_macro("fastuidraw_data_store_block_offset3", number_data_blocks(alignment, 3))
    .add_macro("fastuidraw_data_store_block_offset4", number_data_blocks(alignment, 4));
}

void
add_texture_size_constants(ShaderSource &src,
                           const GlyphAtlas *glyph_atlas,
                           const ImageAtlas *image_atlas,
                           const ColorStopAtlas *colorstop_atlas)
{
  ivec3 glyph_atlas_size, image_atlas_size;
  unsigned int colorstop_atlas_size;

  glyph_atlas_size = glyph_atlas->texel_store()->dimensions();
  image_atlas_size = image_atlas->color_store()->dimensions();
  colorstop_atlas_size = colorstop_atlas->backing_store()->dimensions().x();

  src
    .add_macro("fastuidraw_glyphTexelStore_size_x", glyph_atlas_size.x())
    .add_macro("fastuidraw_glyphTexelStore_size_y", glyph_atlas_size.y())
    .add_macro("fastuidraw_glyphTexelStore_size", "ivec2(fastuidraw_glyphTexelStore_size_x, fastuidraw_glyphTexelStore_size_y)")
    .add_macro("fastuidraw_glyphTexelStore_size_reciprocal_x", "(1.0 / float(fastuidraw_glyphTexelStore_size_x) )")
    .add_macro("fastuidraw_glyphTexelStore_size_reciprocal_y", "(1.0 / float(fastuidraw_glyphTexelStore_size_y) )")
    .add_macro("fastuidraw_glyphTexelStore_size_reciprocal",
               "vec2(fastuidraw_glyphTexelStore_size_reciprocal_x, fastuidraw_glyphTexelStore_size_reciprocal_y)")

    .add_macro("fastuidraw_imageAtlas_size_x", image_atlas_size.x())
    .add_macro("fastuidraw_imageAtlas_size_y", image_atlas_size.y())
    .add_macro("fastuidraw_imageAtlas_size", "ivec2(fastuidraw_imageAtlas_size_x, fastuidraw_imageAtlas_size_y)")
    .add_macro("fastuidraw_imageAtlas_size_reciprocal_x", "(1.0 / float(fastuidraw_imageAtlas_size_x) )")
    .add_macro("fastuidraw_imageAtlas_size_reciprocal_y", "(1.0 / float(fastuidraw_imageAtlas_size_y) )")
    .add_macro("fastuidraw_imageAtlas_size_reciprocal", "vec2(fastuidraw_imageAtlas_size_reciprocal_x, fastuidraw_imageAtlas_size_reciprocal_y)")

    .add_macro("fastuidraw_colorStopAtlas_size", colorstop_atlas_size)
    .add_macro("fastuidraw_colorStopAtlas_size_reciprocal", "(1.0 / float(fastuidraw_colorStopAtlas_size) )");
}

void
stream_alias_varyings(const char *append_to_name,
                      ShaderSource &shader,
                      const varying_list &p,
                      bool define,
                      const DeclareVaryingsStringDatum &datum)
{
  stream_alias_varyings_array(append_to_name, shader, p.uints(), uint_varying_label(),
                              define, datum.m_uint_special_index);
  stream_alias_varyings_array(append_to_name, shader, p.ints(), int_varying_label(),
                              define, datum.m_int_special_index);

  for(unsigned int i = 0; i < varying_list::interpolation_number_types; ++i)
    {
      enum varying_list::interpolation_qualifier_t q;
      q = static_cast<enum varying_list::interpolation_qualifier_t>(i);
      stream_alias_varyings_array(append_to_name, shader, p.floats(q), float_varying_label(q),
                                  define, datum.m_float_special_index[q]);
    }
}

void
stream_varyings_as_local_variables(ShaderSource &shader, const varying_list &p)
{
  stream_varyings_as_local_variables_array(shader, p.uints(), "uint");
  stream_varyings_as_local_variables_array(shader, p.ints(), "uint");

  for(unsigned int i = 0; i < varying_list::interpolation_number_types; ++i)
    {
      enum varying_list::interpolation_qualifier_t q;
      q = static_cast<enum varying_list::interpolation_qualifier_t>(i);
      stream_varyings_as_local_variables_array(shader, p.floats(q), "float");
    }
}

std::string
declare_varyings_string(const char *append_to_name,
                        size_t uint_count, size_t int_count,
                        const_c_array<size_t> float_counts,
                        unsigned int *slot,
                        DeclareVaryingsStringDatum *datum)
{
  std::ostringstream ostr;

  *slot += stream_declare_varyings(append_to_name, ostr, uint_count, int_count, float_counts, *slot);

  datum->m_uint_special_index = compute_special_index(uint_count);
  datum->m_int_special_index = compute_special_index(int_count);
  for(unsigned int i = 0; i < varying_list::interpolation_number_types; ++i)
    {
      datum->m_float_special_index[i] = compute_special_index(float_counts[i]);
    }

  return ostr.str();
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

  {
    shader_unpack_value_set<PainterDashedStrokeParams::stroke_static_data_size> labels;
    labels
      .set(PainterDashedStrokeParams::stroke_width_offset, ".width")
      .set(PainterDashedStrokeParams::stroke_miter_limit_offset, ".miter_limit")
      .set(PainterDashedStrokeParams::stroke_dash_offset_offset, ".dash_offset")
      .set(PainterDashedStrokeParams::stroke_total_length_offset, ".total_length")
      .stream_unpack_function(alignment, str,
                              "fastuidraw_read_dashed_stroking_params_header",
                              "fastuidraw_dashed_stroking_params_header",
                              true);
  }
}


void
stream_uber_vert_shader(bool use_switch,
                        ShaderSource &vert,
                        const_c_array<reference_counted_ptr<PainterItemShaderGLSL> > item_shaders,
                        const DeclareVaryingsStringDatum &datum)
{
  UberShaderStreamer<PainterItemShaderGLSL>::stream_uber(use_switch, vert, item_shaders,
                                                         &PainterItemShaderGLSL::vertex_src,
                                                         &pre_stream_varyings, &post_stream_varyings, datum,
                                                         "vec4", "fastuidraw_run_vert_shader(in fastuidraw_shader_header h, out uint add_z)",
                                                         "fastuidraw_gl_vert_main",
                                                         ", fastuidraw_primary_attribute, fastuidraw_secondary_attribute, "
                                                         "fastuidraw_uint_attribute, h.item_shader_data_location, add_z",
                                                         "h.item_shader");
}

void
stream_uber_frag_shader(bool use_switch,
                        ShaderSource &frag,
                        const_c_array<reference_counted_ptr<PainterItemShaderGLSL> > item_shaders,
                        const DeclareVaryingsStringDatum &datum)
{
  UberShaderStreamer<PainterItemShaderGLSL>::stream_uber(use_switch, frag, item_shaders,
                                                         &PainterItemShaderGLSL::fragment_src,
                                                         &pre_stream_varyings, &post_stream_varyings, datum,
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

}}}
