/**
 *  @file BaseArcset.cpp
 *	@brief Data object that stores information about an integrated arc
 *	
 *	@author Andrew Cox
 *	@version May 25, 2016
 *	@copyright GNU GPL v3.0
 */
 
/*
 *  Astrohelion 
 *  Copyright 2015, Andrew Cox; Protected under the GNU GPL v3.0
 *  
 *  This file is part of the Astrohelion.
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

#include "BaseArcset.hpp"

#include "AsciiOutput.hpp"
#include "EigenDefs.hpp"
#include "SysData.hpp"
#include "Exceptions.hpp"
#include "Utilities.hpp"

#include <algorithm>

namespace astrohelion{

//-----------------------------------------------------
//      *structors
//-----------------------------------------------------

/**
 *	@brief Constructor (requires system data object)
 *	@param sys a pointer to a system data object that describes
 *	the system this trajectory is integrated in
 */
BaseArcset::BaseArcset(const SysData *sys) : sysData(sys){}

/**
 *	@brief Copy constructor
 *	@param d an arcset reference
 */
BaseArcset::BaseArcset(const BaseArcset &d) : sysData(d.sysData){
	copyMe(d);
}//====================================================

/**
 *	@brief Destructor
 */
BaseArcset::~BaseArcset(){}

//-----------------------------------------------------
//      Operators
//-----------------------------------------------------

/**
 *	@brief Set this object equal to another
 *	@param d an arcset reference
 *	@return a reference to this arcset object
 */
BaseArcset& BaseArcset::operator =(const BaseArcset &d){
	copyMe(d);
	sysData = d.sysData;
	return *this;
}//====================================================

/**
 *  @brief Sum two arcset objects.
 *  @details This function returns <tt>result</tt> = <tt>lhs</tt> + <tt>rhs</tt>; 
 *  Both <tt>lhs</tt> and <tt>rhs</tt> are copied and sorted into chronological 
 *  order. The <tt>rhs</tt> is then appended to the end of <tt>lhs</tt> with no
 *  time-of-flight between the end of <tt>lhs</tt> and the beginning of <tt>rhs</tt>.
 *  This will result in the deletion of the first node of <tt>rhs</tt> in the 
 *  concatenated set. To avoid losing data, make sure the first node of <tt>rhs</tt>
 *  is equivalent to the final node (chronologically) of <tt>lhs</tt>.
 * 
 *  @param lhs pointer to an arcset object
 *  @param rhs pointer to an arcset object
 *  @param result a pointer to the an arcset object that will store the result of
 *  the summing operation.
 */
void BaseArcset::sum(const BaseArcset *lhs, const BaseArcset *rhs, BaseArcset *result){
	baseArcsetPtr lhs_cpy = lhs->clone();
	baseArcsetPtr rhs_cpy = rhs->clone();

	lhs_cpy->putInChronoOrder();
	rhs_cpy->putInChronoOrder();
	int lhs_lastNodeID = lhs_cpy->getNodeByIx(-1).getID();
	int rhs_firstNodeID = rhs_cpy->getNodeByIx(0).getID();

	*result = *lhs_cpy;

	result->appendSetAtNode(rhs_cpy.get(), lhs_lastNodeID, rhs_firstNodeID, 0);
}//====================================================

//-----------------------------------------------------
//      Set and Get Functions
//-----------------------------------------------------

/**
 *  @brief Add a constraint to the arcset object.
 *  @details The constraint application type determines whether the constraint
 *  is applied to a node, segment, or the arc as a whole.
 * 
 *  @param con the constraint
 *  @see ConstraintApp_tp
 *  @throws Exception if node or segment ID is out of bounds, or if the
 *  constraint application type is unknown
 */
void BaseArcset::addConstraint(Constraint con){
	int id = con.getID();
	switch(con.getAppType()){
		case ConstraintApp_tp::APP_TO_NODE:
			if(id < 0 || id >= (int)(nodeIDMap.size()))
				throw Exception("BaseArcset::addConstraint: Node ID out of bounds");

			nodes[nodeIDMap[id]].addConstraint(con);
			break;
		case ConstraintApp_tp::APP_TO_SEG:
			if(id < 0 || id >= (int)(segIDMap.size()))
				throw Exception("BaseArcset::addConstraint: Segment ID out of bounds");

			segs[segIDMap[id]].addConstraint(con);
			break;
		case ConstraintApp_tp::APP_TO_ARC:
			cons.push_back(con);
			break;
		default:
			throw Exception("BaseArcset::addConstraint: Constraint application type is unknown");
	}
}//=====================================================

/**
 *  @brief Add a node to this data object
 *  @details A unique key is assigned to the node when it is added
 * 
 *  @param n the node to add
 *  @return the ID assigned to the node
 */
int BaseArcset::addNode(Node n){
	n.clearLinks();			// Cannot have any links coming in
	n.setID(nextNodeID);
	nodes.push_back(n);
	nodeIDMap.push_back(nodes.size()-1);

	return nextNodeID++;
}//=====================================================

/**
 *  @brief Add a segment to the arcset object.
 *  @details Besides adding the segment object to the storage vector,
 *  this function updates the nodes associated with the origin and terminus
 *  of the segment so they contain the proper links. Additionally, the
 *  function checks to make sure time flows in one direction only and that
 *  parallel structures are not created (i.e., two branches extend from
 *  one node in same time direction).
 * 
 *  @param s The segment to add
 *  @return the ID of the segment
 *  @throws Exception if adding the segment will result in a time direction
 *  collision or a parallel structure. Exception is also thrown if the segment
 *  is linked to a non-existant node other than the placeholder 
 *  Linkable::INVALID_ID
 */
int BaseArcset::addSeg(Segment s){
	s.setID(nextSegID);
	
	if(s.getOrigin() == Linkable::INVALID_ID)
		throw Exception("BaseArcset::addSeg: Segment must have a valid origin node\n");
	
	bool foundValidLink = false;
	for(int i = 0; i < Linkable::NUM_LINKS; i++){
		// Get the ID of one of the nodes this segment is linked to
		int linkedNodeID = s.getLink(i);
		if(linkedNodeID != Linkable::INVALID_ID){

			if(linkedNodeID < 0 || linkedNodeID >= (int)(nodeIDMap.size()))
				throw Exception("BaseArcset::addSeg: Segment has link to an invalid node ID");

			// Get the index of that node within the storage array
			int linkedNodeIx = nodeIDMap[linkedNodeID];
			if(linkedNodeIx != Linkable::INVALID_ID){

				// See if the node is linked to any other segments
				Node *theNode = &(nodes[linkedNodeIx]);
				int secondaryLinks = 0;
				for(int j = 0; j < Linkable::NUM_LINKS; j++){
					// Check to make sure the link is to a real segment
					if(theNode->getLink(j) != Linkable::INVALID_ID){
						int nearSegIx = segIDMap[theNode->getLink(j)];
						// Make sure the segment is real
						if(nearSegIx != Linkable::INVALID_ID){
							secondaryLinks++;

							// If the node is linked to another segment, get that segment and compare it to this one
							const Segment *nearSeg = &(segs[nearSegIx]);
							bool sameLinkType = nearSeg->getLink(i) == linkedNodeID;
							bool sameTimeDir = nearSeg->getTOF()*s.getTOF() > 0;

							if(sameLinkType && i == Segment::TERM_IX){
								print();
								astrohelion::printErr("Adding segment (ID %d) O: %d, T: %d\n", s.getID(), s.getOrigin(), s.getTerminus());
								astrohelion::printErr("Conflict at node (ID %d): seg (ID %d) also terminates here!\n", linkedNodeID, nearSeg->getID());
								throw Exception("BaseArcset::addSeg: would create node with two terminating segments");
							}else if(sameLinkType && sameTimeDir){
								// either a time collision (both terminate) or parallel structure (both originate)
								printf("Nearby segment w/ ID %d originates at node %d and terminates at node %d w/ TOF = %.4e\n", nearSeg->getID(), nearSeg->getOrigin(), nearSeg->getTerminus(), nearSeg->getTOF());
								printf("The new seg (ID %d) originates at node %d and terminates at node %d w/ TOF = %.4e\n", s.getID(), s.getOrigin(), s.getTerminus(), s.getTOF());
								// print();
								// printInChrono();
								throw Exception("BaseArcset::addSeg: either time collision or parallel structure");
							}else if(!sameLinkType && !sameTimeDir){
								// parallel structure
								print();
								astrohelion::printErr("Adding segment (ID %d) O: %d, T: %d, tof = %.4f\n", s.getID(), s.getOrigin(), s.getTerminus(), s.getTOF());
								astrohelion::printErr("Conflict at node (ID %d): seg (ID %d) has O: %d, T:%d, tof = %.4f\n", linkedNodeID, nearSeg->getID(),
									nearSeg->getOrigin(), nearSeg->getTerminus(), nearSeg->getTOF());
								throw Exception("BaseArcset::addSeg: parallel structure");
							}else{
								foundValidLink = true;
							}
						}
					}
				}

				// If neither of the nodes are linked to any segments, we should be fine!
				if(secondaryLinks == 0)
					foundValidLink = true;

				if(foundValidLink){
					theNode->addLink(nextSegID);		// OK, looks good from here; node will check for duplicate linkage
					// printf("Node (ID %d) is now linked to segment (ID %d)\n", theNode.getID(), nextSegID);
				}
			}else{
				// Linked node has valid ID but isn't part of this arcset
				throw Exception("BaseArcset::addSeg: Linked node is not part of this arcset object");
			}
		}
	}

	if(foundValidLink){
		segs.push_back(s);
		segIDMap.push_back(segs.size()-1);
		return nextSegID++;	
	}else{
		return Linkable::INVALID_ID;
	}
}//===========================================

/**
 *  @brief Append an arcset object (i.e., a set of nodes and segments) to this one
 * 
 *  @param arcset a pointer to the arcset derivative object to append
 *  @param linkTo_ID the ID of the node in *this* arcset object to link to
 *  @param linkFrom_ID the ID of the node in <tt>set</tt> to link from
 *  @param tof time-of-flight between linkFrom_ID to linkTo_ID; if set to zero, 
 *  it is assumed that the two nodes are identical, and the original (the one in *this* object)
 *  will be retained, the other deleted, and segments rerouted accordingly.
 *  
 *  @return the ID of a new segment that links the old and new arcset objects
 *	@throws Exception if the two arcset objects have different system data objects
 *  @throws Exception if either ID is out of bounds
 *  @throws Exception if one or both of the identifies nodes does not have
 *  a free link slot
 */
int BaseArcset::appendSetAtNode(const BaseArcset *arcset, int linkTo_ID, int linkFrom_ID, double tof){
	if(arcset->sysData != sysData)
		throw Exception("BaseArcset::appendSetAtNode: Cannot concatenate two arcsets with different system data objects");

	// First, check to make sure the specified nodes are valid
	if(linkTo_ID < 0 || linkTo_ID >= (int)(nodeIDMap.size()))
		throw Exception("BaseArcset::appendSetAtNode: linkTo_ID is out of bounds");

	// Create a copy so we don't affect the original
	baseArcsetPtr set = arcset->clone();

	Node linkTo_node = nodes[nodeIDMap[linkTo_ID]];
	Node linkFrom_node = set->getNode(linkFrom_ID);		// Will do its own index checks

	// Both nodes must have one "open port"
	if(!linkTo_node.isLinkedTo(Linkable::INVALID_ID) || !linkFrom_node.isLinkedTo(Linkable::INVALID_ID))
		throw Exception("BaseArcset::appendSetAtNode: specified nodes are not both open to a new link");

	// Determine if linkTo_node is the origin or terminus of a segment
	// printf("linkToNode has links [%d, %d]\n", linkTo_node.getLink(0), linkTo_node.getLink(1));
	// printf("Choosing segment (ID %d)\n", linkTo_node.getLink(0) == Linkable::INVALID_ID ? linkTo_node.getLink(1) : linkTo_node.getLink(0));
	Segment linkTo_seg = getSeg(linkTo_node.getLink(0) == Linkable::INVALID_ID ? linkTo_node.getLink(1) : linkTo_node.getLink(0));
	bool linkTo_isOrigin = linkTo_seg.getOrigin() == linkTo_node.getID();
	Segment linkFrom_seg = set->getSeg(linkFrom_node.getLink(0) == Linkable::INVALID_ID ? linkFrom_node.getLink(1) : linkFrom_node.getLink(0));
	bool linkFrom_isOrigin = linkFrom_seg.getOrigin() == linkFrom_node.getID();

	if(!linkTo_isOrigin && !linkFrom_isOrigin)
		throw Exception("BaseArcset::appendSetAtNode: neither node is an origin; cannot create segment between them\n");

	// if TOF is zero, then linkFrom_node is assumed to be the same as linkTo_node
	// To avoid having a segment with a TOF of zero, we delete one and update the
	// TOF and linkFrom_ID
	if(tof == 0){
		tof = linkFrom_seg.getTOF();						// Update tof

		// Get the next node down the line
		int new_linkFrom_ID = linkFrom_isOrigin ? linkFrom_seg.getTerminus() : linkFrom_seg.getOrigin();

		// Delete the end node and the segment that connects to it
		set->deleteSeg(linkFrom_seg.getID());
		set->deleteNode(linkFrom_ID);
		
		// Update objects and variables that depend on linkFrom_ID
		linkFrom_ID = new_linkFrom_ID;
		linkFrom_node = set->getNode(linkFrom_ID);
		int new_linkFrom_segIx = linkFrom_node.getLink(0) == Linkable::INVALID_ID ? linkFrom_node.getLink(1) : linkFrom_node.getLink(0);
		
		if(new_linkFrom_segIx != Linkable::INVALID_ID){
			linkFrom_seg = set->getSeg(new_linkFrom_segIx);
			linkFrom_isOrigin = linkFrom_seg.getOrigin() == linkFrom_node.getID();
		}else{
			// No segments left, just a node
			// Leave linkFrom_seg the same; this is used later to determine the direction of time
			// make linkFrom_isOrigin = false; no more segments originating from the final node
			linkFrom_isOrigin = false;
		}
	}

	// A mapping vector: index is the old node ID, value is the new node ID
	// All new IDs are initialized to the default INVALID_ID value
	std::vector<int> map_oldID_to_newID = concatArcset(set.get());

	// Add a new segment to link the nodes from [set] to [this object]
	int origin = Linkable::INVALID_ID, terminus = Linkable::INVALID_ID;
	if(!linkTo_isOrigin){
		origin = linkTo_ID;
		terminus = map_oldID_to_newID[linkFrom_ID];
	}else if(!linkFrom_isOrigin){
		origin = map_oldID_to_newID[linkFrom_ID];
		terminus = linkTo_ID;
	}else{
		// Both are origins; the only double-origin node posibility is one where
		// each segment that originates from the node has a different time direction
		if(linkTo_seg.getTOF() < 0){
			origin = tof > 0 ? linkTo_ID : map_oldID_to_newID[linkFrom_ID];
			terminus = tof > 0 ? map_oldID_to_newID[linkFrom_ID] : linkTo_ID;
		}else if(linkFrom_seg.getTOF() < 0){
			origin = tof > 0 ? map_oldID_to_newID[linkFrom_ID] : linkTo_ID;
			terminus = tof > 0 ? linkTo_ID : map_oldID_to_newID[linkFrom_ID];
		}
	}

	// print();
	return addSeg(Segment(origin, terminus, tof));
}//====================================================

/**
 *  @brief Remove all constraints from this arcset object.
 *  @details Note that this does not affect any constraints placed on
 *  individual nodes or segments
 */
void BaseArcset::clearArcConstraints(){ cons.clear(); }


/**
 *  @brief Remove constraints from this arcset object as well as
 *  all its node and segment children
 */
void BaseArcset::clearAllConstraints(){
	for(size_t n = 0; n < nodes.size(); n++){ nodes[n].clearConstraints(); }
	for(size_t s = 0; s < segs.size(); s++){ segs[s].clearConstraints(); }
	cons.clear();
}//====================================================

/**
 *  @brief Concatenate two arcset objects
 *  @details The nodes, segments and constraints are copied from one arcset to another
 *  without creating or deleting any nodes or segments; i.e., the arcset object will
 *  include two independent "flows" without a segment to connect them
 * 
 *  @param set pointer to an arcset object
 *  @return a map relating the nodeIDs in <tt>set</tt> to the new IDs of the same nodes
 *  in this object; the index of the vector is the old node ID and the value is the 
 *  new node ID. If a node does not exist for one of the old ID values, a new value 
 *  equivalent to <tt>Linkable::INVALID_ID</tt> is stored in the associated 
 *  vector element.
 *  
 *  @throws Exception if the input arcset does not have the same system data object as this one
 */
std::vector<int> BaseArcset::concatArcset(const BaseArcset *set){
	if(set->sysData != sysData)
		throw Exception("BaseArcset::concatArcset: Cannot concatenate two arcsets with different system data objects");

	// A mapping vector: index is the old node ID, value is the new node ID
	// All new IDs are initialized to the default INVALID_ID value
	std::vector<int> map_oldID_to_newID(set->getNextNodeID(), Linkable::INVALID_ID);

	// Add all nodes from set to this object and keep track of new IDs
	for(int n = 0; n < set->getNumNodes(); n++){
		Node node = set->getNodeByIx(n);

		// Remove all links to segments; these will be added back when the segments are added to this new arcset object
		node.clearLinks();
		map_oldID_to_newID[node.getID()] = addNode(node);
	}

	// Add all segments from set to this object and update the link IDs
	// The act of adding the segment will update the links in the newly added nodes
	for(int s = 0; s < set->getNumSegs(); s++){
		Segment seg = set->getSegByIx(s);
		
		// Remap the origin and terminus to the new IDs
		if(seg.getOrigin() != Linkable::INVALID_ID)
			seg.setOrigin(map_oldID_to_newID[seg.getOrigin()]);

		if(seg.getTerminus() != Linkable::INVALID_ID)
			seg.setTerminus(map_oldID_to_newID[seg.getTerminus()]);

		addSeg(seg);
	}

	// Copy all constraints
	std::vector<Constraint> arcCons = set->getArcConstraints();
	cons.insert(cons.end(), arcCons.begin(), arcCons.end());

	// Update tolerance to the larger of the two tolerances
	tol = set->getTol() > tol ? set->getTol() : tol;

	return map_oldID_to_newID;
}//====================================================

/**
 *  @brief Delete the node with the specified ID
 *  @details In addition to deleting the desired node, the arcset object is "healed"
 *  so that time continuity is maintained. In the case of a linear-time set, the two segments
 *  on either side of the deleted node are combined. If the deleted node was the origin
 *  of two segments, they are still combined, but subject to a few extra conditions. First,
 *  if one of the segments connects to another segment and, therefore, doesn't link to another
 *  node, then the terminus of the other segment must be used. If both segments that originate
 *  from the deleted node terminate at other segments, an exception is thrown rather than attempting
 *  to step through mulitple segment links to identify the superposition of all the segments.
 * 
 *  @param id the ID of the node to delete; if the ID is out of range, an exception is
 *  thrown. If the ID is in range but doesn't represent an actual node, no action
 *  is taken.
 *  @throws Exception when:
 *  * The ID is out of bounds
 *  * The ID is in bounds but the associated node has been deleted (no longer exists)
 *  * Deleting the node will result in multiple segment interfaces; these must be 
 *  created more explicitely by the user and will not be created automatically by
 *  this function.
 */
void BaseArcset::deleteNode(int id){
	if(id < 0 || id >= (int)(nodeIDMap.size()))
		throw Exception("BaseArcset::deleteNode: id out of bounds");

	int nodeIx = nodeIDMap[id];

	if(nodeIx != Linkable::INVALID_ID){
		
		// printf("Attempting to delete node (ID %d)\n", id);
		// Get the node we're deleting
		const Node *theNode = &(nodes[nodeIx]);

		// Get the indices of any segments this node is linked to
		std::vector<int> linkedSegIxs;
		for(int i = 0; i < Linkable::NUM_LINKS; i++){
			if(theNode->getLink(i) != Linkable::INVALID_ID){
				int segIx = segIDMap[theNode->getLink(i)];
				if(segIx != Linkable::INVALID_ID){
					linkedSegIxs.push_back(segIx);
					// printf("  linked to segment (ID %d) at index %d\n", theNode->getLink(i), segIx);
				}
			}
		}

		// Determine what type of node this is
		if(linkedSegIxs.size() == 2){
			/* There are two possibilities: either this node is part of a linear time progression (one 
			 * terminating segment and one originating segment) or this node is a "double source" with 
			 * two nodes originating, each with different time directions
			 */
			if(segs[linkedSegIxs[0]].getTerminus() == id || segs[linkedSegIxs[1]].getTerminus() == id){
				// Get the segment that terminates at this node, and the segment that originates at this node
				int termSegIx = segs[linkedSegIxs[0]].getTerminus() == id ? 0 : 1;
				const Segment *termSeg = &(segs[linkedSegIxs[termSegIx]]);
				const Segment *origSeg = &(segs[linkedSegIxs[(termSegIx + 1) % 2]]);

				// printf("  > Segment (ID %d) terminates at node (ID %d)\n", termSeg->getID(), id);
				// printf("  > Segment (ID %d) originates at node (ID %d)\n", origSeg->getID(), id);

				// Just to check
				if(termSeg->getTOF()*origSeg->getTOF() < 0){
					throw Exception("BaseArcset::deleteNode: I made an incorrect assumption about origin/terminus TOF direction!");
				}

				// Create a new segment
				Segment combo(termSeg->getOrigin(), origSeg->getTerminus(), termSeg->getTOF() + origSeg->getTOF());

				// Get the IDs of the segments; deleting id1 will change what origSeg points to, which will affect the getID() return value
				int id1 = termSeg->getID(), id2 = origSeg->getID();

				// Replace the two segments with the new combined one
				deleteSeg(id1);	// CANNOT USE *termSeg pointer AFTER THIS LINE
				deleteSeg(id2);	// CANNOT USE *origSeg pointer AFTER THIS LINE
				addSeg(combo);
			}else{
				// Both must originate at node and have opposite time directions
				if(segs[linkedSegIxs[0]].getOrigin() != id || segs[linkedSegIxs[1]].getOrigin() != id){
					throw Exception("BaseArcset::deleteNode: double origin - made incorrect assumption about origin-ness");
				}

				if(segs[linkedSegIxs[0]].getTOF()*segs[linkedSegIxs[1]].getTOF() > 0){
					throw Exception("BaseArcset::deleteNode: double origin - made incorrect assumption about TOF");
				}

				int revSegIx = segs[linkedSegIxs[0]].getTOF() < 0 ? 0 : 1;
				const Segment *revSeg = &(segs[linkedSegIxs[revSegIx]]);
				const Segment *forwardSeg = &(segs[linkedSegIxs[(revSegIx+1) % 2]]);

				/*	It is possible that, in this case, the segment that originates from this node and proceeds
				 * 	in reverse time does not terminate at a node, but links to a forward-propagated segment instead.
				 * 	If this is so, then the combination of the two segments becomes a reverse-time segment. However,
				 * 	if the segments both link to nodes, then the default behavior is to construct a new forward-time
				 * 	segment to replace the reverse and forward time segments that originated from this node.
				 */
				Segment combo;
				if(revSeg->getTerminus() != Linkable::INVALID_ID){
					combo = Segment(revSeg->getTerminus(), forwardSeg->getTerminus(), std::abs(revSeg->getTOF()) + forwardSeg->getTOF());
				}else{
					if(forwardSeg->getTerminus() == Linkable::INVALID_ID)
						throw Exception("BaseArcset::deleteNode: Cannot delete node as both segments terminate at other segments");
					combo = Segment(forwardSeg->getTerminus(), revSeg->getTerminus(), revSeg->getTOF() - forwardSeg->getTOF());
				}

				// Get the IDs of the segments; deleting id1 will change what forwardSeg points to, which will affect the getID() return value
				int id1 = revSeg->getID(), id2 = forwardSeg->getID();

				// Replace the two segments with the new one
				deleteSeg(id1);	// CANNOT USE *revSeg pointer AFTER THIS LINE
				deleteSeg(id2);	// CANNOT USE *forwardSeg pointer AFTER THIS LINE
				addSeg(combo);
			}
		}else if(linkedSegIxs.size() == 1){
			// Must be either the first or last point, so update the one linked segment
			segs[linkedSegIxs[0]].removeLink(id);
		}else{ /* Not linked to anything, just delete it! */ }


		nodes.erase(nodes.begin() + nodeIx);					// Remove node from vector
		nodeIDMap[id] = Linkable::INVALID_ID;				// This ID will never be used again, set to INVALID_ID

		// Update vector that maps ID values to storage indices
		for(size_t n = 0; n < nodeIDMap.size(); n++){
			if(nodeIDMap[n] != Linkable::INVALID_ID){
				// Decrement all indices after the node we just removed
				if(nodeIDMap[n] > nodeIx)
					nodeIDMap[n]--;
			}
		}
	}else{
		printf("Cannot Delete: Node with ID %02d was not located\n", id);
	}
}//===========================================

/**
 *  @brief Delete a segment with the specified ID.
 *  @details Additionally, any nodes linked to the specified segment are updated
 *  so that their link arrays no longer include a relationship with the soon-to-be
 *  deceased segment.
 * 
 *  @param id The ID of the segment to delete. If the ID is out of range, an exception
 *  is thrown. If the ID is in range but doesn't represent an existing segment,
 *  no deletion is made
 * 	@throws Exception if <tt>id</tt> is out of bounds
 */
void BaseArcset::deleteSeg(int id){
	if(id < 0 || id >= (int)(segIDMap.size()))
		throw Exception("BaseArcset::deleteSeg: Invalid ID (out of bounds)");

	int segIx = segIDMap[id];

	if(segIx != Linkable::INVALID_ID){
		// printf("Deleting segment (ID %d)\n", id);

		const Segment *seg = &(segs[segIx]);
		// printf("  Retrieved segment (ID %d)\n", seg->getID());
		for(int i = 0; i < Linkable::NUM_LINKS; i++){
			if(seg->getLink(i) != Linkable::INVALID_ID){
				int nodeIx = nodeIDMap[seg->getLink(i)];
				if(nodeIx != Linkable::INVALID_ID){
					// printf("  * Trying to remove a link to segment (ID %d) from node (ID %d)\n", seg->getID(), nodes[nodeIx].getID());
					nodes[nodeIx].removeLink(id);
				}else{
					// printf("Unable to remove link to segment (ID %d) from node (ID %d)\n", seg->getLink(i), id);
				}
			}
		}

		segs.erase(segs.begin() + segIx);	// CANNOT USE *seg pointer AFTER THIS LINE

		segIDMap[id] = Linkable::INVALID_ID;
		for(size_t s = 0; s < segIDMap.size(); s++){
			if(segIDMap[s] != Linkable::INVALID_ID){
				if(segIDMap[s] > segIx)
					segIDMap[s]--;
			}
		}
	}else{
		// printf("Cannot Delete: Segment with ID %02d was not located\n", id);
	}
}//===========================================

/**
 *  @brief Retrieve the acceleration vector associated with a node
 *  with the specified ID
 * 
 *  @param id the ID of a node
 *  @return the acceleration vector
 *  @throws Exception if <tt>id</tt> is out of bounds
 *  @throws Exception if the node with the specified ID is not located in the nodeIDMap
 */
std::vector<double> BaseArcset::getAccel(int id) const{
	if(id < 0 || id >= (int)(nodeIDMap.size()))
		throw Exception("BaseArcset::getAccel: Node ID out of range");

	int ix = nodeIDMap[id];
	if(ix != Linkable::INVALID_ID && ix < (int)(nodes.size()) && ix >= 0){
		return nodes[ix].getAccel();
	}else{
		throw Exception("BaseArcset::getAccel: Could not locate the node with the specified ID");
	}
}//====================================================

/**
 *	@brief Retrieve an acceleration on the arc
 *	@param ix the step index. If it is negative, the index will count backwards
 *	from the end of the arc (e.g. ix = -1 will return the last acceleration)
 *	@return the acceleration associated with the specified index
 *	@throws Exception if <tt>ix</tt> is out of bounds
 */
std::vector<double> BaseArcset::getAccelByIx(int ix) const{
	if(ix < 0)
		ix += nodes.size();

	if(ix < 0 || ix >= (int)(nodes.size()))
		throw Exception("BaseArcset::getAccelByIx: node index out of bounds");

	return nodes[ix].getAccel();
}//====================================================

/**
 *  @brief Retrieve a vector containing all the constraints applied to this arcset object.
 *  @details This vector does not include constraints placed on individual nodes or segments.
 *  @return a vector containing all the constraints applied to this arcset object.
 */
std::vector<Constraint> BaseArcset::getArcConstraints() const { return cons; }

/**
 *  @brief Determine what order to place the nodes and segments of this object
 *  into to achieve a chronological progression in forward time.
 * 
 *  @return a vector of ArcPiece objects that represent the chronological 
 *  order of the nodes and segments
 */
std::vector<ArcPiece> BaseArcset::getChronoOrder() const{
	// printf("* Beginning getChronoOrder()\n");
	std::vector<ArcPiece> pieces;
	if(nodes.size() == 0){
		astrohelion::printErr("BaseArcset::getChronoOrder: No nodes... exiting\n");
		return pieces;
	}

	return sortArcset(nodes[0].getID(), pieces);
}//====================================================

/**
 *  @brief Sort the arcset into chronological order beginning at a specified node
 *  @details [long description]
 * 
 *  @param ID the ID of a node in the arcset
 *  @return a vector of ArcPiece objects that represent the chronological 
 *  order of the nodes and segments that make up the section of the arcset that
 *  contains the specified node
 *  
 *  @throws Exception if the ID is out of bounds
 */
std::vector<ArcPiece> BaseArcset::sortArcset(int ID, std::vector<ArcPiece> prevPieces) const{
	if(ID < 0 || ID >= nextNodeID)
		throw Exception("BaseArcset::sortArcset: ID out of bounds");

	std::vector<ArcPiece> pieces;

	Node node0 = nodes[nodeIDMap[ID]];
	// printf("Beginning with node ID %d\n", node0.getID());

	pieces.push_back(ArcPiece(ArcPiece::Piece_Tp::NODE, node0.getID()));
	for(int dir = 1; dir > -2; dir -= 2){
		// printf("  Direction is %s\n", dir > 0 ? "[FORWARD]" : "[BACKWARD]");

		bool go = true;
		Node node = node0;
		
		while(go){
			// Get segments connected to node
			bool foundNextSeg = false;
			for(int i = 0; i < Linkable::NUM_LINKS; i++){
				if(node.getLink(i) != Linkable::INVALID_ID){

					Segment seg = getSeg(node.getLink(i));
					// printf("   (checking out segment ID %d): ", seg.getID());

					if(dir > 0 && ((seg.getTerminus() == node.getID() && seg.getTOF() < 0) || (seg.getOrigin() == node.getID() && seg.getTOF() > 0))){
						// This segment moves forward in time from node
						pieces.push_back(ArcPiece(ArcPiece::Piece_Tp::SEG, seg.getID()));
						foundNextSeg = true;
						// printf("USE THIS\n");
					}else if(dir < 0 && ((seg.getTerminus() == node.getID() && seg.getTOF() > 0) || (seg.getOrigin() == node.getID() && seg.getTOF() < 0))){
						// This segment moves in reverse time from node
						pieces.insert(pieces.begin(), ArcPiece(ArcPiece::Piece_Tp::SEG, seg.getID()));
						foundNextSeg = true;
						// printf("USE THIS\n");
					}else{
						foundNextSeg = false;
						// printf("do not use\n");
					}

					if(foundNextSeg){
						// Get next node
						int nextNodeID = Linkable::INVALID_ID;
						if(seg.getTerminus() == node.getID())
							nextNodeID = seg.getOrigin();
						else
							nextNodeID = seg.getTerminus();

						if(nextNodeID != Linkable::INVALID_ID){
							// printf("  Next node has ID %d\n", nextNodeID);
							node = getNode(nextNodeID);

							if(dir > 0)
								pieces.push_back(ArcPiece(ArcPiece::Piece_Tp::NODE, node.getID()));
							else
								pieces.insert(pieces.begin(), ArcPiece(ArcPiece::Piece_Tp::NODE, node.getID()));
						}else{
							// The segment terminates/originates without a node; Look for a link to
							// another segment OR end this section

							int linkedSegID = Linkable::INVALID_ID;
							for(size_t c = 0; c < cons.size(); c++){
								if(cons[c].getType() == Constraint_tp::SEG_CONT_PV){
									// Check to see if this constraint is linked to this node at all
									int ID0 = cons[c].getID(), ID1 = Linkable::INVALID_ID;
									std::vector<double> data = cons[c].getData();
									for(size_t d = 0; d < data.size(); d++){
										if(!std::isnan(data[d])){
											ID1 = (int)(data[d]);
											break;
										}
									}
									if(ID0 == seg.getID()){
										linkedSegID = ID1;
										break;
									}else if(ID1 == seg.getID()){
										linkedSegID = ID0;
										break;
									}
									// Didn't find the correct link, keep looking
								}
							}

							if(linkedSegID != Linkable::INVALID_ID){
								// printf(">Found seg-2-seg link with segment %d\n", linkedSegID);
								
								// Check to see if the identified segment has already been counted
								bool alreadyUsed = false;
								for(size_t p = 0; p < prevPieces.size(); p++){
									if(prevPieces[p].type == ArcPiece::Piece_Tp::SEG && 
										prevPieces[p].id == linkedSegID){
										
										alreadyUsed = true;
										break;
									}
								}

								// Also check the current segment
								if(!alreadyUsed){
									for(size_t p = 0; p < pieces.size(); p++){
										if(pieces[p].type == ArcPiece::Piece_Tp::SEG && pieces[p].id == linkedSegID){
											alreadyUsed = true;
											break;
										}
									}
								}

								if(!alreadyUsed){
									// Located a segment that links to this one, go recursive!
									Segment linkedSeg = segs[segIDMap[linkedSegID]];
									int linkedNodeID = linkedSeg.getOrigin();
									if(linkedNodeID != Linkable::INVALID_ID){
										// Concatenate the pieces I've arranged so far and the previously arranged ones; order is unimportant here
										std::vector<ArcPiece> allPieces = pieces;
										allPieces.insert(allPieces.begin(), prevPieces.begin(), prevPieces.end());
										std::vector<ArcPiece> section = sortArcset(linkedNodeID, allPieces);

										// Append newly arranged section of pieces to the total vector (order IS important here)
										pieces.insert(dir > 0 ? pieces.end() : pieces.begin(), section.begin(), section.end());
									}
								}
							}
							go = false;
						}

						break;	// Exit the loop
					}
				}
			}

			if(!foundNextSeg)
				go = false;		// Exit the iterations and switch directions
		}
	}

	return pieces;
}//====================================================

/**
 *	@brief Get a vector of one coordinate for all nodes
 *	@param ix the index of the coordinate: 0 = x, 1 = y, etc.
 *	@return a vector containing the specified coordinate for all
 *	nodes (not necessarily in chronological order)
 *	@throws Exception if <tt>ix</tt> is out of bounds
 */
std::vector<double> BaseArcset::getCoord(int ix) const{
	if(ix >= 6)
		throw Exception("BaseArcset::getCoord: Index Out of Range");

	std::vector<double> coord;
	for(size_t n = 0; n < nodes.size(); n++){
		coord.push_back(nodes[n].getState()[ix]);
	}

	return coord;
}//====================================================

/**
 *  @brief Retrieve the epoch associated with a node
 *  with the specified ID
 * 
 *  @param id the ID of a node
 *  @return the epoch
 *  @throws Exception if <tt>id</tt> is out of bounds
 *  @throws Exception if the node with the specified ID is not located in the nodeIDMap
 */
double BaseArcset::getEpoch(int id) const{
	if(id < 0 || id >= (int)(nodeIDMap.size()))
		throw Exception("BaseArcset::getEpoch: Node ID out of range");

	int ix = nodeIDMap[id];
	if(ix != Linkable::INVALID_ID && ix < (int)(nodes.size()) && ix >= 0){
		return nodes[ix].getEpoch();
	}else{
		throw Exception("BaseArcset::getEpoch: Could not locate the node with the specified ID");
	}
}//====================================================

/**
 *  @brief Retrieve the epoch of a specific node
 * 
 *  @param ix the node index within the <tt>nodes</tt> storage array; This value
 *	is not necessarily the same as the unique ID assigned to the node when it 
 *	was added to the arcset object. If <tt>n</tt> is negative, this index will
 *	cound backwards from the end of the array.
 *	
 *  @return The epoch associated with the specified node
 *  @throws Exception if <tt>ix</tt> is out of bounds
 */
double BaseArcset::getEpochByIx(int ix) const{
	if(ix < 0)
		ix += nodes.size();

	if(ix < 0 || ix >= (int)(nodes.size()))
		throw Exception("BaseArcset::getEpochByIx: node index out of bounds");

	return nodes[ix].getEpoch();
}//=====================================================

/**
 *	@brief Retrieve a set of extra parameters for the specified node
 *	@param n the node index within the <tt>nodes</tt> storage array; This value
 *	is not necessarily the same as the unique ID assigned to the node when it 
 *	was added to the arcset object. If <tt>n</tt> is negative, this index will
 *	cound backwards from the end of the array.
 *	
 *	@param ix the index of the extra parameter
 *	@return a vector containing the extra parameter at the specified step and index
 *	@throws Exception if <tt>n</tt> or <tt>ix</tt> are out of bounds
 */
std::vector<double> BaseArcset::getExtraParam(int n, int ix) const{
	if(n < 0)
		n += nodes.size();

	if(n < 0 || n >= (int)(nodes.size()))
		throw Exception("BaseArcset::getExtraParam: node index out of bounds");

	if(ix < 0 || ix >= (int)(extraParamRowSize.size()))
		throw Exception("BaseArcset::getExtraParam: parameter index out of bounds");

	int startIx = 0;
	for(int i = 0; i < ix; i++)
		startIx += extraParamRowSize[i];

	int size = extraParamRowSize[ix];
	std::vector<double> extraParam = nodes[n].getExtraParams();
	return std::vector<double>(extraParam.begin()+startIx, extraParam.begin()+startIx + size);
}//====================================================

/**
 *  @brief Retrieve the value of the ID that will be assigned to the next 
 *  node added to this arcset object
 *  @return the next node ID
 */
int BaseArcset::getNextNodeID() const { return nextNodeID; }

/**
 *  @brief Retrieve the value of the ID that will be assigned to the next 
 *  segment added to this arcset object
 *  @return the next segment ID
 */
int BaseArcset::getNextSegID() const { return nextSegID; }

/**
 *	@brief Retrieve the number of nodes
 *	@return the number of nodes
 */
int BaseArcset::getNumNodes() const { return (int)(nodes.size()); }

/**
 *	@brief Retrieve the number of segments
 *	@return the number of segments
 */
int BaseArcset::getNumSegs() const { return (int)(segs.size()); }

/**
 *  @brief Retrieve the total number of constraints contained by all nodes, segments,
 *  and the arcset object itself
 *  @return the total number of constraints applied to this object and its children
 */
int BaseArcset::getNumCons() const {
	int conCount = (int)cons.size();
	for(size_t n = 0; n < nodes.size(); n++){ conCount += nodes[n].getNumCons(); }
	for(size_t s = 0; s < segs.size(); s++){ conCount += segs[s].getNumCons(); }

	return conCount;
}//===================================================

/**
 *  @brief Retrieve a specific node
 * 
 *  @param id the ID of the desired node.
 *	
 *  @return the node located with the specified ID
 *  @throws Exception if <tt>id</tt> is out of bounds or if no node exists with the specified ID
 */
Node BaseArcset::getNode(int id) const{
	if(id < 0 || id >= (int)(nodeIDMap.size()))
		throw Exception("BaseArcset::getNode: Invalid ID (out of bounds)");

	int ix = nodeIDMap[id];
	if(ix != Linkable::INVALID_ID && ix < (int)(nodes.size()) && ix >= 0){
		return nodes[ix];
	}else{
		throw Exception("BaseArcset::getNode: Could not locate a node with the specified ID");
	}
}//====================================================

/**
 *  @brief Retrieve a node based on its index in the storage array
 * 
 *  @param ix The index of the node; if <tt>ix</tt> is negative, the index will
 *  count backwards from the end of the storage array.
 *  @return a segment at the specified index
 *  @throws Exception if <tt>ix</tt> is out of bounds
 */
Node BaseArcset::getNodeByIx(int ix) const{
	if(ix < 0)
		ix += nodes.size();

	if(ix < 0 || ix >= (int)(nodes.size()))
		throw Exception("BaseArcset::getNodeByIx: Index out of bounds");

	return nodes[ix];
}//=====================================================

/**
 *  @brief Retrieve the index of a specific node within the node storage vector
 * 
 *  @param id node ID
 *  @return the index of the node with the specified ID within the storage vector
 *  @throws Exception if <tt>id</tt> is out of bounds
 */
int BaseArcset::getNodeIx(int id) const{
	if(id < 0 || id >= (int)(nodeIDMap.size()))
		throw Exception("BaseArcset::getNodeIx: Inavlid ID; out of bounds");

	return nodeIDMap[id];
}//====================================================

/**
 *  @brief Retrieve a specific node
 * 
 *  @param id the ID of the desired node
 *	
 *  @return the node located with the specified ID
 *  @throws Exception if <tt>id</tt> is out of bounds or if no segment exists with the specified ID
 */
Segment BaseArcset::getSeg(int id) const{
	if(id < 0 || id >= (int)(segIDMap.size()))
		throw Exception("BaseArcset::getSeg: Invalid ID (out of bounds)");

	int ix = segIDMap[id];
	if(ix != Linkable::INVALID_ID && ix < (int)(segs.size()) && ix >= 0){
		return segs[ix];
	}else{
		throw Exception("BaseArcset::getSeg: Could not locate a segment with the specified ID");
	}
}//====================================================

/**
 *  @brief Retrieve a segment based on its index in the storage array
 * 
 *  @param ix The index of the segment; if <tt>ix</tt> is negative, the index will
 *  count backwards from the end of the storage array.
 *  @return a segment at the specified index
 *  @throws Exception if <tt>ix</tt> is out of bounds
 */
Segment BaseArcset::getSegByIx(int ix) const{
	if(ix < 0)
		ix += segs.size();

	if(ix < 0 || ix >= (int)(segs.size()))
		throw Exception("BaseArcset::getSegByIx: Index out of bounds");

	return segs[ix];
}//=====================================================

/**
 *  @brief Retrieve the index of a specific node within the node storage vector
 * 
 *  @param id node ID
 *  @return the index of the node with the specified ID within the storage vector
 *  @throws Exception if <tt>id</tt> is out of bounds
 */
int BaseArcset::getSegIx(int id) const{
	if(id < 0 || id >= (int)(segIDMap.size()))
		throw Exception("BaseArcset::getSegIx: Inavlid ID; out of bounds");

	return segIDMap[id];
}//====================================================

/**
 *  @brief Retrieve the state vector associated with a node
 *  with the specified ID
 * 
 *  @param id the ID of a node
 *  @return the state vector
 *  @throws Exception if <tt>id</tt> is out of bounds
 *  @throws Exception if the node with the specified ID is not located in the nodeIDMap
 */
std::vector<double> BaseArcset::getState(int id) const{
	if(id < 0 || id >= (int)(nodeIDMap.size()))
		throw Exception("BaseArcset::getState: Node ID out of range");

	int ix = nodeIDMap[id];
	if(ix != Linkable::INVALID_ID && ix < (int)(nodes.size()) && ix >= 0){
		return nodes[ix].getState();
	}else{
		throw Exception("BaseArcset::getState: Could not locate the node with the specified ID");
	}
}//====================================================

/**
 *	@brief Retrieve a position-velocity state on the arc
 *	@param ix the node index within the <tt>nodes</tt> storage array; This value
 *	is not necessarily the same as the unique ID assigned to the node when it 
 *	was added to the arcset object. If <tt>n</tt> is negative, this index will
 *	cound backwards from the end of the array.
 *	
 *	@return the state associated with the specified index
 *	@throws Exception if <tt>ix</tt> is out of bounds
 */
std::vector<double> BaseArcset::getStateByIx(int ix) const{
	if(ix < 0)
		ix += nodes.size();

	if(ix < 0 || ix >= (int)(nodes.size()))
		throw Exception("BaseArcset::getStateByIx: node index out of bounds");

	return nodes[ix].getState();
}//====================================================

/**
 *  @brief Retrieve the STM associated with a segment
 *  with the specified ID
 * 
 *  @param id the ID of a segment
 *  @return the STM
 *  @throws Exception if <tt>id</tt> is out of bounds
 *  @throws Exception if the segment with the specified ID is not located in the segIDMap
 */
MatrixXRd BaseArcset::getSTM(int id) const{
	if(id < 0 || id >= (int)(segIDMap.size()))
		throw Exception("BaseArcset::getSTM: Node ID out of range");

	int ix = segIDMap[id];
	if(ix != Linkable::INVALID_ID && ix < (int)(segs.size()) && ix >= 0){
		return segs[ix].getSTM();
	}else{
		throw Exception("BaseArcset::getSTM: Could not locate the segment with the specified ID");
	}
}//====================================================

/**
 *	@brief Retrieve an STM on the arc
 *	@param ix the segment index. If it is negative, the index will count backwards
 *	from the end of the <tt>segs</tt> storage array
 *	
 *	@return the STM associated with the specified index
 *	@throws Exception if <tt>ix</tt> is out of bounds
 */
MatrixXRd BaseArcset::getSTMByIx(int ix) const{
	if(ix < 0)
		ix += segs.size();

	if(ix < 0 || ix >= (int)(segs.size()))
		throw Exception("BaseArcset::getSTMByIx: segment index out of bounds");

	return segs[ix].getSTM();
}//====================================================

/**
 *	@brief Retrieve the a pointer to the system data object associated with this arc
 *	@return a pointer to the system data object associated with this arc
 */
const SysData* BaseArcset::getSysData() const { return sysData; }

/**
 *  @brief Retrieve the time-of-flight associated with a segment
 *  with the specified ID
 * 
 *  @param id the ID of a segment
 *  @return the time-of-flight
 *  @throws Exception if <tt>id</tt> is out of bounds
 *  @throws Exception if the segment with the specified ID is not located in the segIDMap
 */
double BaseArcset::getTOF(int id) const{
	if(id < 0 || id >= (int)(segIDMap.size()))
		throw Exception("BaseArcset::getTOF: Segment ID out of range");

	int ix = segIDMap[id];
	if(ix != Linkable::INVALID_ID && ix < (int)(segs.size()) && ix >= 0){
		return segs[ix].getTOF();
	}else{
		throw Exception("BaseArcset::getTOF: Could not locate the segment with the specified ID");
	}
}//====================================================
/**
 *	@brief Get the time-of-flight for a specific segment
 *	@param ix node index (NOT the ID); if less than 0, the index counts
 *	backwards from the end of the nodeset
 *	@return non-dimensional time-of-flight along the specified segment
 *	@throws Exception if <tt>ix</tt> is out of bounds
 */
double BaseArcset::getTOFByIx(int ix) const {
	if(ix < 0)
		ix += segs.size();

	if(ix < 0 || ix >= ((int)segs.size()))
		throw Exception("Nodeset::getTOFByIx: invalid segment index");

	return segs[ix].getTOF();
}//====================================================

/**
 *	@brief Retrieve the tolerance with which data in this object was computed
 *	@return the tolerance with which data in this object was computed
 */
double BaseArcset::getTol() const { return tol; }

/**
 *  @brief Determine the total time-of-flight along this arc.
 *  @details This function sums the TOF along each segment; derived
 *  classes may override this function to use different methods.
 *  @return the total time-of-flight along this arc, units consistent
 *  with the SysData object
 */
double BaseArcset::getTotalTOF() const{
	double total = 0;
	for(size_t s = 0; s < segs.size(); s++){
		total += segs[s].getTOF();
		// total += std::abs(segs[s].getTOF());
	}
	return total;
}//=================================================

/**
 *  @brief Rearrange the nodes and segments so that they are listed
 *  in chronological order in their storage arrays.
 *  
 *  @details This does not change the ID of any of the nodes or segments,
 *  only their index within the storage array. After calling this function,
 *  accessing the -1 node or segment will return the latest (in time) object.
 *  
 *  @throws Exception if the getChronoOrder() sorting algorithm returns a 
 *  set of ArcPiece objects that has a different size than the combined 
 *  node and segment vectors, the function is aborted as it is likely a node or 
 *  segment was skipped and we don't want to lose information.
 */
void BaseArcset::putInChronoOrder(){
	// First, determine the chronological order
	// print();
	std::vector<ArcPiece> pieces = getChronoOrder();

	if(pieces.size() != nodes.size() + segs.size()){
		astrohelion::printErr("Pieces has %zu elements, but there are %zu nodes and %zu segs\n", pieces.size(), nodes.size(), segs.size());
		throw Exception("BaseArcset::putInChronoOrder: The sorted vector does not include all nodes and segments; aborting to avoid losing data\n");
	}

	std::vector<Node> newNodes;
	std::vector<Segment> newSegs;
	std::vector<int> newNodeIDMap(nodeIDMap.size(), Linkable::INVALID_ID);
	std::vector<int> newSegIDMap(segIDMap.size(), Linkable::INVALID_ID);

	for(size_t i = 0; i < pieces.size(); i++){
		if(pieces[i].type == ArcPiece::Piece_Tp::NODE){
			Node node = getNode(pieces[i].id);
			newNodeIDMap[node.getID()] = newNodes.size();
			newNodes.push_back(node);
		}else if(pieces[i].type == ArcPiece::Piece_Tp::SEG){
			Segment seg = getSeg(pieces[i].id);
			newSegIDMap[seg.getID()] = newSegs.size();
			newSegs.push_back(seg);
		}
	}

	nodes = newNodes;
	segs = newSegs;
	nodeIDMap = newNodeIDMap;
	segIDMap = newSegIDMap;
}//=============================================

/**
 *  @brief Set the acceleration vector associated with a node
 *  with the specified ID
 * 
 *  @param id the ID of a node
 *  @param accel the acceleration vector
 *  @throws Exception if <tt>id</tt> is out of bounds
 */
void BaseArcset::setAccel(int id, std::vector<double> accel){
	if(id < 0 || id >= (int)(nodeIDMap.size()))
		throw Exception("BaseArcset::setAccel: Node ID out of range");

	nodes[nodeIDMap[id]].setAccel(accel);
}//====================================================

/**
 *  @brief Set the acceleration vector for a specific step/node
 * 
 *  @param ix the node index within the <tt>nodes</tt> storage array; This value
 *	is not necessarily the same as the unique ID assigned to the node when it 
 *	was added to the arcset object. If <tt>n</tt> is negative, this index will
 *	cound backwards from the end of the array.
 *	
 *  @param accelVec 3-element (at least) vector of non-dimensional acceleration 
 *  values (ax, ay, az, ...); only the first three are used
 *  @throws Exception if <tt>ix</tt> is out of bounds
 */
void BaseArcset::setAccelByIx(int ix, std::vector<double> accelVec){
	if(ix < 0)
		ix += nodes.size();

	if(ix < 0 || ix >= (int)(nodes.size()))
		throw Exception("BaseArcset::setAccelByIx: node index out of bounds");

	nodes[ix].setAccel(accelVec);
}//=================================================

/**
 *  @brief Set the state vector associated with a node
 *  with the specified ID
 * 
 *  @param id the ID of a node
 *  @param state the state vector
 *  @throws Exception if <tt>id</tt> is out of bounds
 */
void BaseArcset::setState(int id, std::vector<double> state){
	if(id < 0 || id >= (int)(nodeIDMap.size()))
		throw Exception("BaseArcset::setState: Node ID out of range");

	nodes[nodeIDMap[id]].setState(state);
}//====================================================

/**
 *  @brief Set the state vector for a specific step/node
 * 
 *  @param ix the node index within the <tt>nodes</tt> storage array; This value
 *	is not necessarily the same as the unique ID assigned to the node when it 
 *	was added to the arcset object. If <tt>n</tt> is negative, this index will
 *	cound backwards from the end of the array.
 *	
 *  @param stateVec 6-element (at least) vector of non-dimensional state 
 *  values (x, y, z, vx, vy, vz, ...); only the first six are used
 *  @throws Exception if <tt>ix</tt> is out of bounds
 */
void BaseArcset::setStateByIx(int ix, std::vector<double> stateVec){
	if(ix < 0)
		ix += nodes.size();

	if(ix < 0 || ix >= (int)(nodes.size()))
		throw Exception("BaseArcset::setStateByIx: node index out of bounds");

	nodes[ix].setState(stateVec);
}//=================================================

/**
 *  @brief Set the STM associated with a segment
 *  with the specified ID
 * 
 *  @param id the ID of a segment
 *  @param stm the STM
 *  @throws Exception if <tt>id</tt> is out of bounds
 */
void BaseArcset::setSTM(int id, MatrixXRd stm){
	if(id < 0 || id >= (int)(segIDMap.size()))
		throw Exception("BaseArcset::setSTM: Node ID out of range");

	segs[segIDMap[id]].setSTM(stm);
}//====================================================

/**
 *  @brief Set the STM for a specific step/node
 * 
 *  @param ix index of the segment with the <tt>segs</tt> storage array; if it is negative,
 *  it will count backwards from the end of the array.
 *  
 *  @param stm a 6x6 matrix containing the STM
 *  @throws Exception if <tt>ix</tt> is out of bounds
 */
void BaseArcset::setSTMByIx(int ix, MatrixXRd stm){
	if(ix < 0)
		ix += segs.size();

	if(ix < 0 || ix >= (int)(segs.size()))
		throw Exception("BaseArcset::setSTMByIx: node index out of bounds");

	segs[ix].setSTM(stm);
}//=================================================

/**
 *	@brief Set the computational tolerance for this data object
 *	@param d the tolerance
 */
void BaseArcset::setTol(double d){ tol = d; }

/**
 *  @brief Update the epochs of all nodes such that time is continuous.
 *  @details By specifying the epoch of one node in the set, all other
 *  nodes are updated using the segment times-of-flight between them.
 * 
 *  @param nodeID the ID of a node
 *  @param epoch the epoch of the node with the specified ID.
 */
void BaseArcset::updateEpochs(int nodeID, double epoch){
	if(nodeID < 0 || nodeID >= (int)(nodes.size()))
		throw Exception("BaseArcset::updateEpochs: Invalide node ID");

	std::vector<ArcPiece> pieces = getChronoOrder();

	// Update epoch of the specified node
	nodes[nodeIDMap[nodeID]].setEpoch(epoch);

	std::vector<ArcPiece>::iterator pieceIt = std::find(pieces.begin(), pieces.end(), ArcPiece(ArcPiece::Piece_Tp::NODE, nodeID));

	if(pieceIt == pieces.end()){
		astrohelion::printErr("BaseArcset::updateEpochs: Could not find the node with the specified ID... INVESTIGATE THIS, ANDREW!\n");
		return;
	}

	int ixInPieces = pieceIt - pieces.begin();

	// Move outward from that node in the chronological map and
	// update all other nodes
	double ellapsed;
	for(int stepDir = 1; stepDir > -2; stepDir -= 2){
		ellapsed = 0;
		for(int i = ixInPieces+stepDir; i < (int)(pieces.size()) && i >= 0; i += stepDir){
			if(pieces[i].type == ArcPiece::Piece_Tp::SEG){
				ellapsed += segs[segIDMap[pieces[i].id]].getTOF();
			}else if(pieces[i].type == ArcPiece::Piece_Tp::NODE){
				nodes[nodeIDMap[pieces[i].id]].setEpoch(epoch+ellapsed);
			}
		}
	}
}//====================================================

//-----------------------------------------------------
//      Utility Functions
//-----------------------------------------------------

/**
 *	@brief Copy all data from the input arc data to this one
 *	@param d an arc data object reference
 */
void BaseArcset::copyMe(const BaseArcset &d){
	nodes = d.nodes;
	nodeIDMap = d.nodeIDMap;
	segs = d.segs;
	segIDMap = d.segIDMap;
	cons = d.cons;
	numExtraParam = d.numExtraParam;
	extraParamRowSize = d.extraParamRowSize;
	tol = d.tol;
	nextNodeID = d.nextNodeID;
	nextSegID = d.nextSegID;
}//====================================================

/**
 *  @brief Initialize the vectors of node and segment objects from a *.mat file
 *  @details THIS FUNCTION MUST BE THE FIRST READ_DATA-TYPE FUNCTION CALLED because
 *  it clears the vectors and then initializes them by calculating the number
 *  of steps in the arcset object from the state vector. Individual nodes and segments are
 *  able to be called by index after this, though they will not contain data
 *  until another function is called to populate the data fields with values from 
 *  the *.mat file
 * 
 *  @param matFile pointer to an open matlab data file
 *  @param varName the name of a variable that has as many rows as there are
 *  steps along the data object. Valid variables typically include the time vector,
 *  state matrix, or acceleration matrix
 *  @throws Exception if the state vector variable cannot be read from the data file
 */
void BaseArcset::initNodesSegsFromMat(mat_t *matFile, const char* varName){
	matvar_t *stateMat = Mat_VarRead(matFile, varName);
	if(stateMat == NULL){
		throw Exception("BaseArcset::initNodeSegsFromMat: Could not read state data vector");
	}else{
		int numSteps = stateMat->dims[0];
		nodes.clear();
		segs.clear();
		nodeIDMap.clear();
		segIDMap.clear();
		Node blank_node;
		Segment blank_seg;
		
		// Create a set of nodes and segments all linked together in linear time
		for(int i = 0; i < numSteps; i++){
			nodes.push_back(blank_node);
			nodeIDMap.push_back(i);
			
			if(i > 0){
				segs.push_back(Segment(i-1, i, NAN));
				segIDMap.push_back(i-1);
			}
		}
		// nodes.assign(numSteps, blank_node);	// Initialize array with a bunch of default objects
		// segs.assign(numSteps-1, blank_seg);
	}
	Mat_VarFree(stateMat);
}//======================================================

/**
 *  @brief Print a ASCII graphic of the arcset in chronological order
 */
void BaseArcset::printInChrono() const{
	std::vector<ArcPiece> pieces = getChronoOrder();

	for(size_t i = 0; i < pieces.size(); i++){
		ArcPiece p = pieces[i];

		if(p.type == ArcPiece::Piece_Tp::NODE){
			printf("[%02d]", p.id);
		}else if(p.type == ArcPiece::Piece_Tp::SEG){
			if(getSeg(p.id).getTOF() > 0)
				printf("--(%02d)->", p.id);
			else
				printf(">-(%02d)--", p.id);
		}
	}
	printf("\n");
}//====================================================

/**
 *  @brief Print nodeIDMap to standard output
 */
void BaseArcset::printNodeIDMap() const{
	for(size_t i = 0; i < nodeIDMap.size(); i++){
		if(i % 20 == 0)
			printf("----------------\n%4s -> %4s\n----------------\n", "ID", "Ix");

		printf("%4zu -> %4d\n", i, nodeIDMap[i]);
	}
}//====================================================

/**
 *  @brief Print segIDMap to standard output
 */
void BaseArcset::printSegIDMap() const{
	for(size_t i = 0; i < segIDMap.size(); i++){
		if(i % 20 == 0)
			printf("----------------\n%4s -> %4s\n----------------\n", "ID", "Ix");

		printf("%4zu -> %4d\n", i, segIDMap[i]);
	}
}//====================================================

/**
 *  @brief Read the state vector for this arcset object from a matlab data file
 *  @details This function must be called after initNodeSegsFromMat() as it
 *  populates the step vector objects with state data
 * 
 *  @param matFile pointer to an open matlab data file
 *  @param varName the name of the state variable (e.g., "State" or "Nodes")
 *  @throws Exception if there are any issues importing the data
 */
void BaseArcset::readStateFromMat(mat_t *matFile, const char* varName){
	matvar_t *stateMat = Mat_VarRead(matFile, varName);
	if(stateMat == NULL){
		throw Exception("BaseArcset::readStateFromMat: Could not read state data vector");
	}else{
		int numSteps = stateMat->dims[0];
		
		if(nodes.size() == 0){
			throw Exception("BaseArcset::readStateFromMat: Step vector has not been initialized!");
		}

		if(numSteps != (int)nodes.size()){
			throw Exception("BaseArcset::readStateFromMat: State vector has a different size than the initialized step vector");
		}

		if(stateMat->dims[1] != 6){
			throw Exception("BaseArcset::readStateFromMat: Incompatible data file: State width is not 6.");
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

					nodes[i].setState(state);
				}
			}
		}else{
			throw Exception("BaseArcset::readStateFromMat: Incompatible data file: unsupported data type/class");
		}
	}
	Mat_VarFree(stateMat);
}//===============================================

/**
 *  @brief Read acceleration values from a matlab file
 * 
 *  @param matFile pointer to an open Matlab file
 *  @throws Exception if there are any issues importing the data
 */
void BaseArcset::readAccelFromMat(mat_t *matFile){
	matvar_t *accelMat = Mat_VarRead(matFile, "Accel");
	if(accelMat == NULL){
		throw Exception("BaseArcset::readAccelFromMat: Could not read data vector");
	}else{
		int numSteps = accelMat->dims[0];
		
		if(nodes.size() == 0){
			throw Exception("BaseArcset::readAccelFromMat: Node vector has not been initialized!");
		}

		if(numSteps != (int)nodes.size()){
			throw Exception("BaseArcset::readAccelFromMat: Accel vector has a different size than the initialized node vector");
		}

		if(accelMat->dims[1] != 3){
			throw Exception("BaseArcset::readAccelFromMat: Incompatible data file: Accel width is not 3.");
		}

		if(accelMat->class_type == MAT_C_DOUBLE && accelMat->data_type == MAT_T_DOUBLE){
			double *data = static_cast<double *>(accelMat->data);

			if(data != NULL){
				for(int i = 0; i < numSteps; i++){
					double accel[] = {0,0,0};
					accel[0] = data[0*numSteps + i];
					accel[1] = data[1*numSteps + i];
					accel[2] = data[2*numSteps + i];

					nodes[i].setAccel(accel);
				}
			}
		}else{
			throw Exception("BaseArcset::readAccelFromMat: Incompatible data file: unsupported data type/class");
		}
	}
	Mat_VarFree(accelMat);
}//===============================================

/**
 *  @brief Read epoch times from a matlab file in a variable with the specified name
 * 
 *  @param matFile pointer to an open Matlab file
 *  @param varName The name of the variable within the Matlab file
 *  @throws Exception if there are any issues importing the data
 */
void BaseArcset::readEpochFromMat(mat_t *matFile, const char* varName){
	matvar_t *epochMat = Mat_VarRead(matFile, varName);
	if(epochMat == NULL){
		throw Exception("BaseArcset::readEpochFromMat: Could not read data vector");
	}else{
		int numSteps = epochMat->dims[0];

		if(nodes.size() == 0)
			throw Exception("BaseArcset::readEpochFromMat: Node vector has not been initialized");

		if(numSteps != (int)nodes.size())
			throw Exception("BaseArcset::readEpochFromMat: Epoch vector has different size than the initialized node evctor");

		if(epochMat->dims[1] != 1)
			throw Exception("BaseArcset::readEpochFromMat: Incompatible data file: Epoch vector has more than one column");

		if(epochMat->class_type == MAT_C_DOUBLE && epochMat->data_type == MAT_T_DOUBLE){
			double *data = static_cast<double *>(epochMat->data);

			if(data != NULL){
				for(int i = 0; i < numSteps; i++){
					nodes[i].setEpoch(data[i]);
				}
			}
		}else{
			throw Exception("BaseArcset::readEpochFromMat: Incompatible data file: unsupported data type or class");
		}
	}
}//================================================

/**
 *  @brief Read State Transition Matrices from a matlab file
 * 
 *  @param matFile pointer to an open Matlab file
 *  @throws Exception if there are any issues importing the data
 */
void BaseArcset::readSTMFromMat(mat_t *matFile){
	matvar_t *allSTM = Mat_VarRead(matFile, "STM");
	if(allSTM == NULL){
		throw Exception("BaseArcset::readSTMFromMat: Could not read data vector");
	}else{
		int numSteps = allSTM->dims[2];

		if(segs.size() == 0){
			throw Exception("BaseArcset::readSTMFromMat: Step vector has not been initialized!");
		}

		if(numSteps != (int)segs.size()){
			throw Exception("BaseArcset::readSTMFromMat: STM vector has a different size than the initialized step vector");
		}

		if(allSTM->dims[0] != 6 || allSTM->dims[1] != 6){
			throw Exception("BaseArcset::readSTMFromMat: Incompatible data file: STM is not 6x6.");
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
					segs[i].setSTM(P.transpose());
				}
			}
		}else{
			throw Exception("ttpat_arc_data::readSTMFromMat: Incompatible data file: unsupported data type/class");
		}
	}
	Mat_VarFree(allSTM);
}//===============================================

/**
 *  @brief Read times-of-flight from a matlab file in a variable with the specified name
 * 
 *  @param matFile pointer to an open Matlab file
 *  @param varName The name of the variable within the Matlab file
 *  @throws Exception if there are any issues importing the data
 */
void BaseArcset::readTOFFromMat(mat_t *matFile, const char* varName){
	matvar_t *tofMat = Mat_VarRead(matFile, varName);
	if(tofMat == NULL){
		throw Exception("BaseArcset::readTOFFromMat: Could not read data vector");
	}else{
		int numSteps = tofMat->dims[0];

		if(segs.size() == 0)
			throw Exception("BaseArcset::readTOFFromMat: Node vector has not been initialized");

		if(numSteps != (int)segs.size())
			throw Exception("BaseArcset::readTOFFromMat: Epoch vector has different size than the initialized segment evctor");

		if(tofMat->dims[1] != 1)
			throw Exception("BaseArcset::readTOFFromMat: Incompatible data file: Epoch vector has more than one column");

		if(tofMat->class_type == MAT_C_DOUBLE && tofMat->data_type == MAT_T_DOUBLE){
			double *data = static_cast<double *>(tofMat->data);

			if(data != NULL){
				for(int i = 0; i < numSteps; i++){
					segs[i].setTOF(data[i]);
				}
			}
		}else{
			throw Exception("BaseArcset::readTOFFromMat: Incompatible data file: unsupported data type or class");
		}
	}
}//================================================

/**
 *  @brief Read values of the specified extra paramter from a matlab file
 * 
 *  @param matFile pointer to an open Matlab file
 *  @param varIx the index of the extra parameter variable within the <tt>extraParamRowSize</tt> array
 *  @param varName the name of the storage variable within the Matlab file
 *  @throws Exception if there are any issues importing the data
 */
void BaseArcset::readExtraParamFromMat(mat_t *matFile, int varIx, const char *varName){
	if(varIx >= numExtraParam || varIx < 0)
		throw Exception("BaseArcset::readExtraParamFromMat: Could not read extra parameter; index out of bounds");

	// Get starting index of this extra param within a arc step's extra parameter vector
	int ix0 = 0;
	for(int i = 0; i < varIx; i++){ ix0 += extraParamRowSize[i]; }

	matvar_t *matvar = Mat_VarRead(matFile, varName);
	if(matvar == NULL){
		throw Exception("BaseArcset::readExtraParamFromMat: Could not read data vector");
	}else{
		int numSteps = matvar->dims[0];
		
		if(nodes.size() == 0){
			throw Exception("BaseArcset::readExtraParamFromMat: Step vector has not been initialized!");
		}

		if(matvar->dims[1] != ((size_t)extraParamRowSize[varIx])){
			char message[64];
			sprintf(message, "BaseArcset::readExtraParamFromMat: Incompatible data file: %s width is not %d", varName, extraParamRowSize[varIx]);
			throw Exception(message);
		}

		if(matvar->class_type == MAT_C_DOUBLE && matvar->data_type == MAT_T_DOUBLE){
			double *data = static_cast<double *>(matvar->data);
			if(data != NULL){
				for(int i = 0; i < numSteps; i++){
					for(int c = 0; c < extraParamRowSize[varIx]; c++){
						nodes[i].setExtraParam(ix0+c, data[c*numSteps + i]);
					}
				}
			}
		}else{
			throw Exception("BaseArcset::readExtraParamFromMat: Incompatible data file: unsupported data type/class");
		}
	}
	Mat_VarFree(matvar);
}//===============================================

/**
 *	@brief Save the acceleration vector to file
 *	@param matFile a pointer to the destination mat-file
 */
void BaseArcset::saveAccel(mat_t *matFile) const{
	// We store data in row-major order, but the Matlab file-writing algorithm takes data
	// in column-major order, so we transpose our vector and split it into two smaller ones
	std::vector<double> accel_colMaj(3*nodes.size());

	for(size_t r = 0; r < nodes.size(); r++){
		std::vector<double> accel = nodes[r].getAccel();
		for(int c = 0; c < 3; c++){
			accel_colMaj[c*nodes.size() + r] = accel[c];
		}
	}
	
	size_t dims[2] = {nodes.size(), 3};
	matvar_t *matvar = Mat_VarCreate("Accel", MAT_C_DOUBLE, MAT_T_DOUBLE, 2, dims, &(accel_colMaj[0]), MAT_F_DONT_COPY_DATA);
	astrohelion::saveVar(matFile, matvar, "Accel", MAT_COMPRESSION_NONE);
}//=====================================================

/**
 *	@brief Save all node epochs to file
 *	@param matFile a pointer to the destination mat-file
 */
void BaseArcset::saveEpoch(mat_t *matFile) const{
	saveEpoch(matFile, "Epoch");
}//=====================================================

/**
 *	@brief Save all node epochs to file with a specified variable name
 *	@param matFile a pointer to the destination mat-file
 *	@param varName the name of the variable
 */
void BaseArcset::saveEpoch(mat_t *matFile, const char* varName) const{
	std::vector<double> allEpochs(nodes.size());

	for(size_t n = 0; n < nodes.size(); n++){
		allEpochs[n] = nodes[n].getEpoch();
	}
	
	size_t dims[2] = {allEpochs.size(), 1};
	matvar_t *matvar = Mat_VarCreate(varName, MAT_C_DOUBLE, MAT_T_DOUBLE, 2, dims, &(allEpochs[0]), MAT_F_DONT_COPY_DATA);
	astrohelion::saveVar(matFile, matvar, varName, MAT_COMPRESSION_NONE);
}//=====================================================

/**
 *	@brief Save one of the extra parameters to file
 *	@param matFile a pointer to the destination mat-file
 *	@param varIx the index of the parameter
 *	@param name the name of the variable being saved
 *	@throws Exception if <tt>varIx</tt> is out of bounds
 */
void BaseArcset::saveExtraParam(mat_t *matFile, int varIx, const char *name) const{
	if(varIx > numExtraParam || varIx < 0)
		throw Exception("Could not save extra parameter; index out of bounds");

	// Get starting index of this extra param within a arc step's extra parameter vector
	int ix0 = 0;
	for(int i = 0; i < varIx; i++){ ix0 += extraParamRowSize[i]; }

	// Get the specified coordinate
	std::vector<double> param(extraParamRowSize[varIx]*nodes.size());
	for(size_t r = 0; r < nodes.size(); r++){
		std::vector<double> ep  = nodes[r].getExtraParams();
		for(int c = 0; c < extraParamRowSize[varIx];c++){
			// Save NAN (rather than un-allocated memeory) if the index is out of bounds
			if(ix0 + c < (int)(ep.size()))
				param[c*nodes.size() + r] = ep[ix0+c];
			else
				param[c*nodes.size() + r] = NAN;
		}
	}

	size_t dims[2] = {nodes.size(), static_cast<size_t>(extraParamRowSize[varIx])};
	matvar_t *matvar = Mat_VarCreate(name, MAT_C_DOUBLE, MAT_T_DOUBLE, 2, dims, &(param[0]), MAT_F_DONT_COPY_DATA);
	astrohelion::saveVar(matFile, matvar, name, MAT_COMPRESSION_NONE);
}//======================================================

/**
 *	@brief Save the state vector [pos, vel] to a file with variable name "State"
 *	@param matFile a pointer to the destination matlab file 
 */
void BaseArcset::saveState(mat_t *matFile) const{
	saveState(matFile, "State");
}//==================================================

/**
 *	@brief Save the state vector [pos, vel] to a file
 *	@param matFile a pointer to the destination matlab file 
 *	@param varName the name of the variable (e.g. "State" or "Nodes")
 */
void BaseArcset::saveState(mat_t *matFile, const char* varName) const{
	// We store data in row-major order, but the Matlab file-writing algorithm takes data
	// in column-major order, so we transpose our vector and split it into two smaller ones
	std::vector<double> posVel(6*nodes.size());

	for(size_t r = 0; r < nodes.size(); r++){
		std::vector<double> state = nodes[r].getState();
		for(int c = 0; c < 6; c++){
			posVel[c*nodes.size() + r] = state[c];
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
	size_t dims[2] = {nodes.size(), 6};
	matvar_t *matvar = Mat_VarCreate(varName, MAT_C_DOUBLE, MAT_T_DOUBLE, 2, dims, &(posVel[0]), MAT_F_DONT_COPY_DATA);
	astrohelion::saveVar(matFile, matvar, varName, MAT_COMPRESSION_NONE);
}//======================================================

/**
 *	@brief Save the STMs to a file; STMs are stored in a 6x6xn array for 
 *	compatibility with existing MATLAB scripts
 *	@param matFile a pointer to the destination matlab file 
 */
void BaseArcset::saveSTMs(mat_t *matFile) const{
	// Create one large vector to put all the STM elements in
	std::vector<double> allSTMEl(segs.size()*36);

	for (size_t n = 0; n < segs.size(); n++){
		// get the transpose of the STM matrix; we need to store it in column-major order
		// and it's currently in row-major order
		MatrixXRd P = segs[n].getSTM().transpose();
		// Retrieve the data from the matrix
		double *matData = P.data();
		// Store that data in our huge vector
		std::copy(matData, matData+36, &(allSTMEl[0]) + n*36);
	}

	size_t dims[3] = {6, 6, segs.size()};
	matvar_t *matvar = Mat_VarCreate("STM", MAT_C_DOUBLE, MAT_T_DOUBLE, 3, dims, &(allSTMEl[0]), MAT_F_DONT_COPY_DATA);
	astrohelion::saveVar(matFile, matvar, "STM", MAT_COMPRESSION_NONE);
}//======================================================

/**
 *	@brief Save all segment times-of-flight to file with a specified variable name
 *	@param matFile a pointer to the destination mat-file
 *	@param varName the name of the variable
 */
void BaseArcset::saveTOF(mat_t *matFile, const char* varName) const{
	std::vector<double> allTOFs(segs.size());

	for(size_t n = 0; n < segs.size(); n++){
		allTOFs[n] = segs[n].getTOF();
	}
	
	size_t dims[2] = {allTOFs.size(), 1};
	matvar_t *matvar = Mat_VarCreate(varName, MAT_C_DOUBLE, MAT_T_DOUBLE, 2, dims, &(allTOFs[0]), MAT_F_DONT_COPY_DATA);
	astrohelion::saveVar(matFile, matvar, varName, MAT_COMPRESSION_NONE);
}//=====================================================




}// END of Astrohelion namespace