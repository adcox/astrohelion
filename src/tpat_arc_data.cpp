/**
 *  @file tpat_arc_data.cpp
 *	@brief Data object that stores information about an integrated arc
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

#include "tpat_arc_data.hpp"
#include "tpat_eigen_defs.hpp"
#include "tpat_sys_data.hpp"
#include "tpat_exceptions.hpp"
#include "tpat_utilities.hpp"

//-----------------------------------------------------
//      *structors
//-----------------------------------------------------

/**
 *	@brief Constructor (requires system data object)
 *	@param sys a pointer to a system data object that describes
 *	the system this trajectory is integrated in
 */
tpat_arc_data::tpat_arc_data(const tpat_sys_data *sys) : sysData(sys){}

/**
 *	@brief Copy constructor
 *	@param d an arc_data reference
 */
tpat_arc_data::tpat_arc_data(const tpat_arc_data &d){
	copyMe(d);
}//====================================================

/**
 *	@brief Destructor
 */
tpat_arc_data::~tpat_arc_data(){
	steps.clear();
	extraParamRowSize.clear();
}//====================================================

//-----------------------------------------------------
//      Operators
//-----------------------------------------------------

/**
 *	@brief Set this object equal to another
 *	@param d an arc_data reference
 *	@return a reference to this arc_data object
 */
tpat_arc_data& tpat_arc_data::operator =(const tpat_arc_data &d){
	copyMe(d);
	return *this;
}//====================================================

/**
 *	@brief Concatenate two arc_data objects
 *
 * 	When adding A + B, if the final state of A and initial state
 *	of B are the same, this algorithm will skip the initial state
 *	of B in the concatenation to avoid duplicating a node or state.
 *	
 *	The process begins by creating a new arc_data object and setting
 *	it equal to A. B is then appended to the new object and the STM's
 *	from B are updated to be continuous with those found in A. Note 
 *	that if A and B are not conitnuous, the new STMs will be incorrect
 *	and will have no practical meaning
 *
 *	@param rhs the right-hand-side of the addition operation
 *	@return a reference to the concatenated arc_data object
 */
tpat_arc_data& tpat_arc_data::operator +=(const tpat_arc_data &rhs){
	if( *sysData != *(rhs.sysData))
		throw tpat_exception("tpat_arc_data::+: Cannot concatenate data sets from different systems");

	if(steps.size() == 0){
		copyMe(rhs);
		return *this;
	}

	if(rhs.steps.size() == 0)
		return *this;

	int skipShift = 0;
	double newTol = tol > rhs.tol ? tol : rhs.tol;
	if(newTol == 0)
		tol = 1e-9;

	if(steps[0] == rhs.steps[0])
		skipShift = 1;

	// Delete data from the lhs if anything is duplicated; chose LHS because it will serve nodesets better
	if(skipShift > 0)
		steps.erase(steps.end()-skipShift, steps.end());

	size_t lhs_numSteps = steps.size();

	// Copy data from rhs
	steps.insert(steps.end(), rhs.steps.begin(), rhs.steps.end());

	// Adjust STMs (Assuming arcs are continuous)
	MatrixXRd lhs_lastSTM = steps[lhs_numSteps-1].getSTM(); 	// PHI(t1, t0)
	for(size_t n = lhs_numSteps; n < steps.size(); n++){
		MatrixXRd oldSTM = steps[n].getSTM();	// PHI(t2, t1)

		// Multiply PHI(t2, t1)*PHI(t1, t0) to get PHI(t2, t0)
		steps[n].setSTM(oldSTM*lhs_lastSTM);

		// Update constraint step numbers
		steps[n].setConstraintNodeNum(n);
	}

	return *this;
}//====================================================

// tpat_arc_data& operator +(const tpat_arc_data &lhs, const tpat_arc_data &rhs){
// 	if( *(lhs.sysData) != *(rhs.sysData) )
// 		throw tpat_exception("tpat_arc_data::+: Cannot concatenate data sets from different systems");

// 	if(lhs.steps.size() == 0){
// 		return tpat_arc_data(rhs);
// 	}

// 	if(rhs.steps.size() == 0){
// 		return tpat_arc_data(lhs);
// 	}

// 	tpat_arc_data temp = lhs;

// 	int skipShift = 0;
// 	double newTol = lhs.tol > rhs.tol ? lhs.tol : rhs.tol;
// 	if(newTol == 0)
// 		temp.tol = 1e-9;

// 	if(lhs.steps[0] == rhs.steps[0])
// 		skipShift = 1;

// 	// Delete data from the lhs if anything is duplicated; chose LHS because it will serve nodesets better
// 	if(skipShift > 0)
// 		temp.steps.erase(steps.end()-skipShift, steps.end());

// 	size_t lhs_numSteps = temp.steps.size();

// 	// Copy data from rhs
// 	temp.steps.insert(temp.steps.end(), rhs.steps.begin(), rhs.steps.end());

// 	// Adjust STMs (Assuming arcs are continuous)
// 	MatrixXRd lhs_lastSTM = temp.steps[lhs_numSteps-1].getSTM(); 	// PHI(t1, t0)
// 	for(size_t n = lhs_numSteps; n < temp.steps.size(); n++){
// 		MatrixXRd oldSTM = temp.steps[n].getSTM();	// PHI(t2, t1)

// 		// Multiply PHI(t2, t1)*PHI(t1, t0) to get PHI(t2, t0)
// 		temp.steps[n].setSTM(oldSTM*lhs_lastSTM);
// 	}

// 	return temp;
// }//===================================================

//-----------------------------------------------------
//      Set and Get Functions
//-----------------------------------------------------
/**
 *	@brief Retrieve an acceleration on the arc
 *	@param ix the step index. If it is negative, the index will count backwards
 *	from the end of the arc (e.g. ix = -1 will return the last acceleration)
 *	@return the acceleration associated with the specified index
 */
std::vector<double> tpat_arc_data::getAccel(int ix) const{
	if(ix < 0)
		ix += steps.size();
	return steps[ix].getAccel();
}//====================================================

/**
 *	@brief Get a vector of one coordinate for all steps
 *	@param ix the index of the coordinate: 0 = x, 1 = y, etc.
 *	@return a vector containing the specified coordinate for all
 *	integration steps
 */
std::vector<double> tpat_arc_data::getCoord(int ix) const{
	if(ix >= 6)
		throw tpat_exception("tpat_arc_data::getCoord: Index Out of Range");

	std::vector<double> coord;
	for(size_t n = 0; n < steps.size(); n++){
		coord.push_back(steps[n].getPosVelState()[ix]);
	}

	return coord;
}//====================================================

/**
 *	@brief Retrieve a set of extra parameters for the specified step
 *	@param step the step index. If it is negative, this index will count 
 *	backwards from the end of the arc (e.g. step = -1 will return something from
 *	the final integration step)
 *	@param ix the index of the extra parameter
 *	@return a vector containing the extra parameter at the specified step and index
 */
std::vector<double> tpat_arc_data::getExtraParam(int step, int ix) const{
	if(step < 0)
		step += steps.size();

	if(ix < 0 || ix >= (int)(extraParamRowSize.size()))
		throw tpat_exception("tpat_arc_data::getExtraParam: parameter index out of bounds");

	int startIx = 0;
	for(int i = 0; i < ix; i++)
		startIx += extraParamRowSize[i];

	int size = extraParamRowSize[ix];
	std::vector<double> extraParam = steps[step].getExtraParams();
	return std::vector<double>(extraParam.begin()+startIx, extraParam.begin()+startIx + size);
}//====================================================

/**
 *	@brief Retrieve the length of this arc, in number of steps
 *	@return the length of this arc, in number of steps
 */
int tpat_arc_data::getLength() const {return (int)(steps.size()); }

/**
 *	@brief Retrieve a position-velocity state on the arc
 *	@param ix the step index. If it is negative, the index will count backwards
 *	from the end of the arc (e.g. ix = -1 will return the last state)
 *	@return the state associated with the specified index
 */
std::vector<double> tpat_arc_data::getState(int ix) const{
	if(ix < 0)
		ix += steps.size();
	return steps[ix].getPosVelState();
}//====================================================

/**
 *	@brief Retrieve data for one integration step
 *	@param ix the step index; if the index is negative, it will count backwards
 *	from the end of the steps (e.g. ix = -1 will return the last step)
 */
tpat_arc_step tpat_arc_data::getStep(int ix) const{
	if(ix < 0)
		ix += steps.size();
	return steps[ix];
}//====================================================

/**
 *	@brief Retrieve an STM on the arc
 *	@param ix the step index. If it is negative, the index will count backwards
 *	from the end of the arc (e.g. ix = -1 will return the last STM)
 *	@return the STM associated with the specified index
 */
MatrixXRd tpat_arc_data::getSTM(int ix) const{
	if(ix < 0)
		ix += steps.size();
	return steps[ix].getSTM();
}//====================================================

/**
 *	@brief Retrieve the a pointer to the system data object associated with this arc
 *	@return a pointer to the system data object associated with this arc
 */
const tpat_sys_data* tpat_arc_data::getSysData() const { return sysData; }

/**
 *	@brief Retrieve the tolerance with which data in this object was computed
 *	@return the tolerance with which data in this object was computed
 */
double tpat_arc_data::getTol() const { return tol; }

/**
 *	@brief Append a step to the trajectory
 */
void tpat_arc_data::appendStep(tpat_arc_step s){ steps.push_back(s); }

/**
 *  @brief Set the acceleration vector for a specific step/node
 * 
 *  @param ix step index; if it is negative, the index will count backwards
 *  fro the end of the arc
 *  @param accelVec 3-element (at least) vector of non-dimensional acceleration 
 *  values (ax, ay, az, ...); only the first three are used
 */
void tpat_arc_data::setAccel(int ix, std::vector<double> accelVec){
	if(ix < 0)
		ix += steps.size();

	steps[ix].setAccel(accelVec);
}//=================================================

/**
 *  @brief Set the state vector for a specific step/node
 * 
 *  @param ix step index; if it is negative, the index will count backwards
 *  fro the end of the arc
 *  @param stateVec 6-element (at least) vector of non-dimensional state 
 *  values (x, y, z, vx, vy, vz, ...); only the first six are used
 */
void tpat_arc_data::setState(int ix, std::vector<double> stateVec){
	if(ix < 0)
		ix += steps.size();

	steps[ix].setPosVelState(stateVec);
}//=================================================

/**
 *  @brief Set the STM for a specific step/node
 * 
 *  @param ix step index; if it is negative, the index will count backwards
 *  fro the end of the arc
 *  @param stm a 6x6 matrix containing the STM
 */
void tpat_arc_data::setSTM(int ix, MatrixXRd stm){
	if(ix < 0)
		ix += steps.size();

	steps[ix].setSTM(stm);
}//=================================================

/**
 *	@brief Set the computational tolerance for this data object
 *	@param d the tolerance
 */
void tpat_arc_data::setTol(double d){ tol = d; }

//-----------------------------------------------------
//      Utility Functions
//-----------------------------------------------------

/**
 *	@brief Copy all data from the input arc data to this one
 *	@param d an arc data object reference
 */
void tpat_arc_data::copyMe(const tpat_arc_data &d){
	steps = d.steps;
	sysData = d.sysData; // Copying ADDRESS of sys_data object
	numExtraParam = d.numExtraParam;
	extraParamRowSize = d.extraParamRowSize;
	tol = d.tol;
}//====================================================

/**
 *	@brief Save the acceleration vector to file
 *	@param matFile a pointer to the destination mat-file
 */
void tpat_arc_data::saveAccel(mat_t *matFile) const{
	// We store data in row-major order, but the Matlab file-writing algorithm takes data
	// in column-major order, so we transpose our vector and split it into two smaller ones
	std::vector<double> accel_colMaj(3*steps.size());

	for(size_t r = 0; r < steps.size(); r++){
		std::vector<double> accel = steps[r].getAccel();
		for(int c = 0; c < 3; c++){
			accel_colMaj[c*steps.size() + r] = accel[c];
		}
	}
	
	size_t dims[2] = {steps.size(), 3};
	matvar_t *matvar = Mat_VarCreate("Accel", MAT_C_DOUBLE, MAT_T_DOUBLE, 2, dims, &(accel_colMaj[0]), MAT_F_DONT_COPY_DATA);
	saveVar(matFile, matvar, "Accel", MAT_COMPRESSION_NONE);
}//=====================================================

/**
 *	@brief Save one of the extra parameters to file
 *	@param matFile a pointer to the destination mat-file
 *	@param varIx the index of the parameter
 *	@param name the name of the variable being saved
 */
void tpat_arc_data::saveExtraParam(mat_t *matFile, int varIx, const char *name) const{
	if(varIx > numExtraParam || varIx < 0)
		throw tpat_exception("Could not save extra parameter; index out of bounds");

	// Get starting index of this extra param within a arc step's extra parameter vector
	int ix0 = 0;
	for(int i = 0; i < varIx; i++){ ix0 += extraParamRowSize[i]; }

	// Get the specified coordinate
	std::vector<double> param(extraParamRowSize[varIx]*steps.size());
	for(size_t r = 0; r < steps.size(); r++){
		std::vector<double> ep  = steps[r].getExtraParams();
		for(int c = 0; c < extraParamRowSize[varIx];c++){
			// Save NAN (rather than un-allocated memeory) if the index is out of bounds
			if(ix0 + c < (int)(ep.size()))
				param[c*steps.size() + r] = ep[ix0+c];
			else
				param[c*steps.size() + r] = NAN;
		}
	}

	size_t dims[2] = {steps.size(), static_cast<size_t>(extraParamRowSize[varIx])};
	matvar_t *matvar = Mat_VarCreate(name, MAT_C_DOUBLE, MAT_T_DOUBLE, 2, dims, &(param[0]), MAT_F_DONT_COPY_DATA);
	saveVar(matFile, matvar, name, MAT_COMPRESSION_NONE);
}//======================================================

/**
 *	@brief Save the state vector [pos, vel] to a file with variable name "State"
 *	@param matFile a pointer to the destination matlab file 
 */
void tpat_arc_data::saveState(mat_t *matFile) const{
	saveState(matFile, "State");
}//==================================================

/**
 *	@brief Save the state vector [pos, vel] to a file
 *	@param matFile a pointer to the destination matlab file 
 *	@param varName the name of the variable (e.g. "State" or "Nodes")
 */
void tpat_arc_data::saveState(mat_t *matFile, const char* varName) const{
	// We store data in row-major order, but the Matlab file-writing algorithm takes data
	// in column-major order, so we transpose our vector and split it into two smaller ones
	std::vector<double> posVel(6*steps.size());

	for(size_t r = 0; r < steps.size(); r++){
		std::vector<double> state = steps[r].getPosVelState();
		for(int c = 0; c < 6; c++){
			posVel[c*steps.size() + r] = state[c];
		}
	}

	// Next, create a matlab variable for the state and save it to the file
	/*	Create a matlab variable. Arguments are:
	 *	const char *name 	- varName, the name of the variable
	 *	enum matio_classes 	- MAT_C_DOUBLE, Matlab double-precision variable class
	 *	enum matio_types 	- MAT_T_DOUBLE, Matlab IEEE 754 double precision data type
	 * 	int rank 			- 2 - the variable rank. Must be 2 or more; not really sure what this does
	 *	size_t dims 		- dims - the dimensions of the variable (e.g. matrix size) {rows, cols}
	 *	void *data 			- data - the variable we're saving. The algorithm assumes data is in column-major 
	 *							format
	 *	int opt 			- 0, or bit-wise OR of the following options:
	 *							MAT_F_DONT_COPY_DATA: just use the pointer to the data, don't copy it. 
	 *								Note that the pointer should not be freed until you are done with 
	 *								the matvar. The Mat_VarFree function will NOT free data that was
	 *								created with MAT_F_DONT_COPY_DATA, so free it yourself.
	 *							MAT_F_COMPLEX: specify that the data is complex
	 *							MAT_F_GLOBAL: make the matlab variable global
	 *							MAT_F_LOGICAL: this variable is a logical variable
	 */
	size_t dims[2] = {steps.size(), 6};
	matvar_t *matvar = Mat_VarCreate(varName, MAT_C_DOUBLE, MAT_T_DOUBLE, 2, dims, &(posVel[0]), MAT_F_DONT_COPY_DATA);
	saveVar(matFile, matvar, varName, MAT_COMPRESSION_NONE);
}//======================================================

/**
 *	@brief Save the STMs to a file; STMs are stored in a 6x6xn array for 
 *	compatibility with existing MATLAB scripts
 *	@param matFile a pointer to the destination matlab file 
 */
void tpat_arc_data::saveSTMs(mat_t *matFile) const{
	// Create one large vector to put all the STM elements in
	std::vector<double> allSTMEl(steps.size()*36);
	for (size_t n = 0; n < steps.size(); n++){
		// get the transpose of the STM matrix; we need to store it in column-major order
		// and it's currently in row-major order
		MatrixXRd P = steps[n].getSTM().transpose();
		// Retrieve the data from the matrix
		double *matData = P.data();
		// Store that data in our huge vector
		std::copy(matData, matData+36, &(allSTMEl[0]) + n*36);
	}

	size_t dims[3] = {6, 6, steps.size()};
	matvar_t *matvar = Mat_VarCreate("STM", MAT_C_DOUBLE, MAT_T_DOUBLE, 3, dims, &(allSTMEl[0]), MAT_F_DONT_COPY_DATA);
	saveVar(matFile, matvar, "STM", MAT_COMPRESSION_NONE);
}//======================================================

/**
 *  @brief Initialize the vector of arc_step objects from a *.mat file
 *  @details THIS FUNCTION MUST BE THE FIRST READ_DATA-TYPE FUNCTION CALLED because
 *  it clears the steps vector and then initializes it by calculating the number
 *  of steps in the arc_data object from the state vector. Individual steps are
 *  able to be called by index after this, though they will not contain data
 *  until another function is called to populate the data fields with values from 
 *  the *.mat file
 * 
 *  @param matFile pointer to an open matlab data file
 *  @param varName the name of a variable that has as many rows as there are
 *  steps along the data object. Valid variables typically include the time vector,
 *  state matrix, or acceleration matrix
 */
void tpat_arc_data::initStepVectorFromMat(mat_t *matFile, const char* varName){
	matvar_t *stateMat = Mat_VarRead(matFile, varName);
	if(stateMat == NULL){
		throw tpat_exception("tpat_arc_data::initStepVectorFromMat: Could not read state data vector");
	}else{
		int numSteps = stateMat->dims[0];
		steps.clear();
		tpat_arc_step blankStep;
		steps.assign(numSteps, blankStep);	// Initialize array with a bunch of blank steps
	}
	Mat_VarFree(stateMat);
}//======================================================

/**
 *  @brief Read the state vector for this arc_data object from a matlab data file
 *  @details This function must be called after initStepVectorFromMat() as it
 *  populates the step vector objects with state data
 * 
 *  @param matFile pointer to an open matlab data file
 *  @param varName the name of the state variable (e.g., "State" or "Nodes")
 */
void tpat_arc_data::readStateFromMat(mat_t *matFile, const char* varName){
	matvar_t *stateMat = Mat_VarRead(matFile, varName);
	if(stateMat == NULL){
		throw tpat_exception("tpat_arc_data::readStateFromMat: Could not read state data vector");
	}else{
		int numSteps = stateMat->dims[0];
		
		if(steps.size() == 0){
			throw tpat_exception("tpat_arc_data::readStateFromMat: Step vector has not been initialized!");
		}

		if(numSteps != (int)steps.size()){
			throw tpat_exception("tpat_arc_data::readStateFromMat: State vector has a different size than the initialized step vector");
		}

		if(stateMat->dims[1] != 6){
			throw tpat_exception("tpat_arc_data::readStateFromMat: Incompatible data file: State width is not 6.");
		}

		if(stateMat->class_type == MAT_C_DOUBLE && stateMat->data_type == MAT_T_DOUBLE){
			double *data = static_cast<double *>(stateMat->data);

			if(data != NULL){
				for(int i = 0; i < numSteps; i++){
					double state[] = {0,0,0,0,0,0};
					state[0] = data[0*numSteps + i];
					state[1] = data[1*numSteps + i];
					state[2] = data[2*numSteps + i];
					state[3] = data[3*numSteps + i];
					state[4] = data[4*numSteps + i];
					state[5] = data[5*numSteps + i];

					steps[i].setPosVelState(state);
					// tpat_arc_step step;
					// step.setPosVelState(state);
					// steps.push_back(step);
				}
			}
		}else{
			throw tpat_exception("tpat_arc_data::readStateFromMat: Incompatible data file: unsupported data type/class");
		}
	}
	Mat_VarFree(stateMat);
}//===============================================

void tpat_arc_data::readAccelFromMat(mat_t *matFile){
	matvar_t *accelMat = Mat_VarRead(matFile, "Accel");
	if(accelMat == NULL){
		throw tpat_exception("tpat_arc_data::readAccelFromMat: Could not read data vector");
	}else{
		int numSteps = accelMat->dims[0];
		
		if(steps.size() == 0){
			throw tpat_exception("tpat_arc_data::readAccelFromMat: Step vector has not been initialized!");
		}

		if(numSteps != (int)steps.size()){
			throw tpat_exception("tpat_arc_data::readAccelFromMat: Accel vector has a different size than the initialized step vector");
		}

		if(accelMat->dims[1] != 3){
			throw tpat_exception("tpat_arc_data::readAccelFromMat: Incompatible data file: Accel width is not 3.");
		}

		if(accelMat->class_type == MAT_C_DOUBLE && accelMat->data_type == MAT_T_DOUBLE){
			double *data = static_cast<double *>(accelMat->data);

			if(data != NULL){
				for(int i = 0; i < numSteps; i++){
					double accel[] = {0,0,0};
					accel[0] = data[0*numSteps + i];
					accel[1] = data[1*numSteps + i];
					accel[2] = data[2*numSteps + i];

					steps[i].setAccel(accel);
				}
			}
		}else{
			throw tpat_exception("tpat_arc_data::readAccelFromMat: Incompatible data file: unsupported data type/class");
		}
	}
	Mat_VarFree(accelMat);
}//===============================================

void tpat_arc_data::readSTMFromMat(mat_t *matFile){
	matvar_t *allSTM = Mat_VarRead(matFile, "STM");
	if(allSTM == NULL){
		throw tpat_exception("tpat_arc_data::readSTMFromMat: Could not read data vector");
	}else{
		int numSteps = allSTM->dims[2];

		if(steps.size() == 0){
			throw tpat_exception("tpat_arc_data::readSTMFromMat: Step vector has not been initialized!");
		}

		if(numSteps != (int)steps.size()){
			throw tpat_exception("tpat_arc_data::readSTMFromMat: STM vector has a different size than the initialized step vector");
		}

		if(allSTM->dims[0] != 6 || allSTM->dims[1] != 6){
			throw tpat_exception("tpat_arc_data::readSTMFromMat: Incompatible data file: STM is not 6x6.");
		}

		if(allSTM->class_type == MAT_C_DOUBLE && allSTM->data_type == MAT_T_DOUBLE){
			double *data = static_cast<double *>(allSTM->data);

			if(data != NULL){
				for(int i = 0; i < numSteps; i++){
					double stmEl[36];
					for(int j = 0; j < 36; j++){
						stmEl[j] = data[36*i + j];
					}

					MatrixXRd P = Eigen::Map<MatrixXRd>(stmEl, 6, 6);
					steps[i].setSTM(P.transpose());
				}
			}
		}else{
			throw tpat_exception("ttpat_arc_data::readSTMFromMat: Incompatible data file: unsupported data type/class");
		}
	}
	Mat_VarFree(allSTM);
}//===============================================

void tpat_arc_data::readExtraParamFromMat(mat_t *matFile, int varIx, const char *varName){
	if(varIx > numExtraParam || varIx < 0)
		throw tpat_exception("tpat_arc_data::readExtraParamFromMat: Could not read extra parameter; index out of bounds");

	// Get starting index of this extra param within a arc step's extra parameter vector
	int ix0 = 0;
	for(int i = 0; i < varIx; i++){ ix0 += extraParamRowSize[i]; }

	matvar_t *matvar = Mat_VarRead(matFile, varName);
	if(matvar == NULL){
		throw tpat_exception("tpat_arc_data::readExtraParamFromMat: Could not read data vector");
	}else{
		int numSteps = matvar->dims[0];
		
		if(steps.size() == 0){
			throw tpat_exception("tpat_arc_data::readExtraParamFromMat: Step vector has not been initialized!");
		}

		if(matvar->dims[1] != ((size_t)extraParamRowSize[varIx])){
			char message[64];
			sprintf(message, "tpat_arc_data::readExtraParamFromMat: Incompatible data file: %s width is not %d", varName, extraParamRowSize[varIx]);
			throw tpat_exception(message);
		}

		if(matvar->class_type == MAT_C_DOUBLE && matvar->data_type == MAT_T_DOUBLE){
			double *data = static_cast<double *>(matvar->data);
			if(data != NULL){
				for(int i = 0; i < numSteps; i++){
					for(int c = 0; c < extraParamRowSize[varIx]; c++){
						steps[i].setExtraParam(ix0+c, data[c*numSteps + i]);
					}
				}
			}
		}else{
			throw tpat_exception("tpat_arc_data::readExtraParamFromMat: Incompatible data file: unsupported data type/class");
		}
	}
	Mat_VarFree(matvar);
}//===============================================

/**
 *	@brief Update the constraints for every node so that their node numberes
 * 	match the node/step they belong to.
 */
void tpat_arc_data::updateCons(){
	for(size_t n = 0; n < steps.size(); n++){
		std::vector<tpat_constraint> nodeCons = steps[n].getConstraints();
		for(size_t c = 0; c < nodeCons.size(); c++){
			nodeCons[c].setNode(n);
		}
		steps[n].setConstraints(nodeCons);
	}
}//====================================================





