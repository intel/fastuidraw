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

#include <private/util_private.hpp>
#include <private/glsl/dependency_list.hpp>

namespace
{
  class VaryingListPrivate
  {
  public:
    enum
      {
        interpolation_number_types = fastuidraw::glsl::varying_list::interpolation_number_types
      };

    fastuidraw::vecN<fastuidraw::string_array, interpolation_number_types> m_floats;
    fastuidraw::string_array m_ints;
    fastuidraw::string_array m_uints;
  };

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

/////////////////////////////////////////////
// fastuidraw::glsl::varying_list methods
fastuidraw::glsl::varying_list::
varying_list(void)
{
  m_d = FASTUIDRAWnew VaryingListPrivate();
}

fastuidraw::glsl::varying_list::
varying_list(const varying_list &rhs)
{
  VaryingListPrivate *d;
  d = static_cast<VaryingListPrivate*>(rhs.m_d);
  m_d = FASTUIDRAWnew VaryingListPrivate(*d);
}

fastuidraw::glsl::varying_list::
~varying_list()
{
  VaryingListPrivate *d;
  d = static_cast<VaryingListPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = nullptr;
}

assign_swap_implement(fastuidraw::glsl::varying_list)

fastuidraw::c_array<const fastuidraw::c_string>
fastuidraw::glsl::varying_list::
floats(enum interpolation_qualifier_t q) const
{
  VaryingListPrivate *d;
  d = static_cast<VaryingListPrivate*>(m_d);
  return d->m_floats[q].get();
}

fastuidraw::c_array<const fastuidraw::c_string>
fastuidraw::glsl::varying_list::
uints(void) const
{
  VaryingListPrivate *d;
  d = static_cast<VaryingListPrivate*>(m_d);
  return d->m_uints.get();
}

fastuidraw::c_array<const fastuidraw::c_string>
fastuidraw::glsl::varying_list::
ints(void) const
{
  VaryingListPrivate *d;
  d = static_cast<VaryingListPrivate*>(m_d);
  return d->m_ints.get();
}

fastuidraw::glsl::varying_list&
fastuidraw::glsl::varying_list::
add_float(c_string pname, enum interpolation_qualifier_t q)
{
  VaryingListPrivate *d;
  d = static_cast<VaryingListPrivate*>(m_d);
  d->m_floats[q].push_back(pname);
  return *this;
}

fastuidraw::glsl::varying_list&
fastuidraw::glsl::varying_list::
add_uint(c_string pname)
{
  VaryingListPrivate *d;
  d = static_cast<VaryingListPrivate*>(m_d);
  d->m_uints.push_back(pname);
  return *this;
}

fastuidraw::glsl::varying_list&
fastuidraw::glsl::varying_list::
add_int(c_string pname)
{
  VaryingListPrivate *d;
  d = static_cast<VaryingListPrivate*>(m_d);
  d->m_ints.push_back(pname);
  return *this;
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
                      const glsl::ShaderSource &v_src,
                      const glsl::ShaderSource &f_src,
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
                      const glsl::ShaderSource &v_src,
                      const glsl::ShaderSource &f_src,
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
PainterItemCoverageShaderGLSL(const glsl::ShaderSource &v_src,
                              const glsl::ShaderSource &f_src,
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
