/**
 *  @file Arcset_cr3bp.cpp
 *	@brief 
 *	
 *	@author Andrew Cox
 *	@version May 1, 2017
 *	@copyright GNU GPL v3.0
 */
/*
 *	Astrohelion 
 *	Copyright 2015-2018, Andrew Cox; Protected under the GNU GPL v3.0
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

#include "Arcset_cr3bp.hpp"

#include "Exceptions.hpp"
#include "SysData_cr3bp.hpp"
#include "Utilities.hpp"

namespace astrohelion{

//-----------------------------------------------------
//      *structors
//-----------------------------------------------------

/**
 *	@brief Create an arcset with specified system data
 *	@param pData system data object
 */
Arcset_cr3bp::Arcset_cr3bp(const SysData_cr3bp *pData) : Arcset(pData){}

/**
 *	@brief Copy input arcset. 
 *
 *	This function calls the base-class copy constructor to
 *	handle copying the generic fields like state and tofs
 *	@param n a arcset
 */
Arcset_cr3bp::Arcset_cr3bp(const Arcset_cr3bp& n) : Arcset(n) {}

/**
 *	@brief Create a CR3BP arcset from its base class
 *	@param a an arc data reference
 */
Arcset_cr3bp::Arcset_cr3bp(const BaseArcset &a) : Arcset(a) {}

/**
 *  @brief Create a new arcset object on the stack
 *  @details the `delete` function must be called to 
 *  free the memory allocated to this object to avoid 
 *  memory leaks
 * 
 *  @param pSys pointer to a system data object; should be a 
 *  CR3BP system as the pointer will be cast to that derived class
 *  @return a pointer to the newly created arcset
 */
baseArcsetPtr Arcset_cr3bp::create( const SysData *pSys) const{
	const SysData_cr3bp *crSys = static_cast<const SysData_cr3bp*>(pSys);
	return baseArcsetPtr(new Arcset_cr3bp(crSys));
}//====================================================

/**
 *  @brief Create a new arcset object on the stack that is a 
 *  duplicate of this object
 *  @details the `delete` function must be called to 
 *  free the memory allocated to this object to avoid 
 *  memory leaks
 * 
 *  @return a pointer to the newly cloned arcset
 */
baseArcsetPtr Arcset_cr3bp::clone() const{
	return baseArcsetPtr(new Arcset_cr3bp(*this));
}//====================================================

//-----------------------------------------------------
//      Operators
//-----------------------------------------------------


//-----------------------------------------------------
//      Set and Get Functions
//-----------------------------------------------------

/**
 *  @brief Get the Jacobi constant value associated with a node
 *  with the specified ID
 * 
 *  @param id the ID of a node
 *  @return the Jacobi constant value
 *  @throw Exception if `id` is out of bounds
 */
double Arcset_cr3bp::getJacobi(int id){
	if(nodeIDMap.count(id) == 0)
		throw Exception("Arcset_cr3bp::getJacobi: Node ID out of range");

	try{
		return nodes[nodeIDMap[id]].getExtraParam(PARAMKEY_JACOBI);
	}catch(const Exception &e){
		std::vector<double> state = nodes[nodeIDMap[id]].getState();
		double C = DynamicsModel_cr3bp::getJacobi(&(state.front()), static_cast<const SysData_cr3bp *>(pSysData)->getMu());
		nodes[nodeIDMap[id]].setExtraParam(PARAMKEY_JACOBI, C);
		return C;
	}
}//====================================================


/**
 *  @brief Get the Jacobi constant value associated with a node
 *  with the specified ID
 * 
 *  @param id the ID of a node
 *  @return the Jacobi constant value
 *  @throw Exception if `id` is out of bounds
 */
double Arcset_cr3bp::getJacobi(int id) const{
	if(nodeIDMap.count(id) == 0)
		throw Exception("Arcset_cr3bp::getJacobi: Node ID out of range");

	try{
		return nodes[nodeIDMap.at(id)].getExtraParam(PARAMKEY_JACOBI);
	}catch(const Exception &e){
		std::vector<double> state = nodes[nodeIDMap.at(id)].getState();
		return DynamicsModel_cr3bp::getJacobi(&(state.front()), static_cast<const SysData_cr3bp *>(pSysData)->getMu());
	}
}//====================================================

/**
 *	@brief Retrieve the value of Jacobi's Constant at the specified step
 *	@details If the specified node does not include a precomputed Jacobi
 *	constant value, the value is computed and stored in the node for
 *	future reference.
 *	
 *	@param ix step index; if < 0, counts backwards from end of trajectory
 *	@return Jacobi at the specified step
 *	@throws Exception if `ix` is out of bounds
 */
double Arcset_cr3bp::getJacobiByIx(int ix){
	if(ix < 0)
		ix += nodes.size();

	if(ix < 0 || ix > static_cast<int>(nodes.size()))
		throw Exception("Arcset_cr3bp::getJacobiByIx: invalid node index");

	/* 	Try to get the Jacobi value from the extra param vector. If it doesn't exist,
	 *	an Exception is thrown, and the value is computed, saved for later referencing,
	 *	and returned.
	 *	
	 *	By default Jacobi Constant is not computed during integration to improve speed
	 */
	try{
		return nodes[ix].getExtraParam(PARAMKEY_JACOBI);
	}catch(const Exception &e){
		std::vector<double> state = nodes[ix].getState();
		double C = DynamicsModel_cr3bp::getJacobi(&(state.front()), static_cast<const SysData_cr3bp *>(pSysData)->getMu());
		nodes[ix].setExtraParam(PARAMKEY_JACOBI, C);
		return C;
	}
}//====================================================

/**
 *	@brief Retrieve the value of Jacobi's Constant at the specified step
 *	@details If the specified node does not include a precomputed Jacobi
 *	constant value, the value is computed but is NOT stored in order
 *	to preserve the constant trajectory object.
 *	
 *	@param ix step index; if < 0, counts backwards from end of trajectory
 *	@return Jacobi at the specified step
 *	@throws Exception if `ix` is out of bounds
 */
double Arcset_cr3bp::getJacobiByIx(int ix) const{
	if(ix < 0)
		ix += nodes.size();

	if(ix < 0 || ix > static_cast<int>(nodes.size()))
		throw Exception("Arcset_cr3bp::getJacobiByIx_const: invalid node index");

	/* 	Try to get the Jacobi value from the extra param vector. If it doesn't exist,
	 *	an Exception is thrown, and the value is computed, saved for later referencing,
	 *	and returned.
	 *	
	 *	By default Jacobi Constant is not computed during integration to improve speed
	 */
	try{
		return nodes[ix].getExtraParam(PARAMKEY_JACOBI);
	}catch(const Exception &e){
		std::vector<double> state = nodes[ix].getState();
		return DynamicsModel_cr3bp::getJacobi(&(state.front()), static_cast<const SysData_cr3bp *>(pSysData)->getMu());
	}
}//====================================================

/**
 *  @brief Set the Jacobi constant value associated with a node
 *  with the specified ID
 * 
 *  @param id the ID of a node
 *  @param jacobi Jacobi constant value
 *  @throw Exception if `id` is out of bounds
 */
void Arcset_cr3bp::setJacobi(int id, double jacobi){
	if(nodeIDMap.count(id) == 0)
		throw Exception("Arcset_cr3bp::setJacobi: Node ID out of range");

	nodes[nodeIDMap.at(id)].setExtraParam(PARAMKEY_JACOBI, jacobi);
}//====================================================

/**
 *	@brief Set Jacobi at the specified step
 *	@param ix step index; if < 0, counts backwards from end of trajectory
 *	@param val value of Jacobi
 *	@throws Exception if `ix` is out of bounds
 */
void Arcset_cr3bp::setJacobiByIx(int ix, double val){
	if(ix < 0)
		ix += nodes.size();

	if(ix < 0 || ix > static_cast<int>(nodes.size()))
		throw Exception("Arcset_cr3bp::setJacobiByIx: invalid node index");

	nodes[ix].setExtraParam(PARAMKEY_JACOBI, val);
}//====================================================

//-----------------------------------------------------
//      Utility Functions
//-----------------------------------------------------

/**
 *  @brief Execute commands to save data to a Matlab file
 *  @param pMatFile pointer to an open Matlab file
 *  @param saveTp describes how much data to save
 */
void Arcset_cr3bp::saveCmds_toFile(mat_t* pMatFile, Save_tp saveTp) const{
	Arcset::saveCmds_toFile(pMatFile, saveTp);

	matvar_t *pJacobi = createVar_NodeExtraParam(PARAMKEY_JACOBI, saveTp, VARNAME_JACOBI);
	saveVar(pMatFile, pJacobi, VARNAME_JACOBI, MAT_COMPRESSION_NONE);
}//====================================================

/**
 *  @brief Execute commands to save data to a structure array
 * 
 *  @param pStruct pointer to a structure array
 *  @param ix index of this arcset within the structure array
 *  @param saveTp Describes how much data to save
 */
void Arcset_cr3bp::saveCmds_toStruct(matvar_t *pStruct, unsigned int ix, Save_tp saveTp) const{
	Arcset::saveCmds_toStruct(pStruct, ix, saveTp);

	if(matvar_t *pJacobi = createVar_NodeExtraParam(PARAMKEY_JACOBI, saveTp, nullptr))
		Mat_VarSetStructFieldByName(pStruct, VARNAME_JACOBI, ix, pJacobi);
}//====================================================

/**
 *  @brief Execute commands to read data from a Matlab file
 *  @param pMatFile pointer to an open Matlab file
 *  @param refLaws Reference to a vector of ControlLaw pointers. As control laws 
 *  are read from the Matlab file, unique control laws are constructed and 
 *  allocated on the stack. The user must manually delete the ControlLaw objects 
 *  to avoid memory leaks.
 */
void Arcset_cr3bp::readCmds_fromFile(mat_t *pMatFile, 
	std::vector<ControlLaw*> &refLaws){

	Arcset::readCmds_fromFile(pMatFile, refLaws);

	matvar_t *pJacobi = Mat_VarRead(pMatFile, VARNAME_JACOBI);
	if(readVar_NodeExtraParam(pJacobi, PARAMKEY_JACOBI, Save_tp::SAVE_ALL)){
		Mat_VarFree(pJacobi);
	}
}//====================================================

/**
 *  @brief Execute commands to read from data from a structure array
 * 
 *  @param pStruct Pointer to the structure array variable
 *  @param ix index of this arcset within the structure array
 * 	@param refLaws Reference to a vector of ControlLaw pointers. As control laws 
 * 	are read from the Matlab file, unique control laws are constructed and 
 * 	allocated on the stack. The user must manually delete the ControlLaw objects
 * 	to avoid memory leaks.
 */
void Arcset_cr3bp::readCmds_fromStruct(matvar_t *pStruct, unsigned int ix, 
	std::vector<ControlLaw*> &refLaws){

	Arcset::readCmds_fromStruct(pStruct, ix, refLaws);

	matvar_t *pJacobi = Mat_VarGetStructFieldByName(pStruct, VARNAME_JACOBI, ix);
	if(readVar_NodeExtraParam(pJacobi, PARAMKEY_JACOBI, Save_tp::SAVE_ALL)){
		Mat_VarFree(pJacobi);
	}
}//====================================================


}// End of astrohelion namespace