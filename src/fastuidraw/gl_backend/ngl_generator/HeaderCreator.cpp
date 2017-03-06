/*!
 * \file HeaderCreator.cpp
 * \brief file HeaderCreator.cpp
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
 * \file HeaderCreator.cpp
 * \brief file HeaderCreator.cpp
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



#include <stdlib.h>
#include <fstream>
#include <sstream>
#include "HeaderCreator.hpp"

void begin_namespace(const string &pn, ostream &stream)
{
  if(!pn.empty())
    {
      std::size_t prev, current;
      prev = 0;
      do
        {
          current = pn.find("::", prev);
          stream << "namespace " << pn.substr(prev, current) << " {\n";
          prev = current;
          if(prev != std::string::npos)
            {
              prev  += 2;
            }
        }
      while(prev != std::string::npos);
    }
}

void end_namespace(const string &pn, ostream &stream)
{
  if(!pn.empty())
    {
      std::size_t prev, current;
      prev = 0;
      do
        {
          current = pn.find("::", prev);

          stream << "\n\n} //" << pn.substr(prev, current);
          prev = current;
          if(prev != std::string::npos)
            {
              prev += 2;
            }
        }
      while(prev != std::string::npos);
    }
}

class GlobalElements
{
public:
  GlobalElements(void):
    m_numberFunctions(0),
    m_use_function_pointer_mode(true)
  {}

  list<openGL_function_info*> m_openGL_functionList;
  map<string,openGL_function_info*> m_lookUp;
  string m_function_prefix, m_LoadingFunctionName;
  string m_GLErrorFunctionName, m_ErrorLoadingFunctionName;
  string m_laodAllFunctionsName, m_argumentName;;
  string m_insideBeginEndPairNameCounter, m_insideBeginEndPairNameFunction;
  string m_genericCallBackType, m_kglLoggingStream;
  string m_kglLoggingStreamNameOnly, m_GLPreErrorFunctionName;
  string m_macro_prefix, m_namespace, m_CallUnloadableFunction;
  int m_numberFunctions;
  bool m_use_function_pointer_mode;

  static
  GlobalElements&
  get(void)
  {
    static GlobalElements R;
    return R;
  }
};

///////////////////////////////////////
// openGL_function_info static methods
list<openGL_function_info*>&
openGL_function_info::
openGL_functionList(void)
{
  return GlobalElements::get().m_openGL_functionList;
}

bool&
openGL_function_info::
use_function_pointer_mode(void)
{
  return GlobalElements::get().m_use_function_pointer_mode;
}

map<string,openGL_function_info*>&
openGL_function_info::
lookUp(void)
{
  return GlobalElements::get().m_lookUp;
}

void
openGL_function_info::
SetMacroPrefix(const std::string &pre)
{
  GlobalElements::get().m_macro_prefix=pre;
}

void
openGL_function_info::
SetNamespace(const std::string &pre)
{
  GlobalElements::get().m_namespace=pre;
}

void
openGL_function_info::
SetFunctionPrefix(const string &pre)
{
  GlobalElements::get().m_function_prefix=pre;
  GlobalElements::get().m_LoadingFunctionName=GlobalElements::get().m_function_prefix+"loadFunction";
  GlobalElements::get().m_GLErrorFunctionName=GlobalElements::get().m_function_prefix+"ErrorCheck";
  GlobalElements::get().m_GLPreErrorFunctionName=GlobalElements::get().m_function_prefix+"preErrorCheck";
  GlobalElements::get().m_ErrorLoadingFunctionName=GlobalElements::get().m_function_prefix+"on_load_function_error";
  GlobalElements::get().m_laodAllFunctionsName=GlobalElements::get().m_function_prefix+"load_all_functions";
  GlobalElements::get().m_insideBeginEndPairNameCounter=GlobalElements::get().m_function_prefix+"inSideBeginEndPairCounter";
  GlobalElements::get().m_insideBeginEndPairNameFunction=GlobalElements::get().m_function_prefix+"inSideBeginEndPair";
  GlobalElements::get().m_kglLoggingStreamNameOnly=GlobalElements::get().m_function_prefix+"LogStream";
  GlobalElements::get().m_CallUnloadableFunction=GlobalElements::get().m_function_prefix+"call_unloadable_function";
  GlobalElements::get().m_kglLoggingStream=GlobalElements::get().m_kglLoggingStreamNameOnly+"()";
  GlobalElements::get().m_argumentName="argument_";
}

const string&
openGL_function_info::
function_prefix(void) { return GlobalElements::get().m_function_prefix; }

const string&
openGL_function_info::
macro_prefix(void) { return GlobalElements::get().m_macro_prefix; }

const string&
openGL_function_info::
function_loader(void) { return GlobalElements::get().m_LoadingFunctionName; }

const string&
openGL_function_info::
function_error_loading(void) { return GlobalElements::get().m_ErrorLoadingFunctionName; }

const string&
openGL_function_info::
function_call_unloadable_function(void) { return GlobalElements::get().m_CallUnloadableFunction; }

const string&
openGL_function_info::
function_gl_error(void) { return GlobalElements::get().m_GLErrorFunctionName; }

const string&
openGL_function_info::
function_pregl_error(void) { return GlobalElements::get().m_GLPreErrorFunctionName; }

const string&
openGL_function_info::
function_load_all() { return GlobalElements::get().m_laodAllFunctionsName; }

const string&
openGL_function_info::
inside_begin_end_pair_counter(void) { return GlobalElements::get().m_insideBeginEndPairNameCounter; }

const string&
openGL_function_info::
inside_begin_end_pair_function(void) { return GlobalElements::get().m_insideBeginEndPairNameFunction; }

const string&
openGL_function_info::
argument_name(void) { return GlobalElements::get().m_argumentName; }

const string&
openGL_function_info::
call_back_type(void) { return GlobalElements::get().m_genericCallBackType; }

const string&
openGL_function_info::
log_stream(void) { return GlobalElements::get().m_kglLoggingStream; }

const string&
openGL_function_info::
log_stream_function_name(void)  { return GlobalElements::get().m_kglLoggingStreamNameOnly; }

////////////////////////////////////
// openGL_function_info non-static methods
openGL_function_info::
openGL_function_info(const string &line_from_gl_h_in,
                     const string &APIprefix_type,
                     const string &APIsuffix_type):
  m_returnsValue(false),
  m_createdFrom(line_from_gl_h_in),
  m_use_function_pointer(GlobalElements::get().m_use_function_pointer_mode)
{
  ++GlobalElements::get().m_numberFunctions;

  //typical line: expectedAPItype  return-type APIENTRY function-name (argument-list);
  string::size_type firstParen, glStart, glEnd,lastParen, argStart, argEnd;
  string::size_type retBegin,retEnd, frontMaterialEnd;
  string name, retType,argList;
  string line_from_gl_h;

  line_from_gl_h=RemoveEndOfLines(line_from_gl_h_in);

  /* first get the the first parentisis of the line */
  firstParen=line_from_gl_h.find_first_of('(');
  lastParen=line_from_gl_h.find_last_of(')');

  /* here is the argument list without parentesis but with spaces and all */
  argStart=line_from_gl_h.find_first_not_of(' ',firstParen+1);
  argEnd=line_from_gl_h.find_last_not_of(' ',lastParen-1);
  argList=line_from_gl_h.substr(argStart,argEnd-argStart+1);

  if(APIprefix_type.empty())
    {
      retBegin=0;
    }
  else
    {
      retBegin=line_from_gl_h.find(APIprefix_type);
      if(retBegin!=string::npos)
        retBegin+=APIprefix_type.length();
      else
        retBegin=0;
    }

  retEnd=line_from_gl_h.find(APIsuffix_type,retBegin);

  if(retEnd==string::npos)
    {
      //unable to find GLAPIENTRY to mark
      //where the return type ends, so we
      //look for gl
      retEnd=line_from_gl_h.find("gl",retBegin);
    }

  retType=line_from_gl_h.substr(retBegin,retEnd-retBegin);


  /* get the function name by looking at the string before the firstParen */
  glStart=line_from_gl_h.find("gl",retEnd);
  glEnd=line_from_gl_h.rfind(' ',firstParen);
  if(glEnd!=string::npos && glEnd>glStart)
    {
      glEnd--;
    }
  else
    {
      glEnd=firstParen-1;
    }

  /* here is the function name */
  name=line_from_gl_h.substr(glStart,glEnd-glStart+1);

  frontMaterialEnd=line_from_gl_h.find(name);
  m_frontMaterial=line_from_gl_h.substr(0,frontMaterialEnd);

  SetNames(name,retType,argList);

  m_newDeclaration=(GlobalElements::get().m_lookUp.find(function_name())==GlobalElements::get().m_lookUp.end());
  if(m_newDeclaration)
    {
      GlobalElements::get().m_lookUp.insert( pair<string,openGL_function_info*>(function_name(),this) );
    }
  else
    {
      // cout << "Warning: " << function_name() << " in already in list, this object will not be added!\n";
    }
}

void
openGL_function_info::
SetNames(const string &functionName,
         const string &returnType,
         string &argList)
{
  argIterType i;
  string::size_type comma_place,last_comma_place;
  string::size_type start,end;
  ostringstream argListWithNames,argListWithoutNames,argListOnly;
  string arg;
  ArgumentType argType;
  int j;

  m_functionName = RemoveWhiteSpace(functionName);


  if(returnType.length()!=0)
    {
      start = returnType.find_first_not_of(' ');
      end = returnType.find_last_not_of(' ');
      m_returnType = returnType.substr(start, end - start + 1);
    }
  else
    {
      m_returnType = "";
    }

  m_returnsValue = (m_returnType != "void") && (m_returnType != "GLvoid");

  m_pointerToFunctionTypeName = "PFN" + m_functionName + "PROC";
  for(j = 0; j< (int)m_pointerToFunctionTypeName.length(); ++j)
    {
      m_pointerToFunctionTypeName[j] = toupper(m_pointerToFunctionTypeName[j]);
    }


  if(argList != "void" && argList != "GLvoid")
    {
      argList = argList;
    }
  else
    {
      argList = "";
    }

  //if argList is non-empy it may have one or more argument lists.
  last_comma_place = argList.find_first_of(',');
  if(last_comma_place != string::npos)
    {

      arg = argList.substr(0, last_comma_place);
      GetTypeFromArgumentEntry(arg, argType);
      m_argTypes.push_back(pair<ArgumentType, string>(argType, arg));

      //argList has atleast 2 arguments
      last_comma_place++;

      //now last_command_place is at the location just past the first comma.
      for(j=1, comma_place=argList.find_first_of(',',last_comma_place);
          comma_place!=string::npos && comma_place<argList.length();
          comma_place=argList.find_first_of(',',last_comma_place), ++j)
        {
          //add to the end all that was between
          //last_comma_place and comma_place.
          arg=argList.substr(last_comma_place,comma_place-last_comma_place);
          GetTypeFromArgumentEntry(arg, argType);
          m_argTypes.push_back(pair<ArgumentType,string>(argType,arg));

          last_comma_place=comma_place+1;
        }
      //now there is also the last argument...
      arg=argList.substr(last_comma_place);
      GetTypeFromArgumentEntry(arg,argType);
      m_argTypes.push_back(pair<ArgumentType,string>(argType,arg));
    }
  else if(argList.size()!=0)
    {
      GetTypeFromArgumentEntry(argList,argType);
      m_argTypes.push_back(pair<ArgumentType,string>(argType,argList));
    }



  //now go through the list of argtype to
  //build our argument lsit with and without names
  for(j=0, i=m_argTypes.begin();i!=m_argTypes.end();++i, ++j)
    {
      if(j!=0)
        {
          argListWithNames <<",";
          argListWithoutNames <<",";
          argListOnly<<",";
        }

      argListWithNames << i->first.m_front << " " << argument_name() << j << i->first.m_back;
      argListWithoutNames << i->first.m_front << i->first.m_back;
      argListOnly << " " << argument_name() << j;
    }

  m_argListWithNames=argListWithNames.str();
  m_argListWithoutNames=argListWithoutNames.str();
  m_argListOnly=argListOnly.str();

  m_functionPointerName=GlobalElements::get().m_function_prefix+"function_ptr_"+m_functionName;
  m_debugFunctionName=GlobalElements::get().m_function_prefix+"debug_function__"+m_functionName;
  m_localFunctionName=GlobalElements::get().m_function_prefix+"local_function_"+m_functionName;
  m_doNothingFunctionName=GlobalElements::get().m_function_prefix+"do_nothing_function_"+m_functionName;
  m_existsFunctionName=GlobalElements::get().m_function_prefix+"exists_function_"+m_functionName;
  m_getFunctionName=GlobalElements::get().m_function_prefix+"get_function_ptr_"+m_functionName;
}

void
openGL_function_info::
GetInfo(ostream &ostr)
{
  int j;
  argIterType i;

  ostr << "\nCreated From=\"" << m_createdFrom
       <<  "\"\n\tfunctionName=\"" << m_functionName
       << "\"\n\treturnType=\"" << m_returnType
       << "\"\n\tfrontMaterial=\"" << m_frontMaterial
       << "\"\n\targListwithoutName=\"" << m_argListWithoutNames
       << "\"\n\targListwithName=\""<< m_argListWithNames
       << "\"\n\tnumArguments=" << m_argTypes.size() << "\"";
  for(j=0, i=m_argTypes.begin();i!=m_argTypes.end();++i, ++j)
    {
      ostr << "\n\t\tArgumentType(" << j << ")=\"" << i->first.m_front
           << " " << i->first.m_back
           << "\" from \""  << i->second << "\"";
    }


  ostr << "\n\tDoes " << (!m_returnsValue?"NOT ":"") << "return a value"
       << "\n\tpointerTypeName=\"" << m_pointerToFunctionTypeName << "\"\n";
}



void
openGL_function_info::
output_to_header(ostream &headerFile)
{

  if(!m_newDeclaration)
    {
      //      cerr << "Warning: " << function_name() << " in list twice not putting into header file!\n";
      return;
    }

  headerFile << "extern " << function_pointer_type() << " "
             << function_pointer_name() << ";\n";

  if(m_use_function_pointer)
    {
      headerFile << "int " << m_existsFunctionName << "(void);\n"
                 << function_pointer_type() << " " << m_getFunctionName << "(void);\n";
    }
  else
    {
      headerFile << "inline int " << m_existsFunctionName << "(void) { return 1; }\n"
                 << "inline " << function_pointer_type() << " " << m_getFunctionName
                 << "(void) { return " << m_functionName << "; }\n";
    }
  headerFile << "#ifdef GL_DEBUG\n";
  headerFile << return_type() << " " << debug_function_name() << "(";

  if(number_arguments()!=0)
    {
      headerFile << full_arg_list_with_names() << ", " ;
    }
  headerFile << "const char *file, int line, const char *call";

  for(int i=0;i<number_arguments();++i)
    {
      headerFile << ", const char *argumentName_" << i;
    }
  headerFile << ");\n"
             << "#define "  << function_name()
             << "(" << argument_list_names_only()
             << ") " << GlobalElements::get().m_namespace << "::" << debug_function_name()  << "(";

  if(number_arguments()!=0)
    {
      headerFile << argument_list_names_only() << "," ;
    }

  headerFile << " __FILE__, __LINE__, \"" << function_name() << "(\"";

  for(int i=0;i<number_arguments();++i)
    {
      if(i!=0)
        headerFile << "\",\"";
      headerFile << "#" << argument_name() << i;
    }
  headerFile << "\")\"";
  for(int i=0;i<number_arguments();++i)
    {
      headerFile << ", #" <<  argument_name() << i;
    }


  headerFile << ")\n"
             << "#else\n" << "#define " << function_name() << "(" << argument_list_names_only()
             << ") "
             << GlobalElements::get().m_namespace << "::" << function_pointer_name() <<  "(" << argument_list_names_only()
             << ")\n#endif\n\n";



}

void
openGL_function_info::
output_to_source(ostream &sourceFile)
{
  int i;

  if(!m_newDeclaration)
    {
      //      cerr << "Warning: " << function_name() << " in list twice not putting into source file!\n";
      return;
    }

  if(m_use_function_pointer)
    {
      //declare prototypes:
      sourceFile << "int " << m_existsFunctionName << "(void);\n";
      sourceFile << front_material() << " " << local_function_name() << "("
                 << full_arg_list_with_names() <<  ");\n";
      sourceFile << front_material() << " " << do_nothing_function_name() << "("
                 << full_arg_list_withoutnames() <<  ");\n"
                 << function_pointer_type() << " " << m_getFunctionName << "(void);\n";

      //init the pointer for the function!
      sourceFile << function_pointer_type() << " " << function_pointer_name() << "="
                 << local_function_name() << ";\n\n\n";


      //first the function which holds the initial value of the function pointer:
      sourceFile << front_material() << " " << local_function_name() << "("
                 << full_arg_list_with_names() <<  ")\n{\n\t"
                 << m_getFunctionName << "();\n\t";

      if(returns_value())
        {
          sourceFile << "return ";
        }

      sourceFile << function_pointer_name()
                 << "(" << argument_list_names_only() << ");\n}\n\n";

      //thirdly the do nothing function:
      sourceFile << front_material() << " " << do_nothing_function_name() << "("
                 << full_arg_list_withoutnames() <<  ")\n{\n\t";

      if(returns_value())
        {
            sourceFile << return_type() << " retval = 0;\n\t";
        }
      sourceFile << function_call_unloadable_function() << "(\""
                 << function_name() << "\");\n\treturn";

      if(returns_value())
        {
          sourceFile << " retval";
        }
      sourceFile << ";\n}\n";

      //fourthly the getFunction, which does the loading.
      sourceFile << function_pointer_type() << " " << m_getFunctionName << "(void)\n{\n\t"
                 << "if(" << function_pointer_name() << "==" << local_function_name()
                 << ")\n\t{\n\t\t" << function_pointer_name() << "=("
                 << function_pointer_type() << ")" << function_loader() << "(\""
                 << function_name() << "\");\n\t\tif(" << function_pointer_name()
                 << "==NULL)\n\t\t{\n\t\t\t"<< function_error_loading() << "(\""
                 << function_name() << "\");\n\t\t\t" << function_pointer_name()
                 << "=" << do_nothing_function_name() << ";\n\t\t}\n\t}\n\t"
                 << "return " << function_pointer_name() << ";\n}\n\n";

      //lastly the exists function:
      sourceFile << "int " << m_existsFunctionName << "(void)\n{\n\t"
                 << m_getFunctionName << "();\n\t"
                 << "return " << function_pointer_name() << "!="
                 << do_nothing_function_name() << ";\n}\n\n";

    }
  else
    {
      sourceFile << function_pointer_type() << " " << function_pointer_name() << "="
                 << m_functionName << ";\n\n\n";
    }



  //second the debug function.
  sourceFile << "#ifdef GL_DEBUG\n"
             << return_type() << " " << debug_function_name()
             << "(";
  if(number_arguments()!=0)
    {
      sourceFile  << full_arg_list_with_names() << ", ";
    }
  sourceFile << "const char *file, int line, const char *call";
  for(int i=0;i<number_arguments();++i)
    {
      sourceFile  << ", const char *argumentName_" << i;
    }


  sourceFile << ")\n{\n\tstd::ostringstream call_stream;\n\t"
             << "std::string call_string;\n\t";


  if(returns_value())
    {
      sourceFile << return_type() << " retval;\n\t";
    }


  if(function_name()=="glBegin")
    {
      sourceFile << inside_begin_end_pair_counter() << "=1;\n\t";
    }

  sourceFile << "call_stream << \"" << function_name() << "(\" ";
  for(i=0;i<number_arguments();++i)
    {
      if(i!=0)
        {
          sourceFile << " << \",\" ";
        }
      sourceFile << "<< argumentName_" << i << " ";
      if(!arg_type_is_pointer(i))
        {
          sourceFile << "<< \"=0x\" ";
        }
      else
        {
          sourceFile << "<< \"=\" ";
        }

      sourceFile << "<< std::hex << argument_" << i << " ";
    }
  sourceFile << "<< \")\";\n\tcall_string=call_stream.str();\n\t";


  sourceFile << function_pregl_error() << "(call_string.c_str(),call,\""
             << function_name() << "\",file,line,(void*)"
             << function_pointer_name() << ");\n\t";

  if(returns_value())
    {
      sourceFile << "retval=";
    }


  sourceFile << function_pointer_name() << "(" << argument_list_names_only()
             << ");\n\t"
             << function_gl_error() << "(call_string.c_str(),call,\""
             << function_name() << "\",file,line,(void*)"
             << function_pointer_name() << ");\n\t";

  if(function_name()=="glEnd")
    {
      sourceFile << inside_begin_end_pair_counter() << "=0;\n\t";
    }
  if(returns_value())
    {
      sourceFile << "return retval;";
    }
  else
    {
      sourceFile << "//no return value";
    }
  sourceFile << "\n}\n#endif\n\n";




}


///////////////////////////////////////////////////////
// this routine returnes what is the type of the argument
void
openGL_function_info::
GetTypeFromArgumentEntry(const string &inString, ArgumentType &argumentType)
{
  string::size_type startPlace,placeA,placeB;

  //hunt for characters that are not allowed in a name
  //namely, * and ' ' after the leading whitespace

  //first eat up leading spaces and the possible const
  startPlace=inString.rfind("const");

  if(startPlace==string::npos)
    {
      startPlace=0;
    }
  else
    {
      startPlace+=strlen("const");
    }

  startPlace=inString.find_first_not_of(" ",startPlace);
  if(startPlace==string::npos)
    {
      startPlace=0;
    }

  placeA=inString.find_first_of(" *",startPlace);

  std::string tempStr;
  if(placeA!=string::npos)
    {
      //we found a space or *, now we get
      //to the first chacater that is NOT a * or space
      placeB=inString.find_first_not_of(" *",placeA);
      if(placeB!=string::npos)
        {
          std::string str;

          argumentType.m_front = inString.substr(0, placeB);
          str = inString.substr(placeB);
          placeB = str.find_first_of('[');
          if(placeB != string::npos)
            {
              argumentType.m_back = str.substr(placeB);
            }
        }
      else
        {
          argumentType.m_front = inString;
        }
    }
  else
    {
      argumentType.m_front = inString;
    }
}


void
openGL_function_info::
HeaderEnd(ostream &headerFile, const list<string> &fileNames)
{
  end_namespace(GlobalElements::get().m_namespace, headerFile);
}

void
openGL_function_info::
HeaderStart(ostream &headerFile, const list<string> &fileNames)
{
  headerFile << "#pragma once\n\n"
             << "#if defined __WIN32__ && !defined(APIENTRY) && !defined(__CYGWIN__) && !defined(__SCITECH_SNAP__)\n"
             << "#define WIN32_LEAN_AND_MEAN 1\n#include <windows.h>\n#endif\n";

  for(list<string>::const_iterator i=fileNames.begin(); i!=fileNames.end(); ++i)
    {
      headerFile  << "#include <" << *i << ">\n";
    }

  headerFile << "\n\n#ifndef GLAPI\n#define GLAPI extern\n#endif\n"
             << "#ifndef APIENTRY\n#define APIENTRY\n#endif\n"
             << "#ifndef APIENTRYP\n#define APIENTRYP APIENTRY*\n#endif\n";


  begin_namespace(GlobalElements::get().m_namespace, headerFile);

  headerFile << "void* " << function_loader() << "(const char *name);\n"
             << "void " << function_error_loading() << "(const char *fname);\n"
             << "void " << function_call_unloadable_function() << "(const char *fname);\n"
             << "void " << function_gl_error()
             << "(const char *call, const char *src_call, const char *function_name, const char *fileName, int line, void* fptr);\n"
             << "void " << function_pregl_error()
             << "(const char *call, const char *src_call, const char *function_name, const char *fileName, int line, void* fptr);\n"
             << "int  " << inside_begin_end_pair_function() << "(void);\n"
             << "void " << function_load_all() << "(bool emit_load_warning);\n\n";


  headerFile << "#define " << macro_prefix() << "functionExists(name) "
             << GlobalElements::get().m_namespace << "::" << GlobalElements::get().m_function_prefix << "exists_function_##name()\n\n";

  headerFile << "#define " << macro_prefix() << "functionPointer(name) "
             << GlobalElements::get().m_namespace << "::" << GlobalElements::get().m_function_prefix << "get_function_ptr_##name()\n\n";



}


void
openGL_function_info::
SourceEnd(ostream &sourceFile, const list<string> &fileNames)
{
  sourceFile << "\n\nvoid " << function_load_all() << "(bool emit_load_warning)\n{\n\t";
  for(map<string,openGL_function_info*>::iterator i=GlobalElements::get().m_lookUp.begin();
      i!=GlobalElements::get().m_lookUp.end(); ++i)
    {
      if(i->second->m_use_function_pointer)
        {
          sourceFile << i->second->function_pointer_name() << "=("
                     << i->second->function_pointer_type() << ")"
                     << function_loader() << "(\""
                     << i->second->function_name() << "\");\n\t"
                     << "if(" << i->second->function_pointer_name()
                     << "==NULL)\n\t{\n\t\t" << i->second->function_pointer_name()
                     << "=" << i->second->do_nothing_function_name()
                     << ";\n\t\tif(emit_load_warning)\n\t\t\t" << function_error_loading() << "(\""
                     << i->second->function_name() << "\");\n\t"
                     << "}\n\t";
        }
    }
  sourceFile << "\n}\n";

  end_namespace(GlobalElements::get().m_namespace, sourceFile);
}


void
openGL_function_info::
SourceStart(ostream &sourceFile, const list<string> &fileNames)
{
  sourceFile << "#if defined __WIN32__ && !defined(APIENTRY) && !defined(__CYGWIN__) && !defined(__SCITECH_SNAP__)\n"
             << "#define WIN32_LEAN_AND_MEAN 1\n#include <windows.h>\n#endif\n";

  for(list<string>::const_iterator i=fileNames.begin(); i!=fileNames.end(); ++i)
    {
       sourceFile << "#include <" << *i << ">\n";
    }

  sourceFile << "#include <sstream>\n"
             << "#include <iomanip>\n"
	     << "#if defined(__WIN32__)\n"
	     << "#undef GL_APICALL\n#define GL_APICALL\n"
	     << "#undef GL_APIENTRY\n#define GL_APIENTRY\n"
	     << "#endif\n";

  begin_namespace(GlobalElements::get().m_namespace, sourceFile);


  sourceFile << "void* " << function_loader() << "(const char *name);\n"
             << "void " << function_error_loading() << "(const char *fname);\n"
             << "void " << function_call_unloadable_function() << "(const char *fname);\n"
             << "void " << function_gl_error()
             << "(const char *call, const char *src, const char *function_name, const char *fileName, int line, void* fptr);\n"
             << "void " << function_pregl_error()
             << "(const char *call, const char *src, const char *function_name, const char *fileName, int line, void* fptr);\n"
             << "int  " << inside_begin_end_pair_function() << "(void);\n"
             << "void " << function_load_all() << "(void);\n\n";


  sourceFile << "static int " << inside_begin_end_pair_counter() << "=0;\n\n"
             << "int  " << inside_begin_end_pair_function() << "(void)\n"
             << "{\n\rreturn " << inside_begin_end_pair_counter() << "!=0; \n}\n\n";




}



string
RemoveEndOfLines(const string &input)
{
  string retval;
  string::size_type i;

  for(i=0;i<input.length();++i)
    {
      if(input[i]!='\n')
        {
          retval.push_back(input[i]);
        }
    }

  return retval;


}

string
RemoveWhiteSpace(const string &input)
{
  string retval;
  string::size_type i;

  for(i=0;i<input.length();++i)
    {
      if(!isspace(input[i]))
        {
          retval.push_back(input[i]);
        }
    }

  return retval;
}
