/*!
 * \file painter_blend_shader_glsl.cpp
 * \brief file painter_blend_shader_glsl.cpp
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

#include <fastuidraw/glsl/painter_blend_shader_glsl.hpp>
#include <private/glsl/dependency_list.hpp>
#include <private/util_private.hpp>

namespace
{
  class PainterBlendShaderGLSLPrivate
  {
  public:
    typedef fastuidraw::glsl::PainterBlendShaderGLSL shader_type;
    typedef fastuidraw::glsl::detail::DependencyListPrivateT<shader_type> DependencyListPrivate;

    PainterBlendShaderGLSLPrivate(const fastuidraw::glsl::ShaderSource &src,
                                  const DependencyListPrivate &dependencies):
      m_src(src),
      m_dependency_shader_names(dependencies.compute_name_list()),
      m_dependent_shaders(dependencies.compute_shader_list())
    {}

    fastuidraw::glsl::ShaderSource m_src;
    fastuidraw::string_array m_dependency_shader_names;
    std::vector<fastuidraw::reference_counted_ptr<const shader_type> > m_dependent_shaders;
  };
}

//////////////////////////////////////////////////////////////////
// fastuidraw::glsl::PainterBlendShaderGLSL::DependencyList methods
fastuidraw::glsl::PainterBlendShaderGLSL::DependencyList::
DependencyList(void)
{
  m_d = FASTUIDRAWnew PainterBlendShaderGLSLPrivate::DependencyListPrivate();
}

fastuidraw::glsl::PainterBlendShaderGLSL::DependencyList::
DependencyList(const DependencyList &rhs)
{
  PainterBlendShaderGLSLPrivate::DependencyListPrivate *d;
  d = static_cast<PainterBlendShaderGLSLPrivate::DependencyListPrivate*>(rhs.m_d);
  m_d = FASTUIDRAWnew PainterBlendShaderGLSLPrivate::DependencyListPrivate(*d);
}

fastuidraw::glsl::PainterBlendShaderGLSL::DependencyList::
~DependencyList()
{
  PainterBlendShaderGLSLPrivate::DependencyListPrivate *d;
  d = static_cast<PainterBlendShaderGLSLPrivate::DependencyListPrivate*>(m_d);
  FASTUIDRAWdelete(d);
}

assign_swap_implement(fastuidraw::glsl::PainterBlendShaderGLSL::DependencyList)

fastuidraw::glsl::PainterBlendShaderGLSL::DependencyList&
fastuidraw::glsl::PainterBlendShaderGLSL::DependencyList::
add_shader(c_string name,
           const reference_counted_ptr<const PainterBlendShaderGLSL> &shader)
{
  PainterBlendShaderGLSLPrivate::DependencyListPrivate *d;
  d = static_cast<PainterBlendShaderGLSLPrivate::DependencyListPrivate*>(m_d);
  d->add_element(name, shader, nullptr);
  return *this;
}

///////////////////////////////////////////////
// fastuidraw::glsl::PainterBlendShaderGLSL methods
fastuidraw::glsl::PainterBlendShaderGLSL::
PainterBlendShaderGLSL(enum shader_type tp,
                       const ShaderSource &src,
                       unsigned int num_sub_shaders,
                       const DependencyList &dependencies):
  PainterBlendShader(tp, num_sub_shaders)
{
  PainterBlendShaderGLSLPrivate::DependencyListPrivate *d;
  d = static_cast<PainterBlendShaderGLSLPrivate::DependencyListPrivate*>(dependencies.m_d);
  m_d = FASTUIDRAWnew PainterBlendShaderGLSLPrivate(src, *d);
}

fastuidraw::glsl::PainterBlendShaderGLSL::
~PainterBlendShaderGLSL(void)
{
  PainterBlendShaderGLSLPrivate *d;
  d = static_cast<PainterBlendShaderGLSLPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = nullptr;
}

const fastuidraw::glsl::ShaderSource&
fastuidraw::glsl::PainterBlendShaderGLSL::
blend_src(void) const
{
  PainterBlendShaderGLSLPrivate *d;
  d = static_cast<PainterBlendShaderGLSLPrivate*>(m_d);
  return d->m_src;
}

fastuidraw::c_array<const fastuidraw::reference_counted_ptr<const fastuidraw::glsl::PainterBlendShaderGLSL> >
fastuidraw::glsl::PainterBlendShaderGLSL::
dependency_list_shaders(void) const
{
  PainterBlendShaderGLSLPrivate *d;
  d = static_cast<PainterBlendShaderGLSLPrivate*>(m_d);
  return make_c_array(d->m_dependent_shaders);
}

fastuidraw::c_array<const fastuidraw::c_string>
fastuidraw::glsl::PainterBlendShaderGLSL::
dependency_list_names(void) const
{
  PainterBlendShaderGLSLPrivate *d;
  d = static_cast<PainterBlendShaderGLSLPrivate*>(m_d);
  return d->m_dependency_shader_names.get();
}
