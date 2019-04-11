/*!
 * \file painter_item_shader_glsl.cpp
 * \brief file painter_item_shader_glsl.cpp
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

#include <vector>
#include <string>
#include <list>
#include <algorithm>
#include <sstream>

#include <fastuidraw/util/c_array.hpp>
#include <fastuidraw/util/vecN.hpp>
#include <fastuidraw/util/util.hpp>
#include <fastuidraw/glsl/painter_item_shader_glsl.hpp>
#include <fastuidraw/glsl/painter_custom_brush_shader_glsl.hpp>

#include <private/util_private.hpp>
#include <private/glsl/dependency_list.hpp>

namespace
{
  template<typename T>
  class PainterShaderGLSLPrivateCommon
  {
  public:
    typedef fastuidraw::glsl::detail::DependencyListPrivateT<T> DependencyListPrivate;

    PainterShaderGLSLPrivateCommon(const fastuidraw::glsl::ShaderSource &vertex_src,
                                   const fastuidraw::glsl::ShaderSource &fragment_src,
                                   const fastuidraw::glsl::varying_list &varyings,
                                   const DependencyListPrivate &dependencies):
      m_vertex_src(vertex_src),
      m_fragment_src(fragment_src),
      m_varyings(dependencies.compute_varying_list(varyings)),
      m_dependency_shader_names(dependencies.compute_name_list()),
      m_dependent_shaders(dependencies.compute_shader_list())
    {}

    fastuidraw::glsl::ShaderSource m_vertex_src;
    fastuidraw::glsl::ShaderSource m_fragment_src;
    fastuidraw::glsl::varying_list m_varyings;
    fastuidraw::string_array m_dependency_shader_names;
    std::vector<fastuidraw::reference_counted_ptr<const T> > m_dependent_shaders;
  };

  typedef PainterShaderGLSLPrivateCommon<fastuidraw::glsl::PainterItemCoverageShaderGLSL> PainterItemCoverageShaderGLSLPrivate;

  class PainterCustomBrushShaderGLSLPrivate:
    public PainterShaderGLSLPrivateCommon<fastuidraw::glsl::PainterCustomBrushShaderGLSL>
  {
  public:
    PainterCustomBrushShaderGLSLPrivate(unsigned num_external_textures,
                                        const fastuidraw::glsl::ShaderSource &vertex_src,
                                        const fastuidraw::glsl::ShaderSource &fragment_src,
                                        const fastuidraw::glsl::varying_list &varyings,
                                        const DependencyListPrivate &dependencies):
      PainterShaderGLSLPrivateCommon(vertex_src, fragment_src, varyings, dependencies),
      m_number_external_textures(num_external_textures)
    {}

    unsigned int m_number_external_textures;
  };

  class PainterItemShaderGLSLPrivate:
    public PainterShaderGLSLPrivateCommon<fastuidraw::glsl::PainterItemShaderGLSL>
  {
  public:
    PainterItemShaderGLSLPrivate(bool uses_discard,
                                 const fastuidraw::glsl::ShaderSource &vertex_src,
                                 const fastuidraw::glsl::ShaderSource &fragment_src,
                                 const fastuidraw::glsl::varying_list &varyings,
                                 const DependencyListPrivate &dependencies):
      PainterShaderGLSLPrivateCommon(vertex_src, fragment_src, varyings, dependencies),
      m_uses_discard(uses_discard)
    {}

    bool m_uses_discard;
  };
}

//////////////////////////////////////////////////////////////////
// fastuidraw::glsl::PainterItemShaderGLSL::DependencyList methods
fastuidraw::glsl::PainterItemShaderGLSL::DependencyList::
DependencyList(void)
{
  m_d = FASTUIDRAWnew PainterItemShaderGLSLPrivate::DependencyListPrivate();
}

fastuidraw::glsl::PainterItemShaderGLSL::DependencyList::
DependencyList(const DependencyList &rhs)
{
  PainterItemShaderGLSLPrivate::DependencyListPrivate *d;
  d = static_cast<PainterItemShaderGLSLPrivate::DependencyListPrivate*>(rhs.m_d);
  m_d = FASTUIDRAWnew PainterItemShaderGLSLPrivate::DependencyListPrivate(*d);
}

fastuidraw::glsl::PainterItemShaderGLSL::DependencyList::
~DependencyList()
{
  PainterItemShaderGLSLPrivate::DependencyListPrivate *d;
  d = static_cast<PainterItemShaderGLSLPrivate::DependencyListPrivate*>(m_d);
  FASTUIDRAWdelete(d);
}

assign_swap_implement(fastuidraw::glsl::PainterItemShaderGLSL::DependencyList)

fastuidraw::glsl::PainterItemShaderGLSL::DependencyList&
fastuidraw::glsl::PainterItemShaderGLSL::DependencyList::
add_shader(c_string name,
           const reference_counted_ptr<const PainterItemShaderGLSL> &shader)
{
  PainterItemShaderGLSLPrivate::DependencyListPrivate *d;
  d = static_cast<PainterItemShaderGLSLPrivate::DependencyListPrivate*>(m_d);
  d->add_element(name, shader, &shader->varyings());
  return *this;
}

/////////////////////////////////////////////
// fastuidraw::glsl::PainterItemShaderGLSL methods
fastuidraw::glsl::PainterItemShaderGLSL::
PainterItemShaderGLSL(bool puses_discard,
                      const ShaderSource &v_src,
                      const ShaderSource &f_src,
                      const varying_list &varyings,
                      unsigned int num_sub_shaders,
                      const reference_counted_ptr<PainterItemCoverageShaderGLSL> &cvg,
                      const DependencyList &dependencies):
  PainterItemShader(num_sub_shaders, cvg)
{
  PainterItemShaderGLSLPrivate::DependencyListPrivate *d;
  d = static_cast<PainterItemShaderGLSLPrivate::DependencyListPrivate*>(dependencies.m_d);
  m_d = FASTUIDRAWnew PainterItemShaderGLSLPrivate(puses_discard, v_src, f_src, varyings, *d);
}

fastuidraw::glsl::PainterItemShaderGLSL::
PainterItemShaderGLSL(bool puses_discard,
                      const ShaderSource &v_src,
                      const ShaderSource &f_src,
                      const varying_list &varyings,
                      const reference_counted_ptr<PainterItemCoverageShaderGLSL> &cvg,
                      const DependencyList &dependencies):
  PainterItemShader(cvg)
{
  PainterItemShaderGLSLPrivate::DependencyListPrivate *d;
  d = static_cast<PainterItemShaderGLSLPrivate::DependencyListPrivate*>(dependencies.m_d);
  m_d = FASTUIDRAWnew PainterItemShaderGLSLPrivate(puses_discard, v_src, f_src, varyings, *d);
}

fastuidraw::glsl::PainterItemShaderGLSL::
~PainterItemShaderGLSL(void)
{
  PainterItemShaderGLSLPrivate *d;
  d = static_cast<PainterItemShaderGLSLPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = nullptr;
}

fastuidraw::c_array<const fastuidraw::reference_counted_ptr<const fastuidraw::glsl::PainterItemShaderGLSL> >
fastuidraw::glsl::PainterItemShaderGLSL::
dependency_list_shaders(void) const
{
  PainterItemShaderGLSLPrivate *d;
  d = static_cast<PainterItemShaderGLSLPrivate*>(m_d);
  return make_c_array(d->m_dependent_shaders);
}

fastuidraw::c_array<const fastuidraw::c_string>
fastuidraw::glsl::PainterItemShaderGLSL::
dependency_list_names(void) const
{
  PainterItemShaderGLSLPrivate *d;
  d = static_cast<PainterItemShaderGLSLPrivate*>(m_d);
  return d->m_dependency_shader_names.get();
}

get_implement(fastuidraw::glsl::PainterItemShaderGLSL,
              PainterItemShaderGLSLPrivate,
              const fastuidraw::glsl::varying_list&,
              varyings)

get_implement(fastuidraw::glsl::PainterItemShaderGLSL,
              PainterItemShaderGLSLPrivate,
              const fastuidraw::glsl::ShaderSource&,
              vertex_src)

get_implement(fastuidraw::glsl::PainterItemShaderGLSL,
              PainterItemShaderGLSLPrivate,
              const fastuidraw::glsl::ShaderSource&,
              fragment_src)

get_implement(fastuidraw::glsl::PainterItemShaderGLSL,
              PainterItemShaderGLSLPrivate,
              bool,
              uses_discard)

//////////////////////////////////////////////////////////////////
// fastuidraw::glsl::PainterItemCoverageShaderGLSL::DependencyList methods
fastuidraw::glsl::PainterItemCoverageShaderGLSL::DependencyList::
DependencyList(void)
{
  m_d = FASTUIDRAWnew PainterItemCoverageShaderGLSLPrivate::DependencyListPrivate();
}

fastuidraw::glsl::PainterItemCoverageShaderGLSL::DependencyList::
DependencyList(const DependencyList &rhs)
{
  PainterItemCoverageShaderGLSLPrivate::DependencyListPrivate *d;
  d = static_cast<PainterItemCoverageShaderGLSLPrivate::DependencyListPrivate*>(rhs.m_d);
  m_d = FASTUIDRAWnew PainterItemCoverageShaderGLSLPrivate::DependencyListPrivate(*d);
}

fastuidraw::glsl::PainterItemCoverageShaderGLSL::DependencyList::
~DependencyList()
{
  PainterItemCoverageShaderGLSLPrivate::DependencyListPrivate *d;
  d = static_cast<PainterItemCoverageShaderGLSLPrivate::DependencyListPrivate*>(m_d);
  FASTUIDRAWdelete(d);
}

assign_swap_implement(fastuidraw::glsl::PainterItemCoverageShaderGLSL::DependencyList)

fastuidraw::glsl::PainterItemCoverageShaderGLSL::DependencyList&
fastuidraw::glsl::PainterItemCoverageShaderGLSL::DependencyList::
add_shader(c_string name,
           const reference_counted_ptr<const PainterItemCoverageShaderGLSL> &shader)
{
  PainterItemCoverageShaderGLSLPrivate::DependencyListPrivate *d;
  d = static_cast<PainterItemCoverageShaderGLSLPrivate::DependencyListPrivate*>(m_d);
  d->add_element(name, shader, &shader->varyings());
  return *this;
}

///////////////////////////////////////////////
// fastuidraw::glsl::PainterItemCoverageShaderGLSL methods
fastuidraw::glsl::PainterItemCoverageShaderGLSL::
PainterItemCoverageShaderGLSL(const ShaderSource &v_src,
                              const ShaderSource &f_src,
                              const varying_list &varyings,
                              unsigned int num_sub_shaders,
                              const DependencyList &dependencies):
  PainterItemCoverageShader(num_sub_shaders)
{
  PainterItemCoverageShaderGLSLPrivate::DependencyListPrivate *d;
  d = static_cast<PainterItemCoverageShaderGLSLPrivate::DependencyListPrivate*>(dependencies.m_d);
  m_d = FASTUIDRAWnew PainterItemCoverageShaderGLSLPrivate(v_src, f_src, varyings, *d);
}

fastuidraw::glsl::PainterItemCoverageShaderGLSL::
~PainterItemCoverageShaderGLSL(void)
{
  PainterItemCoverageShaderGLSLPrivate *d;
  d = static_cast<PainterItemCoverageShaderGLSLPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = nullptr;
}

fastuidraw::c_array<const fastuidraw::reference_counted_ptr<const fastuidraw::glsl::PainterItemCoverageShaderGLSL> >
fastuidraw::glsl::PainterItemCoverageShaderGLSL::
dependency_list_shaders(void) const
{
  PainterItemCoverageShaderGLSLPrivate *d;
  d = static_cast<PainterItemCoverageShaderGLSLPrivate*>(m_d);
  return make_c_array(d->m_dependent_shaders);
}

fastuidraw::c_array<const fastuidraw::c_string>
fastuidraw::glsl::PainterItemCoverageShaderGLSL::
dependency_list_names(void) const
{
  PainterItemCoverageShaderGLSLPrivate *d;
  d = static_cast<PainterItemCoverageShaderGLSLPrivate*>(m_d);
  return d->m_dependency_shader_names.get();
}

get_implement(fastuidraw::glsl::PainterItemCoverageShaderGLSL,
              PainterItemCoverageShaderGLSLPrivate,
              const fastuidraw::glsl::varying_list&,
              varyings)

get_implement(fastuidraw::glsl::PainterItemCoverageShaderGLSL,
              PainterItemCoverageShaderGLSLPrivate,
              const fastuidraw::glsl::ShaderSource&,
              vertex_src)

get_implement(fastuidraw::glsl::PainterItemCoverageShaderGLSL,
              PainterItemCoverageShaderGLSLPrivate,
              const fastuidraw::glsl::ShaderSource&,
              fragment_src)


//////////////////////////////////////////////////////////////////
// fastuidraw::glsl::PainterCustomBrushShaderGLSL::DependencyList methods
fastuidraw::glsl::PainterCustomBrushShaderGLSL::DependencyList::
DependencyList(void)
{
  m_d = FASTUIDRAWnew PainterCustomBrushShaderGLSLPrivate::DependencyListPrivate();
}

fastuidraw::glsl::PainterCustomBrushShaderGLSL::DependencyList::
DependencyList(const DependencyList &rhs)
{
  PainterCustomBrushShaderGLSLPrivate::DependencyListPrivate *d;
  d = static_cast<PainterCustomBrushShaderGLSLPrivate::DependencyListPrivate*>(rhs.m_d);
  m_d = FASTUIDRAWnew PainterCustomBrushShaderGLSLPrivate::DependencyListPrivate(*d);
}

fastuidraw::glsl::PainterCustomBrushShaderGLSL::DependencyList::
~DependencyList()
{
  PainterCustomBrushShaderGLSLPrivate::DependencyListPrivate *d;
  d = static_cast<PainterCustomBrushShaderGLSLPrivate::DependencyListPrivate*>(m_d);
  FASTUIDRAWdelete(d);
}

assign_swap_implement(fastuidraw::glsl::PainterCustomBrushShaderGLSL::DependencyList)

fastuidraw::glsl::PainterCustomBrushShaderGLSL::DependencyList&
fastuidraw::glsl::PainterCustomBrushShaderGLSL::DependencyList::
add_shader(c_string name,
           const reference_counted_ptr<const PainterCustomBrushShaderGLSL> &shader)
{
  PainterCustomBrushShaderGLSLPrivate::DependencyListPrivate *d;
  d = static_cast<PainterCustomBrushShaderGLSLPrivate::DependencyListPrivate*>(m_d);
  d->add_element(name, shader, &shader->varyings());
  return *this;
}

///////////////////////////////////////////////
// fastuidraw::glsl::PainterCustomBrushShaderGLSL methods
fastuidraw::glsl::PainterCustomBrushShaderGLSL::
PainterCustomBrushShaderGLSL(unsigned num_external_textures,
                             const ShaderSource &v_src,
                             const ShaderSource &f_src,
                             const varying_list &varyings,
                             unsigned int num_sub_shaders,
                             const DependencyList &dependencies):
  PainterCustomBrushShader(num_sub_shaders)
{
  PainterCustomBrushShaderGLSLPrivate::DependencyListPrivate *d;
  d = static_cast<PainterCustomBrushShaderGLSLPrivate::DependencyListPrivate*>(dependencies.m_d);
  m_d = FASTUIDRAWnew PainterCustomBrushShaderGLSLPrivate(num_external_textures, v_src, f_src, varyings, *d);
}

fastuidraw::glsl::PainterCustomBrushShaderGLSL::
~PainterCustomBrushShaderGLSL(void)
{
  PainterCustomBrushShaderGLSLPrivate *d;
  d = static_cast<PainterCustomBrushShaderGLSLPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = nullptr;
}

fastuidraw::c_array<const fastuidraw::reference_counted_ptr<const fastuidraw::glsl::PainterCustomBrushShaderGLSL> >
fastuidraw::glsl::PainterCustomBrushShaderGLSL::
dependency_list_shaders(void) const
{
  PainterCustomBrushShaderGLSLPrivate *d;
  d = static_cast<PainterCustomBrushShaderGLSLPrivate*>(m_d);
  return make_c_array(d->m_dependent_shaders);
}

fastuidraw::c_array<const fastuidraw::c_string>
fastuidraw::glsl::PainterCustomBrushShaderGLSL::
dependency_list_names(void) const
{
  PainterCustomBrushShaderGLSLPrivate *d;
  d = static_cast<PainterCustomBrushShaderGLSLPrivate*>(m_d);
  return d->m_dependency_shader_names.get();
}

get_implement(fastuidraw::glsl::PainterCustomBrushShaderGLSL,
              PainterCustomBrushShaderGLSLPrivate,
              unsigned int, number_external_textures)

get_implement(fastuidraw::glsl::PainterCustomBrushShaderGLSL,
              PainterCustomBrushShaderGLSLPrivate,
              const fastuidraw::glsl::varying_list&,
              varyings)

get_implement(fastuidraw::glsl::PainterCustomBrushShaderGLSL,
              PainterCustomBrushShaderGLSLPrivate,
              const fastuidraw::glsl::ShaderSource&,
              vertex_src)

get_implement(fastuidraw::glsl::PainterCustomBrushShaderGLSL,
              PainterCustomBrushShaderGLSLPrivate,
              const fastuidraw::glsl::ShaderSource&,
              fragment_src)
