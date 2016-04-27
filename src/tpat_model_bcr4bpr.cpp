/**
 *  @file tpat_model_bcr4bpr.cpp
 *  @brief Derivative of tpat_model, specific to BCR4BPR
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

#include "tpat_model_bcr4bpr.hpp"

#include "tpat_calculations.hpp"
#include "tpat_correction_engine.hpp"
#include "tpat_eigen_defs.hpp"
#include "tpat_nodeset_bcr4bp.hpp"
#include "tpat_sys_data_bcr4bpr.hpp"
#include "tpat_traj_bcr4bp.hpp"
#include "tpat_traj_step.hpp"
#include "tpat_event.hpp"
#include "tpat_node.hpp"
#include "tpat_utilities.hpp"

/**
 *  @brief Construct a BCR4BP Dynamic Model
 */
tpat_model_bcr4bpr::tpat_model_bcr4bpr() : tpat_model(MODEL_CR3BP) {
    coreStates = 6;
    stmStates = 36;
    extraStates = 6;
    allowedCons.push_back(tpat_constraint::SP);
    allowedCons.push_back(tpat_constraint::SP_RANGE);
    allowedCons.push_back(tpat_constraint::SP_DIST);
    allowedCons.push_back(tpat_constraint::SP_MAX_DIST);
}//==============================================

/**
 *  @brief Copy constructor
 *  @param m a model reference
 */
tpat_model_bcr4bpr::tpat_model_bcr4bpr(const tpat_model_bcr4bpr &m) : tpat_model(m) {}

/**
 *  @brief Assignment operator
 *  @param m a model reference
 */
tpat_model_bcr4bpr& tpat_model_bcr4bpr::operator =(const tpat_model_bcr4bpr &m){
	tpat_model::operator =(m);
	return *this;
}//==============================================

/**
 *  @brief Retrieve a pointer to the EOM function that computes derivatives
 *  for only the core states (i.e. simple)
 */
tpat_model::eom_fcn tpat_model_bcr4bpr::getSimpleEOM_fcn() const{
	return &simpleEOMs;
}//==============================================

/**
 *  @brief Retrieve a pointer to the EOM function that computes derivatives
 *  for all states (i.e. full)
 */
tpat_model::eom_fcn tpat_model_bcr4bpr::getFullEOM_fcn() const{
	return &fullEOMs;
}//==============================================

/**
 *  @brief Compute the positions of all primaries
 *
 *  @param t the epoch at which the computations occur
 *  @param sysData object describing the specific system
 *  @return an n x 3 vector (row-major order) containing the positions of
 *  n primaries; each row is one position vector in non-dimensional units
 */
std::vector<double> tpat_model_bcr4bpr::getPrimPos(double t, const tpat_sys_data *sysData) const{
    double primPos[9];
    const tpat_sys_data_bcr4bpr *bcSys = static_cast<const tpat_sys_data_bcr4bpr *>(sysData);
    getPrimaryPos(t, bcSys, primPos);

    return std::vector<double>(primPos, primPos+9);
}//==============================================

/**
 *  @brief Compute the velocities of all primaries
 *
 *  @param t the epoch at which the computations occur
 *  @param sysData object describing the specific system
 *  @return an n x 3 vector (row-major order) containing the velocities of
 *  n primaries; each row is one velocity vector in non-dimensional units
 */
std::vector<double> tpat_model_bcr4bpr::getPrimVel(double t, const tpat_sys_data *sysData) const{
    double primVel[9];
    const tpat_sys_data_bcr4bpr *bcSys = static_cast<const tpat_sys_data_bcr4bpr *>(sysData);
    getPrimaryVel(t, bcSys, primVel);

    return std::vector<double>(primVel, primVel+9);
}//==============================================

//------------------------------------------------------------------------------------------------------
//      Simulation Engine Functions
//------------------------------------------------------------------------------------------------------

/**
 *  @brief Takes an input state and time and saves the data to the trajectory
 *  @param y an array containing the core state and any extra states integrated
 *  by the EOM function, including STM elements.
 *  @param t the time at the current integration state
 *  @param traj a pointer to the trajectory we should store the data in
 */
void tpat_model_bcr4bpr::sim_saveIntegratedData(const double* y, double t, tpat_traj* traj) const{

	// Cast the system data to the appropriate derivative type
    const tpat_sys_data_bcr4bpr *bcSys = static_cast<const tpat_sys_data_bcr4bpr*>(traj->getSysData());

    // Compute acceleration (elements 3-5)
    double dsdt[6] = {0};
    eomParamStruct paramStruct(bcSys);
    simpleEOMs(t, y, dsdt, &paramStruct);
    
    // step(state, time, accel, stm) - y(0:5) holds the state, y(6:41) holds the STM
    tpat_traj_step step(y, t, dsdt+3, y+6);

    traj->appendStep(step);
    
    tpat_traj_bcr4bp *bcTraj = static_cast<tpat_traj_bcr4bp*>(traj);
    bcTraj->set_dqdT(-1, y+42); // dqdT is stored in y(42:47)
}//=====================================================

/**
 *  @brief Use a correction algorithm to accurately locate an event crossing
 *
 *  The simulation engine calls this function if and when it determines that an event 
 *  has been crossed. To accurately locate the event, we employ differential corrections
 *  and find the exact event occurence in space and time.
 *
 *  @param event the event we're looking for
 *  @param traj a pointer to the trajectory the event should occur on
 *  @param ic the core state vector for this system
 *  @param t0 non-dimensional time at the beginning of the search arc
 *  @param tof the time-of-flight for the arc to search over
 *  @param verbose whether or not we should be verbose with output messages
 *
 *  @return wether or not the event has been located. If it has, a new point
 *  has been appended to the trajectory's data vectors.
 */
bool tpat_model_bcr4bpr::sim_locateEvent(tpat_event event, tpat_traj *traj,
    const double *ic, double t0, double tof, tpat_verbosity_tp verbose) const{

    // **** Make sure you fix the epoch of the first node as well as the states
    double IC[7] = {0};
    std::copy(ic, ic+6, IC);
    IC[6] = t0;

    // Recast system data pointer
    const tpat_sys_data_bcr4bpr *bcSys = static_cast<const tpat_sys_data_bcr4bpr*>(traj->getSysData());

    // Create a nodeset for this particular type of system
    printVerb(verbose == ALL_MSG, "  Creating nodeset for event location\n");
    tpat_nodeset_bcr4bp eventNodeset(IC, bcSys, t0,
        tof, 2, tpat_nodeset::DISTRO_TIME);

    // Constraint to keep first node unchanged
    tpat_constraint fixFirstCon(tpat_constraint::STATE, 0, IC, 7);

    // Constraint to enforce event
    tpat_constraint eventCon(event.getConType(), 1, event.getConData());

    eventNodeset.addConstraint(fixFirstCon);
    eventNodeset.addConstraint(eventCon);

    if(verbose == ALL_MSG){ eventNodeset.print(); }

    printVerb(verbose == ALL_MSG, "  Applying corrections process to locate event\n");
    tpat_correction_engine corrector;
    corrector.setVarTime(true);
    corrector.setTol(traj->getTol());
    corrector.setVerbose(verbose);
    corrector.setFindEvent(true);   // apply special settings to minimize computations
    try{
        corrector.multShoot(&eventNodeset);
    }catch(tpat_diverge &e){
        printErr("Unable to locate event; corrector diverged\n");
        return false;
    }catch(tpat_linalg_err &e){
        printErr("LinAlg Err while locating event; bug in corrector!\n");
        return false;
    }

    // Because we set findEvent to true, this output nodeset should contain
    // the full (42 or 48 element) final state
    tpat_nodeset_bcr4bp correctedNodes = corrector.getBCR4BPR_Output();

    std::vector<double> state = correctedNodes.getNode(-1).getPosVelState();
    std::vector<double> extra = correctedNodes.getNode(-1).getExtraParams();
    extra.insert(extra.begin(), state.begin(), state.end());

    // event time is the TOF of corrected path + time at the state we integrated from
    double eventTime = correctedNodes.getTOF(0) + t0;

    // Use the data stored in nodes and save the state and time of the event occurence
    sim_saveIntegratedData(&(extra[0]), eventTime, traj);
    
    return true;
}//=========================================================

//------------------------------------------------------------------------------------------------------
//      Multiple Shooting Functions
//------------------------------------------------------------------------------------------------------

/**
 *  @brief Initialize the corrector's design vector with position and velocity states,
 *  and times-of-flight.
 *
 *  Derived models may replace this function or call it and then append more design 
 *  variables.
 *
 *  @param it a pointer to the corrector's iteration data structure
 *  @param set a pointer to the nodeset being corrected
 */
void tpat_model_bcr4bpr::multShoot_initDesignVec(iterationData *it, const tpat_nodeset *set) const{
    // Call base class to do most of the work
    tpat_model::multShoot_initDesignVec(it, set);

    if(it->equalArcTime)
        throw tpat_exception("tpat_model_bcr4bpr::multShoot_initDesignVec: Equal Arc Times have not been implemented in this Model (yet)!");

    // Append the Epoch for each node
    if(it->varTime){
        // epochs come after ALL the TOFs have been added
        const tpat_nodeset_bcr4bp *bcSet = static_cast<const tpat_nodeset_bcr4bp *>(set);
        for(int n = 0; n < bcSet->getNumNodes(); n++){
            it->X.push_back(bcSet->getEpoch(n));
        }
    }
}//============================================================

void tpat_model_bcr4bpr::multShoot_scaleDesignVec(iterationData *it, const tpat_nodeset *set) const{
    (void) set;

    tpat_model::multShoot_scaleDesignVec(it, set);   // Perform default behavior
    const tpat_nodeset_bcr4bp *bcSet = static_cast<const tpat_nodeset_bcr4bp *>(set);

    // Compute the largest magnitude
    Eigen::VectorXd allEpochs(it->numNodes);
    for(int n = 0; n < it->numNodes; n++){
        if(it->varTime){
            allEpochs(n) = it->X[it->equalArcTime ? 6*it->numNodes+1+n : 7*it->numNodes-1+n];
        }else{
            allEpochs(n) = bcSet->getEpoch(n);
        }
    }
    double maxEpoch = allEpochs.cwiseAbs().maxCoeff();

    // Scale each variable type by its maximum so that all values are between -1 and +1
    it->freeVarScale[3] = maxEpoch == 0 ? 1 : 1/maxEpoch;

    printf("  Epo = %.6f\n", it->freeVarScale[3]);

    // Scale all Epochs
    if(it->varTime){
        for(int n = 0; n < it->numNodes; n++){
            it->X[7*it->numNodes-1+n] *= it->freeVarScale[3];
        }
    }
}//===================================================

/**
 *  @brief Create continuity constraints for the correction algorithm; this function
 *  creates position and velocity constraints.
 *
 *  This function overrides the base model's to add time continuity
 *
 *  @param it a pointer to the corrector's iteration data structure
 *  @param set a pointer to the nodeset being corrected
 */ 
void tpat_model_bcr4bpr::multShoot_createContCons(iterationData *it, const tpat_nodeset *set) const{
    tpat_model::multShoot_createContCons(it, set);

    if(it->varTime){
        std::vector<double> zero {0};
        for(int n = 1; n < set->getNumNodes(); n++){
            tpat_constraint timeCont(tpat_constraint::CONT_EX, n, zero);   // 0th index is epoch time in extraParam
            it->allCons.push_back(timeCont);
        }
    }
}//============================================================

/**
 *  @brief Retrieve the initial conditions for a segment that the correction
 *  engine will integrate.
 *
 *  @param it a pointer to the corrector's iteration data structure
 *  @param set a pointer to the nodeset being corrected
 *  @param n the index of the node that serves as the initial state
 *  @param ic a pointer to a 6-element initial state array
 *  @param t0 a pointer to a double representing the initial time (epoch)
 *  @param tof a pointer to a double the time-of-flight on the segment.
 */
void tpat_model_bcr4bpr::multShoot_getSimICs(const iterationData *it, const tpat_nodeset *set, int n,
    double *ic, double *t0, double *tof) const{

    tpat_model::multShoot_getSimICs(it, set, n, ic, t0, tof);   // Perform default behavior

    // Compute and reverse-scale epoch
    const tpat_nodeset_bcr4bp *bcSet = static_cast<const tpat_nodeset_bcr4bp *>(set);

    if(it->varTime){
        *t0 = it->equalArcTime ? it->X[6*it->numNodes+1+n] : it->X[7*it->numNodes-1+n];
        *t0 /= it->freeVarScale[3];
    }else{
        *t0 = bcSet->getEpoch(n);
    }
}//============================================================

/**
 *  @brief Compute the value of a slack variable for an inequality constraint.
 *  @details Computing the value of the slack variable can avoid unneccessary 
 *  shooting iterations when the inequality constraint is already met. If the 
 *  inequality constraint is met, the value returned by this function will make
 *  the constraint function evaluate to zero.
 *  
 *  This method adds additional functionality for the BCR4BP and calls the base
 *  model version for most constraint types
 *  
 *  Note: This function should be called after the state variable vector has 
 *  been initialized by the multiple shooting algorithm
 * 
 *  @param it the iterationData object associated with the multiple shooting process
 *  @param con the inequality constraint for which the slack variable is being computed
 * 
 *  @return The value of the slack variable that minimizes the constraint function
 *  without setting the slack variable to zero
 */
double tpat_model_bcr4bpr::multShoot_getSlackVarVal(const iterationData *it, tpat_constraint con) const{
    switch(con.getType()){
        case tpat_constraint::SP_RANGE:
            return multShoot_targetSPMag_compSlackVar(it, con);
        case tpat_constraint::SP_MAX_DIST:
            return multShoot_targetSP_maxDist_compSlackVar(it, con);
        default:
            return tpat_model::multShoot_getSlackVarVal(it, con);
    }
}//===========================================================

/**
 *  @brief Compute constraint function and partial derivative values for a constraint
 *  
 *  This function calls its relative in the tpat_model base class and appends additional
 *  instructions specific to the BCR4BPR
 *
 *  @param it a pointer to the corrector's iteration data structure
 *  @param con the constraint being applied
 *  @param c the index of the constraint within the total constraint vector (which is, in
 *  turn, stored in the iteration data)
 */ 
void tpat_model_bcr4bpr::multShoot_applyConstraint(iterationData *it, tpat_constraint con, int c) const{

    // Let the base class do its thing first
    tpat_model::multShoot_applyConstraint(it, con, c);

    // Handle constraints specific to the CR3BP
    int row0 = it->conRows[c];

    switch(con.getType()){
        case tpat_constraint::APSE:
            multShoot_targetApse(it, con, row0);
            break;
        case tpat_constraint::SP:
            multShoot_targetSP(it, con, row0);
            break;
        case tpat_constraint::SP_RANGE:
            multShoot_targetSP_mag(it, con, c);
            break;
        case tpat_constraint::SP_DIST:
        case tpat_constraint::SP_MAX_DIST:
            multShoot_targetSP_dist(it, con, c);
            break;
        default: break;
    }
}//=========================================================

/**
 *  @brief Compute position and velocity constraint values and partial derivatives
 *
 *  This function computes and stores the default position continuity constraints as well
 *  as velocity constraints for all nodes marked continuous in velocity. The delta-Vs
 *  between arc segments and node states are recorded, and the partial derivatives of each
 *  node with respect to other nodes and integration time are all computed
 *  and placed in the appropriate spots in the Jacobian matrix.
 *
 *  This function replaces the one found in the base model
 *
 *  @param it a pointer to the correctors iteration data structure
 *  @param con the constraint being applied
 *  @param row0 the first row this constraint applies to
 */
void tpat_model_bcr4bpr::multShoot_targetPosVelCons(iterationData* it, tpat_constraint con, int row0) const{
    // Call base function first to do most of the work
    tpat_model::multShoot_targetPosVelCons(it, con, row0);

    // Add epoch dependencies for this model
    if(it->varTime){
        int n = con.getNode();
        std::vector<double> conData = con.getData();
        std::vector<double> last_dqdT = it->allSegs.at(n-1).getExtraParam(-1, 1);

        // Loop through conData
        for(size_t s = 0; s < conData.size(); s++){
            if(!isnan(conData[s])){
                double scale = s < 3 ? it->freeVarScale[0] : it->freeVarScale[1];
                // Epoch dependencies
                it->DF[it->totalFree*(row0+s) + 7*it->numNodes-1+n-1] = last_dqdT[s]*scale/it->freeVarScale[3];
            }
        }
    }
}//=========================================================

/**
 *  @brief Computes continuity constraints for constraints with the <tt>CONT_EX</tt> type.
 *
 *  This function overrides the base model function
 *
 *  @param it a pointer to the correctors iteration data structure
 *  @param con the constraint being applied
 *  @param row0 the first row this constraint applies to
 */
void tpat_model_bcr4bpr::multShoot_targetExContCons(iterationData *it, tpat_constraint con, int row0) const{
    int n = con.getNode();
    if(con.getData()[0] == 0){
        /* Add time-continuity constraints if applicable; we need to match
        the epoch time of node n to the sum of node n-1's epoch and TOF */
        if(it->varTime){
            int epochCol = it->equalArcTime ? 6*it->numNodes+1+n : 7*it->numNodes-1+n;
            int tofCol = it->equalArcTime ? 6*it->numNodes : 6*it->numNodes+n-1;
            
            double T0 = it->X[epochCol-1]/it->freeVarScale[3];
            double tof = it->equalArcTime ? it->X[tofCol]/(it->numNodes - 1) : it->X[tofCol];
            tof /= it->freeVarScale[2];
            double T1 = it->X[epochCol]/it->freeVarScale[3];

            it->FX[row0] = T1 - (T0 + tof);
            it->DF[it->totalFree*(row0) + tofCol] = -1/it->freeVarScale[2];
            it->DF[it->totalFree*(row0) + epochCol-1] = -1/it->freeVarScale[3];
            it->DF[it->totalFree*(row0) + epochCol] = 1/it->freeVarScale[3];
        }
    }else{
        throw tpat_exception("tpat_model_bcr4bpr::multShoot_createExContCons: Unrecognized extra constraint index");
    }
}//=========================================================

/**
 *  @brief Compute partials and constraint functions for nodes constrained with <tt>STATE</tt>.
 *
 *  This method replaces the base class function and allows the user to constrain epoch as well
 *  as the configuration space states
 *
 *  @param it a pointer to the class containing all the data relevant to the corrections process
 *  @param con the constraint being applied
 *  @param row0 the index of the row this constraint begins at
 */
void tpat_model_bcr4bpr::multShoot_targetState(iterationData* it, tpat_constraint con, int row0) const{
    std::vector<double> conData = con.getData();
    int n = con.getNode();
    // Allow user to constrain all 7 states
    
    int count = 0;  // Count # rows since some may be skipped (NAN)
    for(int s = 0; s < ((int)con.getData().size()); s++){
        if(!isnan(conData[s])){
            if(s < 6){
                double scale = s < 3 ? it->freeVarScale[0] : it->freeVarScale[1];
                it->FX[row0+count] = it->X[6*n+s] - conData[s]*scale;
                it->DF[it->totalFree*(row0 + count) + 6*n + s] = 1;
                count++;
            }else if(s == 6){
                // Allow constraining epoch
                it->FX[row0+count] = it->X[7*it->numNodes-1+n] - conData[s]*it->freeVarScale[3];
                it->DF[it->totalFree*(row0 + count) + 7*it->numNodes-1+n] = 1;
                count++;
            }else{
                throw tpat_exception("State constraints must have <= 6 elements");
            }
        }
    }
}//=================================================

/**
 *  @brief Compute partials and constraint functions for nodes constrained with <tt>DIST</tt>, 
 *  <tt>MIN_DIST</tt>, or <tt>MAX_DIST</tt>
 *
 *  This method overrides the base class function to add functionality for epoch-time dependencies
 *
 *  @param it a pointer to the class containing all the data relevant to the corrections process
 *  @param con a copy of the constraint object
 *  @param c the index of this constraint in the constraint vector object
 */
void tpat_model_bcr4bpr::multShoot_targetDist(iterationData* it, tpat_constraint con, int c) const{

    std::vector<double> conData = con.getData();
    int n = con.getNode();
    int Pix = (int)(conData[0]);    // index of primary
    int row0 = it->conRows[c];
    double sr = it->freeVarScale[0];    // Position scalar
    double sT = it->freeVarScale[3];    // Epoch scalar

    // Get the node epoch either from the design vector or from the original set of nodes
    int epochCol = it->equalArcTime ? 6*it->numNodes+1+n : 7*it->numNodes-1+n;
    double t0 = it->varTime ? it->X[epochCol]/sT : it->origNodes.at(n).getExtraParam(1);

    // Get the primary position
    std::vector<double> primPos = getPrimPos(t0, it->sysData);

    // Get distance between node and primary in x, y, and z-coordinates
    double dx = it->X[6*n+0] - primPos[Pix*3+0]*sr;
    double dy = it->X[6*n+1] - primPos[Pix*3+1]*sr;
    double dz = it->X[6*n+2] - primPos[Pix*3+2]*sr;

    double h = sqrt(dx*dx + dy*dy + dz*dz);     // true distance

    // Compute difference between desired distance and true distance
    it->FX[row0] = h - conData[1]*sr;

    // Partials with respect to node position states
    it->DF[it->totalFree*row0 + 6*n + 0] = dx/h;
    it->DF[it->totalFree*row0 + 6*n + 1] = dy/h;
    it->DF[it->totalFree*row0 + 6*n + 2] = dz/h;

    if(it->varTime){
        // Epoch dependencies from primary positions
        double dhdr_data[] = {-dx/h, -dy/h, -dz/h};
        std::vector<double> primVel = getPrimVel(t0, it->sysData);
    
        Eigen::RowVector3d dhdr = Eigen::Map<Eigen::RowVector3d>(dhdr_data, 1, 3);
        Eigen::Vector3d drdT = Eigen::Map<Eigen::Vector3d>(&(primVel[Pix*3]), 3, 1);
        
        double prod = dhdr*drdT;
        it->DF[it->totalFree*row0 + epochCol] = prod*sr/sT;
    }

    // Extra stuff for inequality constraints
    if(con.getType() == tpat_constraint::MIN_DIST || 
        con.getType() == tpat_constraint::MAX_DIST ){
        // figure out which of the slack variables correspond to this constraint
        std::vector<int>::iterator slackIx = std::find(it->slackAssignCon.begin(), 
            it->slackAssignCon.end(), c);

        // which column of the DF matrix the slack variable is in
        int slackCol = it->totalFree - it->numSlack + (slackIx - it->slackAssignCon.begin());
        int sign = con.getType() == tpat_constraint::MAX_DIST ? 1 : -1;

        // printf("Dist from P%d is %f (%s %f)\n", Pix, h,
        //     con.getType() == tpat_constraint::MIN_DIST ? "Min" : "Max", conData[1]);
        // printf("  Slack Var^2 = %e\n", it->X[slackCol]*it->X[slackCol]);
        // Subtract squared slack variable from constraint
        it->FX[row0] += sign*it->X[slackCol]*it->X[slackCol];

        // Partial with respect to slack variable
        it->DF[it->totalFree*row0 + slackCol] = sign*2*it->X[slackCol];
    }
}// End of targetDist() =========================================

/**
 *  @brief  Compute the value of the slack variable for inequality distance constraints
 *  @details This function computes a value for the slack variable in an
 *  inequality distance constraint. If the constraint is already met by the initial
 *  design, using this value will prevent the multiple shooting algorithm from
 *  searching all over for the propper value.
 *  
 *  Note: This method overrides the base class function to add functionality for non-zero epochs
 * 
 *  @param it the iteration data object for the multiple shooting process
 *  @param con the constraint the slack variable applies to
 *  @return the value of the slack variable that minimizes the constraint function
 *  without setting the slack variable equal to zero
 */
double tpat_model_bcr4bpr::multShoot_targetDist_compSlackVar(const iterationData* it, tpat_constraint con) const{
    std::vector<double> conData = con.getData();
    int n = con.getNode();
    int Pix = (int)(conData[0]);    // index of primary 
    double sr = it->freeVarScale[0];
    double sT = it->freeVarScale[3];

    // Get the node epoch either from the design vector or from the original set of nodes
    int epochCol = it->equalArcTime ? 6*it->numNodes+1+n : 7*it->numNodes-1+n;
    double t0 = it->varTime ? it->X[epochCol]/sT : it->origNodes.at(n).getExtraParam(1);

    // Get the primary position
    std::vector<double> primPos = getPrimPos(t0, it->sysData);

    // Get distance between node and primary in x, y, and z-coordinates
    double dx = it->X[6*n+0] - primPos[Pix*3+0]*sr;
    double dy = it->X[6*n+1] - primPos[Pix*3+1]*sr;
    double dz = it->X[6*n+2] - primPos[Pix*3+2]*sr;

    double h = sqrt(dx*dx + dy*dy + dz*dz);     // true distance
    int sign = con.getType() == tpat_constraint::MAX_DIST ? 1 : -1;
    double diff = conData[1]*sr - h;

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
 *  @brief Compute partials and constraints for all nodes constrained with <tt>DELTA_V</tt> or
 *  <tt>MIN_DELTA_V</tt>
 *
 *  Because the delta-V constraint applies to the entire trajectory, the constraint function values
 *  and partial derivatives must be computed for each node along the trajectory. This function
 *  takes care of all of them at once.
 *
 *  This function overrides the base targeting function to add support for epoch dependencies
 *
 *  @param it a pointer to the class containing all the data relevant to the corrections process
 *  @param con the constraint being applied
 *  @param c the index of the first row for this constraint
 */
void tpat_model_bcr4bpr::multShoot_targetDeltaV(iterationData* it, tpat_constraint con, int c) const{
    // Call base function to take care of most of the constraint computations and partials
    tpat_model::multShoot_targetDeltaV(it, con, c);

    if(it->varTime){
        // Add partials w.r.t. epoch time
        int row0 = it->conRows[c];

        // Don't allow dividing by zero, but otherwise scale by value to keep order ~1
        double dvMax = con.getData()[0] == 0 ? 1 : con.getData()[0]*it->freeVarScale[1];

        for(int n = 0; n < it->numNodes-1; n++){
            // compute magnitude of DV between node n and n+1
            // This takes the form v_n,f - v_n+1,0
            double dvx = it->deltaVs[n*3] * it->freeVarScale[1];
            double dvy = it->deltaVs[n*3+1]*it->freeVarScale[1];
            double dvz = it->deltaVs[n*3+2]*it->freeVarScale[1];
            double dvMag = sqrt(dvx*dvx + dvy*dvy + dvz*dvz);

            // Don't bother computing partials if this dv is zero; all partials will equal zero
            // but the computation would divide by zero too, which would be... unfortunate.
            if(dvMag > 0){
                // Compute parial w.r.t. node n+1 (where velocity is discontinuous)
                double dFdq_n2_data[] = {0, 0, 0, -dvx/dvMag, -dvy/dvMag, -dvz/dvMag};
                Eigen::RowVectorXd dFdq_n2 = Eigen::Map<Eigen::RowVectorXd>(dFdq_n2_data, 1, 6);

                // Compute partial w.r.t. epoch time n
                std::vector<double> last_dqdT = it->allSegs.at(n).getExtraParam(-1, 1);
                Eigen::VectorXd dqdT = Eigen::Map<Eigen::VectorXd>(&(last_dqdT[0]), 6, 1);
                dqdT.segment(0,3) *= it->freeVarScale[0]/it->freeVarScale[3];
                dqdT.segment(3,3) *= it->freeVarScale[1]/it->freeVarScale[3];

                double dFdT_n = dFdq_n2*dqdT;
                int epochCol = it->equalArcTime ? 6*it->numNodes+1+n : 7*it->numNodes-1+n;
                it->DF[it->totalFree*row0 + epochCol] = -dFdT_n/dvMax;
            }
        }
    }
}//==============================================

void tpat_model_bcr4bpr::multShoot_targetApse(iterationData *it, tpat_constraint con, int row0) const{
    std::vector<double> conData = con.getData();
    int Pix = (int)(conData[0]);    // index of primary
    int n = con.getNode();

    double sr = it->freeVarScale[0];
    double sv = it->freeVarScale[1];
    double sT = it->freeVarScale[3];

    int epochCol = it->equalArcTime ? 6*it->numNodes+1+n : 7*it->numNodes-1+n;
    double t0 = it->varTime ? it->X[epochCol]/sT : it->origNodes.at(n).getExtraParam(1);

    const tpat_sys_data_bcr4bpr *bcSys = static_cast<const tpat_sys_data_bcr4bpr *>(it->sysData);
    double primPos[9], primVel[9], primAccel[9];

    getPrimaryPos(t0, bcSys, primPos);
    getPrimaryVel(t0, bcSys, primVel);
    getPrimaryAccel(t0, bcSys, primAccel);

    double dx = it->X[6*n+0]/sr - primPos[3*Pix+0];
    double dy = it->X[6*n+1]/sr - primPos[3*Pix+1];
    double dz = it->X[6*n+2]/sr - primPos[3*Pix+2];
    double dvx = it->X[6*n+3]/sv - primVel[3*Pix+0];
    double dvy = it->X[6*n+4]/sv - primVel[3*Pix+1];
    double dvz = it->X[6*n+5]/sv - primVel[3*Pix+2];

    it->FX[row0] = dx*dvx + dy*dvy + dz*dvz;

    it->DF[it->totalFree*row0 + 6*n+0] = dvx/sr;
    it->DF[it->totalFree*row0 + 6*n+1] = dvy/sr;
    it->DF[it->totalFree*row0 + 6*n+2] = dvz/sr;
    it->DF[it->totalFree*row0 + 6*n+3] = dx/sv;
    it->DF[it->totalFree*row0 + 6*n+4] = dy/sv;
    it->DF[it->totalFree*row0 + 6*n+5] = dz/sv;

    if(it->varTime){
        it->DF[it->totalFree*row0 + epochCol] = -1*(dvx*primVel[3*Pix+0] + dvy*primVel[3*Pix+1] + dvz*primVel[3*Pix+2])/sT;
        it->DF[it->totalFree*row0 + epochCol] -= (dx*primAccel[3*Pix+0] + dy*primAccel[3*Pix+1] + dz*primAccel[3*Pix+2])/sT;
    }
}//===================================================

/**
 *  @brief Compute partials and constraint values for nodes constrained with <tt>SP</tt>
 *
 *  This function computes three constraint values and three rows of partials for the Jacobian.
 *  Each row/function corresponds to one position state. The FX and DF matrices are updated
 *  in place by editing their values stored in <tt>it</tt>
 *
 *  @param it the iterationData object holding the current data for the corrections process
 *  @param con the constraint being applied
 *  @param row0 the index of the first row for this constraint
 */
void tpat_model_bcr4bpr::multShoot_targetSP(iterationData* it, tpat_constraint con, int row0) const{

    int n = con.getNode();
    int epochCol = it->equalArcTime ? 6*it->numNodes+1+n : 7*it->numNodes-1+n;
    double t0 = it->varTime ? it->X[epochCol]/it->freeVarScale[3] : it->origNodes.at(n).getExtraParam(1);

    const tpat_sys_data_bcr4bpr *bcSysData = static_cast<const tpat_sys_data_bcr4bpr *> (it->sysData);

    std::vector<double> primPosData = getPrimPos(t0, it->sysData);
    double sr = it->freeVarScale[0];

    // Get primary positions at the specified epoch time
    Matrix3Rd primPos = Eigen::Map<Matrix3Rd>(&(primPosData[0]), 3, 3);

    double *X = &(it->X[0]);
    Eigen::Vector3d r = Eigen::Map<Eigen::Vector3d>(X+6*n, 3, 1);   // Position vector

    // Create relative position vectors between s/c and primaries
    Eigen::Vector3d r_p1 = r/sr - primPos.row(0).transpose();
    Eigen::Vector3d r_p2 = r/sr - primPos.row(1).transpose();
    Eigen::Vector3d r_p3 = r/sr - primPos.row(2).transpose();

    double d1 = r_p1.norm();
    double d2 = r_p2.norm();
    double d3 = r_p3.norm();

    double k = bcSysData->getK();
    double mu = bcSysData->getMu();
    double nu = bcSysData->getNu();

    // Evaluate three constraint function values 
    Eigen::Vector3d conEval;
    conEval.noalias() = -(1/k - mu)*r_p1/pow(d1, 3) - (mu - nu)*r_p2/pow(d2,3) - nu*r_p3/pow(d3, 3);
    
    // printf("Node %d: Saddle Point Constraint Data:", n);
    // printf("Epoch = %.16f\n", epoch);
    // printf("s = [%.16f, %.16f %.16f], mag = %.16f\n", r_p1(0), r_p1(1), r_p1(2), d1);
    // printf("e = [%.16f, %.16f %.16f], mag = %.16f\n", r_p2(0), r_p2(1), r_p2(2), d2);
    // printf("m = [%.16f, %.16f %.16f], mag = %.16f\n", r_p3(0), r_p3(1), r_p3(2), d3);
    // printf("mu = %.16f\n", mu);
    // printf("nu = %.16f\n", nu);
    // printf("FX = [%.16f, %.16f %.16f]\n", conEval(0), conEval(1), conEval(2));

    // Parials w.r.t. node position r
    double dFdq_data[9] = {0};
    dFdq_data[0] = -(1/k - mu)*(1/pow(d1,3) - 3*pow(r_p1(0),2)/pow(d1,5)) -
        (mu-nu)*(1/pow(d2,3) - 3*pow(r_p2(0),2)/pow(d2,5)) - nu*(1/pow(d3,3) - 
        3*pow(r_p3(0),2)/pow(d3,5));     //dxdx
    dFdq_data[1] = (1/k - mu)*3*r_p1(0)*r_p1(1)/pow(d1,5) + 
        (mu - nu)*3*r_p2(0)*r_p2(1)/pow(d2,5) +
        nu*3*r_p3(0)*r_p3(1)/pow(d3,5);   //dxdy
    dFdq_data[2] = (1/k - mu)*3*r_p1(0)*r_p1(2)/pow(d1,5) +
        (mu - nu)*3*r_p2(0)*r_p2(2)/pow(d2,5) +
        nu*3*r_p3(0)*r_p3(2)/pow(d3,5);   //dxdz
    dFdq_data[3] = dFdq_data[1];    // dydx = dxdy
    dFdq_data[4] = -(1/k - mu)*(1/pow(d1,3) - 3*pow(r_p1(1),2)/pow(d1,5)) -
        (mu-nu)*(1/pow(d2,3) - 3*pow(r_p2(1),2)/pow(d2,5)) - 
        nu*(1/pow(d3,3) - 3*pow(r_p3(1),2)/pow(d3,5));   //dydy
    dFdq_data[5] = (1/k - mu)*3*r_p1(1)*r_p1(2)/pow(d1,5) +
        (mu - nu)*3*r_p2(1)*r_p2(2)/pow(d2,5) +
        nu*3*r_p3(1)*r_p3(2)/pow(d3,5);   //dydz
    dFdq_data[6] = dFdq_data[2];    //dzdx = dxdz
    dFdq_data[7] = dFdq_data[5];    //dzdy = dydz
    dFdq_data[8] = -(1/k - mu)*(1/pow(d1,3) - 3*pow(r_p1(2),2)/pow(d1,5)) -
            (mu-nu)*(1/pow(d2,3) - 3*pow(r_p2(2),2)/pow(d2,5)) - nu*(1/pow(d3,3) - 
            3*pow(r_p3(2),2)/pow(d3,5)); //dzdz

    Matrix3Rd dFdq = Eigen::Map<Matrix3Rd>(dFdq_data, 3, 3);
    dFdq /= sr;

    // Copy data into the correct vectors/matrices
    double* conEvalPtr = conEval.data();
    double* dFdq_ptr = dFdq.data();

    double *FX = &(it->FX[0]);
    double *DF = &(it->DF[0]);

    std::copy(conEvalPtr, conEvalPtr+3, FX+row0);
    std::copy(dFdq_ptr, dFdq_ptr+3, DF + it->totalFree*row0 + 6*n);
    std::copy(dFdq_ptr+3, dFdq_ptr+6, DF + it->totalFree*(row0+1) + 6*n);
    std::copy(dFdq_ptr+6, dFdq_ptr+9, DF + it->totalFree*(row0+2) + 6*n);

    if(it->varTime){
        // Get primary velocities at the specified epoch time
        std::vector<double> primVelData = getPrimVel(t0, it->sysData);
        Matrix3Rd primVel = Eigen::Map<Matrix3Rd>(&(primVelData[0]), 3, 3);

        // Compute partials of state w.r.t. primary positions; dont' compute partials
        // for P1 because its velocity is zero in the rotating frame
        double dfdr2_data[18] = {0};   double dfdr3_data[18] = {0};

        dfdr2_data[9] = -1/pow(d2,3) + 3*pow(r_p2(0),2)/pow(d2,5);        //dxdx2
        dfdr2_data[10] = 3*r_p2(0)*r_p2(1)/pow(d2,5);                  //dxdy2
        dfdr2_data[11] = 3*r_p2(0)*r_p2(2)/pow(d2,5);                  //dxdz2
        dfdr2_data[13] = -1/pow(d2,3) + 3*pow(r_p2(1),2)/pow(d2,5);       //dydy2
        dfdr2_data[14] = 3*r_p2(1)*r_p2(2)/pow(d2,5);                  //dydz2
        dfdr2_data[17] = -1/pow(d2,3) + 3*pow(r_p2(2),2)/pow(d2,5);       //dzdz2

        dfdr2_data[12] = dfdr2_data[10];      // Fill in symmetric matrix
        dfdr2_data[15] = dfdr2_data[11];
        dfdr2_data[16] = dfdr2_data[14];

        dfdr3_data[9] = -1/pow(d3,3) + 3*pow(r_p3(0),2)/pow(d3,5);        //dxdx3
        dfdr3_data[10] = 3*r_p3(0)*r_p3(1)/pow(d3,5);                  //dxdy3
        dfdr3_data[11] = 3*r_p3(0)*r_p3(2)/pow(d3,5);                  //dxdz3
        dfdr3_data[13] = -1/pow(d3,3) + 3*pow(r_p3(1),2)/pow(d3,5);       //dydy3
        dfdr3_data[14] = 3*r_p3(1)*r_p3(2)/pow(d3,5);                  //dydz3
        dfdr3_data[17] = -1/pow(d3,3) + 3*pow(r_p3(2),2)/pow(d3,5);       //dzdz3

        dfdr3_data[12] = dfdr3_data[10];      // Fill in symmetric matrix
        dfdr3_data[15] = dfdr3_data[11];
        dfdr3_data[16] = dfdr3_data[14];

        MatrixXRd dFdr2 = Eigen::Map<MatrixXRd>(dfdr2_data, 6, 3);
        MatrixXRd dFdr3 = Eigen::Map<MatrixXRd>(dfdr3_data, 6, 3);

        // scale matrices by constants
        dFdr2 *= -1*(mu - nu)/sr;
        dFdr3 *= -1*nu/sr;

        // Compute partials of constraint function w.r.t. epoch time
        Eigen::VectorXd dFdT;
        dFdT.noalias() = dFdr2*(primVel.row(1).transpose()) + dFdr3*(primVel.row(2).transpose());
        dFdT *= sr/(it->freeVarScale[3]);

        DF[it->totalFree*row0 + epochCol] = dFdT(3);
        DF[it->totalFree*(row0+1) + epochCol] = dFdT(4);
        DF[it->totalFree*(row0+2) + epochCol] = dFdT(5);
    }
}// End of SP Targeting ==============================

/**
 *  @brief Compute partials and constraint values for nodes constrained with <tt>SP_RANGE</tt>
 *  @details This function computes one constraint value and one row of partials for the Jacobian
 *  because the total acceleration magnitude is targeted rather than individual acceleration 
 *  vector components. The FX and DF matrices are update in place by editing their values 
 *  stored in <tt>it</tt>
 * 
 *  @param it the iterationData object holding the current data for the corrections process
 *  @param con the constraint being applied
 *  @param row0 the index of the first row for this constraint
 */
void tpat_model_bcr4bpr::multShoot_targetSP_mag(iterationData* it, tpat_constraint con, int c) const{

    // int row0 = it->conRows[c];
    // int n = con.getNode();
    // double Amax = con.getData()[0];
    // int epochCol = it->equalArcTime ? 6*it->numNodes+1+n : 7*it->numNodes-1+n;
    // double t0 = it->varTime ? it->X[epochCol]/it->freeVarScale[3] : it->origNodes.at(n).getExtraParam(1);

    // const tpat_sys_data_bcr4bpr *bcSysData = static_cast<const tpat_sys_data_bcr4bpr *> (it->sysData);
    // double sr = it->freeVarScale[0];

    // std::vector<double> primPosData = getPrimPos(t0, it->sysData);

    // // Get primary positions at the specified epoch time
    // Matrix3Rd primPos = Eigen::Map<Matrix3Rd>(&(primPosData[0]), 3, 3);

    // double *X = &(it->X[0]);
    // Eigen::Vector3d r = Eigen::Map<Eigen::Vector3d>(X+6*n, 3, 1);   // Position vector

    // // Create relative position vectors between s/c and primaries
    // Eigen::Vector3d r_p1 = r/sr - primPos.row(0).transpose();
    // Eigen::Vector3d r_p2 = r/sr - primPos.row(1).transpose();
    // Eigen::Vector3d r_p3 = r/sr - primPos.row(2).transpose();

    // double d1 = r_p1.norm();
    // double d2 = r_p2.norm();
    // double d3 = r_p3.norm();

    // double k = bcSysData->getK();
    // double mu = bcSysData->getMu();
    // double nu = bcSysData->getNu();

    // // Compute the acceleration vector at this point
    // Eigen::Vector3d A;
    // A.noalias() = -(1/k - mu)*r_p1/pow(d1, 3) - (mu - nu)*r_p2/pow(d2,3) - nu*r_p3/pow(d3, 3);

    // // Parials w.r.t. node position r
    // double dFdq_data[9] = {0};
    // dFdq_data[0] = -(1/k - mu)*(1/pow(d1,3) - 3*pow(r_p1(0),2)/pow(d1,5)) -
    //     (mu-nu)*(1/pow(d2,3) - 3*pow(r_p2(0),2)/pow(d2,5)) - nu*(1/pow(d3,3) - 
    //     3*pow(r_p3(0),2)/pow(d3,5));     //dxdx
    // dFdq_data[1] = (1/k - mu)*3*r_p1(0)*r_p1(1)/pow(d1,5) + 
    //     (mu - nu)*3*r_p2(0)*r_p2(1)/pow(d2,5) +
    //     nu*3*r_p3(0)*r_p3(1)/pow(d3,5);   //dxdy
    // dFdq_data[2] = (1/k - mu)*3*r_p1(0)*r_p1(2)/pow(d1,5) +
    //     (mu - nu)*3*r_p2(0)*r_p2(2)/pow(d2,5) +
    //     nu*3*r_p3(0)*r_p3(2)/pow(d3,5);   //dxdz
    // dFdq_data[3] = dFdq_data[1];    // dydx = dxdy
    // dFdq_data[4] = -(1/k - mu)*(1/pow(d1,3) - 3*pow(r_p1(1),2)/pow(d1,5)) -
    //     (mu-nu)*(1/pow(d2,3) - 3*pow(r_p2(1),2)/pow(d2,5)) - 
    //     nu*(1/pow(d3,3) - 3*pow(r_p3(1),2)/pow(d3,5));   //dydy
    // dFdq_data[5] = (1/k - mu)*3*r_p1(1)*r_p1(2)/pow(d1,5) +
    //     (mu - nu)*3*r_p2(1)*r_p2(2)/pow(d2,5) +
    //     nu*3*r_p3(1)*r_p3(2)/pow(d3,5);   //dydz
    // dFdq_data[6] = dFdq_data[2];    //dzdx = dxdz
    // dFdq_data[7] = dFdq_data[5];    //dzdy = dydz
    // dFdq_data[8] = -(1/k - mu)*(1/pow(d1,3) - 3*pow(r_p1(2),2)/pow(d1,5)) -
    //     (mu-nu)*(1/pow(d2,3) - 3*pow(r_p2(2),2)/pow(d2,5)) - nu*(1/pow(d3,3) - 
    //     3*pow(r_p3(2),2)/pow(d3,5)); //dzdz

    // Matrix3Rd dAdq = Eigen::Map<Matrix3Rd>(dFdq_data, 3, 3);
    // Eigen::Vector3d dFdq;
    // dFdq.noalias() = 2*dAdq*A/(sr*Amax*Amax);
    // // dFdq.noalias() = 2*dAdq*A/sr;

    // // Copy data into the correct vectors/matrices
    // double* dFdq_ptr = dFdq.data();

    // double *FX = &(it->FX[0]);
    // double *DF = &(it->DF[0]);

    // FX[row0] = A.squaredNorm()/(Amax*Amax) - 1;     // Found this one converges MUCH better
    // // FX[row0] = A.squaredNorm() - Amax*Amax;
    
    // std::copy(dFdq_ptr, dFdq_ptr+3, DF + it->totalFree*row0 + 6*n);

    // if(it->varTime){
    //     // Get primary velocities at the specified epoch time
    //     std::vector<double> primVelData = getPrimVel(t0, it->sysData);
    //     Matrix3Rd primVel = Eigen::Map<Matrix3Rd>(&(primVelData[0]), 3, 3);

    //     // Compute partials of state w.r.t. primary positions; dont' compute partials
    //     // for P1 because its velocity is zero in the rotating frame
    //     double dfdr2_data[18] = {0};   double dfdr3_data[18] = {0};

    //     dfdr2_data[9] = -1/pow(d2,3) + 3*pow(r_p2(0),2)/pow(d2,5);        //dxdx2
    //     dfdr2_data[10] = 3*r_p2(0)*r_p2(1)/pow(d2,5);                  //dxdy2
    //     dfdr2_data[11] = 3*r_p2(0)*r_p2(2)/pow(d2,5);                  //dxdz2
    //     dfdr2_data[13] = -1/pow(d2,3) + 3*pow(r_p2(1),2)/pow(d2,5);       //dydy2
    //     dfdr2_data[14] = 3*r_p2(1)*r_p2(2)/pow(d2,5);                  //dydz2
    //     dfdr2_data[17] = -1/pow(d2,3) + 3*pow(r_p2(2),2)/pow(d2,5);       //dzdz2

    //     dfdr2_data[12] = dfdr2_data[10];      // Fill in symmetric matrix
    //     dfdr2_data[15] = dfdr2_data[11];
    //     dfdr2_data[16] = dfdr2_data[14];

    //     dfdr3_data[9] = -1/pow(d3,3) + 3*pow(r_p3(0),2)/pow(d3,5);        //dxdx3
    //     dfdr3_data[10] = 3*r_p3(0)*r_p3(1)/pow(d3,5);                  //dxdy3
    //     dfdr3_data[11] = 3*r_p3(0)*r_p3(2)/pow(d3,5);                  //dxdz3
    //     dfdr3_data[13] = -1/pow(d3,3) + 3*pow(r_p3(1),2)/pow(d3,5);       //dydy3
    //     dfdr3_data[14] = 3*r_p3(1)*r_p3(2)/pow(d3,5);                  //dydz3
    //     dfdr3_data[17] = -1/pow(d3,3) + 3*pow(r_p3(2),2)/pow(d3,5);       //dzdz3

    //     dfdr3_data[12] = dfdr3_data[10];      // Fill in symmetric matrix
    //     dfdr3_data[15] = dfdr3_data[11];
    //     dfdr3_data[16] = dfdr3_data[14];

    //     Matrix3Rd dFdr2 = Eigen::Map<Matrix3Rd>(dfdr2_data+9, 3, 3);
    //     Matrix3Rd dFdr3 = Eigen::Map<Matrix3Rd>(dfdr3_data+9, 3, 3);

    //     // scale matrices by constants
    //     dFdr2 *= -1*(mu - nu)/sr;
    //     dFdr3 *= -1*nu/sr;

    //     Eigen::VectorXd dAdT;
    //     dAdT.noalias() = A.transpose()*dFdr2*primVel.row(1).transpose() + A.transpose()*dFdr3*primVel.row(2).transpose();
    //     dAdT *= 2*sr/(Amax*Amax*it->freeVarScale[3]);
    //     // dAdT *= 2*sr/it->freeVarScale[3];

    //     int epochCol = it->equalArcTime ? 6*it->numNodes+1+n : 7*it->numNodes-1+n;
    //     DF[it->totalFree*row0 + epochCol] = dAdT(0);
    // }

    // // figure out which of the slack variables correspond to this constraint
    // std::vector<int>::iterator slackIx = std::find(it->slackAssignCon.begin(), 
    //     it->slackAssignCon.end(), c);

    // // which column of the DF matrix the slack variable is in
    // int slackCol = it->totalFree - it->numSlack + (slackIx - it->slackAssignCon.begin());

    // // Add squared slack variable from constraint
    // it->FX[row0] += it->X[slackCol]*it->X[slackCol];

    // // Partial with respect to slack variable
    // it->DF[it->totalFree*row0 + slackCol] = 2*it->X[slackCol];
    
    // // printf("Node %d %s Con: Slack Var = %.4e\n", n, con.getTypeStr(), it->X[slackCol]);

    int row0 = it->conRows[c];
    int n = con.getNode();
    double Amax = con.getData()[0];
    double epoch = it->varTime ? it->X[7*it->numNodes-1+n] : it->origNodes.at(n).getExtraParam(1);
    const tpat_sys_data_bcr4bpr *bcSysData = static_cast<const tpat_sys_data_bcr4bpr *> (it->sysData);

    std::vector<double> primPosData = getPrimPos(epoch, it->sysData);

    // Get primary positions at the specified epoch time
    Matrix3Rd primPos = Eigen::Map<Matrix3Rd>(&(primPosData[0]), 3, 3);

    double *X = &(it->X[0]);
    Eigen::Vector3d r = Eigen::Map<Eigen::Vector3d>(X+6*n, 3, 1);   // Position vector

    // Create relative position vectors between s/c and primaries
    Eigen::Vector3d r_p1 = r - primPos.row(0).transpose();
    Eigen::Vector3d r_p2 = r - primPos.row(1).transpose();
    Eigen::Vector3d r_p3 = r - primPos.row(2).transpose();

    double d1 = r_p1.norm();
    double d2 = r_p2.norm();
    double d3 = r_p3.norm();

    double k = bcSysData->getK();
    double mu = bcSysData->getMu();
    double nu = bcSysData->getNu();

    // Compute the acceleration vector at this point
    Eigen::Vector3d A;
    A.noalias() = -(1/k - mu)*r_p1/pow(d1, 3) - (mu - nu)*r_p2/pow(d2,3) - nu*r_p3/pow(d3, 3);

    // Parials w.r.t. node position r
    double dFdq_data[9] = {0};
    dFdq_data[0] = -(1/k - mu)*(1/pow(d1,3) - 3*pow(r_p1(0),2)/pow(d1,5)) -
        (mu-nu)*(1/pow(d2,3) - 3*pow(r_p2(0),2)/pow(d2,5)) - nu*(1/pow(d3,3) - 
        3*pow(r_p3(0),2)/pow(d3,5));     //dxdx
    dFdq_data[1] = (1/k - mu)*3*r_p1(0)*r_p1(1)/pow(d1,5) + 
        (mu - nu)*3*r_p2(0)*r_p2(1)/pow(d2,5) +
        nu*3*r_p3(0)*r_p3(1)/pow(d3,5);   //dxdy
    dFdq_data[2] = (1/k - mu)*3*r_p1(0)*r_p1(2)/pow(d1,5) +
        (mu - nu)*3*r_p2(0)*r_p2(2)/pow(d2,5) +
        nu*3*r_p3(0)*r_p3(2)/pow(d3,5);   //dxdz
    dFdq_data[3] = dFdq_data[1];    // dydx = dxdy
    dFdq_data[4] = -(1/k - mu)*(1/pow(d1,3) - 3*pow(r_p1(1),2)/pow(d1,5)) -
        (mu-nu)*(1/pow(d2,3) - 3*pow(r_p2(1),2)/pow(d2,5)) - 
        nu*(1/pow(d3,3) - 3*pow(r_p3(1),2)/pow(d3,5));   //dydy
    dFdq_data[5] = (1/k - mu)*3*r_p1(1)*r_p1(2)/pow(d1,5) +
        (mu - nu)*3*r_p2(1)*r_p2(2)/pow(d2,5) +
        nu*3*r_p3(1)*r_p3(2)/pow(d3,5);   //dydz
    dFdq_data[6] = dFdq_data[2];    //dzdx = dxdz
    dFdq_data[7] = dFdq_data[5];    //dzdy = dydz
    dFdq_data[8] = -(1/k - mu)*(1/pow(d1,3) - 3*pow(r_p1(2),2)/pow(d1,5)) -
        (mu-nu)*(1/pow(d2,3) - 3*pow(r_p2(2),2)/pow(d2,5)) - nu*(1/pow(d3,3) - 
        3*pow(r_p3(2),2)/pow(d3,5)); //dzdz

    Matrix3Rd dAdq = Eigen::Map<Matrix3Rd>(dFdq_data, 3, 3);
    Eigen::Vector3d dFdq;
    dFdq.noalias() = 2*dAdq*A/(Amax*Amax);
    // dFdq.noalias() = 2*dAdq*A;

    // Get primary velocities at the specified epoch time
    std::vector<double> primVelData = getPrimVel(epoch, it->sysData);
    Matrix3Rd primVel = Eigen::Map<Matrix3Rd>(&(primVelData[0]), 3, 3);

    // Compute partials of state w.r.t. primary positions; dont' compute partials
    // for P1 because its velocity is zero in the rotating frame
    double dfdr2_data[18] = {0};   double dfdr3_data[18] = {0};

    dfdr2_data[9] = -1/pow(d2,3) + 3*pow(r_p2(0),2)/pow(d2,5);        //dxdx2
    dfdr2_data[10] = 3*r_p2(0)*r_p2(1)/pow(d2,5);                  //dxdy2
    dfdr2_data[11] = 3*r_p2(0)*r_p2(2)/pow(d2,5);                  //dxdz2
    dfdr2_data[13] = -1/pow(d2,3) + 3*pow(r_p2(1),2)/pow(d2,5);       //dydy2
    dfdr2_data[14] = 3*r_p2(1)*r_p2(2)/pow(d2,5);                  //dydz2
    dfdr2_data[17] = -1/pow(d2,3) + 3*pow(r_p2(2),2)/pow(d2,5);       //dzdz2

    dfdr2_data[12] = dfdr2_data[10];      // Fill in symmetric matrix
    dfdr2_data[15] = dfdr2_data[11];
    dfdr2_data[16] = dfdr2_data[14];

    dfdr3_data[9] = -1/pow(d3,3) + 3*pow(r_p3(0),2)/pow(d3,5);        //dxdx3
    dfdr3_data[10] = 3*r_p3(0)*r_p3(1)/pow(d3,5);                  //dxdy3
    dfdr3_data[11] = 3*r_p3(0)*r_p3(2)/pow(d3,5);                  //dxdz3
    dfdr3_data[13] = -1/pow(d3,3) + 3*pow(r_p3(1),2)/pow(d3,5);       //dydy3
    dfdr3_data[14] = 3*r_p3(1)*r_p3(2)/pow(d3,5);                  //dydz3
    dfdr3_data[17] = -1/pow(d3,3) + 3*pow(r_p3(2),2)/pow(d3,5);       //dzdz3

    dfdr3_data[12] = dfdr3_data[10];      // Fill in symmetric matrix
    dfdr3_data[15] = dfdr3_data[11];
    dfdr3_data[16] = dfdr3_data[14];

    Matrix3Rd dFdr2 = Eigen::Map<Matrix3Rd>(dfdr2_data+9, 3, 3);
    Matrix3Rd dFdr3 = Eigen::Map<Matrix3Rd>(dfdr3_data+9, 3, 3);

    // scale matrices by constants
    dFdr2 *= -1*(mu - nu);
    dFdr3 *= -1*nu;

    Eigen::VectorXd dAdT;
    dAdT.noalias() = A.transpose()*dFdr2*primVel.row(1).transpose() + A.transpose()*dFdr3*primVel.row(2).transpose();
    dAdT *= 2/(Amax*Amax);
    // dAdT *= 2;

    // Copy data into the correct vectors/matrices
    double* dFdq_ptr = dFdq.data();
    double* dFdT_ptr = dAdT.data();

    double *FX = &(it->FX[0]);
    double *DF = &(it->DF[0]);

    FX[row0] = A.squaredNorm()/(Amax*Amax) - 1;
    // FX[row0] = A.squaredNorm() - Amax*Amax;
    
    std::copy(dFdq_ptr, dFdq_ptr+3, DF + it->totalFree*row0 + 6*n);

    if(it->varTime){
        std::copy(dFdT_ptr, dFdT_ptr+1, DF + it->totalFree*row0 + 7*it->numNodes-1+n);
    }

    // figure out which of the slack variables correspond to this constraint
    std::vector<int>::iterator slackIx = std::find(it->slackAssignCon.begin(), 
        it->slackAssignCon.end(), c);

    // which column of the DF matrix the slack variable is in
    int slackCol = it->totalFree - it->numSlack + (slackIx - it->slackAssignCon.begin());

    // Add squared slack variable from constraint
    it->FX[row0] += it->X[slackCol]*it->X[slackCol];

    // Partial with respect to slack variable
    it->DF[it->totalFree*row0 + slackCol] = 2*it->X[slackCol];
}// End of SP Targeting (Magnitude) ==============================

/**
 *  @brief Compute an initial value for the slack variable for an SP_RANGE inequality constraint 
 *  @details If the constraint is satisified, the slack variable must be set such that the 
 *  constraint function evaluates to zero; that value is computed here. If the constraint is not
 *  satisfied, a small value is returned.
 * 
 *  @param it the iterationData object holding the current data for the corrections process
 *  @param con the constraint being applied
 * 
 *  @return the initial value for the slack variable associated with an SP_RANGE constraint
 */
double tpat_model_bcr4bpr::multShoot_targetSPMag_compSlackVar(const iterationData *it, tpat_constraint con) const{
    int n = con.getNode();
    double Amax = con.getData()[0];
    int epochCol = it->equalArcTime ? 6*it->numNodes+1+n : 7*it->numNodes-1+n;
    double t0 = it->varTime ? it->X[epochCol]/it->freeVarScale[3] : it->origNodes.at(n).getExtraParam(1);

    const tpat_sys_data_bcr4bpr *bcSysData = static_cast<const tpat_sys_data_bcr4bpr *> (it->sysData);
    double sr = it->freeVarScale[0];

    std::vector<double> primPosData = getPrimPos(t0, it->sysData);

    // Get primary positions at the specified epoch time
    Matrix3Rd primPos = Eigen::Map<Matrix3Rd>(&(primPosData[0]), 3, 3);

    double rData[3];
    std::copy(&(it->X[0]), &(it->X[0])+3, rData);
    Eigen::Vector3d r = Eigen::Map<Eigen::Vector3d>(rData, 3, 1);   // Position vector

    // Create relative position vectors between s/c and primaries
    Eigen::Vector3d r_p1 = r/sr - primPos.row(0).transpose();
    Eigen::Vector3d r_p2 = r/sr - primPos.row(1).transpose();
    Eigen::Vector3d r_p3 = r/sr - primPos.row(2).transpose();

    double d1 = r_p1.norm();
    double d2 = r_p2.norm();
    double d3 = r_p3.norm();

    double k = bcSysData->getK();
    double mu = bcSysData->getMu();
    double nu = bcSysData->getNu();

    // Evaluate three constraint function values
    Eigen::Vector3d A;
    A.noalias() = -(1/k - mu)*r_p1/pow(d1, 3) - (mu - nu)*r_p2/pow(d2,3) - nu*r_p3/pow(d3, 3);

    double diff = 1 - A.squaredNorm()/(Amax*Amax);
    // double diff = Amax*Amax - A.squaredNorm();

    /*  If diff is positive, then the constraint
     *  is satisfied, so compute the value of the slack variable that 
     *  sets the constraint function equal to zero. Otherwise, choose 
     *  a small value of the slack variable but don't set it to zero as 
     *  that will make the partials zero and will prevent the mulitple
     *  shooting algorithm from updating the slack variable
     */
    return diff > 0 ? sqrt(diff) : 1e-4;
}//======================================================

/**
 *  @brief Compute an initial value for the slack variable for an SP_DIST inequality constraint 
 *  @details If the constraint is satisified, the slack variable must be set such that the 
 *  constraint function evaluates to zero; that value is computed here. If the constraint is not
 *  satisfied, a small value is returned.
 * 
 *  @param it the iterationData object holding the current data for the corrections process
 *  @param con the constraint being applied
 * 
 *  @return the initial value for the slack variable associated with an SP_DIST constraint
 */
double tpat_model_bcr4bpr::multShoot_targetSP_maxDist_compSlackVar(const iterationData *it, tpat_constraint con) const{
    int n = con.getNode();
    int epochCol = it->equalArcTime ? 6*it->numNodes+1+n : 7*it->numNodes-1+n;
    double T = it->varTime ? it->X[epochCol]/it->freeVarScale[3] : it->origNodes.at(n).getExtraParam(1);

    std::vector<double> coeff = con.getData();
    double sr = it->freeVarScale[0];

    // Compute SP position from polynomial approximation
    Eigen::Vector3d spPos;
    spPos(0) = T*T*coeff[1] + T*coeff[2] + coeff[3];
    spPos(1) = T*T*coeff[4] + T*coeff[5] + coeff[6];
    spPos(2) = T*T*coeff[7] + T*coeff[8] + coeff[9];

    // double *X = &(it->X[0]);
    double rData[3];
    std::copy(&(it->X[0]), &(it->X[0])+3, rData);
    Eigen::Vector3d r = Eigen::Map<Eigen::Vector3d>(rData, 3, 1);   // Position vector

    Eigen::Vector3d dist = r - spPos*sr;
    // double diff = coeff[0]*sr - dist.norm();
    double diff = coeff[0]*coeff[0]*sr*sr - dist.squaredNorm();
    
    return diff > 0 ? sqrt(diff) : 1e-4;
}//===================================================

/**
 *  @brief Compute partials and constraint values for node constrained with <tt>SP_DIST</tt>
 *  and <tt>SP_MAX_DIST</tt>. 
 *  @details One constraint value and one row of partials are computed. This constraint uses
 *  2nd-order polynomials to approximate the saddle point's location as a function of epoch 
 *  and then targets a node to be at or within a set radius of the saddle point.
 * 
 *  @param it the iterationData object holding the current data for the corrections process
 *  @param con the constraint being applied
 *  @param row0 the index of the first row for this constraint
 */
void tpat_model_bcr4bpr::multShoot_targetSP_dist(iterationData *it, tpat_constraint con, int c) const{
    int row0 = it->conRows[c];
    int n = con.getNode();
    int epochCol = it->equalArcTime ? 6*it->numNodes+1+n : 7*it->numNodes-1+n;
    double T = it->varTime ? it->X[epochCol]/it->freeVarScale[3] : it->origNodes.at(n).getExtraParam(1);

    // const tpat_sys_data_bcr4bpr *bcSysData = static_cast<const tpat_sys_data_bcr4bpr *> (it->sysData);
    std::vector<double> coeff = con.getData();
    double sr = it->freeVarScale[0];

    // Compute SP position from polynomial approximation
    Eigen::Vector3d spPos;
    spPos(0) = T*T*coeff[1] + T*coeff[2] + coeff[3];
    spPos(1) = T*T*coeff[4] + T*coeff[5] + coeff[6];
    spPos(2) = T*T*coeff[7] + T*coeff[8] + coeff[9];

    double *X = &(it->X[0]);
    double *DF = &(it->DF[0]);
    double *FX = &(it->FX[0]);
    Eigen::Vector3d r = Eigen::Map<Eigen::Vector3d>(X+6*n, 3, 1);   // Position vector

    Eigen::Vector3d dist = r - spPos*sr;

    FX[row0] = dist.squaredNorm() - coeff[0]*coeff[0]*sr*sr;    // This one is much more robust, so choose this one
    // double d = dist.norm();          
    // FX[row0] = d - coeff[0]*sr;    // This one converges much more quickly but is much less robust

    // Compute partials w.r.t. node states
    DF[it->totalFree*row0 + 6*n+0] = 2*dist(0);
    DF[it->totalFree*row0 + 6*n+1] = 2*dist(1);
    DF[it->totalFree*row0 + 6*n+2] = 2*dist(2);
    // DF[it->totalFree*row0 + 6*n+0] = dist(0)/d;     // dFdx
    // DF[it->totalFree*row0 + 6*n+1] = dist(1)/d;     // dFdy
    // DF[it->totalFree*row0 + 6*n+2] = dist(2)/d;     // dFdz

    // printf("Dist from Approx SP = [%.4f, %.4f, %.4f]\n", dist(0), dist(1), dist(2));
    // printf("  r = [%.4f, %.4f, %.4f]\n", r(0), r(1), r(2));
    // printf("  sp_pos = [%.4f, %.4f, %.4f]\n", spPos(0), spPos(1), spPos(2));

    // Compute partials w.r.t. epoch Time
    if(it->varTime){
        DF[it->totalFree*row0 + epochCol] = -2*dist(0)*(2*coeff[1]*T + coeff[2]) -
            2*dist(1)*(2*coeff[4]*T + coeff[5]) - 2*dist(2)*(2*coeff[7]*T + coeff[8]);
        // DF[it->totalFree*row0 + epochCol] = -1/d*dist(0)*(2*coeff[1]*T + coeff[2]) -
        //     1/d*dist(1)*(2*coeff[4]*T + coeff[5]) - 1/d*dist(2)*(2*coeff[7]*T + coeff[8]);
        DF[it->totalFree*row0 + epochCol] *= sr/it->freeVarScale[3]; // scale derivative
    }

    if(con.getType() == tpat_constraint::SP_MAX_DIST){
        // figure out which of the slack variables correspond to this constraint
        std::vector<int>::iterator slackIx = std::find(it->slackAssignCon.begin(), 
            it->slackAssignCon.end(), c);

        // which column of the DF matrix the slack variable is in
        int slackCol = it->totalFree - it->numSlack + (slackIx - it->slackAssignCon.begin());

        // Add squared slack variable from constraint
        it->FX[row0] += it->X[slackCol]*it->X[slackCol];

        // Partial with respect to slack variable
        it->DF[it->totalFree*row0 + slackCol] = 2*it->X[slackCol];
        
        // printf("Node %d distance to SP = %.4f km\n", n, dist.norm()*bcSysData->getCharL());
        // if(FX[row0] < 0)     // I've tested this method in test_multShootCons and found no difference in convergence speed
        //     FX[row0]= 0;
    }

    // printf("SP_DIST constraint: dist = %.4e = %.4f km, ||F|| = %.4e\n", d, d*bcSysData->getCharL(), it->FX[row0]);

}//===================================================

/**
 *  @brief Take the final, corrected free variable vector <tt>X</tt> and create an output 
 *  nodeset
 *
 *  If <tt>findEvent</tt> is set to true, the
 *  output nodeset will contain extra information for the simulation engine to use. Rather than
 *  returning only the position and velocity states, the output nodeset will contain the STM 
 *  and dqdT values for the final node; this information will be appended to the extraParameter
 *  vector in the final node.
 *
 *  @param it an iteration data object containing all info from the corrections process
 *  @param nodes_in a pointer to the original, uncorrected nodeset
 *  @param findEvent whether or not this correction process is locating an event
 *
 *  @return a pointer to a nodeset containing the corrected nodes
 */
tpat_nodeset* tpat_model_bcr4bpr::multShoot_createOutput(const iterationData *it, const tpat_nodeset *nodes_in, bool findEvent) const{

    // Create a nodeset with the same system data as the input
    const tpat_sys_data_bcr4bpr *bcSys = static_cast<const tpat_sys_data_bcr4bpr *>(it->sysData);
    tpat_nodeset_bcr4bp *nodeset_out = new tpat_nodeset_bcr4bp(bcSys);

    int numNodes = (int)(it->origNodes.size());
    for(int i = 0; i < numNodes; i++){
        double state[6];
        std::copy(&(it->X[i*6]), &(it->X[i*6])+6, state);

        // Reverse scaling
        for(int i = 0; i < 6; i++){
            state[i] /= i < 3 ? it->freeVarScale[0] : it->freeVarScale[1];
        }

        tpat_node node(state, NAN);
        int epochCol = it->equalArcTime ? 6*it->numNodes+1+i : 7*it->numNodes-1+i;
        double T = it->varTime ? it->X[epochCol]/it->freeVarScale[3] : it->origNodes.at(i).getExtraParam(1);
        node.setExtraParam(1, T);     // save epoch time
        
        node.setVelCon(nodes_in->getNode(i).getVelCon());   // save velocity continuity info
        node.setConstraints(nodes_in->getNode(i).getConstraints());    // save constraints

        if(i + 1 < numNodes){
            // Get TOF, reverse variable scaling, save to node
            double tof;
            if(it->varTime){
                // Get data
                tof = it->equalArcTime ? it->X[6*it->numNodes]/(it->numNodes - 1) : it->X[6*it->numNodes+i];
                // Reverse scaling
                tof /= it->freeVarScale[2];    // Time scaling
            }else{
                tof = nodes_in->getTOF(i);
            }
            node.setTOF(tof);
        }else{
            node.setTOF(NAN);
            
            /* To avoid re-integrating in the simulation engine, we will return the entire 42 or 48-length
                state for the last node. We do this by appending the STM elements and dqdT elements to the
                end of the node array. This output nodeset should have two "nodes": the first 6 elements
                are the first node, the final 42 or 48 elements are the second node with STM and dqdT 
                information*/
            if(findEvent){
                // Append the 36 STM elements to the node vector
                tpat_traj lastSeg = it->allSegs.back();
                MatrixXRd lastSTM = lastSeg.getSTM(-1);
                
                // Create a vector of extra parameters from existing extraParam vector
                std::vector<double> extraParams = nodeset_out->getNode(-1).getExtraParams();
                // append the STM elements at the end
                extraParams.insert(extraParams.end(), lastSTM.data(), lastSTM.data()+36);
                // append the last dqdT vector
                std::vector<double> dqdT = lastSeg.getExtraParam(-1,1);
                extraParams.insert(extraParams.end(), dqdT.begin(), dqdT.end());
                node.setExtraParams(extraParams);
            }
        }

        nodeset_out->appendNode(node);
    }

    return nodeset_out;
}//====================================================

//------------------------------------------------------------------------------------------------------
//      Static Calculation Functions
//------------------------------------------------------------------------------------------------------

/**
 *   @brief Integrate the equations of motion for the BCR4BP, rotating coordinates.
 *
 *   @param t epoch at integration step
 *   @param s the 48-d state vector
 *   @param sdot the 48-d state derivative vector
 *   @param params points to an eomParamStruct object
 */
int tpat_model_bcr4bpr::fullEOMs(double t, const double s[], double sdot[], void *params){
    // Dereference the eom data object
    // tpat_sys_data_bcr4bpr *sysData = static_cast<tpat_sys_data_bcr4bpr *>(params);
    eomParamStruct *paramStruct = static_cast<eomParamStruct *>(params);
    const tpat_sys_data_bcr4bpr *sysData = static_cast<const tpat_sys_data_bcr4bpr *>(paramStruct->sysData);

    // Put the positions of the three primaries in a 3x3 matrix
    double primPosData[9] = {0};
    getPrimaryPos(t, sysData, primPosData);
    Matrix3Rd primPos = Eigen::Map<Matrix3Rd>(primPosData, 3, 3);

    // Put the position states into a 3-element column vector
    double r_data[3] = {0};
    std::copy(s, s+3, r_data);
    Eigen::Vector3d r = Eigen::Map<Eigen::Vector3d>(r_data, 3, 1);

    // Put velocity states into a 3-element column vector
    double v_data[3] = {0};
    std::copy(s+3, s+6, v_data);
    Eigen::Vector3d v = Eigen::Map<Eigen::Vector3d>(v_data, 3, 1);

    // Create relative position vectors between s/c and primaries
    Eigen::Vector3d r_p1, r_p2, r_p3;
    r_p1.noalias() = r - primPos.row(0).transpose();
    r_p2.noalias() = r - primPos.row(1).transpose();
    r_p3.noalias() = r - primPos.row(2).transpose();
    double d1 = r_p1.norm();
    double d2 = r_p2.norm();
    double d3 = r_p3.norm();
    
    // Save constants to short variables for readability
    double k = sysData->getK();
    double mu = sysData->getMu();
    double nu = sysData->getNu();

    // Create C-matrix
    double c[] = {0, 2*k, 0, -2*k, 0, 0, 0, 0, 0};
    MatrixXRd C = Eigen::Map<MatrixXRd>(c, 3, 3);

    // truncated position vector used in EOMs
    double r_trunc_data[3] = {0};
    std::copy(s, s+2, r_trunc_data);
    Eigen::Vector3d r_trunc = Eigen::Map<Eigen::Vector3d>(r_trunc_data, 3, 1);

    // Compute acceleration using matrix math
    Eigen::Vector3d accel;
    accel.noalias() = C*v + k*k*r_trunc - (1/k - mu)*r_p1/pow(d1, 3) - (mu - nu)*r_p2/pow(d2, 3) - 
        nu*r_p3/pow(d3, 3);
    accel[0] += k*(1 - mu*k);     // Add extra term for new base point

    // Compute psuedo-potential
    double dxdx = k*k - (1/k - mu)*(1/pow(d1,3) - 3*r_p1(0)*r_p1(0)/pow(d1,5)) -
            (mu-nu)*(1/pow(d2,3) - 3*r_p2(0)*r_p2(0)/pow(d2,5)) - nu*(1/pow(d3,3) -
                3*r_p3(0)*r_p3(0)/pow(d3,5));
    double dxdy = (1/k - mu)*3*r_p1(0)*r_p1(1)/pow(d1,5) +
            (mu - nu)*3*r_p2(0)*r_p2(1)/pow(d2,5) +
            nu*3*r_p3(0)*r_p3(1)/pow(d3,5);
    double dxdz = (1/k - mu)*3*r_p1(0)*r_p1(2)/pow(d1,5) +
            (mu - nu)*3*r_p2(0)*r_p2(2)/pow(d2,5) +
            nu*3*r_p3(0)*r_p3(2)/pow(d3,5);
    double dydy = k*k - (1/k - mu)*(1/pow(d1,3) - 3*r_p1(1)*r_p1(1)/pow(d1,5)) -
            (mu-nu)*(1/pow(d2,3) - 3*r_p2(1)*r_p2(1)/pow(d2,5)) - nu*(1/pow(d3,3) -
            3*r_p3(1)*r_p3(1)/pow(d3,5));
    double dydz = (1/k - mu)*3*r_p1(1)*r_p1(2)/pow(d1,5) +
            (mu - nu)*3*r_p2(1)*r_p2(2)/pow(d2,5) +
            nu*3*r_p3(1)*r_p3(2)/pow(d3,5);
    double dzdz = -(1/k - mu)*(1/pow(d1,3) - 3*r_p1(2)*r_p1(2)/pow(d1,5)) -
            (mu-nu)*(1/pow(d2,3) - 3*r_p2(2)*r_p2(2)/pow(d2,5)) - nu*(1/pow(d3,3) -
            3*r_p3(2)*r_p3(2)/pow(d3,5));

    // Create A matrix for STM derivative
    double aData[] = {  0, 0, 0, 1, 0, 0,
                        0, 0, 0, 0, 1, 0,
                        0, 0, 0, 0, 0, 1, 
                        dxdx, dxdy, dxdz, c[0], c[1], c[2],
                        dxdy, dydy, dydz, c[3], c[4], c[5],
                        dxdz, dydz, dzdz, c[6], c[7], c[8]};
    MatrixXRd A = Eigen::Map<MatrixXRd>(aData, 6, 6);

    // Compute the STM derivative
    double phiData[36];
    std::copy(s+6, s+42, phiData);
    MatrixXRd Phi = Eigen::Map<MatrixXRd>(phiData, 6, 6);

    MatrixXRd PhiDot(6,6);
    PhiDot.noalias() = A*Phi;

    // Compute partials of state w.r.t. primary positions; dont' compute partials
    // for P1 because its velocity is zero in the rotating frame
    double dfdr2[18] = {0};   double dfdr3[18] = {0};

    dfdr2[9] = -1/pow(d2,3) + 3*r_p2(0)*r_p2(0)/pow(d2,5);        //dxdx2
    dfdr2[10] = 3*r_p2(0)*r_p2(1)/pow(d2,5);                  //dxdy2
    dfdr2[11] = 3*r_p2(0)*r_p2(2)/pow(d2,5);                  //dxdz2
    dfdr2[13] = -1/pow(d2,3) + 3*r_p2(1)*r_p2(1)/pow(d2,5);       //dydy2
    dfdr2[14] = 3*r_p2(1)*r_p2(2)/pow(d2,5);                  //dydz2
    dfdr2[17] = -1/pow(d2,3) + 3*r_p2(2)*r_p2(2)/pow(d2,5);       //dzdz2

    dfdr2[12] = dfdr2[10];      // Fill in symmetric matrix
    dfdr2[15] = dfdr2[11];
    dfdr2[16] = dfdr2[14];

    dfdr3[9] = -1/pow(d3,3) + 3*r_p3(0)*r_p3(0)/pow(d3,5);        //dxdx3
    dfdr3[10] = 3*r_p3(0)*r_p3(1)/pow(d3,5);                  //dxdy3
    dfdr3[11] = 3*r_p3(0)*r_p3(2)/pow(d3,5);                  //dxdz3
    dfdr3[13] = -1/pow(d3,3) + 3*r_p3(1)*r_p3(1)/pow(d3,5);       //dydy3
    dfdr3[14] = 3*r_p3(1)*r_p3(2)/pow(d3,5);                  //dydz3
    dfdr3[17] = -1/pow(d3,3) + 3*r_p3(2)*r_p3(2)/pow(d3,5);       //dzdz3

    dfdr3[12] = dfdr3[10];      // Fill in symmetric matrix
    dfdr3[15] = dfdr3[11];
    dfdr3[16] = dfdr3[14];

    MatrixXRd DfDr2 = Eigen::Map<MatrixXRd>(dfdr2, 6, 3);
    MatrixXRd DfDr3 = Eigen::Map<MatrixXRd>(dfdr3, 6, 3);
    
    // Scale by constants
    DfDr2 *= -1*(mu-nu);
    DfDr3 *= -1*nu;
    
    // Pull the state derivative w.r.t. Epoch time from the large state vector; create column vector
    double dqdT_data[6] = {0};
    std::copy(s+42,s+48, dqdT_data);
    Eigen::VectorXd dqdT = Eigen::Map<Eigen::VectorXd>(dqdT_data, 6, 1);

    // Get the velocity of the primaries
    double primVelData[9] = {0};
    getPrimaryVel(t, sysData, primVelData);
    Matrix3Rd primVel = Eigen::Map<Matrix3Rd>(primVelData, 3, 3);

    // Compute derivative of dqdT
    Eigen::VectorXd dot_dqdT;
    dot_dqdT.noalias() = A*dqdT + DfDr2*(primVel.row(1).transpose()) + DfDr3*(primVel.row(2).transpose());

    // Save derivatives to output vector
    double *accelPtr = accel.data();
    double *phiDotPtr = PhiDot.data();
    double *dqdtDotPtr = dot_dqdT.data();

    std::copy(s+3, s+6, sdot);
    std::copy(accelPtr, accelPtr+3, sdot+3);
    std::copy(phiDotPtr, phiDotPtr+36, sdot+6);
    std::copy(dqdtDotPtr, dqdtDotPtr+6, sdot+42);

    return GSL_SUCCESS;
}//============== END OF BCR4BPR EOMs ======================

/**
 *   @brief Integrate the equations of motion for the BCR4BP, rotating coordinates.
 *
 *   @param t epoch at integration step
 *   @param s the 6-d state vector
 *   @param sdot the 6-d state derivative vector
 *   @param params points to an eomParamStruct object
 */
int tpat_model_bcr4bpr::simpleEOMs(double t, const double s[], double sdot[], void *params){
    // Dereference the eom data object
    // tpat_sys_data_bcr4bpr *sysData = static_cast<tpat_sys_data_bcr4bpr *>(params);
    eomParamStruct *paramStruct = static_cast<eomParamStruct *>(params);
    const tpat_sys_data_bcr4bpr *sysData = static_cast<const tpat_sys_data_bcr4bpr *>(paramStruct->sysData);

    // Put the positions of the three primaries in a 3x3 matrix
    double primPosData[9] = {0};
    getPrimaryPos(t, sysData, primPosData);
    Matrix3Rd primPos = Eigen::Map<Matrix3Rd>(primPosData, 3, 3);

    // Put the position states into a 3-element column vector
    double r_data[3] = {0};
    std::copy(s, s+3, r_data);
    Eigen::Vector3d r = Eigen::Map<Eigen::Vector3d>(r_data, 3, 1);

    // Put velocity states into a 3-element column vector
    double v_data[3] = {0};
    std::copy(s+3, s+6, v_data);
    Eigen::Vector3d v = Eigen::Map<Eigen::Vector3d>(v_data, 3, 1);

    // Create relative position vectors between s/c and primaries
    Eigen::Vector3d r_p1, r_p2, r_p3;
    r_p1.noalias() = r - primPos.row(0).transpose();
    r_p2.noalias() = r - primPos.row(1).transpose();
    r_p3.noalias() = r - primPos.row(2).transpose();
    double d1 = r_p1.norm();
    double d2 = r_p2.norm();
    double d3 = r_p3.norm();
    
    // Save constants to short variables for readability
    double k = sysData->getK();
    double mu = sysData->getMu();
    double nu = sysData->getNu();

    // Create C-matrix
    double c[] = {0, 2*k, 0, -2*k, 0, 0, 0, 0, 0};
    MatrixXRd C = Eigen::Map<MatrixXRd>(c, 3, 3);

    // truncated position vector used in EOMs
    double r_trunc_data[3] = {0};
    std::copy(s, s+2, r_trunc_data);
    Eigen::Vector3d r_trunc = Eigen::Map<Eigen::Vector3d>(r_trunc_data, 3, 1);

    // Compute acceleration using matrix math
    Eigen::Vector3d accel;
    accel.noalias() = C*v + k*k*r_trunc - (1/k - mu)*r_p1/pow(d1, 3) - (mu - nu)*r_p2/pow(d2, 3) - 
        nu*r_p3/pow(d3, 3);
    accel[0] += k*(1 - mu*k);     // Add extra term for new base point

    // Save derivatives to output vector
    double *accelPtr = accel.data();

    std::copy(s+3, s+6, sdot);
    std::copy(accelPtr, accelPtr+3, sdot+3);
    return GSL_SUCCESS;
}//============== END OF BCR4BPR EOMs ======================

/**
 *  @brief Compute the location of the three primaries in the BCR4BP (rotating coord.)
 *
 *  @param t non-dimensional time since t0, where t0 coincides with the positions specified by theta0 and phi0
 *  @param sysData a system data object containing information about the BCR4BP primaries
 *  @param primPos a pointer to a 1x9 double array that will hold the positions of the three primaries in 
 *  row-major order. The first three elements are the position of P1, etc.
 */
void tpat_model_bcr4bpr::getPrimaryPos(double t, const tpat_sys_data_bcr4bpr *sysData, double *primPos){
    double k = sysData->getK();
    double mu = sysData->getMu();
    double nu = sysData->getNu();
    double theta0 = sysData->getTheta0();
    double phi0 = sysData->getPhi0();
    double gamma = sysData->getGamma();
    double ratio = sysData->getCharLRatio();

    // Compute the angles for the system at the specified time
    double theta = theta0 + k*t;
    double phi = phi0 + sqrt(mu/pow(ratio, 3)) * t;

    // P1 position
    // primPos[0] = -mu;    // original derivation
    primPos[0] = -1/k;        // new derivation
    primPos[1] = 0;
    primPos[2] = 0;

    // P2 position
    // primPos[3] = 1/k - mu - nu/mu*ratio * (cos(phi)*cos(gamma)*cos(theta) + sin(phi)*sin(theta));
    primPos[3] = -nu/mu*ratio * (cos(phi)*cos(gamma)*cos(theta) + sin(phi)*sin(theta));
    primPos[4] = -nu/mu*ratio * (sin(phi)*cos(theta) - cos(phi)*sin(theta));
    primPos[5] = nu/mu*ratio * cos(phi) * sin(gamma);

    // P3 position
    // primPos[6] = 1/k - mu + (1 - nu/mu)*ratio * (cos(phi)*cos(gamma)*cos(theta) + sin(phi)*sin(theta));
    primPos[6] = (1 - nu/mu)*ratio * (cos(phi)*cos(gamma)*cos(theta) + sin(phi)*sin(theta));
    primPos[7] = (1 - nu/mu)*ratio * (sin(phi)*cos(theta) - cos(phi)*sin(theta));
    primPos[8] = (nu/mu - 1)*ratio * cos(phi)*sin(gamma);
}//================================================

/**
 *  @brief Compute the velocity of the three primaries in the BCR4BP, rotating coordinates.
 *
 *  @param t non-dimensional time since t0, where t0 coincides with the positions specified by theta0 and phi0
 *  @param sysData a system data object containing information about the BCR4BP primaries
 *  @param primVel a pointer to a 3x3 double array that will hold the velocities of the three primaries in
 *  row-major order. The first three elements hold the velocity of P1, etc.
 */
void tpat_model_bcr4bpr::getPrimaryVel(double t, const tpat_sys_data_bcr4bpr *sysData, double *primVel){

    double k = sysData->getK();
    double mu = sysData->getMu();
    double nu = sysData->getNu();
    double theta0 = sysData->getTheta0();
    double phi0 = sysData->getPhi0();
    double gamma = sysData->getGamma();
    double ratio = sysData->getCharLRatio();

    double thetaDot = k;
    double phiDot = sqrt(mu/pow(ratio, 3));

    double theta = theta0 + thetaDot*t;
    double phi = phi0 + phiDot * t;    

    // P1 is stationary in this coordinate system
    primVel[0] = 0;  primVel[1] = 0;  primVel[2] = 0;

    // Angular velocity of P2-P3 line
    double v_P2P3Line[3] = {0};
    v_P2P3Line[0] = thetaDot*(sin(phi)*cos(theta) - cos(phi)*sin(theta)*cos(gamma)) + 
        phiDot*(cos(phi)*sin(theta) - sin(phi)*cos(theta)*cos(gamma));
    v_P2P3Line[1] = (phiDot - thetaDot)*cos(phi - theta);
    v_P2P3Line[2] = phiDot*sin(phi)*sin(gamma);

    // Multiply by radii of P2 and P3 to get their velocities
    primVel[3] = v_P2P3Line[0] * (-nu/mu)*ratio;
    primVel[4] = v_P2P3Line[1] * (-nu/mu)*ratio;
    primVel[5] = v_P2P3Line[2] * (-nu/mu)*ratio;

    primVel[6] = v_P2P3Line[0] * (1-nu/mu)*ratio;
    primVel[7] = v_P2P3Line[1] * (1-nu/mu)*ratio;
    primVel[8] = v_P2P3Line[2] * (1-nu/mu)*ratio;
}//===================================================================

/**
 *  @brief Compute the acceleration of the primary bodies in the BCR4BP, rotating coordinates
 * 
 *  @param t non-dimensional time since t0, where t0 coincides with the positions specified by theta0 and phi0
 *  @param sysData a system data object containing information about the BCR4BP primaries
 *  @param primAccel a pointer to a 3x3 double array that will hold the accelerations of the three primaries in
 *  row-major order. The first three elements hold the acceleration of P1, etc.
 */
void tpat_model_bcr4bpr::getPrimaryAccel(double t, const tpat_sys_data_bcr4bpr *sysData, double *primAccel){
    double k = sysData->getK();
    double mu = sysData->getMu();
    double nu = sysData->getNu();
    double theta0 = sysData->getTheta0();
    double phi0 = sysData->getPhi0();
    double gamma = sysData->getGamma();
    double ratio = sysData->getCharLRatio();

    double thetaDot = k;
    double phiDot = sqrt(mu/pow(ratio, 3));

    double theta = theta0 + thetaDot*t;
    double phi = phi0 + phiDot * t;    

    // P1 is stationary in this coordinate system
    primAccel[0] = 0;  primAccel[1] = 0;  primAccel[2] = 0;

    // Acceleration of P2-P3 line
    double a_P2P3Line[3] = {0};
    a_P2P3Line[0] = (-thetaDot*thetaDot - phiDot*phiDot)*(cos(theta)*cos(phi)*cos(gamma) + sin(theta)*sin(phi)) + 
        2*thetaDot*phiDot*(cos(theta)*cos(phi) + sin(theta)*sin(phi)*cos(gamma));
    a_P2P3Line[1] = -1*(phiDot - thetaDot)*(phiDot - thetaDot)*sin(phi - theta);
    a_P2P3Line[2] = phiDot*phiDot*cos(phi)*sin(gamma);

    // Multiply by radii of P2 and P3 to get their velocities
    primAccel[3] = a_P2P3Line[0] * (-nu/mu)*ratio;
    primAccel[4] = a_P2P3Line[1] * (-nu/mu)*ratio;
    primAccel[5] = a_P2P3Line[2] * (-nu/mu)*ratio;

    primAccel[6] = a_P2P3Line[0] * (1-nu/mu)*ratio;
    primAccel[7] = a_P2P3Line[1] * (1-nu/mu)*ratio;
    primAccel[8] = a_P2P3Line[2] * (1-nu/mu)*ratio;
}//=====================================================================

/**
 *  @brief Orient a BCR4BPR system so that T = 0 corresponds to the specified epoch time
 *  
 *  @param et epoch time (seconds, J2000, UTC)
 *  @param sysData pointer to system data object; A new theta0 and phi0 will be stored
 *  in this data object
 */
void tpat_model_bcr4bpr::orientAtEpoch(double et, tpat_sys_data_bcr4bpr *sysData){
    // Both theta and phi are approximately equal to zero at REF_EPOCH
    double time_nonDim = (et - tpat_sys_data_bcr4bpr::REF_EPOCH)/sysData->getCharT();
    
    // Compute theta and phi
    double theta = sysData->getK()*time_nonDim;
    double phi = sqrt(sysData->getMu()/pow(sysData->getCharLRatio(), 3))*time_nonDim;

    // Adjust theta and phi to be between 0 and 2*PI
    theta -= floor(theta/(2*PI))*2*PI;
    phi -= floor(phi/(2*PI))*2*PI;

    sysData->setTheta0(theta);
    sysData->setPhi0(phi);
    sysData->setEpoch0(et);
}//===========================================