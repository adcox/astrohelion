/**
 *  @file Traj_2bp.hpp
 *	@brief 
 *	
 *	@author Andrew Cox
 *	@version August 25, 2016
 *	@copyright GNU GPL v3.0
 */
/*
 *	Astrohelion 
 *	Copyright 2016, Andrew Cox; Protected under the GNU GPL v3.0
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
class SysData_2bp;

/**
 *	@brief A derivative class of the Traj object, contains 
 *	trajectory information specific to the CR3BP
 *
 *	@author Andrew Cox
 *	@version August 30, 2015
 *	@copyright GNU GPL v3.0
 */
class Traj_2bp : public Traj{

public:
	// *structors
	Traj_2bp(const SysData_2bp*);
	Traj_2bp(const Traj_2bp&);
	Traj_2bp(const BaseArcset&);
	baseArcsetPtr create(const SysData*) const override;
	baseArcsetPtr clone() const override;

	// Operators
	Traj& operator +=(const Traj&) override;
	
	// Set and Get Functions

	// Utility
	void readFromMat(const char*) override;
	void saveToMat(const char*) const override;
private:

	void initExtraParam();
};

}// END of Astrohelion namespace