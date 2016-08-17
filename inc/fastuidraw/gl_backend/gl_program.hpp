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

#include <fastuidraw/gl_backend/gl_header.hpp>
#include <fastuidraw/util/util.hpp>
#include <fastuidraw/util/vecN.hpp>
#include <fastuidraw/util/reference_counted.hpp>
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
    Enumeration to indiciate
    the source for a shader.
   */
  enum shader_source_type
    {
      /*!
        Shader source code is taken
        from the file whose name
        is the passed string.
       */
      from_file,

      /*!
        The passed string is the
        shader source code.
       */
      from_string,

      /*!
        The passed string is label
        for a string of text fetched
        with fastuidraw::fetch_static_resource()
       */
      from_resource,
    };

  /*!
    Enumeration to determine if source
    code or a macro
   */
  enum add_source_location_type
    {
      /*!
        add the source code or macro
        to the back.
       */
      push_back,

      /*!
        add the source code or macro
        to the front.
       */
      push_front
    };

  /*!
    Enumeration to indicate extension
    enable flags.
   */
  enum shader_extension_enable_type
    {
      /*!
        Requires the named GLSL extension,
        i.e. will add <B>"#extension extension_name: require"</B>
        to GLSL source code.
       */
      require_extension,
      /*!
        Enables the named GLSL extension,
        i.e. will add <B>"#extension extension_name: enable"</B>
        to GLSL source code.
       */
      enable_extension,
      /*!
        Enables the named GLSL extension,
        but request that the GLSL compiler
        issues warning when the extension
        is used, i.e. will add
        <B>"#extension extension_name: warn"</B>
        to GLSL source code.
       */
      warn_extension,
      /*!
        Disables the named GLSL extension,
        i.e. will add <B>"#extension extension_name: disable"</B>
        to GLSL source code.
       */
      disable_extension
    };

  /*!
    A shader_source represents the source code
    to a GLSL shader, specifying sets of source
    code and macros to use.
   */
  class shader_source
  {
  public:
    /*!
      Ctor.
    */
    shader_source(void);

    /*!
      Copy ctor.
      \param obj value from which to copy
    */
    shader_source(const shader_source &obj);

    ~shader_source();

    /*!
      Assignment operator.
      \param obj value from which to copy
    */
    shader_source&
    operator=(const shader_source &obj);

    /*!
      Specifies the version of GLSL to which to
      declare the shader. An empty string indicates
      to not have a "#version" directive in the shader.
     */
    shader_source&
    specify_version(const char *v);

    /*!
      Add shader source code to this shader_source.
      \param str string that is a filename, GLSL source or a resource name
      \param tp interpretation of str, i.e. determines if
                str is a filename, raw GLSL source or a resource
      \param loc location to add source
     */
    shader_source&
    add_source(const char *str, enum shader_source_type tp = from_file,
               enum add_source_location_type loc = push_back);

    /*!
      Add the sources from another shader_source object.
      \param obj shader_source object from which to absorb
     */
    shader_source&
    add_source(const shader_source &obj);

    /*!
      Add a macro to this shader_source.
      Functionally, will insert \#define macro_name macro_value
      in the GLSL source code.
      \param macro_name name of macro
      \param macro_value value to which macro is given
      \param loc location to add macro within code
     */
    shader_source&
    add_macro(const char *macro_name, const char *macro_value = "",
              enum add_source_location_type loc = push_back);

    /*!
      Add a macro to this shader_source.
      Functionally, will insert \#define macro_name macro_value
      in the GLSL source code.
      \param macro_name name of macro
      \param macro_value value to which macro is given
      \param loc location to add macro within code
     */
    shader_source&
    add_macro(const char *macro_name, uint32_t macro_value,
              enum add_source_location_type loc = push_back);

    /*!
      Add a macro to this shader_source.
      Functionally, will insert \#define macro_name macro_value
      in the GLSL source code.
      \param macro_name name of macro
      \param macro_value value to which macro is given
      \param loc location to add macro within code
     */
    shader_source&
    add_macro(const char *macro_name, int32_t macro_value,
              enum add_source_location_type loc = push_back);

    /*!
      Adds the string
      \code
      #undef X
      \endcode
      where X is the passed macro name
      \param macro_name name of macro
     */
    shader_source&
    remove_macro(const char *macro_name);

    /*!
      Specifiy an extension and usage.
      \param ext_name name of GL extension
      \param tp usage of extension
     */
    shader_source&
    specify_extension(const char *ext_name,
                      enum shader_extension_enable_type tp = enable_extension);

    /*!
      Returns the GLSL code assembled. The returned string is only
      gauranteed to be valid up until the shader_source object
      is modified.
     */
    const char*
    assembled_code(void) const;

  private:
    void *m_d;
  };

  /*!
    Ctor. Construct a Shader.
    \param src GLSL source code of the shader
    \param pshader_type type of shader, i.e. GL_VERTEX_SHADER
                        for a vertex shader, etc.
   */
  Shader(const shader_source &src, GLenum pshader_type);

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
    To be implemented by a derived class to
    perform additional one-time actions.
    Function is called the first time the
    GLSL program of the Program is
    bound.
    \param pr Program to initialize
   */
  virtual
  void
  perform_initialization(Program *pr) const = 0;
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
  perform_initialization(Program *pr) const;

protected:

  /*!
    To be implemented by a derived class to make the GL call
    to initialize a uniform in a GLSL shader.
    \param location lcoation of uniform
   */
  virtual
  void
  init_uniform(GLint location) const = 0;

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
  init_uniform(GLint location) const
  {
    Uniform(location, m_value);
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
    \param uniform_name name of uniform in GLSL to initialize
    \param binding_point_index value with which to set the uniform
   */
  UniformBlockInitializer(const char *uniform_name, int binding_point_index);

  ~UniformBlockInitializer();

  virtual
  void
  perform_initialization(Program *pr) const;

private:
  void *m_d;
};

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
  perform_initializations(Program *pr) const;

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
    A parameter_info holds the type,
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
  class parameter_info
  {
  public:
    /*!
      Ctor
     */
    parameter_info(void);

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
      GL API index for the parameter, NOT the
      ID of the parameter as used in uniform_parameter
      or attribute_parameter. The value of index()
      is used in calls to GL to query about
      the parameter, such as glGetActiveUniform
      and glGetActiveUniformsiv.
    */
    GLuint
    index(void) const;

    /*!
      "Location" of the uniform or attribute
      as returned by glGetUniformLocation
      or glGetAttriblocation
     */
    GLint
    location(void) const;

  private:
    explicit
    parameter_info(void*);

    void *m_d;
    friend class Program;
  };

  /*!
    Ctor.
    \param pshaders shaders used to create the Program
    \param action specifies actions to perform before linking of the Program
    \param initers one-time initialization actions to perform the first time the
                   Program is used
   */
  Program(const_c_array<reference_counted_ptr<Shader> > pshaders,
          const PreLinkActionArray &action=PreLinkActionArray(),
          const ProgramInitializerArray &initers=ProgramInitializerArray());

  /*!
    Ctor.
    \param vert_shader pointer to vertex shader to use for the Program
    \param frag_shader pointer to fragment shader to use for the Program
    \param action specifies actions to perform before and
                  after linking of the Program.
    \param initers one-time initialization actions to perform the first time the
                   Program is used
   */
  Program(reference_counted_ptr<Shader> vert_shader,
          reference_counted_ptr<Shader> frag_shader,
          const PreLinkActionArray &action=PreLinkActionArray(),
          const ProgramInitializerArray &initers=ProgramInitializerArray());

  /*!
    Ctor.
    \param vert_shader pointer to vertex shader to use for the Program
    \param frag_shader pointer to fragment shader to use for the Program
    \param action specifies actions to perform before and
                  after linking of the Program.
    \param initers one-time initialization actions to perform the first time the
                   Program is used
   */
  Program(const Shader::shader_source &vert_shader,
          const Shader::shader_source &frag_shader,
          const PreLinkActionArray &action=PreLinkActionArray(),
          const ProgramInitializerArray &initers=ProgramInitializerArray());


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
    alphabetical order of parameter_info::name().
    \param I -array index- (not location) of uniform, I must be less
           that number_active_uniforms()
   */
  parameter_info
  active_uniform(unsigned int I);

  /*!
    Searches active_uniform() to find the named uniform, including
    named elements of an array. This function should only be called
    either after use_program() has been called or only when the GL
    context is current. Returns value -1 indicated that the uniform
    could not be found.
    \param uniform_name name of uniform to find
   */
  GLint
  uniform_location(const char *uniform_name);

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
    alphabetical order of parameter_info::name().
    \param I -array index- (not location) of attribute, I must be less
           that number_active_attributes()
   */
  parameter_info
  active_attribute(unsigned int I);

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
