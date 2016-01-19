/*
 *	Trajectory Propagation and Analysis Toolkit 
 *	Copyright 2015, Andrew Cox; Protected under the GNU GPL v3.0
 *	
 *	This file is part of the Trajectory Propagation and Analysis Toolkit (TPAT).
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
#ifndef H_MODEL_CR3BP
#define H_MODEL_CR3BP

#include "tpat_model.hpp"

/**
 *	@brief Derivative of tpat_model, specific to the CR3BP
 *
 *	The base class's methods provide a good framework for this system,
 *	so only minimal adjustments are needed. This model adds support for 
 *	Jacobi-Constant targeting.
 */
class tpat_model_cr3bp : public tpat_model{
public:
	tpat_model_cr3bp();
	tpat_model_cr3bp(const tpat_model_cr3bp&);
	~tpat_model_cr3bp() {}

	tpat_model_cr3bp& operator=(const tpat_model_cr3bp&);

	// Core Functions
	tpat_model::eom_fcn getFullEOM_fcn() const;
	tpat_model::eom_fcn getSimpleEOM_fcn() const;
	std::vector<double> getPrimPos(double, const tpat_sys_data*) const;
	std::vector<double> getPrimVel(double, const tpat_sys_data*) const;
	void sim_saveIntegratedData(const double*, double, tpat_traj*) const;
	bool sim_locateEvent(tpat_event, tpat_traj*, const double*, double, double, verbosity_t) const;

	// Multiple shooting Functions
	void multShoot_applyConstraint(iterationData*, tpat_constraint, int) const;
	tpat_nodeset* multShoot_createOutput(const iterationData*, const tpat_nodeset*, bool) const;

protected:
	void multShoot_targetJC(iterationData*, tpat_constraint, int) const;
	void multShoot_targetPseudoArc(iterationData*, tpat_constraint, int) const;
};

#endif