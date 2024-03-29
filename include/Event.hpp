/**
 *  @file Event.hpp
 *	@brief Class that contains information about a simulation event
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

#include "Core.hpp"
 
#include "Constraint.hpp"
#include "SysData.hpp"

#include <vector>

namespace astrohelion{

/**
 *	@brief The type of event
 *
 *	This tells the simulation and correction engines how to interpret the
 *	data stored in this object.
 */
enum class Event_tp : int{
	NONE 			= 0,	/*!< Default NULL value cannot be used in integration */
	SIM_TOF 		= -1,	/*!< SIMULATION-GENERATED ONLY. Marks a time-of-flight 
							 * event. This is the default event that ends most 
							 * propagations
							 */
	SIM_COMPTIME	= -2,	/*!< SIMULATION-GENERATED ONLY. Marks a computation 
							 * time-out event. This event is created when a 
							 * maximum computation time is specified in the 
							 * SimEngine.
							 */
	SIM_ERR 		= -3,	/*!< SIMULATION-GENERATED ONLY. Marks an error that 
							 * kills the integration. This event is created 
							 * automatically by the SimEngine
							 */
	YZ_PLANE 		= 1,	/*!< occurs when trajectory crosses an YZ-plane. By 
							 * default, this plane occurs at x = 0, but a custom 
							 * x-coordinate may be specified by placing the 
							 * value of that coordinate in the first `param` 
							 * array entry.
							 */
	XZ_PLANE		= 2,	/*!< occurs when trajectory crosses an XZ-plane. By 
							 * default, this plane occurs at y = 0, but a custom 
							 * y-coordinate may be specified by placing the 
							 * value of that coordinate in the first `param` 
							 * array entry.
							 */
	XY_PLANE		= 3,	/*!< Event occurs when trajectory crosses an XY-plane.
							 * By default, this plane occurs at z = 0, but a 
							 * custom z-coordinate may be specified by placing 
							 * the value of that coordinate in the first `param`
							 * array entry.
							 */
	STATE_PLANE 	= 4,	/*!< occurs when a trajectory crosses a specific 
							 * state value. To specify the stopping condition, 
							 * place the index of the variable in the first 
							 * `param` entry and the value of the variable in the
							 * second `param` entry. For example, if 
							 * `param` = [3, -0.1], the event will fire when the 
							 * trajectory passes a state(3) (usually dx/dt)
							 * 	value of -0.1.
							 */
	CRASH			= 5,	/*!< occurs when trajectory falls below minimum 
							 * acceptable altitude or the surface of one of the 
							 * system primaries. The `param` array should have 
							 * the first element specifying the primary index 
							 * (0 for P1, 1 for P2, etc.) The minimum acceptable 
							 * radius will be the radius of the primary plus the 
							 * minimum acceptable fly-by distance specified in 
							 * the BodyData class.
		 					 */
 	JC 				= 6, 	/*!< occurs when the Jacobi value reaches the 
 							 * specified value of Jacobi Constant. Place this 
 							 * JC value in the first element of the `params` 
 							 * vector present in the 
 							 * Event(SysData*, Event_tp, int, bool, double*) 
 							 * constructor. This event can only be supported by 
 							 * dynamic models that have associated system data 
 							 * objects that can be cast to cr3bp system data 
 							 * objects.
		 					 */
 	APSE 			= 7,	/*!< occurs when an apse is reached. The `param` 
 							 * array should have the first element specifiying 
 							 * the primary index (0 for P1, 1 for P2, etc.). The 
 							 * `direction` of the event identifies what type of 
 							 * apse, i.e. 0 will catch all apsides, -1 will 
 							 * catch only apopases, and +1 will catch  only 
 							 * periapses
		 					 */
 	DIST 			= 8, 	/*!< occurs when a distance from a primary is 
 							 * reached. The `param` array should have two 
 							 * elements: element 0 specifies the primary index, 
 							 * and element 1 specifies the distance from the 
 							 * center of the primary in non-dimensional units. 
 							 * The `direction` of the event identifies the 
 							 * direction of travel relative to the primary; +1 
 							 * is away from the primary, -1 is towards the 
 							 * primary, and 0 triggers for either.
		 					 */
	MASS 			= 9,	/*!< occurs when the s/c mass reaches a minimum or 
							 * maximum amount. The `param` array should have one 
							 * element: the nondimensional mass limit (between 0 
							 * and 1).
						 	 */
	ANGLE_PLANE_P1 	= 10	/*!< Occurs when the s/c reaches a plane normal to 
							 * the XY-plane, rotated by a specific angle about 
							 * P1. The `param` array should have one element: 
							 * the angle of the plane relative to the positive 
							 * x-axis.
							 */
};

		
/**
 *	@brief A data object containing information about an event that may
 *	occur during simulation
 *
 *	**Adding a New Event**
 *	* Add a new enumerated type value with detailed documentation of how it will
 *		be parsed.
 * 	* Update the initEvent() function
 *	* Update the getConTypeStr() function
 *	* Define or identify a constraint type that can locate this event to a high
 *		degree of accuracy.
 *	* Update the getDist() function
 * 	* Add event to list of allowed constraints in relevant DynamicalModel derived classes
 *
 *	@author Andrew Cox
 *	@version Mar 7, 2017
 *	@copyright GNU GPL v3.0
 */
class Event : public Core{
	public:
		/**
		 *  \name *structors
		 *  \{
		 */
		Event();
		Event(Event_tp, int, bool);
		Event(Event_tp, int, bool, std::vector<double>);
		Event(const Event&);
		void createEvent(Event_tp, int, bool);
		void createEvent(Event_tp, int, bool, std::vector<double>);
		void initialize(const SysData*);
		~Event();
		//\}

		/**
		 *  \name Operators
		 *  \{
		 */
		Event& operator =(const Event&);
		friend bool operator ==(const Event&, const Event&);
		friend bool operator !=(const Event&, const Event&);
		//\}

		/**
		 *  \name Set and Get Functions
		 *  \{
		 */
		std::vector<double> getConData() const;
		Constraint_tp getConType() const;
		static const char* getEventTpStr(Event_tp);
		int getTriggerDir() const;
		int getStopCount() const;
		int getTriggerCount() const;
		Event_tp getType() const;
		const char* getTypeStr() const;
		const SysData* getSysData();

		void setTriggerDir(int);
		void setStopCount(int);
		void setStopOnEvent(bool);

		bool stopOnEvent() const;
		//\}

		/**
		 *  \name Analysis Functions
		 *  \{
		 */
		bool crossedEvent(const double*, unsigned int, double, int) const;
		void incrementCount();
		void reset();
		void updateDist(const double*, unsigned int, double);
		//\}

		/**
		 *  \name Utility Functions
		 *  \{
		 */
		void print() const;
		//\}
	private:

		Event_tp type = Event_tp::NONE; //!< The type of event this is

		/** Direction of desired event crossing: +1 for positive, -1 for negative, 0 for both */
		int triggerDir = 0;

		/** Number of times this event has been triggered */
		int triggerCount = 0;

		/** The number of times this event can be triggered before the simulation
		 	is stopped (if applicable, i.e. stop = true)*/
		int stopCount = 1;

		/** Whether or not to stop integration when this event occurs */
		bool bStop = true;

		double dist = 100000;		//!< Distance to the event; must be able to change sign
		double lastDist = 100000; 	//!< distance to event at previous iteration

		/** Type of constraint used by the shooting algorithm to locate this event */
		Constraint_tp conType = Constraint_tp::NONE;

		/** Data for the constraint used by the shooting algorithm to locate this event */
		std::vector<double> conData {};

		/** Storage vector for data passed to the event before it is manipulated and placed in a constraint object */
		std::vector<double> paramsIn {};
		
		SysData* pSysData = nullptr; 	//!< Copy of the system data pointer

		/**
		 *  \name Utility Functions
		 *  \{
		 */
		void copyMe(const Event&);
		//\}

		/**
		 *  \name *structors
		 *  \{
		 */
		void initEvent(Event_tp, int, bool, std::vector<double>);
		//\}

		/**
		 *  \name Analysis Functions
		 *  \{
		 */
		double getDist(const double*, unsigned int, double) const;
		int getDir(int) const;
		//\}
};

}