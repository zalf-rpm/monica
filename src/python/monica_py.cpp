#include "core/monica.h"
#include <boost/python.hpp>

BOOST_PYTHON_MODULE(monica_py)
{
  using namespace boost::python;
  def("greet", Monica::greet);
}