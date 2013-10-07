%module monica
%include "std_string.i"
%include "std_vector.i"

%{
#define SWIG_COMPILATION
/* Includes the header in the wrapper code */
#include "../src/simulation.h"
#include "../src/monica-parameters.h"
#include "../src/monica.h"
#include "../src/eva_methods.h"
#include "../src/cc_germany_methods.h"
#include "../src/gis_simulation_methods.h"
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
%include "../src/simulation.h"
%include "../src/monica-typedefs.h"
%include "../src/monica-parameters.h"
%include "../src/soiltemperature.h"
%include "../src/soilmoisture.h"
%include "../src/soilorganic.h"
%include "../src/soiltransport.h"
%include "../src/monica.h"
%include "../src/cc_germany_methods.h"
%include "../src/gis_simulation_methods.h"
%include "../src/eva_methods.h"
