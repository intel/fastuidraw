#pragma once

#include <string>
#include <map>
#include <fastuidraw/colorstop.hpp>

#include "generic_command_line.hpp"

class color_stop_arguments:public command_line_argument
{
public:

  class colorstop_data
  {
  public:
    colorstop_data(void):
      m_discretization(16)
    {}

    fastuidraw::ColorStopSequence m_stops;
    int m_discretization;
  };

  typedef std::map<std::string, colorstop_data*> hoard;


  explicit
  color_stop_arguments(command_line_register &parent);

  ~color_stop_arguments();

  virtual
  int
  check_arg(const std::vector<std::string> &args, int location);

  virtual
  void
  print_command_line_description(std::ostream &ostr) const;

  virtual
  void
  print_detailed_description(std::ostream &ostr) const;

  //if necessary allocate the object too.
  colorstop_data&
  fetch(const std::string &pname);

  hoard m_values;

private:

  std::string m_add, m_add2, m_disc;
};
