/*
  Copyright (c) 2009, Kevin Rogovin All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

    * Redistributions of source code must retain the above copyright
    * notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above
    * copyright notice, this list of conditions and the following
    * disclaimer in the documentation and/or other materials provided
    * with the distribution.  Neither the name of the Kevin Rogovin or
    * kRogue Technologies  nor the names of its contributors may
    * be used to endorse or promote products derived from this
    * software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/


/** \file generic_command_line.hpp */
#pragma once

#include <iostream>
#include <vector>
#include <algorithm>
#include <map>
#include <string>
#include <sstream>
#include <ciso646>
#include <fastuidraw/util/util.hpp>

class command_line_register;
class command_line_argument;


//!\class command_line_register
/*!
  A command_line_register walks the a command
  line argument list, calling its command_line_argument
  children's check_arg() method to get the values
  from a command line argument list.
 */
class command_line_register:fastuidraw::noncopyable
{
private:
  friend class command_line_argument;

  std::vector<command_line_argument*> m_children;

public:

  command_line_register(void)
  {}

  ~command_line_register();

  void
  parse_command_line(int argc, char **argv);

  void
  parse_command_line(const std::vector<std::string> &strings);


  void
  print_help(std::ostream&) const;

  void
  print_detailed_help(std::ostream&) const;



};

//!\class command_line_argument
/*!
  A command_line_argument reads from an argument
  list to set a value.
 */
class command_line_argument:fastuidraw::noncopyable
{
private:

  friend class command_line_register;

  int m_location;
  command_line_register *m_parent;

public:

  explicit
  command_line_argument(command_line_register &parent):
    m_parent(&parent)
  {
    m_location=parent.m_children.size();
    parent.m_children.push_back(this);
  }

  command_line_argument(void):
    m_location(-1),
    m_parent(nullptr)
  {}

  virtual
  ~command_line_argument();

  void
  attach(command_line_register &p)
  {
    FASTUIDRAWassert(m_parent==nullptr);

    m_parent=&p;
    m_location=m_parent->m_children.size();
    m_parent->m_children.push_back(this);
  }

  command_line_register*
  parent(void)
  {
    return m_parent;
  }
  //!\fn check_arg
  /*!
    To be implemented by a dervied class.

    \param args the argv argument of main(int argc, char **argv)
                as an array of std::string, note that
                args.size() equals argc
    \param location current index into args[] being examined

    returns the number of arguments consumed, 0 is allowed.

   */
  virtual
  int
  check_arg(const std::vector<std::string> &args,
            int location)=0;

  virtual
  void
  print_command_line_description(std::ostream&) const=0;

  virtual
  void
  print_detailed_description(std::ostream&) const=0;

  static
  std::string
  format_description_string(const std::string &name, const std::string &desc);

  static
  std::string
  produce_formatted_detailed_description(const std::string &cmd,
                                         const std::string &desc);

  static
  std::string
  tabs_to_spaces(const std::string &pin);
};

/*
  not a command line option, but prints a separator for detailed help
 */
class command_separator:public command_line_argument
{
public:
  command_separator(const std::string &label,
                    command_line_register &parent):
    command_line_argument(parent),
    m_label(label)
  {}

  virtual
  int
  check_arg(const std::vector<std::string> &, int)
  {
    return 0;
  }

  virtual
  void
  print_command_line_description(std::ostream&) const
  {}

  virtual
  void
  print_detailed_description(std::ostream &ostr) const
  {
    ostr << "\n\n---------- " << m_label << " ------------------\n";
  }

private:
  std::string m_label;
};
/*
  not a command line option, but prints an about
 */
class command_about:public command_line_argument
{
public:
  command_about(const std::string &label,
                command_line_register &parent):
    command_line_argument(parent),
    m_label(tabs_to_spaces(format_description_string("", label)))
  {}

  virtual
  int
  check_arg(const std::vector<std::string> &, int)
  {
    return 0;
  }

  virtual
  void
  print_command_line_description(std::ostream&) const
  {}

  virtual
  void
  print_detailed_description(std::ostream &ostr) const
  {
    ostr << "\n\n " << m_label << "\n";
  }

private:
  std::string m_label;
};




template<typename T>
void
readvalue_from_string(T &value,
                      const std::string &value_string)
{
  std::istringstream istr(value_string);
  istr >> value;
}


template<>
inline
void
readvalue_from_string(std::string &value,
                      const std::string &value_string)
{
  value=value_string;
}


template<>
inline
void
readvalue_from_string(bool &value, const std::string &value_string)
{
  if (value_string==std::string("on")
     or value_string==std::string("true"))
    {
      value=true;
    }
  else if (value_string==std::string("off")
     or value_string==std::string("false"))
    {
      value=false;
    }
}

template<typename T>
void
writevalue_to_stream(const T&value, std::ostream &ostr)
{
  ostr << value;
}

template<>
inline
void
writevalue_to_stream(const bool &value, std::ostream &ostr)
{
  if (value)
    {
      ostr << "on/true";
    }
  else
    {
      ostr << "off/false";
    }
}


template<typename T>
class enumerated_string_type
{
public:
  typedef std::pair<std::string, std::string> label_desc;

  std::map<std::string, T> m_value_strings;
  std::map<T, label_desc> m_value_Ts;

  enumerated_string_type&
  add_entry(const std::string &label, T v, const std::string &description)
  {
    m_value_strings[label]=v;
    m_value_Ts[v]=label_desc(label, description);
    return *this;
  }
};


template<typename T>
class enumerated_type
{
public:
  T m_value;
  enumerated_string_type<T> m_label_set;

  enumerated_type(T v, const enumerated_string_type<T> &L):
    m_value(v),
    m_label_set(L)
  {}

};



template<typename T>
class command_line_argument_value:public command_line_argument
{
private:
  std::string m_name;
  std::string m_description;
  bool m_set_by_command_line, m_print_at_set;

public:
  T m_value;

  command_line_argument_value(T v, const std::string &nm,
                              const std::string &desc,
                              command_line_register &p,
                              bool print_at_set=true):
    command_line_argument(p),
    m_name(nm),
    m_set_by_command_line(false),
    m_print_at_set(print_at_set),
    m_value(v)
  {
    std::ostringstream ostr;
    ostr << "\n\t"
         << m_name << " (default value=";

    writevalue_to_stream(m_value, ostr);
    ostr << ") " << format_description_string(m_name, desc);
    m_description=tabs_to_spaces(ostr.str());
  }

  const std::string&
  label(void) const
  {
    return m_name;
  }

  bool
  set_by_command_line(void) const
  {
    return m_set_by_command_line;
  }

  virtual
  void
  on_set_by_command_line(void)
  {}

  virtual
  int
  check_arg(const std::vector<std::string> &argv, int location)
  {
    const std::string &str(argv[location]);
    std::string::const_iterator iter;
    int argc(argv.size());

    iter=std::find(str.begin(), str.end(), '=');
    if (iter==str.end())
      {
        iter=std::find(str.begin(), str.end(), ':');
      }

    if (iter!=str.end() and m_name==std::string(str.begin(), iter) )
      {
        std::string val(iter+1, str.end());

        readvalue_from_string(m_value, val);

        if (m_print_at_set)
          {
            std::cout << "\n\t" << m_name
                      << " set to ";
            writevalue_to_stream(m_value, std::cout);
          }

        m_set_by_command_line=true;
        on_set_by_command_line();
        return 1;
      }
    else if (location<argc-1 and str==m_name)
      {
        const std::string &val(argv[location+1]);

        readvalue_from_string(m_value, val);

        if (m_print_at_set)
          {
            std::cout << "\n\t" << m_name
                      << " set to ";
            writevalue_to_stream(m_value, std::cout);
          }
        m_set_by_command_line=true;
        on_set_by_command_line();
        return 2;
      }

    return 0;
  }

  virtual
  void
  print_command_line_description(std::ostream &ostr) const
  {
    ostr << "[" << m_name << "=value] "
         << "[" << m_name << ":value] "
         << "[" << m_name << " value]";
  }

  virtual
  void
  print_detailed_description(std::ostream &ostr) const
  {
    ostr << m_description;
  }

};


template<typename T>
class enumerated_command_line_argument_value:public command_line_argument
{
private:
  std::string m_name;
  std::string m_description;
  bool m_set_by_command_line, m_print_at_set;

public:
  enumerated_type<T> m_value;

  enumerated_command_line_argument_value(T v, const enumerated_string_type<T> &L,
                                         const std::string &nm, const std::string &desc,
                                         command_line_register &p,
                                         bool print_at_set=true):
    command_line_argument(p),
    m_name(nm),
    m_set_by_command_line(false),
    m_print_at_set(print_at_set),
    m_value(v,L)
  {
    std::ostringstream ostr;
    typename std::map<T, typename enumerated_string_type<T>::label_desc>::const_iterator iter, end;

    ostr << "\n\t"
         << m_name << " (default value=";

    iter=m_value.m_label_set.m_value_Ts.find(v);
    if (iter!=m_value.m_label_set.m_value_Ts.end())
      {
        ostr << iter->second.first;
      }
    else
      {
        ostr << v;
      }

    ostr << ")";
    std::ostringstream ostr_desc;
    ostr_desc << desc << " Possible values:\n\n";
    for(iter=m_value.m_label_set.m_value_Ts.begin(),
          end=m_value.m_label_set.m_value_Ts.end();
        iter!=end; ++iter)
      {
        ostr_desc << iter->second.first << ":" << iter->second.second << "\n\n";
      }
    ostr << format_description_string(m_name, ostr_desc.str());
    m_description=tabs_to_spaces(ostr.str());
  }

  bool
  set_by_command_line(void)
  {
    return m_set_by_command_line;
  }


  virtual
  int
  check_arg(const std::vector<std::string> &argv, int location)
  {
    const std::string &str(argv[location]);
    std::string::const_iterator iter;
    int argc(argv.size());

    iter=std::find(str.begin(), str.end(), '=');
    if (iter==str.end())
      {
        iter=std::find(str.begin(), str.end(), ':');
      }

    if (iter!=str.end() and m_name==std::string(str.begin(), iter) )
      {
        std::string val(iter+1, str.end());
        typename std::map<std::string, T>::const_iterator iter;

        iter=m_value.m_label_set.m_value_strings.find(val);
        if (iter!=m_value.m_label_set.m_value_strings.end())
          {
            m_value.m_value=iter->second;

            if (m_print_at_set)
              {
                std::cout << "\n\t" << m_name
                          << " set to " << val;
              }
          }
        m_set_by_command_line=true;
        return 1;
      }
    else if (location<argc-1 and str==m_name)
      {
        const std::string &val(argv[location+1]);
        typename std::map<std::string, T>::const_iterator iter;

        iter=m_value.m_label_set.m_value_strings.find(val);
        if (iter!=m_value.m_label_set.m_value_strings.end())
          {
            m_value.m_value=iter->second;
            if (m_print_at_set)
              {
                std::cout << "\n\t" << m_name
                          << " set to " << val;
              }
          }
        m_set_by_command_line=true;
        return 2;
      }

    return 0;
  }

  virtual
  void
  print_command_line_description(std::ostream &ostr) const
  {
    ostr << "[" << m_name << "=value] "
         << "[" << m_name << ":value] "
         << "[" << m_name << " value]";
  }

  virtual
  void
  print_detailed_description(std::ostream &ostr) const
  {
    ostr << m_description;
  }



};
