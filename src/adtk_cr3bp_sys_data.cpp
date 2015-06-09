/**
 *	@adtk_cr3bp_sys_data.cpp
 *
 *	adtk_cr3bp_sys_data.cpp
 *
 * 	System Data object specifically for CR3BP
 */

#include "adtk_cr3bp_sys_data.hpp"
 
#include "adtk_body_data.hpp"
#include "adtk_constants.hpp"
#include "adtk_utilities.hpp"

#include <cmath>
#include <exception>
#include <iostream>

using namespace std;

/**
 *	@brief Default constructor
 */
adtk_cr3bp_sys_data::adtk_cr3bp_sys_data() : adtk_sys_data(){
	numPrimaries = 2;
	type = adtk_sys_data::CR3BP_SYS;
}//========================================

/**
 *	@brief Create a system data object using data from the two primaries
 *	@param P1 the name of the larger primary
 *	@param P2 the name of the smaller primary; P2 must orbit P1
 */
adtk_cr3bp_sys_data::adtk_cr3bp_sys_data(std::string P1, std::string P2){
	numPrimaries = 2;
	type = adtk_sys_data::CR3BP_SYS;
	
	adtk_body_data p1Data(P1);
	adtk_body_data p2Data(P2);

	primaries.push_back(p1Data.getName());
	primIDs.push_back(p1Data.getID());
	primaries.push_back(p2Data.getName());
	primIDs.push_back(p2Data.getID());

	// Check to make sure P1 is P2's parent
	if(p2Data.getName().compare(p1Data.getName())){
		charL = p2Data.getOrbitRad();
		charM = p1Data.getMass() + p2Data.getMass();
		charT = sqrt(pow(charL, 3)/(G*charM));

		mu = p2Data.getMass()/charM;
	}else{
		cout << "adtk_cr3bp_sys_Data constructor :: P1 must be the parent of P2" << endl;
		throw;
	}
}//===================================================

adtk_cr3bp_sys_data::adtk_cr3bp_sys_data(const adtk_cr3bp_sys_data &d) : adtk_sys_data(d){
	mu = d.mu;
}

/**
 *	@brief Copy operator; makes a clean copy of a data object into this one
 *	@param d a CR3BP system data object
 *	@return this system data object
 */
adtk_cr3bp_sys_data& adtk_cr3bp_sys_data::operator= (const adtk_cr3bp_sys_data &d){
	adtk_sys_data::operator= (d);
	mu = d.mu;
	return *this;
}//===================================================

/**
 *	@return the non-dimensional mass ratio for the system
 */
double adtk_cr3bp_sys_data::getMu() const { return mu; }

/**
 *	@brief Save system data, like the names of the primaries and the system mass ratio, to a .mat file
 *	@param matFile a pointer to the .mat file
 */
void adtk_cr3bp_sys_data::saveToMat(mat_t *matFile){
	size_t dims[2] = {1,1};

	// Initialize character array (larger than needed), copy in the name of the primary, then create a var.
	char p1_str[64];
	strcpy(p1_str, primaries.at(0).c_str());
	dims[1] = primaries.at(0).length();
	matvar_t *p1_var = Mat_VarCreate("P1", MAT_C_CHAR, MAT_T_UTF8, 2, dims, &(p1_str[0]), MAT_F_DONT_COPY_DATA);
	saveVar(matFile, p1_var, "P1", MAT_COMPRESSION_NONE);

	char p2_str[64];
	strcpy(p2_str, primaries.at(1).c_str());
	dims[1] = primaries.at(1).length();
	matvar_t *p2_var = Mat_VarCreate("P2", MAT_C_CHAR, MAT_T_UTF8, 2, dims, &(p2_str[0]), MAT_F_DONT_COPY_DATA);
	saveVar(matFile, p2_var, "P2", MAT_COMPRESSION_NONE);

	dims[1] = 1;	
	matvar_t *mu_var = Mat_VarCreate("Mu", MAT_C_DOUBLE, MAT_T_DOUBLE, 2, dims, &mu, MAT_F_DONT_COPY_DATA);
	saveVar(matFile, mu_var, "Mu", MAT_COMPRESSION_NONE);
}//===================================================