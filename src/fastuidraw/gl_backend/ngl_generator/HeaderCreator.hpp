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
private:
  vector<pair<string,string> > m_argTypes; //first is the argumentType, second is the source!
  typedef vector<pair<string,string> >::iterator argIterType;
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
  GetTypeAndNameFromArgumentEntry(const string &inString, string &argumentType);

  static string sm_function_prefix, sm_LoadingFunctionName, sm_GLErrorFunctionName, sm_ErrorLoadingFunctionName;
  static string sm_laodAllFunctionsName,sm_insideBeginEndPairNameCounter, sm_insideBeginEndPairNameFunction;
  static string sm_argumentName,sm_genericCallBackType, sm_kglLoggingStream,sm_kglLoggingStreamNameOnly;
  static string sm_GLPreErrorFunctionName, sm_macro_prefix, sm_namespace;


public:

  openGL_function_info(const string &createFrom,
                       const string &APIprefix_type,
                       const string &APIsuffix_type);
  openGL_function_info(const string &createFrom);

  virtual
  ~openGL_function_info() {}

  static
  void
  SetMacroPrefix(const std::string &pre)
  {
    sm_macro_prefix=pre;
  }

  static
  void
  SetNamespace(const std::string &pre)
  {
    sm_namespace=pre;
  }

  static
  void
  SetFunctionPrefix(const string &pre)
  {
    sm_function_prefix=pre;
    sm_LoadingFunctionName=sm_function_prefix+"loadFunction";
    sm_GLErrorFunctionName=sm_function_prefix+"ErrorCheck";
    sm_GLPreErrorFunctionName=sm_function_prefix+"preErrorCheck";
    sm_ErrorLoadingFunctionName=sm_function_prefix+"on_load_function_error";
    sm_laodAllFunctionsName=sm_function_prefix+"load_all_functions";
    sm_insideBeginEndPairNameCounter=sm_function_prefix+"inSideBeginEndPairCounter";
    sm_insideBeginEndPairNameFunction=sm_function_prefix+"inSideBeginEndPair";
    sm_kglLoggingStreamNameOnly=sm_function_prefix+"LogStream";
    sm_kglLoggingStream=sm_kglLoggingStreamNameOnly+"()";
    sm_argumentName="argument_";
  }

  static
  const string&
  function_prefix(void) { return sm_function_prefix; }

  static
  const string&
  macro_prefix(void) { return sm_macro_prefix; }

  static
  const string&
  function_loader(void) { return sm_LoadingFunctionName; }

  static
  const string&
  function_error_loading(void) { return sm_ErrorLoadingFunctionName; }

  static
  const string&
  function_gl_error(void) { return sm_GLErrorFunctionName; }

  static
  const string&
  function_pregl_error(void) { return sm_GLPreErrorFunctionName; }

  static
  const string&
  function_load_all() { return sm_laodAllFunctionsName; }

  static
  const string&
  inside_begin_end_pair_counter(void) { return sm_insideBeginEndPairNameCounter; }

  static
  const string&
  inside_begin_end_pair_function(void) { return sm_insideBeginEndPairNameFunction; }

  static
  const string&
  argument_name(void) { return sm_argumentName; }

  static
  const string&
  call_back_type(void) { return sm_genericCallBackType; }

  static
  const string&
  log_stream(void) { return sm_kglLoggingStream; }

  static
  const string&
  log_stream_function_name(void)  { return sm_kglLoggingStreamNameOnly; }



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

  const string&
  arg_type(int i) { return m_argTypes[i].first; }

  bool
  arg_type_is_pointer(int i) { return m_argTypes[i].first.find('*') != string::npos; }

  int
  number_arguments(void) { return m_argTypes.size(); }

  const string&
  front_material(void) { return m_frontMaterial; }

  const string&
  created_from(void) { return m_createdFrom; }

  static map<string,openGL_function_info*> sm_lookUp;

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
  bool
  sm_use_function_pointer_mode;


  static
  int
  sm_numberFunctions;

};




string
RemoveWhiteSpace(const string &input);

string
RemoveEndOfLines(const string &input);

extern list<openGL_function_info*> openGL_functionList;


#endif
