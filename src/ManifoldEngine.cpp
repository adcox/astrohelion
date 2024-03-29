/**
 *  @file ManifoldEngine.cpp
 *  @brief 
 *  
 *  @author Andrew Cox
 *  @version May 1, 2017
 *  @copyright GNU GPL v3.0
 */
/*
 *  Astrohelion 
 *  Copyright 2015-2018, Andrew Cox; Protected under the GNU GPL v3.0
 *  
 *  This file is part of Astrohelion
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
#include "ManifoldEngine.hpp"

#include "assert.h"
#include <Eigen/Dense>
#include <iostream>

#include "Arcset_cr3bp.hpp"
#include "Arcset_cr3bp_lt.hpp"
#include "Calculations.hpp"
#include "ControlLaw_cr3bp_lt.hpp"
#include "Exceptions.hpp"
#include "SimEngine.hpp"
#include "SysData_cr3bp.hpp"
#include "SysData_cr3bp_lt.hpp"
#include "Utilities.hpp"

namespace astrohelion{

//-----------------------------------------------------
//      *structors
//-----------------------------------------------------
/**
 *  @brief Default constructor
 */
ManifoldEngine::ManifoldEngine(){}

/**
 *  @brief Copy constructor
 *  @param m Reference to another ManifoldEngine object
 */
ManifoldEngine::ManifoldEngine(const ManifoldEngine &m){
	copyMe(m);
}//====================================================

//-----------------------------------------------------
//      Set and Get Functions
//-----------------------------------------------------

/**
 *  @brief Retreive the step-off distance, in kilometers
 *  @details The details of the step-off type are dictated by
 *  the Manifold_StepOff_tp passed into the manifold generation
 *  algorithm
 * 
 *  @return The step-off distance, kilometers
 */
double ManifoldEngine::getStepOffDist() const{ return stepOffDist; }

/**
 *  @brief Set the step-off distance, in kilometers
 *  @details The details of the step-off type are dictated by
 *  the Manifold_StepOff_tp passed into the manifold generation
 *  algorithm
 * 
 *  @param dist The step-off distance, kilometers
 */
void ManifoldEngine::setStepOffDist(double dist){ stepOffDist = dist; }

//-----------------------------------------------------
//      Manifold Generation Algorithms
//-----------------------------------------------------

/**
 *  @brief Compute manifold arcs from a number of points spaced equally around a
 *  low-thrust periodic orbit.
 *  @details Make sure that the STMs along the orbit represent the cumulative 
 *  evolution rather than the segment-wise evolution. This characteristic can be 
 *  guaranteed by calling setSTMs_sequence() on the Arcset in question.
 *  
 *  @param manifoldType The type of manifolds to generate
 *  @param pPerOrbit A periodic, CR3BP orbit. No checks are made to ensure 
 *  periodicity, so this function also performs well for nearly periodic 
 *  segments of quasi-periodic arcs. If an arc that is not approximately 
 *  periodic is input, the behavior may be... strange.
 *  @param numMans The number of manifolds to generate
 *  @param tof Time-of-flight along each manifold arc after stepping off the 
 *  specified orbit. If tof is set to zero, none of the trajectories will be 
 *  integrated; each will contain a single node with the initial condition.
 *  @param stepType Describes the method leveraged to step off of the fixed 
 *  point along the stable or unstable eigenvector
 *  
 *  @return a vector of trajectory objects, one for each manifold arc.
 *  @throws Exception if the eigenvalues cannot be computed, or if only one
 *  stable or unstable eigenvalue is computed (likely because of an impropper 
 *  monodromy matrix)
 *  
 *  @see computeSingleFromPeriodic()
 *  @see manifoldsFromPOPoint()
 */
std::vector<Arcset_cr3bp_lt> ManifoldEngine::computeSetFromLTPeriodic(
    Manifold_tp manifoldType, const Arcset_cr3bp_lt *pPerOrbit,
    ControlLaw_cr3bp_lt *pLaw, unsigned int numMans, double tof, 
    Manifold_StepOff_tp stepType){

    std::vector<Arcset_cr3bp_lt> allManifolds;

    if(numMans == 0)
        return allManifolds;

    // Determine how many propagations will be made per manifold
    unsigned int propPerMan = 0;
    switch(std::abs(to_underlying(manifoldType))){
        case 0: propPerMan = 4; break;      // All stable and unstable manifolds
        case 1: propPerMan = 2; break;      // All stable or all unstable manifolds
        default: propPerMan = 1; break;     // One stable or one unstable manifold
    }

    // Pre-allocate space for the computed trajectories
    allManifolds.reserve(numMans*propPerMan);

    // Get a bunch of points to use as starting guesses for the manifolds
    if(numMans > pPerOrbit->getNumNodes()){
        if(verbosity >= Verbosity_tp::SOME_MSG)
            astrohelion::printWarn("ManifoldEngine::computeSetFromLTPeriodic: "
                "Requested too many manifolds... will return fewer\n");
        numMans = pPerOrbit->getNumNodes();
    }

    double stepSize = (static_cast<double>(pPerOrbit->getNumSegs()))/
        (static_cast<double>(numMans));

    std::vector<int> pointIx(numMans, 0);
    for(unsigned int i = 0; i < numMans; i++){
        pointIx[i] = floor(i*stepSize+0.5);
    }

    // Get the eigenvalues and eigenvectors
    std::vector<cdouble> eigVals;
    MatrixXRd eigVecs = eigVecValFromPeriodic(manifoldType, pPerOrbit, &eigVals);

    // Loop through each point from which to generate manifolds
    for(unsigned int m = 0; m < numMans; m++){
        // Get the state on the periodic orbit to step away from
        std::vector<double> state = pPerOrbit->getStateByIx(pointIx[m]);
        std::vector<double> ctrl0 {};
        try{
            ctrl0 = pPerOrbit->getExtraParamVecByIx(pointIx[m], PARAMKEY_CTRL);
        }catch(const Exception &e){}

        MatrixXRd STM = pointIx[m] == 0 ? MatrixXRd::Identity(6,6) : 
            pPerOrbit->getSTMByIx(pointIx[m] - 1);

        if(STM.rows() > 6 && STM.cols() > 6)
            STM = STM.block<6,6>(0,0);

        // Compute manifolds from this point and add them to the big list
        std::vector<Arcset_cr3bp_lt> subset = manifoldsFromLTPOPoint(manifoldType, 
            state, ctrl0, STM, eigVals, eigVecs, tof,
            static_cast<const SysData_cr3bp_lt *>(pPerOrbit->getSysData()), 
            pLaw, stepType);
        allManifolds.insert(allManifolds.end(), subset.begin(), subset.end());
    }// End of point on periodic orbit loop

    return allManifolds;
}//====================================================

/**
 *  @brief Compute manifold arcs from a number of points spaced equally around a
 *  periodic orbit.
 *  @details Make sure that the STMs along the orbit represent the cumulative evolution
 *  rather than the segment-wise evolution. This characteristic can be guaranteed by calling
 *  setSTMs_sequence() on the Arcset in question.
 * 	
 *  @param manifoldType The type of manifolds to generate
 *  @param pPerOrbit A periodic, CR3BP orbit. No checks are made to ensure periodicity,
 *  so this function also performs well for nearly periodic segments of quasi-periodic
 *  arcs. If an arc that is not approximately periodic is input, the behavior may be... strange.
 *  @param numMans The number of manifolds to generate
 *  @param tof Time-of-flight along each manifold arc after stepping off the specified orbit. If
 *  tof is set to zero, none of the trajectories will be integrated; each will contain a single
 *  node with the initial condition.
 *  @param stepType Describes the method leveraged to step off of the fixed point along the 
 *  stable or unstable eigenvector
 *  
 *  @return a vector of trajectory objects, one for each manifold arc.
 *  @throws Exception if the eigenvalues cannot be computed, or if only one
 *  stable or unstable eigenvalue is computed (likely because of an impropper monodromy matrix)
 *  @see computeSingleFromPeriodic()
 *  @see manifoldsFromPOPoint()
 */
std::vector<Arcset_cr3bp> ManifoldEngine::computeSetFromPeriodic(
    Manifold_tp manifoldType, const Arcset_cr3bp *pPerOrbit, 
    unsigned int numMans, double tof, Manifold_StepOff_tp stepType){

	std::vector<Arcset_cr3bp> allManifolds;

	if(numMans == 0)
		return allManifolds;

	// Determine how many propagations will be made per manifold
	unsigned int propPerMan = 0;
	switch(std::abs(to_underlying(manifoldType))){
		case 0: propPerMan = 4; break;		// All stable and unstable manifolds
		case 1: propPerMan = 2; break;		// All stable or all unstable manifolds
		default: propPerMan = 1; break;		// One stable or one unstable manifold
	}

	// Pre-allocate space for the computed trajectories
	allManifolds.reserve(numMans*propPerMan);

    // Get a bunch of points to use as starting guesses for the manifolds
    if(numMans > pPerOrbit->getNumNodes()){
    	if(verbosity >= Verbosity_tp::SOME_MSG)
        	astrohelion::printWarn("ManifoldEngine::computeEigVecValFromPeriodic:"
                " Requested too many manifolds... will return fewer\n");
        numMans = pPerOrbit->getNumNodes();
    }

    double stepSize = (static_cast<double>(pPerOrbit->getNumSegs()))/\
        (static_cast<double>(numMans));
    std::vector<int> pointIx(numMans, 0);
    for(unsigned int i = 0; i < numMans; i++){
        pointIx[i] = floor(i*stepSize+0.5);
    }

    // Get the eigenvalues and eigenvectors
	std::vector<cdouble> eigVals;
	MatrixXRd eigVecs = eigVecValFromPeriodic(manifoldType, pPerOrbit, &eigVals);

	// Loop through each point from which to generate manifolds
    for(unsigned int m = 0; m < numMans; m++){
    	// Get the state on the periodic orbit to step away from
		std::vector<double> state = pPerOrbit->getStateByIx(pointIx[m]);

    	MatrixXRd STM = pointIx[m] == 0 ? MatrixXRd::Identity(6,6) :\
            pPerOrbit->getSTMByIx(pointIx[m] - 1);

    	// Compute manifolds from this point and add them to the big list
    	std::vector<Arcset_cr3bp> subset = manifoldsFromPOPoint(manifoldType, 
            state, STM, eigVals, eigVecs, tof,
    		static_cast<const SysData_cr3bp *>(pPerOrbit->getSysData()), stepType);
    	allManifolds.insert(allManifolds.end(), subset.begin(), subset.end());
    }// End of point on periodic orbit loop

    return allManifolds;
}//====================================================

/**
 *  @brief Compute manifold arc(s) from a single point on a periodic orbit
 * 
 *  @param manifoldType Type of manifold
 *  @param pPerOrbit Pointer to a periodic orbit; only include one revolution of 
 *  the orbit in the rotating frame.
 *  @param pPerOrbit pointer to the periodic orbit arcset object
 *  @param orbitTOF Specifies the point on the periodic orbit that the manifold 
 *  should emenate from.
 *  If the TOF is negative or greater than the period of the periodic orbit, the 
 *  TOF is adjusted so that the initial state is propagated for no more than one 
 *  period of the periodic orbit to avoid numerical errors.
 *  @param manifoldTOF Amount of time to propagate the manifold arc from the 
 *  initial state on the periodic orbit.
 *  @param stepType Describes how the initial step along the eigenvector is 
 *  computed
 *  
 *  @return A manifold arc
 *  @see manifoldsFromPOPoint()
 */
std::vector<Arcset_cr3bp> ManifoldEngine::computeSingleFromPeriodic(
    Manifold_tp manifoldType, const Arcset_cr3bp *pPerOrbit,
    double orbitTOF, double manifoldTOF, Manifold_StepOff_tp stepType){

	// Change orbitTOF to be between 0 and the periodic orbit period
	double period = pPerOrbit->getTotalTOF();
	while(orbitTOF < 0)
		orbitTOF += period;

	while(orbitTOF > period)
		orbitTOF -= period;

	// Get the eigenvalues and eigenvectors
	std::vector<cdouble> eigVals;
	MatrixXRd eigVecs = eigVecValFromPeriodic(manifoldType, pPerOrbit, &eigVals);

    // Get the state and STM from the initial periodic orbit state to the 
    // desired state at orbitTOF
	SimEngine sim;
    sim.setVerbosity(static_cast<Verbosity_tp>(to_underlying(verbosity) - 1));
	sim.setVarStepSize(false);	// Do simple propagation; only need final state
	sim.setNumSteps(2);

	const SysData_cr3bp *sys = static_cast<const SysData_cr3bp *>(pPerOrbit->getSysData());
	Arcset_cr3bp arc(sys);
	sim.runSim(pPerOrbit->getStateByIx(0), orbitTOF, &arc);

	return manifoldsFromPOPoint(manifoldType, arc.getStateByIx(-1), 
        arc.getSTMByIx(-1), eigVals, eigVecs, manifoldTOF, sys, stepType);
}//====================================================

/**
 *  @brief Compute manifolds arcs from a point on a periodic orbit (PO)
 * 
 *  @param manifoldType Describes the type of manifold(s) to be computed
 *  @param state The state on the periodic orbit to compute manifold arcs from
 *  @param STM State Transition Matrix from time t0 to time t1; t1 corresponds 
 *  with <code>state</code>; Note that this is NOT the monodromy matrix (in general).
 *  @param eigVals Eigenvalues of the monodromy matrix, Phi(t0+T, t0), where T 
 *  is the period of the periodic orbit
 *  @param eigVecs Eigenvectors of the monodromy matrix that correspond to the 
 *  eigenvalues in <code>eigVals</code>
 *  @param tof Amount of time (nondimensional) for which to propagate the 
 *  manifold arc; sign is unimportant. If tof is set to zero, none of the 
 *  trajectories will be integrated; each will contain a single node with the 
 *  initial condition.
 *  @param pSys Pointer to a SysData object consistent with the periodic orbit
 *  @param stepType Describes how the step is taken away from <code>state</code> 
 *  along the eigenvector. By default, <code>stepType</code> is set to
 *  <code>STEP_VEC_NORMFULL</code>
 *  
 *  @return The computed manifold arc(s)
 */
std::vector<Arcset_cr3bp> ManifoldEngine::manifoldsFromPOPoint(Manifold_tp manifoldType, 
    std::vector<double> state, MatrixXRd STM, std::vector<cdouble> eigVals, 
    MatrixXRd eigVecs, double tof, const SysData_cr3bp *pSys, 
    Manifold_StepOff_tp stepType){

	assert(pSys->getDynamicsModel()->getCoreStateSize() == 6);

	std::vector<Arcset_cr3bp> manifolds;
	SimEngine sim;
    sim.setVerbosity(static_cast<Verbosity_tp>(to_underlying(verbosity) - 1));
	double mu = pSys->getMu();
	Eigen::VectorXd q0 = Eigen::Map<Eigen::VectorXd>(&(state[0]), 6, 1);
	double C = DynamicsModel_cr3bp::getJacobi(&(state[0]), mu);

	// Loop through each stability type
	for(unsigned int s = 0; s < eigVals.size(); s++){
		// Transform the eigenvectors to this updated time
    	MatrixXRd newVec = STM*eigVecs.col(s);

    	// Factor with which to normalize the eigenvector
        double mag = (stepType == Manifold_StepOff_tp::STEP_VEC_NORMFULL) ?
        	newVec.norm() : sqrt(newVec(0)*newVec(0) + newVec(1)*newVec(1) + newVec(2)*newVec(2));

        // Use reverse time for stable manifolds
    	sim.setRevTime(std::abs(eigVals[s]) < 1);

    	// Scale the vector and make sure it is pointing in the +x direction
        Eigen::VectorXd baseDirection = newVec/mag;
        if(astrohelion::sign(baseDirection(0)) < 0)
    	   baseDirection *= -1;

    	// Determine which directions the user has specified with manifoldType
    	std::vector<int> dirs;
    	if(std::abs(to_underlying(manifoldType)) <= 1){
    		// Both directions are desired
    		dirs.push_back(1);
    		dirs.push_back(-1);
    	}else{
    		// Only one direction is desired
    		if(std::abs(to_underlying(manifoldType)) == 2){ dirs.push_back(1); }
    		if(std::abs(to_underlying(manifoldType)) == 3){ dirs.push_back(-1); }
    	}

        Arcset_cr3bp traj(static_cast<const SysData_cr3bp *>(pSys));
    	for(unsigned int di = 0; di < dirs.size(); di++){
    		// Flip to (+) or (-) direction according to manifoldType
    		// Step away from the point on the arc in the direction of the eigenvector
    		Eigen::VectorXd ic = q0 + stepOffDist/pSys->getCharL() * dirs[di]*baseDirection;

    		// Scale the velocity part of ic to match Jacobi if that setting is enabled
	        if(stepType == Manifold_StepOff_tp::STEP_MATCH_JC){
		        Eigen::Vector3d v = ic.block<3,1>(3,0);
		        v /= v.norm();  // make it a unit vector

		        // Compute the pseudopotential 
		        double r13 = sqrt((ic[0] + mu)*(ic[0] + mu) + ic[1]*ic[1] + ic[2]*ic[2]);
		        double r23 = sqrt((ic[0] - 1 + mu)*(ic[0] - 1 + mu) + ic[1]*ic[1] + ic[2]*ic[2]);
		        double U = (1 - mu)/r13 + mu/r23 + 0.5*(ic[0]*ic[0] + ic[1]*ic[1]);

		        // Rescale the velocity vector so that the magnitude yields the exact Jacobi Constant
		        v *= sqrt(2*U - C);
		        ic.block<3,1>(3,0) = v; // Reassign the velocity components of ic
		    }

		    // Simulate for some time to generate a manifold arc
	        traj.reset();
	        if(std::abs(tof) > 1e-6){
	            sim.runSim(ic.data(), tof, &traj);
	        }else{
	            traj.addNode(Node(ic.data(), 6, 0));
	        }
	        manifolds.push_back(traj);
    	} // End of +/- direction loop
	}// End of Stable/Unstable eigenvector loop

	return manifolds;
}//====================================================

/**
 *  @brief Compute manifolds arcs from a point on a low-thrust periodic orbit 
 *  (LTPO)
 * 
 *  @param manifoldType Describes the type of manifold(s) to be computed
 *  @param state The state on the periodic orbit to compute manifold arcs from
 *  @param STM State Transition Matrix from time t0 to time t1; t1 corresponds 
 *  with <code>state</code>; Note that this is NOT the monodromy matrix 
 *  (in general).
 *  @param eigVals Eigenvalues of the monodromy matrix, Phi(t0+T, t0), where T 
 *  is the period of the periodic orbit
 *  @param eigVecs Eigenvectors of the monodromy matrix that correspond to the 
 *  eigenvalues in <code>eigVals</code>
 *  @param tof Amount of time (nondimensional) for which to propagate the 
 *  manifold arc; sign is unimportant. If tof is set to zero, none of the 
 *  trajectories will be integrated; each will contain a single node with the 
 *  initial condition.
 *  @param pSys Pointer to a SysData object consistent with the periodic orbit
 *  @param stepType Describes how the step is taken away from <code>state</code> 
 *  along the eigenvector
 *  
 *  @return The computed manifold arc(s)
 */
std::vector<Arcset_cr3bp_lt> ManifoldEngine::manifoldsFromLTPOPoint(
    Manifold_tp manifoldType, std::vector<double> state, 
    std::vector<double> ctrl0, MatrixXRd STM,
    std::vector<cdouble> eigVals, MatrixXRd eigVecs, double tof, 
    const SysData_cr3bp_lt *pSys, ControlLaw_cr3bp_lt *pLaw,
    Manifold_StepOff_tp stepType){

    assert(pSys->getDynamicsModel()->getCoreStateSize() == 7);

    std::vector<Arcset_cr3bp_lt> manifolds;
    SimEngine sim;
    sim.setVerbosity(static_cast<Verbosity_tp>(to_underlying(verbosity) - 1));
    double mu = pSys->getMu();

    Eigen::VectorXd q0 = Eigen::Map<Eigen::VectorXd>(&(state[0]), 7, 1);

    // Get the energy at the point
    std::vector<double> q_full = state;
    q_full.insert(q_full.end(), ctrl0.begin(), ctrl0.end());
    double Hlt = DynamicsModel_cr3bp_lt::getHamiltonian(0, &(q_full[0]), pSys, pLaw);

    // Loop through each stability type
    for(unsigned int s = 0; s < eigVals.size(); s++){
        // Transform the eigenvectors to this updated time
        MatrixXRd newVec = STM*eigVecs.col(s);

        // Factor with which to normalize the eigenvector
        double mag = (stepType == Manifold_StepOff_tp::STEP_VEC_NORMFULL) ?
            newVec.norm() : sqrt(newVec(0)*newVec(0) + newVec(1)*newVec(1) + newVec(2)*newVec(2));

        // Use reverse time for stable manifolds
        sim.setRevTime(std::abs(eigVals[s]) < 1);

        // Scale the vector and make sure it is pointing in the +x direction
        Eigen::VectorXd baseDirection(7);
        baseDirection.head(6) = newVec/mag;
        baseDirection[6] = 0;
        // if(astrohelion::sign(newVec(0)) != 0)
           // baseDirection *= astrohelion::sign(newVec(0));

        // Determine which directions the user has specified with manifoldType
        std::vector<int> dirs;
        if(std::abs(to_underlying(manifoldType)) <= 1){
            // Both directions are desired
            dirs.push_back(1);
            dirs.push_back(-1);
        }else{
            // Only one direction is desired
            if(std::abs(to_underlying(manifoldType)) == 2){ dirs.push_back(1); }
            if(std::abs(to_underlying(manifoldType)) == 3){ dirs.push_back(-1); }
        }

        for(unsigned int di = 0; di < dirs.size(); di++){
            // Flip to (+) or (-) direction according to manifoldType
            Eigen::VectorXd direction = dirs[di]*baseDirection;

            // Step away from the point on the arc in the direction of the eigenvector
            Eigen::VectorXd ic = q0 + stepOffDist/pSys->getCharL() * direction;

            // Scale the velocity part of ic to match Jacobi if that setting is enabled
            if(stepType == Manifold_StepOff_tp::STEP_MATCH_JC){
                Eigen::Vector3d v = ic.block<3,1>(3,0);
                v /= v.norm();  // make it a unit vector

                // Compute the pseudopotential 
                double r13 = sqrt((ic[0] + mu)*(ic[0] + mu) + ic[1]*ic[1] + ic[2]*ic[2]);
                double r23 = sqrt((ic[0] - 1 + mu)*(ic[0] - 1 + mu) + ic[1]*ic[1] + ic[2]*ic[2]);
                double U = (1 - mu)/r13 + mu/r23 + 0.5*(ic[0]*ic[0] + ic[1]*ic[1]);

                std::vector<double> ic_full(ic.data(), ic.data()+ic.cols());
                ic_full.insert(ic_full.end(), ctrl0.begin(), ctrl0.end());
                double alt[3] = {0};
                pLaw->getOutput(0, &(ic_full[0]), pSys, alt, 3);
        
                double r_dot_a = ic[0]*alt[0] + ic[1]*alt[1] + ic[2]*alt[2];

                // Rescale the velocity vector so that the magnitude yields the exact Jacobi Constant
                v *= sqrt(2*(Hlt + U + r_dot_a));
                ic.block<3,1>(3,0) = v; // Reassign the velocity components of ic
            }

            std::vector<double> ic_vec(ic.data(), ic.data() + ic.size());

            // Simulate for some time to generate a manifold arc
            Arcset_cr3bp_lt traj(static_cast<const SysData_cr3bp_lt *>(pSys));
            if(tof > 0){
                sim.runSim(ic_vec, ctrl0, 0, tof, &traj, pLaw);
            }else{
                traj.addNode(Node(ic.data(), ic.size(), 0));
            }
            manifolds.push_back(traj);
        } // End of +/- direction loop
    }// End of Stable/Unstable eigenvector loop

    return manifolds;
}//====================================================

//-----------------------------------------------------
//      Utility Functions
//-----------------------------------------------------

/**
 *  @brief Compute the eigenvector/eigenvalue pair(s) that matches the desired 
 *  manifold stability from a CR3BP periodic orbit
 *  @details If the stable/unstable subpsace has more than one set of manifolds, 
 *  the algorithm selects the eigenvalue/eigenvector that is most unstable/stable.
 * 
 *  @param manifoldType The type of manifold desired
 *  @param pPerOrbit pointer to a periodic CR3BP orbit. No checks are performed 
 *  to ensure periodicity; If the trajectory is not periodic or nearly periodic, 
 *  you may get unexpected results. It is assumed that the final STM in the orbit 
 *  object represents the monodromy matrix.
 *  @param eigVals_final pointer to a vector of doubles; the computed 
 *  eigenvalue(s) will be stored here
 *  
 *  @return The eigenvector(s) (column) that best matches the criteria
 *  @throws Exception if the eigenvalue computation is unsuccessful
 *  @throws Excpetion if no stable or unstable eigenvalues are identified
 */
MatrixXRd ManifoldEngine::eigVecValFromPeriodic(Manifold_tp manifoldType, 
    const Arcset_cr3bp *pPerOrbit, std::vector<cdouble> *eigVals_final){

    unsigned int coreDim = pPerOrbit->getSysData()->getDynamicsModel()->getCoreStateSize();
	// assert(pPerOrbit->getSysData()->getDynamicsModel()->getCoreStateSize() == 6);

	printVerb(verbosity >= Verbosity_tp::DEBUG, "Eigenvector/Eigenvalue Computations\n");

	// First, get the monodromy matrix and compute the eigenvectors/values
    MatrixXRd mono = pPerOrbit->getSTMByIx(-1);
    if(verbosity >= Verbosity_tp::DEBUG)
        std::cout << "Monodromy Matrix:\n" << mono << std::endl;

    if(coreDim > 6){
        MatrixXRd temp = mono.block<6,6>(0,0);
        mono = temp;
        if(verbosity >= Verbosity_tp::DEBUG)
            std::cout << "Trimmed M to\n" << mono << std::endl;
    }

    Eigen::EigenSolver<MatrixXRd> eigensolver(mono);
    if(eigensolver.info() != Eigen::Success)
        throw Exception("ManifoldEngine::computeEigVecValFromPeriodic:"
            " Could not compute eigenvalues of monodromy matrix");

    Eigen::VectorXcd vals = eigensolver.eigenvalues();
    MatrixXRcd eigVecs = eigensolver.eigenvectors();
    std::vector<cdouble> eigData(vals.data(), vals.data() + vals.rows());

    // Sort eigenvalues to put them in a "propper" order, get indices
    // to sort the eigenvectors to match
    std::vector<MatrixXRcd> tempVecs {eigVecs};
    std::vector<unsigned int> sortedIx = sortEig(eigData, tempVecs);
    std::vector<cdouble> sortedEig;
    for(unsigned int i = 0; i < eigData.size(); i++){
        sortedEig.push_back(eigData[sortedIx[i]]);
    }

    printVerb(verbosity >= Verbosity_tp::DEBUG, "  Manifold Type = %d\n", 
        to_underlying(manifoldType));

    // Figure out which eigenvalues are the stable and unstable ones,
    // only keep the ones that match the stability of the desired manifolds
    std::vector<cdouble> nonCenterVals;
    std::vector<cdouble> nonCenterVecs;
    for(unsigned int c = 0; c < sortedEig.size(); c++){
        double realErr = std::real(sortedEig[c]) - 1.0;
        double imagErr = std::imag(sortedEig[c]);

        bool keepPair = false;
        if(std::abs(realErr) > tol_eigVal && std::abs(imagErr) < tol_eigVal){
            // This eigenvector/eigenvalue pair is stable or unstable, not a center
            if(to_underlying(manifoldType) >= 0){
            	// Keep the pair if they are unstable
            	keepPair = std::abs(sortedEig[c]) > 1.0;
            }
            if(!keepPair && to_underlying(manifoldType) <= 0){
            	// Only run this check if the previous one didn't return true 
                // (overlap for manifoldType = 0)
            	// Keep the pair if they are stable
            	keepPair = std::abs(sortedEig[c]) < 1.0;
            }
        }else{
        	printVerb(verbosity >= Verbosity_tp::DEBUG, 
                "  Eigenvalue %s is on the unit circle\n", 
                complexToStr(sortedEig[c]).c_str());
        }

        if(keepPair){
        	printVerb(verbosity >= Verbosity_tp::DEBUG, 
                "  Keeping eigenvalue %s\n", complexToStr(sortedEig[c]).c_str());
        	nonCenterVals.push_back(sortedEig[c]);
            unsigned int vecIx = sortedIx[c];
            nonCenterVecs.insert(nonCenterVecs.end(),
                eigVecs.data()+vecIx*6, eigVecs.data()+(vecIx+1)*6); 
        }else{
        	printVerb(verbosity >= Verbosity_tp::DEBUG, 
                "  Discarding eigenvalue %s\n", complexToStr(sortedEig[c]).c_str());
        }
    }

    if(nonCenterVals.size() == 0){
    	if(verbosity >= Verbosity_tp::SOME_MSG)
        	printWarn("ManifoldEngine::computeEigVecValFromPeriodic: "
                "No stable/unstable eigenvalues were found\n");
        throw Exception("ManifoldEngine::computeEigVecValFromPeriodic: "
            "No stable/unstable eigenvalues were found\n");
    }

    if(nonCenterVals.size() == sortedEig.size()){
        if(verbosity >= Verbosity_tp::SOME_MSG)
            printWarn("ManifoldEngine::computeEigVecValFromPeriodic: "
                "No center eigenvalues were found\nCheck to make sure the input"
                " orbit is truely periodic and the STMs represent the\nsequential"
                " evolution rather than parallel\n");
    }

    // Only keep the real parts of the eigenvectors
    std::vector<double> realVecs = astrohelion::real(nonCenterVecs);
    MatrixXRd eigVecs_final = Eigen::Map<MatrixXRd>(&(realVecs[0]), 
        nonCenterVecs.size()/6, 6);
    eigVecs_final.transposeInPlace();   // Transpose so eigenvectors are columns

    // Determine how many eigenvectors we're expected to return
    unsigned int numVecs = 0;
	switch(std::abs(to_underlying(manifoldType))){
		case 0: numVecs = 2; break;		// Both stable and unstable
		default: numVecs = 1; break;	// Only stable or unstable
	}

	printVerb(verbosity >= Verbosity_tp::DEBUG, 
        "  Will return %u vectors/values\n", numVecs);

	// Trim the eigenvector and eigenvalue objects to keep ones with the largest stability index
    if(nonCenterVals.size() > numVecs){
    	if(verbosity >= Verbosity_tp::SOME_MSG)
        	astrohelion::printWarn("ManifoldEngine::computeEigVecValFromPeriodic:"
                " Stable/Unstable subspace is larger than 2D. Only pair with the"
                " largest stability index will be returned\n");

        // Find the most unstable eigenvalue (largest magnitude)
    	auto it_U = max_element(nonCenterVals.begin(), nonCenterVals.end(), compareCDouble);
    	// Find the most stable eigenvalue (smallest magnitude);
    	auto it_S = min_element(nonCenterVals.begin(), nonCenterVals.end(), compareCDouble);

    	// Create temporary storage to store the values/vectors we want to keep
    	std::vector<cdouble> tempVals(numVecs, 0);
    	MatrixXRd tempVecs(6, numVecs);

    	if(numVecs == 2){
    		// Keep both
			tempVals[0] = *it_S;
			tempVals[1] = *it_U;
			int ix_S = std::distance(nonCenterVals.begin(), it_S);
			int ix_U = std::distance(nonCenterVals.begin(), it_U);
			tempVecs.col(0) = eigVecs_final.col(ix_S);
			tempVecs.col(1) = eigVecs_final.col(ix_U);
    	}else{
    		// Keep only one
    		auto it = to_underlying(manifoldType) < 0 ? it_S : it_U;
    		tempVals[0] = *it;
    		int ix = std::distance(nonCenterVals.begin(), it);
    		tempVecs.col(0) = eigVecs_final.col(ix);
    	}

    	// Overwrite the storage
    	eigVecs_final = tempVecs;
    	nonCenterVals = tempVals;
    }

    // Make sure the order is correct (stable, then unstable)
    if(numVecs == 2 && std::abs(nonCenterVals[0]) > 1){
    	std::reverse(nonCenterVals.begin(), nonCenterVals.end());
    	
    	// Flip order of eigenvectors
    	MatrixXRd temp = eigVecs_final.col(0);
    	eigVecs_final.col(0) = eigVecs_final.col(1);
    	eigVecs_final.col(1) = temp;
    }

    // Write data to outputs
    if(eigVals_final){
    	eigVals_final->clear();
    	eigVals_final->insert(eigVals_final->end(), nonCenterVals.begin(), 
            nonCenterVals.end());
    }

    return eigVecs_final;
}//====================================================

/**
 *  @brief Copy the ManifoldEngine
 *  @details This engine will be made a mirror image of the
 *  specified engine.
 * 
 *  @param m Reference to another ManifoldEngine object
 */
void ManifoldEngine::copyMe(const ManifoldEngine &m){
	Engine::copyBaseEngine(m);
	stepOffDist = m.stepOffDist;
}//====================================================

/**
 *  @brief Reset all parameters to their default values
 */
void ManifoldEngine::reset(){
	stepOffDist = 20;	// km
}//====================================================

/**
 *  @brief A comparator for complex, double precision numbers
 *  @details [long description]
 * 
 *  @param lhs a complex number
 *  @param rhs another complex number
 * 
 *  @return whether or not the magnitude of lhs is less than the magnitude of rhs
 */
bool ManifoldEngine::compareCDouble(cdouble lhs, cdouble rhs){
	return std::abs(lhs) < std::abs(rhs);
}//====================================================

const char* ManifoldEngine::manType2Str(Manifold_tp tp){
    switch(tp){
        case Manifold_tp::MAN_ALL: return "All Manifolds";
        case Manifold_tp::MAN_U: return "Unstable Manifolds";
        case Manifold_tp::MAN_U_RIGHT: return "Unstable Manifolds (+x)";
        case Manifold_tp::MAN_U_LEFT: return "Unstable Manifolds (-x)";
        case Manifold_tp::MAN_S: return "Stable Manifolds";
        case Manifold_tp::MAN_S_RIGHT: return "Stable Manifolds (+x)";
        case Manifold_tp::MAN_S_LEFT: return "Stable Manifolds (-x)";
    }
    return "UNKNOWN VALUE";
}//====================================================

const char* ManifoldEngine::stepType2Str(Manifold_StepOff_tp tp){
    switch(tp){
        case Manifold_StepOff_tp::STEP_MATCH_JC: return "Match Jacobi";
        case Manifold_StepOff_tp::STEP_VEC_NORMPOS: return "Normalize by Position";
        case Manifold_StepOff_tp::STEP_VEC_NORMFULL: return "Normalize by Full Vector";
    }
    return "UNKOWN VALUE";
}//====================================================


}// End of Astrohelion Namespace