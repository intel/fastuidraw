/*!
 * \file HeaderCreator.hpp
 * \brief file HeaderCreator.hpp
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


/*!
 * \file HeaderCreator.hpp
 * \brief file HeaderCreator.hpp
 *
 * Copyright 2013 by Nomovok Ltd.
 *
 * Contact: info@nomovok.com
 *
 * This Source Code Form is subject to the
 * terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with
 * this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 *
 * \author Kevin Rogovin <kevin.rogovin@nomovok.com>
 *
 */



#ifndef __HEADER_CREATOR_CPP__
#define __HEADER_CREATOR_CPP__

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <list>
#include <set>
#include <map>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cstddef>
#include <cmath>
#include <cctype>
#include <fstream>
#include <sstream>
#include <list>
#include <map>
#include <string>
#include <algorithm>
#include <ostream>
#include <sstream>


using namespace std;

class openGL_function_info
{
public:
  class ArgumentType
  {
  public:
    std::string m_front, m_back;
  };

private:
  vector<pair<ArgumentType,string> > m_argTypes; //first is the argumentType, second is the source!
  typedef vector<pair<ArgumentType, string> >::iterator argIterType;
  string m_functionName, m_returnType;
  string m_pointerToFunctionTypeName;

  bool m_returnsValue;
  string m_frontMaterial; /*includes all the funky extras and the return type */

  //derived strings
  string m_argListWithNames, m_argListWithoutNames, m_argListOnly;
  string m_functionPointerName, m_debugFunctionName, m_localFunctionName;
  string m_doNothingFunctionName, m_existsFunctionName, m_getFunctionName;

  string m_createdFrom;  // string that genertated this object
  string m_argListInput; // the string that generated our arguments lists.

  bool m_newDeclaration;

  bool m_use_function_pointer;

  static
  void
  GetTypeFromArgumentEntry(const string &inString, ArgumentType &argumentType);

public:

  openGL_function_info(const string &createFrom,
                       const string &APIprefix_type,
                       const string &APIsuffix_type);
  openGL_function_info(const string &createFrom);

  virtual
  ~openGL_function_info() {}

  static
  void
  SetMacroPrefix(const std::string &pre);

  static
  void
  SetNamespace(const std::string &pre);

  static
  void
  SetFunctionPrefix(const string &pre);

  static
  const string&
  function_prefix(void);

  static
  const string&
  macro_prefix(void);

  static
  const string&
  function_loader(void);

  static
  const string&
  function_error_loading(void);

  static
  const string&
  function_call_unloadable_function(void);

  static
  const string&
  function_gl_error(void);

  static
  const string&
  function_pregl_error(void);

  static
  const string&
  function_load_all();

  static
  const string&
  inside_begin_end_pair_counter(void);

  static
  const string&
  inside_begin_end_pair_function(void);

  static
  const string&
  argument_name(void);

  static
  const string&
  call_back_type(void);

  static
  const string&
  log_stream(void);

  static
  const string&
  log_stream_function_name(void);

  void
  SetNames(const string &functionName,const string &returnType,string &argList);

  void
  GetInfo(ostream &);

  virtual
  void
  output_to_header(ostream &headerFile);

  virtual
  void
  output_to_source(ostream &sourceFile);

  const string&
  function_name(void) { return m_functionName; }

  const string&
  function_pointer_type(void) { return m_pointerToFunctionTypeName; }

  const string&
  function_pointer_name(void) { return m_functionPointerName; }

  const string&
  debug_function_name(void)   { return m_debugFunctionName; }

  const string&
  local_function_name(void)   { return m_localFunctionName; }

  const string&
  do_nothing_function_name(void) { return m_doNothingFunctionName; }

  const string&
  load_function_name(void) { return m_existsFunctionName; }

  const string&
  return_type(void) { return m_returnType; }

  bool
  returns_value(void) { return m_returnsValue; }

  const string&
  full_arg_list_with_names(void) { return m_argListWithNames; }

  const string&
  full_arg_list_withoutnames(void) { return m_argListWithoutNames; }

  const string&
  argument_list_names_only(void) { return m_argListOnly; }

  const ArgumentType&
  arg_type(int i) { return m_argTypes[i].first; }

  bool
  arg_type_is_pointer(int i)
  {
    return m_argTypes[i].first.m_front.find('*') != string::npos
      || !m_argTypes[i].first.m_back.empty();
  }

  int
  number_arguments(void) { return m_argTypes.size(); }

  const string&
  front_material(void) { return m_frontMaterial; }

  const string&
  created_from(void) { return m_createdFrom; }

  static
  void
  HeaderEnd  (ostream &, const list<string> &fileNames);

  static
  void
  HeaderStart(ostream &, const list<string> &fileNames);

  static
  void
  SourceEnd  (ostream &, const list<string> &fileNames);

  static
  void
  SourceStart(ostream &, const list<string> &fileNames);

  static
  list<openGL_function_info*>&
  openGL_functionList(void);

  static
  bool&
  use_function_pointer_mode(void);

  static
  map<string,openGL_function_info*>&
  lookUp(void);
};


string
RemoveWhiteSpace(const string &input);

string
RemoveEndOfLines(const string &input);


#endif
