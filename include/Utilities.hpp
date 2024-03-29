/**
 *	@file Utilities.hpp
 *	@brief Contains miscellaneous utility functions that make 
 *	coding in C++ easier
 *
 *	@author Andrew Cox
 *	@version May 25, 2016
 *	@copyright GNU GPL v3.0
 */

/*
 *	Astrohelion 
 *	Copyright 2015-2018, Andrew Cox; Protected under the GNU GPL v3.0
 *	
 *	This file is part of Astrohelion
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

#pragma once

#include "Common.hpp"
#include "EigenDefs.hpp"

#include <cspice/SpiceZdf.h>	// typedefs for SPICE objects, like SpiceDouble 
#include "matio.h"
 
#include <algorithm>
#include <complex>
#include <fstream>
#include <iostream>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace astrohelion{
	/**
	 * @addtogroup util
	 * \{
	 */


	/**
	 *	@brief Worker function to compute permutations via recursion
	 *	@param values a vector of all possible values for each element
	 *	@param numSpots the number of elements, or "spots"
	 *	@param ixs the indices of values for each element in the current permuation
	 *	@param perms a pointer to a storage vector for the permutations; stored in 
	 *	row-major order
	 */
	template <typename T>
	void permute(std::vector<T> values, unsigned int numSpots, std::vector<unsigned int> ixs, std::vector<T> *perms){
		if(ixs.size() < numSpots){	// Not done with recursion; continue deeper!
			ixs.push_back(-1);
			for(unsigned int i = 0; i < values.size(); i++){
				ixs.back() = i;
				permute(values, numSpots, ixs, perms);
			}
		}else{ // We've reached the bottom, compute the permuation and return
			for(unsigned int i = 0; i < numSpots; i++){
				perms->push_back(values[ixs[i]]);
			}
			return;
		}
	}//==============================================

	/**
	 *	@brief Worker function to compute permutations via recursion without repeating values
	 *	@param values a vector of all possible values
	 *	@param ixs the indices of values for each element in the current permutation
	 *	@param perms a pointer to a storage vector for the permutations; stored in row-major order
	 */
	template <typename T>
	void permute(std::vector<T> values, std::vector<unsigned int> ixs, std::vector<T> *perms){
		if(ixs.size() < values.size()){
			ixs.push_back(-1);
			for(unsigned int i = 0; i < values.size(); i++){
				bool bAlreadyUsed = false;
				for(unsigned int j = 0; j < ixs.size(); j++){
					if(ixs[j] == i){
						bAlreadyUsed = true;
						break;
					}
				}

				if(!bAlreadyUsed){
					ixs.back() = i;
					permute(values, ixs, perms);
				}
			}
		}else{
			for(unsigned int i = 0; i < ixs.size(); i++){
				perms->push_back(values[ixs[i]]);
			}
			return;
		}
	}//==========================================

	/**
	 *	@brief Check if two numbers are equal within a given tolerance
	 *	@param t1 a number
	 *	@param t2 another number
	 *	@param tol the tolerance
	 *	@return true if the absolute value of the difference between t1 
	 *	and t2 is less than the tolerance
	 */
	template <typename T>
	bool aboutEquals(T t1, T t2, double tol){
		return std::abs(t1 - t2) < tol;
	}//=========================================

	/**
	 *	@ brief Check if two vectors are equal to a given tolerance
	 *	@param v1 a vector
	 *	@param v2 another vector
	 *	@param tol the tolerance
	 *	@return true if v1 and v2 are the same size and their corresponding
	 *	elements differ by less than the tolerance
	 */
	template <typename T>
	bool aboutEquals(std::vector<T> v1, std::vector<T> v2, double tol){
		if(v1.size() != v2.size())
			return false;

		for(unsigned int i = 0; i < v1.size(); i++){
			if(!aboutEquals(v1[i], v2[i], tol))
				return false;
		}

		return true;
	}//==========================================

	/**
	 *	@brief concatenate two vectors
	 *	@param lhs the left-hand-side vector
	 *	@param rhs the righ-hand-side vector
	 *	@return a new vector that includes both input vectors in order, i.e. [lhs, rhs]
	 */
	template<class T>
	std::vector<T> concatVecs(std::vector<T> lhs, std::vector<T> rhs){
		std::vector<T> tempVec(lhs.begin(), lhs.end());
		tempVec.insert(tempVec.end(), rhs.begin(), rhs.end());
		return tempVec;
	}//=======================================

	/**
	 *  @brief Create an Identity matrix (e.g., to act as a dummy value for an STM)
	 * 
	 *  @param matRef reference to a vector that stores matrix elements. Any nonzero elements
	 *  are overwritten
	 *  @param size side length of the matrix (e.g., the size of a 6x6 matrix is 6)
	 */
	template<class T>
	void createIdentity(std::vector<T> &matRef, unsigned int size){
	    matRef.assign(size*size, 0);
	    for(unsigned int i = 0; i < size; i++)
	        matRef[i*(size+1)] = 1;
	}//=======================================================

	/**
	 *	@brief sort a vector and retrieve the indices of the sorted elements
	 *
	 *	Takes a copy of a vector and sorts it, retaining the original indices
	 *	of the elements. For example, sorting {1,3,2} would return the indices 
	 *	{0,2,1}.
	 *
	 *	@param v a vector to sort
	 *	@return the indices of the sorted elements
	 */
	template <typename T>
	std::vector<int> getSortedInd(const std::vector<T> &v) {

	  	// initialize original index locations
	  	std::vector<int> idx(v.size());
	  	for (unsigned int i = 0; i < idx.size(); i++){
			idx[i] = static_cast<int>(i);
	  	}

	  	// sort indexes based on comparing values in v
	  	std::sort(idx.begin(), idx.end(), 
	  		[&v](int i1, int i2) {return v[i1] < v[i2];} );

	  	return idx; 
	}//===========================================

	/**
	 *	@brief Generate all permutations of a set of n elements
	 *
	 *	Each of the n elements can contain any of the values stored in the values vector
	 *	@param values a vector containing all possible values for each element
	 *	@param n the number of elements in the permutation
	 *	
	 *	@return a vector containing all permutations, in row-major order where each row
	 *	is a separate permutation
	 */
	template <typename T>
	std::vector<T> generatePerms(std::vector<T> values, unsigned int n){
		std::vector<unsigned int> ixs;
		std::vector<T> perms;
		permute(values, n, ixs, &perms);
		return perms;
	}//=========================================

	/**
	 *	@brief Generate all permutations of a set of values without repeating the values
	 *	@param values the set of values
	 *	@return a vector containing all permutations, in row-major order where each row
	 *	is a seperate permutation of `values`
	 */	
	template <typename T>
	std::vector<T> generatePerms(std::vector<T> values){
		std::vector<unsigned int> ixs;
		std::vector<T> perms;
		permute(values, ixs, &perms);
		return perms;
	}

	/**
	 *	@brief Get the imaginary parts of every element of a vector
	 *	@param compVec a vector of complex numbers
	 *	@return a vector of the imaginary parts of the complex vector
	 */
	template<typename T>
	std::vector<T> imag(std::vector<std::complex<T> > compVec){
		std::vector<T> imagParts;
		for(unsigned int i = 0; i < compVec.size(); i++)
			imagParts.push_back(std::imag(compVec[i]));

		return imagParts;
	}//================================================

	/**
	 *	@brief sum all values in an array
	 *
	 *	For this function to work, the class must overload +=
	 *
	 *	@param data an array
	 *	@param length the number of elements in the array that can be summed.
	 *	@return a single object representing the sum of all the elements of data.
	 */
	template<typename T>
	T sum(T* data, int length){
		T total = 0;
		for(int n = 0; n < length; n++){
			total += data[n];
		}

		return total;
	}//=====================================

	/**
	 *  @brief Sum all values in a vector
	 * 
	 *  @param data a vector of data
	 *  @tparam T numerical data type, like int, double, long, etc.
	 *  @return the sum
	 */
	template<typename T>
	T sum(std::vector<T> data){
		return sum(&(data[0]), data.size());
	}//==============================================
	
	/**
	 *	@brief Compute the mean (average) of an array of data
	 *
	 *	@param data an array of numbers
	 *	@param length the length of the array
	 *
	 *	@return the mean
	 */
	template<typename T>
	T mean(T *data, int length){
		return sum(data, length)/length;
	}//================================================

	/**
	 *  @brief Compute the mean (average) of a vector of data
	 * 
	 *  @param data a vector of numbers
	 *  @tparam T a numerical type, like int, double, long, etc.
	 *  @return the mean
	 */
	template<typename T>
	T mean(std::vector<T> data){
		return mean(&(data[0]), data.size());
	}//================================================

	/**
	 *	@brief Get the real parts of every element of a vector
	 *	@param compVec a vector of complex numbers
	 *	@return a vector of the real parts of the complex vector
	 */
	template<typename T>
	std::vector<T> real(std::vector<std::complex<T> > compVec){
		std::vector<T> realParts;
		for(unsigned int i = 0; i < compVec.size(); i++)
			realParts.push_back(std::real(compVec[i]));

		return realParts;
	}//================================================

	/**
	 *	@brief Get the sign of a number
	 *	@param num a number
	 *	@return +/- 1 for the sign (0 if T = 0)
	 */
	template<typename T>
	int sign(T num){
		if(num == 0)
			return 0;
		else
			return num < 0 ? -1 : 1;
	}//================================================

	/**
	 *  @brief Compare two complex numbers by comparing their magnitudes
	 * 
	 *  @param lhs 
	 *  @param rhs 
	 *  @return whether or not the magnitude of lhs is less than the magnitude of rhs
	 */
	template<typename T>
	bool compareMagnitude(std::complex<T> lhs, std::complex<T> rhs){
		return std::abs(lhs) < std::abs(rhs);
	}//================================================

	/**
	 *  @brief Explicitly cast a strongly typed enumerate type to its underlying type
	 *  @details For example, if an enum is declared as <code>enum class tp : int</code>,
	 *  this function will cast any <code>tp::value</code> class to the underlying type,
	 *  <code>int</code>. This is useful if you want to compare <code>int</code> values 
	 *  with the enum values.
	 * 
	 *  @param e Object to retrieve the underlying type of
	 *  @return The underlying type of e.
	 */
	template<typename T>
	constexpr auto to_underlying(T e) -> typename std::underlying_type<T>::type{
		return static_cast<typename std::underlying_type<T>::type>(e);
	}//================================================

	template<typename T>
	void toCSV(const Eigen::Matrix<T, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>& m, const char* filename){
		std::ofstream outFile(filename, std::ios::out);
    
	    // After this attempt to open a file, we can safely use perror() only  
	    // in case f.is_open() returns False.
	    if (!outFile.is_open())
	        perror("Utilities::toCSV: Error while opening file");
	    
	    bool isComplex = std::is_same<T, std::complex<double> >::value ||
	    				std::is_same<T, std::complex<float> >::value;

	    for (int r = 0; r < m.rows(); r++){
	        for (int c = 0; c < m.cols(); c++){
	            char buffer[64] = "";

	            if(isComplex){
	            	snprintf(buffer, 64, "%.20f%c%.20fi", std::real(m(r,c)),
	            		std::imag(m(r,c)) >= 0.f ? '+':'-', std::abs(std::imag(m(r,c))));
	            }else{
	            	snprintf(buffer, 64, "%.20f", m(r,c));
	            }

	            outFile << buffer;
	            if(c < m.cols()-1)
	            	outFile << ", ";
	            else
	            	outFile << '\n';
	        }
	    }

	    // Only in case of set badbit we are sure that errno has been set in
	    // the current context. Use perror() to print error details.
	    if (outFile.bad())
	        perror("Utilities::toCSV: Error while writing file ");

	    outFile.close();
	}//================================================

	/**
	 * @brief Define a type that stores an index and a value
	 * as a pair. This is used in parallel sorting to return 
	 * both the minimum value and the associated index. See
	 * Calculations::sortEig() for an example use case.
	 */
	typedef std::pair<unsigned int, double> IndexValuePair;

	

	/** \} */ // END of util group


	double getCPUTime();
	double boundValue(double, double, double);
	void checkAndReThrowSpiceErr(const char*);
	std::string complexToStr(std::complex<double>);
	std::string eigenCompInfo2Str(Eigen::ComputationInfo);
	std::string getNameFromSpiceID(int);
	SpiceInt getSpiceIDFromName(const char*);
	IndexValuePair minVal(IndexValuePair, IndexValuePair);

	/**
	 *  \name Standard Output
	 *  \{
	 */
	bool isColorOn();
	int printf(const char*, ...);
	int printErr(const char*, ...);
	int printWarn(const char*, ...);
	int printVerb(bool, const char*, ...);
	int printColor(const char*, const char*, ...);
	int printVerbColor(bool, const char*, const char*, ...);
	//\}

	/**
	 *  \name File I/O
	 *  \{
	 */
	//int readIntFromMat(mat_t*, const char*, matio_types, matio_classes);
	double readDoubleFromMat(mat_t*, const char*);
	MatrixXRd readMatrixFromMat(const char*, const char*);
	std::string readStringFromMat(mat_t*, const char* , matio_types, matio_classes);
	void saveDoubleToFile(mat_t*, const char*, double);
	void saveStringToFile(mat_t*, const char*, std::string, const int);
	void saveMatrixToFile(const char*, const char*, const std::vector<double>&, size_t, size_t);
	void saveMatrixToFile(mat_t*, const char*, const std::vector<double>&, size_t, size_t);
	void saveTimestampToFile(mat_t*, const char *varName = VARNAME_TIMESTAMP);
	void saveVar(mat_t*, matvar_t*, const char*, matio_compression);
	bool fileExists (const char*);
	//\}
	
	double resolveAngle(double, double);
	double wrapToPi(double);
	
	void waitForUser();
}
