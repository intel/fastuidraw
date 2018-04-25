#pragma once

#include <set>
#include "generic_command_line.hpp"

template<typename T>
class command_line_list:
  public command_line_argument,
  public std::set<T>
{
public:
  command_line_list(const std::string &nm,
                    const std::string &desc,
                    command_line_register &p):
    command_line_argument(p),
    m_name(nm)
  {
    std::ostringstream ostr;
    ostr << "\n\t" << m_name << " value"
         << format_description_string(m_name, desc);
    m_description = tabs_to_spaces(ostr.str());
  }

  virtual
  int
  check_arg(const std::vector<std::string> &argv, int location)
  {
    int argc(argv.size());
    if (location + 1 < argc && argv[location] == m_name)
      {
        T v;
        readvalue_from_string(v, argv[location + 1]);
        this->insert(v);
        std::cout << "\n\t" << m_name << " added: ";
        writevalue_to_stream(v, std::cout);
        return 2;
      }
    return 0;
  }

  virtual
  void
  print_command_line_description(std::ostream &ostr) const
  {
    ostr << "[" << m_name << " value] ";
  }

  virtual
  void
  print_detailed_description(std::ostream &ostr) const
  {
    ostr << m_description;
  }

private:
  std::string m_name, m_description;
};
