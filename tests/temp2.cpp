#include "AllIncludes.hpp"

#include <exception>
#include <iostream>
#include "matio.h"
#include <vector>

using namespace astrohelion;

int main(){

	/*
	 *	Propagate an Earth-Moon CR3BP L1 Lyapunov manifold to the x=0 plane
	 */
	std::vector<double> q0 {0.95463, 0.31288, 0, 0.17574, -0.12038, 0, 1};
	// double tof = 3*PI;
	double tof = 2;

	SimEngine sim;
	SysData_cr3bp_lt sys("earth", "moon", 1);
	double Isp = 1500;
	ControlLaw_cr3bp_lt law(ControlLaw_cr3bp_lt::VAR_F_GENERAL, 
		std::vector<double> {Isp});
	std::vector<double> ctrl0{0,0,0};	// {sqrt(f), alpha, beta}

	Arcset_cr3bp natArc(&sys);

	Event yzCross(Event_tp::YZ_PLANE, 0, true);
	sim.addEvent(yzCross);
	sim.runSim_manyNodes(q0, ctrl0, 0, tof, 2, &natArc, &law);

	// natArc.saveToMat("temp.mat");

	// waitForUser();

	/*
	 *	Formulate corrections problem
	 */
	// Fix initial state
	Constraint initStateCon(Constraint_tp::STATE, natArc.getNodeByIx(0).getID(),
		q0);
	natArc.addConstraint(initStateCon);

	// Target a final state
	// std::vector<double> qf {0, 0.3, NAN, NAN, NAN, NAN, NAN};
	// std::vector<double> qf {0.9, -0.3, NAN, NAN, NAN, NAN, NAN};
	std::vector<double> qf {1.05, -0.3, NAN, NAN, NAN, NAN, NAN};
	// Constraint finalStateCon(Constraint_tp::STATE, natArc.getNodeByIx(-1).getID(),
	// 	qf);
	// natArc.addConstraint(finalStateCon);

	Constraint rmFinalCtrl(Constraint_tp::RM_CTRL, 
		natArc.getNodeByIx(-1).getID(), nullptr, 0);
	natArc.addConstraint(rmFinalCtrl);

	Constraint endSegCon(Constraint_tp::ENDSEG_STATE, 
		natArc.getSegByIx(-1).getID(), qf);
	natArc.addConstraint(endSegCon);

	Constraint rmFinalState(Constraint_tp::RM_STATE, 
		natArc.getNodeByIx(-1).getID(), nullptr, 0);

	MultShootEngine shooter;
	// shooter.setVerbosity(Verbosity_tp::NO_MSG);
	shooter.setSaveEachIt(true);
	shooter.setMaxIts(200);
	shooter.setDoLineSearch(true);
	// ctrl0[0] = sqrt(1e-4);
	ctrl0[0] = 0;
	ctrl0[2] = 0;

	std::map<double, std::vector<double> > allData;
	const std::vector<double> nanData {NAN, NAN, NAN};
	double alpha0 = 0;
	unsigned int numSteps = 1;
	double alphaStep = 2*PI/numSteps;
	
	#pragma omp parallel for firstprivate(ctrl0, natArc, shooter) schedule(dynamic)
	for(unsigned int i = 0; i < numSteps; i++){
		double a = alpha0 + i*alphaStep;
		Arcset_cr3bp_lt transfer(&sys);
		std::vector<double> data;
		/*
		 *	Prep guess for low-thrust
		 */
		ctrl0[1] = a;
		for(unsigned int n = 0; n < natArc.getNumNodes(); n++){
			natArc.getNodeRefByIx(n).setExtraParamVec(PARAMKEY_CTRL, ctrl0);
		}

		/*
		 *	Do the corrections
		 */
		MultShootData it(&natArc);
		try{
			shooter.multShoot(&natArc, &transfer, &it);

			// Save {f_f, alpha_f, beta_f, it_count}
			std::vector<double> ctrlf = transfer.getNodeRefByIx(0).\
				getExtraParamVec(PARAMKEY_CTRL);
			data.insert(data.end(), ctrlf.begin(), ctrlf.end());
			printColor(GREEN, "alpha = %06.2f deg converged\n", a*180/PI);
		}catch(Exception &e){
			// Put NAN values in for the converged control variables
			data.insert(data.end(), nanData.begin(), nanData.end());
			printColor(RED, "alpha = %06.2f deg diverged\n", a*180/PI);
		}

		data.push_back(it.count);

		allData[a] = data;
	}

	std::vector<double> outData(5*allData.size(), NAN);
	unsigned int i = 0;
	for(auto entry : allData){
		outData[5*i] = entry.first;
		std::vector<double> &d = entry.second;
		std::copy(d.begin(), d.end(), outData.begin() + 5*i+1);
		i++;
	}
	// saveMatrixToFile("MultShootResults.mat", "results", outData, 
	// 	outData.size()/5, 5);

	return EXIT_SUCCESS;
}