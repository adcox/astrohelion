/**
 * Test the matio reading and writing functions I've created
 */

#include "tpat.hpp"

#include "tpat_ascii_output.hpp"
#include "tpat_exceptions.hpp"
#include "tpat_utilities.hpp"

#include "matio.h"

#include <cstdio>
#include <iostream>

static const char* PASS = BOLDGREEN "PASS" RESET;
static const char* FAIL = BOLDRED "FAIL" RESET;

using namespace std;

int main(){

	int int8Test = 1;	// cannot be negative (not sure why?)
	int int16Test = 3;	// cannot be negative (not sure why?)
	int int32Test = -5;
	int int64Test = -7;

	uint uint8Test = 2;
	uint uint16Test = 4;
	uint uint32Test = 6;
	uint uint64Test = 8;

	double doubleTest = PI;

	// ----------------- //
	// Create variables  //
	// ----------------- //
	mat_t *matfp = Mat_CreateVer("data/matioTest.mat", NULL, MAT_FT_DEFAULT);
	if(NULL == matfp){
		printErr("Could not create mat file... exiting\n");
	}else{
		size_t dims[2] {1,1};
		
		matvar_t *int8Var = Mat_VarCreate("Int8Test", MAT_C_INT8, MAT_T_INT8, 2, dims, &int8Test, MAT_F_DONT_COPY_DATA);
		saveVar(matfp, int8Var, "Int8Test", MAT_COMPRESSION_NONE);

		matvar_t *uint8Var = Mat_VarCreate("UInt8Test", MAT_C_UINT8, MAT_T_UINT8, 2, dims, &uint8Test, MAT_F_DONT_COPY_DATA);
		saveVar(matfp, uint8Var, "UInt8Test", MAT_COMPRESSION_NONE);

		matvar_t *int16Var = Mat_VarCreate("Int16Test", MAT_C_INT16, MAT_T_INT16, 2, dims, &int16Test, MAT_F_DONT_COPY_DATA);
		saveVar(matfp, int16Var, "Int16Test", MAT_COMPRESSION_NONE);

		matvar_t *uint16Var = Mat_VarCreate("UInt16Test", MAT_C_UINT16, MAT_T_UINT16, 2, dims, &uint16Test, MAT_F_DONT_COPY_DATA);
		saveVar(matfp, uint16Var, "UInt16Test", MAT_COMPRESSION_NONE);		

		matvar_t *int32Var = Mat_VarCreate("Int32Test", MAT_C_INT32, MAT_T_INT32, 2, dims, &int32Test, MAT_F_DONT_COPY_DATA);
		saveVar(matfp, int32Var, "Int32Test", MAT_COMPRESSION_NONE);

		matvar_t *uint32Var = Mat_VarCreate("UInt32Test", MAT_C_UINT32, MAT_T_UINT32, 2, dims, &uint32Test, MAT_F_DONT_COPY_DATA);
		saveVar(matfp, uint32Var, "UInt32Test", MAT_COMPRESSION_NONE);

		matvar_t *int64Var = Mat_VarCreate("Int64Test", MAT_C_INT64, MAT_T_INT64, 2, dims, &int64Test, MAT_F_DONT_COPY_DATA);
		saveVar(matfp, int64Var, "Int64Test", MAT_COMPRESSION_NONE);

		matvar_t *uint64Var = Mat_VarCreate("UInt64Test", MAT_C_UINT64, MAT_T_UINT64, 2, dims, &uint64Test, MAT_F_DONT_COPY_DATA);
		saveVar(matfp, uint64Var, "UInt64Test", MAT_COMPRESSION_NONE);

		matvar_t *doubleVar = Mat_VarCreate("DoubleTest", MAT_C_DOUBLE, MAT_T_DOUBLE, 2, dims, &doubleTest, MAT_F_DONT_COPY_DATA);
		saveVar(matfp, doubleVar, "DoubleTest", MAT_COMPRESSION_NONE);
	}
	Mat_Close(matfp);


	// ---------------------- //
	// Read & Test variables  //
	// ---------------------- //
	mat_t *matReadFile = Mat_Open("data/matioTest.mat", MAT_ACC_RDONLY);
	if(NULL == matfp){
		printErr("Could not open mat file... exiting\n");
	}else{

		printf("Signed Integers:\n");

		double val = readDoubleFromMat(matReadFile, "Int8Test");
		cout << "Int 8 Test: " << (int8Test == (int)val ? PASS : FAIL) << endl;

		val = readDoubleFromMat(matReadFile, "Int16Test");
		cout << "Int 16 Test: " << (int16Test == (int)val ? PASS : FAIL) << endl;

		val = readDoubleFromMat(matReadFile, "Int32Test");
		cout << "Int 32 Test: " << (int32Test == (int)val ? PASS : FAIL) << endl;

		val = readDoubleFromMat(matReadFile, "Int64Test");
		cout << "Int 64 Test: " << (int64Test == (int)val ? PASS : FAIL) << endl;

		printf("\nUnsigned Integers:\n");

		val = readDoubleFromMat(matReadFile, "UInt8Test");
		cout << "UInt 8 Test: " << (uint8Test == (uint)val ? PASS : FAIL) << endl;

		val = readDoubleFromMat(matReadFile, "UInt16Test");
		cout << "UInt 16 Test: " << (uint16Test == (uint)val ? PASS : FAIL) << endl;

		val = readDoubleFromMat(matReadFile, "UInt32Test");
		cout << "UInt 32 Test: " << (uint32Test == (uint)val ? PASS : FAIL) << endl;

		val = readDoubleFromMat(matReadFile, "UInt64Test");
		cout << "UInt 64 Test: " << (uint64Test == (uint)val ? PASS : FAIL) << endl;

		printf("\nHigher Precision Numbers:\n");

		val = readDoubleFromMat(matReadFile, "DoubleTest");
		cout << "Double Test: " << (doubleTest == val ? PASS : FAIL) << endl;		
	}

	Mat_Close(matReadFile);

	return EXIT_SUCCESS;
}