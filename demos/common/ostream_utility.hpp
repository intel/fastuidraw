/*!
 * \file ostream_utility.hpp
 * \brief file ostream_utility.hpp
 *
 * Adapted from: ostream_utility.hpp of WRATH:
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


#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <list>
#include <map>
#include <set>
#include <utility>

#include <fastuidraw/util/util.hpp>
#include <fastuidraw/util/c_array.hpp>
#include <fastuidraw/util/vecN.hpp>
#include <fastuidraw/util/matrix.hpp>

/*!\class format_tabbing
  Simple class with overloaded operator<<
  to print a number of indenting characters
  to an std::ostream.
*/
class format_tabbing
{
public:
  /*!\fn format_tabbing
    construct a format_tabbing
    \param ct number of times to print the indent character
    \param c indent character, default is a tab (i.e. \\t)
  */
  explicit
  format_tabbing(unsigned int ct, char c='\t'):m_count(ct), m_char(c) {}

  /*!\var m_count
    Number of times to print \ref m_char.
  */
  unsigned int m_count;

  /*!\var m_char
    Indent character to print.
  */
  char m_char;
};

/*!\class print_range_type
  Simple type to print an STL range of elements
  via overloading operator<<.
*/
template<typename iterator>
class print_range_type
{
public:
  /*!\fn print_range_type
    Ctor.
    \param begin iterator to 1st element to print
    \param end iterator to one past the last element to print
    \param spacingCharacter string to print between consecutive elements
  */
  print_range_type(iterator begin, iterator end,
                   const std::string &spacingCharacter=", "):
    m_begin(begin), m_end(end), m_spacingCharacter(spacingCharacter) {}

  /*!\var m_begin
    Iterator to first element to print
  */
  iterator m_begin;

  /*!\var m_end
    Iterator to one element past the last element to print
  */
  iterator m_end;

  /*!\var m_spacingCharacter
    string to print between consecutive elements
  */
  std::string m_spacingCharacter;
};

/*!\fn print_range_type<iterator> print_range(iterator, iterator, const std::string&)
  Returns a print_range_type to print a
  range of elements to an std::ostream.
  \tparam iterator type
  \param begin iterator to 1st element to print
  \param end iterator to one past the last element to print
  \param str string to print between consecutive elements
*/
template<typename iterator>
print_range_type<iterator>
print_range(iterator begin, iterator end, const std::string &str=", ")
{
  return print_range_type<iterator>(begin, end, str);
}

/*!\class print_range_as_matrix_type
 */
template<typename iterator>
class print_range_as_matrix_type
{
public:
  /*!\fn print_range_as_matrix_type
    Ctor.
    \param begin iterator to 1st element to print
    \param end iterator to one past the last element to print
    \param leadingDimension how many elements to print between rows
    \param spacingCharacter string to print between consecutive elements
  */
  print_range_as_matrix_type(iterator begin, iterator end,
                             unsigned int leadingDimension,
                             const std::string &beginOfLine="",
                             const std::string &endOfLine="\n",
                             const std::string &spacingCharacter=", "):
    m_begin(begin),
    m_end(end),
    m_spacingCharacter(spacingCharacter),
    m_leadingDimension(leadingDimension),
    m_endOfLine(endOfLine),
    m_beginOfLine(beginOfLine)
  {}

  /*!\var m_begin
    Iterator to first element to print
  */
  iterator m_begin;

  /*!\var m_end
    Iterator to one element past the last element to print
  */
  iterator m_end;

  /*!\var m_spacingCharacter
    string to print between consecutive elements
  */
  std::string m_spacingCharacter;

  /*!\var m_leadingDimension;
   */
  unsigned int m_leadingDimension;

  /*!\var m_endOfLine
   */
  std::string m_endOfLine;

  /*!\var m_beginOfLine
   */
  std::string m_beginOfLine;
};

template<typename iterator>
print_range_as_matrix_type<iterator>
print_range_as_matrix(iterator begin, iterator end,
                      unsigned int leadingDimension,
                      const std::string &beginOfLine="",
                      const std::string &endOfLine="\n",
                      const std::string &str=", ")
{
  return print_range_as_matrix_type<iterator>(begin, end, leadingDimension,
                                              beginOfLine, endOfLine, str);
}



/*!\fn std::ostream& operator<<(std::ostream&, const fastuidraw::range_type<T>&)
  conveniance operator<< to print the values of a range_type.
  \param ostr std::ostream to which to print
  \param obj range_type to print to str
*/
template<typename T>
std::ostream&
operator<<(std::ostream &ostr, const fastuidraw::range_type<T> &obj)
{
  ostr << "[" << obj.m_begin << "," << obj.m_end << ")";
  return ostr;
}

/*!\fn std::ostream& operator<<(std::ostream&, const print_range_type<iterator>&)
  overload of operator<< to print the contents of
  an iterator range to an std::ostream.
  \param ostr std::ostream to which to print
  \param obj print_range_type object to print to ostr
*/
template<typename iterator>
std::ostream&
operator<<(std::ostream &ostr,
           const print_range_type<iterator> &obj)
{
  iterator iter;

  for(iter=obj.m_begin; iter!=obj.m_end; ++iter)
    {
      if(iter!=obj.m_begin)
        {
          ostr << obj.m_spacingCharacter;
        }

      ostr << (*iter);
    }
  return ostr;
}

/*!\fn std::ostream& operator<<(std::ostream&, const print_range_as_matrix_type<iterator>&)
  overload of operator<< to print the contents of
  an iterator range to an std::ostream.
  \param ostr std::ostream to which to print
  \param obj  print_range_as_matrix_type object to print to ostr
*/
template<typename iterator>
std::ostream&
operator<<(std::ostream &ostr,
           const print_range_as_matrix_type<iterator> &obj)
{
  iterator iter;
  int idx;

  for(iter=obj.m_begin, idx=0; iter!=obj.m_end; ++iter)
    {
      if(idx == 0)
        {
          ostr << obj.m_beginOfLine;
        }
      else
        {
          ostr << obj.m_spacingCharacter;
        }

      ostr << (*iter);

      ++idx;
      if(idx == obj.m_leadingDimension)
        {
          ostr << obj.m_endOfLine;
          idx = 0;
        }
    }
  return ostr;
}

/*!\fn std::ostream& operator<<(std::ostream&, const format_tabbing&)
  overload of operator<< to print a format_tabbing object
  to an std::ostream.
  \param str std::ostream to which to print
  \param tabber format_tabbing object to print to str
*/
inline
std::ostream&
operator<<(std::ostream &str, const format_tabbing &tabber)
{
  for(unsigned int i=0;i<tabber.m_count;++i)
    {
      str << tabber.m_char;
    }
  return str;
}

/*!\fn std::ostream& operator<<(std::ostream&, const_c_array<T>)
  Conveniance overload of operator<< to print the contents
  pointed to by a c_array to an std::ostream
  \param str std::ostream to which to print
  \param obj c_array to print contents
*/
template<typename T>
inline
std::ostream&
operator<<(std::ostream &str, fastuidraw::const_c_array<T> obj)
{
  str <<  "( " << print_range(obj.begin(), obj.end(), ", ") << " )";
  return str;
}

/*!\fn std::ostream& operator<<(std::ostream&, c_array<T>)
  Conveniance overload of operator<< to print the contents
  pointed to by a const_c_array to an std::ostream
  \param str std::ostream to which to print
  \param obj const_c_array to print contents
*/
template<typename T>
inline
std::ostream&
operator<<(std::ostream &str, fastuidraw::c_array<T> obj)
{
  str <<  "( " << print_range(obj.begin(), obj.end(), ", ") << " )";
  return str;
}

/*!\fn std::ostream& operator<<(std::ostream&, const vecN<T,N>&)
  Overloaded operator<< to print to an std::stream
  the contents of a vecN
  \param str std::stream to which to print
  \param obj contents to print to obj
*/
template<typename T, size_t N>
inline
std::ostream&
operator<<(std::ostream &str, const fastuidraw::vecN<T,N> &obj)
{
  str <<  "( " << print_range(obj.begin(), obj.end(), ", ") << " )";
  return str;
}

/*!\fn std::ostream& operator<<(std::ostream&, const std::pair<T,S>&)
  conveniance operator<< to print an std::pair to an std::ostream.
  \param str std::ostream to which to print
  \param obj std::pair to print to str
 */
template<typename T, typename S>
inline
std::ostream&
operator<<(std::ostream &str, const std::pair<T,S> &obj)
{
  str << "(" << obj.first << "," << obj.second << ")";
  return str;
}

/*!\fn std::ostream& operator<<(std::ostream&, const std::set<T,_Compare,_Alloc>&)
  conveniance operator<< to print the contents
  of an std::set to an std::ostream.
  \param str std::ostream to which to print
  \param obj std::set to print to str
 */
template<typename T, typename _Compare, typename _Alloc>
inline
std::ostream&
operator<<(std::ostream &str, const std::set<T,_Compare,_Alloc> &obj)
{
  str <<  "{ " << print_range(obj.begin(), obj.end(), ", ") << " }";
  return str;
}

/*!\fn std::ostream& operator<<(std::ostream&, const std::list<T,_Alloc>&)
  conveniance operator<< to print the contents
  of an std::list to an std::ostream.
  \param str std::ostream to which to print
  \param obj std::list to print to str
 */
template<typename T, typename _Alloc>
inline
std::ostream&
operator<<(std::ostream &str, const std::list<T,_Alloc> &obj)
{
  str << "( " << print_range(obj.begin(), obj.end(), ", ") << " )";
  return str;
}

template<size_t N, size_t M, typename T>
std::ostream&
operator<<(std::ostream &str, const fastuidraw::matrixNxM<N, M, T> &matrix)
{
  for(unsigned int row = 0; row < N; ++row)
    {
      str << "|";
      for(unsigned int col = 0; col < M; ++col)
        {
          str << std::setw(10) << matrix(row, col) << " ";
        }
      str << "|\n";
    }
  return str;
}


/*! @} */
