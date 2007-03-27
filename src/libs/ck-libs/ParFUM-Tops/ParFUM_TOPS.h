/**
   @file
   @brief A ParFUM "Tops" compatibility layer API Definition

   @author Isaac Dooley

   ParFUM-TOPS provides a Tops-like API for ParFUM.

\note \code
put example here!
\endcode

@note ::NodeAtt and ::ElemAtt are just replaced with void* for this implementation.

*/

#ifndef __PARFUM_TOPS___H
#define __PARFUM_TOPS___H

#include <ParFUM.h>
#include <ParFUM_internals.h>


#include "ParFUM_TOPS_Types.h"
#include "ParFUM_TOPS_CUDA.h"


/** A tops model is roughly equivalent to a ParFUM FEM_Mesh object */
typedef struct{
    FEM_Mesh *mesh;
    void *mAtt;
    AllocTable2d<unsigned char> *ElemData_T;
    AllocTable2d<unsigned char> *NodeData_T;
    AllocTable2d<int> *ElemConn_T;
    AllocTable2d<FP_TYPE_LOW> *coord_T;
    AllocTable2d<int> *node_id_T;
    AllocTable2d<int> *elem_id_T;


    unsigned node_attr_size;
    unsigned elem_attr_size;
    unsigned model_attr_size;

    /** number of local elements */
    unsigned num_local_elem;
    /** number of local nodes */
    unsigned num_local_node;

#ifdef CUDA
    TopModelDevice device_model;
#endif

} TopModel;



/** Node Iterator */
class TopNodeItr{
public:
  /** The signed index used to refer to a ParFUM Element. Non-negatives are ghosts*/
  int parfum_index;
  /** The associated model */
  TopModel *model;
};

/** @brief Element Iterator. See notes for class TopNodeItr */
class TopElemItr{
public:
  int parfum_index;
    TopModel *model;
};


/**
Create and access a Tops model. Only call from Init
Currently only one model can be created. To extend, each model must just reference a different FEM_Mesh object
*/
TopModel* topModel_Create_Init(int elem_attr_sz, int node_attr_sz);

/** Create and access a Tops model. Only call from Driver */
TopModel* topModel_Create_Driver(int elem_attr_sz, int node_attr_sz, int model_attr_sz, void* mAtt);

/** Cleanup a model. Currently does nothing */
void topModel_Destroy(TopModel* m);

/** Insert a node */
TopNode topModel_InsertNode(TopModel*, float x, float y, float z);
TopNode topModel_InsertNode(TopModel*, double x, double y, double z);

void topModel_SuggestInitialSize(TopModel* m, unsigned numNodes, unsigned numElements);


/** Set id of a node */
void topNode_SetId(TopModel*, TopNode, TopID id);

/** Set attribute of a node */
void topNode_SetAttrib(TopModel*, TopNode, void*);

/** Insert an element */
TopElement topModel_InsertElem(TopModel*, TopElemType, TopNode*);

/** Set id of an element */
void topElement_SetId(TopModel*, TopElement, TopID id);

/** Set attribute of an element */
void topElement_SetAttrib(TopModel*, TopElement, void*);

/** Get node via id */
TopNode topModel_GetNodeAtId(TopModel*,TopID);

/** Get nodal attribute */
void* topNode_GetAttrib(TopModel*, TopNode);

/** Get element attribute */
void* topElement_GetAttrib(TopModel*, TopElement);

TopNode topElement_GetNode(TopModel*,TopElement,int idx);

int topNode_GetId(TopModel* m, TopNode n);

int topModel_GetNNodes(TopModel *model);

int topElement_GetNNodes(TopModel* model, TopElement elem);

void topNode_GetPosition(TopModel*model, TopNode node,float*x,float*y,float*z);
void topNode_GetPosition(TopModel*model, TopNode node,double*x,double*y,double*z);

/** Create Iterator for nodes */
TopNodeItr*  topModel_CreateNodeItr(TopModel*);

/** Destroy Iterator */
void topNodeItr_Destroy(TopNodeItr*);

/** Initialize Iterator */
void topNodeItr_Begin(TopNodeItr*);

/** Determine if Iterator is valid or if it has iterated past last Node */
bool topNodeItr_IsValid(TopNodeItr*);

/** Increment iterator */
void topNodeItr_Next(TopNodeItr*);

/** Get TopNode associated with the iterator */
TopNode topNodeItr_GetCurr(TopNodeItr*);


/** Create Iterator for elements */
TopElemItr*  topModel_CreateElemItr(TopModel*);

/** Destroy Iterator */
void topElemItr_Destroy(TopElemItr*);

/** Initialize Iterator */
void topElemItr_Begin(TopElemItr*);

/** Determine if Iterator is valid or if it has iterated past last Element */
bool topElemItr_IsValid(TopElemItr*);

/** Increment iterator */
void topElemItr_Next(TopElemItr*);

/** Get TopElement associated with the iterator */
TopElement topElemItr_GetCurr(TopElemItr*);

/** Perform sanity check on iterators. This checks to make sure that the count of the itereated elements and nodes matches that returned by ParFUM's countValid() */
void topModel_TestIterators(TopModel*m);


void top_retreive_elem_data(TopModel* m);
void top_retreive_node_data(TopModel* m);

#endif