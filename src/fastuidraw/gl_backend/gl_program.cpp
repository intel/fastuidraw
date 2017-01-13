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
#include <fastuidraw/gl_backend/gl_context_properties.hpp>
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

  std::string
  get_program_resource_name(GLuint program,
                            GLenum program_interface,
                            GLint index)
  {
    GLint name_length(0);
    GLenum prop_name_length(GL_NAME_LENGTH);

    glGetProgramResourceiv(program, program_interface, index,
                           1, &prop_name_length,
                           1, NULL, &name_length);
    if(name_length <= 0)
      {
        return std::string();
      }

    std::vector<GLchar> dst_name(name_length, '\0');
    glGetProgramResourceName(program, program_interface, index,
                             dst_name.size(), NULL, &dst_name[0]);

    return std::string(&dst_name[0]);
  }

  class ShaderVariableInterfaceQueryList;

  /* class to hold information about a single
     GLSL variable.
   */
  class ShaderVariableInfo
  {
  public:
    ShaderVariableInfo(void):
      m_type(GL_INVALID_ENUM),
      m_count(-1),
      m_index(-1),
      m_location(-1),
      m_ubo_index(-1),
      m_offset(-1),
      m_array_stride(-1),
      m_matrix_stride(-1),
      m_is_row_major(0),
      m_abo_index(-1),
      m_shader_storage_buffer_index(-1),
      m_shader_storage_buffer_top_level_array_size(-1),
      m_shader_storage_buffer_top_level_array_stride(-1)
    {
    }

    bool
    operator<(const ShaderVariableInfo &rhs) const
    {
      return m_name < rhs.m_name;
    }

    std::string m_name;
    GLint m_type;
    GLint m_count;
    GLuint m_index;
    GLint m_location;

    GLint m_ubo_index;
    GLint m_offset;
    GLint m_array_stride;
    GLint m_matrix_stride;
    GLint m_is_row_major;
    GLint m_abo_index;

    GLint m_shader_storage_buffer_index;
    GLint m_shader_storage_buffer_top_level_array_size;
    GLint m_shader_storage_buffer_top_level_array_stride;
  };

  /* class to use program interface query to fill
     the fields of a ShaderVariableInfo
   */
  class ShaderVariableInterfaceQueryList
  {
  public:
    typedef GLint ShaderVariableInfo::*dst_type;

    ShaderVariableInterfaceQueryList&
    add(GLenum e, dst_type d)
    {
      m_enums.push_back(e);
      m_dsts.push_back(d);
      return *this;
    }

    /* fill the fields of a ShaderVariableInfo by using GL program interface query
        - program: what GLSL program
        - program_inteface: what GLSL interface from which to take the
          variable information (for example GL_UNIFORM, GL_BUFFER_VARIABLE,
          GL_PROGRAM_INPUT, GL_PROGRAM_OUTPUT)
        - interface_index: GL index of variable from the interface
        - queries: what GL enums and where to write to in ShaderVariableInfo
     */
    void
    fill_variable(GLuint program,
                  GLenum variable_interface,
                  GLuint variable_interface_index,
                  ShaderVariableInfo &dst) const;

  private:
    mutable std::vector<GLint> m_work_room;
    std::vector<GLenum> m_enums;
    std::vector<dst_type> m_dsts;
  };

  /* class to fill fields associated to uniforms of a ShaderVariableInfo
     using glGetActiveUniformsiv (thus NOT using the program interface
     query interface)
   */
  class ShaderUniformQueryList
  {
  public:
    typedef GLint ShaderVariableInfo::*dst_type;

    void
    fill_variables(GLuint program,
                   std::vector<ShaderVariableInfo> &dst) const;

    ShaderUniformQueryList&
    add(GLenum e, dst_type d)
    {
      m_enums.push_back(e);
      m_dsts.push_back(d);
      return *this;
    }

  private:
    std::vector<GLenum> m_enums;
    std::vector<dst_type> m_dsts;
  };

  class FindShaderVariableResult
  {
  public:
    FindShaderVariableResult(void):
      m_ptr(NULL),
      m_v_index(~0u),
      m_array_element(0)
    {}

    FindShaderVariableResult(const ShaderVariableInfo *p, unsigned int vindex, int ele = 0):
      m_ptr(p),
      m_v_index(vindex),
      m_array_element(ele)
    {}

    int
    gl_location(void) const
    {
      return m_ptr != NULL && m_ptr->m_location != -1 ?
        m_ptr->m_location + m_array_element :
        -1;
    }

    int
    gl_offset(void) const
    {
      return m_ptr != NULL && m_ptr->m_offset != -1 ?
        m_ptr->m_offset + m_array_element * m_ptr->m_array_stride :
        -1;
    }

    unsigned int
    set_id(void) const
    {
      return m_ptr != NULL ?
        m_v_index :
        ~0u;
    }

    const ShaderVariableInfo *m_ptr;
    int m_v_index;
    int m_array_element;
  };

  class ShaderVariableSet
  {
  public:
    ShaderVariableSet(void):
      m_finalized(false)
    {}

    unsigned int
    add_element(const ShaderVariableInfo &value)
    {
      unsigned int return_value(m_values.size());
      assert(!m_finalized);
      m_values.push_back(value);
      return return_value;
    }

    void
    finalize(void);

    const std::vector<ShaderVariableInfo>&
    values(void) const
    {
      return m_values;
    }

    const ShaderVariableInfo&
    value(unsigned int I) const
    {
      return I < m_values.size() ?
        m_values[I] :
        m_empty;
    }

    FindShaderVariableResult
    find_variable(const std::string &pname) const;

    /* Poplulate using the old GL interface for
       getting information about uniforms, uniform blocks
       and attributes.
     */
    template<typename F, typename G>
    void
    populate_non_program_interface_query(GLuint program,
                                         GLenum count_enum, GLenum length_enum,
                                         F fptr, G gptr,
                                         const ShaderUniformQueryList &query_list = ShaderUniformQueryList());

    /* Poplulate from a named interface block
       - program interface: what GLSL interface from which to take the
         variable information (for example GL_UNIFORM_BLOCK,
         GL_ATOMIC_COUNTER_BUFFER, GL_TRANSFORM_FEEDBACK_BUFFER or
         GL_SHADER_STORAGE_BLOCK
       - interface_index: the name of the interface block
       - variable_interface: the interface to access the contents of
                             the named block
     */
    void
    populate_from_interface_block(GLuint program,
                                  GLenum program_interface,
                                  GLuint interface_index,
                                  GLenum variable_interface,
                                  const ShaderVariableInterfaceQueryList &queries);

    /* Poplulate from a named interface block
        - program interface: one of GL_PROGRAM_INPUT, GL_PROGRAM_OUPUT, GL_UNIFORM
     */
    void
    populate_from_resource(GLuint program,
                           GLenum resource_interface,
                           const ShaderVariableInterfaceQueryList &queries);

  private:
    template<typename iterator>
    static
    std::string
    filter_name(iterator begin, iterator end, int &array_index);

    bool m_finalized;
    ShaderVariableInfo m_empty;
    std::vector<ShaderVariableInfo> m_values;
    std::map<std::string, unsigned int> m_map;
  };

  /* class to hold attribute information of a GLSL
     program
   */
  class AttributeInfo:public ShaderVariableSet
  {
  public:
    void
    populate(GLuint program, const fastuidraw::gl::ContextProperties &ctx);
  };

  class AtomicBufferInfo
  {
  public:
    AtomicBufferInfo(void):
      m_buffer_binding(-1),
      m_buffer_index(-1),
      m_size_bytes(0)
    {}

    GLint m_buffer_binding;
    GLint m_buffer_index;
    GLint m_size_bytes;
    ShaderVariableSet m_members;
  };

  class BlockInfoPrivate
  {
  public:
    BlockInfoPrivate(void):
      m_block_index(-1),
      m_size_bytes(0)
    {}

    /* populate the block information using
       program interface query
     */
    void
    populate(GLuint program, GLenum program_interface,
             GLint resource_index,
             GLenum variable_resource,
             const ShaderVariableInterfaceQueryList &queries);

    std::string m_name;
    GLint m_block_index;
    GLint m_size_bytes;
    ShaderVariableSet m_members;
  };

  /* class to hold information for multiple
     (indexed) blocks
   */
  class BlockSetInfoBase
  {
  public:
    BlockSetInfoBase(void):
      m_finalized(false)
    {}

    unsigned int
    number_active_blocks(void) const
    {
      assert(m_finalized);
      return m_blocks_sorted.size();
    }

    const BlockInfoPrivate*
    block(unsigned int I)
    {
      assert(m_finalized);
      return (I < m_blocks_sorted.size()) ?
        m_blocks_sorted[I] :
        NULL;
    }

    unsigned int
    block_id(const std::string &name) const
    {
      std::map<std::string, unsigned int>::const_iterator iter;

      assert(m_finalized);
      iter = m_map.find(name);
      return (iter != m_map.end()) ?
        iter->second:
        ~0u;
    }

    void
    populate_from_resource(GLuint program,
                           GLenum resource_interface,
                           GLenum variable_interface,
                           const ShaderVariableInterfaceQueryList &queries);

    void
    populate_as_empty(void)
    {
      assert(!m_finalized);
      assert(m_map.empty());
      assert(m_blocks.empty());
      assert(m_blocks_sorted.empty());
      m_finalized = true;
    }

  protected:
    void
    finalize(void);

    void
    resize_number_blocks(unsigned int N);

    BlockInfoPrivate&
    block_ref(unsigned int I)
    {
      assert(!m_finalized);
      assert(I < m_blocks.size());
      return m_blocks[I];
    }

  private:
    static
    bool
    compare_function(const BlockInfoPrivate *lhs,
                     const BlockInfoPrivate *rhs)
    {
      assert(lhs != NULL);
      assert(rhs != NULL);
      return lhs->m_name < rhs->m_name;
    }

    bool m_finalized;
    std::vector<BlockInfoPrivate> m_blocks;
    std::vector<BlockInfoPrivate*> m_blocks_sorted;
    std::map<std::string, unsigned int> m_map; //gives indice into m_blocks_sorted
  };

  /* class to information for each uniform block,
     including the default uniform block of a GLSL
     program
   */
  class UniformBlockSetInfo:public BlockSetInfoBase
  {
  public:
    void
    populate(GLuint program,
             const fastuidraw::gl::ContextProperties &ctx_props);

    const BlockInfoPrivate*
    default_uniform_block(void) const
    {
      return &m_default_block;
    }

    const ShaderVariableSet&
    all_uniforms(void) const
    {
      return m_all_uniforms;
    }

    unsigned int
    number_atomic_buffers(void) const
    {
      return m_abo_buffers.size();
    }

    const AtomicBufferInfo*
    atomic_buffer(unsigned int I) const
    {
      return (I < m_abo_buffers.size()) ?
        &m_abo_buffers[I] :
        NULL;
    }

  private:
    void
    populate_private_non_program_interface_query(GLuint program,
                                                 const fastuidraw::gl::ContextProperties &ctx_props);

    void
    populate_private_program_interface_query(GLuint program,
                                             const fastuidraw::gl::ContextProperties &ctx_props);

    ShaderVariableSet m_all_uniforms;
    BlockInfoPrivate m_default_block;
    std::vector<AtomicBufferInfo> m_abo_buffers;
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
    typedef fastuidraw::gl::Shader Shader;
    typedef fastuidraw::reference_counted_ptr<Shader> ShaderRef;

    ProgramPrivate(const fastuidraw::const_c_array<ShaderRef> pshaders,
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
    AttributeInfo m_attribute_list;
    UniformBlockSetInfo m_uniform_list;
    BlockSetInfoBase m_storage_buffer_list;
    std::vector<AtomicBufferInfo> m_abo_list;
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

//////////////////////////////////////
// ShaderVariableInterfaceQueryList methods
void
ShaderVariableInterfaceQueryList::
fill_variable(GLuint program,
              GLenum variable_interface,
              GLuint variable_interface_index,
              ShaderVariableInfo &dst) const
{
  dst.m_index = variable_interface_index;
  dst.m_name = get_program_resource_name(program, variable_interface, dst.m_index);

  if(!m_enums.empty())
    {
      m_work_room.resize(m_enums.size());
      for(unsigned int i = 0, endi = m_enums.size(); i < endi; ++i)
        {
          m_work_room[i] = dst.*m_dsts[i];
        }

      glGetProgramResourceiv(program, variable_interface, variable_interface_index,
                             m_enums.size(), &m_enums[0],
                             m_work_room.size(), NULL, &m_work_room[0]);

      for(unsigned int i = 0, endi = m_enums.size(); i < endi; ++i)
        {
          dst.*m_dsts[i] = m_work_room[i];
        }
    }
}

////////////////////////////////////
// ShaderUniformQueryList methods
void
ShaderUniformQueryList::
fill_variables(GLuint program,
               std::vector<ShaderVariableInfo> &dst) const
{
  assert(m_enums.size() == m_dsts.size());
  if(m_enums.empty() || dst.empty())
    {
      return;
    }

  std::vector<GLint> values(dst.size());
  std::vector<GLuint> indxs(dst.size());
  ShaderVariableInfo default_value;

  for(unsigned int v = 0, endv = dst.size(); v < endv; ++v)
    {
      indxs[v] = dst[v].m_index;
    }

  for(unsigned int q = 0, endq = m_enums.size(); q < endq; ++q)
    {
      std::fill(values.begin(), values.end(), default_value.*m_dsts[q]);
      glGetActiveUniformsiv(program, indxs.size(), &indxs[0], m_enums[q], &values[0]);
      for(unsigned int v = 0, endv = indxs.size(); v < endv; ++v)
        {
          dst[v].*m_dsts[q] = values[v];
        }
    }
}

////////////////////////////////////
// ShaderVariableSet methods
void
ShaderVariableSet::
finalize(void)
{
  assert(!m_finalized);
  m_finalized = true;
  std::sort(m_values.begin(), m_values.end());
  for(unsigned int i = 0, endi = m_values.size(); i < endi; ++i)
    {
      m_map[m_values[i].m_name] = i;
    }
}

FindShaderVariableResult
ShaderVariableSet::
find_variable(const std::string &pname) const
{
  std::map<std::string, unsigned int>::const_iterator iter;

  assert(m_finalized);
  iter = m_map.find(pname);
  if(iter != m_map.end())
    {
      const ShaderVariableInfo *q;
      q = &m_values[iter->second];
      return FindShaderVariableResult(q, iter->second);
    }

  std::string filtered_name;
  int array_index;
  filtered_name = filter_name(pname.begin(), pname.end(), array_index);
  iter = m_map.find(filtered_name);
  if(iter != m_map.end())
    {
      const ShaderVariableInfo *q;
      q = &m_values[iter->second];
      if(array_index < q->m_count)
        {
          return FindShaderVariableResult(q, iter->second, array_index);
        }
    }

  return FindShaderVariableResult();
}

template<typename F, typename G>
void
ShaderVariableSet::
populate_non_program_interface_query(GLuint program,
                                     GLenum count_enum, GLenum length_enum,
                                     F fptr, G gptr,
                                     const ShaderUniformQueryList &query_list)
{
  GLint count(0);

  glGetProgramiv(program, count_enum, &count);
  if(count > 0)
    {
      GLint largest_length(0);
      std::vector<char> pname;

      glGetProgramiv(program, length_enum, &largest_length);

      ++largest_length;
      pname.resize(largest_length, '\0');

      for(int i = 0; i != count; ++i)
        {
          ShaderVariableInfo dst;
          GLsizei name_length, psize;
          GLenum ptype;

          std::memset(&pname[0], 0, largest_length);

          fptr(program, i, largest_length,
               &name_length, &psize,
               &ptype, &pname[0]);

          dst.m_type = ptype;
          dst.m_count = psize;
          dst.m_name = std::string(pname.begin(), pname.begin() + name_length);
          dst.m_index = i;
          dst.m_location = gptr(program, &pname[0]);
          add_element(dst);
        }
      query_list.fill_variables(program, m_values);
    }
  finalize();
}

void
ShaderVariableSet::
populate_from_resource(GLuint program,
                       GLenum resource_interface,
                       const ShaderVariableInterfaceQueryList &queries)
{
  GLint num(0);
  glGetProgramInterfaceiv(program, resource_interface, GL_ACTIVE_RESOURCES, &num);
  for(GLint i = 0; i < num; ++i)
    {
      ShaderVariableInfo p;
      queries.fill_variable(program, resource_interface, i, p);
      add_element(p);
    }
  finalize();
}

void
ShaderVariableSet::
populate_from_interface_block(GLuint program,
                              GLenum program_interface,
                              GLuint interface_index,
                              GLenum variable_interface,
                              const ShaderVariableInterfaceQueryList &queries)
{
  const GLenum prop_num_active(GL_NUM_ACTIVE_VARIABLES);
  GLint num_variables(0);

  glGetProgramResourceiv(program, program_interface, interface_index,
                         1, &prop_num_active,
                         1, NULL, &num_variables);
  if(num_variables > 0)
    {
      const GLenum prop_active(GL_ACTIVE_VARIABLES);
      std::vector<GLint> variable_index(num_variables, -1);

      glGetProgramResourceiv(program, program_interface, interface_index,
                             1, &prop_active,
                             num_variables, NULL, &variable_index[0]);
      for(GLint i = 0; i < num_variables; ++i)
        {
          ShaderVariableInfo p;
          queries.fill_variable(program, variable_interface, variable_index[i], p);
          add_element(p);
        }
    }
  finalize();
}

template<typename iterator>
std::string
ShaderVariableSet::
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

/////////////////////////////
// BlockInfoPrivate methods
void
BlockInfoPrivate::
populate(GLuint program, GLenum program_interface,
         GLint resource_index,
         GLenum variable_interface,
         const ShaderVariableInterfaceQueryList &queries)
{
  m_block_index = resource_index;
  m_members.populate_from_interface_block(program, program_interface,
                                          m_block_index, variable_interface,
                                          queries);
  m_name = get_program_resource_name(program, program_interface,
                                     resource_index);

  const GLenum prop_data_size(GL_BUFFER_DATA_SIZE);
  glGetProgramResourceiv(program, program_interface, m_block_index,
                         1, &prop_data_size,
                         1, NULL, &m_size_bytes);
}


///////////////////////////////
// AttributeInfo methods
void
AttributeInfo::
populate(GLuint program, const fastuidraw::gl::ContextProperties &ctx_props)
{
  bool use_program_interface_query;

  if(ctx_props.is_es())
    {
      use_program_interface_query = (ctx_props.version() >= fastuidraw::ivec2(3, 1));
    }
  else
    {
      use_program_interface_query = (ctx_props.version() >= fastuidraw::ivec2(4, 3))
        || ctx_props.has_extension("GL_ARB_program_interface_query");
    }

  if(use_program_interface_query)
    {
      ShaderVariableInterfaceQueryList attribute_queries;

      attribute_queries
        .add(GL_TYPE, &ShaderVariableInfo::m_type)
        .add(GL_ARRAY_SIZE, &ShaderVariableInfo::m_count)
        .add(GL_LOCATION, &ShaderVariableInfo::m_location);
      populate_from_resource(program, GL_PROGRAM_INPUT, attribute_queries);
    }
  else
    {
      populate_non_program_interface_query(program,
                                           GL_ACTIVE_ATTRIBUTES,
                                           GL_ACTIVE_ATTRIBUTE_MAX_LENGTH,
                                           FASTUIDRAWglfunctionPointer(glGetActiveAttrib),
                                           FASTUIDRAWglfunctionPointer(glGetAttribLocation));
    }
}

///////////////////////////////////////
//BlockSetInfoBase methods
void
BlockSetInfoBase::
populate_from_resource(GLuint program,
                       GLenum resource_interface,
                       GLenum variable_interface,
                       const ShaderVariableInterfaceQueryList &queries)
{
  GLint num(0);

  glGetProgramInterfaceiv(program, resource_interface, GL_ACTIVE_RESOURCES, &num);
  resize_number_blocks(num);

  for(GLint i = 0; i < num; ++i)
    {
      block_ref(i).m_members.populate_from_interface_block(program, resource_interface,
                                                           i, variable_interface, queries);
    }
  finalize();
}

void
BlockSetInfoBase::
finalize(void)
{
  assert(!m_finalized);
  m_finalized = true;
  std::sort(m_blocks_sorted.begin(), m_blocks_sorted.end(), compare_function);
  for(unsigned int i = 0, endi = m_blocks_sorted.size(); i < endi; ++i)
    {
      m_blocks_sorted[i]->m_members.finalize();
      m_map[m_blocks_sorted[i]->m_name] = i;
    }
}

void
BlockSetInfoBase::
resize_number_blocks(unsigned int N)
{
  assert(!m_finalized);
  assert(m_blocks.empty());
  assert(m_blocks_sorted.empty());
  m_blocks.resize(N);
  m_blocks_sorted.resize(N);
  for(unsigned int i = 0; i < N; ++i)
    {
      m_blocks_sorted[i] = &m_blocks[i];
    }
}

////////////////////////////////////////////////
//UniformBlockSetInfo methods
void
UniformBlockSetInfo::
populate(GLuint program,
         const fastuidraw::gl::ContextProperties &ctx_props)
{
  bool use_program_interface_query;

  if(ctx_props.is_es())
    {
      use_program_interface_query = (ctx_props.version() >= fastuidraw::ivec2(3, 1));
    }
  else
    {
      use_program_interface_query = (ctx_props.version() >= fastuidraw::ivec2(4, 3))
        || ctx_props.has_extension("GL_ARB_program_interface_query");
    }

  if(use_program_interface_query)
    {
      populate_private_program_interface_query(program, ctx_props);
    }
  else
    {
      populate_private_non_program_interface_query(program, ctx_props);
    }
}

void
UniformBlockSetInfo::
populate_private_program_interface_query(GLuint program,
                                         const fastuidraw::gl::ContextProperties &ctx_props)
{
  /* first populate our list of all uniforms
   */
  ShaderVariableInterfaceQueryList uniform_queries;
  uniform_queries
    .add(GL_TYPE, &ShaderVariableInfo::m_type)
    .add(GL_ARRAY_SIZE, &ShaderVariableInfo::m_count)
    .add(GL_LOCATION, &ShaderVariableInfo::m_location)
    .add(GL_BLOCK_INDEX, &ShaderVariableInfo::m_ubo_index)
    .add(GL_OFFSET, &ShaderVariableInfo::m_offset)
    .add(GL_ARRAY_STRIDE, &ShaderVariableInfo::m_array_stride)
    .add(GL_MATRIX_STRIDE, &ShaderVariableInfo::m_matrix_stride)
    .add(GL_IS_ROW_MAJOR, &ShaderVariableInfo::m_is_row_major)
    .add(GL_ATOMIC_COUNTER_BUFFER_INDEX, &ShaderVariableInfo::m_abo_index);

  m_all_uniforms.populate_from_resource(program, GL_UNIFORM, uniform_queries);

  /* populate the ABO blocks
   */
  GLint abo_count(0);
  glGetProgramInterfaceiv(program, GL_ATOMIC_COUNTER_BUFFER, GL_ACTIVE_RESOURCES, &abo_count);
  m_abo_buffers.resize(abo_count);

  /* extract from m_all_uniforms those uniforms from the default block
   */
  for(std::vector<ShaderVariableInfo>::const_iterator iter = m_all_uniforms.values().begin(),
        end = m_all_uniforms.values().end(); iter != end; ++iter)
    {
      if(iter->m_ubo_index == -1)
        {
          m_default_block.m_members.add_element(*iter);
        }

      if(iter->m_abo_index != -1)
        {
          m_abo_buffers[iter->m_abo_index].m_members.add_element(*iter);
        }
    }

  /* poplulate each of the uniform blocks.
   */
  GLint ubo_count(0);
  glGetProgramInterfaceiv(program, GL_UNIFORM_BLOCK, GL_ACTIVE_RESOURCES, &ubo_count);
  resize_number_blocks(ubo_count);
  for(GLint i = 0; i < ubo_count; ++i)
    {
      block_ref(i).populate(program, GL_UNIFORM_BLOCK, i, GL_UNIFORM, uniform_queries);
    }

  for(GLint i = 0; i < abo_count; ++i)
    {
      const GLenum prop_data_size(GL_BUFFER_DATA_SIZE);

      m_abo_buffers[i].m_buffer_index = i;
      glGetProgramResourceiv(program, GL_ATOMIC_COUNTER_BUFFER, i,
                             1, &prop_data_size,
                             1, NULL, &m_abo_buffers[i].m_size_bytes);
      m_abo_buffers[i].m_members.finalize();
    }

  /* finalize default uniform block
   */
  m_default_block.m_members.finalize();

  /* finalize non-default uniform blocks
   */
  finalize();

  FASTUIDRAWunused(ctx_props);
}

void
UniformBlockSetInfo::
populate_private_non_program_interface_query(GLuint program,
                                             const fastuidraw::gl::ContextProperties &ctx_props)
{
  /* first populate our list of all uniforms
   */
  ShaderUniformQueryList query_list;

  query_list
    .add(GL_UNIFORM_BLOCK_INDEX, &ShaderVariableInfo::m_ubo_index)
    .add(GL_UNIFORM_OFFSET, &ShaderVariableInfo::m_offset)
    .add(GL_UNIFORM_ARRAY_STRIDE, &ShaderVariableInfo::m_array_stride)
    .add(GL_UNIFORM_MATRIX_STRIDE, &ShaderVariableInfo::m_matrix_stride)
    .add(GL_UNIFORM_IS_ROW_MAJOR, &ShaderVariableInfo::m_is_row_major);

  #ifndef FASTUIDRAW_GL_USE_GLES
    {
      bool abo_supported;
      GLint abo_count(0);

      abo_supported = ctx_props.version() >= fastuidraw::ivec2(4, 2)
        || ctx_props.has_extension("GL_ARB_shader_atomic_counters");

      if(abo_supported)
        {
          query_list.add(GL_ATOMIC_COUNTER_BUFFER_INDEX, &ShaderVariableInfo::m_abo_index);
          glGetProgramiv(program, GL_ACTIVE_ATOMIC_COUNTER_BUFFERS, &abo_count);

          m_abo_buffers.resize(abo_count);
          for(int i = 0; i != abo_count; ++i)
            {
              m_abo_buffers[i].m_buffer_index = i;
              glGetActiveAtomicCounterBufferiv(program, i, GL_ATOMIC_COUNTER_BUFFER_DATA_SIZE,
                                               &m_abo_buffers[i].m_size_bytes);
              glGetActiveAtomicCounterBufferiv(program, i, GL_ATOMIC_COUNTER_BUFFER_BINDING,
                                               &m_abo_buffers[i].m_buffer_binding);
            }
        }
    }
  #endif

  m_all_uniforms.populate_non_program_interface_query(program,
                                                      GL_ACTIVE_UNIFORMS,
                                                      GL_ACTIVE_UNIFORM_MAX_LENGTH,
                                                      FASTUIDRAWglfunctionPointer(glGetActiveUniform),
                                                      FASTUIDRAWglfunctionPointer(glGetUniformLocation),
                                                      query_list);

  /* get the UBO block information: name and size
   */
  GLint ubo_count(0);
  glGetProgramiv(program, GL_ACTIVE_UNIFORM_BLOCKS, &ubo_count);
  resize_number_blocks(ubo_count);
  if(ubo_count > 0)
    {
      GLint largest_length(0);
      std::vector<char> pname;

      glGetProgramiv(program, GL_ACTIVE_UNIFORM_BLOCK_MAX_NAME_LENGTH, &largest_length);
      ++largest_length;
      pname.resize(largest_length, '\0');

      for(int i = 0; i < ubo_count; ++i)
        {
          GLsizei name_length(0), psize(0);
          std::memset(&pname[0], 0, largest_length);

          glGetActiveUniformBlockName(program, i, largest_length, &name_length, &pname[0]);
          glGetActiveUniformBlockiv(program, i, GL_UNIFORM_BLOCK_DATA_SIZE, &psize);

          block_ref(i).m_block_index = i;
          block_ref(i).m_name = std::string(pname.begin(), pname.begin() + name_length);
          block_ref(i).m_size_bytes = psize;
        }
    }

  /* extract uniform data from all_uniforms, note that
     m_blocks[i] holds the uniform block with block_index
     of value i currently.
  */
  for(std::vector<ShaderVariableInfo>::const_iterator iter = m_all_uniforms.values().begin(),
        end = m_all_uniforms.values().end(); iter != end; ++iter)
    {
      if(iter->m_ubo_index != -1)
        {
          block_ref(iter->m_ubo_index).m_members.add_element(*iter);
        }
      else
        {
          m_default_block.m_members.add_element(*iter);
        }

      if(iter->m_abo_index != -1)
        {
          assert(iter->m_abo_index < static_cast<int>(m_abo_buffers.size()));
          m_abo_buffers[iter->m_abo_index].m_members.add_element(*iter);
        }
    }

  /* finalize each atomic buffer block
   */
  for(unsigned int i = 0, endi = m_abo_buffers.size(); i < endi; ++i)
    {
      m_abo_buffers[i].m_members.finalize();
    }

  /* finalize default uniform block
   */
  m_default_block.m_members.finalize();

  /* finalize non-default uniform blocks
   */
  finalize();

  FASTUIDRAWunused(ctx_props);
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

////////////////////////////////////
// ProgramSeparable methods
void
fastuidraw::gl::ProgramSeparable::
action(GLuint glsl_program) const
{
  glProgramParameteri(glsl_program, GL_PROGRAM_SEPARABLE, GL_TRUE);
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
// fastuidraw::gl::Program::shader_variable_info methods
fastuidraw::gl::Program::shader_variable_info::
shader_variable_info(const void *d):
  m_d(d)
{}

fastuidraw::gl::Program::shader_variable_info::
shader_variable_info(void):
  m_d(NULL)
{}

const char*
fastuidraw::gl::Program::shader_variable_info::
name(void) const
{
  const ShaderVariableInfo *d;
  d = reinterpret_cast<const ShaderVariableInfo*>(m_d);
  return (d) ? d->m_name.c_str() : "";
}

GLenum
fastuidraw::gl::Program::shader_variable_info::
type(void) const
{
  const ShaderVariableInfo *d;
  d = reinterpret_cast<const ShaderVariableInfo*>(m_d);
  return (d) ? d->m_type : GL_INVALID_ENUM;
}

GLint
fastuidraw::gl::Program::shader_variable_info::
count(void) const
{
  const ShaderVariableInfo *d;
  d = reinterpret_cast<const ShaderVariableInfo*>(m_d);
  return (d) ? d->m_count : -1;
}

GLuint
fastuidraw::gl::Program::shader_variable_info::
index(void) const
{
  const ShaderVariableInfo *d;
  d = reinterpret_cast<const ShaderVariableInfo*>(m_d);
  return (d) ? d->m_index : -1;
}

GLint
fastuidraw::gl::Program::shader_variable_info::
location(void) const
{
  const ShaderVariableInfo *d;
  d = reinterpret_cast<const ShaderVariableInfo*>(m_d);
  return (d) ? d->m_location : -1;
}

GLint
fastuidraw::gl::Program::shader_variable_info::
ubo_index(void) const
{
  const ShaderVariableInfo *d;
  d = reinterpret_cast<const ShaderVariableInfo*>(m_d);
  return (d) ? d->m_ubo_index : -1;
}

GLint
fastuidraw::gl::Program::shader_variable_info::
buffer_offset(void) const
{
  const ShaderVariableInfo *d;
  d = reinterpret_cast<const ShaderVariableInfo*>(m_d);
  return (d) ? d->m_offset : -1;
}

GLint
fastuidraw::gl::Program::shader_variable_info::
array_stride(void) const
{
  const ShaderVariableInfo *d;
  d = reinterpret_cast<const ShaderVariableInfo*>(m_d);
  return (d) ? d->m_array_stride : -1;
}

GLint
fastuidraw::gl::Program::shader_variable_info::
matrix_stride(void) const
{
  const ShaderVariableInfo *d;
  d = reinterpret_cast<const ShaderVariableInfo*>(m_d);
  return (d) ? d->m_matrix_stride : -1;
}

bool
fastuidraw::gl::Program::shader_variable_info::
is_row_major(void) const
{
  const ShaderVariableInfo *d;
  d = reinterpret_cast<const ShaderVariableInfo*>(m_d);
  return (d) ? d->m_is_row_major : false;
}

GLint
fastuidraw::gl::Program::shader_variable_info::
abo_index(void) const
{
  const ShaderVariableInfo *d;
  d = reinterpret_cast<const ShaderVariableInfo*>(m_d);
  return (d) ? d->m_abo_index : -1;
}

GLint
fastuidraw::gl::Program::shader_variable_info::
shader_storage_buffer_index(void) const
{
  const ShaderVariableInfo *d;
  d = reinterpret_cast<const ShaderVariableInfo*>(m_d);
  return (d) ? d->m_shader_storage_buffer_index : -1;
}

GLint
fastuidraw::gl::Program::shader_variable_info::
shader_storage_buffer_top_level_array_size(void) const
{
  const ShaderVariableInfo *d;
  d = reinterpret_cast<const ShaderVariableInfo*>(m_d);
  return (d) ? d->m_shader_storage_buffer_top_level_array_size : -1;
}

GLint
fastuidraw::gl::Program::shader_variable_info::
shader_storage_buffer_top_level_array_stride(void) const
{
  const ShaderVariableInfo *d;
  d = reinterpret_cast<const ShaderVariableInfo*>(m_d);
  return (d) ? d->m_shader_storage_buffer_top_level_array_stride : -1;
}

///////////////////////////////////////////////////
// fastuidraw::gl::Program::block_info methods
fastuidraw::gl::Program::block_info::
block_info(const void *d):
  m_d(d)
{}

fastuidraw::gl::Program::block_info::
block_info(void):
  m_d(NULL)
{}

const char*
fastuidraw::gl::Program::block_info::
name(void) const
{
  const BlockInfoPrivate *d;
  d = reinterpret_cast<const BlockInfoPrivate*>(m_d);
  return (d) ? d->m_name.c_str() : "";
}

GLint
fastuidraw::gl::Program::block_info::
block_index(void) const
{
  const BlockInfoPrivate *d;
  d = reinterpret_cast<const BlockInfoPrivate*>(m_d);
  return (d) ? d->m_block_index : -1;
}

GLint
fastuidraw::gl::Program::block_info::
buffer_size(void) const
{
  const BlockInfoPrivate *d;
  d = reinterpret_cast<const BlockInfoPrivate*>(m_d);
  return (d) ? d->m_size_bytes : 0;
}

unsigned int
fastuidraw::gl::Program::block_info::
number_variables(void) const
{
  const BlockInfoPrivate *d;
  d = reinterpret_cast<const BlockInfoPrivate*>(m_d);
  return d ? d->m_members.values().size() : 0;
}

fastuidraw::gl::Program::shader_variable_info
fastuidraw::gl::Program::block_info::
variable(unsigned int I)
{
  const BlockInfoPrivate *d;
  const void *q;

  d = reinterpret_cast<const BlockInfoPrivate*>(m_d);
  q = d ? &d->m_members.value(I) : NULL;
  return shader_variable_info(q);
}

unsigned int
fastuidraw::gl::Program::block_info::
variable_id(const char *name)
{
  const BlockInfoPrivate *d;

  d = reinterpret_cast<const BlockInfoPrivate*>(m_d);
  return d ? d->m_members.find_variable(name).set_id() : ~0u;
}

int
fastuidraw::gl::Program::block_info::
variable_offset(const char *name)
{
  const BlockInfoPrivate *d;

  d = reinterpret_cast<const BlockInfoPrivate*>(m_d);
  return d ? d->m_members.find_variable(name).gl_offset() : ~0u;
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
  const AtomicBufferInfo *d;
  d = reinterpret_cast<const AtomicBufferInfo*>(m_d);
  return (d) ? d->m_buffer_index : -1;
}

GLint
fastuidraw::gl::Program::atomic_buffer_info::
buffer_binding(void) const
{
  const AtomicBufferInfo *d;
  d = reinterpret_cast<const AtomicBufferInfo*>(m_d);
  return (d) ? d->m_buffer_binding : -1;
}

GLint
fastuidraw::gl::Program::atomic_buffer_info::
buffer_size(void) const
{
  const AtomicBufferInfo *d;
  d = reinterpret_cast<const AtomicBufferInfo*>(m_d);
  return (d) ? d->m_size_bytes : 0;
}

unsigned int
fastuidraw::gl::Program::atomic_buffer_info::
number_atomic_variables(void) const
{
  const AtomicBufferInfo *d;
  d = reinterpret_cast<const AtomicBufferInfo*>(m_d);
  return d ? d->m_members.values().size() : 0;
}

fastuidraw::gl::Program::shader_variable_info
fastuidraw::gl::Program::atomic_buffer_info::
atomic_variable(unsigned int I)
{
  const AtomicBufferInfo *d;
  const void *q;

  d = reinterpret_cast<const AtomicBufferInfo*>(m_d);
  q = d ? &d->m_members.value(I) : NULL;
  return shader_variable_info(q);
}

unsigned int
fastuidraw::gl::Program::atomic_buffer_info::
atomic_variable_id(const char *name)
{
  const AtomicBufferInfo *d;
  d = reinterpret_cast<const AtomicBufferInfo*>(m_d);
  if(d != NULL)
    {
      return d->m_members.find_variable(name).set_id();
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
      fastuidraw::gl::ContextProperties ctx_props;
      bool ssbo_supported;

      m_uniform_list.populate(m_name, ctx_props);
      m_attribute_list.populate(m_name, ctx_props);

      if(ctx_props.is_es())
        {
          ssbo_supported = ctx_props.version() >= fastuidraw::ivec2(3, 1);
        }
      else
        {
          ssbo_supported = ctx_props.version() >= fastuidraw::ivec2(4, 3)
            || ctx_props.has_extension("GL_ARB_shader_storage_buffer_object");
        }

      if(ssbo_supported)
        {
          ShaderVariableInterfaceQueryList ssbo_query;

          ssbo_query
            .add(GL_TYPE, &ShaderVariableInfo::m_type)
            .add(GL_ARRAY_SIZE, &ShaderVariableInfo::m_count)
            .add(GL_OFFSET, &ShaderVariableInfo::m_offset)
            .add(GL_ARRAY_STRIDE, &ShaderVariableInfo::m_array_stride)
            .add(GL_MATRIX_STRIDE, &ShaderVariableInfo::m_matrix_stride)
            .add(GL_IS_ROW_MAJOR, &ShaderVariableInfo::m_is_row_major)
            .add(GL_BLOCK_INDEX, &ShaderVariableInfo::m_shader_storage_buffer_index)
            .add(GL_TOP_LEVEL_ARRAY_SIZE, &ShaderVariableInfo::m_shader_storage_buffer_top_level_array_size)
            .add(GL_TOP_LEVEL_ARRAY_STRIDE, &ShaderVariableInfo::m_shader_storage_buffer_top_level_array_stride);

          m_storage_buffer_list.populate_from_resource(m_name, GL_SHADER_STORAGE_BLOCK,
                                                       GL_BUFFER_VARIABLE, ssbo_query);
        }
      else
        {
          m_storage_buffer_list.populate_as_empty();
        }

      /* TODO: if the GLSL program is seperable, it might be *BAD*
         to call glUseProgram if it does not have a vertex shader
         or if it does not have a fragment shader. Would be better
         if we could assume that each of the initializers did not
         need the program to be current; This can be done via the
         glProgramUniform calls, but glProgramUniform is available
         in GL 4.1 or higher of via GL_ARB_seperate_shader_objects
         and for ES, GLES3.1 is required.
       */
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
      ostr << "\n\nGLSL name = " << iter->m_name
           << ", type = " << fastuidraw::gl::Shader::gl_shader_type_label(iter->m_shader_type)
           << "\nSource:\n" << iter->m_source_code
           << "\nCompileLog:\n" << iter->m_compile_log;
    }

  ostr << "\nLink Log:\n" << m_link_log << "\n";

  if(m_link_success)
    {
      ostr << "\n\nUniforms of Default Block:";
      for(std::vector<ShaderVariableInfo>::const_iterator
            iter = m_uniform_list.default_uniform_block()->m_members.values().begin(),
            end = m_uniform_list.default_uniform_block()->m_members.values().end();
          iter != end; ++iter)
        {
          assert(iter->m_ubo_index == -1);
          ostr << "\n\t" << iter->m_name
               << "\n\t\ttype = 0x"
               << std::hex << iter->m_type
               << "\n\t\tcount = " << std::dec << iter->m_count
               << "\n\t\tindex = " << std::dec << iter->m_index
               << "\n\t\tlocation = " << iter->m_location
               << "\n\t\tabo_index = " << iter->m_abo_index;
        }

      for(unsigned int endi = m_uniform_list.number_active_blocks(),
            i = 0; i < endi; ++i)
        {
          const BlockInfoPrivate *ubo(m_uniform_list.block(i));
          ostr << "\n\nUniformBlock:" << ubo->m_name
               << "\n\tblock_index = " << ubo->m_block_index
               << "\n\tblock_size = " << ubo->m_size_bytes
               << "\n\tmembers:";

          for(std::vector<ShaderVariableInfo>::const_iterator
                iter = ubo->m_members.values().begin(),
                end = ubo->m_members.values().end();
              iter != end; ++iter)
            {
              assert(iter->m_ubo_index == ubo->m_block_index);
              ostr << "\n\t\t" << iter->m_name
                   << "\n\t\t\ttype = 0x"
                   << std::hex << iter->m_type
                   << "\n\t\t\tcount = " << std::dec << iter->m_count
                   << "\n\t\t\tindex = " << std::dec << iter->m_index
                   << "\n\t\t\tubo_index = " << iter->m_ubo_index
                   << "\n\t\t\toffset = " << iter->m_offset
                   << "\n\t\t\tarray_stride = " << iter->m_array_stride
                   << "\n\t\t\tmatrix_stride = " << iter->m_matrix_stride
                   << "\n\t\t\tis_row_major = " << bool(iter->m_is_row_major);
            }
        }

      for(unsigned int endi = m_storage_buffer_list.number_active_blocks(),
            i = 0; i < endi; ++i)
        {
          const BlockInfoPrivate *ssbo(m_storage_buffer_list.block(i));
          ostr << "\n\nUniformBlock:" << ssbo->m_name
               << "\n\tblock_index = " << ssbo->m_block_index
               << "\n\tblock_size = " << ssbo->m_size_bytes
               << "\n\tmembers:";

          for(std::vector<ShaderVariableInfo>::const_iterator
                iter = ssbo->m_members.values().begin(),
                end = ssbo->m_members.values().end();
              iter != end; ++iter)
            {
              assert(iter->m_shader_storage_buffer_index == ssbo->m_block_index);
              ostr << "\n\t\t" << iter->m_name
                   << "\n\t\t\ttype = 0x"
                   << std::hex << iter->m_type
                   << "\n\t\t\tcount = " << std::dec << iter->m_count
                   << "\n\t\t\tindex = " << std::dec << iter->m_index
                   << "\n\t\t\tshader_storage_buffer_index = " << iter->m_shader_storage_buffer_index
                   << "\n\t\t\toffset = " << iter->m_offset
                   << "\n\t\t\tarray_stride = " << iter->m_array_stride
                   << "\n\t\t\tmatrix_stride = " << iter->m_matrix_stride
                   << "\n\t\t\tis_row_major = " << bool(iter->m_is_row_major)
                   << "\n\t\t\tshader_storage_buffer_top_level_array_size = "
                   << iter->m_shader_storage_buffer_top_level_array_size
                   << "\n\t\t\tshader_storage_buffer_top_level_array_stride = "
                   << iter->m_shader_storage_buffer_top_level_array_stride;
            }
        }

      ostr << "\n\nAttributes:";
      for(std::vector<ShaderVariableInfo>::const_iterator
            iter = m_attribute_list.values().begin(),
            end = m_attribute_list.values().end();
          iter != end; ++iter)
        {
          ostr << "\n\t"
               << iter->m_name
               << "\n\t\ttype = 0x"
               << std::hex << iter->m_type
               << "\n\t\tcount = " << std::dec << iter->m_count
               << "\n\t\tindex = " << std::dec << iter->m_index
               << "\n\t\tlocation = " << iter->m_location;
        }
      ostr << "\n";
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
  return d->m_link_success ?
    d->m_uniform_list.all_uniforms().values().size() :
    0;
}

fastuidraw::gl::Program::shader_variable_info
fastuidraw::gl::Program::
active_uniform(unsigned int I)
{
  ProgramPrivate *d;
  d = reinterpret_cast<ProgramPrivate*>(m_d);
  d->assemble(this);
  return d->m_link_success ?
    shader_variable_info(&d->m_uniform_list.all_uniforms().value(I)) :
    shader_variable_info(NULL);
}

unsigned int
fastuidraw::gl::Program::
active_uniform_id(const char *pname)
{
  ProgramPrivate *d;

  d = reinterpret_cast<ProgramPrivate*>(m_d);
  d->assemble(this);
  return d->m_link_success ?
    d->m_uniform_list.all_uniforms().find_variable(pname).set_id() :
    ~0u;
}

GLint
fastuidraw::gl::Program::
uniform_location(const char *pname)
{
  ProgramPrivate *d;

  d = reinterpret_cast<ProgramPrivate*>(m_d);
  d->assemble(this);
  return d->m_link_success ?
    d->m_uniform_list.all_uniforms().find_variable(pname).gl_location() :
    -1;
}

fastuidraw::gl::Program::block_info
fastuidraw::gl::Program::
default_uniform_block(void)
{
  ProgramPrivate *d;
  d = reinterpret_cast<ProgramPrivate*>(m_d);
  d->assemble(this);
  return d->m_link_success ?
    block_info(d->m_uniform_list.default_uniform_block()) :
    block_info(NULL);
}

unsigned int
fastuidraw::gl::Program::
number_active_uniform_blocks(void)
{
  ProgramPrivate *d;
  d = reinterpret_cast<ProgramPrivate*>(m_d);
  d->assemble(this);
  return d->m_link_success ?
    d->m_uniform_list.number_active_blocks() :
    0;
}

fastuidraw::gl::Program::block_info
fastuidraw::gl::Program::
uniform_block(unsigned int I)
{
  ProgramPrivate *d;
  d = reinterpret_cast<ProgramPrivate*>(m_d);
  d->assemble(this);
  return d->m_link_success ?
    block_info(d->m_uniform_list.block(I)) :
    block_info(NULL);
}

unsigned int
fastuidraw::gl::Program::
uniform_block_id(const char *uniform_block_name)
{
  ProgramPrivate *d;
  d = reinterpret_cast<ProgramPrivate*>(m_d);
  d->assemble(this);
  return d->m_link_success ?
    d->m_uniform_list.block_id(uniform_block_name) :
    ~0u;
}

unsigned int
fastuidraw::gl::Program::
number_active_shader_storage_blocks(void)
{
  ProgramPrivate *d;
  d = reinterpret_cast<ProgramPrivate*>(m_d);
  d->assemble(this);
  return d->m_link_success ?
    d->m_storage_buffer_list.number_active_blocks() :
    0;
}

fastuidraw::gl::Program::block_info
fastuidraw::gl::Program::
shader_storage_block(unsigned int I)
{
  ProgramPrivate *d;
  d = reinterpret_cast<ProgramPrivate*>(m_d);
  d->assemble(this);
  return d->m_link_success ?
    block_info(d->m_storage_buffer_list.block(I)) :
    block_info(NULL);
}

unsigned int
fastuidraw::gl::Program::
shader_storage_block_id(const char *shader_storage_block_name)
{
  ProgramPrivate *d;
  d = reinterpret_cast<ProgramPrivate*>(m_d);
  d->assemble(this);
  return d->m_link_success ?
    d->m_storage_buffer_list.block_id(shader_storage_block_name) :
    ~0u;
}

unsigned int
fastuidraw::gl::Program::
number_active_atomic_buffers(void)
{
  ProgramPrivate *d;
  d = reinterpret_cast<ProgramPrivate*>(m_d);
  d->assemble(this);
  return d->m_link_success ?
    d->m_uniform_list.number_atomic_buffers() :
    0;
}

fastuidraw::gl::Program::atomic_buffer_info
fastuidraw::gl::Program::
atomic_buffer(unsigned int I)
{
  ProgramPrivate *d;
  d = reinterpret_cast<ProgramPrivate*>(m_d);
  d->assemble(this);
  return d->m_link_success ?
    atomic_buffer_info(d->m_uniform_list.atomic_buffer(I)) :
    atomic_buffer_info(NULL);
}

unsigned int
fastuidraw::gl::Program::
number_active_attributes(void)
{
  ProgramPrivate *d;
  d = reinterpret_cast<ProgramPrivate*>(m_d);
  d->assemble(this);
  return d->m_link_success ?
    d->m_attribute_list.values().size() :
    0;
}

fastuidraw::gl::Program::shader_variable_info
fastuidraw::gl::Program::
active_attribute(unsigned int I)
{
  ProgramPrivate *d;
  d = reinterpret_cast<ProgramPrivate*>(m_d);
  d->assemble(this);
  return d->m_link_success ?
    shader_variable_info(&d->m_attribute_list.value(I)) :
    shader_variable_info(NULL);
}

unsigned int
fastuidraw::gl::Program::
active_attribute_id(const char *pname)
{
  ProgramPrivate *d;
  d = reinterpret_cast<ProgramPrivate*>(m_d);
  d->assemble(this);
  return d->m_link_success ?
    d->m_attribute_list.find_variable(pname).set_id() :
    ~0u;
}

GLint
fastuidraw::gl::Program::
attribute_location(const char *pname)
{
  ProgramPrivate *d;
  d = reinterpret_cast<ProgramPrivate*>(m_d);
  d->assemble(this);
  return d->m_link_success ?
    d->m_attribute_list.find_variable(pname).gl_location() :
    -1;
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
