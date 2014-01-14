%module monica
%include "std_string.i"
%include "std_vector.i"

%{
#define SWIG_COMPILATION
/* Includes the header in the wrapper code */
#include "/home/specka/devel/github/monica/src/simulation.h"
#include "/home/specka/devel/github/monica/src/monica-parameters.h"
#include "/home/specka/devel/github/monica/src/monica.h"
#include "macsur_scaling_interface.h"
%}


// Instantiate templates used by example
namespace std {
   %template(IntVector) vector<int>;
   %template(DoubleVector) vector<double>;
   %template(StringVector) vector<std::string>;
   %template(PPVector) vector<Monica::ProductionProcess>;   
   %nodefaultdtor Climate::DataAccessor;
   
}


/* Parse the header file to generate wrappers */
%include "/home/specka/devel/github/monica/src/simulation.h"
%include "/home/specka/devel/github/monica/src/monica-typedefs.h"
%include "/home/specka/devel/github/monica/src/monica-parameters.h"
%include "/home/specka/devel/github/monica/src/soiltemperature.h"
%include "/home/specka/devel/github/monica/src/soilmoisture.h"
%include "/home/specka/devel/github/monica/src/soilorganic.h"
%include "/home/specka/devel/github/monica/src/soiltransport.h"
%include "/home/specka/devel/github/monica/src/monica.h"
%include "macsur_scaling_interface.h"
