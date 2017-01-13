/*!
 * \file gl_program.hpp
 * \brief file gl_program.hpp
 *
 * Adapted from: WRATHGLProgram.hpp of WRATH:
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
 *
 */


#pragma once

#include <fastuidraw/util/util.hpp>
#include <fastuidraw/util/vecN.hpp>
#include <fastuidraw/util/reference_counted.hpp>
#include <fastuidraw/glsl/shader_source.hpp>
#include <fastuidraw/gl_backend/gl_header.hpp>
#include <fastuidraw/gl_backend/gluniform.hpp>

namespace fastuidraw {
namespace gl {

/*!\addtogroup GLUtility
  @{
 */


/*!
  Simple Shader utility class, providing a simple interface to build
  shader source code from mutliple files, resources and strings. In
  addition the actual GL object creation is defferred to later, in
  doing so, one can create Shader objects from outside the main GL
  thread. Each of the following commands
    - compile_success()
    - compile_log()
    - name()

  triggers the GL commands to compile the shader if the shader has
  not been yet attmpeted to compile. Hence one may only call these from
  outside the rendering thread if shader_ready() returns true. Moreover,
  a Shader may only be delete from the GL rendering thread.

  If the shader source is from a file (Shader::from_file),
  the file will be taken from the current working directory. For
  string sources (Shader::from_file) and resource sources
  (Shader::from_resource), the shader source is taken from
  a resource.
*/
class Shader:
  public reference_counted<Shader>::default_base
{
public:
  /*!
    Ctor. Construct a Shader.
    \param src GLSL source code of the shader
    \param pshader_type type of shader, i.e. GL_VERTEX_SHADER
                        for a vertex shader, etc.
   */
  Shader(const glsl::ShaderSource &src, GLenum pshader_type);

  ~Shader();

  /*!
    The actual GL shader is NOT built at constructor,
    rather it is built if any of
    - compile_success()
    - compile_log()
    - name()

    are called. This way, one can construct Shader
    objects from outside the GL thread. The functions
    return true if and only if the shader has been built.
   */
  bool
  shader_ready(void);

  /*!
    Returns the GLSL source string fed to GL
    to create the GLSL shader.
   */
  const char*
  source_code(void);

  /*!
    Returns the GLSL compile log of
    the GLSL source code.
    If the shader source has not yet
    been sent to GL for compiling, will
    trigger those commands. Hence, should
    only be called from the GL rendering
    thread or if shader_ready() returns true.
   */
  const char*
  compile_log(void);

  /*!
    Returns true if and only if GL
    successfully compiled the shader.
    If the shader source has not yet
    been sent to GL for compiling, will
    trigger those commands. Hence, should
    only be called from the GL rendering
    thread or if shader_ready() returns true.
   */
  bool
  compile_success(void);

  /*!
    Returns the GL name (i.e. ID assigned by GL)
    of this Shader.
    If the shader source has not yet
    been sent to GL for compiling, will
    trigger those commands. Hence, should
    only be called from the GL rendering
    thread or if shader_ready() returns true.
   */
  GLuint
  name(void);

  /*!
    Returns the shader type of this
    Shader as set by it's constructor.
   */
  GLenum
  shader_type(void);

  /*!
    Provided as a conveniance to return a string
    from a GL enumeration naming a shader type.
    For example <B>GL_VERTEX_SHADER</B> will
    return the string "GL_VERTEX_SHADER".
    Unreconized shader types will return the
    label "UNKNOWN_SHADER_STAGE".
   */
  static
  const char*
  gl_shader_type_label(GLenum ptype);

  /*!
    Returns the default shader version to feed to
    \ref glsl::ShaderSource::specify_version() to
    match with the GL API. If GL backend, then
    gives "330". If GLES backend, then gives "300 es".
   */
  static
  const char*
  default_shader_version(void);

private:
  void *m_d;
};



/*!
  A PreLinkAction is an action to apply to a Program
  after attaching shaders but before linking.
 */
class PreLinkAction:
  public reference_counted<PreLinkAction>::default_base
{
public:
  virtual
  ~PreLinkAction()
  {}

  /*!
    To be implemented by a derived class to perform an action _before_ the
    GLSL program is linked. Default implementation does nothing.
    \param glsl_program GL name of GLSL program on which to perform the action.
   */
  virtual
  void
  action(GLuint glsl_program) const = 0;
};


/*!
  A BindAttribute inherits from PreLinkAction,
  it's purpose is to bind named attributes to named
  locations, i.e. it calls glBindAttributeLocation().
 */
class BindAttribute:public PreLinkAction
{
public:
  /*!
    Ctor.
    \param pname name of attribute in GLSL code
    \param plocation location to which to place the attribute
   */
  BindAttribute(const char *pname, int plocation);

  ~BindAttribute();

  virtual
  void
  action(GLuint glsl_program) const;

private:
  void *m_d;
};

/*!
  A ProgramSeparable inherits from PreLinkAction,
  its purpose is to set a GLSL program as separable,
  so that it can be used by a GLSL pipeline.
  Using a ProgramSeparable requires:
  - for GLES: GLES3.0 or higher
  - for GL: either GL version 4.1 or the extension GL_ARB_separate_shader_objects
 */
class ProgramSeparable:public PreLinkAction
{
public:
  virtual
  void
  action(GLuint glsl_program) const;
};

/*!
  A PreLinkActionArray is a conveniance class
  wrapper over an array of PreLinkAction handles.
 */
class PreLinkActionArray
{
public:
  /*!
    Ctor.
  */
  PreLinkActionArray(void);

  /*!
    Copy ctor.
    \param obj value from which to copy
  */
  PreLinkActionArray(const PreLinkActionArray &obj);

  ~PreLinkActionArray();

  /*!
    Assignment operator.
    \param rhs value from which to copy.
   */
  PreLinkActionArray&
  operator=(const PreLinkActionArray &rhs);

  /*!
    Add a prelink action to execute.
    \param h handle to action to add
   */
  PreLinkActionArray&
  add(reference_counted_ptr<PreLinkAction> h);

  /*!
    Provided as a conveniance, equivalent to
    \code
    add(new BindAttribute(pname, plocation))
    \endcode
    \param pname name of the attribute
    \param plocation location to which to bind the attribute.
   */
  PreLinkActionArray&
  add_binding(const char *pname, int plocation)
  {
    reference_counted_ptr<PreLinkAction> h(FASTUIDRAWnew BindAttribute(pname, plocation));
    return add(h);
  }

  /*!
    Executes PreLinkAction::action() for each of those
    actions added via add().
   */
  void
  execute_actions(GLuint glsl_program) const;

private:
  void *m_d;
};

class Program;

/*!
  A ProgramInitializer is a functor object called the first time
  a Program is bound (i.e. the first
  time Program::use_program() is called).
  It's main purpose is to facilitate initializing
  uniform values.
 */
class ProgramInitializer:
  public reference_counted<ProgramInitializer>::default_base
{
public:
  virtual
  ~ProgramInitializer()
  {}

  /*!
    To be implemented by a derived class to perform additional
    one-time actions. Function is called after the GL program
    object is successfully linked.
    \param pr Program to initialize
    \param program_bound GLSL Program is already bound, the program is
                         NOT bound if the GL/GLES API supports seperable
                         program objects.
   */
  virtual
  void
  perform_initialization(Program *pr, bool program_bound) const = 0;
};

/*!
  A UniformInitalizerBase is a base class for
  initializing a uniform, the actual GL call to
  set the uniform value is to be implemented by
  a derived class in init_uniform().
 */
class UniformInitalizerBase:public ProgramInitializer
{
public:
  /*!
    Ctor.
    \param uniform_name name of uniform to initialize
   */
  UniformInitalizerBase(const char *uniform_name);

  ~UniformInitalizerBase();

  virtual
  void
  perform_initialization(Program *pr, bool program_bound) const;

protected:

  /*!
    To be implemented by a derived class to make the GL call
    to initialize a uniform in a GLSL shader.
    \param location lcoation of uniform
   */
  virtual
  void
  init_uniform(GLuint program, GLint location, bool program_bound) const = 0;

private:
  void *m_d;
};

/*!
  Initialize a uniform via the templated
  overloaded function fastuidraw::gl::Uniform().
 */
template<typename T>
class UniformInitializer:public UniformInitalizerBase
{
public:
  /*!
    Ctor.
    \param uniform_name name of uniform in GLSL to initialize
    \param value value with which to set the uniform
   */
  UniformInitializer(const char *uniform_name, const T &value):
    UniformInitalizerBase(uniform_name),
    m_value(value)
  {}

protected:

  virtual
  void
  init_uniform(GLuint program, GLint location, bool program_bound) const
  {
    if(program_bound)
      {
        Uniform(location, m_value);
      }
    else
      {
        ProgramUniform(program, location, m_value);
      }
  }

private:
  T m_value;
};

/*!
  Conveniance typedef to initialize samplers.
 */
typedef UniformInitializer<int> SamplerInitializer;

/*!
  A UniformBlockInitializer is used to initalize the binding point
  used by a bindable uniform (aka Uniform buffer object, see the
  GL spec on glGetUniformBlockIndex and glUniformBlockBinding.
 */
class UniformBlockInitializer:public ProgramInitializer
{
public:
  /*!
    Ctor.
    \param name name of uniform block in GLSL to initialize
    \param binding_point_index value with which to set the uniform
   */
  UniformBlockInitializer(const char *name, int binding_point_index);

  ~UniformBlockInitializer();

  virtual
  void
  perform_initialization(Program *pr, bool program_bound) const;

private:
  void *m_d;
};

#ifndef FASTUIDRAW_GL_USE_GLES
/*!
  A ShaderStorageBlockInitializer is used to initalize the binding point
  used by a shader storage block (see the GL spec on
  glShaderStorageBlockBinding). Initializer is not supported
  in OpenGL ES.
 */
class ShaderStorageBlockInitializer:public ProgramInitializer
{
public:
  /*!
    Ctor.
    \param name name of shader storage block in GLSL to initialize
    \param binding_point_index value with which to set the uniform
   */
  ShaderStorageBlockInitializer(const char *name, int binding_point_index);

  ~ShaderStorageBlockInitializer();

  virtual
  void
  perform_initialization(Program *pr, bool program_bound) const;

private:
  void *m_d;
};

#endif

/*!
  Conveniance class to hold an array of handles
  of ProgramInitializer objects
 */
class ProgramInitializerArray
{
public:
  /*!
    Ctor.
  */
  ProgramInitializerArray(void);

  /*!
    Copy ctor.
    \param obj value from which to copy
  */
  ProgramInitializerArray(const ProgramInitializerArray &obj);

  ~ProgramInitializerArray();

  /*!
    Assignment operator.
    \param rhs value from which to copy.
   */
  ProgramInitializerArray&
  operator=(const ProgramInitializerArray &rhs);

  /*!
    Add an initializer
    \param h handle to initializer to add
   */
  ProgramInitializerArray&
  add(reference_counted_ptr<ProgramInitializer> h);

  /*!
    Provided as a conveniance, creates a UniformInitializer
    object and adds that via add().
    \param uniform_name name of uniform in GLSL to initialize
    \param value value with which to set the uniform
   */
  template<typename T>
  ProgramInitializerArray&
  add_uniform_initializer(const char *uniform_name, const T &value)
  {
    return add(FASTUIDRAWnew UniformInitializer<T>(uniform_name, value));
  }

  /*!
    Provided as a conveniance, creates a SamplerInitializer
    object and adds that via add().
    \param uniform_name name of uniform in GLSL to initialize
    \param value value with which to set the uniform, in this
                 case specifies the texture unit as follows:
                 a value of n means to use GL_TEXTUREn texture
                 unit.
  */
  ProgramInitializerArray&
  add_sampler_initializer(const char *uniform_name, int value)
  {
    return add(FASTUIDRAWnew SamplerInitializer(uniform_name, value));
  }

  /*!
    Provided as a conveniance, creates a UniformBlockInitializer
    object and adds that via add().
    \param uniform_name name of uniform in GLSL to initialize
    \param value value with which to set the uniform, in this
                 case specifies the binding point index to
                 pass to glBindBufferBase or glBindBufferRange.
  */
  ProgramInitializerArray&
  add_uniform_block_binding(const char *uniform_name, int value)
  {
    return add(FASTUIDRAWnew UniformBlockInitializer(uniform_name, value));
  }

  /*!
    For each objected added via add(), call
    ProgramInitializer::perform_initialization().
    \param pr Program to pass along
   */
  void
  perform_initializations(Program *pr, bool program_bound) const;

  /*!
    Clear all elements that have been added via add().
   */
  void
  clear(void);

private:
  void *m_d;
};

/*!
  Class for creating and using GLSL programs.
  A Program delays the GL commands to
  create the actual GL program until the first time
  it is bound with use_program(). In addition to
  proving the GL code to create the GLSL code,
  Program also provides queries GL for all
  active uniforms and attributes (see active_attributes(),
  active_uniforms(), find_uniform() and find_attribute()).
  Also, provides an interface so that a sequence of
  GL commands are executed the first time it is bound
  and also an interface so a sequence of actions is
  executed every time it is bound.
  Program's are considered a resource,
  as such have a resource manager.
 */
class Program:
  public reference_counted<Program>::default_base
{
public:
  /*!
    A shader_variable_info holds the type,
    size and name of a uniform or an attribute
    of a GL program. This data is fetched from GL
    via glGetActiveAttrib/glGetAttribLocation
    for attributes and glGetActiveUniform/glGetUniformLocation
    for uniforms. Note that, depending on the GL
    implementation, arrays may or may
    not be listed with an appended '[0]' and
    that usually elements of an array are
    NOT listed individually.
  */
  class shader_variable_info
  {
  public:
    /*!
      Ctor
     */
    shader_variable_info(void);

    /*!
      Name of the parameter within
      the GL API.
    */
    const char*
    name(void) const;

    /*!
      GL enumeration stating the
      parameter's type.
    */
    GLenum
    type(void) const;

    /*!
      If parameter is an array, holds
      the legnth of the array, otherwise
      is 1.
    */
    GLint
    count(void) const;

    /*!
      GL API index for the parameter. The value of index()
      is used in calls to GL to query about the parameter,
      such as glGetActiveUniform and glGetActiveUniformsiv.
    */
    GLuint
    index(void) const;

    /*!
      "Location" of the uniform or attribute
      as returned by glGetUniformLocation
      or glGetAttribLocation. For members of
      a uniform block or a shader storage
      buffer, value is -1.
     */
    GLint
    location(void) const;

    /*!
      Returns the index to what uniform block this
      belongs. If this value does not reside on a
      uniform block, returns -1. The index is the
      value to feed as bufferIndex in the GL API
      function's
      \code
      glGetProgramResourceiv(program, GL_UNIFORM_BLOCK, bufferIndex, ...)
      glGetProgramResourceName(program, GL_UNIFORM_BLOCK, bufferIndex, ...)
      glGetActiveUniformBlockiv(program, bufferIndex, ..)
      glGetActiveUniformBlockName(program, bufferIndex, ...)
      glUniformBlockBinding(program, bufferIndex, ...)
      \endcode
     */
    GLint
    ubo_index(void) const;

    /*!
      Returns the offset into a backing buffer object
      on which the this is sourced (or written to).
      For uniforms of the default uniform block and
      for attributes, returns -1.
     */
    GLint
    buffer_offset(void) const;

    /*!
      If this is an array, not an attribute or uniform
      of the default uniform block, then returns the stride,
      in bytes, between elements of the array. Otherwise
      returns -1.
     */
    GLint
    array_stride(void) const;

    /*!
      Returns -1 if this is not an array of matrices
      from a uniform block that is not the default block.
      If not, then returns the stride between columns
      for column major matrixes and otherwise returns
      the stride between row major matrices.
     */
    GLint
    matrix_stride(void) const;

    /*!
      If this a matrix from not the default uniform block,
      return true if the matrix is row major. Otherwise
      returns false.
     */
    bool
    is_row_major(void) const;

    /*!
      If this is an atomic counter, returns the index of atomic
      buffer that the counter is associated with. If not, returns
      -1. The index is the value to feed as bufferIndex in the
      GL API function's
      \code
      glGetProgramResourceiv(program, GL_ATOMIC_COUNTER_BUFFER, bufferIndex, ...)
      glGetActiveAtomicCounterBufferiv(GLint program, GLuint bufferIndex, ..)
      \endcode
     */
    GLint
    abo_index(void) const;

    /*!
      If this variable is a member of a shader storage buffer, returns
      to what shader storage buffer block this belongs. If not a shader
      storage buffer variable, returns -1. This value is used for
      bufferIndex in the GL API routines
      \code
      glGetProgramResourceiv(program, GL_SHADER_STORAGE_BUFFER, bufferIndex, ...)
      glGetProgramResourceName(program, GL_SHADER_STORAGE_BUFFER, bufferIndex, ...)
      glShaderStorageBlockBinding(program, bufferIndex, ...)
      \endcode
     */
    GLint
    shader_storage_buffer_index(void) const;

    /*!
      If this variable has shader_storage_buffer_index() return -1, then
      returns -1. Otherwise returns the size of the top level
      array to which the variable belongs. If the top level
      array is unsized returns 0.
     */
    GLint
    shader_storage_buffer_top_level_array_size(void) const;

    /*!
      If this variable has shader_storage_buffer_index() return -1, then
      returns -1. Otherwise returns the stride of the top level
      array to which the variable belongs. If it does belong
      to a top level array, returns 0.
     */
    GLint
    shader_storage_buffer_top_level_array_stride(void) const;

  private:
    explicit
    shader_variable_info(const void*);

    const void *m_d;
    friend class Program;
  };

  /*!
    A block_info represents an object from which
    one can query the members of a uniform or
    shader storage block.
   */
  class block_info
  {
  public:
    /*!
      Ctor
     */
    block_info(void);

    /*!
      Name of the parameter within
      the GL API.
    */
    const char*
    name(void) const;

    /*!
      GL API index for the parameter. The value of
      block_index() is used in calls to GL to query about
      the parameter. The default uniform block will
      have this value as -1.
     */
    GLint
    block_index(void) const;

    /*!
      Returns the size in bytes of the uniform block (i.e.
      the size needed for a buffer object to correctly back
      the uniform block). The default uniform block will
      have the size as 0.
     */
    GLint
    buffer_size(void) const;

    /*!
      Returns the number of active variables of the block.
      Note that an array of is classified as a single
      variable.
    */
    unsigned int
    number_variables(void) const;

    /*!
      Returns the ID'd variable. The values are sorted in
      alphabetical order of shader_variable_info::name(). The
      variable list is for those variables of this block.
      \param I -ID- (not location) of variable, if I is
               not less than number_variables(), returns
               a shader_variable_info indicating no value (i.e.
               shader_variable_info::name() is an empty string and
               shader_variable_info::index() is -1).
     */
    shader_variable_info
    variable(unsigned int I);

    /*!
      Returns the ID'd value to feed to variable() to
      get the variable of the given name. If the variable
      cannot be found, returns ~0u.
     */
    unsigned int
    variable_id(const char *name);

    /*!
      Returns the offset into the buffer for the location
      of the variable. The name can end in a non-zero array
      index and the returned offset will give the location
      of the named element of the array. If the element is
      not in the block, returns -1.
     */
    int
    variable_offset(const char *name);

  private:
    explicit
    block_info(const void*);

    const void *m_d;
    friend class Program;
  };

  /*!
    A block_info represents an object from which
    one can query teh data of an atomic buffer.
   */
  class atomic_buffer_info
  {
  public:
    /*!
      Ctor
     */
    atomic_buffer_info(void);

    /*!
      GL API index for the atomic buffer. The value of
      buffer_index() is used in calls to GL, i.e. the
      value to feed as bufferIndex to the GL API functions:
      \code
      glGetActiveAtomicCounterBufferiv(GLuint program, GLuint bufferIndex, ...)
      \endcode
     */
    GLint
    buffer_index(void) const;

    /*!
      The GL API index for the binding point of the atomic
      buffer, i.e. the value to feed as bufferIndex to the
      GL API functions:
      \code
      glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, bufferIndex, ...)
      glBindBufferRange(GL_ATOMIC_COUNTER_BUFFER, bufferIndex, ..)
      \endcode
     */
    GLint
    buffer_binding(void) const;

    /*!
      Returns the size in bytes of the atomic buffer (i.e.
      the size needed for a buffer object to correctly back
      the atomic buffer block).
     */
    GLint
    buffer_size(void) const;

    /*!
      Returns the number of atomic -variables- of the
      atomic buffer. Note that an array is classified
      as a single variable.
    */
    unsigned int
    number_atomic_variables(void) const;

    /*!
      Returns the ID'd atomic variable. The values are sorted in
      alphabetical order of shader_variable_info::name(). The variable
      list is for those atomic variables of this atomic buffer
      \param I -array index- (not location) of atomic, if I is
               not less than number_atomic_variables(), returns
               a shader_variable_info indicating nothing (i.e.
               shader_variable_info::name() is an empty string and
               shader_variable_info::index() is -1).
     */
    shader_variable_info
    atomic_variable(unsigned int I);

    /*!
      Returns the ID value to feed to atomic_variable()
      to get the atomic variable of the given name. If the
      variable cannot be found, returns ~0u.
     */
    unsigned int
    atomic_variable_id(const char *name);

  private:
    explicit
    atomic_buffer_info(const void*);

    const void *m_d;
    friend class Program;
  };

  /*!
    Ctor.
    \param pshaders shaders used to create the Program
    \param action specifies actions to perform before linking of the Program
    \param initers one-time initialization actions to perform at GLSL
                   program creation
   */
  Program(const_c_array<reference_counted_ptr<Shader> > pshaders,
          const PreLinkActionArray &action = PreLinkActionArray(),
          const ProgramInitializerArray &initers = ProgramInitializerArray());

  /*!
    Ctor.
    \param vert_shader pointer to vertex shader to use for the Program
    \param frag_shader pointer to fragment shader to use for the Program
    \param action specifies actions to perform before and
                  after linking of the Program.
    \param initers one-time initialization actions to perform at GLSL
                   program creation
   */
  Program(reference_counted_ptr<Shader> vert_shader,
          reference_counted_ptr<Shader> frag_shader,
          const PreLinkActionArray &action = PreLinkActionArray(),
          const ProgramInitializerArray &initers = ProgramInitializerArray());

  /*!
    Ctor.
    \param vert_shader pointer to vertex shader to use for the Program
    \param frag_shader pointer to fragment shader to use for the Program
    \param action specifies actions to perform before and
                  after linking of the Program.
    \param initers one-time initialization actions to perform at GLSL
                   program creation
   */
  Program(const glsl::ShaderSource &vert_shader,
          const glsl::ShaderSource &frag_shader,
          const PreLinkActionArray &action = PreLinkActionArray(),
          const ProgramInitializerArray &initers = ProgramInitializerArray());


  ~Program(void);

  /*!
    Call to set GL to use the GLSLProgram
    of this Program. The GL context
    must be current.
   */
  void
  use_program(void);

  /*!
    Returns the GL name (i.e. ID assigned by GL,
    for use in glUseProgram) of this Program.
    This function should
    only be called either after use_program() has
    been called or only when the GL context is
    current.
  */
  GLuint
  name(void);

  /*!
    Returns the link log of this Program,
    essentially the value returned by
    glGetProgramInfoLog. This function should
    only be called either after use_program() has
    been called or only when the GL context is
    current.
   */
  const char*
  link_log(void);

  /*!
    Returns how many seconds it took for the program
    to be assembled and linked.
   */
  float
  program_build_time(void);

  /*!
    Returns true if and only if this Program
    successfully linked. This function should
    only be called either after use_program() has
    been called or only when the GL context is
    current.
   */
  bool
  link_success(void);

  /*!
    Returns the full log (including shader source
    code and link_log()) of this Program.
    This function should only be called either after
    use_program() has been called or only when the GL
    context is current.
   */
  const char*
  log(void);

  /*!
    Returns the number of active uniforms. Note that An array
    of uniforms is classified as a single uniform. This function
    should only be called either after use_program() has been called
    or only when the GL context is current.
   */
  unsigned int
  number_active_uniforms(void);

  /*!
    Returns the indexed uniform. This function should only be
    called either after use_program() has been called or only
    when the GL context is current. The values are sorted in
    alphabetical order of shader_variable_info::name(). The uniform
    list includes both uniforms from the default block and the
    uniforms of block sourced from buffer objects.
    \param I -array index- (not location) of uniform, if I is
             not less than number_active_uniforms(), returns
             a shader_variable_info indicating no uniform (i.e.
             shader_variable_info::name() is an empty string and
             shader_variable_info::index() is -1).
   */
  shader_variable_info
  active_uniform(unsigned int I);

  /*!
    Returns the ID value to feed to active_uniform() to
    get the uniform of the given name. This function should
    only be called either after use_program() has been called
    or only when the GL context is current. If the uniform
    cannot be found, returns ~0u.
    \param name name of uniform to find.
   */
  unsigned int
  active_uniform_id(const char *name);

  /*!
    Searches active_uniform() to find the named uniform, including
    named elements of an array. This function should only be called
    either after use_program() has been called or only when the GL
    context is current. Returns value -1 indicates that a uniform
    with that could not be found on the default uniform block.
    \param uniform_name name of uniform to find
   */
  GLint
  uniform_location(const char *uniform_name);

  /*!
    Returns a \ref block_info of the default uniform block.
   */
  block_info
  default_uniform_block(void);

  /*!
    Returns the number of active uniform blocks (not
    including the default uniform block). This function should
    only be called either after use_program() has been called
    or only when the GL context is current.
   */
  unsigned int
  number_active_uniform_blocks(void);

  /*!
    Returns the indexed uniform block. The values are sorted in
    alphabetical order of block_info::name(). This
    function should only be called either after use_program()
    has been called or only when the GL context is current.
    \param I which one to fetch, I must be less than
             number_active_uniform_blocks()
   */
  block_info
  uniform_block(unsigned int I);

  /*!
    Searches uniform_block() to find the named uniform block.
    Returns value -1 indicated that the uniform block could
    not be found. This function should only be called either
    after use_program() has been called or only when the GL
    context is current.
    \param uniform_block_name name of uniform to find
   */
  unsigned int
  uniform_block_id(const char *uniform_block_name);

  /*!
    Returns the number of active shader storage blocks.
    This function should only be called either after
    use_program() has been called or only when the GL context
    is current.
   */
  unsigned int
  number_active_shader_storage_blocks(void);

  /*!
    Returns the indexed shader_storage block. The values are
    sorted in alphabetical order of block_info::name(). This
    function should only be called either after use_program()
    has been called or only when the GL context is current.
    \param I which one to fetch, I must be less than
             number_active_shader_storage_blocks()
   */
  block_info
  shader_storage_block(unsigned int I);

  /*!
    Searches shader_storage_block() to find the named shader_storage block.
    Returns value -1 indicated that the shader_storage block could
    not be found. This function should only be called either
    after use_program() has been called or only when the GL
    context is current.
    \param shader_storage_block_name name of shader storage to find
   */
  unsigned int
  shader_storage_block_id(const char *shader_storage_block_name);

  /*!
    Returns the number of active atomic buffers. This function
    should only be called either after use_program() has been
    called or only when the GL context is current.
   */
  unsigned int
  number_active_atomic_buffers(void);

  /*!
    Returns the indexed atomic buffer. This function should only
    be called either after use_program() has been called or only
    when the GL context is current.
    \param I which one to fetch, I must be less than
             number_active_atomic_buffers()
   */
  atomic_buffer_info
  atomic_buffer(unsigned int I);

  /*!
    Returns the number of active attributes. Note that an array
    of uniforms is classified as a single uniform. This function
    should only be called either after use_program() has been called
    or only when the GL context is current.
   */
  unsigned int
  number_active_attributes(void);

  /*!
    Returns the indexed attribute. This function should only be
    called either after use_program() has been called or only
    when the GL context is current. The values are sorted in
    alphabetical order of shader_variable_info::name().
    \param I -ID- (not location) of attribute, if I is
             not less than number_active_attributes(), returns
             a shader_variable_info indicating no attribute (i.e.
             shader_variable_info::name() is an empty string and
             shader_variable_info::index() is -1).
   */
  shader_variable_info
  active_attribute(unsigned int I);

  /*!
    Returns the ID value to feed to active_attribute() to
    get the attribute of the given name. This function should
    only be called either after use_program() has been called
    or only when the GL context is current. If the attribute
    cannot be found, returns ~0u.
    \param name name of attribute to find.
   */
  unsigned int
  active_attribute_id(const char *name);

  /*!
    Searches active_attribute() to find the named attribute, including
    named elements of an array. This function should only be called
    either after use_program() has been called or only when the GL
    context is current. Returns value -1 indicated that the uniform
    could not be found.
    \param attribute_name name of attribute to find
   */
  GLint
  attribute_location(const char *attribute_name);

  /*!
    Returns the number of shaders of a given type attached to
    the Program.
    \param tp GL enumeration of the shader type, see Shader::shader_type()
   */
  unsigned int
  num_shaders(GLenum tp) const;

  /*!
    Returns the source code string for a shader attached to
    the Program.
    \param tp GL enumeration of the shader type, see Shader::shader_type()
    \param i which shader with 0 <= i < num_shaders(tp)
   */
  const char*
  shader_src_code(GLenum tp, unsigned int i) const;

private:
  void *m_d;
};
/*! @} */


} //namespace gl
} //namespace fastuidraw
