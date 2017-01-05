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
#include <sys/time.h>

#include <fastuidraw/util/static_resource.hpp>
#include <fastuidraw/gl_backend/ngl_header.hpp>
#include <fastuidraw/gl_backend/gl_get.hpp>
#include <fastuidraw/gl_backend/gl_program.hpp>

namespace
{
  class ShaderPrivate
  {
  public:
    ShaderPrivate(const fastuidraw::glsl::ShaderSource &src,
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
    std::vector<fastuidraw::reference_counted_ptr<fastuidraw::gl::PreLinkAction> > m_values;
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
    std::vector<fastuidraw::reference_counted_ptr<fastuidraw::gl::ProgramInitializer> > m_values;
  };

  class ParameterInfoPrivate
  {
  public:
    ParameterInfoPrivate(void):
      m_name(""),
      m_type(GL_INVALID_ENUM),
      m_count(0),
      m_index(-1),
      m_location(-1),
      m_block_index(-1),
      m_offset(-1),
      m_array_stride(-1),
      m_matrix_stride(-1),
      m_is_row_major(0),
      m_abo_index(-1)
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

    GLint m_block_index;
    GLint m_offset;
    GLint m_array_stride;
    GLint m_matrix_stride;
    GLint m_is_row_major;
    GLint m_abo_index;
  };

  class FindParameterResult
  {
  public:
    FindParameterResult(void)
    {}

    FindParameterResult(int location, unsigned int idx):
      m_location(location),
      m_idx(idx)
    {}

    int m_location;
    unsigned int m_idx;
  };

  class ParameterInfoPrivateHoard
  {
  public:
    ParameterInfoPrivateHoard(void):
      m_finalized(false)
    {}

    void
    add_element(const ParameterInfoPrivate &value)
    {
      assert(!m_finalized);
      m_values.push_back(value);
    }

    template<typename F, typename G>
    void
    fill_hoard(GLuint program,
               GLenum count_enum, GLenum length_enum,
               F fptr, G gptr, bool fill_ubo_data);

    void
    finalize(void);

    FindParameterResult
    find_parameter(const std::string &pname) const;

    const std::vector<ParameterInfoPrivate>&
    values(void) const
    {
      return m_values;
    }

    const ParameterInfoPrivate&
    value(unsigned int I) const
    {
      return I < m_values.size() ?
        m_values[I] :
        m_empty;
    }

  private:
    template<typename iterator>
    static
    std::string
    filter_name(iterator begin, iterator end, int &array_index);

    bool m_finalized;
    ParameterInfoPrivate m_empty;
    std::vector<ParameterInfoPrivate> m_values;
    std::map<std::string, unsigned int> m_map;
  };

  class AtomicBufferInfoPrivate
  {
  public:
    AtomicBufferInfoPrivate(void):
      m_buffer_index(-1),
      m_size_bytes(0)
    {}

    GLint m_buffer_index;
    GLint m_size_bytes;
    ParameterInfoPrivateHoard m_members;
  };

  class UnformBlockInfoPrivate
  {
  public:
    UnformBlockInfoPrivate(void):
      m_block_index(-1),
      m_size_bytes(0)
    {}

    std::string m_name;
    GLint m_block_index;
    GLint m_size_bytes;
    ParameterInfoPrivateHoard m_members;
  };

  class UnformBlockInfoPrivateHoard
  {
  public:
    void
    fill_hoard(GLuint program,
               const std::vector<ParameterInfoPrivate> &all_uniforms);

    const UnformBlockInfoPrivate*
    default_uniform_block(void) const
    {
      return &m_default_block;
    }

    unsigned int
    number_active_uniform_blocks(void) const
    {
      return m_blocks_sorted.size();
    }

    const UnformBlockInfoPrivate*
    uniform_block(unsigned int I)
    {
      assert(I < m_blocks_sorted.size());
      return m_blocks_sorted[I];
    }

    unsigned int
    uniform_block_id(const std::string &name) const
    {
      std::map<std::string, unsigned int>::const_iterator iter;
      iter = m_map.find(name);
      return (iter != m_map.end()) ?
        iter->second:
        ~0u;
    }

    unsigned int
    number_atomic_buffers(void) const
    {
      return m_abo_buffers.size();
    }

    const AtomicBufferInfoPrivate*
    atomic_buffer(unsigned int I) const
    {
      assert(I < m_abo_buffers.size());
      return &m_abo_buffers[I];
    }

  private:
    static
    bool
    compare_function(const UnformBlockInfoPrivate *lhs,
                     const UnformBlockInfoPrivate *rhs)
    {
      assert(lhs != NULL);
      assert(rhs != NULL);
      return lhs->m_name < rhs->m_name;
    }

    UnformBlockInfoPrivate m_default_block;
    std::vector<UnformBlockInfoPrivate> m_blocks;
    std::vector<UnformBlockInfoPrivate*> m_blocks_sorted;
    std::vector<AtomicBufferInfoPrivate> m_abo_buffers;
    std::map<std::string, unsigned int> m_map; //gives indice into m_blocks_sorted
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
    ProgramPrivate(const fastuidraw::const_c_array<fastuidraw::reference_counted_ptr<fastuidraw::gl::Shader> > pshaders,
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

    ProgramPrivate(fastuidraw::reference_counted_ptr<fastuidraw::gl::Shader> vert_shader,
                   fastuidraw::reference_counted_ptr<fastuidraw::gl::Shader> frag_shader,
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

    ProgramPrivate(const fastuidraw::glsl::ShaderSource &vert_shader,
                   const fastuidraw::glsl::ShaderSource &frag_shader,
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
    assemble(fastuidraw::gl::Program *program);

    void
    clear_shaders_and_save_shader_data(void);

    void
    generate_log(void);

    std::vector<fastuidraw::reference_counted_ptr<fastuidraw::gl::Shader> > m_shaders;
    std::vector<ShaderData> m_shader_data;
    std::map<GLenum, std::vector<int> > m_shader_data_sorted_by_type;

    GLuint m_name;
    bool m_link_success, m_assembled;
    std::string m_link_log;
    std::string m_log;
    float m_assemble_time;

    std::set<std::string> m_binded_attributes;
    ParameterInfoPrivateHoard m_uniform_list;
    ParameterInfoPrivateHoard m_attribute_list;
    UnformBlockInfoPrivateHoard m_uniform_block_list;
    std::vector<AtomicBufferInfoPrivate> m_abo_list;
    fastuidraw::gl::ProgramInitializerArray m_initializers;
    fastuidraw::gl::PreLinkActionArray m_pre_link_actions;
    fastuidraw::gl::Program *m_p;

  };
}

/////////////////////////////////////////
// ShaderPrivate methods
ShaderPrivate::
ShaderPrivate(const fastuidraw::glsl::ShaderSource &src,
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
           F fptr, G gptr, bool fill_ubo_data)
{
  GLint count;

  glGetProgramiv(program, count_enum, &count);

  if(count > 0)
    {
      GLint largest_length(0);
      std::vector<char> pname;

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

      if(fill_ubo_data)
        {
          std::vector<GLuint> indxs(m_values.size());
          std::vector<GLint> block_idxs(m_values.size());
          std::vector<GLint> offsets(m_values.size());
          std::vector<GLint> array_strides(m_values.size());
          std::vector<GLint> matrix_strides(m_values.size());
          std::vector<GLint> is_row_major(m_values.size());
          std::vector<GLint> abo_index(m_values.size());

          for(unsigned int i = 0, endi = indxs.size(); i < endi; ++i)
            {
              indxs[i] = m_values[i].m_index;
            }

          glGetActiveUniformsiv(program, indxs.size(), &indxs[0], GL_UNIFORM_BLOCK_INDEX, &block_idxs[0]);
          glGetActiveUniformsiv(program, indxs.size(), &indxs[0], GL_UNIFORM_OFFSET, &offsets[0]);
          glGetActiveUniformsiv(program, indxs.size(), &indxs[0], GL_UNIFORM_ARRAY_STRIDE, &array_strides[0]);
          glGetActiveUniformsiv(program, indxs.size(), &indxs[0], GL_UNIFORM_MATRIX_STRIDE, &matrix_strides[0]);
          glGetActiveUniformsiv(program, indxs.size(), &indxs[0], GL_UNIFORM_IS_ROW_MAJOR, &is_row_major[0]);
          glGetActiveUniformsiv(program, indxs.size(), &indxs[0], GL_UNIFORM_ATOMIC_COUNTER_BUFFER_INDEX, &abo_index[0]);

          for(unsigned int i = 0, endi = indxs.size(); i < endi; ++i)
            {
              ParameterInfoPrivate &dst(m_values[i]);

              dst.m_block_index = block_idxs[i];
              dst.m_offset = offsets[i];
              dst.m_array_stride = array_strides[i];
              dst.m_matrix_stride = matrix_strides[i];
              dst.m_is_row_major = is_row_major[i];
              dst.m_abo_index = abo_index[i];
            }
        }

      finalize();
    }
}

void
ParameterInfoPrivateHoard::
finalize(void)
{
  assert(!m_finalized);
  m_finalized = true;

  /* sort m_values by name and then make our map
   */
  std::sort(m_values.begin(), m_values.end());
  for(unsigned int i = 0, endi = m_values.size(); i < endi; ++i)
    {
      m_map[m_values[i].m_name] = i;
    }
}


FindParameterResult
ParameterInfoPrivateHoard::
find_parameter(const std::string &pname) const
{
  std::map<std::string, unsigned int>::const_iterator iter;

  assert(m_finalized);
  iter = m_map.find(pname);
  if(iter != m_map.end())
    {
      const ParameterInfoPrivate *q;
      q = &m_values[iter->second];
      return FindParameterResult(q->m_location, iter->second);
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
          return FindParameterResult(q->m_location + array_index, iter->second);
        }
    }

  return FindParameterResult(-1, ~0u);
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

////////////////////////////////////////////////
//UnformBlockInfoPrivateHoard methods
void
UnformBlockInfoPrivateHoard::
fill_hoard(GLuint program,
           const std::vector<ParameterInfoPrivate> &all_uniforms)
{
  /* get what we need...
   */
  GLint count(0), abo_count(0);

  glGetProgramiv(program, GL_ACTIVE_UNIFORM_BLOCKS, &count);
  if(count > 0)
    {
      GLint largest_length(0);

      std::vector<char> pname;
      m_blocks.resize(count);
      m_blocks_sorted.resize(count);

      glGetProgramiv(program, GL_ACTIVE_UNIFORM_BLOCK_MAX_NAME_LENGTH, &largest_length);
      ++largest_length;
      pname.resize(largest_length, '\0');

      for(int i = 0; i != count; ++i)
        {
          GLsizei name_length(0), psize(0);
          std::memset(&pname[0], 0, largest_length);

          glGetActiveUniformBlockName(program, i, largest_length, &name_length, &pname[0]);
          glGetActiveUniformBlockiv(program, i, GL_UNIFORM_BLOCK_DATA_SIZE, &psize);

          m_blocks[i].m_block_index = i;
          m_blocks[i].m_name = std::string(pname.begin(), pname.begin() + name_length);
          m_blocks[i].m_size_bytes = psize;
          m_blocks_sorted[i] = &m_blocks[i];
        }
    }

  #ifdef FASTUIDRAW_GL_USE_GLES
    {
      fastuidraw::gl::ContextProperties ctx;
      if(ctx.version() >= fastuidraw::ivec2(3, 1))
        {
          glGetProgramiv(program, GL_ACTIVE_ATOMIC_COUNTER_BUFFERS, &abo_count);
        }
    }
  #else
    {
      glGetProgramiv(program, GL_ACTIVE_ATOMIC_COUNTER_BUFFERS, &abo_count);
    }
  #endif

  if(abo_count > 0)
    {
      m_abo_buffers.resize(abo_count);
      for(int i = 0; i != count; ++i)
        {
          GLint sz(0);
          glGetActiveAtomicCounterBufferiv(program, i, GL_ATOMIC_COUNTER_BUFFER_DATA_SIZE, &sz);
          m_abo_buffers[i].m_buffer_index = i;
          m_abo_buffers[i].m_size_bytes = sz;
        }
    }

  /* extract uniform data from all_uniforms, note that
     m_blocks[i] holds the uniform block with block_index
     of value i currently.
  */
  for(std::vector<ParameterInfoPrivate>::const_iterator iter = all_uniforms.begin(),
        end = all_uniforms.end(); iter != end; ++iter)
    {
      if(iter->m_block_index != -1)
        {
          m_blocks[iter->m_block_index].m_members.add_element(*iter);
        }
      else
        {
          m_default_block.m_members.add_element(*iter);
        }

      if(iter->m_abo_index != -1)
        {
          m_abo_buffers[iter->m_abo_index].m_members.add_element(*iter);
        }
    }

  /* sort and finalize each uniform block
   */
  for(unsigned int i = 0, endi = m_abo_buffers.size(); i < endi; ++i)
    {
      m_abo_buffers[i].m_members.finalize();
    }

  std::sort(m_blocks_sorted.begin(), m_blocks_sorted.end(), compare_function);
  for(unsigned int i = 0, endi = m_blocks_sorted.size(); i < endi; ++i)
    {
      m_blocks_sorted[i]->m_members.finalize();
      m_map[m_blocks_sorted[i]->m_name] = i;
    }
  m_default_block.m_members.finalize();

}

////////////////////////////////////////////////
// fastuidraw::gl::Shader methods
fastuidraw::gl::Shader::
Shader(const glsl::ShaderSource &src, GLenum pshader_type)
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

const char*
fastuidraw::gl::Shader::
default_shader_version(void)
{
  #ifdef FASTUIDRAW_GL_USE_GLES
    {
      return "300 es";
    }
  #else
    {
      return "330";
    }
  #endif
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
// fastuidraw::gl::PreLinkActionArray methods
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
add(reference_counted_ptr<PreLinkAction> h)
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
  for(std::vector<reference_counted_ptr<PreLinkAction> >::const_iterator
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
parameter_info(const void *d):
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
  const ParameterInfoPrivate *d;
  d = reinterpret_cast<const ParameterInfoPrivate*>(m_d);
  return (d) ? d->m_name.c_str() : "";
}

GLenum
fastuidraw::gl::Program::parameter_info::
type(void) const
{
  const ParameterInfoPrivate *d;
  d = reinterpret_cast<const ParameterInfoPrivate*>(m_d);
  return (d) ? d->m_type : GL_INVALID_ENUM;
}

GLint
fastuidraw::gl::Program::parameter_info::
count(void) const
{
  const ParameterInfoPrivate *d;
  d = reinterpret_cast<const ParameterInfoPrivate*>(m_d);
  return (d) ? d->m_count : -1;
}

GLuint
fastuidraw::gl::Program::parameter_info::
index(void) const
{
  const ParameterInfoPrivate *d;
  d = reinterpret_cast<const ParameterInfoPrivate*>(m_d);
  return (d) ? d->m_index : -1;
}

GLint
fastuidraw::gl::Program::parameter_info::
location(void) const
{
  const ParameterInfoPrivate *d;
  d = reinterpret_cast<const ParameterInfoPrivate*>(m_d);
  return (d) ? d->m_location : -1;
}

GLint
fastuidraw::gl::Program::parameter_info::
block_index(void) const
{
  const ParameterInfoPrivate *d;
  d = reinterpret_cast<const ParameterInfoPrivate*>(m_d);
  return (d) ? d->m_block_index : -1;
}

GLint
fastuidraw::gl::Program::parameter_info::
buffer_offset(void) const
{
  const ParameterInfoPrivate *d;
  d = reinterpret_cast<const ParameterInfoPrivate*>(m_d);
  return (d) ? d->m_offset : -1;
}

GLint
fastuidraw::gl::Program::parameter_info::
array_stride(void) const
{
  const ParameterInfoPrivate *d;
  d = reinterpret_cast<const ParameterInfoPrivate*>(m_d);
  return (d) ? d->m_array_stride : -1;
}

GLint
fastuidraw::gl::Program::parameter_info::
matrix_stride(void) const
{
  const ParameterInfoPrivate *d;
  d = reinterpret_cast<const ParameterInfoPrivate*>(m_d);
  return (d) ? d->m_matrix_stride : -1;
}

bool
fastuidraw::gl::Program::parameter_info::
is_row_major(void) const
{
  const ParameterInfoPrivate *d;
  d = reinterpret_cast<const ParameterInfoPrivate*>(m_d);
  return (d) ? d->m_is_row_major : false;
}

GLint
fastuidraw::gl::Program::parameter_info::
abo_index(void) const
{
  const ParameterInfoPrivate *d;
  d = reinterpret_cast<const ParameterInfoPrivate*>(m_d);
  return (d) ? d->m_abo_index : -1;
}

///////////////////////////////////////////////////
// fastuidraw::gl::Program::uniform_block_info methods
fastuidraw::gl::Program::uniform_block_info::
uniform_block_info(const void *d):
  m_d(d)
{}

fastuidraw::gl::Program::uniform_block_info::
uniform_block_info(void):
  m_d(NULL)
{}

const char*
fastuidraw::gl::Program::uniform_block_info::
name(void) const
{
  const UnformBlockInfoPrivate *d;
  d = reinterpret_cast<const UnformBlockInfoPrivate*>(m_d);
  return (d) ? d->m_name.c_str() : "";
}

GLint
fastuidraw::gl::Program::uniform_block_info::
block_index(void) const
{
  const UnformBlockInfoPrivate *d;
  d = reinterpret_cast<const UnformBlockInfoPrivate*>(m_d);
  return (d) ? d->m_block_index : -1;
}

GLint
fastuidraw::gl::Program::uniform_block_info::
buffer_size(void) const
{
  const UnformBlockInfoPrivate *d;
  d = reinterpret_cast<const UnformBlockInfoPrivate*>(m_d);
  return (d) ? d->m_size_bytes : 0;
}

unsigned int
fastuidraw::gl::Program::uniform_block_info::
number_uniforms(void) const
{
  const UnformBlockInfoPrivate *d;
  d = reinterpret_cast<const UnformBlockInfoPrivate*>(m_d);
  return d ? d->m_members.values().size() : 0;
}

fastuidraw::gl::Program::parameter_info
fastuidraw::gl::Program::uniform_block_info::
uniform(unsigned int I)
{
  const UnformBlockInfoPrivate *d;
  const void *q;

  d = reinterpret_cast<const UnformBlockInfoPrivate*>(m_d);
  q = d ? &d->m_members.value(I) : NULL;
  return parameter_info(q);
}

unsigned int
fastuidraw::gl::Program::uniform_block_info::
uniform_index(const char *name)
{
  const UnformBlockInfoPrivate *d;
  d = reinterpret_cast<const UnformBlockInfoPrivate*>(m_d);
  if(d != NULL)
    {
      FindParameterResult R;
      R = d->m_members.find_parameter(name);
      return R.m_idx;
    }
  return ~0u;
}

///////////////////////////////////////////////////
// fastuidraw::gl::Program::atomic_buffer_info methods
fastuidraw::gl::Program::atomic_buffer_info::
atomic_buffer_info(const void *d):
  m_d(d)
{}

fastuidraw::gl::Program::atomic_buffer_info::
atomic_buffer_info(void):
  m_d(NULL)
{}

GLint
fastuidraw::gl::Program::atomic_buffer_info::
buffer_index(void) const
{
  const AtomicBufferInfoPrivate *d;
  d = reinterpret_cast<const AtomicBufferInfoPrivate*>(m_d);
  return (d) ? d->m_buffer_index : -1;
}

GLint
fastuidraw::gl::Program::atomic_buffer_info::
buffer_size(void) const
{
  const AtomicBufferInfoPrivate *d;
  d = reinterpret_cast<const AtomicBufferInfoPrivate*>(m_d);
  return (d) ? d->m_size_bytes : 0;
}

unsigned int
fastuidraw::gl::Program::atomic_buffer_info::
number_atomic_variables(void) const
{
  const AtomicBufferInfoPrivate *d;
  d = reinterpret_cast<const AtomicBufferInfoPrivate*>(m_d);
  return d ? d->m_members.values().size() : 0;
}

fastuidraw::gl::Program::parameter_info
fastuidraw::gl::Program::atomic_buffer_info::
atomic_variable(unsigned int I)
{
  const AtomicBufferInfoPrivate *d;
  const void *q;

  d = reinterpret_cast<const AtomicBufferInfoPrivate*>(m_d);
  q = d ? &d->m_members.value(I) : NULL;
  return parameter_info(q);
}

unsigned int
fastuidraw::gl::Program::atomic_buffer_info::
atomic_variable_index(const char *name)
{
  const AtomicBufferInfoPrivate *d;
  d = reinterpret_cast<const AtomicBufferInfoPrivate*>(m_d);
  if(d != NULL)
    {
      FindParameterResult R;
      R = d->m_members.find_parameter(name);
      return R.m_idx;
    }
  return ~0u;
}


/////////////////////////////////////////////////////////
//ProgramPrivate methods
void
ProgramPrivate::
assemble(fastuidraw::gl::Program *program)
{
  if(m_assembled)
    {
      return;
    }

  struct timeval start_time, end_time;
  gettimeofday(&start_time, NULL);

  std::ostringstream error_ostr;

  m_assembled = true;
  assert(m_name == 0);
  m_name = glCreateProgram();
  m_link_success = true;

  //attatch the shaders, attaching a bad shader makes
  //m_link_success become false
  for(std::vector<fastuidraw::reference_counted_ptr<fastuidraw::gl::Shader> >::iterator iter = m_shaders.begin(),
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

  //perform any pre-link actions and then clear them
  m_pre_link_actions.execute_actions(m_name);
  m_pre_link_actions = fastuidraw::gl::PreLinkActionArray();

  //now finally link!
  glLinkProgram(m_name);

  gettimeofday(&end_time, NULL);
  m_assemble_time = float(end_time.tv_sec - start_time.tv_sec)
    + float(end_time.tv_usec - start_time.tv_usec) / 1e6f;

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
                                  FASTUIDRAWglfunctionPointer(glGetAttribLocation),
                                  false);

      m_uniform_list.fill_hoard(m_name,
                                GL_ACTIVE_UNIFORMS,
                                GL_ACTIVE_UNIFORM_MAX_LENGTH,
                                FASTUIDRAWglfunctionPointer(glGetActiveUniform),
                                FASTUIDRAWglfunctionPointer(glGetUniformLocation),
                                true);

      m_uniform_block_list.fill_hoard(m_name, m_uniform_list.values());

      int current_program;
      current_program = fastuidraw::gl::context_get<int>(GL_CURRENT_PROGRAM);
      glUseProgram(m_name);
      m_initializers.perform_initializations(program);
      glUseProgram(current_program);
    }
  else
    {
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
  generate_log();
  m_initializers.clear();
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
      m_shader_data_sorted_by_type[m_shader_data[i].m_shader_type].push_back(i);
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
      ostr << "\n\nUniforms of Default Block:";
      for(std::vector<ParameterInfoPrivate>::const_iterator
            iter = m_uniform_block_list.default_uniform_block()->m_members.values().begin(),
            end = m_uniform_block_list.default_uniform_block()->m_members.values().end();
          iter != end; ++iter)
        {
          assert(iter->m_block_index == -1);
          ostr << "\n\t" << iter->m_name
               << "\n\t\ttype=0x"
               << std::hex << iter->m_type
               << "\n\t\tcount=" << std::dec << iter->m_count
               << "\n\t\tindex=" << std::dec << iter->m_index
               << "\n\t\tlocation=" << iter->m_location
               << "\n\t\tabo_index=" << iter->m_abo_index;
        }

      for(unsigned int endi = m_uniform_block_list.number_active_uniform_blocks(),
            i = 0; i < endi; ++i)
        {
          const UnformBlockInfoPrivate *ubo(m_uniform_block_list.uniform_block(i));
          ostr << "\n\nUniformBlock:" << ubo->m_name
               << "\n\tblock_index=" << ubo->m_block_index
               << "\n\tblock_size=" << ubo->m_size_bytes
               << "\n\tmembers:";

          for(std::vector<ParameterInfoPrivate>::const_iterator
                iter = ubo->m_members.values().begin(),
                end = ubo->m_members.values().end();
              iter != end; ++iter)
            {
              assert(iter->m_block_index == ubo->m_block_index);
              ostr << "\n\t\t" << iter->m_name
                   << "\n\t\t\ttype=0x"
                   << std::hex << iter->m_type
                   << "\n\t\t\tcount=" << std::dec << iter->m_count
                   << "\n\t\t\tindex=" << std::dec << iter->m_index
                   << "\n\t\t\tblock_index=" << iter->m_block_index
                   << "\n\t\t\toffset=" << iter->m_offset
                   << "\n\t\t\tarray_stride=" << iter->m_array_stride
                   << "\n\t\t\tmatrix_stride=" << iter->m_matrix_stride
                   << "\n\t\t\tis_row_major=" << bool(iter->m_is_row_major);
            }
        }

      ostr << "\n\nAttributes:";
      for(std::vector<ParameterInfoPrivate>::const_iterator
            iter = m_attribute_list.values().begin(),
            end = m_attribute_list.values().end();
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
Program(const_c_array<reference_counted_ptr<Shader> > pshaders,
        const PreLinkActionArray &action,
        const ProgramInitializerArray &initers)
{
  m_d = FASTUIDRAWnew ProgramPrivate(pshaders, action, initers, this);
}

fastuidraw::gl::Program::
Program(reference_counted_ptr<Shader> vert_shader,
        reference_counted_ptr<Shader> frag_shader,
        const PreLinkActionArray &action,
        const ProgramInitializerArray &initers)
{
  m_d = FASTUIDRAWnew ProgramPrivate(vert_shader, frag_shader, action, initers, this);
}

fastuidraw::gl::Program::
Program(const glsl::ShaderSource &vert_shader,
        const glsl::ShaderSource &frag_shader,
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
  d->assemble(this);

  assert(d->m_name != 0);
  assert(d->m_link_success);

  glUseProgram(d->m_name);
}

GLuint
fastuidraw::gl::Program::
name(void)
{
  ProgramPrivate *d;
  d = reinterpret_cast<ProgramPrivate*>(m_d);
  d->assemble(this);
  return d->m_name;
}

const char*
fastuidraw::gl::Program::
link_log(void)
{
  ProgramPrivate *d;
  d = reinterpret_cast<ProgramPrivate*>(m_d);
  d->assemble(this);
  return d->m_link_log.c_str();
}

float
fastuidraw::gl::Program::
program_build_time(void)
{
  ProgramPrivate *d;
  d = reinterpret_cast<ProgramPrivate*>(m_d);
  d->assemble(this);
  return d->m_assemble_time;
}

bool
fastuidraw::gl::Program::
link_success(void)
{
  ProgramPrivate *d;
  d = reinterpret_cast<ProgramPrivate*>(m_d);
  d->assemble(this);
  return d->m_link_success;
}

const char*
fastuidraw::gl::Program::
log(void)
{
  ProgramPrivate *d;
  d = reinterpret_cast<ProgramPrivate*>(m_d);
  d->assemble(this);
  return d->m_log.c_str();
}

unsigned int
fastuidraw::gl::Program::
number_active_uniforms(void)
{
  ProgramPrivate *d;
  d = reinterpret_cast<ProgramPrivate*>(m_d);
  d->assemble(this);
  return d->m_uniform_list.values().size();
}

fastuidraw::gl::Program::parameter_info
fastuidraw::gl::Program::
active_uniform(unsigned int I)
{
  ProgramPrivate *d;
  d = reinterpret_cast<ProgramPrivate*>(m_d);
  d->assemble(this);
  return parameter_info(&d->m_uniform_list.value(I));
}

unsigned int
fastuidraw::gl::Program::
active_uniform_id(const char *pname)
{
  FindParameterResult R;
  ProgramPrivate *d;

  d = reinterpret_cast<ProgramPrivate*>(m_d);
  d->assemble(this);
  R = d->m_uniform_list.find_parameter(pname);
  return R.m_idx;
}

GLint
fastuidraw::gl::Program::
uniform_location(const char *pname)
{
  FindParameterResult R;
  ProgramPrivate *d;

  d = reinterpret_cast<ProgramPrivate*>(m_d);
  d->assemble(this);
  R = d->m_uniform_list.find_parameter(pname);
  return R.m_location;
}

fastuidraw::gl::Program::uniform_block_info
fastuidraw::gl::Program::
default_uniform_block(void)
{
  ProgramPrivate *d;
  d = reinterpret_cast<ProgramPrivate*>(m_d);
  d->assemble(this);
  return uniform_block_info(d->m_uniform_block_list.default_uniform_block());
}

unsigned int
fastuidraw::gl::Program::
number_active_uniform_blocks(void)
{
  ProgramPrivate *d;
  d = reinterpret_cast<ProgramPrivate*>(m_d);
  d->assemble(this);
  return d->m_uniform_block_list.number_active_uniform_blocks();
}

fastuidraw::gl::Program::uniform_block_info
fastuidraw::gl::Program::
uniform_block(unsigned int I)
{
  ProgramPrivate *d;
  d = reinterpret_cast<ProgramPrivate*>(m_d);
  d->assemble(this);
  return uniform_block_info(d->m_uniform_block_list.uniform_block(I));
}

unsigned int
fastuidraw::gl::Program::
uniform_block_id(const char *uniform_block_name)
{
  ProgramPrivate *d;
  d = reinterpret_cast<ProgramPrivate*>(m_d);
  d->assemble(this);
  return d->m_uniform_block_list.uniform_block_id(uniform_block_name);
}

unsigned int
fastuidraw::gl::Program::
number_active_atomic_buffers(void)
{
  ProgramPrivate *d;
  d = reinterpret_cast<ProgramPrivate*>(m_d);
  d->assemble(this);
  return d->m_uniform_block_list.number_atomic_buffers();
}

fastuidraw::gl::Program::atomic_buffer_info
fastuidraw::gl::Program::
atomic_buffer(unsigned int I)
{
  ProgramPrivate *d;
  d = reinterpret_cast<ProgramPrivate*>(m_d);
  d->assemble(this);
  return atomic_buffer_info(d->m_uniform_block_list.atomic_buffer(I));
}

unsigned int
fastuidraw::gl::Program::
number_active_attributes(void)
{
  ProgramPrivate *d;
  d = reinterpret_cast<ProgramPrivate*>(m_d);
  d->assemble(this);
  return d->m_attribute_list.values().size();
}

fastuidraw::gl::Program::parameter_info
fastuidraw::gl::Program::
active_attribute(unsigned int I)
{
  ProgramPrivate *d;
  d = reinterpret_cast<ProgramPrivate*>(m_d);
  d->assemble(this);
  return parameter_info(&d->m_attribute_list.value(I));
}

unsigned int
fastuidraw::gl::Program::
active_attribute_id(const char *pname)
{
  FindParameterResult R;
  ProgramPrivate *d;

  d = reinterpret_cast<ProgramPrivate*>(m_d);
  d->assemble(this);
  R = d->m_attribute_list.find_parameter(pname);
  return R.m_idx;
}

GLint
fastuidraw::gl::Program::
attribute_location(const char *pname)
{
  FindParameterResult R;
  ProgramPrivate *d;

  d = reinterpret_cast<ProgramPrivate*>(m_d);
  d->assemble(this);
  R = d->m_attribute_list.find_parameter(pname);
  return R.m_location;
}


unsigned int
fastuidraw::gl::Program::
num_shaders(GLenum tp) const
{
  ProgramPrivate *d;
  d = reinterpret_cast<ProgramPrivate*>(m_d);

  std::map<GLenum, std::vector<int> >::const_iterator iter;
  iter = d->m_shader_data_sorted_by_type.find(tp);
  return (iter != d->m_shader_data_sorted_by_type.end()) ?
    iter->second.size() : 0;
}

const char*
fastuidraw::gl::Program::
shader_src_code(GLenum tp, unsigned int i) const
{
  ProgramPrivate *d;
  d = reinterpret_cast<ProgramPrivate*>(m_d);

  std::map<GLenum, std::vector<int> >::const_iterator iter;
  iter = d->m_shader_data_sorted_by_type.find(tp);
  if(iter != d->m_shader_data_sorted_by_type.end() && i < iter->second.size())
    {
      return d->m_shader_data[iter->second[i]].m_source_code.c_str();
    }
  else
    {
      return "";
    }
}


//////////////////////////////////////
// fastuidraw::gl::ProgramInitializerArray methods
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
add(reference_counted_ptr<ProgramInitializer> h)
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
  for(std::vector<reference_counted_ptr<ProgramInitializer> >::const_iterator
        iter = d->m_values.begin(), end = d->m_values.end();
      iter != end; ++iter)
    {
      const reference_counted_ptr<ProgramInitializer> &v(*iter);
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
  if(loc == -1)
    {
      loc = glGetUniformLocation(pr->name(), d->c_str());
      if(loc != -1)
        {
          std::cerr << "gl_program::uniform_location failed to find uniform, "
                    << "but glGetUniformLocation succeeded\n";
        }
    }

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
