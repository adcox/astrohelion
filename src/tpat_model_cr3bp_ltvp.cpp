/**
 *  @file tpat_model_cr3bp_ltvp.cpp
 *
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

#include "tpat_model_cr3bp_ltvp.hpp"

#include "tpat_calculations.hpp"
#include "tpat_correction_engine.hpp"
#include "tpat_nodeset_cr3bp.hpp"
#include "tpat_sys_data_cr3bp_ltvp.hpp"
#include "tpat_traj_cr3bp_ltvp.hpp"
#include "tpat_event.hpp"
#include "tpat_matrix.hpp"
#include "tpat_utilities.hpp"

tpat_model_cr3bp_ltvp::tpat_model_cr3bp_ltvp() : tpat_model(MODEL_CR3BP_LTVP) {
    coreStates = 6;
    stmStates = 36;
    extraStates = 0;
}//==============================================

tpat_model_cr3bp_ltvp::tpat_model_cr3bp_ltvp(const tpat_model_cr3bp_ltvp &m) : tpat_model(m) {}

tpat_model_cr3bp_ltvp& tpat_model_cr3bp_ltvp::operator =(const tpat_model_cr3bp_ltvp &m){
	tpat_model::operator =(m);
	return *this;
}//==============================================

tpat_model::eom_fcn tpat_model_cr3bp_ltvp::getSimpleEOM_fcn(){
	return &cr3bp_ltvp_simple_EOMs;
}//==============================================

tpat_model::eom_fcn tpat_model_cr3bp_ltvp::getFullEOM_fcn(){
	return &cr3bp_ltvp_EOMs;
}//==============================================

std::vector<double> tpat_model_cr3bp_ltvp::getPrimPos(double t, tpat_sys_data *sysData){
    double primPos[6] = {0};
    tpat_sys_data_cr3bp_ltvp crSys(*static_cast<tpat_sys_data_cr3bp_ltvp *>(sysData));

    primPos[0] = -1*crSys.getMu();
    primPos[3] = 1 - crSys.getMu();

    return std::vector<double>(primPos, primPos+6);
}//==============================================

std::vector<double> tpat_model_cr3bp_ltvp::getPrimVel(double t, tpat_sys_data *sysData){
    double primVel[6] = {0};
    
    return std::vector<double>(primVel, primVel+6);
}//==============================================

void tpat_model_cr3bp_ltvp::saveIntegratedData(double* y, double t, tpat_traj* traj){
	// Save the position and velocity states
    for(int i = 0; i < 6; i++){
        traj->getState()->push_back(y[i]);
    }

    // Save time
    traj->getTime()->push_back(t);

    // Save STM
    double stmElm[36];
    std::copy(y+6, y+42, stmElm);
    traj->getSTM()->push_back(tpat_matrix(6,6,stmElm));

    // Cast trajectory to a cr3bp_traj and then store a value for Jacobi Constant
    tpat_traj_cr3bp_ltvp *cr3bpTraj = static_cast<tpat_traj_cr3bp_ltvp*>(traj);
    tpat_sys_data_cr3bp_ltvp sysData = cr3bpTraj->getSysData();

    // Compute acceleration
    double dsdt[6] = {0};
    cr3bp_ltvp_simple_EOMs(0, y, dsdt, &sysData);

    // Save the accelerations
    traj->getAccel()->push_back(dsdt[3]);
    traj->getAccel()->push_back(dsdt[4]);
    traj->getAccel()->push_back(dsdt[5]);

    // Save Jacobi for CR3BP - it won't be constant any more, but is definitely useful to have
    cr3bpTraj->getJacobi()->push_back(cr3bp_getJacobi(y, sysData.getMu()));
}//=====================================================

bool tpat_model_cr3bp_ltvp::locateEvent(tpat_event event, tpat_traj* traj, tpat_model* model,
    double *ic, double t0, double tof, bool verbose){

    // // Copy system data object
    // tpat_traj_cr3bp_ltvp *crTraj = static_cast<tpat_traj_cr3bp_ltvp *>(traj);

    // // Create a nodeset for this particular type of system
    // printVerb(verbose, "  Creating nodeset for event location\n");
    // tpat_cr3bp_ltvp_nodeset eventNodeset(ic, crTraj->getSysData(), tof, 2, tpat_nodeset::DISTRO_TIME);

    // // Constraint to keep first node unchanged
    // tpat_constraint fixFirstCon(tpat_constraint::STATE, 0, ic, 6);

    // // Constraint to enforce event
    // tpat_constraint eventCon(event.getConType(), event.getConNode(), event.getConData());

    // eventNodeset.addConstraint(fixFirstCon);
    // eventNodeset.addConstraint(eventCon);

    // if(verbose){ eventNodeset.print(); }

    // printVerb(verbose, "  Applying corrections process to locate event\n");
    // tpat_correction_engine corrector;
    // corrector.setVarTime(true);
    // corrector.setTol(traj->getTol());
    // corrector.setVerbose(verbose);
    // corrector.setFindEvent(true);   // apply special settings to minimize computations
    // try{
    //     corrector.correct_cr3bp(&eventNodeset);
    // }catch(tpat_diverge &e){
    //     printErr("Unable to locate event; corrector diverged\n");
    //     return false;
    // }catch(tpat_linalg_err &e){
    //     printErr("LinAlg Err while locating event; bug in corrector!\n");
    //     return false;
    // }

    // // Because we set findEvent to true, this output nodeset should contain
    // // the full (42 or 48 element) final state
    // tpat_cr3bp_ltvp_nodeset correctedNodes = corrector.getCR3BP_LTVP_Output();
    // std::vector<double> *nodes = correctedNodes.getNodes();

    // // event time is the TOF of corrected path + time at the state we integrated from
    // double eventTime = correctedNodes.getTOF(0) + t0;

    // // Use the data stored in nodes and save the state and time of the event occurence
    // model->saveIntegratedData(&(nodes->at(6)), eventTime, traj);

    return true;
}//=======================================================

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
 *  @param nodeset_out a pointer to the output nodeset object
 *  @param findEvent whether or not this correction process is locating an event
 */
tpat_nodeset* tpat_model_cr3bp_ltvp::corrector_createOutput(iterationData *it, bool findEvent){

    // Create a nodeset with the same system data as the input
    // tpat_sys_data_bcr4bpr *bcSys = static_cast<tpat_sys_data_bcr4bpr *>(it->sysData);
    // tpat_nodeset_bcr4bpr *nodeset_out = new tpat_nodeset_bcr4bpr(*bcSys);
    // nodeset_out->setNodeDistro(tpat_nodeset::DISTRO_NONE);

    // int numNodes = (int)(it->origNodes.size());
    // for(int i = 0; i < numNodes; i++){
    //     if(i + 1 < numNodes){
    //         tpat_node node(&(it->X[i*6]), it->X[numNodes*6 + i]);   // create node with state and TOF
    //         node.setExtraParam(0, it->X[7*numNodes-1 + i]);     // save epoch time
    //         nodeset_out->appendNode(node);
    //     }else{
    //         tpat_node node(&(it->X[i*6]), NAN);                 // create node with state and fake TOF (last node)
    //         node.setExtraParam(0, it->X[7*numNodes-1 + i]);     // save epoch time
            
    //          To avoid re-integrating in the simulation engine, we will return the entire 42 or 48-length
    //             state for the last node. We do this by appending the STM elements and dqdT elements to the
    //             end of the node array. This output nodeset should have two "nodes": the first 6 elements
    //             are the first node, the final 42 or 48 elements are the second node with STM and dqdT 
    //             information
    //         if(findEvent){
    //             // Append the 36 STM elements to the node vector
    //             tpat_traj lastSeg = it->allSegs.back();
    //             tpat_matrix lastSTM = lastSeg.getSTM(-1);
                
    //             // Create a vector of extra parameters from existing extraParam vector
    //             std::vector<double> extraParams = nodeset_out->getNode(-1).getExtraParams();
    //             // append the STM elements at the end
    //             extraParams.insert(extraParams.end(), lastSTM.getDataPtr(), lastSTM.getDataPtr()+36);
    //             // append the last dqdT vector
    //             std::vector<double> dqdT = lastSeg.getExtraParam(-1);
    //             extraParams.insert(extraParams.end(), dqdT.begin(), dqdT.end());
    //             node.setExtraParams(extraParams);
    //         }
    //         nodeset_out->appendNode(node);
    //     }
    // }

    return new tpat_nodeset_cr3bp;
}//====================================================