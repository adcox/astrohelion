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

#ifndef H_TPAT_NODE
#define H_TPAT_NODE

#include "tpat_linkable.hpp"

#include "tpat_constraint.hpp"

#include <cmath>
#include <vector>
// Forward Declarations


/**
 *	@brief A brief description
 *
 *	@author Andrew Cox
 *	@version 
 *	@copyright GNU GPL v3.0
 */
class TPAT_Node : public TPAT_Linkable{

public:
	// *structors
	TPAT_Node();
	TPAT_Node(const double[6], double);
	TPAT_Node(std::vector<double>, double);
	TPAT_Node(const double[6], const double[3], double);
	TPAT_Node(std::vector<double>, std::vector<double>, double);
	TPAT_Node(const TPAT_Node&);
	// ~TPAT_Node();

	// Operators
	TPAT_Node& operator =(const TPAT_Node&);
	friend bool operator ==(const TPAT_Node&, const TPAT_Node&);
	friend bool operator !=(const TPAT_Node&, const TPAT_Node&);

	// Set and Get functions
	void addConstraint(TPAT_Constraint);
	void clearConstraints();
	std::vector<double> getAccel() const;
	std::vector<TPAT_Constraint> getConstraints() const;
	double getEpoch() const;
	double getExtraParam(int) const;
	std::vector<double> getExtraParams() const;
	int getNumCons() const;
	std::vector<double> getState() const;
	void removeConstraint(int);
	void setAccel(const double*);
	void setAccel(std::vector<double>);
	void setConstraints(std::vector<TPAT_Constraint>);
	void setConstraintID(int);
	void setEpoch(double);
	void setExtraParam(int, double);
	void setExtraParams(std::vector<double>);
	void setState(const double*);
	void setState(std::vector<double>);

protected:
	virtual void copyMe(const TPAT_Node&);

	double state[6] = {NAN, NAN, NAN, NAN, NAN, NAN};	//!< Stores 3 position and 3 velocity states
	double accel[3] = {NAN, NAN, NAN};					//!< Stores 3 acceleration states
	double epoch = 0;	//!< The epoch associated with this node, relative to some base epoch

	/** Stores extra parameters like mass, costates, etc. */
	std::vector<double> extraParam {};

	/** Stores constraints on this node (especially usefull in nodesets) */
	std::vector<TPAT_Constraint> cons {};
};

#endif