/**
* 	Header file for calculations library
*/

#ifndef __H_CALCULATIONS__
#define __H_CALCULATIONS__

// #include <vector>

const double dtGuess = 1e-12;
const double absTol = 1e-12;
const double relTol = 1e-14;

int cr3bp_EOMs(double t, const double s[], double sdot[], void *params);
int cr3bp_simple_EOMs(const double s[], double sdot[], double);
void cr3bp_getUDDots(double, double, double, double, double*);
// std::vector<double> adtk_cr3bp_integrate(double ic[], double t[], double mu, int t_dim);

#endif