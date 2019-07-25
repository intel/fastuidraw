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
 * \author Kevin Rogovin <kevin.rogovin@gmail.com>
 *
 */


#ifndef FASTUIDRAW_GL_PROGRAM_HPP
#define FASTUIDRAW_GL_PROGRAM_HPP

#include <fastuidraw/util/util.hpp>
#include <fastuidraw/util/vecN.hpp>
#include <fastuidraw/util/reference_counted.hpp>
#include <fastuidraw/util/string_array.hpp>
#include <fastuidraw/glsl/shader_source.hpp>
#include <fastuidraw/gl_backend/gl_header.hpp>
#include <fastuidraw/gl_backend/gluniform.hpp>

namespace fastuidraw {
namespace gl {

/*!\addtogroup GLUtility
 * @{
 */


/*!
 * \brief
 * Simple Shader utility class providing a simple interface to build
 * GL shader objects using a glsl::ShaderSouce as its source code.
 *
 * The actual GL object creation is defferred to later, in
 * doing so, one can create Shader objects from outside the main GL
 * thread. Each of the following commands
 *   - compile_success()
 *   - compile_log()
 *   - name()
 *
 * triggers the GL commands to compile the shader if the shader has
 * not been yet attempeted to be compiled. Hence one may only call
 * these from outside the rendering thread if shader_ready() returns
 * true. Moreover, a Shader may only be deleted from the GL rendering
 * thread.
 */
class Shader:
  public reference_counted<Shader>::concurrent
{
public:
  /*!
   * Ctor. Construct a Shader.
   * \param src GLSL source code of the shader
   * \param pshader_type type of shader, i.e. GL_VERTEX_SHADER
   *                     for a vertex shader, etc.
   */
  Shader(const glsl::ShaderSource &src, GLenum pshader_type);

  ~Shader();

  /*!
   * The actual GL shader is NOT built at constructor,
   * rather it is built if any of
   * - compile_success()
   * - compile_log()
   * - name()
   *
   * are called. This way, one can construct Shader
   * objects from outside the GL thread. The functions
   * return true if and only if the shader has been built.
   */
  bool
  shader_ready(void);

  /*!
   * Returns the GLSL source string fed to GL
   * to create the GLSL shader.
   */
  c_string
  source_code(void);

  /*!
   * Returns the GLSL compile log of
   * the GLSL source code.
   * If the shader source has not yet
   * been sent to GL for compiling, will
   * trigger those commands. Hence, should
   * only be called from the GL rendering
   * thread or if shader_ready() returns true.
   */
  c_string
  compile_log(void);

  /*!
   * Returns true if and only if GL
   * successfully compiled the shader.
   * If the shader source has not yet
   * been sent to GL for compiling, will
   * trigger those commands. Hence, should
   * only be called from the GL rendering
   * thread or if shader_ready() returns true.
   */
  bool
  compile_success(void);

  /*!
   * Returns the GL name (i.e. ID assigned by GL)
   * of this Shader.
   * If the shader source has not yet
   * been sent to GL for compiling, will
   * trigger those commands. Hence, should
   * only be called from the GL rendering
   * thread or if shader_ready() returns true.
   */
  GLuint
  name(void);

  /*!
   * Returns the shader type of this
   * Shader as set by it's constructor.
   */
  GLenum
  shader_type(void);

  /*!
   * Provided as a conveniance to return a string
   * from a GL enumeration naming a shader type.
   * For example <B>GL_VERTEX_SHADER</B> will
   * return the string "GL_VERTEX_SHADER".
   * Unreconized shader types will return the
   * label "UNKNOWN_SHADER_STAGE".
   */
  static
  c_string
  gl_shader_type_label(GLenum ptype);

  /*!
   * Returns the default shader version to feed to
   * \ref glsl::ShaderSource::specify_version() to
   * match with the GL API. If GL backend, then
   * gives "330". If GLES backend, then gives "300 es".
   */
  static
  c_string
  default_shader_version(void);

private:
  void *m_d;
};

/*!
 * \brief
 * A PreLinkAction is an action to apply to a \ref Program
 * after attaching shaders but before linking.
 */
class PreLinkAction:
  public reference_counted<PreLinkAction>::concurrent
{
public:
  virtual
  ~PreLinkAction()
  {}

  /*!
   * To be implemented by a derived class to perform an action _before_ the
   * GLSL program is linked. Default implementation does nothing.
   * \param glsl_program GL name of GLSL program on which to perform the action.
   */
  virtual
  void
  action(GLuint glsl_program) const = 0;
};


/*!
 * \brief
 * A BindAttribute inherits from \ref PreLinkAction,
 * it's purpose is to bind named attributes to named
 * locations, i.e. it calls glBindAttributeLocation().
 */
class BindAttribute:public PreLinkAction
{
public:
  /*!
   * Ctor.
   * \param pname name of attribute in GLSL code
   * \param plocation location to which to place the attribute
   */
  BindAttribute(c_string pname, int plocation);

  ~BindAttribute();

  virtual
  void
  action(GLuint glsl_program) const;

private:
  void *m_d;
};

/*!
 * \brief
 * A ProgramSeparable inherits from \ref PreLinkAction,
 * its purpose is to set a GLSL program as separable,
 * so that it can be used by a GLSL pipeline.
 * Using a ProgramSeparable requires:
 * - for GLES: GLES3.0 or higher
 * - for GL: either GL version 4.1 or the extension GL_ARB_separate_shader_objects
 */
class ProgramSeparable:public PreLinkAction
{
public:
  virtual
  void
  action(GLuint glsl_program) const;
};

/*!
 * \brief
 * A BindFragDataLocation inherits from \ref PreLinkAction,
 * its purpose is to bind a fragment shader out to
 * a named location and index. Using a BindFragDataLocation
 * requires:
 * - for GLES: GLES3.0 (or higher) and the extension GL_EXT_blend_func_extended
 * - for GL: GL version 3.3 (or higher)
 */
class BindFragDataLocation:public PreLinkAction
{
public:
  /*!
   * Ctor.
   * \param pname name of attribute in GLSL code
   * \param plocation location for fragment shader output to occupy
   * \param pindex index (used for dual source blending) for
   *               fragment shader output to occupy
   */
  BindFragDataLocation(c_string pname, int plocation, int pindex = 0);

  ~BindFragDataLocation();

  virtual
  void
  action(GLuint glsl_program) const;

private:
  void *m_d;
};

/*!
 * \brief
 * A TransformFeedbackVarying encapsulates a call to
 * glTransformFeedbackVarying. Note that if there are
 * multiple \ref TransformFeedbackVarying objects on a
 * single \ref PreLinkActionArray, then only the last
 * one added has effect.
 */
class TransformFeedbackVarying:public PreLinkAction
{
public:
  /*!
   * Ctor.
   * \param buffer_mode the buffer mode to use on
   *        glTransformFeedbackVarying.
   */
  explicit
  TransformFeedbackVarying(GLenum buffer_mode = GL_INTERLEAVED_ATTRIBS);

  ~TransformFeedbackVarying();

  /*!
   * Return the \ref string_array holding the varyings to
   * capture in transform feedback in the order they will
   * be captured; modify this object the change what is
   * captured in transform feedback.
   */
  string_array&
  transform_feedback_varyings(void);

  /*!
   * Return the \ref string_array holding the varyings to
   * capture in transform feedback in the order they will
   * be captured.
   */
  const string_array&
  transform_feedback_varyings(void) const;

  virtual
  void
  action(GLuint glsl_program) const;

private:
  void *m_d;
};

/*!
 * \brief
 * A PreLinkActionArray is a conveniance class
 * wrapper over an array of \ref PreLinkAction handles.
 */
class PreLinkActionArray
{
public:
  /*!
   * Ctor.
   */
  PreLinkActionArray(void);

  /*!
   * Copy ctor.
   * \param obj value from which to copy
   */
  PreLinkActionArray(const PreLinkActionArray &obj);

  ~PreLinkActionArray();

  /*!
   * Assignment operator.
   * \param rhs value from which to copy.
   */
  PreLinkActionArray&
  operator=(const PreLinkActionArray &rhs);

  /*!
   * Swap operation
   * \param obj object with which to swap
   */
  void
  swap(PreLinkActionArray &obj);

  /*!
   * Add a prelink action to execute.
   * \param h handle to action to add
   */
  PreLinkActionArray&
  add(reference_counted_ptr<PreLinkAction> h);

  /*!
   * Provided as a conveniance, equivalent to
   * \code
   * add(FASTUIDRAWnew BindAttribute(pname, plocation))
   * \endcode
   * \param pname name of the attribute
   * \param plocation location to which to bind the attribute.
   */
  PreLinkActionArray&
  add_binding(c_string pname, int plocation)
  {
    reference_counted_ptr<PreLinkAction> h;
    h = FASTUIDRAWnew BindAttribute(pname, plocation);
    return add(h);
  }

  /*!
   * Provided as a conveniance, equivalent to
   * \code
   * add(FASTUIDRAWnew BindFragDataLocation(pname, plocation, pindex))
   * \endcode
   * \param pname name of the attribute
   * \param plocation location for fragment shader output to occupy
   * \param pindex index (used for dual source blending) for
   *               fragment shader output to occupy
   */
  PreLinkActionArray&
  add_frag_binding(c_string pname, int plocation, int pindex = 0)
  {
    reference_counted_ptr<PreLinkAction> h;
    h = FASTUIDRAWnew BindFragDataLocation(pname, plocation, pindex);
    return add(h);
  }

  /*!
   * Provided as a conveniance, equivalent to
   * \code
   * reference_counted_ptr<TransformFeedbackVarying> h;
   * h = TransformFeedbackVarying(buffer_mode);
   * h->transform_feedback_varyings() = varyings;
   * add(h)
   * \endcode
   * \param varyings list of varyings to capture for transform feedback
   * \param buffer_mode buffer mode (i.e. interleaved or not) for transform feedback
   */
  PreLinkActionArray&
  set_transform_feedback(const string_array &varyings,
                         GLenum buffer_mode = GL_INTERLEAVED_ATTRIBS)
  {
    reference_counted_ptr<TransformFeedbackVarying> h;
    h = FASTUIDRAWnew TransformFeedbackVarying(buffer_mode);
    h->transform_feedback_varyings() = varyings;
    return add(h);
  }

  /*!
   * Executes PreLinkAction::action() for each of those
   * actions added via add().
   */
  void
  execute_actions(GLuint glsl_program) const;

private:
  void *m_d;
};

class Program;

/*!
 * \brief
 * A ProgramInitializer is a functor object called the first time
 * a Program is bound (i.e. the first
 * time Program::use_program() is called).
 * It's main purpose is to facilitate initializing
 * uniform values.
 */
class ProgramInitializer:
  public reference_counted<ProgramInitializer>::concurrent
{
public:
  virtual
  ~ProgramInitializer()
  {}

  /*!
   * To be implemented by a derived class to perform additional
   * one-time actions. Function is called after the GL program
   * object is successfully linked.
   * \param pr Program to initialize
   * \param program_bound GLSL Program is already bound, the program is
   *                      NOT bound if the GL/GLES API supports seperable
   *                      program objects.
   */
  virtual
  void
  perform_initialization(Program *pr, bool program_bound) const = 0;
};

/*!
 * \brief
 * Conveniance class to hold an array of handles
 * of ProgramInitializer objects
 */
class ProgramInitializerArray
{
public:
  /*!
   * Ctor.
   */
  ProgramInitializerArray(void);

  /*!
   * Copy ctor.
   * \param obj value from which to copy
   */
  ProgramInitializerArray(const ProgramInitializerArray &obj);

  ~ProgramInitializerArray();

  /*!
   * Assignment operator.
   * \param rhs value from which to copy.
   */
  ProgramInitializerArray&
  operator=(const ProgramInitializerArray &rhs);

  /*!
   * Swap operation
   * \param obj object with which to swap
   */
  void
  swap(ProgramInitializerArray &obj);

  /*!
   * Add an initializer
   * \param h handle to initializer to add
   */
  ProgramInitializerArray&
  add(reference_counted_ptr<ProgramInitializer> h);

  /*!
   * Provided as a conveniance, creates a UniformInitializer
   * object and adds that via add().
   * \param uniform_name name of uniform in GLSL to initialize
   * \param value value with which to set the uniform
   */
  template<typename T>
  ProgramInitializerArray&
  add_uniform_initializer(c_string uniform_name, const T &value);

  /*!
   * Provided as a conveniance, creates a SamplerInitializer
   * object and adds that via add().
   * \param uniform_name name of uniform in GLSL to initialize
   * \param value value with which to set the uniform, in this
   *              case specifies the texture unit as follows:
   *              a value of n means to use GL_TEXTUREn texture
   *              unit.
   */
  ProgramInitializerArray&
  add_sampler_initializer(c_string uniform_name, int value);

  /*!
   * Provided as a conveniance, creates a UniformBlockInitializer
   * object and adds that via add().
   * \param uniform_name name of uniform in GLSL to initialize
   * \param value value with which to set the uniform, in this
   *              case specifies the binding point index to
   *              pass to glBindBufferBase or glBindBufferRange.
   */
  ProgramInitializerArray&
  add_uniform_block_binding(c_string uniform_name, int value);

  /*!
   * For each objected added via add(), call
   * ProgramInitializer::perform_initialization().
   * \param pr Program to pass along
   * \param program_bound if the program is currently bound to
   *                      the GL context
   */
  void
  perform_initializations(Program *pr, bool program_bound) const;

  /*!
   * Clear all elements that have been added via add().
   */
  void
  clear(void);

private:
  void *m_d;
};

/*!
 * \brief
 * Class for creating and using GLSL programs.
 *
 * A Program delays the GL commands to
 * create the actual GL program until the first time
 * it is bound with use_program(). In addition to
 * proving the GL code to create the GLSL code,
 * Program also provides queries GL for all
 * active uniforms and attributes (see active_attributes(),
 * active_uniforms(), find_uniform() and find_attribute()).
 * Also, provides an interface so that a sequence of
 * GL commands are executed the first time it is bound
 * and also an interface so a sequence of actions is
 * executed every time it is bound.
 * Program's are considered a resource,
 * as such have a resource manager.
 */
class Program:
  public reference_counted<Program>::concurrent
{
public:
  /*!
   * \brief
   * Enumeration to describe the backing of a shader
   * variable's value.
   */
  enum shader_variable_src_t
    {
      /*!
       * Indicates that the shader variable is from
       * the default uniform block; the variable's
       * value is not sourced from a backing buffer
       * object.
       */
      src_default_uniform_block,

      /*!
       * Indicats that the shader variable is an
       * atomic buffer counter and sourced from
       * a backing buffer object.
       */
      src_abo,

      /*!
       * Indicats that the shader variable is from
       * the a uniform block; the variable's value
       * is sourced from a backing buffer object.
       */
      src_uniform_block,

      /*!
       * Indicats that the shader variable is from
       * the a shader storage block; the variable
       * is backed by a buffer object.
       */
      src_shader_storage_block,

      /*!
       * Indicates that the shader variable is an
       * input (i.e an 'in' of GLSL).
       */
      src_shader_input,

      /*!
       * Indicates that the shader variable is an
       * output (i.e an 'out' of GLSL).
       */
      src_shader_output,

      /*!
       * Indicates that the shader variable is a
       * transform feedback variable
       */
      src_shader_transform_feedback,

      /*!
       * Indicates that the shader variable is a nullptr
       * value; such values are returned when a query
       * for a shader variable is made and there is
       * no such shader variable.
       */
      src_null,
    };

  /*!
   * \brief
   * A shader_variable_info holds the type,
   * size and name of a uniform or an attribute
   * of a GL program.
   */
  class shader_variable_info
  {
  public:
    /*!
     * Ctor
     */
    shader_variable_info(void);

    /*!
     * Implicit cast operator to bool. If returns
     * false, indicates that the shader_variable_info
     * is null, and returns values for members also
     * will indicate that the value is not an attribute,
     * a uniform of the default uniform block, a variable
     * of a shader storage block, or an atomic counter.
     */
    operator bool() const
    {
      return m_d != nullptr;
    }

    /*!
     * Name of the parameter within the GL API.
     */
    c_string
    name(void) const;

    /*!
     * GL enumeration stating the shader variable's
     * GLSL type.
     */
    GLenum
    glsl_type(void) const;

    /*!
     * Returns the shader variables's backing source
     * type.
     */
    enum shader_variable_src_t
    shader_variable_src(void) const;

    /*!
     * If parameter is an array, holds
     * the legnth of the array, otherwise
     * is 1.
     */
    GLint
    count(void) const;

    /*!
     * GL API index for the parameter. The value of index()
     * is used in calls to GL to query about the parameter,
     * such as glGetActiveUniform and glGetActiveUniformsiv.
     */
    GLuint
    index(void) const;

    /*!
     * "Location" of the uniform or attribute
     * as returned by glGetUniformLocation
     * or glGetAttribLocation. For members of
     * a uniform block or a shader storage
     * buffer, value is -1.
     * \param array_element index into array variable represents
     */
    GLint
    location(unsigned int array_element = 0) const;

    /*!
     * Returns the index to what uniform block this
     * belongs. If this value does not reside on a
     * uniform block, returns -1. The index is the
     * value to feed as bufferIndex in the GL API
     * function's
     * \code
     * glGetProgramResourceiv(program, GL_UNIFORM_BLOCK, bufferIndex, ...)
     * glGetProgramResourceName(program, GL_UNIFORM_BLOCK, bufferIndex, ...)
     * glGetActiveUniformBlockiv(program, bufferIndex, ..)
     * glGetActiveUniformBlockName(program, bufferIndex, ...)
     * glUniformBlockBinding(program, bufferIndex, ...)
     * \endcode
     */
    GLint
    ubo_index(void) const;

    /*!
     * Returns the offset into a backing buffer object
     * on which the this is sourced (or written to).
     * For attributes and uniforms of the default uniform
     * block which are not atomic counters, returns -1.
     * \param array_index index into array variable represents
     * \param leading_array_index index into leading array (for case where
     *                            shader_storage_buffer_top_level_array_size()
     *                            is not negative one).
     */
    GLint
    buffer_offset(unsigned int array_index = 0,
                  unsigned int leading_array_index = 0) const;

    /*!
     * If this is an array, not an attribute or uniform
     * of the default uniform block, then returns the stride,
     * in bytes, between elements of the array. Otherwise
     * returns -1.
     */
    GLint
    array_stride(void) const;

    /*!
     * Returns -1 if this is not an array of matrices
     * from a uniform block that is not the default block.
     * If not, then returns the stride between columns
     * for column major matrixes and otherwise returns
     * the stride between row major matrices.
     */
    GLint
    matrix_stride(void) const;

    /*!
     * If this a matrix from not the default uniform block,
     * return true if the matrix is row major. Otherwise
     * returns false.
     */
    bool
    is_row_major(void) const;

    /*!
     * If this is an atomic counter, returns the index of atomic
     * buffer that the counter is associated with. If not, returns
     * -1. The index is the value to feed as bufferIndex in the
     * GL API to query the atomic buffer block, i.e the functions
     * \code
     * glGetProgramResourceiv(program, GL_ATOMIC_COUNTER_BUFFER, bufferIndex, ...)
     * glGetActiveAtomicCounterBufferiv(GLint program, GLuint bufferIndex, ..)
     * \endcode
     */
    GLint
    abo_index(void) const;

    /*!
     * If this variable is a member of a shader storage buffer, returns
     * to what shader storage buffer block this belongs. If not a shader
     * storage buffer variable, returns -1. This value is used for
     * bufferIndex in the GL API routines
     * \code
     * glGetProgramResourceiv(program, GL_SHADER_STORAGE_BUFFER, bufferIndex, ...)
     * glGetProgramResourceName(program, GL_SHADER_STORAGE_BUFFER, bufferIndex, ...)
     * glShaderStorageBlockBinding(program, bufferIndex, ...)
     * \endcode
     */
    GLint
    shader_storage_buffer_index(void) const;

    /*!
     * If this variable has shader_storage_buffer_index() return -1, then
     * returns -1. Otherwise returns the size of the top level
     * array to which the variable belongs. If the top level
     * array is unsized returns 0.
     */
    GLint
    shader_storage_buffer_top_level_array_size(void) const;

    /*!
     * If this variable has shader_storage_buffer_index() return -1, then
     * returns -1. Otherwise returns the stride of the top level
     * array to which the variable belongs. If it does belong
     * to a top level array, returns 0.
     */
    GLint
    shader_storage_buffer_top_level_array_stride(void) const;

    /*!
     * If this variable is a transform feedback variable, returns
     * to what transform feedback buffer the variable is written.
     * If not a transform feedback variable, returns -1.
     */
    GLint
    transform_feedback_buffer_index(void) const;

  private:
    explicit
    shader_variable_info(const void*);

    const void *m_d;
    friend class Program;
  };

  /*!
   * \brief
   * A block_info represents an object from which
   * one can query the members of a uniform or
   * shader storage block.
   */
  class block_info
  {
  public:
    /*!
     * Ctor
     */
    block_info(void);

    /*!
     * Implicit cast operator to bool. If returns
     * false, indicates that the block_info
     * is null, and thus name() returns an empty
     * string, block_index() returns -1, buffer_size()
     * returns 0 and so on.
     */
    operator bool() const
    {
      return m_d ? true : false;
    }

    /*!
     * Name of the block within the GL API.
     */
    c_string
    name(void) const;

    /*!
     * Returns the backing type of the block.
     */
    enum shader_variable_src_t
    shader_variable_src(void) const;

    /*!
     * GL API index for the parameter. The value of block_index() is used
     * in calls to GL to query and set properties of the block. The
     * default uniform block will have this value as -1. The value of
     * block_index() is the same value as shader_variable_info::ubo_index()
     * for shader variables on a uniform block, for shader variables
     * on a shader storage buffer block and it is the same value as
     * shader_variable_info::shader_storage_buffer_index(void).
     * Lastly, block_index() is the value to feed to Program::uniform_block()
     * for uniform blocks and the value to feed to Program::shader_storage_block()
     * for shader storage blocks.
     */
    GLint
    block_index(void) const;

    /*!
     * Returns the size in bytes of the block (i.e.the size needed for a
     * buffer object to correctly back the block). The default uniform
     * block will have the size as 0.
     */
    GLint
    buffer_size(void) const;

    /*!
     * Returns the buffer binding point of the block when the GLSL
     * program was -FIRST- created. If that binding point has been
     * changed (for instance for UBO's by calling glUniformBlockBinding,
     * or for SSBO's by calling glShaderStorageBlockBinding), then the
     * return value is not suitable to be used to compute the binding
     * point. Note that GLES3 does not allow for the binding point
     * of an SSBO to change (because glShaderStorageBlockBinding is
     * not part of its API).
     */
    GLint
    initial_buffer_binding(void) const;

    /*!
     * Returns the number of active variables of the block.
     * Note that an array of is classified as a single
     * variable.
     */
    unsigned int
    number_variables(void) const;

    /*!
     * Returns the ID'd variable. The values are sorted in
     * alphabetical order of shader_variable_info::name(). The
     * variable list is for those variables of this block.
     * \param I -ID- (not location) of variable, if I is
     *          not less than number_variables(), returns
     *          a shader_variable_info indicating no value (i.e.
     *          shader_variable_info::name() is an empty string and
     *          shader_variable_info::index() is -1).
     */
    shader_variable_info
    variable(unsigned int I);

    /*!
     * Find a shader variable in the block from the a name. The search
     * handles the case where looking for an element of an array
     * and also, in the case of a shader storage block variable where
     * shader_variable_info::shader_storage_buffer_top_level_array_size()
     * being not negative one, giving the leading index into the top
     * level array as well. The values can be used in the methods
     * shader_variable_info::location() and shader_variable_info::buffer_offset()
     * to get the location and buffer offset of the element of the shader
     * variable.
     * \param name variable name to look for
     * \param *out_array_index location to which to write the array index
     *                         value; if nullptr, value is not written
     * \param *out_leading_array_index location to which to write the
     *                                 leading array index value; if nullptr,
     *                                 value is not written
     */
    shader_variable_info
    variable(c_string name,
             unsigned int *out_array_index = nullptr,
             unsigned int *out_leading_array_index = nullptr);

    /*!
     * Comparison operation for sorting. Comparison is done
     * by internal pointer value of object not the values
     * of the object.
     * \param rhs block_info to which to compare.
     */
    bool
    operator<(block_info rhs) const
    {
      return m_d < rhs.m_d;
    }

  private:
    explicit
    block_info(const void*);

    const void *m_d;
    friend class Program;
  };

  /*!
   * \brief
   * An atomic_buffer_info represents an object from which
   * one can query the data of an atomic buffer.
   */
  class atomic_buffer_info
  {
  public:
    /*!
     * Ctor
     */
    atomic_buffer_info(void);

    /*!
     * Implicit cast operator to bool. If returns
     * false, indicates that the atomic_buffer_info
     * is null, and thus buffer_index() returns -1,
     * buffer_size() returns 0 and so on.
     */
    operator bool() const
    {
      return m_d ? true : false;
    }

    /*!
     * Block label (to match better interface of \ref block_info);
     * name is given as "#X_atomic_buffer" where X is the value of
     * buffer_binding().
     */
    c_string
    name(void) const;

    /*!
     * GL API index for querying the atomic buffer. The value
     * of buffer_index() is used in calls to GL to query the
     * atomic buffer block, i.e. the value to feed as index to
     * the GL API functions:
     * \code
     * glGetProgramResourceiv(program, GL_ATOMIC_COUNTER_BUFFER, index, ...)
     * glGetActiveAtomicCounterBufferiv(GLuint program, GLuint index, ...)
     * \endcode
     * It is also the value to feed to Program::atomic_buffer().
     */
    GLint
    buffer_index(void) const;

    /*!
     * The GL API index for the binding point of the atomic
     * buffer, i.e. the value to feed as bufferIndex to the
     * GL API functions:
     * \code
     * glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, bufferIndex, ...)
     * glBindBufferRange(GL_ATOMIC_COUNTER_BUFFER, bufferIndex, ..)
     * \endcode
     */
    GLint
    buffer_binding(void) const;

    /*!
     * Returns the size in bytes of the atomic buffer (i.e.
     * the size needed for a buffer object to correctly back
     * the atomic buffer block).
     */
    GLint
    buffer_size(void) const;

    /*!
     * Returns the number of atomic -variables- of the
     * atomic buffer. Note that an array is classified
     * as a single variable.
     */
    unsigned int
    number_variables(void) const;

    /*!
     * Returns the ID'd atomic variable. The values are sorted in
     * alphabetical order of shader_variable_info::name(). The variable
     * list is for those atomic variables of this atomic buffer
     * \param I -array index- (not location) of atomic, if I is
     *          not less than number_atomic_variables(), returns
     *          a shader_variable_info indicating nothing (i.e.
     *          shader_variable_info::name() is an empty string and
     *          shader_variable_info::index() is -1).
     */
    shader_variable_info
    variable(unsigned int I);

    /*!
     * Find a shader variable in the block from the a name. The search
     * handles the case where looking for an element of an array
     * The value can be used in the method shader_variable_info::buffer_offset()
     * to get the buffer offset of the element of the shader variable.
     * \param name variable name to look for
     * \param[out] *out_array_index location to which to write the array index
     *                              value; if nullptr, value is not written
     */
    shader_variable_info
    variable(c_string name,
             unsigned int *out_array_index = nullptr);

    /*!
     * Comparison operation for sorting. Comparison is done
     * by internal pointer value of object not the values
     * of the object.
     * \param rhs atomic_buffer_info to which to compare.
     */
    bool
    operator<(atomic_buffer_info rhs) const
    {
      return m_d < rhs.m_d;
    }

  private:
    explicit
    atomic_buffer_info(const void*);

    const void *m_d;
    friend class Program;
  };

  /*!
   * Ctor.
   * \param pshaders shaders used to create the Program
   * \param action specifies actions to perform before linking of the Program
   * \param initers one-time initialization actions to perform at GLSL
   *                program creation
   */
  Program(c_array<const reference_counted_ptr<Shader> > pshaders,
          const PreLinkActionArray &action = PreLinkActionArray(),
          const ProgramInitializerArray &initers = ProgramInitializerArray());

  /*!
   * Ctor.
   * \param vert_shader pointer to vertex shader to use for the Program
   * \param frag_shader pointer to fragment shader to use for the Program
   * \param action specifies actions to perform before and
   *               after linking of the Program.
   * \param initers one-time initialization actions to perform at GLSL
   *                program creation
   */
  Program(reference_counted_ptr<Shader> vert_shader,
          reference_counted_ptr<Shader> frag_shader,
          const PreLinkActionArray &action = PreLinkActionArray(),
          const ProgramInitializerArray &initers = ProgramInitializerArray());

  /*!
   * Ctor.
   * \param vert_shader pointer to vertex shader to use for the Program
   * \param frag_shader pointer to fragment shader to use for the Program
   * \param action specifies actions to perform before and
   *               after linking of the Program.
   * \param initers one-time initialization actions to perform at GLSL
   *                program creation
   */
  Program(const glsl::ShaderSource &vert_shader,
          const glsl::ShaderSource &frag_shader,
          const PreLinkActionArray &action = PreLinkActionArray(),
          const ProgramInitializerArray &initers = ProgramInitializerArray());

  /*!
   * Ctor. Create a \ref Program from a previously linked GL shader.
   * \param pname GL ID of previously linked shader
   * \param take_ownership if true when dtor is called glDeleteProgram
   *                       is called as well
   */
  Program(GLuint pname, bool take_ownership);

  ~Program(void);

  /*!
   * Call to set GL to use the GLSLProgram
   * of this Program. The GL context
   * must be current.
   */
  void
  use_program(void);

  /*!
   * Returns the GL name (i.e. ID assigned by GL,
   * for use in glUseProgram) of this Program.
   * This function should
   * only be called either after use_program() has
   * been called or only when the GL context is
   * current.
   */
  GLuint
  name(void);

  /*!
   * Returns the link log of this Program,
   * essentially the value returned by
   * glGetProgramInfoLog. This function should
   * only be called either after use_program() has
   * been called or only when the GL context is
   * current.
   */
  c_string
  link_log(void);

  /*!
   * Returns how many seconds it took for the program
   * to be assembled and linked.
   */
  float
  program_build_time(void);

  /*!
   * Returns true if and only if this Program
   * successfully linked. This function should
   * only be called either after use_program() has
   * been called or only when the GL context is
   * current.
   */
  bool
  link_success(void);

  /*!
   * Returns the full log (including shader source
   * code and link_log()) of this Program.
   * This function should only be called either after
   * use_program() has been called or only when the GL
   * context is current.
   */
  c_string
  log(void);

  /*!
   * Returns a \ref block_info of the default uniform block,
   * the default uniform block does NOT include shader variables
   * coming from atomic buffer counters.
   */
  block_info
  default_uniform_block(void);

  /*!
   * Returns the number of active uniform blocks (not
   * including the default uniform block). This function should
   * only be called either after use_program() has been called
   * or only when the GL context is current.
   */
  unsigned int
  number_active_uniform_blocks(void);

  /*!
   * Returns the indexed uniform block; the index passed
   * is the index as used by the GL API to query the uniform
   * block (and thus the same as shader_variable_info::ubo_index()).
   * This function should only be called either after use_program()
   * has been called or only when the GL context is current.
   * \param I which one to fetch, if I is not less than
   *          number_active_uniform_blocks(), then returns
   *          a null block_info object.
   */
  block_info
  uniform_block(unsigned int I);

  /*!
   * Searches uniform_block(unsigned int) to find the
   * named uniform block. Return value ~0u indicates that
   * the uniform block could not be found, otherwise returns
   * the value to feed to uniform_block(unsigned int). This
   * function should only be called either after use_program()
   * has been called or only when the GL context is current.
   * \param uniform_block_name name of uniform block to find
   */
  unsigned int
  uniform_block_id(c_string uniform_block_name);

  /*!
   * Seaches uniform_block(unsigned int) to find the
   * named uniform block. If no such uniform block
   * has that name returns a null block_info object.
   * Provided as a conveniance, equivalent to
   * \code
   * uniform_block(uniform_block_id(uniform_block_name))
   * \endcode
   * This function should only be called either after
   * use_program() has been called or only when the GL
   * context is current.
   * \param uniform_block_name name of uniform block to find
   */
  block_info
  uniform_block(c_string uniform_block_name)
  {
    return uniform_block(uniform_block_id(uniform_block_name));
  }

  /*!
   * Returns the location of a uniform and also correctly
   * handles fetching the uniform of an element of a uniform
   * array. Returns -1 if there is no uniform on the default
   * block with that name.
   */
  GLint
  uniform_location(c_string name);

  /*!
   * Returns the number of active shader storage blocks.
   * This function should only be called either after
   * use_program() has been called or only when the GL context
   * is current.
   */
  unsigned int
  number_active_shader_storage_blocks(void);

  /*!
   * Returns the indexed shader_storage block; the index passed
   * is the index as used by the GL API to query the uniform
   * block (and thus the same as
   * shader_variable_info::shader_storage_buffer_index()).
   * This function should only be called either after use_program()
   * \param I which one to fetch, if I is not less than
   *          number_active_shader_storage_blocks(), then returns
   *          a nullptr block_info
   */
  block_info
  shader_storage_block(unsigned int I);

  /*!
   * Searches shader_storage_block(unsigned int) to find the named
   * shader_storage block. Return value ~0u indicates that the
   * shader_storage block could not be found. This function should
   * only be called either after use_program() has been called or
   * only when the GL context is current.
   * \param shader_storage_block_name name of shader storage to find
   */
  unsigned int
  shader_storage_block_id(c_string shader_storage_block_name);

  /*!
   * Seaches shader_storage_block(unsigned int) to find the
   * named uniform block. If no such uniform block
   * has that name returns a null block_info object.
   * Provided as a conveniance, equivalent to
   * \code
   * shader_storage_block(shader_storage_block_id(shader_storage_block_name))
   * \endcode
   * This function should only be called either after
   * use_program() has been called or only when the GL
   * context is current.
   * \param shader_storage_block_name name of shader storage to find
   */
  block_info
  shader_storage_block(c_string shader_storage_block_name)
  {
    return shader_storage_block(shader_storage_block_id(shader_storage_block_name));
  }

  /*!
   * Returns the number of active atomic buffers. This function
   * should only be called either after use_program() has been
   * called or only when the GL context is current.
   */
  unsigned int
  number_active_atomic_buffers(void);

  /*!
   * Returns the indexed atomic buffer. the index passed
   * is the index as used by the GL API to query the uniform
   * block (and thus the same as shader_variable_info::abo_index()).
   * This function should only be called either after use_program()
   * has been called or only when the GL context is current.
   * \param I which one to fetch, if I is not less than
   *          number_active_atomic_buffers(), then returns
   *          a null atomic_buffer_info
   */
  atomic_buffer_info
  atomic_buffer(unsigned int I);

  /*!
   * Returns the index to feed to atomic_buffer(unsigned int)
   * to fetch the atomic buffer with the value of
   * atomic_buffer_info::buffer_binding().
   * \param binding_point value for atomic_buffer_info::buffer_binding()
   *                      to search for.
   */
  unsigned int
  atomic_buffer_id(unsigned int binding_point);

  /*!
   * Searches the default uniform block, all uniform blocks
   * and all shader storage blocks for a shader variable.
   */
  shader_variable_info
  find_shader_variable(c_string name,
                       unsigned int *out_array_index = nullptr,
                       unsigned int *out_leading_array_index = nullptr);

  /*!
   * Returns the number of active attributes. Note that an array
   * of uniforms is classified as a single uniform. This function
   * should only be called either after use_program() has been called
   * or only when the GL context is current.
   */
  unsigned int
  number_active_attributes(void);

  /*!
   * Returns the indexed attribute. This function should only be
   * called either after use_program() has been called or only
   * when the GL context is current. The values are sorted in
   * alphabetical order of shader_variable_info::name().
   * \param I -ID- (not location) of attribute, if I is
   *          not less than number_active_attributes(), returns
   *          a shader_variable_info indicating no attribute (i.e.
   *          shader_variable_info::name() is an empty string and
   *          shader_variable_info::index() is -1).
   */
  shader_variable_info
  active_attribute(unsigned int I);

  /*!
   * Returns the number of active transform feedbacks. Note that an
   * array of transform feedback is classified as a single element.
   * This will also include padding in the form of gl_SkipComponents1,
   * gl_SkipComponents2, gl_SkipComponents3 and gl_SkipComponents4.
   * In addition it will also include gl_NextBuffer to indicated
   * advancing to the next buffer. This function should only be called
   * either after use_program() has been called or only when the GL
   * context is current.
   */
  unsigned int
  number_transform_feedbacks(void);

  /*!
   * Returns the indexed transform-feedback. This function should only
   * be called either after use_program() has been called or only
   * when the GL context is current. The values are sorted in
   * alphabetical order of shader_variable_info::name().
   * \param I -ID- (not location or index) of transform feedbach,
   *          if I is not less than number_transform_feedbacks(),
   *          returns a shader_variable_info indicating null (i.e.
   *          shader_variable_info::name() is an empty string and
   *          shader_variable_info::index() is -1).
   */
  shader_variable_info
  transform_feedback(unsigned int I);

  /*!
   * Returns the number of transform feedback buffers.
   */
  unsigned int
  number_transform_feedback_buffers(void);

  /*!
   * Returns the stride in bytes between each element in the
   * named transform feedback buffer.
   * \param B which transform feedback buffer
   */
  unsigned int
  transform_feedback_buffer_stride(unsigned int B);

  /*!
   * Searches active_attribute() to find the named attribute, including
   * named elements of an array. This function should only be called
   * either after use_program() has been called or only when the GL
   * context is current. Returns value -1 indicated that the attribute
   * could not be found.
   * \param attribute_name name of attribute to find
   */
  GLint
  attribute_location(c_string attribute_name);

  /*!
   * Returns the number of shaders of a given type attached to
   * the Program.
   * \param tp GL enumeration of the shader type, see Shader::shader_type()
   */
  unsigned int
  num_shaders(GLenum tp) const;

  /*!
   * Returns true if the source code string for a shader
   * attached to the Program compiled successfully.
   * \param tp GL enumeration of the shader type, see Shader::shader_type()
   * \param i which shader with 0 <= i < num_shaders(tp)
   */
  bool
  shader_compile_success(GLenum tp, unsigned int i) const;

  /*!
   * Returns the source code string for a shader attached to
   * the Program.
   * \param tp GL enumeration of the shader type, see Shader::shader_type()
   * \param i which shader with 0 <= i < num_shaders(tp)
   */
  c_string
  shader_src_code(GLenum tp, unsigned int i) const;

  /*!
   * Returns the compile log for a shader attached to
   * the Program.
   * \param tp GL enumeration of the shader type, see Shader::shader_type()
   * \param i which shader with 0 <= i < num_shaders(tp)
   */
  c_string
  shader_compile_log(GLenum tp, unsigned int i) const;

private:
  void *m_d;
};

/*!
 * \brief
 * A UniformInitalizerBase is a base class for
 * initializing a uniform, the actual GL call to
 * set the uniform value is to be implemented by
 * a derived class in init_uniform().
 */
class UniformInitalizerBase:public ProgramInitializer
{
public:
  /*!
   * Ctor.
   * \param uniform_name name of uniform to initialize
   */
  UniformInitalizerBase(c_string uniform_name);

  ~UniformInitalizerBase();

  virtual
  void
  perform_initialization(Program *pr, bool program_bound) const;

protected:

  /*!
   * To be implemented by a derived class to make the GL call
   * to initialize a uniform in a GLSL shader.
   * \param program GL program
   * \param info information on uniform (name, type, location, etc)
   * \param array_index index into GLSL uniform if it is an array
   * \param program_bound true if and only if the program named
   *                      by program is bound (via glUseProgram).
   *                      If the program is not bound, then one
   *                      SHOULD not bind the program and instead
   *                      use the GL API points to set values of
   *                      the uniform(s) that do not rely on having
   *                      the program bound. One can also safely assume
   *                      that those API points are supported in
   *                      the case it is not bound (in particular
   *                      the function family ProgramUniform() can
   *                      be safely used).
   */
  virtual
  void
  init_uniform(GLuint program, Program::shader_variable_info info,
               unsigned int array_index, bool program_bound) const = 0;

private:
  void *m_d;
};

/*!
 * \brief
 * Initialize a uniform via the templated
 * overloaded function fastuidraw::gl::Uniform().
 */
template<typename T>
class UniformInitializer:public UniformInitalizerBase
{
public:
  /*!
   * Ctor.
   * \param uniform_name name of uniform in GLSL to initialize
   * \param value value with which to set the uniform
   */
  UniformInitializer(c_string uniform_name, const T &value):
    UniformInitalizerBase(uniform_name),
    m_value(value)
  {}

protected:

  virtual
  void
  init_uniform(GLuint program, Program::shader_variable_info info,
               unsigned int array_index, bool program_bound) const
  {
    if (program_bound)
      {
        Uniform(info.location(array_index), m_value);
      }
    else
      {
        ProgramUniform(program, info.location(array_index), m_value);
      }
  }

private:
  T m_value;
};

/*!
 * \brief
 * Specialization for type c_array<const T> for \ref UniformInitializer
 * so that data behind the c_array is copied
 */
template<typename T>
class UniformInitializer<c_array<const T> >:public UniformInitalizerBase
{
public:
  /*!
   * Ctor.
   * \param uniform_name name of uniform in GLSL to initialize
   * \param value value with which to set the uniform
   */
  UniformInitializer(c_string uniform_name, const c_array<const T> &value):
    UniformInitalizerBase(uniform_name),
    m_data(nullptr)
  {
    if (!value.empty())
      {
        m_data = FASTUIDRAWmalloc(sizeof(T) * value.size());
        for (unsigned int i = 0; i < value.size(); ++i)
          {
            new (&m_data[i]) T(value[i]);
          }
        m_value = c_array<const T>(m_data, value.size());
      }
  }

  ~UniformInitializer()
  {
    if (m_data != nullptr)
      {
        for (unsigned int i = 0; i < m_value.size(); ++i)
          {
            m_data[i].~T();
          }
        FASTUIDRAWfree(m_data);
      }
  }

protected:

  virtual
  void
  init_uniform(GLuint program, Program::shader_variable_info info,
               unsigned int array_index, bool program_bound) const
  {
    if (program_bound)
      {
        Uniform(info.location(array_index), m_value);
      }
    else
      {
        ProgramUniform(program, info.location(array_index), m_value);
      }
  }

private:
  T *m_data;
  c_array<const T> m_value;
};

/*!
 * \brief
 * Class to intialize the binding points of samplers.
 * If the uniform is an array, the first element will
 * be given the specified binding point and successive
 * elements in the array will be given successive
 * binding points.
 */
class SamplerInitializer:public UniformInitalizerBase
{
public:
  /*!
   * Ctor.
   * \param uniform_name name of uniform sampler2D in GLSL to
   *                     initialize
   * \param binding_point binding point to initialize the uniform to;
   *                      If the uniform is an array, the first element
   *                      will be given the specified binding point and
   *                      successive elements in the array will be given
   *                      successive binding points.
   */
  SamplerInitializer(c_string uniform_name, int binding_point):
    UniformInitalizerBase(uniform_name),
    m_value(binding_point)
  {}

protected:

  virtual
  void
  init_uniform(GLuint program, Program::shader_variable_info info,
               unsigned int array_index, bool program_bound) const;

private:
  int m_value;
};

/*!
 * \brief
 * A UniformBlockInitializer is used to initalize the binding point
 * used by a bindable uniform (aka Uniform buffer object, see the
 * GL spec on glGetUniformBlockIndex and glUniformBlockBinding.
 */
class UniformBlockInitializer:public ProgramInitializer
{
public:
  /*!
   * Ctor.
   * \param name name of uniform block in GLSL to initialize
   * \param binding_point_index value with which to set the uniform
   */
  UniformBlockInitializer(c_string name, int binding_point_index);

  ~UniformBlockInitializer();

  virtual
  void
  perform_initialization(Program *pr, bool program_bound) const;

private:
  void *m_d;
};

//////////////////////////////////////////////////
// inlined methods that need above classes defined
template<typename T>
ProgramInitializerArray&
ProgramInitializerArray::
add_uniform_initializer(c_string uniform_name, const T &value)
{
  return add(FASTUIDRAWnew UniformInitializer<T>(uniform_name, value));
}

inline
ProgramInitializerArray&
ProgramInitializerArray::
add_sampler_initializer(c_string uniform_name, int value)
{
  return add(FASTUIDRAWnew SamplerInitializer(uniform_name, value));
}

inline
ProgramInitializerArray&
ProgramInitializerArray::
add_uniform_block_binding(c_string uniform_name, int value)
{
  return add(FASTUIDRAWnew UniformBlockInitializer(uniform_name, value));
}

#ifndef FASTUIDRAW_GL_USE_GLES
/*!
 * \brief
 * A ShaderStorageBlockInitializer is used to initalize the binding point
 * used by a shader storage block (see the GL spec on
 * glShaderStorageBlockBinding). Initializer is not supported
 * in OpenGL ES.
 */
class ShaderStorageBlockInitializer:public ProgramInitializer
{
public:
  /*!
   * Ctor.
   * \param name name of shader storage block in GLSL to initialize
   * \param binding_point_index value with which to set the uniform
   */
  ShaderStorageBlockInitializer(c_string name, int binding_point_index);

  ~ShaderStorageBlockInitializer();

  virtual
  void
  perform_initialization(Program *pr, bool program_bound) const;

private:
  void *m_d;
};

#endif
/*! @} */


} //namespace gl
} //namespace fastuidraw

#endif
