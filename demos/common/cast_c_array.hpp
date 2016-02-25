#pragma once

#include <vector>
#include <fastuidraw/util/c_array.hpp>

template<typename T>
fastuidraw::const_c_array<T>
cast_c_array(const std::vector<T> &p)
{
  return (p.empty()) ?
    fastuidraw::const_c_array<T>() :
    fastuidraw::const_c_array<T>(&p[0], p.size());
}

template<typename T>
fastuidraw::c_array<T>
cast_c_array(std::vector<T> &p)
{
  return (p.empty()) ?
    fastuidraw::c_array<T>() :
    fastuidraw::c_array<T>(&p[0], p.size());
}
