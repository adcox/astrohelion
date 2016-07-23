/**
 *  @file Traj_cr3bp.hpp
 *	@brief 
 *	
 *	@author Andrew Cox
 *	@version May 25, 2016
 *	@copyright GNU GPL v3.0
 */
/*
 *	Astrohelion 
 *	Copyright 2015, Andrew Cox; Protected under the GNU GPL v3.0
 *	
 *	This file is part of Astrohelion
 *
 *  Astrohelion is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  Astrohelion is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Astrohelion.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "Traj.hpp"


namespace astrohelion{

// Forward Declarations
class Nodeset_cr3bp;
class SysData_cr3bp;

/**
 *	@brief A derivative class of the Traj object, contains 
 *	trajectory information specific to the CR3BP
 *
 *	@author Andrew Cox
 *	@version August 30, 2015
 *	@copyright GNU GPL v3.0
 */
class Traj_cr3bp : public Traj{

public:
	// *structors
	Traj_cr3bp(const SysData_cr3bp*);
	Traj_cr3bp(const Traj_cr3bp&);
	Traj_cr3bp(const BaseArcset&);
	baseArcsetPtr create(const SysData*) const;
	baseArcsetPtr clone() const;

	// Operators
	Traj& operator +=(const Traj&);
	
	// Set and Get Functions
	double getJacobiByIx(int) const;
	void setJacobiByIx(int, double);

	// Utility
	void readFromMat(const char*);
	void saveToMat(const char*) const;
private:

	void initExtraParam();
};

}// END of Astrohelion namespace