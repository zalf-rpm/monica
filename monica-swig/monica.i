%module monica
%include "std_string.i"
%include "std_vector.i"
%include "std_map.i"
%include "std_shared_ptr.i"

%shared_ptr(Monica::WorkStep)
%shared_ptr(Monica::Seed)
%shared_ptr(Monica::Harvest)
%shared_ptr(Monica::Cutting)
%shared_ptr(Monica::MineralFertiliserApplication)
%shared_ptr(Monica::OrganicFertiliserApplication)
%shared_ptr(Monica::TillageApplication)
%shared_ptr(Monica::IrrigationApplication)

%{
#define SWIG_COMPILATION
/* Includes the header in the wrapper code */
#include "src/simulation.h"
#include "src/monica-parameters.h"
#include "src/monica.h"
//#include "src/eva_methods.h"
#include "src/carbiocial.h"
%}

// Instantiate templates used by example
namespace std {
   %template(IntVector) vector<int>;
   %template(DoubleVector) vector<double>;
   %template(StringVector) vector<std::string>;
   %template(PPVector) vector<Monica::ProductionProcess>; 	
   %nodefaultdtor Climate::DataAccessor;
	 %template(IntDoubleMap) map<int, double>;
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
//%include "src/eva_methods.h"
%include "src/carbiocial.h"
