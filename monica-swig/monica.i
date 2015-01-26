%module monica
%include "std_string.i"
%include "std_vector.i"
%include "std_map.i"

%{
#define SWIG_COMPILATION
/* Includes the header in the wrapper code */
#include "src/simulation.h"
#include "src/monica-parameters.h"
#include "src/monica.h"
#include "src/eva_methods.h"
#include "src/cc_germany_methods.h"
#include "src/gis_simulation_methods.h"
#include "src/conversion.h"
#include "../util/tools/date.h"
#include "src/carbiocial.h"
#include "src/macsur_scaling_interface.h"
%}

// Instantiate templates used by example
namespace std {
   %template(IntVector) vector<int>;
   %template(DoubleVector) vector<double>;
   %template(StringVector) vector<std::string>;
   %template(PPVector) vector<Monica::ProductionProcess>;   
   %nodefaultdtor Climate::DataAccessor;
   %template() std::pair<int, double>;
   %template(MapIntDouble) map<int, double>;
   %nodefaultdtor Monica::CropPtr;
}

/* Parse the header file to generate wrappers */
%include "src/simulation.h"
%include "src/monica-typedefs.h"
%include "src/monica-parameters.h"
%include "src/soiltemperature.h"
%include "src/soilmoisture.h"
%include "src/soilorganic.h"
%include "src/soiltransport.h"
%include "src/monica.h"
%include "src/carbiocial.h"
%include "src/cc_germany_methods.h"
%include "src/gis_simulation_methods.h"
%include "src/eva_methods.h"
%include "src/macsur_scaling_interface.h"
