/*!
 * \file gl_program.cpp
 * \brief file gl_program.cpp
 *
 * Adapted from: WRATHGLProgram.cpp of WRATH:
 *
 * Copyright 2013 by Nomovok Ltd.
 * Contact: info@nomovok.com
 * This Source Code Form is subject to the
 * terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with
 * this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 *
 * \author Kevin Rogovin <kevin.rogovin@nomovok.com>
 * \author Kevin Rogovin <kevin.rogovin@intel.com>
 */


#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <set>
#include <string>
#include <map>
#include <vector>
#include <list>
#include <algorithm>
#include <sstream>
#include <stdint.h>
#include <boost/utility.hpp>


#include <fastuidraw/util/static_resource.hpp>
#include <fastuidraw/gl_backend/gl_program.hpp>

namespace
{
  class SourcePrivate
  {
  public:
    SourcePrivate();

    typedef enum fastuidraw::gl::Shader::shader_source_type source_type;
    typedef enum fastuidraw::gl::Shader::shader_extension_enable_type extension_enable_type;
    typedef std::pair<std::string, source_type> source_code_type;

    bool m_dirty;
    std::list<source_code_type> m_values;
    std::map<std::string, extension_enable_type> m_extensions;
    std::string m_version;

    std::string m_assembled_code;

    static
    std::string
    strip_leading_white_spaces(const std::string &S);

    static
    void
    emit_source_line(std::ostream &output_stream,
                     const std::string &source,
                     int line_number, const std::string &label);

    static
    void
    add_source_code_from_stream(const std::string &label,
                                std::istream &istr,
                                std::ostream &output_stream);

    static
    void
    add_source_entry(const source_code_type &v, std::ostream &output_stream);

    static
    const char*
    string_from_extension_type(extension_enable_type tp);
  };

  class ShaderPrivate
  {
  public:
    ShaderPrivate(const fastuidraw::gl::Shader::shader_source &src,
                  GLenum pshader_type);

    void
    compile(void);

    bool m_shader_ready;
    GLuint m_name;
    GLenum m_shader_type;

    std::string m_source_code;
    std::string m_compile_log;
    bool m_compile_success;
  };

  class BindAttributePrivate
  {
  public:
    BindAttributePrivate(const char *pname, int plocation):
      m_label(pname),
      m_location(plocation)
    {}

    std::string m_label;
    int m_location;
  };

  class PreLinkActionArrayPrivate
  {
  public:
    std::vector<fastuidraw::gl::PreLinkAction::handle> m_values;
  };

  class UniformBlockInitializerPrivate
  {
  public:
    UniformBlockInitializerPrivate(const char *n, int v):
      m_block_name(n),
      m_binding_point(v)
    {}

    std::string m_block_name;
    int m_binding_point;
  };

  class ProgramInitializerArrayPrivate
  {
  public:
    std::vector<fastuidraw::gl::ProgramInitializer::handle> m_values;
  };

  class ParameterInfoPrivate
  {
  public:
    ParameterInfoPrivate(void):
      m_name(""),
      m_type(GL_INVALID_ENUM),
      m_count(0),
      m_index(-1),
      m_location(-1)
    {}

    bool
    operator<(const ParameterInfoPrivate &rhs) const
    {
      return m_name < rhs.m_name;
    }

    std::string m_name;
    GLenum m_type;
    GLint m_count;
    GLuint m_index;
    GLint m_location;
  };

  typedef std::pair<int, const ParameterInfoPrivate*> FindParameterResult;

  class ParameterInfoPrivateHoard
  {
  public:

    FindParameterResult
    find_parameter(const std::string &pname);

    template<typename F, typename G>
    void
    fill_hoard(GLuint program,
               GLenum count_enum, GLenum length_enum,
               F fptr, G gptr);

    template<typename iterator>
    static
    std::string
    filter_name(iterator begin, iterator end, int &array_index);

    std::vector<ParameterInfoPrivate> m_values;
    std::map<std::string, int> m_map;
  };

  class ShaderData
  {
  public:
    std::string m_source_code;
    GLuint m_name;
    GLenum m_shader_type;
    std::string m_compile_log;
  };

  class ProgramPrivate
  {
  public:
    ProgramPrivate(const fastuidraw::const_c_array<fastuidraw::gl::Shader::handle> pshaders,
                   const fastuidraw::gl::PreLinkActionArray &action,
                   const fastuidraw::gl::ProgramInitializerArray &initers,
                   fastuidraw::gl::Program *p):
      m_shaders(pshaders.begin(), pshaders.end()),
      m_name(0),
      m_assembled(false),
      m_initializers(initers),
      m_pre_link_actions(action),
      m_p(p)
    {
    }

    ProgramPrivate(fastuidraw::gl::Shader::handle vert_shader,
                   fastuidraw::gl::Shader::handle frag_shader,
                   const fastuidraw::gl::PreLinkActionArray &action,
                   const fastuidraw::gl::ProgramInitializerArray &initers,
                   fastuidraw::gl::Program *p):
      m_name(0),
      m_assembled(false),
      m_initializers(initers),
      m_pre_link_actions(action),
      m_p(p)
    {
      m_shaders.push_back(vert_shader);
      m_shaders.push_back(frag_shader);
    }

    ProgramPrivate(const fastuidraw::gl::Shader::shader_source &vert_shader,
                   const fastuidraw::gl::Shader::shader_source &frag_shader,
                   const fastuidraw::gl::PreLinkActionArray &action,
                   const fastuidraw::gl::ProgramInitializerArray &initers,
                   fastuidraw::gl::Program *p):
      m_name(0),
      m_assembled(false),
      m_initializers(initers),
      m_pre_link_actions(action),
      m_p(p)
    {
      m_shaders.push_back(FASTUIDRAWnew fastuidraw::gl::Shader(vert_shader, GL_VERTEX_SHADER));
      m_shaders.push_back(FASTUIDRAWnew fastuidraw::gl::Shader(frag_shader, GL_FRAGMENT_SHADER));
    }

    void
    assemble(void);

    void
    clear_shaders_and_save_shader_data(void);

    void
    generate_log(void);

    std::vector<fastuidraw::gl::Shader::handle> m_shaders;
    std::vector<ShaderData> m_shader_data;

    GLuint m_name;
    bool m_link_success, m_assembled;
    std::string m_link_log;
    std::string m_log;

    std::set<std::string> m_binded_attributes;
    ParameterInfoPrivateHoard m_uniform_list;
    ParameterInfoPrivateHoard m_attribute_list;
    fastuidraw::gl::ProgramInitializerArray m_initializers;
    fastuidraw::gl::PreLinkActionArray m_pre_link_actions;
    fastuidraw::gl::Program *m_p;
  };
}

//////////////////////////////////////////////////
// SourcePrivate methods
SourcePrivate::
SourcePrivate(void):
  m_dirty(true),
#ifdef FASTUIDRAW_GL_USE_GLES
  m_version("300 es")
#else
  m_version("330")
#endif
{
}


const char*
SourcePrivate::
string_from_extension_type(enum fastuidraw::gl::Shader::shader_extension_enable_type tp)
{
  switch(tp)
    {
    case fastuidraw::gl::Shader::enable_extension:
      return "enable";
      break;

    case fastuidraw::gl::Shader::require_extension:
      return "require";
      break;

    case fastuidraw::gl::Shader::warn_extension:
      return "warn";
      break;

    case fastuidraw::gl::Shader::disable_extension:
      return "disable";
      break;

    default:
      assert(!"Unknown value for gl::Shader::shader_extension_enable_type");
      return "";
    }

}

std::string
SourcePrivate::
strip_leading_white_spaces(const std::string &S)
{
  std::string::const_iterator iter, end;
  for(iter = S.begin(), end = S.end(); iter != end and isspace(*iter); ++iter)
    {
    }
  return (iter != end and *iter == '#')?
    std::string(iter, end):
    S;
}

void
SourcePrivate::
emit_source_line(std::ostream &output_stream,
                 const std::string &source,
                 int line_number, const std::string &label)
{
  std::string S;
  S = strip_leading_white_spaces(source);
  output_stream << S;

  #ifndef NDEBUG
    {
      if(S.empty() || *S.rbegin() != '\\')
        {
          output_stream << std::setw(80-S.length()) << "  //LOCATION("
                        << std::setw(3) << line_number
                        << ", " << label
                        << ")";
        }
    }
  #endif

  output_stream << "\n";
}


void
SourcePrivate::
add_source_code_from_stream(const std::string &label,
                            std::istream &istr,
                            std::ostream &output_stream)
{
  std::string S;
  int line_number(1);

  while(getline(istr, S))
    {
      emit_source_line(output_stream, S, line_number, label);
      ++line_number;
      S.clear();
    }
}

void
SourcePrivate::
add_source_entry(const source_code_type &v, std::ostream &output_stream)
{
  if(v.second == fastuidraw::gl::Shader::from_file)
    {
      std::ifstream file(v.first.c_str());

      if(file)
        {
          add_source_code_from_stream(v.first, file, output_stream);
        }
      else
        {
          output_stream << "\n//WARNING: Could not open file \""
                        << v.first << "\"\n";
        }
    }
  else
    {
      std::istringstream istr;
      std::string label;

      if(v.second == fastuidraw::gl::Shader::from_string)
        {
          istr.str(v.first);
          label="raw string";
        }
      else
        {
          fastuidraw::const_c_array<uint8_t> resource_string;

          resource_string = fastuidraw::fetch_static_resource(v.first.c_str());
          label=v.first;
          if(!resource_string.empty() && resource_string.back() == 0)
            {
              const char *s;
              s = reinterpret_cast<const char*>(resource_string.c_ptr());
              istr.str(std::string(s));
            }
          else
            {
              output_stream << "\n//Unable to fetch string resource \"" << v.first
                            << "\"\n";
              return;
            }
        }
      add_source_code_from_stream(label, istr, output_stream);
    }
}


/////////////////////////////////////////
// ShaderPrivate methods
ShaderPrivate::
ShaderPrivate(const fastuidraw::gl::Shader::shader_source &src,
              GLenum pshader_type):
  m_shader_ready(false),
  m_name(0),
  m_shader_type(pshader_type),
  m_compile_success(false)
{
  m_source_code = std::string(src.assembled_code());
}

void
ShaderPrivate::
compile(void)
{
  if(m_shader_ready)
    {
      return;
    }

  //now do the GL work, create a name and compile the source code:
  assert(m_name == 0);

  m_shader_ready = true;
  m_name = glCreateShader(m_shader_type);

  const char *sourceString[1];
  sourceString[0] = m_source_code.c_str();


  glShaderSource(m_name, //shader handle
                 1, //number strings
                 sourceString, //array of strings
                 NULL); //lengths of each string or NULL implies each is 0-terminated

  glCompileShader(m_name);

  GLint logSize(0), shaderOK;
  std::vector<char> raw_log;

  //get shader compile status and log length.
  glGetShaderiv(m_name, GL_COMPILE_STATUS, &shaderOK);
  glGetShaderiv(m_name, GL_INFO_LOG_LENGTH, &logSize);

  //retrieve the compile log string, eh gross.
  raw_log.resize(logSize+2,'\0');
  glGetShaderInfoLog(m_name, //shader handle
                     logSize+1, //maximum size of string
                     NULL, //GLint* return length of string
                     &raw_log[0]); //char* to write log to.

  m_compile_log = &raw_log[0];
  m_compile_success = (shaderOK == GL_TRUE);

  if(!m_compile_success)
    {
      std::ostringstream oo;

      oo << "bad_shader_" << m_name << ".glsl";

      std::ofstream eek(oo.str().c_str());
      eek << m_source_code
          << "\n\n"
          << m_compile_log;
    }

}

///////////////////////////////////
// ParameterInfoPrivateHoard methods
template<typename F, typename G>
void
ParameterInfoPrivateHoard::
fill_hoard(GLuint program,
           GLenum count_enum, GLenum length_enum,
           F fptr, G gptr)
{
  GLint count, largest_length;
  std::vector<char> pname;

  glGetProgramiv(program, count_enum, &count);

  if(count > 0)
    {
      m_values.resize(count);
      glGetProgramiv(program, length_enum, &largest_length);

      ++largest_length;
      pname.resize(largest_length, '\0');

      for(int i = 0; i != count; ++i)
        {
          GLsizei name_length, psize;
          GLenum ptype;
          int array_index;

          std::memset(&pname[0], 0, largest_length);

          fptr(program, i, largest_length,
               &name_length, &psize,
               &ptype, &pname[0]);

          m_values[i].m_type = ptype;
          m_values[i].m_count = psize;
          m_values[i].m_name = filter_name(pname.begin(),
                                           pname.begin() + name_length,
                                           array_index);
          if(array_index != 0)
            {
              /*
                crazy GL... it lists an element
                from an array as a unique location,
                chicken out and add it with the
                array index:
              */
              m_values[i].m_name = std::string(pname.begin(),
                                               pname.begin() + name_length);
            }

          m_values[i].m_index = i;
          m_values[i].m_location = gptr(program, &pname[0]);

        }

      /* sort m_values by name and then make our map
       */
      std::sort(m_values.begin(), m_values.end());
      for(unsigned int i = 0, endi = m_values.size(); i < endi; ++i)
        {
          m_map[m_values[i].m_name] = i;
        }
    }
}


std::pair<int, const ParameterInfoPrivate*>
ParameterInfoPrivateHoard::
find_parameter(const std::string &pname)
{
  std::map<std::string, int>::const_iterator iter;

  iter = m_map.find(pname);
  if(iter != m_map.end())
    {
      const ParameterInfoPrivate *q;
      q = &m_values[iter->second];
      return FindParameterResult(q->m_location, q);
    }

  std::string filtered_name;
  int array_index;
  filtered_name = filter_name(pname.begin(), pname.end(), array_index);
  iter = m_map.find(filtered_name);
  if(iter != m_map.end())
    {
      const ParameterInfoPrivate *q;
      q = &m_values[iter->second];
      if(array_index < q->m_count)
        {
          return FindParameterResult(q->m_location + array_index, q);
        }
    }

  return FindParameterResult(-1, NULL);
}


template<typename iterator>
std::string
ParameterInfoPrivateHoard::
filter_name(iterator begin, iterator end, int &array_index)
{
  std::string return_value;

  /*
    Firstly, strip out all white spaces:
  */
  return_value.reserve(std::distance(begin,end));
  for(iterator iter = begin; iter != end; ++iter)
    {
      if(!std::isspace(*iter))
        {
          return_value.append(1, *iter);
        }
    }

  /*
    now check if the last character is a ']'
    and if it is remove characters until
    a '[' is encountered.
  */
  if(!return_value.empty() and *return_value.rbegin() == ']')
    {
      std::string::size_type loc;

      loc = return_value.find_last_of('[');
      assert(loc != std::string::npos);

      std::istringstream array_value(return_value.substr(loc+1, return_value.size()-1));
      array_value >> array_index;

      //also resize return_value to remove
      //the [array_index]:
      return_value.resize(loc);
    }
  else
    {
      array_index = 0;
    }
  return return_value;
}

////////////////////////////////////
//fastuidraw::gl::Shader::shader_source methods
fastuidraw::gl::Shader::shader_source::
shader_source(void)
{
  m_d = FASTUIDRAWnew SourcePrivate();
}

fastuidraw::gl::Shader::shader_source::
shader_source(const shader_source &obj)
{
  SourcePrivate *d;
  d = reinterpret_cast<SourcePrivate*>(obj.m_d);
  m_d = FASTUIDRAWnew SourcePrivate(*d);
}

fastuidraw::gl::Shader::shader_source::
~shader_source()
{
  SourcePrivate *d;
  d = reinterpret_cast<SourcePrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = NULL;
}

fastuidraw::gl::Shader::shader_source&
fastuidraw::gl::Shader::shader_source::
operator=(const shader_source &obj)
{
  if(this != &obj)
    {
      SourcePrivate *d, *obj_d;
      d = reinterpret_cast<SourcePrivate*>(m_d);
      obj_d = reinterpret_cast<SourcePrivate*>(obj.m_d);
      *d = *obj_d;
    }
  return *this;
}

fastuidraw::gl::Shader::shader_source&
fastuidraw::gl::Shader::shader_source::
specify_version(const char *v)
{
  SourcePrivate *d;
  d = reinterpret_cast<SourcePrivate*>(m_d);
  d->m_version = v ? std::string(v) : "";
  d->m_dirty = true;
  return *this;
}

fastuidraw::gl::Shader::shader_source&
fastuidraw::gl::Shader::shader_source::
add_source(const char *str, enum shader_source_type tp,
           enum add_source_location_type loc)
{
  SourcePrivate *d;
  d = reinterpret_cast<SourcePrivate*>(m_d);

  assert(str);
  SourcePrivate::source_code_type v(str, tp);

  if(loc == push_front)
    {
      d->m_values.push_front(v);
    }
  else
    {
      d->m_values.push_back(v);
    }
  d->m_dirty = true;
  return *this;
}

fastuidraw::gl::Shader::shader_source&
fastuidraw::gl::Shader::shader_source::
add_source(const shader_source &obj)
{
  SourcePrivate *d, *obj_d;
  d = reinterpret_cast<SourcePrivate*>(m_d);
  obj_d = reinterpret_cast<SourcePrivate*>(obj.m_d);

  std::copy(obj_d->m_values.begin(), obj_d->m_values.end(),
            std::insert_iterator<std::list<SourcePrivate::source_code_type> >(d->m_values, d->m_values.end()));
  d->m_dirty = true;
  return *this;
}

fastuidraw::gl::Shader::shader_source&
fastuidraw::gl::Shader::shader_source::
add_macro(const char *macro_name, const char *macro_value,
          enum add_source_location_type loc)
{
  std::ostringstream ostr;
  ostr << "#define " << macro_name << " " << macro_value;
  return add_source(ostr.str().c_str(), from_string, loc);
}

fastuidraw::gl::Shader::shader_source&
fastuidraw::gl::Shader::shader_source::
add_macro(const char *macro_name, uint32_t macro_value,
          enum add_source_location_type loc)
{
  std::ostringstream ostr;
  ostr << "#define " << macro_name << " " << macro_value;
  return add_source(ostr.str().c_str(), from_string, loc);
}

fastuidraw::gl::Shader::shader_source&
fastuidraw::gl::Shader::shader_source::
add_macro(const char *macro_name, int32_t macro_value,
          enum add_source_location_type loc)
{
  std::ostringstream ostr;
  ostr << "#define " << macro_name << " " << macro_value;
  return add_source(ostr.str().c_str(), from_string, loc);
}

fastuidraw::gl::Shader::shader_source&
fastuidraw::gl::Shader::shader_source::
remove_macro(const char *macro_name)
{
  std::ostringstream ostr;
  ostr << "#undef " << macro_name;
  return add_source(ostr.str().c_str(), from_string);
}


fastuidraw::gl::Shader::shader_source&
fastuidraw::gl::Shader::shader_source::
specify_extension(const char *ext_name, enum shader_extension_enable_type tp)
{
  SourcePrivate *d;
  d = reinterpret_cast<SourcePrivate*>(m_d);
  d->m_extensions[std::string(ext_name)] = tp;
  d->m_dirty = true;
  return *this;
}

const char*
fastuidraw::gl::Shader::shader_source::
assembled_code(void) const
{
  SourcePrivate *d;
  d = reinterpret_cast<SourcePrivate*>(m_d);

  if(d->m_dirty)
    {
      std::ostringstream output_glsl_source_code;

      if(!d->m_version.empty())
        {
          output_glsl_source_code <<"\n#version " << d->m_version << "\n";
        }

      for(std::map<std::string, enum shader_extension_enable_type>::const_iterator
            iter = d->m_extensions.begin(), end = d->m_extensions.end(); iter != end; ++iter)
        {
          output_glsl_source_code << "#extension " << iter->first
                                  << ": " << SourcePrivate::string_from_extension_type(iter->second) << "\n";
        }

      output_glsl_source_code << "uint fastuidraw_mask(uint num_bits) { return (uint(1) << num_bits) - uint(1); }\n"
                              << "uint fastuidraw_extract_bits(uint bit0, uint num_bits, uint src) { return (src >> bit0) & fastuidraw_mask(num_bits); }\n"
                              << "#define FASTUIDRAW_MASK(num_bits) fastuidraw_mask(uint(num_bits))\n"
                              << "#define FASTUIDRAW_EXTRACT_BITS(bit0, num_bits, src) fastuidraw_extract_bits(uint(bit0), uint(num_bits), uint(src) )\n";

      for(std::list<SourcePrivate::source_code_type>::const_iterator
            iter= d->m_values.begin(), end = d->m_values.end(); iter != end; ++iter)
        {
          SourcePrivate::add_source_entry(*iter, output_glsl_source_code);
        }

      /*
        some GLSL pre-processors do not like to end on a
        comment or other certain tokens, to make them
        less grouchy, we emit a few extra \n's
      */
      output_glsl_source_code << "\n\n\n";

      d->m_assembled_code = output_glsl_source_code.str();
      d->m_dirty = false;
    }
  return d->m_assembled_code.c_str();
}


////////////////////////////////////////////////
//gl::Shader methods
fastuidraw::gl::Shader::
Shader(const shader_source &src, GLenum pshader_type)
{
  m_d = FASTUIDRAWnew ShaderPrivate(src, pshader_type);
}

fastuidraw::gl::Shader::
~Shader()
{
  ShaderPrivate *d;
  d = reinterpret_cast<ShaderPrivate*>(m_d);

  /*
    TODO: deletion of a shader should not be
    required to be done with a GL context
    current.
   */
  if(d->m_name)
    {
      glDeleteShader(d->m_name);
    }
  FASTUIDRAWdelete(d);
  m_d = NULL;
}


bool
fastuidraw::gl::Shader::
compile_success(void)
{
  ShaderPrivate *d;
  d = reinterpret_cast<ShaderPrivate*>(m_d);
  d->compile();
  return d->m_compile_success;
}

const char*
fastuidraw::gl::Shader::
compile_log(void)
{
  ShaderPrivate *d;
  d = reinterpret_cast<ShaderPrivate*>(m_d);
  d->compile();
  return d->m_compile_log.c_str();
}

GLuint
fastuidraw::gl::Shader::
name(void)
{
  ShaderPrivate *d;
  d = reinterpret_cast<ShaderPrivate*>(m_d);
  d->compile();
  return d->m_name;
}

bool
fastuidraw::gl::Shader::
shader_ready(void)
{
  ShaderPrivate *d;
  d = reinterpret_cast<ShaderPrivate*>(m_d);
  return d->m_shader_ready;
}

const char*
fastuidraw::gl::Shader::
source_code(void)
{
  ShaderPrivate *d;
  d = reinterpret_cast<ShaderPrivate*>(m_d);
  return d->m_source_code.c_str();
}

GLenum
fastuidraw::gl::Shader::
shader_type(void)
{
  ShaderPrivate *d;
  d = reinterpret_cast<ShaderPrivate*>(m_d);
  return d->m_shader_type;
}


const char*
fastuidraw::gl::Shader::
gl_shader_type_label(GLenum shader_type)
{
  #define CASE(X) case X: return #X

  switch(shader_type)
    {
    default:
      return "UNKNOWN_SHADER_STAGE";

      CASE(GL_FRAGMENT_SHADER);
      CASE(GL_VERTEX_SHADER);

      #ifdef GL_GEOMETRY_SHADER
        CASE(GL_GEOMETRY_SHADER);
      #endif

      #ifdef GL_TESS_EVALUATION_SHADER
        CASE(GL_TESS_EVALUATION_SHADER);
      #endif

      #ifdef GL_TESS_CONTROL_SHADER
        CASE(GL_TESS_CONTROL_SHADER);
      #endif
    }

  #undef CASE
}

////////////////////////////////////////
// fastuidraw::gl::BindAttribute methods
fastuidraw::gl::BindAttribute::
BindAttribute(const char *pname, int plocation)
{
  m_d = FASTUIDRAWnew BindAttributePrivate(pname, plocation);
}

fastuidraw::gl::BindAttribute::
~BindAttribute()
{
  BindAttributePrivate *d;
  d = reinterpret_cast<BindAttributePrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = NULL;
}

void
fastuidraw::gl::BindAttribute::
action(GLuint glsl_program) const
{
  BindAttributePrivate *d;
  d = reinterpret_cast<BindAttributePrivate*>(m_d);
  glBindAttribLocation(glsl_program, d->m_location, d->m_label.c_str());
}


////////////////////////////////////////////
// gl::PreLinkActionArray methods
fastuidraw::gl::PreLinkActionArray::
PreLinkActionArray(void)
{
  m_d = FASTUIDRAWnew PreLinkActionArrayPrivate();
}

fastuidraw::gl::PreLinkActionArray::
PreLinkActionArray(const PreLinkActionArray &obj)
{
  PreLinkActionArrayPrivate *d;
  d = reinterpret_cast<PreLinkActionArrayPrivate*>(obj.m_d);
  m_d = FASTUIDRAWnew PreLinkActionArrayPrivate(*d);
}

fastuidraw::gl::PreLinkActionArray::
~PreLinkActionArray()
{
  PreLinkActionArrayPrivate *d;
  d = reinterpret_cast<PreLinkActionArrayPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = NULL;
}

fastuidraw::gl::PreLinkActionArray&
fastuidraw::gl::PreLinkActionArray::
operator=(const PreLinkActionArray &rhs)
{
  if(this != &rhs)
    {
      PreLinkActionArrayPrivate *d, *rhs_d;
      d = reinterpret_cast<PreLinkActionArrayPrivate*>(m_d);
      rhs_d = reinterpret_cast<PreLinkActionArrayPrivate*>(rhs.m_d);
      *d = *rhs_d;
    }
  return *this;
}

fastuidraw::gl::PreLinkActionArray&
fastuidraw::gl::PreLinkActionArray::
add(PreLinkAction::handle h)
{
  PreLinkActionArrayPrivate *d;
  d = reinterpret_cast<PreLinkActionArrayPrivate*>(m_d);

  assert(h);
  d->m_values.push_back(h);
  return *this;
}

void
fastuidraw::gl::PreLinkActionArray::
execute_actions(GLuint pr) const
{
  PreLinkActionArrayPrivate *d;
  d = reinterpret_cast<PreLinkActionArrayPrivate*>(m_d);
  for(std::vector<PreLinkAction::handle>::const_iterator
        iter = d->m_values.begin(), end = d->m_values.end();
      iter != end; ++iter)
    {
      if(*iter)
        {
          (*iter)->action(pr);
        }
    }
}


///////////////////////////////////////////////////
// fastuidraw::gl::Program::parameter_info methods
fastuidraw::gl::Program::parameter_info::
parameter_info(void *d):
  m_d(d)
{}

fastuidraw::gl::Program::parameter_info::
parameter_info(void):
  m_d(NULL)
{}

const char*
fastuidraw::gl::Program::parameter_info::
name(void) const
{
  ParameterInfoPrivate *d;
  d = reinterpret_cast<ParameterInfoPrivate*>(m_d);
  return (d) ? d->m_name.c_str() : "";
}

GLenum
fastuidraw::gl::Program::parameter_info::
type(void) const
{
  ParameterInfoPrivate *d;
  d = reinterpret_cast<ParameterInfoPrivate*>(m_d);
  return (d) ? d->m_type : GL_INVALID_ENUM;
}

GLint
fastuidraw::gl::Program::parameter_info::
count(void) const
{
  ParameterInfoPrivate *d;
  d = reinterpret_cast<ParameterInfoPrivate*>(m_d);
  return (d) ? d->m_count : -1;
}

GLuint
fastuidraw::gl::Program::parameter_info::
index(void) const
{
  ParameterInfoPrivate *d;
  d = reinterpret_cast<ParameterInfoPrivate*>(m_d);
  return (d) ? d->m_index : -1;
}

GLint
fastuidraw::gl::Program::parameter_info::
location(void) const
{
  ParameterInfoPrivate *d;
  d = reinterpret_cast<ParameterInfoPrivate*>(m_d);
  return (d) ? d->m_location : -1;
}

/////////////////////////////////////////////////////////
//ProgramPrivate methods
void
ProgramPrivate::
assemble(void)
{
  if(m_assembled)
    {
      return;
    }

  std::ostringstream error_ostr;

  m_assembled = true;
  assert(m_name == 0);
  m_name = glCreateProgram();


  m_link_success = true;

  //attatch the shaders, attaching a bad shader makes
  //m_link_success become false
  for(std::vector<fastuidraw::gl::Shader::handle>::iterator iter = m_shaders.begin(),
        end = m_shaders.end(); iter != end; ++iter)
    {
      if((*iter)->compile_success())
        {
          glAttachShader(m_name, (*iter)->name());
        }
      else
        {
          m_link_success = false;
        }
    }

  //we no longer need the GL shaders.
  clear_shaders_and_save_shader_data();

  //perform any pre-link actions.
  m_pre_link_actions.execute_actions(m_name);

  //now finally link!
  glLinkProgram(m_name);

  //retrieve the log fun
  std::vector<char> raw_log;
  GLint logSize, linkOK;

  glGetProgramiv(m_name, GL_LINK_STATUS, &linkOK);
  glGetProgramiv(m_name, GL_INFO_LOG_LENGTH, &logSize);

  raw_log.resize(logSize+2);
  glGetProgramInfoLog(m_name, logSize+1, NULL , &raw_log[0]);

  error_ostr << "\n-----------------------\n" << &raw_log[0];

  m_link_log = error_ostr.str();
  m_link_success = m_link_success and (linkOK == GL_TRUE);

  if(m_link_success)
    {
      m_attribute_list.fill_hoard(m_name,
                                  GL_ACTIVE_ATTRIBUTES,
                                  GL_ACTIVE_ATTRIBUTE_MAX_LENGTH,
                                  FASTUIDRAWglfunctionPointer(glGetActiveAttrib),
                                  FASTUIDRAWglfunctionPointer(glGetAttribLocation));

      m_uniform_list.fill_hoard(m_name,
                                GL_ACTIVE_UNIFORMS,
                                GL_ACTIVE_UNIFORM_MAX_LENGTH,
                                FASTUIDRAWglfunctionPointer(glGetActiveUniform),
                                FASTUIDRAWglfunctionPointer(glGetUniformLocation));

    }

  generate_log();

  if(!m_link_success)
    {
      //since the program cannot be used,
      //clear it's initers..
      m_initializers = fastuidraw::gl::ProgramInitializerArray();

      std::ostringstream oo;
      oo << "bad_program_" << m_name << ".glsl";
      std::ofstream eek(oo.str().c_str());

      for(std::vector<ShaderData>::iterator iter = m_shader_data.begin(),
            end = m_shader_data.end(); iter!=end; ++iter)
        {
          eek << "\n\nshader: " << iter->m_name
              << "[" << fastuidraw::gl::Shader::gl_shader_type_label(iter->m_shader_type) << "]\n"
              << "shader_source:\n"
              << iter->m_source_code
              << "compile log:\n"
              << iter->m_compile_log;
        }
      eek << "\n\nLink Log: " << m_link_log;
    }
  m_pre_link_actions = fastuidraw::gl::PreLinkActionArray();
}

void
ProgramPrivate::
clear_shaders_and_save_shader_data(void)
{
  m_shader_data.resize(m_shaders.size());
  for(unsigned int i = 0, endi = m_shaders.size(); i<endi; ++i)
    {
      m_shader_data[i].m_source_code = m_shaders[i]->source_code();
      m_shader_data[i].m_name = m_shaders[i]->name();
      m_shader_data[i].m_shader_type = m_shaders[i]->shader_type();
      m_shader_data[i].m_compile_log = m_shaders[i]->compile_log();
    }
  m_shaders.clear();
}

void
ProgramPrivate::
generate_log(void)
{
  std::ostringstream ostr;

  ostr << "gl::Program: "
       << "[GLname: " << m_name << "]:\tShaders:";

  for(std::vector<ShaderData>::const_iterator
        iter = m_shader_data.begin(), end = m_shader_data.end();
      iter != end; ++iter)
    {
      ostr << "\n\nGLSL name=" << iter->m_name
           << ", type=" << fastuidraw::gl::Shader::gl_shader_type_label(iter->m_shader_type)
           << "\nSource:\n" << iter->m_source_code
           << "\nCompileLog:\n" << iter->m_compile_log;
    }

  ostr << "\nLink Log:\n" << m_link_log << "\n";

  if(m_link_success)
    {
      ostr << "\n\nUniforms:";
      for(std::vector<ParameterInfoPrivate>::const_iterator
            iter = m_uniform_list.m_values.begin(),
            end = m_uniform_list.m_values.end();
          iter != end; ++iter)
        {
          ostr << "\n\t"
               << iter->m_name
               << "\n\t\ttype=0x"
               << std::hex << iter->m_type
               << "\n\t\tcount=" << std::dec << iter->m_count
               << "\n\t\tindex=" << std::dec << iter->m_index
               << "\n\t\tlocation=" << iter->m_location;
        }

      ostr << "\n\nAttributes:";
      for(std::vector<ParameterInfoPrivate>::const_iterator
            iter = m_attribute_list.m_values.begin(),
            end = m_attribute_list.m_values.end();
          iter != end; ++iter)
        {
          ostr << "\n\t"
               << iter->m_name
               << "\n\t\ttype=0x"
               << std::hex << iter->m_type
               << "\n\t\tcount=" << std::dec << iter->m_count
               << "\n\t\tindex=" << std::dec << iter->m_index
               << "\n\t\tlocation=" << iter->m_location;
        }
    }

  m_log = ostr.str();
}

////////////////////////////////////////////////////////
//fastuidraw::gl::Program methods
fastuidraw::gl::Program::
Program(const_c_array<Shader::handle> pshaders,
        const PreLinkActionArray &action,
        const ProgramInitializerArray &initers)
{
  m_d = FASTUIDRAWnew ProgramPrivate(pshaders, action, initers, this);
}

fastuidraw::gl::Program::
Program(Shader::handle vert_shader,
        Shader::handle frag_shader,
        const PreLinkActionArray &action,
        const ProgramInitializerArray &initers)
{
  m_d = FASTUIDRAWnew ProgramPrivate(vert_shader, frag_shader, action, initers, this);
}

fastuidraw::gl::Program::
Program(const Shader::shader_source &vert_shader,
        const Shader::shader_source &frag_shader,
        const PreLinkActionArray &action,
        const ProgramInitializerArray &initers)
{
  m_d = FASTUIDRAWnew ProgramPrivate(vert_shader, frag_shader, action, initers, this);
}

fastuidraw::gl::Program::
~Program()
{
  ProgramPrivate *d;
  d = reinterpret_cast<ProgramPrivate*>(m_d);
  if(d->m_name)
    {
      glDeleteProgram(d->m_name);
    }
  FASTUIDRAWdelete(d);
  m_d = NULL;
}

void
fastuidraw::gl::Program::
use_program(void)
{
  ProgramPrivate *d;
  d = reinterpret_cast<ProgramPrivate*>(m_d);
  d->assemble();

  assert(d->m_name != 0);
  assert(d->m_link_success);

  glUseProgram(d->m_name);
  d->m_initializers.perform_initializations(this);
  d->m_initializers.clear();
}

GLuint
fastuidraw::gl::Program::
name(void)
{
  ProgramPrivate *d;
  d = reinterpret_cast<ProgramPrivate*>(m_d);
  d->assemble();
  return d->m_name;
}

const char*
fastuidraw::gl::Program::
link_log(void)
{
  ProgramPrivate *d;
  d = reinterpret_cast<ProgramPrivate*>(m_d);
  d->assemble();
  return d->m_link_log.c_str();
}

bool
fastuidraw::gl::Program::
link_success(void)
{
  ProgramPrivate *d;
  d = reinterpret_cast<ProgramPrivate*>(m_d);
  d->assemble();
  return d->m_link_success;
}

const char*
fastuidraw::gl::Program::
log(void)
{
  ProgramPrivate *d;
  d = reinterpret_cast<ProgramPrivate*>(m_d);
  d->assemble();
  return d->m_log.c_str();
}

unsigned int
fastuidraw::gl::Program::
number_active_uniforms(void)
{
  ProgramPrivate *d;
  d = reinterpret_cast<ProgramPrivate*>(m_d);
  d->assemble();
  return d->m_uniform_list.m_values.size();
}

fastuidraw::gl::Program::parameter_info
fastuidraw::gl::Program::
active_uniform(unsigned int I)
{
  ProgramPrivate *d;
  d = reinterpret_cast<ProgramPrivate*>(m_d);
  d->assemble();
  assert(I < d->m_uniform_list.m_values.size());
  return parameter_info(&d->m_uniform_list.m_values[I]);
}

GLint
fastuidraw::gl::Program::
uniform_location(const char *pname)
{
  FindParameterResult R;
  ProgramPrivate *d;

  d = reinterpret_cast<ProgramPrivate*>(m_d);
  d->assemble();
  R = d->m_uniform_list.find_parameter(pname);
  return R.first;
}

unsigned int
fastuidraw::gl::Program::
number_active_attributes(void)
{
  ProgramPrivate *d;
  d = reinterpret_cast<ProgramPrivate*>(m_d);
  d->assemble();
  return d->m_attribute_list.m_values.size();
}

fastuidraw::gl::Program::parameter_info
fastuidraw::gl::Program::
active_attribute(unsigned int I)
{
  ProgramPrivate *d;
  d = reinterpret_cast<ProgramPrivate*>(m_d);
  d->assemble();
  assert(I < d->m_attribute_list.m_values.size());
  return parameter_info(&d->m_attribute_list.m_values[I]);
}

GLint
fastuidraw::gl::Program::
attribute_location(const char *pname)
{
  FindParameterResult R;
  ProgramPrivate *d;

  d = reinterpret_cast<ProgramPrivate*>(m_d);
  d->assemble();
  R = d->m_attribute_list.find_parameter(pname);
  return R.first;
}

//////////////////////////////////////
//gl::ProgramInitializerArray methods
fastuidraw::gl::ProgramInitializerArray::
ProgramInitializerArray(void)
{
  m_d = FASTUIDRAWnew ProgramInitializerArrayPrivate();
}

fastuidraw::gl::ProgramInitializerArray::
ProgramInitializerArray(const ProgramInitializerArray &obj)
{
  ProgramInitializerArrayPrivate *d;
  d = reinterpret_cast<ProgramInitializerArrayPrivate*>(obj.m_d);
  m_d = FASTUIDRAWnew ProgramInitializerArrayPrivate(*d);
}


fastuidraw::gl::ProgramInitializerArray::
~ProgramInitializerArray()
{
  ProgramInitializerArrayPrivate *d;
  d = reinterpret_cast<ProgramInitializerArrayPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = NULL;
}

fastuidraw::gl::ProgramInitializerArray&
fastuidraw::gl::ProgramInitializerArray::
operator=(const ProgramInitializerArray &rhs)
{
  if(this != &rhs)
    {
      ProgramInitializerArrayPrivate *d, *rhs_d;
      d = reinterpret_cast<ProgramInitializerArrayPrivate*>(m_d);
      rhs_d = reinterpret_cast<ProgramInitializerArrayPrivate*>(rhs.m_d);
      *d = *rhs_d;
    }
  return *this;
}

fastuidraw::gl::ProgramInitializerArray&
fastuidraw::gl::ProgramInitializerArray::
add(ProgramInitializer::handle h)
{
  ProgramInitializerArrayPrivate *d;
  d = reinterpret_cast<ProgramInitializerArrayPrivate*>(m_d);
  d->m_values.push_back(h);
  return *this;
}


void
fastuidraw::gl::ProgramInitializerArray::
perform_initializations(Program *pr) const
{
  ProgramInitializerArrayPrivate *d;
  d = reinterpret_cast<ProgramInitializerArrayPrivate*>(m_d);
  for(std::vector<ProgramInitializer::handle>::const_iterator
        iter = d->m_values.begin(), end = d->m_values.end();
      iter != end; ++iter)
    {
      const ProgramInitializer::handle &v(*iter);
      assert(v);
      v->perform_initialization(pr);
    }
}

void
fastuidraw::gl::ProgramInitializerArray::
clear(void)
{
  ProgramInitializerArrayPrivate *d;
  d = reinterpret_cast<ProgramInitializerArrayPrivate*>(m_d);
  d->m_values.clear();
}

//////////////////////////////////////////////////
// fastuidraw::gl::UniformBlockInitializer methods
fastuidraw::gl::UniformBlockInitializer::
UniformBlockInitializer(const char *uniform_name, int binding_point_index)
{
  m_d = FASTUIDRAWnew UniformBlockInitializerPrivate(uniform_name, binding_point_index);
}

fastuidraw::gl::UniformBlockInitializer::
~UniformBlockInitializer()
{
  UniformBlockInitializerPrivate *d;
  d = reinterpret_cast<UniformBlockInitializerPrivate*>(m_d);
  assert(d != NULL);
  FASTUIDRAWdelete(d);
  m_d = NULL;
}

void
fastuidraw::gl::UniformBlockInitializer::
perform_initialization(Program *pr) const
{
  UniformBlockInitializerPrivate *d;
  d = reinterpret_cast<UniformBlockInitializerPrivate*>(m_d);
  assert(d != NULL);

  int loc;
  loc = glGetUniformBlockIndex(pr->name(), d->m_block_name.c_str());
  if(loc != -1)
    {
      glUniformBlockBinding(pr->name(), loc, d->m_binding_point);
    }
  else
    {
      std::cerr << "Failed to find uniform block \""
                << d->m_block_name
                << "\" in program " << pr->name()
                << " for initialization\n";
    }
}

/////////////////////////////////////////////////
// fastuidraw::gl::UniformInitalizerBase methods
fastuidraw::gl::UniformInitalizerBase::
UniformInitalizerBase(const char *uniform_name)
{
  assert(uniform_name);
  m_d = FASTUIDRAWnew std::string(uniform_name);
}

fastuidraw::gl::UniformInitalizerBase::
~UniformInitalizerBase()
{
  std::string *d;
  d = reinterpret_cast<std::string*>(m_d);
  assert(d != NULL);
  FASTUIDRAWdelete(d);
  m_d = NULL;
}

void
fastuidraw::gl::UniformInitalizerBase::
perform_initialization(Program *pr) const
{
  std::string *d;
  d = reinterpret_cast<std::string*>(m_d);
  assert(d != NULL);

  int loc;
  loc = pr->uniform_location(d->c_str());
  if(loc != -1)
    {
      init_uniform(loc);
    }
  else
    {
      std::cerr << "Failed to find uniform \"" << *d
                << "\" in program " << pr->name()
                << " for initialization\n";
    }
}
