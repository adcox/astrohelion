
#include "AsciiOutput.hpp"
#include "SimEngine.hpp"
#include "SysData_bc4bp.hpp"
#include "SysData_cr3bp.hpp"
#include "Traj_bc4bp.hpp"
#include "Traj_cr3bp.hpp"

#include <iostream>

static const char* PASS = BOLDGREEN "PASS" RESET;
static const char* FAIL = BOLDRED "FAIL" RESET;

using namespace std;
using namespace astrohelion;

void testCR3BPTraj(){
	SysData_cr3bp emData("earth", "moon");
	SimEngine sim;
	double ic[] = {0.887415132364297, 0, 0, 0, -0.332866299501083, 0};	// EM L1
	double T = 3.02796323553149;	// EM L1 Period
	Traj_cr3bp crTraj(&emData);
	sim.runSim(ic, T, &crTraj);
	crTraj.saveToMat("data/crTraj.mat");

	Traj_cr3bp crTemp(&emData);
	crTemp.readFromMat("data/crTraj.mat");

	printf("Testing Save/Read functions on CR3BP Trajectory\n");
	cout << "Same Final State: " << (crTraj.getStateByIx(-1) == crTemp.getStateByIx(-1) ? PASS : FAIL) << endl;
	cout << "Same Final Accel: " << (crTraj.getAccelByIx(-1) == crTemp.getAccelByIx(-1) ? PASS : FAIL) << endl;
	cout << "Same Final Time: " << (crTraj.getTimeByIx(-1) == crTemp.getTimeByIx(-1) ? PASS : FAIL) << endl;
	cout << "Same Final STM: " << (crTraj.getSTMByIx(-1) == crTemp.getSTMByIx(-1) ? PASS : FAIL) << endl;
	cout << "Same Final Jacobi: " << (crTraj.getJacobiByIx(-1) == crTemp.getJacobiByIx(-1) ? PASS : FAIL) << endl;
}

void testBC4BPTraj(){
	SysData_bc4bp semData("sun", "earth", "moon");
	SimEngine sim;
	double ic[] = {-0.745230328320519, 7.22625684942683e-04, 7.45549413286038e-05, -7.30710697247992e-06, -0.0148897145134465, -1.23266135281459e-06};
	double T = 313;	// SE L1 Period
	Traj_bc4bp bcTraj(&semData);
	sim.runSim(ic, T, &bcTraj);
	bcTraj.saveToMat("data/bcTraj.mat");

	Traj_bc4bp bcTemp(&semData);
	bcTemp.readFromMat("data/bcTraj.mat");

	printf("Testing Save/Read functions on CR3BP Trajectory\n");
	cout << "Same Final State: " << (bcTraj.getStateByIx(-1) == bcTemp.getStateByIx(-1) ? PASS : FAIL) << endl;
	cout << "Same Final Accel: " << (bcTraj.getAccelByIx(-1) == bcTemp.getAccelByIx(-1) ? PASS : FAIL) << endl;
	cout << "Same Final Time: " << (bcTraj.getTimeByIx(-1) == bcTemp.getTimeByIx(-1) ? PASS : FAIL) << endl;
	cout << "Same Final STM: " << (bcTraj.getSTMByIx(-1) == bcTemp.getSTMByIx(-1) ? PASS : FAIL) << endl;
	cout << "Same Final dqdT: " << (bcTraj.get_dqdTByIx(-1) == bcTemp.get_dqdTByIx(-1) ? PASS : FAIL) << endl;
}


int main(){
	
	testCR3BPTraj();
	testBC4BPTraj();

	return EXIT_SUCCESS;
}