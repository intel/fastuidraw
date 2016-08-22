#pragma once

#include <list>
#include <map>
#include <sstream>
#include <vector>

#include <fastuidraw/gl_backend/gl_program.hpp>
#include <fastuidraw/gl_backend/gl_get.hpp>
#include <fastuidraw/gl_backend/gl_context_properties.hpp>
#include <fastuidraw/gl_backend/painter_backend_gl.hpp>

#include <fastuidraw/stroked_path.hpp>
#include <fastuidraw/painter/painter_brush.hpp>
#include <fastuidraw/painter/packing/painter_packing_brush.hpp>
#include <fastuidraw/painter/packing/painter_packing_enums.hpp>
#include <fastuidraw/painter/painter_stroke_value.hpp>

namespace fastuidraw { namespace gl { namespace detail { namespace shader_builder {

/*
  enumeration for blend mode type
*/
enum blending_code_type
  {
    single_src_blending,
    dual_src_blending,
    framebuffer_fetch_blending,
  };

unsigned int
number_data_blocks(unsigned int alignment, unsigned int sz);

void
add_enums(unsigned int alignment, glsl::ShaderSource &src);

void
add_texture_size_constants(glsl::ShaderSource &src,
                           const PainterBackendGL::params &P);

const char*
float_varying_label(unsigned int t);

const char*
int_varying_label(void);

const char*
uint_varying_label(void);

void
stream_declare_varyings(std::ostream &str, unsigned int cnt,
                        const std::string &qualifier,
                        const std::string &type,
                        const std::string &name);

void
stream_declare_varyings(std::ostream &str,
                        size_t uint_count, size_t int_count,
                        const_c_array<size_t> float_counts);

void
stream_alias_varyings(glsl::ShaderSource &vert,
                      const_c_array<const char*> p,
                      const std::string &s, bool define);

void
stream_alias_varyings(glsl::ShaderSource &shader,
                      const varying_list &p,
                      bool define);
void
pre_stream_varyings(glsl::ShaderSource &dst,
                    const reference_counted_ptr<PainterItemShaderGL> &sh);

void
post_stream_varyings(glsl::ShaderSource &dst,
                     const reference_counted_ptr<PainterItemShaderGL> &sh);

void
stream_unpack_code(unsigned int alignment,
                   glsl::ShaderSource &str);

void
stream_uber_vert_shader(bool use_switch, glsl::ShaderSource &vert,
                        const_c_array<reference_counted_ptr<PainterItemShaderGL> > item_shaders);

void
stream_uber_frag_shader(bool use_switch, glsl::ShaderSource &frag,
                        const_c_array<reference_counted_ptr<PainterItemShaderGL> > item_shaders);


void
stream_uber_blend_shader(bool use_switch, glsl::ShaderSource &frag,
                         const_c_array<reference_counted_ptr<BlendShaderSourceCode> > blend_shaders,
                         enum blending_code_type tp);
template<typename T>
class UberShaderStreamer
{
public:
  typedef reference_counted_ptr<T> ref_type;
  typedef const_c_array<ref_type> array_type;
  typedef const glsl::ShaderSource& (T::*get_src_type)(void) const;
  typedef void (*pre_post_stream_type)(glsl::ShaderSource &dst, const ref_type &sh);

  static
  void
  stream_nothing(glsl::ShaderSource &, const ref_type &)
  {}

  static
  void
  stream_uber(bool use_switch, glsl::ShaderSource &dst, array_type shaders,
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
  stream_uber(bool use_switch, glsl::ShaderSource &dst, array_type shaders,
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

template<typename T>
void
UberShaderStreamer<T>::
stream_uber(bool use_switch, glsl::ShaderSource &dst, array_type shaders,
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
  dst.add_source(str.str().c_str(), glsl::ShaderSource::from_string);
}

}}}}
