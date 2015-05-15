/**
 *	The body data object provides a way to store and retrieve information about different
 *	celestial bodies. This may be supplemented/usurped by calls to SPICE in the future.
 *
 *	Author: Andrew Cox
 *
 *	Version: May 15, 2015
 */

/*
 *	Astrodynamics Toolkit 
 *	Copyright 2015, Andrew Cox; Protected under the GNU GPL v3.0
 *	
 *	This file is part of the Astrodynamics Toolkit (ADTK).
 *
 *  ADTK is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  ADTK is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with ATDK.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef __H_BODYDATA__
#define __H_BODYDATA__

#include <string>

class adtk_body_data{
	private:
		/** Mean radius of the body, km */
		double radius;

		/** Mass of the body, kg */
		double mass;

		/** Mean orbital radius (distance from parent body), km*/
		double orbitRad;

		/** Gravitational parameter associated with this body, kg^3/s^2 */
		double gravParam;

		/** Minmum acceptable fly-by altitude for this body. Altitudes lower than this will
		trigger a crash event in the numerical simulation */
		double minFlyByAlt;

		/** Name of this body */
		std::string name;

		/** Name of the parent body */
		std::string parent;

	public:

		// Constructors
		adtk_body_data();
		adtk_body_data(std::string);
		adtk_body_data(double m, double R, double r, double mu, std::string name, std::string parent);

		// Set and Get Functions
		double getRadius();
		double getMass();
		double getGravParam();
		double getOrbitRad();
		double getMinFlyBy();
		std::string getName();
		std::string getParent();

		void setRadius(double);
		void setMass(double);
		void setOrbitRad(double);
		void setGravParam(double);
		void setName(std::string);
		void setParent(std::string);
};

#endif
//END