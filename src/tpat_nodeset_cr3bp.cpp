/**
 *  @file tpat_nodeset_cr3bp.cpp
 *	@brief Derivative of tpat_nodeset, specific to CR3BP
 *
 *	@author Andrew Cox
 *	@version September 2, 2015
 *	@copyright GNU GPL v3.0
 */
 
/*
 *  Trajectory Propagation and Analysis Toolkit 
 *  Copyright 2015, Andrew Cox; Protected under the GNU GPL v3.0
 *  
 *  This file is part of the Trajectory Propagation and Analysis Toolkit (TPAT).
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

#include "tpat_nodeset_cr3bp.hpp"

#include "tpat_traj_cr3bp.hpp"
#include "tpat_sys_data_cr3bp.hpp"

//-----------------------------------------------------
//      *structors
//-----------------------------------------------------
/**
 *	@brief Create a nodeset with specified system data
 *	@param data system data object
 */
tpat_nodeset_cr3bp::tpat_nodeset_cr3bp(tpat_sys_data_cr3bp *data) : tpat_nodeset(data){}

/**
 *	@brief Compute a set of nodes by integrating from initial conditions for some time, then split the
 *	integrated trajectory into pieces (nodes).
 *
 *	@param IC a set of initial conditions, non-dimensional units
 *	@param data a pointer to a system data object that describes the model to integrate in
 *	@param tof duration of the simulation, non-dimensional
 *	@param numNodes number of nodes to create, including IC
 *	@param type node distribution type
 */
tpat_nodeset_cr3bp::tpat_nodeset_cr3bp(double IC[6], tpat_sys_data_cr3bp *data, double tof,
	int numNodes, node_distro_t type) : tpat_nodeset(data){

	initSetFromICs(IC, data, 0, tof, numNodes, type);
}//======================================================================

/**
 *	@brief Compute a set of nodes by integrating from initial conditions for some time, then split the
 *	integrated trajectory into pieces (nodes).
 *
 *	@param IC a set of initial conditions, non-dimensional units
 *	@param data a pointer to a system data object that describes the model to integrate in
 *	@param tof duration of the simulation, non-dimensional
 *	@param numNodes number of nodes to create, including IC
 *	@param type node distribution type
 */
tpat_nodeset_cr3bp::tpat_nodeset_cr3bp(std::vector<double> IC, tpat_sys_data_cr3bp *data, double tof,
	int numNodes, node_distro_t type) : tpat_nodeset(data){

	initSetFromICs(&(IC[0]), data, 0, tof, numNodes, type);
}//=====================================================================

/**
 *	@brief Compute a set of nodes by integrating from initial conditions for some time, then split the
 *	integrated trajectory into pieces (nodes).
 *
 *	The type is automatically specified as splitting the trajectory equally in TIME
 *
 *	@param IC a set of initial conditions, non-dimensional units
 *	@param data a pointer to a system data object that describes the model to integrate in
 *	@param tof duration of the simulation, non-dimensional
 *	@param numNodes number of nodes to create, including IC
 */
tpat_nodeset_cr3bp::tpat_nodeset_cr3bp(double IC[6], tpat_sys_data_cr3bp *data, double tof, 
	int numNodes) : tpat_nodeset(data){

	initSetFromICs(IC, data, 0, tof, numNodes, tpat_nodeset::DISTRO_TIME);
}//======================================================================

/**
 *	@brief Compute a set of nodes by integrating from initial conditions for some time, then split the
 *	integrated trajectory into pieces (nodes).
 *
 *	The type is automatically specified as splitting the trajectory equally in TIME
 *
 *	@param IC a set of initial conditions, non-dimensional units
 *	@param data a pointer to a system data object that describes the model to integrate in
 *	@param tof duration of the simulation, non-dimensional
 *	@param numNodes number of nodes to create, including IC
 */
tpat_nodeset_cr3bp::tpat_nodeset_cr3bp(std::vector<double> IC, tpat_sys_data_cr3bp *data, double tof, 
	int numNodes) : tpat_nodeset(data){

	initSetFromICs(&(IC[0]), data, 0, tof, numNodes, tpat_nodeset::DISTRO_TIME);
}//======================================================================

/**
 *	@brief Create a noteset by splitting a trajectory into pieces (nodes)
 *
 *	The node distribution type is automatically specified as splitting the trajectory equally in TIME
 *
 *	@param traj the trajectory to split
 *	@param numNodes the number of nodes.
 */
tpat_nodeset_cr3bp::tpat_nodeset_cr3bp(tpat_traj_cr3bp traj, int numNodes) : tpat_nodeset(traj.getSysData()){
	initSetFromTraj(traj, traj.getSysData(), numNodes, tpat_nodeset::DISTRO_TIME);
}//===========================================

/**
 *	@brief Create a noteset by splitting a trajectory into pieces (nodes)
 *
 *	@param traj the trajectory to split
 *	@param numNodes the number of nodes.
 *	@param type the node distribution type
 */
tpat_nodeset_cr3bp::tpat_nodeset_cr3bp(tpat_traj_cr3bp traj, int numNodes,
	node_distro_t type) : tpat_nodeset(traj.getSysData()){
	
	initSetFromTraj(traj, traj.getSysData(), numNodes, type);
}//===========================================

/**
 *	@brief Create a nodeset as a subset of another
 *	@param orig Original nodeset
 *	@param first index of the first node to be included in the new nodeset
 *	@param last index of the last node to be included in the new nodeset
 */
tpat_nodeset_cr3bp::tpat_nodeset_cr3bp(const tpat_nodeset_cr3bp &orig, int first,
	int last) : tpat_nodeset(orig, first, last){}

/**
 *	@brief Copy input nodeset. 
 *
 *	This function calls the base-class copy constructor to
 *	handle copying the generic fields like state and tofs
 *	@param n a nodeset
 */
tpat_nodeset_cr3bp::tpat_nodeset_cr3bp(const tpat_nodeset_cr3bp& n) : tpat_nodeset(n) {}

/**
 *	@brief Create a CR3BP nodeset from its base class
 *	@param a an arc data reference
 */
tpat_nodeset_cr3bp::tpat_nodeset_cr3bp(const tpat_arc_data &a) : tpat_nodeset(a) {}

//-----------------------------------------------------
//      Operators
//-----------------------------------------------------

//-----------------------------------------------------
//      Set and Get Functions
//-----------------------------------------------------

//-----------------------------------------------------
//      Utility Functions
//-----------------------------------------------------