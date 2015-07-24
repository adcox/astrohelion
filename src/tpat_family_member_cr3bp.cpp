/**
 *	@file tpat_family_member_cr3bp
 *	@brief Data object for CR3BP family members
 */
/*
 *	Trajectory Propagation and Analysis Toolkit 
 *	Copyright 2015, Andrew Cox; Protected under the GNU GPL v3.0
 *	
 *	This file is part of the Trajectory Propagation and Analysis Toolkit (TPAT).
 *
 *  TPAT is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  TPAT is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with TPAT.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "tpat.hpp"
 
#include "tpat_family_member_cr3bp.hpp"

#include "tpat_constants.hpp"
#include "tpat_exceptions.hpp"
#include "tpat_traj_cr3bp.hpp"
#include "tpat_nodeset_cr3bp.hpp"

//-----------------------------------------------------
// 		Constructors
//-----------------------------------------------------

tpat_family_member_cr3bp::tpat_family_member_cr3bp(double *ic, double tof,
	double jc, double xWid, double yWid, double zWid){
	IC.clear();
	IC.insert(IC.begin(), ic, ic+6);
	TOF = tof;
	JC = jc;
	xWidth = xWid;
	yWidth = yWid;
	zWidth = zWid;
}//====================================================

/**
 *	@brief Create a family member from a trajectory object
 */
tpat_family_member_cr3bp::tpat_family_member_cr3bp(const tpat_traj_cr3bp traj){
	IC = traj.getState(0);
	TOF = traj.getTime(-1);
	JC = traj.getJC(0);

	std::vector<double> x = traj.getCoord(0);
	std::vector<double> y = traj.getCoord(0);
	std::vector<double> z = traj.getCoord(0);

	xWidth = *std::max_element(x.begin(), x.end()) - *std::min_element(x.begin(), x.end());
	yWidth = *std::max_element(y.begin(), y.end()) - *std::min_element(y.begin(), y.end());
	zWidth = *std::max_element(z.begin(), z.end()) - *std::min_element(z.begin(), z.end());
}//===================================================

/**
 *	@brief Copy constructor
 */
tpat_family_member_cr3bp::tpat_family_member_cr3bp(const tpat_family_member_cr3bp& mem){
	copyMe(mem);
}//====================================================

/**
 *	@brief Destructor
 */
tpat_family_member_cr3bp::~tpat_family_member_cr3bp(){
	IC.clear();
}//===================================================


//-----------------------------------------------------
// 		Operators
//-----------------------------------------------------

/**
 *	@brief Assignment operator
 */
tpat_family_member_cr3bp& tpat_family_member_cr3bp::operator= (const tpat_family_member_cr3bp& mem){
	copyMe(mem);
	return *this;
}//====================================================

//-----------------------------------------------------
// 		Set and Get Functions
//-----------------------------------------------------

/**
 *	@brief Retrieve a vector of eigenvalues (of the final STM, likely the Monodromy matrix)
 */
std::vector<cdouble> tpat_family_member_cr3bp::getEigVals() const { return eigVals; }

/**
 *	@brief Retrieve the initial state for this trajectory (non-dim)
 */
std::vector<double> tpat_family_member_cr3bp::getIC() const { return IC; }

/**
 *	@breif Retrieve the Time-Of-Flight along this trajectory (non-dim)
 */
double tpat_family_member_cr3bp::getTOF() const { return TOF; }

/**
 *	@brief Retrieve the Jacobi Constant for this trajectory
 */
double tpat_family_member_cr3bp::getJacobi() const { return JC; }

/**
 *	@brief Retrieve the maximum width in the x-direction
 */
double tpat_family_member_cr3bp::getXWidth() const { return xWidth; }

/**
 *	@brief Retrieve the maximum width in the y-direction
 */
double tpat_family_member_cr3bp::getYWidth() const { return yWidth; }

/**
 *	@brief Retrieve the maximum width in the z-direction
 */
double tpat_family_member_cr3bp::getZWidth() const { return zWidth; }

/**
 *	@brief Set the eigenvalues for this orbit
 *
 *	These should be the eigenvalues of the final STM and/or Monodromy matrix
 *	@param vals the eigenvalues
 */
void tpat_family_member_cr3bp::setEigVals(std::vector<cdouble> vals) {
	if(vals.size() != 6)
		throw tpat_exception("tpat_family_member_cr3bp::setEigVals: There must be 6 eigenvalues");
	eigVals = vals;
}//====================================================

/**
 *	@brief Set the initial state
 *	@param ic The initial state (non-dim)
 */
void tpat_family_member_cr3bp::setIC( std::vector<double> ic ){
if(ic.size() != 6)
	throw tpat_exception("tpat_family_member_cr3bp::setIC: There must be 6 elements!");
	IC = ic;
}

/**
 *	@brief Set the time-of-flight
 *	@param tof The time-of-flight (non-dim)
 */
void tpat_family_member_cr3bp::setTOF( double tof ){ TOF = tof; }

/**
 *	@brief Set the Jacobi Constant
 *	@param jc The Jacobi Constant (non-dim)
 */
void tpat_family_member_cr3bp::setJacobi( double jc ){ JC = jc; }

/**
 *	@brief Set the width of this trajectory in the x-direction (non-dim)
 */
void tpat_family_member_cr3bp::setXWidth(double w){ xWidth = w; }

/**
 *	@brief Set the width of this trajectory in the x-direction (non-dim)
 */
void tpat_family_member_cr3bp::setYWidth(double w){ yWidth = w; }

/**
 *	@brief Set the width of this trajectory in the x-direction (non-dim)
 */
void tpat_family_member_cr3bp::setZWidth(double w){ zWidth = w; }

//-----------------------------------------------------
// 		Utility Functions
//-----------------------------------------------------

/**
 *	@brief Copy an input family member into this one
 *	@param mem some other family member
 */
void tpat_family_member_cr3bp::copyMe(const tpat_family_member_cr3bp& mem){
	eigVals = mem.eigVals;
	IC = mem.IC;
	JC = mem.JC;
	TOF = mem.TOF;
	xWidth = mem.xWidth;
	yWidth = mem.yWidth;
	zWidth = mem.zWidth;
}//===================================================