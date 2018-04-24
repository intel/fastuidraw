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
    BindAttributePrivate(fastuidraw::c_string pname, int plocation):
      m_label(pname),
      m_location(plocation)
    {}

    std::string m_label;
    int m_location;
  };

  class BindFragDataLocationPrivate
  {
  public:
    BindFragDataLocationPrivate(fastuidraw::c_string pname, int plocation, int pindex):
      m_label(pname),
      m_location(plocation),
      m_index(pindex)
    {}

    std::string m_label;
    int m_location, m_index;
  };

  class PreLinkActionArrayPrivate
  {
  public:
    std::vector<fastuidraw::reference_counted_ptr<fastuidraw::gl::PreLinkAction> > m_values;
  };

  class BlockInitializerPrivate
  {
  public:
    BlockInitializerPrivate(fastuidraw::c_string n, int v):
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
                           1, nullptr, &name_length);
    if (name_length <= 0)
      {
        return std::string();
      }

    std::vector<GLchar> dst_name(name_length, '\0');
    glGetProgramResourceName(program, program_interface, index,
                             dst_name.size(), nullptr, &dst_name[0]);

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
      m_glsl_type(GL_INVALID_ENUM),
      m_shader_variable_src(fastuidraw::gl::Program::src_null),
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

    bool
    array_index_ok(int v) const
    {
      return m_count == 0 || v < m_count;
    }

    bool
    leading_index_ok(int v) const
    {
      return m_shader_storage_buffer_top_level_array_size == 0
        || v < m_shader_storage_buffer_top_level_array_size;
    }

    std::string m_name;
    GLint m_glsl_type;
    enum fastuidraw::gl::Program::shader_variable_src_t m_shader_variable_src;
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

    static
    void
    set_shader_variable_src_type(bool shader_variables_are_attributes,
                                 std::vector<ShaderVariableInfo> &dst);

  private:

    std::vector<GLenum> m_enums;
    std::vector<dst_type> m_dsts;
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
      FASTUIDRAWassert(!m_finalized);
      m_values.push_back(value);
      return return_value;
    }

    template<typename iterator>
    void
    add_elements(iterator begin, iterator end)
    {
      for(;begin != end; ++begin)
        {
          add_element(*begin);
        }
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

    const ShaderVariableInfo*
    find_variable(const std::string &pname,
                  unsigned int *array_index,
                  unsigned int *leading_array_index,
                  bool check_leading_dim) const;

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
    filter_name(iterator begin, iterator end, unsigned int *array_index);

    template<typename iterator>
    static
    std::string
    filter_name_leading_index(iterator begin, iterator end,
                              unsigned int *leading_index);

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

    void
    finalize(void);

    void
    add_element(ShaderVariableInfo v);

    const ShaderVariableSet&
    members(void) const
    {
      return m_members;
    }

    GLint m_buffer_binding;
    GLint m_buffer_index;
    GLint m_size_bytes;
    std::string m_name;

  private:
    ShaderVariableSet m_members;
  };

  class BlockInfoPrivate
  {
  public:
    BlockInfoPrivate(void):
      m_initial_buffer_binding(-1),
      m_block_index(-1),
      m_size_bytes(0),
      m_backing_type(fastuidraw::gl::Program::src_null)
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
    GLint m_initial_buffer_binding;
    GLint m_block_index;
    GLint m_size_bytes;
    ShaderVariableSet m_members;
    enum fastuidraw::gl::Program::shader_variable_src_t m_backing_type;
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
      FASTUIDRAWassert(m_finalized);
      return m_blocks.size();
    }

    const BlockInfoPrivate*
    block(unsigned int I)
    {
      FASTUIDRAWassert(m_finalized);
      return (I < m_blocks.size()) ?
        &m_blocks[I] :
        nullptr;
    }

    unsigned int
    block_id(const std::string &name) const
    {
      std::map<std::string, unsigned int>::const_iterator iter;

      FASTUIDRAWassert(m_finalized);
      iter = m_map.find(name);
      return (iter != m_map.end()) ?
        iter->second:
        ~0u;
    }

    void
    populate_as_empty(void)
    {
      FASTUIDRAWassert(!m_finalized);
      FASTUIDRAWassert(m_map.empty());
      FASTUIDRAWassert(m_blocks.empty());
      m_finalized = true;
    }

  protected:
    void
    finalize(void);

    void
    resize_number_blocks(unsigned int N,
                         enum fastuidraw::gl::Program::shader_variable_src_t tp);

    BlockInfoPrivate&
    block_ref(unsigned int I)
    {
      FASTUIDRAWassert(!m_finalized);
      FASTUIDRAWassert(I < m_blocks.size());
      return m_blocks[I];
    }

  private:
    bool m_finalized;
    std::vector<BlockInfoPrivate> m_blocks;
    std::map<std::string, unsigned int> m_map; //gives index into m_blocks
  };

  /* class to hold information for each shader storage
     block
  */
  class ShaderStorageBlockSetInfo:public BlockSetInfoBase
  {
  public:
    void
    populate(GLuint program,
             const fastuidraw::gl::ContextProperties &ctx_props);

    const ShaderVariableSet&
    all_shader_storage_variables(void) const
    {
      return m_all_shader_storage_variables;
    }

  private:
    ShaderVariableSet m_all_shader_storage_variables;
  };

  /* class to hold information for each uniform block,
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
        nullptr;
    }

    unsigned int
    atomic_buffer_id(unsigned int binding_pt) const
    {
      std::map<unsigned int, unsigned int>::const_iterator iter;
      iter = m_atomic_map.find(binding_pt);
      return (iter != m_atomic_map.end()) ?
        iter->second :
        ~0u;
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
    std::map<unsigned int, unsigned int> m_atomic_map;
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

    ProgramPrivate(const fastuidraw::c_array<const ShaderRef> pshaders,
                   const fastuidraw::gl::PreLinkActionArray &action,
                   const fastuidraw::gl::ProgramInitializerArray &initers,
                   fastuidraw::gl::Program *p):
      m_shaders(pshaders.begin(), pshaders.end()),
      m_name(0),
      m_delete_program(true),
      m_assembled(false),
      m_initializers(initers),
      m_pre_link_actions(action),
      m_p(p)
    {
      for(const ShaderRef &R : m_shaders)
        {
          FASTUIDRAWassert(R);
        }
    }

    ProgramPrivate(fastuidraw::reference_counted_ptr<fastuidraw::gl::Shader> vert_shader,
                   fastuidraw::reference_counted_ptr<fastuidraw::gl::Shader> frag_shader,
                   const fastuidraw::gl::PreLinkActionArray &action,
                   const fastuidraw::gl::ProgramInitializerArray &initers,
                   fastuidraw::gl::Program *p):
      m_name(0),
      m_delete_program(true),
      m_assembled(false),
      m_initializers(initers),
      m_pre_link_actions(action),
      m_p(p)
    {
      FASTUIDRAWassert(vert_shader && vert_shader->shader_type() == GL_VERTEX_SHADER);
      FASTUIDRAWassert(frag_shader && frag_shader->shader_type() == GL_FRAGMENT_SHADER);
      m_shaders.push_back(vert_shader);
      m_shaders.push_back(frag_shader);
    }

    ProgramPrivate(const fastuidraw::glsl::ShaderSource &vert_shader,
                   const fastuidraw::glsl::ShaderSource &frag_shader,
                   const fastuidraw::gl::PreLinkActionArray &action,
                   const fastuidraw::gl::ProgramInitializerArray &initers,
                   fastuidraw::gl::Program *p):
      m_name(0),
      m_delete_program(true),
      m_assembled(false),
      m_initializers(initers),
      m_pre_link_actions(action),
      m_p(p)
    {
      m_shaders.push_back(FASTUIDRAWnew fastuidraw::gl::Shader(vert_shader, GL_VERTEX_SHADER));
      m_shaders.push_back(FASTUIDRAWnew fastuidraw::gl::Shader(frag_shader, GL_FRAGMENT_SHADER));
    }

    ProgramPrivate(GLuint pname, bool take_ownership, fastuidraw::gl::Program *p);

    void
    assemble(void);

    void
    populate_info(void);

    void
    clear_shaders_and_save_shader_data(void);

    void
    generate_log(void);

    std::vector<fastuidraw::reference_counted_ptr<fastuidraw::gl::Shader> > m_shaders;
    std::vector<ShaderData> m_shader_data;
    std::map<GLenum, std::vector<int> > m_shader_data_sorted_by_type;

    GLuint m_name;
    bool m_delete_program;
    bool m_link_success, m_assembled;
    std::string m_link_log;
    std::string m_log;
    float m_assemble_time;

    std::set<std::string> m_binded_attributes;
    AttributeInfo m_attribute_list;
    UniformBlockSetInfo m_uniform_list;
    ShaderStorageBlockSetInfo m_storage_buffer_list;
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
  if (m_shader_ready)
    {
      return;
    }

  //now do the GL work, create a name and compile the source code:
  FASTUIDRAWassert(m_name == 0);

  m_shader_ready = true;
  m_name = glCreateShader(m_shader_type);

  fastuidraw::c_string sourceString[1];
  sourceString[0] = m_source_code.c_str();

  glShaderSource(m_name, //shader handle
                 1, //number strings
                 sourceString, //array of strings
                 nullptr); //lengths of each string or nullptr implies each is 0-terminated

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
                     nullptr, //GLint* return length of string
                     &raw_log[0]); //char* to write log to.

  m_compile_log = &raw_log[0];
  m_compile_success = (shaderOK == GL_TRUE);

  if (!m_compile_success)
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

  if (!m_enums.empty())
    {
      m_work_room.resize(m_enums.size());
      for(unsigned int i = 0, endi = m_enums.size(); i < endi; ++i)
        {
          m_work_room[i] = dst.*m_dsts[i];
        }

      glGetProgramResourceiv(program, variable_interface, variable_interface_index,
                             m_enums.size(), &m_enums[0],
                             m_work_room.size(), nullptr, &m_work_room[0]);

      for(unsigned int i = 0, endi = m_enums.size(); i < endi; ++i)
        {
          dst.*m_dsts[i] = m_work_room[i];
        }
    }

  switch(variable_interface)
    {
    case GL_UNIFORM:
      if (dst.m_abo_index != -1)
        {
          dst.m_shader_variable_src = fastuidraw::gl::Program::src_abo;
        }
      else if (dst.m_ubo_index != -1)
        {
          dst.m_shader_variable_src = fastuidraw::gl::Program::src_uniform_block;
        }
      else
        {
          dst.m_shader_variable_src = fastuidraw::gl::Program::src_default_uniform_block;
        }
      break;

    case GL_PROGRAM_INPUT:
      dst.m_shader_variable_src = fastuidraw::gl::Program::src_shader_input;
      break;

    case GL_BUFFER_VARIABLE:
      dst.m_shader_variable_src = fastuidraw::gl::Program::src_shader_storage_block;
      break;

    case GL_PROGRAM_OUTPUT:
      dst.m_shader_variable_src = fastuidraw::gl::Program::src_shader_output;
      break;

    default:
      FASTUIDRAWassert(!"Unhandled variable interface type to assign to m_shader_variable_src");
    }
}

////////////////////////////////////
// ShaderUniformQueryList methods
void
ShaderUniformQueryList::
fill_variables(GLuint program,
               std::vector<ShaderVariableInfo> &dst) const
{
  FASTUIDRAWassert(m_enums.size() == m_dsts.size());
  if (dst.empty())
    {
      return;
    }

  if (m_enums.empty())
    {
      // values for non-uniforms, thus must be attributes.
      set_shader_variable_src_type(true, dst);
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

  // values of for uniforms, thus are not attributes.
  set_shader_variable_src_type(false, dst);
}

void
ShaderUniformQueryList::
set_shader_variable_src_type(bool shader_variables_are_attributes,
                             std::vector<ShaderVariableInfo> &dst)
{
  if (shader_variables_are_attributes)
    {
      for(ShaderVariableInfo &v : dst)
        {
          v.m_shader_variable_src = fastuidraw::gl::Program::src_shader_input;
        }
    }
  else
    {
      for(ShaderVariableInfo &v : dst)
        {
          v.m_shader_variable_src = (v.m_ubo_index != -1) ?
            fastuidraw::gl::Program::src_uniform_block :
            fastuidraw::gl::Program::src_default_uniform_block;
        }
    }
}

////////////////////////////////////
// ShaderVariableSet methods
void
ShaderVariableSet::
finalize(void)
{
  if (m_finalized)
    {
      return;
    }

  m_finalized = true;
  std::sort(m_values.begin(), m_values.end());
  for(unsigned int i = 0, endi = m_values.size(); i < endi; ++i)
    {
      m_map[m_values[i].m_name] = i;
    }
}

const ShaderVariableInfo*
ShaderVariableSet::
find_variable(const std::string &pname,
              unsigned int *array_index,
              unsigned int *leading_array_index,
              bool check_leading_index) const
{
  std::map<std::string, unsigned int>::const_iterator iter;
  unsigned int A;

  if (array_index == nullptr)
    {
      array_index = &A;
    }
  if (leading_array_index == nullptr)
    {
      leading_array_index = &A;
    }

  *array_index = 0;
  *leading_array_index = 0;

  FASTUIDRAWassert(m_finalized);
  iter = m_map.find(pname);
  if (iter != m_map.end())
    {
      return &m_values[iter->second];
    }

  std::string filtered_name, filtered_name2;

  filtered_name = filter_name(pname.begin(), pname.end(), array_index);
  iter = m_map.find(filtered_name);
  if (iter == m_map.end())
    {
      filtered_name2 = filtered_name + "[0]";
      iter = m_map.find(filtered_name2);
    }

  if (iter != m_map.end())
    {
      const ShaderVariableInfo *q;

      q = &m_values[iter->second];
      if (q->array_index_ok(*array_index))
        {
          return q;
        }
      else
        {
          return nullptr;
        }
    }

  if (check_leading_index)
    {
      filtered_name = filter_name_leading_index(filtered_name.begin(),
                                                filtered_name.end(),
                                                leading_array_index);
      iter = m_map.find(filtered_name);
      if (iter == m_map.end())
        {
          filtered_name = filter_name_leading_index(filtered_name2.begin(),
                                                    filtered_name2.end(),
                                                    leading_array_index);
          iter = m_map.find(filtered_name);
        }

      if (iter != m_map.end())
        {
          const ShaderVariableInfo *q;

          q = &m_values[iter->second];
          if (q->array_index_ok(*array_index) && q->leading_index_ok(*leading_array_index))
            {
              return q;
            }
          else
            {
              return nullptr;
            }
        }
    }

  return nullptr;
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
  if (count > 0)
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

          dst.m_glsl_type = ptype;
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
                         1, nullptr, &num_variables);
  if (num_variables > 0)
    {
      const GLenum prop_active(GL_ACTIVE_VARIABLES);
      std::vector<GLint> variable_index(num_variables, -1);

      glGetProgramResourceiv(program, program_interface, interface_index,
                             1, &prop_active,
                             num_variables, nullptr, &variable_index[0]);
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
filter_name(iterator begin, iterator end, unsigned int *array_index)
{
  std::string return_value;

  /*
    Firstly, strip out all white spaces:
  */
  return_value.reserve(std::distance(begin,end));
  for(iterator iter = begin; iter != end; ++iter)
    {
      if (!std::isspace(*iter))
        {
          return_value.append(1, *iter);
        }
    }

  /*
    now check if the last character is a ']'
    and if it is remove characters until
    a '[' is encountered.
  */
  if (!return_value.empty() and *return_value.rbegin() == ']')
    {
      std::string::size_type loc;

      loc = return_value.find_last_of('[');
      FASTUIDRAWassert(loc != std::string::npos);

      std::istringstream array_value(return_value.substr(loc+1, return_value.size()-1));
      array_value >> *array_index;

      //also resize return_value to remove
      //the [array_index]:
      return_value.resize(loc);
    }
  else
    {
      *array_index = 0;
    }
  return return_value;
}

template<typename iterator>
std::string
ShaderVariableSet::
filter_name_leading_index(iterator begin, iterator end, unsigned int *leading_index)
{
  iterator open_bracket, open_bracket_plus_one, close_bracket;

  open_bracket = std::find(begin, end, '[');
  if (open_bracket == end)
    {
      *leading_index = ~0u;
      return std::string();
    }

  open_bracket_plus_one = open_bracket;
  ++open_bracket_plus_one;

  close_bracket = std::find(open_bracket_plus_one, end, ']');
  if (close_bracket == end)
    {
      *leading_index = ~0u;
      return std::string();
    }

  std::istringstream istr(std::string(open_bracket_plus_one, close_bracket));
  istr >> *leading_index;

  ++close_bracket;
  return std::string(begin, open_bracket) + "[0]" + std::string(close_bracket, end);
}

///////////////////////////////////
// AtomicBufferInfo methods
void
AtomicBufferInfo::
finalize(void)
{
  m_members.finalize();

  std::ostringstream str;

  FASTUIDRAWassert(m_buffer_binding >= 0);
  FASTUIDRAWassert(m_name.empty());
  str << "#" << m_buffer_binding << "_atomic_buffer";
  m_name = str.str();
}

void
AtomicBufferInfo::
add_element(ShaderVariableInfo v)
{
  v.m_shader_variable_src = fastuidraw::gl::Program::src_abo;
  m_members.add_element(v);
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
                         1, nullptr, &m_size_bytes);

  const GLenum prop_binding_point(GL_BUFFER_BINDING);
  glGetProgramResourceiv(program, program_interface, m_block_index,
                         1, &prop_binding_point,
                         1, nullptr, &m_initial_buffer_binding);
}


///////////////////////////////
// AttributeInfo methods
void
AttributeInfo::
populate(GLuint program, const fastuidraw::gl::ContextProperties &ctx_props)
{
  bool use_program_interface_query;

  if (ctx_props.is_es())
    {
      use_program_interface_query = (ctx_props.version() >= fastuidraw::ivec2(3, 1));
    }
  else
    {
      use_program_interface_query = (ctx_props.version() >= fastuidraw::ivec2(4, 3))
        || ctx_props.has_extension("GL_ARB_program_interface_query");
    }

  if (use_program_interface_query)
    {
      ShaderVariableInterfaceQueryList attribute_queries;

      attribute_queries
        .add(GL_TYPE, &ShaderVariableInfo::m_glsl_type)
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
finalize(void)
{
  if (m_finalized)
    {
      return;
    }

  m_finalized = true;
  for(int i = 0, endi = m_blocks.size(); i < endi; ++i)
    {
      m_blocks[i].m_members.finalize();
      m_map[m_blocks[i].m_name] = i;
    }
}

void
BlockSetInfoBase::
resize_number_blocks(unsigned int N,
                     enum fastuidraw::gl::Program::shader_variable_src_t tp)
{
  FASTUIDRAWassert(!m_finalized);
  FASTUIDRAWassert(m_blocks.empty());
  m_blocks.resize(N);
  for(unsigned int i = 0; i < N; ++i)
    {
      m_blocks[i].m_backing_type = tp;
    }
}

///////////////////////////////////////////
//ShaderStorageBlockSetInfo methods
void
ShaderStorageBlockSetInfo::
populate(GLuint program,
         const fastuidraw::gl::ContextProperties &ctx_props)
{
  bool ssbo_supported;

  if (ctx_props.is_es())
    {
      ssbo_supported = ctx_props.version() >= fastuidraw::ivec2(3, 1);
    }
  else
    {
      ssbo_supported = ctx_props.version() >= fastuidraw::ivec2(4, 3)
        || ctx_props.has_extension("GL_ARB_shader_storage_buffer_object");
    }

  if (ssbo_supported)
    {
      ShaderVariableInterfaceQueryList ssbo_query;
      GLint ssbo_count(0);

      ssbo_query
        .add(GL_TYPE, &ShaderVariableInfo::m_glsl_type)
        .add(GL_ARRAY_SIZE, &ShaderVariableInfo::m_count)
        .add(GL_OFFSET, &ShaderVariableInfo::m_offset)
        .add(GL_ARRAY_STRIDE, &ShaderVariableInfo::m_array_stride)
        .add(GL_MATRIX_STRIDE, &ShaderVariableInfo::m_matrix_stride)
        .add(GL_IS_ROW_MAJOR, &ShaderVariableInfo::m_is_row_major)
        .add(GL_BLOCK_INDEX, &ShaderVariableInfo::m_shader_storage_buffer_index)
        .add(GL_TOP_LEVEL_ARRAY_SIZE, &ShaderVariableInfo::m_shader_storage_buffer_top_level_array_size)
        .add(GL_TOP_LEVEL_ARRAY_STRIDE, &ShaderVariableInfo::m_shader_storage_buffer_top_level_array_stride);

      glGetProgramInterfaceiv(program, GL_SHADER_STORAGE_BLOCK, GL_ACTIVE_RESOURCES, &ssbo_count);
      resize_number_blocks(ssbo_count, fastuidraw::gl::Program::src_shader_storage_block);
      for(GLint i = 0; i < ssbo_count; ++i)
        {
          block_ref(i).populate(program, GL_SHADER_STORAGE_BLOCK, i, GL_BUFFER_VARIABLE, ssbo_query);
          m_all_shader_storage_variables.add_elements(block_ref(i).m_members.values().begin(),
                                                      block_ref(i).m_members.values().end());
        }
      finalize();
      m_all_shader_storage_variables.finalize();
    }
  else
    {
      populate_as_empty();
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

  if (ctx_props.is_es())
    {
      use_program_interface_query = (ctx_props.version() >= fastuidraw::ivec2(3, 1));
    }
  else
    {
      use_program_interface_query = (ctx_props.version() >= fastuidraw::ivec2(4, 3))
        || ctx_props.has_extension("GL_ARB_program_interface_query");
    }

  if (use_program_interface_query)
    {
      populate_private_program_interface_query(program, ctx_props);
    }
  else
    {
      populate_private_non_program_interface_query(program, ctx_props);
    }

  /* finalize default uniform block
   */
  m_default_block.m_backing_type = fastuidraw::gl::Program::src_default_uniform_block;
  m_default_block.m_members.finalize();

  /* finalize non-default uniform blocks
   */
  finalize();

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
    .add(GL_TYPE, &ShaderVariableInfo::m_glsl_type)
    .add(GL_ARRAY_SIZE, &ShaderVariableInfo::m_count)
    .add(GL_LOCATION, &ShaderVariableInfo::m_location)
    .add(GL_BLOCK_INDEX, &ShaderVariableInfo::m_ubo_index)
    .add(GL_OFFSET, &ShaderVariableInfo::m_offset)
    .add(GL_ARRAY_STRIDE, &ShaderVariableInfo::m_array_stride)
    .add(GL_MATRIX_STRIDE, &ShaderVariableInfo::m_matrix_stride)
    .add(GL_IS_ROW_MAJOR, &ShaderVariableInfo::m_is_row_major)
    .add(GL_ATOMIC_COUNTER_BUFFER_INDEX, &ShaderVariableInfo::m_abo_index);

  m_all_uniforms.populate_from_resource(program, GL_UNIFORM, uniform_queries);

  /* populate the ABO blocks */
  GLint abo_count(0);
  glGetProgramInterfaceiv(program, GL_ATOMIC_COUNTER_BUFFER, GL_ACTIVE_RESOURCES, &abo_count);
  m_abo_buffers.resize(abo_count);

  /* extract from m_all_uniforms those uniforms from the default block */
  for(const ShaderVariableInfo &u : m_all_uniforms.values())
    {
      /* Do NOT put ABO's in m_default_block */
      if (u.m_abo_index != -1)
        {
          m_abo_buffers[u.m_abo_index].add_element(u);
        }
      else if (u.m_ubo_index == -1)
        {
          m_default_block.m_members.add_element(u);
        }
    }

  /* poplulate each of the uniform blocks. */
  GLint ubo_count(0);
  glGetProgramInterfaceiv(program, GL_UNIFORM_BLOCK, GL_ACTIVE_RESOURCES, &ubo_count);
  resize_number_blocks(ubo_count, fastuidraw::gl::Program::src_uniform_block);
  for(GLint i = 0; i < ubo_count; ++i)
    {
      block_ref(i).populate(program, GL_UNIFORM_BLOCK, i, GL_UNIFORM, uniform_queries);
    }

  for(GLint i = 0; i < abo_count; ++i)
    {
      const GLenum prop_data_size(GL_BUFFER_DATA_SIZE);
      const GLenum prop_binding(GL_BUFFER_BINDING);

      m_abo_buffers[i].m_buffer_index = i;
      glGetProgramResourceiv(program, GL_ATOMIC_COUNTER_BUFFER, i,
                             1, &prop_data_size,
                             1, nullptr, &m_abo_buffers[i].m_size_bytes);
      glGetProgramResourceiv(program, GL_ATOMIC_COUNTER_BUFFER, i,
                             1, &prop_binding,
                             1, nullptr, &m_abo_buffers[i].m_buffer_binding);
      m_abo_buffers[i].finalize();
      m_atomic_map[m_abo_buffers[i].m_buffer_binding] = i;
    }

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

      if (abo_supported)
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
  resize_number_blocks(ubo_count, fastuidraw::gl::Program::src_uniform_block);
  if (ubo_count > 0)
    {
      GLint largest_length(0);
      std::vector<char> pname;

      glGetProgramiv(program, GL_ACTIVE_UNIFORM_BLOCK_MAX_NAME_LENGTH, &largest_length);
      ++largest_length;
      pname.resize(largest_length, '\0');

      for(int i = 0; i < ubo_count; ++i)
        {
          GLsizei name_length(0);
          std::memset(&pname[0], 0, largest_length);

          glGetActiveUniformBlockName(program, i, largest_length, &name_length, &pname[0]);
          glGetActiveUniformBlockiv(program, i, GL_UNIFORM_BLOCK_DATA_SIZE, &block_ref(i).m_size_bytes);
          glGetActiveUniformBlockiv(program, i, GL_UNIFORM_BLOCK_BINDING, &block_ref(i).m_initial_buffer_binding);

          block_ref(i).m_block_index = i;
          block_ref(i).m_name = std::string(pname.begin(), pname.begin() + name_length);
        }
    }

  /* extract uniform data from all_uniforms, note that
     m_blocks[i] holds the uniform block with block_index
     of value i currently.
  */
  for(const ShaderVariableInfo &u : m_all_uniforms.values())
    {
      /* Do NOT put ABO's in m_default_block */
      if (u.m_abo_index != -1)
        {
          FASTUIDRAWassert(u.m_abo_index < static_cast<int>(m_abo_buffers.size()));
          m_abo_buffers[u.m_abo_index].add_element(u);
        }
      else if (u.m_ubo_index != -1)
        {
          block_ref(u.m_ubo_index).m_members.add_element(u);
        }
      else
        {
          m_default_block.m_members.add_element(u);
        }
    }

  /* finalize each atomic buffer block
   */
  for(unsigned int i = 0, endi = m_abo_buffers.size(); i < endi; ++i)
    {
      m_abo_buffers[i].finalize();
      m_atomic_map[m_abo_buffers[i].m_buffer_binding] = i;
    }

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
  d = static_cast<ShaderPrivate*>(m_d);

  /*
    TODO: deletion of a shader should not be
    required to be done with a GL context
    current.
   */
  if (d->m_name)
    {
      glDeleteShader(d->m_name);
    }
  FASTUIDRAWdelete(d);
  m_d = nullptr;
}


bool
fastuidraw::gl::Shader::
compile_success(void)
{
  ShaderPrivate *d;
  d = static_cast<ShaderPrivate*>(m_d);
  d->compile();
  return d->m_compile_success;
}

fastuidraw::c_string
fastuidraw::gl::Shader::
compile_log(void)
{
  ShaderPrivate *d;
  d = static_cast<ShaderPrivate*>(m_d);
  d->compile();
  return d->m_compile_log.c_str();
}

GLuint
fastuidraw::gl::Shader::
name(void)
{
  ShaderPrivate *d;
  d = static_cast<ShaderPrivate*>(m_d);
  d->compile();
  return d->m_name;
}

bool
fastuidraw::gl::Shader::
shader_ready(void)
{
  ShaderPrivate *d;
  d = static_cast<ShaderPrivate*>(m_d);
  return d->m_shader_ready;
}

fastuidraw::c_string
fastuidraw::gl::Shader::
source_code(void)
{
  ShaderPrivate *d;
  d = static_cast<ShaderPrivate*>(m_d);
  return d->m_source_code.c_str();
}

GLenum
fastuidraw::gl::Shader::
shader_type(void)
{
  ShaderPrivate *d;
  d = static_cast<ShaderPrivate*>(m_d);
  return d->m_shader_type;
}


fastuidraw::c_string
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
      CASE(GL_GEOMETRY_SHADER);
      CASE(GL_TESS_EVALUATION_SHADER);
      CASE(GL_TESS_CONTROL_SHADER);
      CASE(GL_COMPUTE_SHADER);
    }

  #undef CASE
}

fastuidraw::c_string
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
BindAttribute(c_string pname, int plocation)
{
  m_d = FASTUIDRAWnew BindAttributePrivate(pname, plocation);
}

fastuidraw::gl::BindAttribute::
~BindAttribute()
{
  BindAttributePrivate *d;
  d = static_cast<BindAttributePrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = nullptr;
}

void
fastuidraw::gl::BindAttribute::
action(GLuint glsl_program) const
{
  BindAttributePrivate *d;
  d = static_cast<BindAttributePrivate*>(m_d);
  glBindAttribLocation(glsl_program, d->m_location, d->m_label.c_str());
}


////////////////////////////////////////
// fastuidraw::gl::BindFragDataLocation methods
fastuidraw::gl::BindFragDataLocation::
BindFragDataLocation(c_string pname, int plocation, int pindex)
{
  m_d = FASTUIDRAWnew BindFragDataLocationPrivate(pname, plocation, pindex);
}

fastuidraw::gl::BindFragDataLocation::
~BindFragDataLocation()
{
  BindFragDataLocationPrivate *d;
  d = static_cast<BindFragDataLocationPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = nullptr;
}

void
fastuidraw::gl::BindFragDataLocation::
action(GLuint glsl_program) const
{
  BindFragDataLocationPrivate *d;
  d = static_cast<BindFragDataLocationPrivate*>(m_d);
  #ifdef FASTUIDRAW_GL_USE_GLES
    {
      glBindFragDataLocationIndexedEXT(glsl_program, d->m_location, d->m_index, d->m_label.c_str());
    }
  #else
    {
      glBindFragDataLocationIndexed(glsl_program, d->m_location, d->m_index, d->m_label.c_str());
    }
  #endif
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
  d = static_cast<PreLinkActionArrayPrivate*>(obj.m_d);
  m_d = FASTUIDRAWnew PreLinkActionArrayPrivate(*d);
}

fastuidraw::gl::PreLinkActionArray::
~PreLinkActionArray()
{
  PreLinkActionArrayPrivate *d;
  d = static_cast<PreLinkActionArrayPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = nullptr;
}

void
fastuidraw::gl::PreLinkActionArray::
swap(PreLinkActionArray &obj)
{
  std::swap(m_d, obj.m_d);
}

fastuidraw::gl::PreLinkActionArray&
fastuidraw::gl::PreLinkActionArray::
operator=(const PreLinkActionArray &rhs)
{
  if (this != &rhs)
    {
      PreLinkActionArray v(rhs);
      swap(v);
    }
  return *this;
}

fastuidraw::gl::PreLinkActionArray&
fastuidraw::gl::PreLinkActionArray::
add(reference_counted_ptr<PreLinkAction> h)
{
  PreLinkActionArrayPrivate *d;
  d = static_cast<PreLinkActionArrayPrivate*>(m_d);

  FASTUIDRAWassert(h);
  d->m_values.push_back(h);
  return *this;
}

void
fastuidraw::gl::PreLinkActionArray::
execute_actions(GLuint pr) const
{
  PreLinkActionArrayPrivate *d;
  d = static_cast<PreLinkActionArrayPrivate*>(m_d);
  for(const auto &p : d->m_values)
    {
      if (p)
        {
          p->action(pr);
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
  m_d(nullptr)
{}

fastuidraw::c_string
fastuidraw::gl::Program::shader_variable_info::
name(void) const
{
  const ShaderVariableInfo *d;
  d = static_cast<const ShaderVariableInfo*>(m_d);
  return (d) ? d->m_name.c_str() : "";
}

GLenum
fastuidraw::gl::Program::shader_variable_info::
glsl_type(void) const
{
  const ShaderVariableInfo *d;
  d = static_cast<const ShaderVariableInfo*>(m_d);
  return (d) ? d->m_glsl_type : GL_INVALID_ENUM;
}

enum fastuidraw::gl::Program::shader_variable_src_t
fastuidraw::gl::Program::shader_variable_info::
shader_variable_src(void) const
{
  const ShaderVariableInfo *d;
  d = static_cast<const ShaderVariableInfo*>(m_d);
  return (d) ? d->m_shader_variable_src : src_null;
}

GLint
fastuidraw::gl::Program::shader_variable_info::
count(void) const
{
  const ShaderVariableInfo *d;
  d = static_cast<const ShaderVariableInfo*>(m_d);
  return (d) ? d->m_count : -1;
}

GLuint
fastuidraw::gl::Program::shader_variable_info::
index(void) const
{
  const ShaderVariableInfo *d;
  d = static_cast<const ShaderVariableInfo*>(m_d);
  return (d) ? d->m_index : -1;
}

GLint
fastuidraw::gl::Program::shader_variable_info::
location(unsigned int array_index) const
{
  const ShaderVariableInfo *d;
  d = static_cast<const ShaderVariableInfo*>(m_d);
  return (d && d->m_location != -1) ?
    d->m_location + array_index:
    -1;
}

GLint
fastuidraw::gl::Program::shader_variable_info::
ubo_index(void) const
{
  const ShaderVariableInfo *d;
  d = static_cast<const ShaderVariableInfo*>(m_d);
  return (d) ? d->m_ubo_index : -1;
}

GLint
fastuidraw::gl::Program::shader_variable_info::
buffer_offset(unsigned int array_index,
              unsigned int leading_array_index) const
{
  const ShaderVariableInfo *d;
  d = static_cast<const ShaderVariableInfo*>(m_d);
  return (d && d->m_offset != -1) ?
    d->m_offset
    + array_index * d->m_array_stride
    + leading_array_index * d->m_shader_storage_buffer_top_level_array_stride:
    -1;
}

GLint
fastuidraw::gl::Program::shader_variable_info::
array_stride(void) const
{
  const ShaderVariableInfo *d;
  d = static_cast<const ShaderVariableInfo*>(m_d);
  return (d) ? d->m_array_stride : -1;
}

GLint
fastuidraw::gl::Program::shader_variable_info::
matrix_stride(void) const
{
  const ShaderVariableInfo *d;
  d = static_cast<const ShaderVariableInfo*>(m_d);
  return (d) ? d->m_matrix_stride : -1;
}

bool
fastuidraw::gl::Program::shader_variable_info::
is_row_major(void) const
{
  const ShaderVariableInfo *d;
  d = static_cast<const ShaderVariableInfo*>(m_d);
  return (d) ? d->m_is_row_major : false;
}

GLint
fastuidraw::gl::Program::shader_variable_info::
abo_index(void) const
{
  const ShaderVariableInfo *d;
  d = static_cast<const ShaderVariableInfo*>(m_d);
  return (d) ? d->m_abo_index : -1;
}

GLint
fastuidraw::gl::Program::shader_variable_info::
shader_storage_buffer_index(void) const
{
  const ShaderVariableInfo *d;
  d = static_cast<const ShaderVariableInfo*>(m_d);
  return (d) ? d->m_shader_storage_buffer_index : -1;
}

GLint
fastuidraw::gl::Program::shader_variable_info::
shader_storage_buffer_top_level_array_size(void) const
{
  const ShaderVariableInfo *d;
  d = static_cast<const ShaderVariableInfo*>(m_d);
  return (d) ? d->m_shader_storage_buffer_top_level_array_size : -1;
}

GLint
fastuidraw::gl::Program::shader_variable_info::
shader_storage_buffer_top_level_array_stride(void) const
{
  const ShaderVariableInfo *d;
  d = static_cast<const ShaderVariableInfo*>(m_d);
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
  m_d(nullptr)
{}

fastuidraw::c_string
fastuidraw::gl::Program::block_info::
name(void) const
{
  const BlockInfoPrivate *d;
  d = static_cast<const BlockInfoPrivate*>(m_d);
  return (d) ? d->m_name.c_str() : "";
}

enum fastuidraw::gl::Program::shader_variable_src_t
fastuidraw::gl::Program::block_info::
shader_variable_src(void) const
{
  const BlockInfoPrivate *d;
  d = static_cast<const BlockInfoPrivate*>(m_d);
  return (d) ? d->m_backing_type : src_null;
}

GLint
fastuidraw::gl::Program::block_info::
block_index(void) const
{
  const BlockInfoPrivate *d;
  d = static_cast<const BlockInfoPrivate*>(m_d);
  return (d) ? d->m_block_index : -1;
}

GLint
fastuidraw::gl::Program::block_info::
buffer_size(void) const
{
  const BlockInfoPrivate *d;
  d = static_cast<const BlockInfoPrivate*>(m_d);
  return (d) ? d->m_size_bytes : 0;
}

GLint
fastuidraw::gl::Program::block_info::
initial_buffer_binding(void) const
{
  const BlockInfoPrivate *d;
  d = static_cast<const BlockInfoPrivate*>(m_d);
  return (d) ? d->m_initial_buffer_binding : 0;
}

unsigned int
fastuidraw::gl::Program::block_info::
number_variables(void) const
{
  const BlockInfoPrivate *d;
  d = static_cast<const BlockInfoPrivate*>(m_d);
  return d ? d->m_members.values().size() : 0;
}

fastuidraw::gl::Program::shader_variable_info
fastuidraw::gl::Program::block_info::
variable(unsigned int I)
{
  const BlockInfoPrivate *d;
  const void *q;

  d = static_cast<const BlockInfoPrivate*>(m_d);
  q = d ? &d->m_members.value(I) : nullptr;
  return shader_variable_info(q);
}

fastuidraw::gl::Program::shader_variable_info
fastuidraw::gl::Program::block_info::
variable(c_string name,
         unsigned int *out_array_index,
         unsigned int *out_leading_array_index)
{
  const BlockInfoPrivate *d;
  const ShaderVariableInfo *q;

  d = static_cast<const BlockInfoPrivate*>(m_d);
  q = (d) ?
    d->m_members.find_variable(name, out_array_index,
                               out_leading_array_index,
                               d->m_backing_type == fastuidraw::gl::Program::src_shader_storage_block):
    nullptr;

  return shader_variable_info(q);
}

///////////////////////////////////////////////////
// fastuidraw::gl::Program::atomic_buffer_info methods
fastuidraw::gl::Program::atomic_buffer_info::
atomic_buffer_info(const void *d):
  m_d(d)
{}

fastuidraw::gl::Program::atomic_buffer_info::
atomic_buffer_info(void):
  m_d(nullptr)
{}

fastuidraw::c_string
fastuidraw::gl::Program::atomic_buffer_info::
name(void) const
{
  const AtomicBufferInfo *d;
  d = static_cast<const AtomicBufferInfo*>(m_d);
  return (d) ? d->m_name.c_str() : "";
}

GLint
fastuidraw::gl::Program::atomic_buffer_info::
buffer_index(void) const
{
  const AtomicBufferInfo *d;
  d = static_cast<const AtomicBufferInfo*>(m_d);
  return (d) ? d->m_buffer_index : -1;
}

GLint
fastuidraw::gl::Program::atomic_buffer_info::
buffer_binding(void) const
{
  const AtomicBufferInfo *d;
  d = static_cast<const AtomicBufferInfo*>(m_d);
  return (d) ? d->m_buffer_binding : -1;
}

GLint
fastuidraw::gl::Program::atomic_buffer_info::
buffer_size(void) const
{
  const AtomicBufferInfo *d;
  d = static_cast<const AtomicBufferInfo*>(m_d);
  return (d) ? d->m_size_bytes : 0;
}

unsigned int
fastuidraw::gl::Program::atomic_buffer_info::
number_variables(void) const
{
  const AtomicBufferInfo *d;
  d = static_cast<const AtomicBufferInfo*>(m_d);
  return d ? d->members().values().size() : 0;
}

fastuidraw::gl::Program::shader_variable_info
fastuidraw::gl::Program::atomic_buffer_info::
variable(unsigned int I)
{
  const AtomicBufferInfo *d;
  const void *q;

  d = static_cast<const AtomicBufferInfo*>(m_d);
  q = d ? &d->members().value(I) : nullptr;
  return shader_variable_info(q);
}

fastuidraw::gl::Program::shader_variable_info
fastuidraw::gl::Program::atomic_buffer_info::
variable(c_string name,
         unsigned int *out_array_index)
{
  const AtomicBufferInfo *d;
  const ShaderVariableInfo *q;

  d = static_cast<const AtomicBufferInfo*>(m_d);
  q = (d) ?
    d->members().find_variable(name, out_array_index, nullptr, false):
    nullptr;
  return shader_variable_info(q);
}


/////////////////////////////////////////////////////////
//ProgramPrivate methods
ProgramPrivate::
ProgramPrivate(GLuint pname, bool take_ownership, fastuidraw::gl::Program *p):
  m_name(pname),
  m_delete_program(take_ownership),
  m_link_success(true),
  m_assembled(true),
  m_assemble_time(0.0f),
  m_p(p)
{
  populate_info();
}

void
ProgramPrivate::
populate_info(void)
{
  //retrieve the log
  std::vector<char> raw_log;
  GLint logSize, linkOK;

  glGetProgramiv(m_name, GL_LINK_STATUS, &linkOK);
  glGetProgramiv(m_name, GL_INFO_LOG_LENGTH, &logSize);

  raw_log.resize(logSize + 2);
  glGetProgramInfoLog(m_name, logSize + 1, nullptr , &raw_log[0]);

  m_link_log = std::string(&raw_log[0]);
  m_link_success = m_link_success && (linkOK == GL_TRUE);

  if (m_link_success)
    {
      fastuidraw::gl::ContextProperties ctx_props;
      m_attribute_list.populate(m_name, ctx_props);
      m_uniform_list.populate(m_name, ctx_props);
      m_storage_buffer_list.populate(m_name, ctx_props);

      int current_program;
      bool sso_supported;

      if (ctx_props.is_es())
        {
          sso_supported = ctx_props.version() >= fastuidraw::ivec2(3, 1);
        }
      else
        {
          sso_supported = ctx_props.version() >= fastuidraw::ivec2(4, 1)
            || ctx_props.has_extension("GL_ARB_separate_shader_objects");
        }

      if (!sso_supported)
        {
          current_program = fastuidraw::gl::context_get<int>(GL_CURRENT_PROGRAM);
          glUseProgram(m_name);
        }

      m_initializers.perform_initializations(m_p, !sso_supported);

      if (!sso_supported)
        {
          glUseProgram(current_program);
        }
    }
  m_initializers.clear();
}

void
ProgramPrivate::
assemble(void)
{
  if (m_assembled)
    {
      return;
    }

  struct timeval start_time, end_time;
  gettimeofday(&start_time, nullptr);

  std::ostringstream error_ostr;

  m_assembled = true;
  FASTUIDRAWassert(m_name == 0);
  m_name = glCreateProgram();
  m_link_success = true;

  //attatch the shaders, attaching a bad shader makes
  //m_link_success become false
  for(const auto &sh : m_shaders)
    {
      if (sh->compile_success())
        {
          glAttachShader(m_name, sh->name());
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

  gettimeofday(&end_time, nullptr);
  m_assemble_time = float(end_time.tv_sec - start_time.tv_sec)
    + float(end_time.tv_usec - start_time.tv_usec) / 1e6f;

  populate_info();

  if (!m_link_success)
    {
      std::ostringstream oo;
      oo << "bad_program_" << m_name << ".glsl";
      std::ofstream eek(oo.str().c_str());

      for(const auto &d : m_shader_data)
        {
          eek << "\n\nshader: " << d.m_name
              << "[" << fastuidraw::gl::Shader::gl_shader_type_label(d.m_shader_type) << "]\n"
              << "shader_source:\n"
              << d.m_source_code
              << "compile log:\n"
              << d.m_compile_log;
        }
      eek << "\n\nLink Log: " << m_link_log;
    }
  generate_log();
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

  ostr << "gl::Program [GLname: " << m_name << "]\n";
  if (!m_shader_data.empty())
    {
      ostr << "Shader Source Codes:";

      for(const auto &d : m_shader_data)
        {
          ostr << "\n\nGLSL name = " << d.m_name
               << ", type = " << fastuidraw::gl::Shader::gl_shader_type_label(d.m_shader_type)
               << "\nSource:\n" << d.m_source_code
               << "\n\n";
        }
    }

  if (m_link_success)
    {
      fastuidraw::gl::Program::block_info blk(m_p->default_uniform_block());
      FASTUIDRAWassert(blk);

      if (blk.number_variables() > 0)
        {
          ostr << "\n\nUniforms of Default Block:";
          for(unsigned int j = 0, endj = blk.number_variables(); j < endj; ++j)
            {
              fastuidraw::gl::Program::shader_variable_info v(blk.variable(j));

              FASTUIDRAWassert(v);
              FASTUIDRAWassert(v.ubo_index() == -1);
              FASTUIDRAWassert(v.shader_storage_buffer_index() == -1);
              FASTUIDRAWassert(v.abo_index() == -1);
              ostr << "\n\t" << v.name()
                   << "\n\t\ttype = 0x"
                   << std::hex << v.glsl_type()
                   << "\n\t\tcount = " << std::dec << v.count()
                   << "\n\t\tindex = " << std::dec << v.index()
                   << "\n\t\tlocation = " << v.location();
            }
        }
      else
        {
          ostr << "\n\nNo uniforms in default block (i.e. all uniforms are backed by buffers or textures";
        }

      for(unsigned int endi = m_p->number_active_atomic_buffers(),
            i = 0; i < endi; ++i)
        {
          fastuidraw::gl::Program::atomic_buffer_info abo(m_p->atomic_buffer(i));
          GLint abo_index(i);

          FASTUIDRAWunused(abo_index);
          FASTUIDRAWassert(abo);
          FASTUIDRAWassert(abo_index == abo.buffer_index());

          ostr << "\nABO #" << abo.buffer_index() << ":"
               << "\n\tblock_index = " << abo.buffer_index()
               << "\n\tblock_size = " << abo.buffer_size()
               << "\n\tinitial_buffer_binding = " << abo.buffer_binding()
               << "\n\tmembers:";

          for(unsigned int j = 0, endj = abo.number_variables(); j < endj; ++j)
            {
              fastuidraw::gl::Program::shader_variable_info v(abo.variable(j));

              FASTUIDRAWassert(v);
              FASTUIDRAWassert(v.abo_index() == abo.buffer_index());
              FASTUIDRAWassert(abo == m_p->atomic_buffer(abo.buffer_index()));
              FASTUIDRAWassert(v.location() == -1);
              FASTUIDRAWassert(v.ubo_index() == -1);
              FASTUIDRAWassert(v.shader_storage_buffer_index() == -1);
              ostr << "\n\t\t" << v.name()
                   << "\n\t\t\ttype = 0x"
                   << std::hex << v.glsl_type()
                   << "\n\t\t\tcount = " << std::dec << v.count()
                   << "\n\t\t\tindex = " << std::dec << v.index()
                   << "\n\t\t\tubo_index = " << v.abo_index()
                   << "\n\t\t\toffset = " << v.buffer_offset();
            }
        }

      for(unsigned int endi = m_p->number_active_uniform_blocks(),
            i = 0; i < endi; ++i)
        {
          fastuidraw::gl::Program::block_info ubo(m_p->uniform_block(i));
          FASTUIDRAWassert(ubo);

          ostr << "\n\nUniformBlock \"" << ubo.name() << "\":"
               << "\n\tblock_index = " << ubo.block_index()
               << "\n\tblock_size = " << ubo.buffer_size()
               << "\n\tinitial_buffer_binding = " << ubo.initial_buffer_binding()
               << "\n\tmembers:";

          for(unsigned int j = 0, endj = ubo.number_variables(); j < endj; ++j)
            {
              fastuidraw::gl::Program::shader_variable_info v(ubo.variable(j));

              FASTUIDRAWassert(v);
              FASTUIDRAWassert(v.ubo_index() == ubo.block_index());
              FASTUIDRAWassert(ubo == m_p->uniform_block(ubo.block_index()));
              FASTUIDRAWassert(v.location() == -1);
              FASTUIDRAWassert(v.abo_index() == -1);
              FASTUIDRAWassert(v.shader_storage_buffer_index() == -1);
              ostr << "\n\t\t" << v.name()
                   << "\n\t\t\ttype = 0x"
                   << std::hex << v.glsl_type()
                   << "\n\t\t\tcount = " << std::dec << v.count()
                   << "\n\t\t\tindex = " << std::dec << v.index()
                   << "\n\t\t\tubo_index = " << v.ubo_index()
                   << "\n\t\t\toffset = " << v.buffer_offset()
                   << "\n\t\t\tarray_stride = " << v.array_stride()
                   << "\n\t\t\tmatrix_stride = " << v.matrix_stride()
                   << "\n\t\t\tis_row_major = " << v.is_row_major();
            }
        }

      for(unsigned int endi = m_p->number_active_shader_storage_blocks(),
            i = 0; i < endi; ++i)
        {
          fastuidraw::gl::Program::block_info ssbo(m_p->shader_storage_block(i));
          FASTUIDRAWassert(ssbo);

          ostr << "\n\nShaderStorageBlock \"" << ssbo.name() << "\":"
               << "\n\tblock_index = " << ssbo.block_index()
               << "\n\tblock_size = " << ssbo.buffer_size()
               << "\n\tinitial_buffer_binding = " << ssbo.initial_buffer_binding()
               << "\n\tmembers:";

          for(unsigned int j = 0, endj = ssbo.number_variables(); j < endj; ++j)
            {
              fastuidraw::gl::Program::shader_variable_info v(ssbo.variable(j));

              FASTUIDRAWassert(v);
              FASTUIDRAWassert(v.shader_storage_buffer_index() == ssbo.block_index());
              FASTUIDRAWassert(ssbo == m_p->shader_storage_block(ssbo.block_index()));
              FASTUIDRAWassert(v.location() == -1);
              FASTUIDRAWassert(v.abo_index() == -1);
              FASTUIDRAWassert(v.ubo_index() == -1);
              ostr << "\n\t\t" << v.name()
                   << "\n\t\t\ttype = 0x"
                   << std::hex << v.glsl_type()
                   << "\n\t\t\tcount = " << std::dec << v.count()
                   << "\n\t\t\tindex = " << std::dec << v.index()
                   << "\n\t\t\tshader_storage_buffer_index = " << v.shader_storage_buffer_index()
                   << "\n\t\t\toffset = " << v.buffer_offset()
                   << "\n\t\t\tarray_stride = " << v.array_stride()
                   << "\n\t\t\tmatrix_stride = " << v.matrix_stride()
                   << "\n\t\t\tis_row_major = " << v.is_row_major()
                   << "\n\t\t\tshader_storage_buffer_top_level_array_size = "
                   << v.shader_storage_buffer_top_level_array_size()
                   << "\n\t\t\tshader_storage_buffer_top_level_array_stride = "
                   << v.shader_storage_buffer_top_level_array_stride();
            }
        }

      ostr << "\n\nAttributes:";
      for(unsigned int j = 0, endj = m_p->number_active_attributes(); j < endj; ++j)
        {
          fastuidraw::gl::Program::shader_variable_info v(m_p->active_attribute(j));

          FASTUIDRAWassert(v);
          ostr << "\n\t"
               << v.name()
               << "\n\t\ttype = 0x"
               << std::hex << v.glsl_type()
               << "\n\t\tcount = " << std::dec << v.count()
               << "\n\t\tindex = " << std::dec << v.index()
               << "\n\t\tlocation = " << v.location();
        }
    }

  if (!m_shader_data.empty())
    {
      ostr << "\nShader Logs:";
      for(const auto &d : m_shader_data)
        {
          ostr << "\n\nGLSL name = " << d.m_name
               << ", type = " << fastuidraw::gl::Shader::gl_shader_type_label(d.m_shader_type)
               << "\nSource:\n" << d.m_compile_log
               << "\n\n";
        }
    }

  ostr << "\nLink Log:\n" << m_link_log << "\n";
  m_log = ostr.str();
}

////////////////////////////////////////////////////////
//fastuidraw::gl::Program methods
fastuidraw::gl::Program::
Program(c_array<const reference_counted_ptr<Shader> > pshaders,
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
Program(GLuint pname, bool take_ownership)
{
  m_d = FASTUIDRAWnew ProgramPrivate(pname, take_ownership, this);
}

fastuidraw::gl::Program::
~Program()
{
  ProgramPrivate *d;
  d = static_cast<ProgramPrivate*>(m_d);
  if (d->m_name && d->m_delete_program)
    {
      glDeleteProgram(d->m_name);
    }
  FASTUIDRAWdelete(d);
  m_d = nullptr;
}

void
fastuidraw::gl::Program::
use_program(void)
{
  ProgramPrivate *d;
  d = static_cast<ProgramPrivate*>(m_d);
  d->assemble();

  FASTUIDRAWassert(d->m_name != 0);
  FASTUIDRAWassert(d->m_link_success);

  glUseProgram(d->m_name);
}

GLuint
fastuidraw::gl::Program::
name(void)
{
  ProgramPrivate *d;
  d = static_cast<ProgramPrivate*>(m_d);
  d->assemble();
  return d->m_name;
}

fastuidraw::c_string
fastuidraw::gl::Program::
link_log(void)
{
  ProgramPrivate *d;
  d = static_cast<ProgramPrivate*>(m_d);
  d->assemble();
  return d->m_link_log.c_str();
}

float
fastuidraw::gl::Program::
program_build_time(void)
{
  ProgramPrivate *d;
  d = static_cast<ProgramPrivate*>(m_d);
  d->assemble();
  return d->m_assemble_time;
}

bool
fastuidraw::gl::Program::
link_success(void)
{
  ProgramPrivate *d;
  d = static_cast<ProgramPrivate*>(m_d);
  d->assemble();
  return d->m_link_success;
}

fastuidraw::c_string
fastuidraw::gl::Program::
log(void)
{
  ProgramPrivate *d;
  d = static_cast<ProgramPrivate*>(m_d);
  d->assemble();
  return d->m_log.c_str();
}

fastuidraw::gl::Program::block_info
fastuidraw::gl::Program::
default_uniform_block(void)
{
  ProgramPrivate *d;
  d = static_cast<ProgramPrivate*>(m_d);
  d->assemble();
  return d->m_link_success ?
    block_info(d->m_uniform_list.default_uniform_block()) :
    block_info(nullptr);
}

unsigned int
fastuidraw::gl::Program::
number_active_uniform_blocks(void)
{
  ProgramPrivate *d;
  d = static_cast<ProgramPrivate*>(m_d);
  d->assemble();
  return d->m_link_success ?
    d->m_uniform_list.number_active_blocks() :
    0;
}

fastuidraw::gl::Program::block_info
fastuidraw::gl::Program::
uniform_block(unsigned int I)
{
  ProgramPrivate *d;
  d = static_cast<ProgramPrivate*>(m_d);
  d->assemble();
  return d->m_link_success ?
    block_info(d->m_uniform_list.block(I)) :
    block_info(nullptr);
}

unsigned int
fastuidraw::gl::Program::
uniform_block_id(c_string uniform_block_name)
{
  ProgramPrivate *d;
  d = static_cast<ProgramPrivate*>(m_d);
  d->assemble();
  return d->m_link_success ?
    d->m_uniform_list.block_id(uniform_block_name) :
    ~0u;
}

GLint
fastuidraw::gl::Program::
uniform_location(c_string name)
{
  shader_variable_info S;
  unsigned int array_index(0);
  S = default_uniform_block().variable(name, &array_index);
  return S.location(array_index);
}

fastuidraw::gl::Program::shader_variable_info
fastuidraw::gl::Program::
find_shader_variable(c_string pname,
                     unsigned int *out_array_index,
                     unsigned int *out_leading_array_index)
{
  ProgramPrivate *d;
  const ShaderVariableInfo *q;

  d = static_cast<ProgramPrivate*>(m_d);
  d->assemble();

  if (!d->m_link_success)
    {
      return shader_variable_info();
    }

  /* check the UBO's and ABO's
   */
  q = d->m_uniform_list.all_uniforms().find_variable(pname,
                                                     out_array_index,
                                                     out_leading_array_index,
                                                     false);
  if (q != nullptr)
    {
      return shader_variable_info(q);
    }

  /* check SSBO's
   */
  q = d->m_storage_buffer_list.all_shader_storage_variables().find_variable(pname,
                                                                            out_array_index,
                                                                            out_leading_array_index,
                                                                            true);
  if (q != nullptr)
    {
      return shader_variable_info(q);
    }

  return shader_variable_info();
}

unsigned int
fastuidraw::gl::Program::
number_active_shader_storage_blocks(void)
{
  ProgramPrivate *d;
  d = static_cast<ProgramPrivate*>(m_d);
  d->assemble();
  return d->m_link_success ?
    d->m_storage_buffer_list.number_active_blocks() :
    0;
}

fastuidraw::gl::Program::block_info
fastuidraw::gl::Program::
shader_storage_block(unsigned int I)
{
  ProgramPrivate *d;
  d = static_cast<ProgramPrivate*>(m_d);
  d->assemble();
  return d->m_link_success ?
    block_info(d->m_storage_buffer_list.block(I)) :
    block_info(nullptr);
}

unsigned int
fastuidraw::gl::Program::
shader_storage_block_id(c_string shader_storage_block_name)
{
  ProgramPrivate *d;
  d = static_cast<ProgramPrivate*>(m_d);
  d->assemble();
  return d->m_link_success ?
    d->m_storage_buffer_list.block_id(shader_storage_block_name) :
    ~0u;
}

unsigned int
fastuidraw::gl::Program::
number_active_atomic_buffers(void)
{
  ProgramPrivate *d;
  d = static_cast<ProgramPrivate*>(m_d);
  d->assemble();
  return d->m_link_success ?
    d->m_uniform_list.number_atomic_buffers() :
    0;
}

fastuidraw::gl::Program::atomic_buffer_info
fastuidraw::gl::Program::
atomic_buffer(unsigned int I)
{
  ProgramPrivate *d;
  d = static_cast<ProgramPrivate*>(m_d);
  d->assemble();
  return d->m_link_success ?
    atomic_buffer_info(d->m_uniform_list.atomic_buffer(I)) :
    atomic_buffer_info(nullptr);
}

unsigned int
fastuidraw::gl::Program::
atomic_buffer_id(unsigned int binding_point)
{
  ProgramPrivate *d;
  d = static_cast<ProgramPrivate*>(m_d);
  d->assemble();
  return d->m_link_success ?
    d->m_uniform_list.atomic_buffer_id(binding_point) :
    ~0u;
}

unsigned int
fastuidraw::gl::Program::
number_active_attributes(void)
{
  ProgramPrivate *d;
  d = static_cast<ProgramPrivate*>(m_d);
  d->assemble();
  return d->m_link_success ?
    d->m_attribute_list.values().size() :
    0;
}

fastuidraw::gl::Program::shader_variable_info
fastuidraw::gl::Program::
active_attribute(unsigned int I)
{
  ProgramPrivate *d;
  d = static_cast<ProgramPrivate*>(m_d);
  d->assemble();
  return d->m_link_success ?
    shader_variable_info(&d->m_attribute_list.value(I)) :
    shader_variable_info(nullptr);
}

GLint
fastuidraw::gl::Program::
attribute_location(c_string pname)
{
  ProgramPrivate *d;
  d = static_cast<ProgramPrivate*>(m_d);
  d->assemble();
  if (d->m_link_success)
    {
      const ShaderVariableInfo *q;
      unsigned int array_index(0);

      q = d->m_attribute_list.find_variable(pname, &array_index, nullptr, false);
      return (q != nullptr && q->m_location != -1) ?
        q->m_location + array_index :
        -1;
    }
  return -1;
}

unsigned int
fastuidraw::gl::Program::
num_shaders(GLenum tp) const
{
  ProgramPrivate *d;
  d = static_cast<ProgramPrivate*>(m_d);

  std::map<GLenum, std::vector<int> >::const_iterator iter;
  iter = d->m_shader_data_sorted_by_type.find(tp);
  return (iter != d->m_shader_data_sorted_by_type.end()) ?
    iter->second.size() : 0;
}

fastuidraw::c_string
fastuidraw::gl::Program::
shader_src_code(GLenum tp, unsigned int i) const
{
  ProgramPrivate *d;
  d = static_cast<ProgramPrivate*>(m_d);

  std::map<GLenum, std::vector<int> >::const_iterator iter;
  iter = d->m_shader_data_sorted_by_type.find(tp);
  if (iter != d->m_shader_data_sorted_by_type.end() && i < iter->second.size())
    {
      return d->m_shader_data[iter->second[i]].m_source_code.c_str();
    }
  else
    {
      return "";
    }
}

fastuidraw::c_string
fastuidraw::gl::Program::
shader_compile_log(GLenum tp, unsigned int i) const
{
  ProgramPrivate *d;
  d = static_cast<ProgramPrivate*>(m_d);

  std::map<GLenum, std::vector<int> >::const_iterator iter;
  iter = d->m_shader_data_sorted_by_type.find(tp);
  if (iter != d->m_shader_data_sorted_by_type.end() && i < iter->second.size())
    {
      return d->m_shader_data[iter->second[i]].m_compile_log.c_str();
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
  d = static_cast<ProgramInitializerArrayPrivate*>(obj.m_d);
  m_d = FASTUIDRAWnew ProgramInitializerArrayPrivate(*d);
}


fastuidraw::gl::ProgramInitializerArray::
~ProgramInitializerArray()
{
  ProgramInitializerArrayPrivate *d;
  d = static_cast<ProgramInitializerArrayPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = nullptr;
}

void
fastuidraw::gl::ProgramInitializerArray::
swap(ProgramInitializerArray &obj)
{
  std::swap(m_d, obj.m_d);
}

fastuidraw::gl::ProgramInitializerArray&
fastuidraw::gl::ProgramInitializerArray::
operator=(const ProgramInitializerArray &rhs)
{
  if (this != &rhs)
    {
      ProgramInitializerArray v(rhs);
      swap(v);
    }
  return *this;
}

fastuidraw::gl::ProgramInitializerArray&
fastuidraw::gl::ProgramInitializerArray::
add(reference_counted_ptr<ProgramInitializer> h)
{
  ProgramInitializerArrayPrivate *d;
  d = static_cast<ProgramInitializerArrayPrivate*>(m_d);
  d->m_values.push_back(h);
  return *this;
}


void
fastuidraw::gl::ProgramInitializerArray::
perform_initializations(Program *pr, bool program_bound) const
{
  ProgramInitializerArrayPrivate *d;
  d = static_cast<ProgramInitializerArrayPrivate*>(m_d);
  for(const auto &v : d->m_values)
    {
      FASTUIDRAWassert(v);
      v->perform_initialization(pr, program_bound);
    }
}

void
fastuidraw::gl::ProgramInitializerArray::
clear(void)
{
  ProgramInitializerArrayPrivate *d;
  d = static_cast<ProgramInitializerArrayPrivate*>(m_d);
  d->m_values.clear();
}

//////////////////////////////////////////////////
// fastuidraw::gl::UniformBlockInitializer methods
fastuidraw::gl::UniformBlockInitializer::
UniformBlockInitializer(c_string uniform_name, int binding_point_index)
{
  m_d = FASTUIDRAWnew BlockInitializerPrivate(uniform_name, binding_point_index);
}

fastuidraw::gl::UniformBlockInitializer::
~UniformBlockInitializer()
{
  BlockInitializerPrivate *d;
  d = static_cast<BlockInitializerPrivate*>(m_d);
  FASTUIDRAWassert(d != nullptr);
  FASTUIDRAWdelete(d);
  m_d = nullptr;
}

void
fastuidraw::gl::UniformBlockInitializer::
perform_initialization(Program *pr, bool program_bound) const
{
  BlockInitializerPrivate *d;
  d = static_cast<BlockInitializerPrivate*>(m_d);
  FASTUIDRAWassert(d != nullptr);

  int loc;
  loc = glGetUniformBlockIndex(pr->name(), d->m_block_name.c_str());
  if (loc != -1)
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
  FASTUIDRAWunused(program_bound);
}


//////////////////////////////////////////////////
// fastuidraw::gl::ShaderStorageBlockInitializer methods
#ifndef FASTUIDRAW_GL_USE_GLES
fastuidraw::gl::ShaderStorageBlockInitializer::
ShaderStorageBlockInitializer(c_string uniform_name, int binding_point_index)
{
  m_d = FASTUIDRAWnew BlockInitializerPrivate(uniform_name, binding_point_index);
}

fastuidraw::gl::ShaderStorageBlockInitializer::
~ShaderStorageBlockInitializer()
{
  BlockInitializerPrivate *d;
  d = static_cast<BlockInitializerPrivate*>(m_d);
  FASTUIDRAWassert(d != nullptr);
  FASTUIDRAWdelete(d);
  m_d = nullptr;
}

void
fastuidraw::gl::ShaderStorageBlockInitializer::
perform_initialization(Program *pr, bool program_bound) const
{
  BlockInitializerPrivate *d;
  d = static_cast<BlockInitializerPrivate*>(m_d);
  FASTUIDRAWassert(d != nullptr);

  int loc;
  loc = glGetProgramResourceIndex(pr->name(), GL_SHADER_STORAGE_BLOCK, d->m_block_name.c_str());
  if (loc != -1)
    {
      glShaderStorageBlockBinding(pr->name(), loc, d->m_binding_point);
    }
  else
    {
      std::cerr << "Failed to find shader storage block \""
                << d->m_block_name
                << "\" in program " << pr->name()
                << " for initialization\n";
    }
  FASTUIDRAWunused(program_bound);
}
#endif


/////////////////////////////////////////////////
// fastuidraw::gl::UniformInitalizerBase methods
fastuidraw::gl::UniformInitalizerBase::
UniformInitalizerBase(c_string uniform_name)
{
  FASTUIDRAWassert(uniform_name);
  m_d = FASTUIDRAWnew std::string(uniform_name);
}

fastuidraw::gl::UniformInitalizerBase::
~UniformInitalizerBase()
{
  std::string *d;
  d = static_cast<std::string*>(m_d);
  FASTUIDRAWassert(d != nullptr);
  FASTUIDRAWdelete(d);
  m_d = nullptr;
}

void
fastuidraw::gl::UniformInitalizerBase::
perform_initialization(Program *pr, bool program_bound) const
{
  std::string *d;
  d = static_cast<std::string*>(m_d);
  FASTUIDRAWassert(d != nullptr);

  int loc;
  loc = pr->uniform_location(d->c_str());
  if (loc == -1)
    {
      loc = glGetUniformLocation(pr->name(), d->c_str());
      if (loc != -1)
        {
          std::cerr << "gl_program::uniform_location failed to find uniform, \""
                    << *d << "\"but glGetUniformLocation succeeded\n";
        }
    }

  if (loc != -1)
    {
      init_uniform(pr->name(), loc, program_bound);
    }
  else
    {
      std::cerr << "Failed to find uniform \"" << *d
                << "\" in program " << pr->name()
                << " for initialization\n";
    }
}
