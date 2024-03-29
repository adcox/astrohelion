/**
 *	@file Constraint.cpp
 *	@brief Data object that stores information about a node constraint
 *	
 *	@author Andrew Cox
 *	@version May 25, 2016
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

#include "Constraint.hpp"
#include "Utilities.hpp"

#include <cmath>
#include <cstdio>

namespace astrohelion{
//-----------------------------------------------------
// 		Constructors
//-----------------------------------------------------

/**
 *	@brief Construct a constraint
 */
Constraint::Constraint(){
	data.clear();
}//====================================================

/**
 *	@brief Construct a constraint with specified constraint type
 *	@param type constraint type
 */
Constraint::Constraint(Constraint_tp type){
	this->type = type;
	data.clear();
	setAppType();
	setDataStoreIDFlag();
}//====================================================

/**
 *	@brief Construct a constraint with specified constraint type and data values
 *	@param type constraint type
 *	@param id the ID of the object this constriant applies to
 *	@param data data vector (length n)
 */
Constraint::Constraint(Constraint_tp type, int id, std::vector<double> data){
	this->type = type;
	this->id = id;
	this->data = data;
	setAppType();
	setDataStoreIDFlag();
}//====================================================

/**
 *	@brief Construct a constraint with specified constraint type, and data values
 *	@param type constraint type
 *	@param id the ID of the object this constriant applies to
 *	@param data data vector
 *	@param data_len the number of elements in d_len
 */
Constraint::Constraint(Constraint_tp type, int id, const double* data, int data_len){
	this->type = type;
	this->id = id;
	this->data.insert(this->data.begin(), data, data + data_len);
	setAppType();
	setDataStoreIDFlag();
}//====================================================

/**
 *	@brief Create a copy of the specified constraint
 *	@param c a constraint
 */
Constraint::Constraint(const Constraint& c){
	copyMe(c);
}//====================================================

/**
 *	@brief Destructor
 */
Constraint::~Constraint(){}

//-----------------------------------------------------
// 		Operator Functions
//-----------------------------------------------------

/**
 *	@brief Assignment operator
 *	@param c a constraint
 *	@return this constraint, now equal to c
 */
Constraint& Constraint::operator =(const Constraint& c){
	copyMe(c);
	return *this;
}//====================================================

/**
 *  @brief Compare two constraint objects
 * 
 *  @param lhs a constraint object
 *  @param rhs a constraint object
 * 
 *  @return True if the paramters are identical and the data vector
 *  elements match to within a tolerance of 1e-14
 */
bool operator ==(const Constraint &lhs, const Constraint &rhs){
	bool sameParams = rhs.type == lhs.type &&
		rhs.appType == lhs.appType &&
		rhs.id == lhs.id &&
		rhs.data.size() == lhs.data.size();

	if(sameParams){
		for(unsigned int i = 0; i < rhs.data.size(); i++){
			if(std::abs(rhs.data[i] - lhs.data[i]) > 1e-14)
				return false;
		}

		return true;
	}else{
		return false;
	}
}//====================================================

/**
 *  @brief Compare to constraint objects
 * 
 *  @param lhs a constraint object
 *  @param rhs a constraint object
 * 
 *  @return whether or not lhs and rhs are not equatl
 */
bool operator !=(const Constraint &lhs, const Constraint &rhs){
	return !(lhs == rhs);
}//====================================================

//-----------------------------------------------------
// 		Set and Get Functions
//-----------------------------------------------------

/**
 * @brief Determine if this constraint stores Node or Segment IDs in the
 * constraint data vector
 * @return whether or not this constraint stores Node or Segment IDs in the
 * constraint data vector
 */
bool Constraint::dataStoresID() const { return bDataStoreID; }

/**
 *  @brief Retrieve the application type for this constraint, i.e., what type of
 *  objects it controls and can be applied to
 *  @return the application type
 */
ConstraintApp_tp Constraint::getAppType() const{ return appType; }

/**
 *	@return what type of constraint this is
 */
Constraint_tp Constraint::getType() const { return type; }

/**
 *	@return the ID that identifies the object constrained by this constraint
 */
int Constraint::getID() const { return id; }

/**
 *	@return the data vector for this constraint
 */
std::vector<double> Constraint::getData() const { return data; }

/**
 * @brief Retrieve a reference to the data vector
 * @details This method avoids a potentially expensive copy operation
 * @return a reference to the data vector
 */
const std::vector<double>& Constraint::getDataRef() const{ return data; }

/**
 *  @brief Retrieve the first data value that is not an NAN
 *  @return The value of the first data value. If no data
 *  value is located, NAN is returned.
 *  @see getFirstDataValue(int*)
 */
double Constraint::getFirstDataValue() const{
	int ix = 0;
	return getFirstDataValue(&ix);
}//====================================================

/**
 *  @brief Retrieve the first data value that is not an NAN
 * 
 *  @param ix The index of the first data value will be
 *  stored in this integer. If no data value is located,
 *  ix will be set to -1.
 *  @return The value of the first data value. If no data
 *  value is located, NAN is returned.
 */
double Constraint::getFirstDataValue(int *ix) const{
	for(unsigned int i = 0; i < data.size(); i++){
		if(!std::isnan(data[i])){
			*ix = static_cast<int>(i);
			return data[i];
		}
	}

	*ix = -1;
	return NAN;
}//====================================================

/**
 *	@return a count of the constrained states; certain constraint types, 
 *	like Constraint_tp::MATCH_CUST, give the option of constraining a subset of the entire
 *	node.
 */
int Constraint::countConstrainedStates() const{
	int count = 0;
	for(unsigned int n = 0; n < data.size(); n++){
		count += !std::isnan(data.at(n));
	}
	return count;
}//====================================================

/**
 *	@brief Set the constraint type
 *	@param t the type
 */
void Constraint::setType(Constraint_tp t){
	type = t;
	setAppType(); 	// Update, if necessary
	setDataStoreIDFlag();
}//====================================================

/**
 *	@brief Set the object ID this constraint applies to
 *	@param n the ID
 */
void Constraint::setID(int n){ id = n; }

/**
 *	Set the data for this id (should have nodeSize # elements)
 *	@param d the data, dimensions that match node dimensions
 */
void Constraint::setData(std::vector<double> d){ data = d; }

/**
 *  @brief Set the data for this constraint
 * 
 *  @param dat an array of data values
 *  @param len number of elements in `dat`
 */
void Constraint::setData(const double *dat, int len){
	data.clear();
	data.insert(data.begin(), dat, dat+len);
}//====================================================

//-----------------------------------------------------
// 		Utility Functions
//-----------------------------------------------------

/**
 * @brief Determine if this constraint conflics with another
 * @details Conflicts may result in singular Jacobian matrices or simply 
 * nonconvergence
 * 
 * @param con reference to another constraint
 * @return true if this constraint conflicts with con
 */
bool Constraint::conflicts(const Constraint &con) const{
	if(appType == con.getAppType()){
		if(type == con.getType()){
			switch(type){
				case Constraint_tp::STATE:
				case Constraint_tp::CTRL:
				case Constraint_tp::ENDSEG_STATE:
				{
					//Only a conflict if they try to constrain the same value(s)
					std::vector<double> data2 = con.getData();
					unsigned int len = std::min(data.size(), data2.size());
					for(unsigned int i = 0; i < len; i++){
						if(!std::isnan(data[i]) && !std::isnan(data2[i])){
							return true;
						}
					}
					return false;
				}
				default:
					// All other constraints cannot be duplicated
					return true;
			}
		}else{
			switch(type){
				case Constraint_tp::JC:
				case Constraint_tp::HLT:
					// Energy constraints conflict with state constraints
					return con.getType() == Constraint_tp::STATE;

				case Constraint_tp::STATE:
					// State constraints conflict with energy constraints
					return con.getType() == Constraint_tp::JC ||
						con.getType() == Constraint_tp::HLT;
				default:
					return false;
			}
		}
	}else{
		return false;
	}
}//====================================================

/**
 *  @brief Copy the constraint
 * 
 *  @param c reference to a constraint object
 */
void Constraint::copyMe(const Constraint &c){
	appType = c.appType;
	type = c.type;
	id = c.id;
	data = c.data;
	bDataStoreID = c.bDataStoreID;
}//====================================================

/**
 *  @brief Get a human-redable string representing the constraint type
 *  @return a human-redable string representing the constraint type
 */
const char* Constraint::getTypeStr() const{ return getConTypeStr(type); }

/**
 *	@param t a constraint type
 *	@return a human-readable string representing a constraint type
 */
const char* Constraint::getConTypeStr(Constraint_tp t){
	switch(t){
		case Constraint_tp::NONE: { return "NONE"; }
		case Constraint_tp::STATE: { return "STATE"; }
		case Constraint_tp::MATCH_ALL: { return "MATCH_ALL"; }
		case Constraint_tp::MATCH_CUST: { return "MATCH_CUST"; }
		case Constraint_tp::EPOCH: { return "EPOCH"; }
		case Constraint_tp::DIST: { return "DIST"; }
		case Constraint_tp::MIN_DIST: { return "MIN_DIST"; }
		case Constraint_tp::MAX_DIST: { return "MAX_DIST"; }
		case Constraint_tp::MAX_DELTA_V: { return "MAX_DELTA_V"; }
		case Constraint_tp::DELTA_V: { return "DELTA_V"; }
		case Constraint_tp::JC: { return "JC"; }
		case Constraint_tp::HLT: { return "H_LT"; }
		case Constraint_tp::SP: { return "SP"; }
		case Constraint_tp::SP_RANGE: { return "SP_RANGE"; }
		case Constraint_tp::SP_DIST: { return "SP_DIST"; }
		case Constraint_tp::SP_MAX_DIST: { return "SP_MAX_DIST"; }
		case Constraint_tp::TOF_TOTAL: { return "TOF"; }
		case Constraint_tp::APSE: { return "APSE"; }
		case Constraint_tp::ANGLE: { return "ANGLE"; }
		case Constraint_tp::CTRL: {return "CTRL"; }
		case Constraint_tp::CONT_CTRL: { return "CONTINUOUS CONTROL STATES"; }
		case Constraint_tp::CONT_PV: { return "CONTINUOUS SEG2NODE_POSITION_VELOCITY"; }
		case Constraint_tp::CONT_EX: { return "CONTINUOUS SEG2NODE_EXTRA"; }
		case Constraint_tp::SEG_CONT_PV: { return "CONTINUOUS SEG2SEG_POSITION_VELOCITY"; }
		case Constraint_tp::SEG_CONT_EX: { return "CONTINUOUS SEG2SEG_EXTRA"; }
		case Constraint_tp::PSEUDOARC: { return "PSEUDO-ARCLENGTH"; }
		case Constraint_tp::RM_STATE: { return "RM_STATE"; }
		case Constraint_tp::RM_EPOCH: { return "RM_EPOCH"; }
		case Constraint_tp::RM_CTRL: { return "RM_CTRL"; }
		case Constraint_tp::ENDSEG_STATE: { return "ENDSEG_STATE"; }
		case Constraint_tp::ENDSEG_APSE: { return "ENDSEG_APSE"; }
		case Constraint_tp::ENDSEG_JC: { return "ENDSEG_JC"; }
		case Constraint_tp::ENDSEG_DIST: { return "ENDSEG_DIST"; }
		case Constraint_tp::ENDSEG_MIN_DIST: { return "ENDSEG_MIN_DIST"; }
		case Constraint_tp::ENDSEG_MAX_DIST: { return "ENDSEG_MAX_DIST"; }
		case Constraint_tp::ENDSEG_ANGLE: { return "ENDSEG_ANGLE"; }
		default: { return "UNDEFINED!"; }
	}
}//====================================================

/**
 *  @param t a constraint application type
 *  @return a human-readable string representing an application type
 */
const char* Constraint::getAppTypeStr(ConstraintApp_tp t){
	switch(t){
		case ConstraintApp_tp::APP_TO_NODE: return "Node"; break;
		case ConstraintApp_tp::APP_TO_ARC: return "Whole Arcset"; break;
		case ConstraintApp_tp::APP_TO_SEG: return "Segment"; break;
		default: return "Undefined!"; break;
	}
}//====================================================

/**
 *	@brief Print this constraint and its data to the standard output.
 */
void Constraint::print() const {
	astrohelion::printf("%s Constraint:\n  Applies to: %s (ID %d)\n  Data: {",
		getConTypeStr(type), getAppTypeStr(appType), id);
	for(unsigned int n = 0; n < data.size(); n++){
		astrohelion::printf("%12.5f ", data[n]);
	}
	astrohelion::printf("}\n");
}//====================================================

/**
 *  @brief Set the application type appropriately based on the constraint type
 */
void Constraint::setAppType(){
	switch(type){
		case Constraint_tp::PSEUDOARC:
		case Constraint_tp::TOF_TOTAL:
		case Constraint_tp::MAX_DELTA_V:
		case Constraint_tp::DELTA_V:
		case Constraint_tp::SEG_CONT_PV:
		case Constraint_tp::SEG_CONT_EX:
			appType = ConstraintApp_tp::APP_TO_ARC;
			break;
		case Constraint_tp::CONT_CTRL:
		case Constraint_tp::CONT_PV:
		case Constraint_tp::CONT_EX:
		case Constraint_tp::ENDSEG_STATE:
		case Constraint_tp::ENDSEG_APSE:
		case Constraint_tp::ENDSEG_DIST:
		case Constraint_tp::ENDSEG_MIN_DIST:
		case Constraint_tp::ENDSEG_MAX_DIST:
		case Constraint_tp::ENDSEG_JC:
			appType = ConstraintApp_tp::APP_TO_SEG;
			break;
		default:
			appType = ConstraintApp_tp::APP_TO_NODE;
			break;
	}
}//====================================================

/**
 * @brief Set the flag that tells other functions whether or not
 * this constraint stores Node or Segment IDs in the data vector
 */
void Constraint::setDataStoreIDFlag(){
	switch(type){
		case Constraint_tp::MATCH_ALL:
		case Constraint_tp::MATCH_CUST:
			// These constraints store node or segment IDs in the
			// data vectorand will need to be updated if IDs change
			bDataStoreID = true;
			break;
		default:
			// Default behavior (set in header) is bDatStoreID = false
			break;
	}
}//====================================================

}// END of Astrohelion namespace