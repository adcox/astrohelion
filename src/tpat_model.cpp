/**
 *  @file tpat_model.cpp
 *	@brief Defines behavior for a dynamic model
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

#include "tpat_model.hpp"

#include "tpat_correction_engine.hpp"
#include "tpat_event.hpp"
#include "tpat_eigen_defs.hpp"
#include "tpat_exceptions.hpp"
#include "tpat_multShoot_data.hpp"
#include "tpat_node.hpp"
#include "tpat_nodeset.hpp"
#include "tpat_traj.hpp"
#include "tpat_sys_data.hpp"
#include "tpat_utilities.hpp"
 
#include <algorithm>
#include <cmath>

/**
 *	@brief Default constructor
 *	@param type the model type
 */
tpat_model::tpat_model(tpat_dynamicsModel_tp type){
	modelType = type;
}//===========================================

/**
 *	@brief Copy constructor
 */
tpat_model::tpat_model(const tpat_model &m){
	copyMe(m);
}//===========================================

/**
 *	@brief Deconstructor
 */
tpat_model::~tpat_model(){}

/**
 *	@brief Copy Operator
 */	
tpat_model& tpat_model::operator =(const tpat_model &m){
	copyMe(m);
	return *this;
}//============================================

/**
 *	@brief Copies all data from a dynamic model into this one
 *	@param m another dynamic model
 */
void tpat_model::copyMe(const tpat_model &m){
	modelType = m.modelType;
	coreStates = m.coreStates;
	stmStates = m.stmStates;
	extraStates = m.extraStates;
}//============================================

/**
 *	@brief Retrieve the number of core states
 *	@return the number of core states
 */
int tpat_model::getCoreStateSize() const { return coreStates; }

/**
 *	@brief Retrieve the number of STM elements stored in the state vector
 *	@return the number of STM elements stored in the state vector
 */
int tpat_model::getSTMStateSize() const { return stmStates; }

/**
 *	@brief Retrieve the number of extra states stored after the core states and STM elements
 *	@return the number of extra states stored after the core states and STM elements
 */
int tpat_model::getExtraStateSize() const { return extraStates; }

/**
 *	@brief Determine whether the specified constraint type is supported in this model
 *	@return whether or not the specified constraint type is supported in this model
 */
bool tpat_model::supportsCon(tpat_constraint::tpat_constraint_tp type) const{
	return std::find(allowedCons.begin(), allowedCons.end(), type) != allowedCons.end();
}//===================================================

/**
 *	@brief Determine whether the specified event type is supported in this model
 *	@return whether or not the specified event type is supported in this model
 */
bool tpat_model::supportsEvent(tpat_event::tpat_event_tp type) const{
	return std::find(allowedEvents.begin(), allowedEvents.end(), type) != allowedEvents.end();
}//===================================================

/**
 *  @brief Determine the time derivative of the magnitude of a vector from a primary to
 *  the body of interest, non-dimensional units
 *  @details This derivation assumes the primary of interest is fixed in the frame the
 *  spacecraft coordinates are expressed in. If this is not the case (i.e., the primary
 *  moves in the working frame), this function will need to be overridden
 * 
 *  @param Pix Index of the primary
 *  @param t Non-dimensional epoch to compute r-dot at
 *  @param state six-element non-dimensional spacecraft state
 *  @param sys system data object
 *  @return Time derivative of the magnitude of a vector from a primary to the body
 *  of interest, non-dimensional velocity units
 */
double tpat_model::getRDot(int Pix, double t, const double *state, const tpat_sys_data *sys) const{
	
	std::vector<double> primPos = getPrimPos(t, sys);
    double dx = state[0] - primPos[3*Pix+0];
    double dy = state[1] - primPos[3*Pix+1];
    double dz = state[2] - primPos[3*Pix+2];

    std::vector<double> primVel = getPrimVel(t, sys);
    double num = dx*(state[3] - primVel[3*Pix+0]) + dy*(state[4] - primVel[3*Pix+1])+ dz*(state[5] - primVel[3*Pix+2]);

    return num/sqrt(dx*dx + dy*dy + dz*dz);
}//==================================================


/**
 *	@brief Initialize the corrector's design vector with position and velocity states,
 *	and times-of-flight.
 *
 *	Derived models may replace this function or call it and then append more design 
 * 	variables.
 *
 *	@param it a pointer to the corrector's iteration data structure
 *	@param set a pointer to the nodeset being corrected
 */
void tpat_model::multShoot_initDesignVec(tpat_multShoot_data *it, const tpat_nodeset *set) const{
	// Create the initial state vector
	it->X.clear();
	it->freeVarScale.clear();

	// Copy in the position and velocity states for each node
	int rowNum = 0;
	for(int n = 0; n < set->getNumNodes(); n++){
		std::vector<double> state = set->getNodeByIx(n).getState();
		rowNum = it->X.size();
		it->X.insert(it->X.end(), state.begin(), state.end());
		it->freeVarMap.push_back(ms_varMap_obj(ms_varMap_obj::STATE, rowNum, set->getNodeByIx(n).getID()));
	}

	if(it->varTime){		
		if(it->equalArcTime){
			// Append the total TOF for the arc
			it->freeVarMap.push_back(ms_varMap_obj(ms_varMap_obj::TOF_TOTAL, (int)(it->X.size()), -1));
			it->X.insert(it->X.end(), set->getTotalTOF());
		}else{
			// Append the TOF for each segment
			for(int s = 0; s < set->getNumSegs(); s++){
				it->freeVarMap.push_back(ms_varMap_obj(ms_varMap_obj::TOF, (int)(it->X.size()), set->getSegByIx(s).getID()));
				it->X.insert(it->X.end(), set->getSegByIx(s).getTOF());
			}
		}
	}
}//============================================================

/**
 *  @brief Scale the design variable vector so that all elements have approximately the 
 *  same magnitude.
 *  @details This can improve the corrections process, although it is not guaranteed to,
 *  and CAN impede the corrections process as well.
 * 
 *  @param it iteraiton data object for the current corrections process
 *  @param set the nodeset that corrections are being applied to
 */
void tpat_model::multShoot_scaleDesignVec(tpat_multShoot_data *it, const tpat_nodeset *set) const{
	// Group all like variables and then compute the largest magnitude of each
	Eigen::VectorXd allPos(3*it->numNodes);
	Eigen::VectorXd allVel(3*it->numNodes);
	Eigen::VectorXd allTOF(set->getNumSegs());
	for(int n = 0; n < it->numNodes; n++){
		allPos(3*n+0) = it->X[6*n+0];
		allPos(3*n+1) = it->X[6*n+1];
		allPos(3*n+2) = it->X[6*n+2];
		allVel(3*n+0) = it->X[6*n+3];
		allVel(3*n+1) = it->X[6*n+4];
		allVel(3*n+2) = it->X[6*n+5];
	}

	for(int s = 0; s < set->getNumSegs(); s++){
		if(it->varTime)
			allTOF[s] = it->equalArcTime ? it->X[6*it->numNodes]/(set->getNumSegs()) : it->X[6*it->numNodes+s];
		else
			allTOF[s] = set->getTOFByIx(s);
	}

	double maxPos = allPos.cwiseAbs().maxCoeff();
	double maxVel = allVel.cwiseAbs().maxCoeff();
	double maxTime = allVel.cwiseAbs().maxCoeff();
	
	// Scale each variable type by its maximum so that all values are between -1 and +1
	it->freeVarScale[0] = maxPos == 0 ? 1 : 1/maxPos;		// Position scalar
	it->freeVarScale[1] = maxVel == 0 ? 1 : 1/maxVel;		// Velocity scalar
	it->freeVarScale[2] = maxTime == 0 ? 1 : 1/maxTime;		// TOF scalar

	printf("Variable Scalings:\n  Pos = %.6f\n  Vel = %.6f\n  TOF = %.5f\n", 
		it->freeVarScale[0], it->freeVarScale[1], it->freeVarScale[2]);

	// Loop through all nodes and scale position and velocity
	for(int n = 0; n < it->numNodes; n++){
		it->X[6*n+0] *= it->freeVarScale[0];	// position
		it->X[6*n+1] *= it->freeVarScale[0];
		it->X[6*n+2] *= it->freeVarScale[0];
		it->X[6*n+3] *= it->freeVarScale[1];	// velocity
		it->X[6*n+4] *= it->freeVarScale[1];
		it->X[6*n+5] *= it->freeVarScale[1];
	}

	// Scale all times-of-flight
	if(it->varTime){
		if(it->equalArcTime)
			it->X[6*it->numNodes] *= it->freeVarScale[2];	// TOF
		else{
			for(int s = 0; s < it->nodeset->getNumSegs(); s++){
				it->X[6*it->numNodes+s] *= it->freeVarScale[2];	// TOF
			}
		}
	}
}//===================================================

/**
 *	@brief Create continuity constraints for the correction algorithm; this function
 *	creates position and velocity constraints.
 *
 *	Derived models may replace this function or call it and then append more constraints
 *	for other variables that may be continuous, such as time (non-autonomous systems)
 *	or mass.
 *
 *	@param it a pointer to the corrector's iteration data structure
 *	@param set a pointer to the nodeset being corrected
 */	
void tpat_model::multShoot_createContCons(tpat_multShoot_data *it, const tpat_nodeset *set) const{
	// Create position and velocity constraints
	for(int s = 0; s < set->getNumSegs(); s++){
		// Force all positions to be continuous
		double contStates[] = {1, 1, 1, 1, 1, 1};
		// Get a vector specifying which velocity states are continuous
		std::vector<bool> velCon = set->getSegByIx(s).getVelCon();
		// If not continuous, put NAN into the constraint data; else unity
		contStates[3] = velCon[0] ? 1 : NAN;
		contStates[4] = velCon[1] ? 1 : NAN;
		contStates[5] = velCon[2] ? 1 : NAN;
		// Create a constraint
		tpat_constraint con(tpat_constraint::CONT_PV, s, contStates, 6);
		// Save constraint to constraint vector
		it->allCons.push_back(con);
	}

	// Next, create other continuity constraints via the CONT_EX constraint type
	// These may include time continuity for non-autonomous systems, or mass continuity
}//============================================================

/**
 *	@brief Retrieve the initial conditions for a segment that the correction
 *	engine will integrate.
 *
 *	Derived models may replace this function to change how the initial conditions
 *	are chosen from the design vector.
 *
 *	@param it a pointer to the corrector's iteration data structure
 *	@param set a pointer to the nodeset being corrected
 *	@param s the ID of the segment being propagated
 *	@param ic a pointer to a 6-element initial state array
 *	@param t0 a pointer to a double representing the initial time (epoch)
 *	@param tof a pointer to a double the time-of-flight on the segment.
 */
void tpat_model::multShoot_getSimICs(const tpat_multShoot_data *it, const tpat_nodeset *set, int s,
	double *ic, double *t0, double *tof) const{
	(void) set;

	// Retrieve  representative object and get data from free var vec
	ms_varMap_obj state = it->getVarMap_obj(ms_varMap_obj::STATE, it->nodeset->getSeg(s).getOrigin());
	std::copy(it->X.begin()+state.row0, it->X.begin()+state.row0 + state.nRows, ic);

	// Reverse Scaling
	for(int i = 0; i < 6; i++){
		ic[i] /= i < 3 ? it->freeVarScale[0] : it->freeVarScale[1];
	}

	if(it->varTime){
		// Retrieve  representative object and get data from free var vec
		ms_varMap_obj tof_obj = it->getVarMap_obj(it->equalArcTime ? ms_varMap_obj::TOF_TOTAL : ms_varMap_obj::TOF,
			it->equalArcTime ? -1 : s);
		*tof = it->equalArcTime ? it->X[tof_obj.row0]/(it->nodeset->getNumSegs()) : it->X[tof_obj.row0];
		// Reverse scaling
		*tof /= it->freeVarScale[2]; 	// Time scaling
	}
	*t0 = 0;

	// printf("  ID = %02d\n", s);
	// printf("  Origin Node = %02d\n", it->nodeset->getSeg(s).getOrigin());
	// printf("  tof = %.4f\n", *tof);
	// printf("  t0 = %.4f\n", *t0);
}//============================================================

/**
 *  @brief Compute the value of a slack variable for an inequality constraint.
 *  @details Computing the value of the slack variable can avoid unneccessary 
 *  shooting iterations when the inequality constraint is already met. If the 
 *  inequality constraint is met, the value returned by this function will make
 *  the constraint function evaluate to zero.
 *  
 *  Note: This function should be called after the state variable vector has 
 *  been initialized by the multiple shooting algorithm
 * 
 *  @param it the tpat_multShoot_data object associated with the multiple shooting process
 *  @param con the inequality constraint for which the slack variable is being computed
 * 
 *  @return The value of the slack variable that minimizes the constraint function
 *  without setting the slack variable to zero
 *  @throws tpat_exception if the constraint type does not include a slack variable
 */
double tpat_model::multShoot_getSlackVarVal(const tpat_multShoot_data *it, tpat_constraint con) const{
	switch(con.getType()){
		case tpat_constraint::MAX_DIST:
		case tpat_constraint::MIN_DIST:
			return multShoot_targetDist_compSlackVar(it, con);
		case tpat_constraint::MAX_DELTA_V:
			return multShoot_targetDeltaV_compSlackVar(it, con);
		default:
			throw tpat_exception("tpat_model::multShoot_getSlackVarVal: Cannot compute slack variable values for equality constraints");
	}
}//===========================================================

/**
 *	@brief Compute constraint function and partial derivative values for a constraint
 *	
 *	This function provides a framework for most of the constraints available to all models.
 *	Given an iteration data object, a constraint, and the index of that constraint, we can
 *	compute the value of the constraint function(s) associated with the constraint as well
 *	as the partial derivatives that relate the constraint functions to design variables.
 *	Although this model defines behavior for many of the constraints, it is up to the model
 *	developer to ensure any new constraints are implemented in a derived model, and any
 *	existing constraints are treated fully and appropriately by the new, derived model.
 *
 *	@param it a pointer to the corrector's iteration data structure
 *	@param con the constraint being applied
 *	@param c the index of the constraint within the total constraint vector (which is, in
 *	turn, stored in the iteration data)
 */	
void tpat_model::multShoot_applyConstraint(tpat_multShoot_data *it, tpat_constraint con, int c) const{

	int row0 = it->conRows[c];

	switch(con.getType()){
		case tpat_constraint::CONT_PV:
			// Apply position-velocity continuity constraints
			multShoot_targetPosVelCons(it, con, row0);
			break;
		case tpat_constraint::CONT_EX:
			// Apply extra continuity constraints
			multShoot_targetExContCons(it, con, row0);
			break;
		case tpat_constraint::STATE:
			multShoot_targetState(it, con, row0);
			break;
		case tpat_constraint::MATCH_ALL:
			multShoot_targetMatchAll(it, con, row0);
			break;
		case tpat_constraint::MATCH_CUST:
			multShoot_targetMatchCust(it, con, row0);
			break;
		case tpat_constraint::MAX_DIST:
		case tpat_constraint::MIN_DIST:
		case tpat_constraint::DIST:
			multShoot_targetDist(it, con, c);
			break;
		case tpat_constraint::DELTA_V:
		case tpat_constraint::MAX_DELTA_V:
			multShoot_targetDeltaV(it, con, c);
			break;
		case tpat_constraint::TOF:
			multShoot_targetTOF(it, con, row0);
			break;
		case tpat_constraint::APSE:
			multShoot_targetApse(it, con, row0);
			break;	
		default: break;
	}
}//=========================================================

/**
 *	@brief Compute position and velocity constraint values and partial derivatives
 *
 *	This function computes and stores the default position continuity constraints as well
 *	as velocity constraints for all nodes marked continuous in velocity. The delta-Vs
 *	between arc segments and node states are recorded, and the partial derivatives of each
 *	node with respect to other nodes and integration time are all computed
 *	and placed in the appropriate spots in the Jacobian matrix.
 *
 *	Derived models may replace this function.
 *
 *	@param it a pointer to the correctors iteration data structure
 *	@param con the constraint being applied
 *	@param row0 the first row this constraint applies to
 *	@throws tpat_exception if the node ID stored in the constraint is zero
 */
void tpat_model::multShoot_targetPosVelCons(tpat_multShoot_data* it, tpat_constraint con, int row0) const{
	int segID = con.getID();	// get segment ID
	std::vector<double> conData = con.getData();

	// Get index of segment
	int segIx = it->nodeset->getSegIx(segID);
	printf("Trageting position/velocity continuity at segment: ID = %d, ix = %d\n", segID, segIx);
	std::vector<double> lastState = it->propSegs[segIx].getStateByIx(-1);
	std::vector<double> lastAccel = it->propSegs[segIx].getAccelByIx(-1);
	MatrixXRd stm = it->propSegs[segIx].getSTMByIx(-1);

	// Get index of origin node
	int origIx = it->nodeset->getNodeIx(it->nodeset->getSeg(segID).getOrigin());
	int termIx = it->nodeset->getNodeIx(it->nodeset->getSeg(segID).getTerminus());

	// Loop through conData
	for(size_t s = 0; s < conData.size(); s++){
		if(!std::isnan(conData[s])){
			// This state is constrained to be continuous; compute error
			double scale = s < 3 ? it->freeVarScale[0] : it->freeVarScale[1];
			it->FX[row0+s] = lastState[s]*scale - it->X[6*termIx+s];

			// Loop through all design variables for this node (6) and compute partials of F w.r.t. x
			for(size_t x = 0; x < 6; x++){
				// put STM elements into DF matrix
				double scale2 = x < 3 ? it->freeVarScale[0] : it->freeVarScale[1];
				it->DF[it->totalFree*(row0+s) + 6*origIx+x] = stm(s,x)*scale/scale2;
				// Negative identity matrix
				if(s == x)
					it->DF[it->totalFree*(row0+s) + 6*termIx+x] = -1;
			}

			// Compute partials of F w.r.t. times-of-flight
			// Columns of DF based on time constraints
			if(it->varTime){
				
				// If equal arc time is enabled, place a 1/(n-1) in front of all time derivatives
				double timeCoeff = it->equalArcTime ? 1.0/(it->numNodes - 1) : 1.0;

				// If equal arc time is enabled, all time derivatives are in one column
				int timeCol = it->equalArcTime ? 6*it->numNodes : 6*it->numNodes+segIx;
				
				// Column of state derivatives: [vel; accel]
				if(s < 3)
					it->DF[it->totalFree*(row0+s) + timeCol] = timeCoeff*lastState[s+3]*it->freeVarScale[0]/it->freeVarScale[2];
				else{					
					it->DF[it->totalFree*(row0+s) + timeCol] = timeCoeff*lastAccel[s-3]*it->freeVarScale[1]/it->freeVarScale[2];
				}
			}
		}
	}
}//=========================================================

/**
 *	@brief Computes continuity constraints for constraints with the <tt>CONT_EX</tt> type.
 *
 *	In this base model, no behavior is defined for extra constraints. It is intended to enforce
 *	continuity constraints like epoch (time) continuity, mass continuity, etc.
 *
 *	@param it a pointer to the correctors iteration data structure
 *	@param con the constraint being applied
 *	@param row0 the first row this constraint applies to
 */
void tpat_model::multShoot_targetExContCons(tpat_multShoot_data *it, tpat_constraint con, int row0) const{
	// Do absoluately nothing
	(void)it;
	(void)con;
	(void)row0;
}//======================================================

/**
 *	@brief Compute partials and constraint functions for nodes constrained with <tt>STATE</tt>.
 *
 *	This method *should* provide full state constraining for any model; the STM and identity 
 *	matrices are used to relate node states and integrated states.
 *
 *	@param it a pointer to the class containing all the data relevant to the corrections process
 *	@param con the constraint being applied
 *	@param row0 the index of the row this constraint begins at
 */
void tpat_model::multShoot_targetState(tpat_multShoot_data* it, tpat_constraint con, int row0) const{
	std::vector<double> conData = con.getData();
	int nodeIx = it->nodeset->getNodeIx(con.getID());

	// Allow user to constrain 6 states	
	int count = 0; 	// Count # rows since some may be skipped (NAN)
	for(int s = 0; s < ((int)con.getData().size()); s++){
		if(!std::isnan(conData[s])){
			if(s < 6){
				double scale = s < 3 ? it->freeVarScale[0] : it->freeVarScale[1];
				
				it->FX[row0+count] = it->X[6*nodeIx+s] - conData[s]*scale;
				it->DF[it->totalFree*(row0 + count) + 6*nodeIx + s] = 1;
				count++;
			}else{
				printErr("tpat_model::multShoot_targetState: State constraint has more than 6 elements...\n");
			}
		}
	}	
}//=================================================

/**
 *	@brief Compute partials and constraint functions for nodes constrained with <tt>MATCH_ALL</tt>
 *
 *	This method *should* provide full functionality for any model; only 1's and 0's are applied
 *	to the Jacobian matrix.
 *
 *	@param it a pointer to the class containing all the data relevant to the corrections process
 *	@param con a copy of the constraint object
 *	@param row0 the index of the row this constraint begins at
 */
void tpat_model::multShoot_targetMatchAll(tpat_multShoot_data* it, tpat_constraint con, int row0) const{
	// Only allow matching 6 states, not TOF (state 7)
	int nodeIx = it->nodeset->getNodeIx(con.getID());
	int otherNodeIx = it->nodeset->getNodeIx(con.getData()[0]);
	
	for(int row = 0; row < 6; row++){
		// Constrain the states of THIS node to be equal to the node 
		// with index stored in conData[0]
		it->FX[row0+row] = it->X[6*nodeIx+row] - it->X[6*otherNodeIx+row];

		// Partial of this constraint wrt THIS node = I
		it->DF[it->totalFree*(row0 + row) + 6*nodeIx + row] = 1;

		// Partial of this constraint wrt other node = -I
		it->DF[it->totalFree*(row0 + row) + 6*otherNodeIx+row] = -1;
	}
}//=============================================

/**
 *	@brief Compute partials and constraint functions for nodes constrained with <tt>MATCH_CUST</tt>
 *
 *	This method *should* provide full functionality for any model; Only 1's and 0's are applied
 *	to the Jacobian matrix.
 *
 *	@param it a pointer to the class containing all the data relevant to the corrections process
 *	@param con a copy of the constraint object
 *	@param row0 the index of the row this constraint begins at
 */
void tpat_model::multShoot_targetMatchCust(tpat_multShoot_data* it, tpat_constraint con, int row0) const{
	std::vector<double> conData = con.getData();
	int nodeIx = it->nodeset->getNodeIx(con.getID());
	int otherNodeIx = it->nodeset->getNodeIx(conData[0]);
	int count = 0;
	
	for(int s = 0; s < 6; s++){
		if(!std::isnan(conData[s])){
			it->FX[row0 + count] = it->X[6*nodeIx+s] - it->X[6*otherNodeIx+s];

			// partial of this constraint wrt THIS node = 1
			it->DF[it->totalFree*(row0 + count) + 6*nodeIx+s] = 1;

			// partial of this constraint wrt other node = -1
			it->DF[it->totalFree*(row0 + count) + 6*otherNodeIx+s] = -1;

			count++;
		}
	}
}//===============================================

/**
 *	@brief Compute partials and constraint functions for nodes constrained with <tt>DIST</tt>, 
 *	<tt>MIN_DIST</tt>, or <tt>MAX_DIST</tt>
 *
 *	This method *should* provide full functionality for any model; It calls the getPrimPos() 
 *	functions, which all models define and uses dynamic-independent computations to populate
 *	the constraint vector and Jacobian matrix.
 *
 *	@param it a pointer to the class containing all the data relevant to the corrections process
 *	@param con a copy of the constraint object
 *	@param c the index of this constraint in the constraint vector object
 */
void tpat_model::multShoot_targetDist(tpat_multShoot_data* it, tpat_constraint con, int c) const{
	std::vector<double> conData = con.getData();
	int nodeIx = it->nodeset->getNodeIx(con.getID());
	int Pix = (int)(conData[0]);	// index of primary
	int row0 = it->conRows[c];
	double t = 0;	// If the system is non-autonomous, this will need to be replaced with an epoch time

	// Get the primary position
	std::vector<double> primPos = getPrimPos(t, it->sysData);

	// Get distance between node and primary in x, y, and z-coordinates
	double dx = it->X[6*nodeIx+0] - primPos[Pix*3+0]*it->freeVarScale[0];
	double dy = it->X[6*nodeIx+1] - primPos[Pix*3+1]*it->freeVarScale[0];
	double dz = it->X[6*nodeIx+2] - primPos[Pix*3+2]*it->freeVarScale[0];

	double h = sqrt(dx*dx + dy*dy + dz*dz); 	// true distance

	// Compute difference between desired distance and true distance
	it->FX[row0] = h - conData[1]*it->freeVarScale[0];

	// Partials with respect to node position states
	it->DF[it->totalFree*row0 + 6*nodeIx + 0] = dx/h;
	it->DF[it->totalFree*row0 + 6*nodeIx + 1] = dy/h;
	it->DF[it->totalFree*row0 + 6*nodeIx + 2] = dz/h;

	// Extra stuff for inequality constraints
	if(con.getType() == tpat_constraint::MIN_DIST || 
		con.getType() == tpat_constraint::MAX_DIST ){
		// figure out which of the slack variables correspond to this constraint
		std::vector<int>::iterator slackIx = std::find(it->slackAssignCon.begin(), 
			it->slackAssignCon.end(), c);

		// which column of the DF matrix the slack variable is in
		int slackCol = it->totalFree - it->numSlack + (slackIx - it->slackAssignCon.begin());
		int sign = con.getType() == tpat_constraint::MAX_DIST ? 1 : -1;

		// Subtract squared slack variable from constraint
		it->FX[row0] += sign*it->X[slackCol]*it->X[slackCol];

		// Partial with respect to slack variable
		it->DF[it->totalFree*row0 + slackCol] = sign*2*it->X[slackCol];
	}
}// End of targetDist() =========================================

/**
 *  @brief Compute the value of the slack variable for inequality distance constraints
 *  @details This function computes a value for the slack variable in an
 *  inequality distance constraint. If the constraint is already met by the initial
 *  design, using this value will prevent the multiple shooting algorithm from
 *  searching all over for the propper value.
 * 
 *  @param it the iteration data object for the multiple shooting process
 *  @param con the constraint the slack variable applies to
 *  @return the value of the slack variable
 */
double tpat_model::multShoot_targetDist_compSlackVar(const tpat_multShoot_data* it, tpat_constraint con) const{
	std::vector<double> conData = con.getData();
	int nodeIx = it->nodeset->getNodeIx(con.getID());
	int Pix = (int)(conData[0]);	// index of primary	
	double t = 0;	// If the system is non-autonomous, this will need to be replaced with an epoch time

	// Get the primary position
	std::vector<double> primPos = getPrimPos(t, it->sysData);

	// Get distance between node and primary in x, y, and z-coordinates
	double dx = it->X[6*nodeIx+0] - primPos[Pix*3+0]*it->freeVarScale[0];
	double dy = it->X[6*nodeIx+1] - primPos[Pix*3+1]*it->freeVarScale[0];
	double dz = it->X[6*nodeIx+2] - primPos[Pix*3+2]*it->freeVarScale[0];

	double h = sqrt(dx*dx + dy*dy + dz*dz); 	// true distance
	int sign = con.getType() == tpat_constraint::MAX_DIST ? 1 : -1;
    double diff = conData[1]*it->freeVarScale[0] - h;

    /*  If diff and sign have the same sign (+/-), then the constraint
     *  is satisfied, so compute the value of the slack variable that 
     *  sets the constraint function equal to zero. Otherwise, choose 
     *  a small value of the slack variable but don't set it to zero as 
     *  that will make the partials zero and will prevent the mulitple
     *  shooting algorithm from updating the slack variable
     */
    return diff*sign > 0 ? sqrt(std::abs(diff)) : 1e-4;
}//==========================================================

/**
 *	@brief Compute partials and constraints for all nodes constrained with <tt>DELTA_V</tt> or
 *	<tt>MIN_DELTA_V</tt>
 *
 *	Because the delta-V constraint applies to the entire trajectory, the constraint function values
 *	and partial derivatives must be computed for each node along the trajectory. This function
 *	takes care of all of them at once.
 *
 *	This function computes the partials and constraints for an autonomous system; Non-autonomous
 *	dynamic models should implement a similar copy of this function that computes partials
 *	with respect to their additional design variables.
 *
 *	@param it a pointer to the class containing all the data relevant to the corrections process
 *	@param con the constraint being applied
 *	@param c the index of the first row for this constraint
 */
void tpat_model::multShoot_targetDeltaV(tpat_multShoot_data* it, tpat_constraint con, int c) const{

	int row0 = it->conRows[c];

	// Don't allow dividing by zero, but otherwise scale by value to keep order ~1
	double dvMax = con.getData()[0] == 0 ? 1 : con.getData()[0]*it->freeVarScale[1];

	// Compute total deltaV magnitude
	double totalDV = 0;
	for(int s = 0; s < it->nodeset->getNumSegs(); s++){
		// compute magnitude of DV between segment s and its terminal point
		// This takes the form v_n,f - v_n+1,0
		double dvx = it->deltaVs[s*3] * it->freeVarScale[1];
		double dvy = it->deltaVs[s*3+1]*it->freeVarScale[1];
		double dvz = it->deltaVs[s*3+2]*it->freeVarScale[1];
		double dvMag = sqrt(dvx*dvx + dvy*dvy + dvz*dvz);

		// If dvMag is zero, don't bother computing partials; they're all equal to zero but the
		// computation will try to divide by zero which is problematic...
		if(dvMag > 0){
			totalDV += dvMag;

			// Compute parial w.r.t. terminus node (where velocity is discontinuous)
			double dFdq_n2_data[] = {0, 0, 0, -dvx/dvMag, -dvy/dvMag, -dvz/dvMag};
			Eigen::RowVectorXd dFdq_n2 = Eigen::Map<Eigen::RowVectorXd>(dFdq_n2_data, 1, 6);

			// Get info about the final state/accel of the integrated segment

			MatrixXRd stm = it->propSegs[s].getSTMByIx(-1);

			// Partial w.r.t. integrated path (newSeg) from origin node
			Eigen::RowVectorXd dFdq_nf = -1*dFdq_n2*stm;
			
			// Adjust scaling of STM after multiplication (easier here)
			dFdq_nf.segment(0, 3) *= it->freeVarScale[1]/it->freeVarScale[0];

			int origIx = it->nodeset->getNodeIx(it->nodeset->getSegByIx(s).getOrigin());
			int termIx = it->nodeset->getNodeIx(it->nodeset->getSegByIx(s).getTerminus());

			for(int i = 0; i < 6; i++){
				it->DF[it->totalFree*row0 + 6*termIx + i] += dFdq_n2(0, i)/dvMax*it->freeVarScale[1];	// Not sure why this scaling factor belongs here...
				it->DF[it->totalFree*row0 + 6*origIx + i] += dFdq_nf(0, i)/dvMax;
			}

			// Compute partial w.r.t. segment time-of-flight
			if(it->varTime){
				// Derivative of the final state of segment s
				std::vector<double> state_dot_data;
				std::vector<double> lastState = it->propSegs[s].getStateByIx(-1);
				std::vector<double> lastAccel = it->propSegs[s].getAccelByIx(-1);
				state_dot_data.insert(state_dot_data.end(), lastState.begin()+3, lastState.begin()+6);
				state_dot_data.insert(state_dot_data.end(), lastAccel.begin(), lastAccel.end());
				Eigen::VectorXd state_dot = Eigen::Map<Eigen::VectorXd>(&(state_dot_data[0]), 6, 1);

				// Scale derivatives
				state_dot.segment(0,3) *= it->freeVarScale[0]/it->freeVarScale[2];
				state_dot.segment(3,3) *= it->freeVarScale[1]/it->freeVarScale[2];

				double timeCoeff = it->equalArcTime ? 1.0/(it->numNodes - 1) : 1.0;
				int timeCol = it->equalArcTime ? 6*it->numNodes : 6*it->numNodes+s;

				Eigen::RowVectorXd dFdt_n = -1*dFdq_n2 * state_dot;
				it->DF[it->totalFree*row0 + timeCol] = timeCoeff*dFdt_n(0)/dvMax;
			}
		}
	}
	
	// Copute the difference between the actual deltaV and the desired deltaV
	it->FX[row0] = con.getData()[0] == 0 ? totalDV : totalDV/dvMax - 1;

	if(con.getType() == tpat_constraint::MAX_DELTA_V){
		// figure out which of the slack variables correspond to this constraint
		std::vector<int>::iterator slackIx = std::find(it->slackAssignCon.begin(),
			it->slackAssignCon.end(), c);

		// which column of the DF matrix the slack variable is in
		int slackCol = it->totalFree - it->numSlack + (slackIx - it->slackAssignCon.begin());
		
		// printf("MAX_DELTA_V Constraint Details:\n");
		// printf("Total DV")
		it->FX[row0] += it->X[slackCol]*it->X[slackCol];
		it->DF[it->totalFree*row0 + slackCol] = 2*it->X[slackCol];
	}
}//==============================================

/**
 *  @brief Compute the slack variable value for a delta-V constraint
 *  @details This function currently returns a hard-coded value of 1e-2
 * 
 *  @param it a pointer to the class containing all the data relevant to the corrections process
 *	@param con the constraint being applied
 * 
 *  @return Ideal value of the slack variable
 */
double tpat_model::multShoot_targetDeltaV_compSlackVar(const tpat_multShoot_data *it, tpat_constraint con) const{
	// double totalDV = 0;
	// for(int n = 0; n < it->numNodes-1; n++){
	// 	// compute squared magnitude of DV between node n and n+1
	// 	// This takes the form v_n,f - v_n+1,0
	// 	totalDV += it->deltaVs[n*3]*it->deltaVs[n*3] +
	// 		it->deltaVs[n*3+1]*it->deltaVs[n*3+1] + 
	// 		it->deltaVs[n*3+2]*it->deltaVs[n*3+2];
	// }
	// // Value of the constraint function
	// double F = totalDV - con.getData()[0];
	
	// No info about delta-Vs between segments exists yet because nothing
	// has been integrated... return a constant for now
	(void) it;
	(void) con;
	return 1e-2;

	// If F < 0, the constraint is satisfied, so choose a slack variable
	// that sets F = 0; else choose a small slack variable value
	// return F < 0 ? sqrt(std::abs(F)) : 1e-4;
}//====================================================

/**
 *	@brief Compute partials and constraint function values for time-of-flight constraints
 *
 *	This method *should* provide full functionality for any model; only 1's and 0's are
 *	used to relate TOFs.
 *	
 *	@param it a pointer to the class containing all the data relevant to the corrections process
 *	@param con a copy of the constraint object
 *	@param row0 the index of the row this constraint begins at
 *	
 *	@throws tpat_exception if variable time is set to OFF
 */
void tpat_model::multShoot_targetTOF(tpat_multShoot_data *it, tpat_constraint con, int row0) const{
	if(! it->varTime)
		throw tpat_exception("tpat_model::multShoot_targetTOF: Cannot target TOF when variable time is off!");

	if(it->equalArcTime){
		it->FX[row0] = it->X[6*it->numNodes];
		it->DF[it->totalFree*row0 + 6*it->numNodes] = 1;
	}else{
		// Sum all TOF for total, set partials w.r.t. integration times equal to one
		for(int s = 0; s < it->nodeset->getNumSegs(); s++){
			it->FX[row0] += it->X[6*it->numNodes+s];
			it->DF[it->totalFree*row0 + 6*it->numNodes+s] = 1;
		}
	}
	
	// subtract the desired TOF from the constraint to finish its computation
	it->FX[row0] -= con.getData()[0]*it->freeVarScale[2];
}//====================================================

/**
 *	@brief Compute partials and constraint function values for apse constraints
 *
 *	This method *should* provide full functionality for any autonomous model. Non-
 *	autonomous models will need to modify the function to account for epoch time
 *	
 *	@param it a pointer to the class containing all the data relevant to the corrections process
 *	@param con a copy of the constraint object
 *	@param row0 the index of the row this constraint begins at
 */
void tpat_model::multShoot_targetApse(tpat_multShoot_data *it, tpat_constraint con, int row0) const{
	std::vector<double> conData = con.getData();
	int nodeIx = it->nodeset->getNodeIx(con.getID());
	int Pix = (int)(conData[0]);	// index of primary
	double t = 0;	// If the system is non-autonomous, this will need to be replaced with an epoch time
	
	double sr = it->freeVarScale[0];
	double sv = it->freeVarScale[1];
	
	// Get the primary position
	std::vector<double> primPos = getPrimPos(t, it->sysData);

	// Get distance between node and primary in x, y, and z-coordinates, use non-scaled coordinates
	double dx = it->X[6*nodeIx+0]/sr - primPos[Pix*3+0];
	double dy = it->X[6*nodeIx+1]/sr - primPos[Pix*3+1];
	double dz = it->X[6*nodeIx+2]/sr - primPos[Pix*3+2];
	double vx = it->X[6*nodeIx+3]/sv;
	double vy = it->X[6*nodeIx+4]/sv;
	double vz = it->X[6*nodeIx+5]/sv;

	// Constraint function: r_dot = 0 (using non-scaled coordinates)
	it->FX[row0] = dx*vx + dy*vy + dz*vz;

	// Partials of F w.r.t. node state
	it->DF[it->totalFree*row0 + 6*nodeIx+0] = vx/sr;
	it->DF[it->totalFree*row0 + 6*nodeIx+1] = vy/sr;
	it->DF[it->totalFree*row0 + 6*nodeIx+2] = vz/sr;
	it->DF[it->totalFree*row0 + 6*nodeIx+3] = dx/sv;
	it->DF[it->totalFree*row0 + 6*nodeIx+4] = dy/sv;
	it->DF[it->totalFree*row0 + 6*nodeIx+5] = dz/sv;
}//====================================================




