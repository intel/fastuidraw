/*!
 * \file uber_shader_builder.cpp
 * \brief file uber_shader_builder.cpp
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

#include <sstream>
#include <iomanip>

#include <private/glsl/uber_shader_builder.hpp>
#include <private/util_private.hpp>
#include <private/util_private_ostream.hpp>

namespace
{
  struct varying_type_information
  {
    fastuidraw::c_string m_qualifier;
    fastuidraw::vecN<fastuidraw::c_string, 4> m_types;
    fastuidraw::c_string m_fastuidraw_prefix;
    bool m_is_flat;
  };

  std::string
  make_name(fastuidraw::c_string name,
            unsigned int idx)
  {
    std::ostringstream str;

    str << name << idx;
    return str.str();
  }

  std::string
  replace_double_colon_with_double_D(const std::string &input)
  {
    std::string return_value(input);
    char *prev_char(nullptr);

    for (auto iter = return_value.begin(), end = return_value.end(); iter != end; ++iter)
      {
        if (prev_char && *prev_char == ':' && *iter == ':')
          {
            *prev_char = 'D';
            *iter = 'D';
          }
        prev_char = &(*iter);
      }

    return return_value;
  }

  bool
  has_double_colon(fastuidraw::c_string v)
  {
    FASTUIDRAWassert(v);

    char prev_char(0);
    for (const char *p = v; *p != 0; ++p)
      {
        if (prev_char == ':' && *p == ':')
          {
            return true;
          }
        prev_char = *p;
      }

    return false;
  }

  bool
  does_not_have_double_colon(fastuidraw::c_string v)
  {
    return !has_double_colon(v);
  }

  template<typename T>
  class StreamVaryingsHelper
  {
  public:
    StreamVaryingsHelper(bool for_fragment_shadering,
                         const fastuidraw::glsl::detail::UberShaderVaryings &src,
                         const fastuidraw::glsl::detail::AliasVaryingLocation &datum):
      m_src(src),
      m_datum(datum),
      m_for_fragment_shadering(for_fragment_shadering)
    {}

    void
    before_shader(fastuidraw::glsl::ShaderSource &dst,
                  const fastuidraw::reference_counted_ptr<const T> &sh) const
    {
      m_src.stream_alias_varyings(m_for_fragment_shadering, dst, sh->varyings(),
                                  true, m_datum, has_double_colon);
    }

    void
    after_shader(fastuidraw::glsl::ShaderSource &dst,
                 const fastuidraw::reference_counted_ptr<const T> &sh) const
    {
      m_src.stream_alias_varyings(m_for_fragment_shadering, dst, sh->varyings(),
                                  false, m_datum, has_double_colon);
    }

    void
    pre_source(fastuidraw::glsl::ShaderSource &dst,
               unsigned int depth, fastuidraw::c_string dep_name,
               const fastuidraw::reference_counted_ptr<const T> &sh) const
    {
      dst << "//PreSource, depth = " << depth << ", dep_name = " << dep_name << "\n";
      if (depth == 0)
        {
          m_src.stream_alias_varyings(m_for_fragment_shadering, dst, sh->varyings(),
                                      true, m_datum, does_not_have_double_colon);
        }
      else
        {
          for (unsigned int i = 0; i < fastuidraw::glsl::varying_list::interpolator_number_types; ++i)
            {
              enum fastuidraw::glsl::varying_list::interpolator_type_t q;
              q = static_cast<enum fastuidraw::glsl::varying_list::interpolator_type_t>(i);

              for (fastuidraw::c_string v : sh->varyings().varyings(q))
                {
                  if (does_not_have_double_colon(v))
                    {
                      std::string vv(replace_double_colon_with_double_D(v));
                      dst << "#define " << vv << " " << dep_name << "DD" << vv << "\n";
                    }
                }
            }
          fastuidraw::c_array<const fastuidraw::c_string> names(sh->varyings().alias_varying_names());
          fastuidraw::c_array<const fastuidraw::c_string> src_names(sh->varyings().alias_varying_source_names());

          FASTUIDRAWassert(names.size() == src_names.size());
          for (unsigned int i = 0; i < names.size(); ++i)
            {
              if (does_not_have_double_colon(names[i]))
                {
                  std::string name(replace_double_colon_with_double_D(names[i]));
                  std::string src_name(replace_double_colon_with_double_D(src_names[i]));
                  dst.add_macro(name.c_str(), src_name.c_str());
                }
            }
        }
    }

    void
    post_source(fastuidraw::glsl::ShaderSource &dst,
                unsigned int depth, fastuidraw::c_string dep_name,
                const fastuidraw::reference_counted_ptr<const T> &sh) const
    {
      dst << "//PostSource, depth = " << depth << ", dep_name = " << dep_name << "\n";
      if (depth == 0)
        {
          m_src.stream_alias_varyings(m_for_fragment_shadering, dst, sh->varyings(),
                                      false, m_datum, does_not_have_double_colon);
        }
      else
        {
          fastuidraw::c_array<const fastuidraw::c_string> names(sh->varyings().alias_varying_names());
          for (unsigned int i = 0; i < names.size(); ++i)
            {
              if (does_not_have_double_colon(names[i]))
                {
                  std::string name(replace_double_colon_with_double_D(names[i]));
                  dst.remove_macro(name.c_str());
                }
            }

          for (unsigned int i = 0; i < fastuidraw::glsl::varying_list::interpolator_number_types; ++i)
            {
              enum fastuidraw::glsl::varying_list::interpolator_type_t q;
              q = static_cast<enum fastuidraw::glsl::varying_list::interpolator_type_t>(i);

              for (fastuidraw::c_string v : sh->varyings().varyings(q))
                {
                  if (does_not_have_double_colon(v))
                    {
                      std::string vv(replace_double_colon_with_double_D(v));
                      dst << "#undef " << vv << "\n";
                    }
                }
            }
        }
    }

    void
    before_dependency(fastuidraw::glsl::ShaderSource &dst,
                      fastuidraw::c_string dep_name,
                      const fastuidraw::reference_counted_ptr<const T> &child_shader) const
    {
      dst << "// stream dependency varyings for " << dep_name << "\n";
      for (unsigned int i = 0; i < fastuidraw::glsl::varying_list::interpolator_number_types; ++i)
        {
          enum fastuidraw::glsl::varying_list::interpolator_type_t q;
          q = static_cast<enum fastuidraw::glsl::varying_list::interpolator_type_t>(i);

          for (fastuidraw::c_string v : child_shader->varyings().varyings(q))
            {
              if (has_double_colon(v))
                {
                  std::string vv(replace_double_colon_with_double_D(v));
                  dst << "#define " << vv << " " << dep_name << "DD" << vv << "\n";
                }
            }
        }
      fastuidraw::c_array<const fastuidraw::c_string> names(child_shader->varyings().alias_varying_names());
      fastuidraw::c_array<const fastuidraw::c_string> src_names(child_shader->varyings().alias_varying_source_names());

      FASTUIDRAWassert(names.size() == src_names.size());
      for (unsigned int i = 0; i < names.size(); ++i)
        {
          if (has_double_colon(names[i]))
            {
              std::string name(replace_double_colon_with_double_D(names[i]));
              std::string src_name(replace_double_colon_with_double_D(src_names[i]));
              dst.add_macro(name.c_str(), src_name.c_str());
            }
        }
    }

    void
    after_dependency(fastuidraw::glsl::ShaderSource &dst,
                     fastuidraw::c_string dep_name,
                     const fastuidraw::reference_counted_ptr<const T> &child_shader) const
    {
      dst << "// unstream dependency varyings for " << dep_name << "\n";
      fastuidraw::c_array<const fastuidraw::c_string> names(child_shader->varyings().alias_varying_names());
      for (unsigned int i = 0; i < names.size(); ++i)
        {
          if (has_double_colon(names[i]))
            {
              std::string name(replace_double_colon_with_double_D(names[i]));
              dst.remove_macro(name.c_str());
            }
        }

      for (unsigned int i = 0; i < fastuidraw::glsl::varying_list::interpolator_number_types; ++i)
        {
          enum fastuidraw::glsl::varying_list::interpolator_type_t q;
          q = static_cast<enum fastuidraw::glsl::varying_list::interpolator_type_t>(i);

          for (fastuidraw::c_string v : child_shader->varyings().varyings(q))
            {
              if (has_double_colon(v))
                {
                  std::string vv(replace_double_colon_with_double_D(v));
                  dst << "#undef " << vv << "\n";
                }
            }
        }
    }

  private:
    const fastuidraw::glsl::detail::UberShaderVaryings &m_src;
    const fastuidraw::glsl::detail::AliasVaryingLocation &m_datum;
    bool m_for_fragment_shadering;
  };

  template<>
  class StreamVaryingsHelper<fastuidraw::glsl::PainterBlendShaderGLSL>
  {
  public:
    void
    before_shader(fastuidraw::glsl::ShaderSource&,
                  const fastuidraw::reference_counted_ptr<const fastuidraw::glsl::PainterBlendShaderGLSL> &) const
    {}

    void
    after_shader(fastuidraw::glsl::ShaderSource&,
                 const fastuidraw::reference_counted_ptr<const fastuidraw::glsl::PainterBlendShaderGLSL> &) const
    {}

    void
    pre_source(fastuidraw::glsl::ShaderSource&,
               unsigned int, fastuidraw::c_string,
               const fastuidraw::reference_counted_ptr<const fastuidraw::glsl::PainterBlendShaderGLSL> &) const
    {}

    void
    post_source(fastuidraw::glsl::ShaderSource&,
                unsigned int, fastuidraw::c_string,
                const fastuidraw::reference_counted_ptr<const fastuidraw::glsl::PainterBlendShaderGLSL> &) const
    {}

    void
    before_dependency(fastuidraw::glsl::ShaderSource &,
                      fastuidraw::c_string,
                      const fastuidraw::reference_counted_ptr<const fastuidraw::glsl::PainterBlendShaderGLSL> &) const
    {}

    void
    after_dependency(fastuidraw::glsl::ShaderSource &,
                     fastuidraw::c_string,
                     const fastuidraw::reference_counted_ptr<const fastuidraw::glsl::PainterBlendShaderGLSL> &) const
    {}
  };

  template<typename T>
  class StreamSurroundSrcHelper
  {
  public:
    void
    pre_source(fastuidraw::glsl::ShaderSource&,
               const fastuidraw::reference_counted_ptr<const T> &) const
    {}

    void
    post_source(fastuidraw::glsl::ShaderSource&) const
    {}
  };

  template<>
  class StreamSurroundSrcHelper<fastuidraw::glsl::PainterBrushShaderGLSL>
  {
  public:
    StreamSurroundSrcHelper(void):
      m_count(0)
    {}

    void
    pre_source(fastuidraw::glsl::ShaderSource &dst,
               const fastuidraw::reference_counted_ptr<const fastuidraw::glsl::PainterBrushShaderGLSL> &sh) const
    {
      FASTUIDRAWunused(sh);
      dst << "\n#define fastuidraw_brush_start_context_texture " << m_count
          << "\n#define fastuidraw_brush_context_texture(X) "
          << "fastuidraw_context_texture[X + fastuidraw_brush_start_context_texture]\n";
      m_count += sh->number_context_textures();
    }

    void
    post_source(fastuidraw::glsl::ShaderSource &dst) const
    {
      dst << "\n#undef fastuidraw_brush_start_context_texture\n"
          << "#undef fastuidraw_brush_context_texture\n";
    }

  private:
    mutable unsigned int m_count;
  };

  template<typename T>
  fastuidraw::c_string
  item_shader_vert_name(const fastuidraw::reference_counted_ptr<const T> &)
  {
    return "fastuidraw_gl_vert_main";
  }

  template<typename T>
  fastuidraw::c_string
  item_shader_frag_name(const fastuidraw::reference_counted_ptr<const T> &)
  {
    return "fastuidraw_gl_frag_main";
  }

  template<>
  fastuidraw::c_string
  item_shader_vert_name(const fastuidraw::reference_counted_ptr<const fastuidraw::glsl::PainterBrushShaderGLSL> &)
  {
    return "fastuidraw_gl_vert_brush_main";
  }

  template<>
  fastuidraw::c_string
  item_shader_frag_name(const fastuidraw::reference_counted_ptr<const fastuidraw::glsl::PainterBrushShaderGLSL> &)
  {
    return "fastuidraw_gl_frag_brush_main";
  }

  fastuidraw::c_string
  blend_shader_name(const fastuidraw::reference_counted_ptr<const fastuidraw::glsl::PainterBlendShaderGLSL> &shader)
  {
    switch(shader->type())
      {
      case fastuidraw::PainterBlendShader::single_src:
        return "fastuidraw_gl_compute_blend_value";

      case fastuidraw::PainterBlendShader::dual_src:
        return "fastuidraw_gl_compute_blend_factors";

      case fastuidraw::PainterBlendShader::framebuffer_fetch:
        return "fastuidraw_gl_compute_post_blended_value";

      default:
        FASTUIDRAWassert(!"Bad PainterBlendShader::shader_type enum value");
        return "unknown";
      }
  }

  void
  add_macro_requirement(fastuidraw::glsl::ShaderSource &dst,
                        bool should_be_defined,
                        fastuidraw::c_string macro,
                        fastuidraw::c_string error_message)
  {
    fastuidraw::c_string not_cnd, msg;

    not_cnd = (should_be_defined) ? "!defined" : "defined";
    msg = (should_be_defined) ? "" : "not ";
    dst << "#if " << not_cnd << "(" << macro << ")\n"
        << "#error \"" << error_message << ": "
        << macro << " should " << msg << "be defined\"\n"
        << "#endif\n";
  }

  void
  add_macro_requirement(fastuidraw::glsl::ShaderSource &dst,
                        fastuidraw::c_string macro1,
                        fastuidraw::c_string macro2,
                        fastuidraw::c_string error_message)
  {
    dst << "#if (!defined(" << macro1 << ") && !defined(" << macro2 << ")) "
        << " || (defined(" << macro1 << ") && defined(" << macro2 << "))\n"
        << "#error \"" << error_message << ": exactly one of "
        << macro1 << " or " << macro2 << " should be defined\"\n"
        << "#endif\n";
  }

  template<typename T>
  class UberShaderStreamer
  {
  public:
    typedef fastuidraw::glsl::ShaderSource ShaderSource;
    typedef fastuidraw::reference_counted_ptr<T> ref_type;
    typedef fastuidraw::reference_counted_ptr<const T> ref_const_type;
    typedef fastuidraw::c_array<const ref_type> array_type;
    typedef const ShaderSource& (T::*get_src_type)(void) const;
    typedef fastuidraw::c_string (*get_main_name_type)(const ref_const_type&);

    static
    void
    stream_uber(bool use_switch, ShaderSource &dst, array_type shaders,
                get_src_type get_src, get_main_name_type get_main_name,
                const StreamVaryingsHelper<T> &stream_varyings_helper,
                const StreamSurroundSrcHelper<T> &stream_surround_src,
                const std::string &return_type,
                const std::string &uber_func_with_args,
                const std::string &shader_args, //of the form ", arg1, arg2,..,argN" or empty string
                const std::string &shader_id);

  private:
    static
    void
    stream_source(ShaderSource &dst, const std::string &prefix,
                  const ShaderSource &shader);

    static
    void
    stream_shader(ShaderSource &dst, const std::string &prefix,
                  const std::string &dep_name,
                  get_src_type get_src, get_main_name_type get_main_name,
                  const StreamVaryingsHelper<T> &stream_varyings_helper,
                  const StreamSurroundSrcHelper<T> &stream_surround_src,
                  const fastuidraw::reference_counted_ptr<const T> &shader,
                  int dependency_depth);

    static
    std::string
    stream_dependency(ShaderSource &dst, const std::string &prefix, int idx,
                      get_src_type get_src, get_main_name_type get_main_name,
                      const StreamVaryingsHelper<T> &stream_varyings_helper,
                      const StreamSurroundSrcHelper<T> &stream_surround_src,
                      const fastuidraw::reference_counted_ptr<const T> &shader,
                      fastuidraw::c_string dep_name, int dependency_depth);
  };

}

template<typename T>
void
UberShaderStreamer<T>::
stream_source(ShaderSource &dst, const std::string &in_prefix,
              const ShaderSource &shader)
{
  /* This terribly hack is because GLES specfication mandates
   * for GLSL in GLES to not support token pasting (##) in the
   * pre-processor. Many GLES drivers support it anyways, but
   * Mesa does not. Sighs. So we emulate the token pasting
   * for the FASTUIDRAW_LOCAL() macro.
   */
  using namespace fastuidraw;
  std::string src, needle;
  std::string::size_type pos, last_pos;
  std::string prefix(in_prefix + "_local_");

  needle = "FASTUIDRAW_LOCAL";
  src = shader.assembled_code(true);

  /* HUNT for FASTUIDRAW_LOCAL(X) anywhere in the code and expand
   * it. NOTE: this is NOT robust at all as it is not a real
   * pre-processor, just a hack. It will fail if the macro
   * invocation is spread across multiple lines or if the agument
   * was a macro itself that needs to expanded by the pre-processor.
   */
  for (last_pos = 0, pos = src.find(needle); pos != std::string::npos; last_pos = pos + 1, pos = src.find(needle, pos + 1))
    {
      std::string::size_type open_pos, close_pos;

      /* stream up to pos */
      if (pos > last_pos)
        {
          dst << replace_double_colon_with_double_D(src.substr(last_pos, pos - last_pos));
        }

      /* find the first open and close-parentesis pair after pos. */
      open_pos = src.find('(', pos);
      close_pos = src.find(')', pos);

      if (open_pos != std::string::npos
          && close_pos != std::string::npos
          && open_pos < close_pos)
        {
          std::string tmp;
          tmp = src.substr(open_pos + 1, close_pos - open_pos - 1);
          // trim the string of white spaces.
          tmp.erase(0, tmp.find_first_not_of(" \t"));
          tmp.erase(tmp.find_last_not_of(" \t") + 1);
          dst << prefix << tmp;
          pos = close_pos;
        }
    }

  dst << replace_double_colon_with_double_D(src.substr(last_pos))
      << "\n";
}

template<typename T>
void
UberShaderStreamer<T>::
stream_shader(ShaderSource &dst, const std::string &prefix,
              const std::string &dep_name,
              get_src_type get_src, get_main_name_type get_main_name,
              const StreamVaryingsHelper<T> &stream_varyings_helper,
              const StreamSurroundSrcHelper<T> &stream_surround_src,
              const fastuidraw::reference_counted_ptr<const T> &sh,
              int dependency_depth)
{
  using namespace fastuidraw;

  c_array<const reference_counted_ptr<const T> > deps(sh->dependency_list_shaders());
  c_array<const c_string> dep_names(sh->dependency_list_names());

  FASTUIDRAWassert(deps.size() == dep_names.size());
  dst << "// Have " << deps.size() << " dependencies at depth " << dependency_depth << "\n";
  for (unsigned int i = 0; i < deps.size(); ++i)
    {
      std::string realized_name;
      realized_name = stream_dependency(dst, prefix, i, get_src, get_main_name,
                                        stream_varyings_helper, stream_surround_src,
                                        deps[i], dep_names[i], dependency_depth + 1);
      dst.add_macro(dep_names[i], realized_name.c_str());
    }

  dst.add_macro(get_main_name(sh), prefix.c_str());
  stream_varyings_helper.pre_source(dst, dependency_depth, dep_name.c_str(), sh);
  stream_surround_src.pre_source(dst, sh);
  stream_source(dst, prefix, (sh.get()->*get_src)());
  stream_surround_src.post_source(dst);
  stream_varyings_helper.post_source(dst, dependency_depth, dep_name.c_str(), sh);
  dst.remove_macro(get_main_name(sh));

  for (unsigned int i = 0; i < deps.size(); ++i)
    {
      dst.remove_macro(dep_names[i]);
    }
}

template<typename T>
std::string
UberShaderStreamer<T>::
stream_dependency(ShaderSource &dst, const std::string &in_prefix, int idx,
                  get_src_type get_src, get_main_name_type get_main_name,
                  const StreamVaryingsHelper<T> &stream_varyings_helper,
                  const StreamSurroundSrcHelper<T> &stream_surround_src,
                  const fastuidraw::reference_counted_ptr<const T> &shader,
                  fastuidraw::c_string dep_name, int dependency_depth)
{
  std::ostringstream str;
  std::string nm;

  str << in_prefix << "_dep" << idx;
  nm = str.str();
  dst << "// stream-dependency #" << idx << "{depth = "
      << dependency_depth << "}: " << in_prefix
      << ", name_in_parent = " << dep_name << "\n";

  stream_varyings_helper.before_dependency(dst, dep_name, shader);
  stream_shader(dst, nm, dep_name, get_src, get_main_name,
                stream_varyings_helper, stream_surround_src,
                shader, dependency_depth);
  stream_varyings_helper.after_dependency(dst, dep_name, shader);

  return nm;
}

template<typename T>
void
UberShaderStreamer<T>::
stream_uber(bool use_switch, ShaderSource &dst, array_type shaders,
            get_src_type get_src, get_main_name_type get_main_name,
            const StreamVaryingsHelper<T> &stream_varyings_helper,
            const StreamSurroundSrcHelper<T> &stream_surround_src,
            const std::string &return_type,
            const std::string &uber_func_with_args,
            const std::string &shader_args, //of the form ", arg1, arg2,..,argN" or empty string
            const std::string &shader_id)
{
  using namespace fastuidraw;

  /* first stream all of the shaders with predefined macros. */
  for(const auto &sh : shaders)
    {
      std::ostringstream str;

      dst << "\n/////////////////////////////////////////\n"
          << "// Start Shader #" << sh->ID() << " with "
          << sh->number_sub_shaders() << " sub-shaers\n";

      str << get_main_name(sh) << sh->ID();
      stream_varyings_helper.before_shader(dst, sh);
      stream_shader(dst, str.str(), "",
                    get_src, get_main_name,
                    stream_varyings_helper,
                    stream_surround_src,
                    sh, 0);
      stream_varyings_helper.after_shader(dst, sh);
    }

  bool has_sub_shaders(false), has_return_value(return_type != "void");
  std::string tab;
  bool first_entry(true);

  dst << return_type << "\n"
      << uber_func_with_args << "\n"
      << "{\n";

  if (has_return_value)
    {
      dst << "    " << return_type << " p;\n";
    }

  for(const auto &sh : shaders)
    {
      if (sh->number_sub_shaders() > 1)
        {
          unsigned int start, end;
          start = sh->ID();
          end = start + sh->number_sub_shaders();
          if (has_sub_shaders)
            {
              dst << "    else ";
            }
          else
            {
              dst << "    ";
            }

          dst << "if (" << shader_id << " >= uint(" << start
              << ") && " << shader_id << " < uint(" << end << "))\n"
              << "    {\n"
              << "        ";
          if (has_return_value)
            {
              dst << "p = ";
            }
          dst << get_main_name(sh) << sh->ID()
              << "(" << shader_id << " - uint(" << start << ")" << shader_args << ");\n"
              << "    }\n";
          has_sub_shaders = true;
          first_entry = false;
        }
    }

  if (has_sub_shaders && use_switch)
    {
      dst << "    else\n"
          << "    {\n";
      tab = "        ";
    }
  else
    {
      tab = "    ";
    }

  if (use_switch)
    {
      dst << tab << "switch(" << shader_id << ")\n"
          << tab << "{\n";
    }

  for(const auto &sh : shaders)
    {
      if (sh->number_sub_shaders() == 1)
        {
          if (use_switch)
            {
              dst << tab << "case uint(" << sh->ID() << "):\n"
                  << tab << "    {\n"
                  << tab << "        ";
            }
          else
            {
              if (!first_entry)
                {
                  dst << tab << "else if";
                }
              else
                {
                  dst << tab << "if";
                }
              dst << "(" << shader_id << " == uint(" << sh->ID() << "))\n"
                  << tab << "{\n"
                  << tab << "    ";
            }

          if (has_return_value)
            {
              dst << "p = ";
            }

          dst << get_main_name(sh) << sh->ID()
              << "(uint(0)" << shader_args << ");\n";

          if (use_switch)
            {
              dst << tab << "    }\n"
                  << tab << "    break;\n\n";
            }
          else
            {
              dst << tab << "}\n";
            }
          first_entry = false;
        }
    }

  if (use_switch)
    {
      dst << tab << "}\n";
    }

  if (has_sub_shaders && use_switch)
    {
      dst << "    }\n";
    }

  if (has_return_value)
    {
      dst << "    return p;\n";
    }

  dst << "}\n";
}


namespace fastuidraw { namespace glsl { namespace detail {

//////////////////////////
// UberShaderVaryings methods
void
UberShaderVaryings::
add_varyings(c_string label,
             c_array<const size_t> counts,
             AliasVaryingLocation *datum)
{
  struct varying_type_information info[varying_list::interpolator_number_types] =
    {
      [varying_list::interpolator_smooth] =
      {
        .m_qualifier = "",
        .m_types =
        {
          "float",
          "vec2",
          "vec3",
          "vec4",
        },
        .m_fastuidraw_prefix = "fastuidraw_float_smooth_varying",
        .m_is_flat = false,
      },

      [varying_list::interpolator_noperspective] =
      {
        .m_qualifier = "noperspective",
        .m_types =
        {
          "float",
          "vec2",
          "vec3",
          "vec4",
        },
        .m_fastuidraw_prefix = "fastuidraw_float_flat_varying",
        .m_is_flat = false,
      },

      [varying_list::interpolator_flat] =
      {
        .m_qualifier = "flat",
        .m_types =
        {
          "float",
          "vec2",
          "vec3",
          "vec4",
        },
        .m_fastuidraw_prefix = "fastuidraw_float_flat_varying",
        .m_is_flat = true,
      },

      [varying_list::interpolator_uint] =
      {
        .m_qualifier = "flat",
        .m_types =
        {
          "uint",
          "uvec2",
          "uvec3",
          "uvec4",
        },
        .m_fastuidraw_prefix = "fastuidraw_uint_varying",
        .m_is_flat = true,
      },

      [varying_list::interpolator_int] =
      {
        .m_qualifier = "flat",
        .m_types =
        {
          "int",
          "ivec2",
          "ivec3",
          "ivec4",
        },
        .m_fastuidraw_prefix = "fastuidraw_int_varying",
        .m_is_flat = true,
      },
    };

  for (unsigned int i = 0; i < varying_list::interpolator_number_types; ++i)
    {
      datum->m_varying_start[i] = add_varyings_impl_type(m_varyings[i], counts[i],
                                                         info[i].m_qualifier, info[i].m_types,
                                                         info[i].m_fastuidraw_prefix, info[i].m_is_flat);
    }

  datum->m_label = label;
}

uvec2
UberShaderVaryings::
add_varyings_impl_type(std::vector<per_varying> &varyings,
                       unsigned int cnt,
                       c_string qualifier,
                       vecN<c_string, 4> types,
                       c_string name, bool is_flat)
{
  uvec2 return_value;

  /* first add to the back of varying up to 4-components */
  if (!varyings.empty() && varyings.back().m_num_components < 4)
    {
      unsigned int add_to_back, space_left;

      return_value.x() = varyings.size() - 1;
      return_value.y() = varyings.back().m_num_components;

      space_left = 4u - varyings.back().m_num_components;
      add_to_back = t_min(space_left, cnt);

      varyings.back().m_num_components += add_to_back;
      varyings.back().m_type = types[varyings.back().m_num_components - 1];

      cnt -= add_to_back;
    }
  else
    {
      return_value.x() = varyings.size();
      return_value.y() = 0u;
    }

  if (cnt == 0)
    {
      return return_value;
    }

  per_varying V;
  unsigned int num_vec4(cnt / 4);
  unsigned int remaining(cnt % 4);

  V.m_is_flat = is_flat;
  V.m_qualifier = qualifier;
  for (unsigned int i = 0; i < num_vec4; ++i)
    {
      V.m_name = make_name(name, varyings.size());
      V.m_num_components = 4;
      V.m_type = types[V.m_num_components - 1];
      varyings.push_back(V);
    }

  if (remaining > 0)
    {
      V.m_name = make_name(name, varyings.size());
      V.m_num_components = remaining;
      V.m_type = types[V.m_num_components - 1];
      varyings.push_back(V);
    }

  return return_value;
}

void
UberShaderVaryings::
declare_varyings(std::ostringstream &str,
                 c_string varying_qualifier,
                 c_string interface_name,
                 c_string instance_name) const
{
  unsigned int cnt(0);
  if (interface_name)
    {
      str << varying_qualifier << " " << interface_name << "\n{\n";
      varying_qualifier = "";
    }

  for (unsigned int i = 0; i < varying_list::interpolator_number_types; ++i)
    {
      declare_varyings_impl(str, m_varyings[i], varying_qualifier, cnt);
    }

  if (interface_name)
    {
      str << "}";
      if (instance_name)
        {
          str << " " << instance_name;
        }
      str << ";\n";
    }
}

void
UberShaderVaryings::
stream_varying_rw_copies(ShaderSource &dst) const
{
  for (const std::vector<per_varying> &varyings : m_varyings)
    {
      for (const per_varying &V : varyings)
        {
          dst << V.m_type << " " << V.m_name << "_rw_copy;\n";
        }
    }

  dst << "void fastuidraw_mirror_varyings(void)\n"
      << "{\n";
  for (const std::vector<per_varying> &varyings : m_varyings)
    {
      for (const per_varying &V : varyings)
        {
          dst << V.m_name << "_rw_copy = " << V.m_name << ";\n";
        }
    }
  dst << "}\n";
}

void
UberShaderVaryings::
declare_varyings_impl(std::ostringstream &str,
                      const std::vector<per_varying> &varyings,
                      c_string varying_qualifier,
                      unsigned int &slot) const
{
  for (const per_varying &V : varyings)
    {
      str << "FASTUIDRAW_LAYOUT_VARYING(" << slot << ") "
          << V.m_qualifier << " " << varying_qualifier << " " << V.m_type
          << " " << V.m_name << ";\n";
      ++slot;
    }
}

void
UberShaderVaryings::
stream_alias_varyings_impl(bool use_rw_copies,
                           const std::vector<per_varying> &varyings_to_use,
                           ShaderSource &shader,
                           c_array<const c_string> p,
                           bool add_aliases, uvec2 start,
                           filter_varying_t filter_varying) const
{
  if (add_aliases)
    {
      fastuidraw::c_string ext = "xyzw";
      for (unsigned int i = 0; i < p.size(); ++i, ++start.y())
        {
          if (start.y() == 4)
            {
              ++start.x();
              start.y() = 0;
            }

          FASTUIDRAWassert(start.x() < varyings_to_use.size());
          FASTUIDRAWassert(start.y() < 4u);

          const per_varying &V(varyings_to_use[start.x()]);
          std::ostringstream str;

          str << V.m_name;
          if (use_rw_copies)
            {
              str << "_rw_copy";
            }

          if (V.m_num_components != 1)
            {
              str << "." << ext[start.y()];
            }

          if (filter_varying(p[i]))
            {
              std::string pp(replace_double_colon_with_double_D(p[i]));
              shader.add_macro(pp.c_str(), str.str().c_str());
            }
        }
    }
  else
    {
      for (unsigned int i = 0; i < p.size(); ++i)
        {
          if (filter_varying(p[i]))
            {
              std::string pp(replace_double_colon_with_double_D(p[i]));
              shader.remove_macro(pp.c_str());
            }
        }
    }
}

void
UberShaderVaryings::
stream_alias_varyings(bool use_rw_copies,
                      ShaderSource &shader, const varying_list &p,
                      bool add_aliases,
                      const AliasVaryingLocation &datum,
                      filter_varying_t filter_varying) const
{
  if (add_aliases)
    {
      shader << "//////////////////////////////////////////////////\n"
             << "// Stream varying aliases for: " << datum.m_label
             << " @" << datum.m_varying_start
             << "\n";
    }
  else
    {
      shader << "// Remove varying aliases\n";
    }

  for (unsigned int i = 0; i < varying_list::interpolator_number_types; ++i)
    {
      enum varying_list::interpolator_type_t q;
      q = static_cast<enum varying_list::interpolator_type_t>(i);

      stream_alias_varyings_impl(use_rw_copies, m_varyings[i], shader, p.varyings(q),
                                 add_aliases, datum.m_varying_start[i], filter_varying);
    }

  if (add_aliases)
    {
      c_array<const c_string> names(p.alias_varying_names());
      c_array<const c_string> src_names(p.alias_varying_source_names());

      FASTUIDRAWassert(names.size() == src_names.size());
      for (unsigned int i = 0; i < names.size(); ++i)
        {
          if (filter_varying(names[i]))
            {
              std::string name(replace_double_colon_with_double_D(names[i]));
              std::string src_name(replace_double_colon_with_double_D(src_names[i]));
              shader.add_macro(name.c_str(), src_name.c_str());
            }
        }
    }
  else
    {
      c_array<const c_string> names(p.alias_varying_names());
      for (unsigned int i = 0; i < names.size(); ++i)
        {
          if (filter_varying(names[i]))
            {
              std::string name(replace_double_colon_with_double_D(names[i]));
              shader.remove_macro(name.c_str());
            }
        }
    }
}

/////////////////////
// non-class methods
void
stream_uber_vert_shader(bool use_switch,
                        ShaderSource &vert,
                        c_array<const reference_counted_ptr<PainterItemShaderGLSL> > item_shaders,
                        const UberShaderVaryings &declare_varyings,
                        const AliasVaryingLocation &datum)
{
  UberShaderStreamer<PainterItemShaderGLSL>::stream_uber(use_switch, vert, item_shaders,
                                                         &PainterItemShaderGLSL::vertex_src,
                                                         item_shader_vert_name<PainterItemShaderGLSL>,
                                                         StreamVaryingsHelper<PainterItemShaderGLSL>(false, declare_varyings, datum),
                                                         StreamSurroundSrcHelper<PainterItemShaderGLSL>(),
                                                         "void",
                                                         "fastuidraw_run_vert_shader(in fastuidraw_header h, out int add_z, out vec2 brush_p, out vec3 clip_p)",
                                                         ", fastuidraw_attribute0, fastuidraw_attribute1, "
                                                         "fastuidraw_attribute2, h.item_shader_data_location, add_z, brush_p, clip_p",
                                                         "h.item_shader");
}

void
stream_uber_frag_shader(bool use_switch,
                        ShaderSource &frag,
                        c_array<const reference_counted_ptr<PainterItemShaderGLSL> > item_shaders,
                        const UberShaderVaryings &declare_varyings,
                        const AliasVaryingLocation &datum)
{
  UberShaderStreamer<PainterItemShaderGLSL>::stream_uber(use_switch, frag, item_shaders,
                                                         &PainterItemShaderGLSL::fragment_src,
                                                         item_shader_frag_name<PainterItemShaderGLSL>,
                                                         StreamVaryingsHelper<PainterItemShaderGLSL>(true, declare_varyings, datum),
                                                         StreamSurroundSrcHelper<PainterItemShaderGLSL>(),
                                                         "vec4",
                                                         "fastuidraw_run_frag_shader(in uint frag_shader, in uint frag_shader_data_location)",
                                                         ", frag_shader_data_location",
                                                         "frag_shader");
}

void
stream_uber_vert_shader(bool use_switch,
                        ShaderSource &vert,
                        c_array<const reference_counted_ptr<PainterItemCoverageShaderGLSL> > item_shaders,
                        const UberShaderVaryings &declare_varyings,
                        const AliasVaryingLocation &datum)
{
  UberShaderStreamer<PainterItemCoverageShaderGLSL>::stream_uber(use_switch, vert, item_shaders,
                                                                 &PainterItemCoverageShaderGLSL::vertex_src,
                                                                 item_shader_vert_name<PainterItemCoverageShaderGLSL>,
                                                                 StreamVaryingsHelper<PainterItemCoverageShaderGLSL>(false, declare_varyings, datum),
                                                                 StreamSurroundSrcHelper<PainterItemCoverageShaderGLSL>(),
                                                                 "void",
                                                                 "fastuidraw_run_vert_shader(in fastuidraw_header h, out vec3 clip_p)",
                                                                 ", fastuidraw_attribute0, fastuidraw_attribute1, "
                                                                 "fastuidraw_attribute2, h.item_shader_data_location, clip_p",
                                                                 "h.item_shader");
}

void
stream_uber_frag_shader(bool use_switch,
                        ShaderSource &frag,
                        c_array<const reference_counted_ptr<PainterItemCoverageShaderGLSL> > item_shaders,
                        const UberShaderVaryings &declare_varyings,
                        const AliasVaryingLocation &datum)
{
  UberShaderStreamer<PainterItemCoverageShaderGLSL>::stream_uber(use_switch, frag, item_shaders,
                                                                 &PainterItemCoverageShaderGLSL::fragment_src,
                                                                 item_shader_frag_name<PainterItemCoverageShaderGLSL>,
                                                                 StreamVaryingsHelper<PainterItemCoverageShaderGLSL>(true, declare_varyings, datum),
                                                                 StreamSurroundSrcHelper<PainterItemCoverageShaderGLSL>(),
                                                                 "float",
                                                                 "fastuidraw_run_frag_shader(in uint frag_shader, in uint frag_shader_data_location)",
                                                                 ", frag_shader_data_location",
                                                                 "frag_shader");
}

void
stream_uber_blend_shader(bool use_switch,
                         ShaderSource &frag,
                         c_array<const reference_counted_ptr<PainterBlendShaderGLSL> > shaders,
                         enum PainterBlendShader::shader_type tp)
{
  std::string sub_func_args, func_name;

  switch(tp)
    {
    default:
      FASTUIDRAWassert("Unknown blend_code_type!");
      //fall through
    case PainterBlendShader::single_src:
      func_name = "fastuidraw_run_blend_shader(in uint blend_shader, in uint blend_shader_data_location, in vec4 in_src, out vec4 out_src)";
      sub_func_args = ", blend_shader_data_location, in_src, out_src";
      add_macro_requirement(frag, true, "FASTUIDRAW_PAINTER_BLEND_SINGLE_SRC_BLEND", "Mismatch macros determining blend shader type!");
      add_macro_requirement(frag, false, "FASTUIDRAW_PAINTER_BLEND_DUAL_SRC_BLEND", "Mismatch macros determining blend shader type!");
      add_macro_requirement(frag, false, "FASTUIDRAW_PAINTER_BLEND_FRAMEBUFFER_FETCH", "Mismatch macros determining blend shader type!");
      add_macro_requirement(frag, false, "FASTUIDRAW_PAINTER_BLEND_INTERLOCK", "Mismatch macros determining blend shader type!");
      break;

    case PainterBlendShader::dual_src:
      func_name = "fastuidraw_run_blend_shader(in uint blend_shader, in uint blend_shader_data_location, in vec4 color0, out vec4 src0, out vec4 src1)";
      sub_func_args = ", blend_shader_data_location, color0, src0, src1";
      add_macro_requirement(frag, false, "FASTUIDRAW_PAINTER_BLEND_SINGLE_SRC_BLEND", "Mismatch macros determining blend shader type");
      add_macro_requirement(frag, true, "FASTUIDRAW_PAINTER_BLEND_DUAL_SRC_BLEND", "Mismatch macros determining blend shader type");
      add_macro_requirement(frag, false, "FASTUIDRAW_PAINTER_BLEND_FRAMEBUFFER_FETCH", "Mismatch macros determining blend shader type");
      add_macro_requirement(frag, false, "FASTUIDRAW_PAINTER_BLEND_INTERLOCK", "Mismatch macros determining blend shader type!");
      break;

    case PainterBlendShader::framebuffer_fetch:
      func_name = "fastuidraw_run_blend_shader(in uint blend_shader, in uint blend_shader_data_location, in vec4 in_src, in vec4 in_fb, out vec4 out_src)";
      sub_func_args = ", blend_shader_data_location, in_src, in_fb, out_src";
      add_macro_requirement(frag, false, "FASTUIDRAW_PAINTER_BLEND_SINGLE_SRC_BLEND", "Mismatch macros determining blend shader type");
      add_macro_requirement(frag, false, "FASTUIDRAW_PAINTER_BLEND_DUAL_SRC_BLEND", "Mismatch macros determining blend shader type");
      add_macro_requirement(frag,
                            "FASTUIDRAW_PAINTER_BLEND_FRAMEBUFFER_FETCH",
                            "FASTUIDRAW_PAINTER_BLEND_INTERLOCK",
                            "Mismatch macros determining blend shader type");
      break;
    }
  UberShaderStreamer<PainterBlendShaderGLSL>::stream_uber(use_switch, frag, shaders,
                                                          &PainterBlendShaderGLSL::blend_src,
                                                          blend_shader_name,
                                                          StreamVaryingsHelper<PainterBlendShaderGLSL>(),
                                                          StreamSurroundSrcHelper<PainterBlendShaderGLSL>(),
                                                          "void", func_name,
                                                          sub_func_args, "blend_shader");
}

void
stream_uber_brush_vert_shader(bool use_switch,
                              ShaderSource &vert,
                              c_array<const reference_counted_ptr<PainterBrushShaderGLSL> > brush_shaders,
                              const UberShaderVaryings &declare_varyings,
                              const AliasVaryingLocation &datum)
{
  UberShaderStreamer<PainterBrushShaderGLSL>::stream_uber(use_switch, vert, brush_shaders,
                                                          &PainterBrushShaderGLSL::vertex_src,
                                                          item_shader_vert_name<PainterBrushShaderGLSL>,
                                                          StreamVaryingsHelper<PainterBrushShaderGLSL>(false, declare_varyings, datum),
                                                          StreamSurroundSrcHelper<PainterBrushShaderGLSL>(),
                                                          "void",
                                                          "fastuidraw_run_brush_vert_shader(in fastuidraw_header h, in vec2 brush_p)",
                                                          ", h.brush_shader_data_location, brush_p",
                                                          "h.brush_shader");
}

void
stream_uber_brush_frag_shader(bool use_switch,
                              ShaderSource &frag,
                              c_array<const reference_counted_ptr<PainterBrushShaderGLSL> > brush_shaders,
                              const UberShaderVaryings &declare_varyings,
                              const AliasVaryingLocation &datum)
{
  UberShaderStreamer<PainterBrushShaderGLSL>::stream_uber(use_switch, frag, brush_shaders,
                                                          &PainterBrushShaderGLSL::fragment_src,
                                                          item_shader_frag_name<PainterBrushShaderGLSL>,
                                                          StreamVaryingsHelper<PainterBrushShaderGLSL>(true, declare_varyings, datum),
                                                          StreamSurroundSrcHelper<PainterBrushShaderGLSL>(),
                                                          "vec4",
                                                          "fastuidraw_run_brush_frag_shader(in uint frag_shader, in uint frag_shader_data_location)",
                                                          ", frag_shader_data_location",
                                                          "frag_shader");
}

}}}
