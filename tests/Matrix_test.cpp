/**
*	Test the matrix object and its operations
*/

#include <iostream>

#include "adtk_matrix.hpp"

using namespace std;

static int I_r = 2, I_c = 2;
static int B_r = 2, B_c = 2;
static int C_r = 2, C_c = 3;
static double I_data[] = {1,0,0,1};
static double B_data[] = {1,1,0,1};
static double C_data[] = {1,2,3,4,5,6};

bool test_constructor(adtk_matrix C){
	// Check the elements are correct
	for(int r=0; r<C_r; r++){
		for(int c=0; c<C_c; c++){
			if(gsl_matrix_get(C.getGSLMat(), r, c) != C_data[r*C_c + c])
				return false;
		}
	}

	return true;
}//===========================================

bool test_matMult(adtk_matrix I, adtk_matrix B, adtk_matrix C){
	// Test to make sure it catches size-mismatch
	try{
		C*C;
		return false;	// if C*C succeeds, there is a problem!
	}catch(...){}

	// Test simple identity matrix multiplication
	adtk_matrix Q = I*I;
	if(Q != I)
		return false;

	// Test different 2x2 multiplication
	Q = B*B;
	double sol[] = {1,2,0,1};
	adtk_matrix Sol(B.getRows(), B.getCols(), sol);
	if(Q != Sol)
			return false;

	// Test 2x2 * 2x3 multiplication
	adtk_matrix Q2 = B*C;
	double sol2[] = {5,7,9,4,5,6};
	adtk_matrix Sol2(B.getRows(), C.getCols(), sol2);
	if(Q2 != Sol2)
		return false;

	return true;
}//===========================================

bool test_matMult_inPlace(adtk_matrix I, adtk_matrix B, adtk_matrix C){
	try{
		C*=I;
		return false;
	}catch(...){}

	adtk_matrix tempI = I;
	if((tempI*=tempI) != I){
		return false;
	}

	adtk_matrix tempB = B;
	double sol[] = {1,2,0,1};
	adtk_matrix Sol(B.getRows(), B.getCols(), sol);
	if((tempB*=tempB) != Sol)
		return false;

	return true;
}

bool test_multScalar(adtk_matrix I){
	double sol[] = {5,0,0,5};
	adtk_matrix Sol(I_r, I_c, sol);

	adtk_matrix Q = I*5;
	if(Q != Sol)
		return false;

	return true;
}//===========================================

bool test_multScalar_inPlace(){
	double data[] = {1,0,0,1};
	double sol[] = {-1,0,0,-1};
	adtk_matrix Sol(I_r, I_c, sol);

	adtk_matrix I2(2,2,data);
	I2 *= -1;
	if(I2 != Sol)
		return false;

	return true;
}//===========================================

bool test_matAdd(adtk_matrix B){
	double sol[] = {2,2,0,2};
	adtk_matrix Sol(2,2,sol);
	adtk_matrix Q = B + B;

	if(Q != Sol)
		return false;

	return true;
}//===========================================

bool test_matSubtract(adtk_matrix C){
	adtk_matrix Zeros(2,3);
	if(C - C != Zeros)
		return false;

	return true;
}//===========================================

bool test_identity(adtk_matrix I){
	adtk_matrix I2 = adtk_matrix::Identity(2);
	cout << endl;
	I2.print();
	adtk_matrix Q = I - I2;
	Q.print("%12.3e");

	return I2 == I;
}//===========================================

int main(void){

	adtk_matrix I(2, 2, I_data);
	adtk_matrix B(2, 2, B_data);
	adtk_matrix C(2, 3, C_data);

	cout << "Test: getRows()... " << (C.getRows() == C_r ? "PASS" : "FAIL") << endl;
	cout << "Test: getColss()... " << (C.getCols() == C_c ? "PASS" : "FAIL") << endl;
	cout << "Test: Constructor... " << (test_constructor(C) ? "PASS" : "FAIL") << endl;

	cout << "\nIf any of the above tests failed, all the following tests cannot be trusted!\n" <<endl;

	cout << "Test: operator A == B ... " << ( ((I == I) == 1 && (I == B) == 0) ? "PASS" : "FAIL") << endl;
	cout << "Test: operator A != B ... " << ( ((I != I) == 0 && (I != B) == 1) ? "PASS" : "FAIL") << endl;

	cout << "\nIf any of the above tests failed, all the following tests cannot be trusted!\n" <<endl;

	cout << "Test: operator A+B ... " << ( test_matAdd(B) ? "PASS" : "FAIL") << endl;
	cout << "Test: operator A-B ... " << ( test_matSubtract(C) ? "PASS" : "FAIL") << endl;
	cout << "Test: operator A*B ... " << ( test_matMult(I, B, C) ? "PASS" : "FAIL") << endl;
	cout << "Test: operator A*q ... " << ( test_multScalar(I) ? "PASS" : "FAIL") << endl;
	cout << "Test: operator A+=B ... " << "NOT IMPLEMENTED" << endl;
	cout << "Test: operator A-=B ... " << "NOT IMPLEMENTED" << endl;
	cout << "Test: operator A*=B ... " << ( test_matMult_inPlace(I, B, C) ? "PASS" : "FAIL") << endl;
	cout << "Test: operator A*=q ... " << ( test_multScalar_inPlace() ? "PASS" : "FAIL") << endl;

	cout << "Test: Create Identity... " << (test_identity(I) ? "PASS" : "FAIL") << endl;
	return 0;
}
