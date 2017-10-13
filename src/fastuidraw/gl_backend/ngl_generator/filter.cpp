/*!
 * \file filter.cpp
 * \brief file filter.cpp
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
 * \file filter.cpp
 * \brief file filter.cpp
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



#include <iostream>
#include <fstream>
#include <set>
#include <string>

bool
use_function_pointer(const std::string &filename)
{
  return filename.find("gl2.h")==std::string::npos
    || filename.find("egl.h")==std::string::npos;
}



int main(int argc, char **argv)
{
  /*read each file passed and output them to stdout after filtering */

  std::set<std::string> fileNames;

  for(int i=1;i<argc;++i)
    {
      fileNames.insert(argv[i]);
    }

  for(std::set<std::string>::const_iterator i=fileNames.begin(); i!=fileNames.end(); ++i)
    {
      std::ifstream inFile;

      inFile.open(i->c_str());
      if(inFile)
        {
          int parenCount;
          bool last_char_is_white;

          if(use_function_pointer(*i))
            {
              std::cout << "\nFUNCTIONPOINTERMODE\n";
            }
          else
            {
              std::cout << "\nNONFUNCTIONPOINTERMODE\n";
            }

          parenCount=0; last_char_is_white=false;
          while(inFile)
            {
              char ch;


              inFile.get(ch);
              if(ch=='(')
                {
                  ++parenCount;
                  std::cout << ch;
                }
              else if(ch==')')
                {
                  --parenCount;
                  std::cout << ch;
                }
              else if(ch=='\n' and parenCount>0)
                {

                }
              else if(last_char_is_white and isspace(ch) and ch!='\n')
                {

                }
              else
                {
                  std::cout << ch;
                  last_char_is_white=isspace(ch) and ch!='\n';
                }
            }
          inFile.close();
        }
    }

  return 0;
}
