/**
 * @file ControlLaw_cr3bp_lt.hpp
 * @brief Control Law for CR3BP-LT system header file 
 * 
 * @author Andrew Cox
 * @version March 3, 2017
 * @copyright GNU GPL v3.0
 */
 
/*
 *	Astrohelion 
 *	Copyright 2015-2017, Andrew Cox; Protected under the GNU GPL v3.0
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
 
#include "ControlLaw_cr3bp_lt.hpp"

#include <cmath>

#include "Exceptions.hpp"

namespace astrohelion{

ControlLaw_cr3bp_lt::ControlLaw_cr3bp_lt(){}

void ControlLaw_cr3bp_lt::getLaw(double t, const double *s, const SysData *sysData, unsigned int lawID,
	double *law, unsigned int len) const{

	switch(lawID){
		case Law_tp::CONST_C_2D_LEFT:
			getLaw_ConstC_2D_Left(t, s, sysData, law, len);
			break;
		case Law_tp::CONST_C_2D_RIGHT:
			getLaw_ConstC_2D_Right(t, s, sysData, law, len);
			break;
		case Law_tp::PRO_VEL:
			getLaw_Pro_Vel(t, s, sysData, law, len);
			break;
		case Law_tp::ANTI_VEL:
			getLaw_Anti_Vel(t, s, sysData, law, len);
			break;
		default:
			ControlLaw::getLaw(t, s, sysData, lawID, law, len);
	}
}//====================================================

void ControlLaw_cr3bp_lt::getLaw_ConstC_2D_Right(double t, const double *s, const SysData *pSys,
	double *law, unsigned int len) const{

	if(len < 3)
		throw Exception("ControlLaw_cr3bp_lt::getLaw_ConstC_2D_Right: law data length must be at least 3!");

	double v = sqrt(s[3]*s[3] + s[4]*s[4] + s[5]*s[5]);
	law[0] = s[4]/v;
	law[1] = -s[3]/v;
	law[2] = 0;

	(void) pSys;
	(void) t;
}//====================================================

void ControlLaw_cr3bp_lt::getLaw_ConstC_2D_Left(double t, const double *s, const SysData *pSys,
	double *law, unsigned int len) const{

	if(len < 3)
		throw Exception("ControlLaw_cr3bp_lt::getLaw_ConstC_2D_Left: law data length must be at least 3!");

	double v = sqrt(s[3]*s[3] + s[4]*s[4] + s[5]*s[5]);
	law[0] = -s[4]/v;
	law[1] = s[3]/v;
	law[2] = 0;

	(void) pSys;
	(void) t;
}//====================================================

void ControlLaw_cr3bp_lt::getLaw_Pro_Vel(double t, const double *s, const SysData *pSys,
	double *law, unsigned int len) const{

	if(len < 3)
		throw Exception("ControlLaw_cr3bp_lt::getLaw_Pro_Vel: law data length must be at least 3!");

	double v = sqrt(s[3]*s[3] + s[4]*s[4] + s[5]*s[5]);
	law[0] = s[3]/v;
	law[1] = s[4]/v;
	law[2] = s[5]/v;

	(void) pSys;
	(void) t;
}//====================================================

void ControlLaw_cr3bp_lt::getLaw_Anti_Vel(double t, const double *s, const SysData *pSys,
	double *law, unsigned int len) const{

	if(len < 3)
		throw Exception("ControlLaw_cr3bp_lt::getLaw_Anti_Vel: law data length must be at least 3!");

	double v = sqrt(s[3]*s[3] + s[4]*s[4] + s[5]*s[5]);
	law[0] = -s[3]/v;
	law[1] = -s[4]/v;
	law[2] = -s[5]/v;

	(void) pSys;
	(void) t;
}//====================================================

}
