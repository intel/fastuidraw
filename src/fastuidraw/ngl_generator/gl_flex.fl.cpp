/*!
 * \file gl_flex.fl.cpp
 * \brief file gl_flex.fl.cpp
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
 * \file gl_flex.fl.cpp
 * \brief file gl_flex.fl.cpp
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



%option noyywrap

%{
#include "HeaderCreator.hpp"


%}
/* definitions */
space [ \t]+
allSpace [ \t\n]
anychar .|{allSpace}
const "const"{allSpace}+
GLTYPEARB GLchar|GLcharARB|GLintptr|GLintpreARB|GLsizeiptr|GLsizeiptrARB|GLhandleARB|GLhalfARB|GLhalfNV
GLTYPESIMPLE GLenum|GLbitfield|GLboolean|GLsizei|GLvoid|GLuint64EXT|GLuint64|GLint64|GLint64EXT
GLTYPEBYTE GLbyte|GLubyte
GLTYPESHORT GLshort|GLushort
GLTYPEINT GLint|GLuint|int
GLTYPEFLOAT GLfloat|GLdouble|GLclampf|GLclampd|float|double
GLTYE {GLTYPEARB}|{GLTYPESIMPLE}|{GLTYPEBYTE}|{GLTYPESHORT}|{GLTYPEINT}|{GLTYPEFLOAT}|void|wchar_t
GLPTR {GLTYE}{allSpace}*"*"
GLTYPE {GLTYE}|{GLPTR}
CGLTYPE {const}{GLTYPE}
CGLGLTYPE {CGLTYPE}|{GLTYPE}|GLDEBUGPROC|GLDEBUGPROCARB|GLVULKANPROCNV

EGLPLATFORMTYPE EGLint|EGLNativeDisplayType|EGLNativePixmapType|EGLNativeWindowType
EGLBASETYPE void|EGLBoolean|EGLDisplay|EGLConfig|EGLSurface|EGLContext|EGLenum|EGLClientBuffer|EGLSync|EGLAttrib|EGLTime|EGLImage
EGLKHRTYPE EGLSyncKHR|EGLAttribKHR|EGLLabelKHR|EGLObjectKHR|EGLTimeKHR|EGLImageKHR|EGLStreamKHR|EGLuint64KHR|EGLNativeFileDescriptorKHR
EGLANDTYPE EGLsizeiANDROID|EGLSetBlobFuncANDROID|EGLsizeiANDROID|EGLnsecsANDROID
EGLEXTTYPE EGLDeviceEXT|EGLOutputLayerEXT|EGLOutputPortEXT|EGLSyncNV|EGLTimeNV|EGLuint64NV|struct{space}EGLClientPixmapHI
EGLTYP {EGLPLATFORMTYPE}|{EGLBASETYPE}|{EGLKHRTYPE}|{EGLANDTYPE}|{EGLEXTTYPE}

EGLPTR {EGLTYP}{allSpace}*"*"
EGLTYPE {EGLTYP}|{EGLPTR}
CEGLTYPE {const}{EGLTYPE}
CEGLEGLTYPE {EGLTYPE}|{CEGLTYPE}

%%

%{
 /*rules*/
%}

GLAPI{space}+{CGLGLTYPE}{space}*+APIENTRY{space}+gl[^\n]*\n  {
  openGL_function_info *ptr;
  ptr=new openGL_function_info(yytext, "GLAPI", "APIENTRY");
  openGL_function_info::openGL_functionList().push_back(ptr);
}

GL_APICALL{space}+{CGLGLTYPE}{space}*+GL_APIENTRY{space}+gl[^\n]*\n  {
  openGL_function_info *ptr;
  ptr=new openGL_function_info(yytext,"GL_APICALL", "GL_APIENTRY");
  openGL_function_info::openGL_functionList().push_back(ptr);
}

EGLAPI{space}+{CEGLEGLTYPE}{space}*+EGLAPIENTRY{space}+egl[^\n]*\n {
  openGL_function_info *ptr;
  ptr=new openGL_function_info(yytext,"EGLAPI", "EGLAPIENTRY", "egl");
  /* disclude if function name is eglGetProcAddress */
  if (ptr->function_name() != "eglGetProcAddress")
    {
      openGL_function_info::openGL_functionList().push_back(ptr);
    }
  else
    {
      delete ptr;
    }
}

FUNCTIONPOINTERMODE {
  openGL_function_info::use_function_pointer_mode()=use_function_pointer;
}

NOFUNCTIONPOINTERMODE_PTR_TYPE_DECLARED {
  openGL_function_info::use_function_pointer_mode()=dont_use_function_pointer_type_declared;
}

NOFUNCTIONPOINTERMODE_PTR_TYPE_NOTDECLARED {
  openGL_function_info::use_function_pointer_mode()=dont_use_function_pointer_type_undeclared;
}


[ \t\n]+  { /* do nothing */  }
. { /* do nothing */  }
%%

/*code*/


int main(int argc, char **argv)
{
  //read input from stdin!
  //arguments affect behavior.

  string headerName, sourceName, current;
  string macroPrefix(""), function_prefix(""), namespaceName;
  string path, output_cpp("kgl.cpp"), output_hpp("kgl.hpp");
  ofstream sourceFile, headerFile;
  list<string> fileNames;
  int num;
  map<string,openGL_function_info*>::iterator gg;

  for(int i=1;i<argc;++i)
    {
      string::iterator iter;

      current=argv[i];
      iter=find(current.begin(), current.end(), '=');
      if (iter!=current.end() && iter+1!=current.end())
        {
          if (string(current.begin(),iter)==string("macro_prefix"))
            {
              macroPrefix=string(iter+1,current.end());
            }

          if (string(current.begin(),iter)==string("function_prefix"))
            {
              function_prefix=string(iter+1,current.end());
            }

          if (string(current.begin(),iter)==string("namespace"))
            {
              namespaceName=string(iter+1,current.end());
            }

          if (string(current.begin(),iter)==string("path"))
            {
              path=string(iter+1,current.end());
            }

          if (string(current.begin(),iter)==string("output_cpp"))
            {
              output_cpp=string(iter+1,current.end());
            }

          if (string(current.begin(),iter)==string("output_hpp"))
            {
              output_hpp=string(iter+1,current.end());
            }
        }
      else //not a command parameter, then is a file.
        {
          fileNames.push_back(current);
        }
    }

  headerName=output_hpp;
  sourceName=output_cpp;

  sourceFile.open(sourceName.c_str());
  headerFile.open(headerName.c_str());
  if (!sourceFile || !headerFile)
    {
      cerr << "Failed to open one file for writing!\n";
      exit(-1);
    }

  openGL_function_info::SetMacroPrefix(macroPrefix);
  openGL_function_info::SetFunctionPrefix(function_prefix);
  openGL_function_info::SetNamespace(namespaceName);

  //read from stdin the function prototypes
  YY_FLUSH_BUFFER;
  yyrestart(stdin);
  yylex();

  openGL_function_info::HeaderStart(headerFile, fileNames);
  openGL_function_info::SourceStart(sourceFile, fileNames);

  for(num=0,gg=openGL_function_info::lookUp().begin(); gg!=openGL_function_info::lookUp().end(); ++gg, ++num)
    {

      gg->second->output_to_header(headerFile);
      gg->second->output_to_source(sourceFile);

      //gg->second->GetInfo(std::cout);
      //std::cout << gg->second->function_name() << "\n";

    }

  openGL_function_info::HeaderEnd(headerFile, fileNames);
  openGL_function_info::SourceEnd(sourceFile, fileNames);

  //  cout << "\nGL functions counted=" << openGL_function_info::numberFunctions()
  //   << "\n";

}
