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

#ifndef H_TRAJ_STEP
#define H_TRAJ_STEP

/**
 *	@brief Derived from tpat_arc_step with specific access methods
 *	for steps along a trajectory object
 */
class tpat_traj_step : pulic tpat_arc_step{

public:
	// *structors
	tpat_traj_step(double*, double);
	tpat_traj_step(double*, double, double*, double*);
	
	// Set and Get functions
	double getTime() const;
	void setTime(double);

private:
	void initArrays();
};

#endif