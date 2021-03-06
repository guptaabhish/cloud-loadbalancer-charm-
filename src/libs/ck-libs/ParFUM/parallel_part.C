/**
 * \addtogroup ParFUM
*/
/*@{*/

/**
  This file contains code for parallely partitioning
  the initial mesh into multiple chunks 
  It uses PARMETIS to do the actual partitioning.
  It also uses the MSA framework to do data transfer.
  Author Sayantan Chakravorty
  05/30/2004
*/

#include "ParFUM.h"
#include "ParFUM_internals.h"
#include "MsaHashtable.h"

#include <parmetis.h>

double elemlistaccTime=0;
extern void clearPartition(void);

int FEM_Mesh_Parallel_broadcast(int fem_mesh,int masterRank,FEM_Comm_t comm_context){
  int myRank;
  MPI_Comm_rank((MPI_Comm)comm_context,&myRank);
  //printf("[%d] FEM_Mesh_Parallel_broadcast called for mesh %d\n",myRank,fem_mesh);
  int new_mesh;
  if(myRank == masterRank){
    //I am the master, i have the element connectivity data and need
    //to send it to everybody
    printf("[%d] Memory usage on vp 0 at the begining of partition %d \n",CkMyPe(),CmiMemoryUsage());
    new_mesh=FEM_master_parallel_part(fem_mesh,masterRank,comm_context);
		
  }else{
    new_mesh=FEM_slave_parallel_part(fem_mesh,masterRank,comm_context);
  }
  //temp to keep stuff from falling apart
  MPI_Barrier((MPI_Comm)comm_context);
	if(myRank == masterRank){
		clearPartition();
	}
  //printf("[%d] Partitioned mesh number %d \n",myRank,new_mesh);
  return new_mesh;
}

int FEM_master_parallel_part(int fem_mesh,int masterRank,FEM_Comm_t comm_context){
  const char *caller="FEM_Create_connmsa"; 
  FEMAPI(caller);
  FEM_chunk *c=FEM_chunk::get(caller);
  FEM_Mesh *m=c->lookup(fem_mesh,caller);
  m->setAbsoluteGlobalno();
  int nelem = m->nElems();
  int numChunks;
  MPI_Comm_size((MPI_Comm)comm_context,&numChunks);
  printf("master -> number of elements %d \n",nelem);
  DEBUG(m->print(0));


  /*load the connectivity information into the eptr and
    eind datastructure. It will be read by the other slave 
    elements and used to call parmetis*/
  MSA1DINT eptrMSA(nelem,numChunks);
  MSA1DINT eindMSA(nelem*10,numChunks);
  /*
    after the msa array has been created and loaded with connectivity data
    tell the slaves about the msa array 
  */
  struct conndata data;
  data.nelem = nelem;
  data.nnode = m->node.size();
  data.arr1 = eptrMSA;
  data.arr2 = eindMSA;
  MPI_Bcast_pup(data,masterRank,(MPI_Comm)comm_context);

  eptrMSA.enroll(numChunks);
  eindMSA.enroll(numChunks);
  MSA1DINT::Write wPtr = eptrMSA.getInitialWrite();
  MSA1DINT::Write wInd = eindMSA.getInitialWrite();
  int indcount=0,ptrcount=0;
  for(int t=0;t<m->elem.size();t++){
    if(m->elem.has(t)){
      FEM_Elem &k=m->elem[t];
      for(int e=0;e<k.size();e++){
				wPtr.set(ptrcount)=indcount;
				ptrcount++;
				for(int n=0;n<k.getNodesPer();n++){
				  wInd.set(indcount)=k.getConn(e,n);
				  indcount++;
				}
      }
    }
  }
  wPtr.set(ptrcount) = indcount;
  printf("master -> ptrcount %d indcount %d sizeof(MSA1DINT) %d sizeof(MSA1DINTLIST) %d memory %d\n",ptrcount,indcount,sizeof(MSA1DINT),sizeof(MSA1DINTLIST),CmiMemoryUsage());
  /*
    break up the mesh such that each chunk gets the same number of elements
    and the nodes corresponding to those elements. However this is not the partition.
    This is just distributing the data, so that when partition is done using parmetis
    all the requests for data do not go to chunk 0. Instead after partition each chunk
    can send the element and node data to the chunks that will need it
  */
  FEM_Mesh *mesh_array=FEM_break_mesh(m,ptrcount,numChunks);
  /*
    Send the broken up meshes to the different chunks. 
  */
  sendBrokenMeshes(mesh_array,comm_context);
  delete [] mesh_array;
  FEM_Mesh mypiece;
  MPI_Recv_pup(mypiece,masterRank,MESH_CHUNK_TAG,(MPI_Comm)comm_context);
	
  /*
    call parmetis
  */
  double  parStartTime = CkWallTimer();
  MSA1DINT::Read rPtr = wPtr.syncToRead();
  MSA1DINT::Read rInd = wInd.syncToRead();
  printf("starting FEM_call_parmetis \n");
  struct partconndata *partdata = FEM_call_parmetis(data.nelem, rPtr, rInd, comm_context);

  printf("done with parmetis %d FEM_Mesh %d in %.6lf \n",CmiMemoryUsage(),sizeof(FEM_Mesh),CkWallTimer()-parStartTime);
	
	double dataArrangeStartTime = CkWallTimer();
  /*
    Set up a msa to store the partitions to which a node belongs.
    A node can belong to multiple partitions.
  */
  int totalNodes = m->node.size();
  MSA1DINTLIST nodepart(totalNodes,numChunks);
  MPI_Bcast_pup(nodepart,masterRank,(MPI_Comm)comm_context);
  nodepart.enroll(numChunks);
  MSA1DINTLIST::Accum nodepartAcc = nodepart.getInitialAccum();
	
  FEM_write_nodepart(nodepartAcc,partdata,(MPI_Comm)comm_context);
  printf("Creating mapping of node to partition took %.6lf\n",CkWallTimer()-dataArrangeStartTime);
  dataArrangeStartTime = CkWallTimer();
  MSA1DINTLIST::Read nodepartRead = nodepartAcc.syncToRead();
	
  /*
    Set up a msa to store the nodes that belong to a partition
  */
  MSA1DNODELIST part2node(numChunks,numChunks);
  MPI_Bcast_pup(part2node,masterRank,(MPI_Comm)comm_context);
  part2node.enroll(numChunks);
  MSA1DNODELIST::Accum part2nodeAcc = part2node.getInitialAccum();

  FEM_write_part2node(nodepartRead, part2nodeAcc, partdata, (MPI_Comm)comm_context);

	
  /*
    Get the list of elements and nodes that belong to this partition
  */
  MSA1DNODELIST::Read rPart2node = part2nodeAcc.syncToRead();
  NodeList lnodes = rPart2node.get(masterRank);
  lnodes.uniquify();
//  IntList lelems = part2elem.get(masterRank);
	

	printf("Creating mapping of  partition to node took %.6lf\n",CkWallTimer()-dataArrangeStartTime);
  printf("Time spent doing +=ElemList %.6lf \n",elemlistaccTime);
	dataArrangeStartTime = CkWallTimer();

  /*
    Build an MSA of FEM_Mesh, with each index containing the mesh for that  chunk
  */
  MSA1DFEMMESH part2mesh(numChunks,numChunks);
  MPI_Bcast_pup(part2mesh,masterRank,(MPI_Comm)comm_context);
  part2mesh.enroll(numChunks);
  MSA1DFEMMESH::Accum aPart2mesh = part2mesh.getInitialAccum();

  FEM_write_part2mesh(aPart2mesh,partdata, &data,nodepartRead,numChunks,masterRank,&mypiece);
  /*
    Get your mesh consisting of elements and nodes out of the mesh MSA
  */
  MSA1DFEMMESH::Read rPart2mesh = aPart2mesh.syncToRead();
  MeshElem me = rPart2mesh.get(masterRank);
  //printf("[%d] Number of elements in my partitioned mesh %d number of nodes %d \n",masterRank,me.m->nElems(),me.m->node.size());
	
  DEBUG(printf("[%d] Memory usage on vp 0 close to max %d \n",CkMyPe(),CmiMemoryUsage()));
	//Free up the eptr and eind MSA arrays stored in data
  delete &rPtr;
  delete &rInd;
  data.arr1.FreeMem();
  data.arr2.FreeMem();
  nodepart.FreeMem();
  DEBUG(printf("[%d] Memory usage on vp 0 after FreeMem %d \n",CkMyPe(),CmiMemoryUsage()));
	
  addIDXLists(me.m,lnodes,masterRank);
	
	part2node.FreeMem();
  DEBUG(printf("[%d] Memory usage on vp 0 after addIDXL %d \n",CkMyPe(),CmiMemoryUsage()));
	
  /*
    Broadcast  the user data to all the meshes
  */
  DEBUG(printf("[%d] Length of udata vector in master %d \n",masterRank,m->udata.size()));
  MPI_Bcast_pup(m->udata,masterRank,(MPI_Comm)comm_context);
  me.m->udata = m->udata;
	
	
  delete partdata;
  
	printf("[%d] Data Arrangement took %.6lf \n",masterRank,CkWallTimer()-dataArrangeStartTime);
	
	/*
    collect the ghost data and send it to all the chunks.
  */
  struct ghostdata *gdata = gatherGhosts();
  DEBUG(printf("[%d] number of ghost layers %d \n",masterRank,gdata->numLayers));
  MPI_Bcast_pup(*gdata,masterRank,(MPI_Comm)comm_context);

  /*
    make ghosts for this mesh
  */
  printf("[%d] Starting to generate number of ghost layers %d \n",masterRank,gdata->numLayers);
	double _startTime = CkWallTimer();
  makeGhosts(me.m,(MPI_Comm)comm_context,masterRank,gdata->numLayers,gdata->layers);
  delete gdata;
	
	printf("[%d] Ghost generation took %.6lf \n",masterRank,CkWallTimer()-_startTime);
	
  me.m->becomeGetting();
  FEM_chunk *chunk = FEM_chunk::get("FEM_Mesh_Parallel_broadcast");
  int tempMeshNo = chunk->meshes.put(me.m);
  int new_mesh = FEM_Mesh_copy(tempMeshNo);
	
  FEM_Mesh *nmesh = c->lookup(new_mesh,"master_parallel_broadcast");
  DEBUG(printf("[%d] Length of udata vector in master new_mesh %d \n",masterRank,nmesh->udata.size()));
	
	part2mesh.FreeMem();
  printf("[%d] Max Memory usage on vp 0 at end of parallel partition %d \n",CkMyPe(),CmiMaxMemoryUsage());
		
  return new_mesh;
}

int FEM_slave_parallel_part(int fem_mesh,int masterRank,FEM_Comm_t comm_context){
  int myRank;
  MPI_Comm_rank((MPI_Comm)comm_context,&myRank);
  int numChunks;
  MPI_Comm_size((MPI_Comm)comm_context,&numChunks);
		
  /*Receive the name of the msa arrays that contain the
    connectivity information*/
  struct conndata data;
  MPI_Bcast_pup(data,masterRank,(MPI_Comm)comm_context);
  data.arr1.enroll(numChunks);
  data.arr2.enroll(numChunks);
  DEBUG(printf("Recv -> %d \n",data.nelem));
  /*
    Receive the broken up mesh from the masterRank.
    These will be used later to give each partitioned mesh
    its elements and data.
  */
  FEM_Mesh mypiece;
  MPI_Recv_pup(mypiece,masterRank,MESH_CHUNK_TAG,(MPI_Comm)comm_context);
	
  /*
    call parmetis and get the resuts back from it
  */
  MSA1DINT::Read rPtr = data.arr1.getInitialWrite().syncToRead();
  MSA1DINT::Read rInd = data.arr1.getInitialWrite().syncToRead();
  struct partconndata *partdata = FEM_call_parmetis(data.nelem, rPtr, rInd, comm_context);
	
  /*
    write to the msa that contains the partitions to which a node belongs
  */
  MSA1DINTLIST nodepart;
  MPI_Bcast_pup(nodepart,masterRank,(MPI_Comm)comm_context);
  nodepart.enroll(numChunks);
  MSA1DINTLIST::Accum nodepartAcc = nodepart.getInitialAccum();
	
  FEM_write_nodepart(nodepartAcc,partdata,(MPI_Comm)comm_context);
	
  /*
    write to the msa that stores the nodes that belong to each partition
  */

  MSA1DNODELIST part2node;
  MPI_Bcast_pup(part2node,masterRank,(MPI_Comm)comm_context);
  part2node.enroll(numChunks);
  MSA1DNODELIST::Accum part2nodeAcc = part2node.getInitialAccum();
  MSA1DINTLIST::Read nodepartRead = nodepartAcc.syncToRead();


  FEM_write_part2node(nodepartRead, part2nodeAcc, partdata, (MPI_Comm)comm_context);

  /*
    Get the list of elements and nodes that belong to this partition
  */
  MSA1DNODELIST::Read part2nodeRead = part2nodeAcc.syncToRead();
  NodeList lnodes = part2nodeRead.get(myRank);
  lnodes.uniquify();
//  IntList lelems = part2elem.get(myRank);

  /*
    Get the FEM msa and write the different mesh
  */
  MSA1DFEMMESH part2mesh;
  MPI_Bcast_pup(part2mesh,masterRank,(MPI_Comm)comm_context);
  part2mesh.enroll(numChunks);
  MSA1DFEMMESH::Accum aPart2mesh = part2mesh.getInitialAccum();
  FEM_write_part2mesh(aPart2mesh, partdata, &data, nodepartRead,numChunks, myRank, &mypiece);
	
  /*
    Get your mesh consisting of elements and nodes out of the mesh MSA
  */
  MSA1DFEMMESH::Read rPart2mesh = aPart2mesh.syncToRead();
  MeshElem me = rPart2mesh.get(myRank);
  //printf("[%d] Number of elements in my partitioned mesh %d number of nodes %d \n",myRank,me.m->nElems(),me.m->node.size());
	
	//Free up the eptr and eind MSA arrays stored in data
  delete &rPtr;
  delete &rInd;
	data.arr1.FreeMem();
	data.arr2.FreeMem();
	nodepart.FreeMem();
	
  addIDXLists(me.m,lnodes,myRank);
	
  /*
    Receive the user data from master
  */
  MPI_Bcast_pup(me.m->udata,masterRank,(MPI_Comm)comm_context);
  DEBUG(printf("[%d] Length of udata vector %d \n",myRank,me.m->udata.size()));
	
  delete partdata;
	
  struct ghostdata *gdata = new ghostdata;
  MPI_Bcast_pup(*gdata,masterRank,(MPI_Comm)comm_context);
  //printf("[%d] number of ghost layers %d \n",myRank,gdata->numLayers);
	
  /*
    make ghosts
  */
  makeGhosts(me.m,(MPI_Comm )comm_context,masterRank,gdata->numLayers,gdata->layers);
	
  me.m->becomeGetting();
  FEM_chunk *chunk = FEM_chunk::get("FEM_Mesh_Parallel_broadcast");
  int tempMeshNo = chunk->meshes.put(me.m);
  int new_mesh = FEM_Mesh_copy(tempMeshNo);


	part2mesh.FreeMem();
  delete gdata;
  return new_mesh;
}

/*
  Function that reads in the data from the msa array and calls
  parmetis. It returns the partition and the connectivity of
  the elements for which this processor is responsible.
*/
struct partconndata * FEM_call_parmetis(int nelem, MSA1DINT::Read &rPtr, MSA1DINT::Read &rInd, FEM_Comm_t comm_context)
{
  int myRank,numChunks;
  MPI_Comm_size((MPI_Comm)comm_context,&numChunks);
  MPI_Comm_rank((MPI_Comm)comm_context,&myRank);
	
  /*
    Setup the elmdist array. All processors
    get equal number of elements. This is not
    the partition but just a map which stores the
    elements that a processor is responsible for,ie
    supplies the data for in the call to parmetis.
  */
  int *elmdist = new int[numChunks+1];
  DEBUG(printf("[%d] elmdist \n",myRank));
  for(int i=0;i<=numChunks;i++){
    elmdist[i] = (i*nelem)/numChunks;
    DEBUG(printf(" %d ",elmdist[i]));
  }
  DEBUG(printf("\n"));
  int startindex = elmdist[myRank];
  int endindex = elmdist[myRank+1];
  int numindices = endindex - startindex;
  int *eptr = new int[numindices+1];
  /*
    Read the msa arrays to extract the data
    Store it in the eptr and eind arrays
  */
  int startConn = rPtr.get(startindex);
  int endConn = rPtr.get(endindex);
  int numConn = endConn - startConn;
  int *eind = new int[numConn];
  DEBUG(printf("%d startindex %d endindex %d startConn %d endConn %d \n",myRank,startindex,endindex,startConn,endConn));
  for(int i=startindex;i<endindex;i++){
    int conn1 = rPtr.get(i);
    int conn2 = rPtr.get(i+1);
    eptr[i-startindex] = conn1 - startConn;
    for(int j=conn1;j<conn2;j++){
      eind[j-startConn] = rInd.get(j);
    }
  }
  eptr[numindices] = endConn - startConn;
  /*
    printf("%d eptr ",myRank);
    for(int i=0;i<=numindices;i++){
    printf(" %d",eptr[i]);
    }
    printf("\n");

    printf("%d eind ",myRank);
    for(int i=0;i<numConn;i++){
    printf(" %d",eind[i]);
    }
    printf("\n");
  */
  int wgtflag=0,numflag=0,ncon=1,ncommonnodes=2,options[5],edgecut=0;
  double ubvec = 1.05;
  double *tpwgts = new double[numChunks];
  int *parts = new int[numindices+1];
  for(int i=0;i<numChunks;i++){
    tpwgts[i]=1/(double)numChunks;
  }
  options[0]=0;
  MPI_Barrier((MPI_Comm)comm_context);
  ParMETIS_V3_PartMeshKway (elmdist,eptr,eind,NULL,&wgtflag,&numflag,&ncon,&ncommonnodes,&numChunks,tpwgts,&ubvec,options,&edgecut,parts,(MPI_Comm *)&comm_context);
  DEBUG(CkPrintf("%d partition ",myRank);)
    for(int i=0;i<numindices;i++){
      DEBUG(CkPrintf(" <%d %d> ",i+startindex,parts[i]));
    }
  DEBUG(CkPrintf("\n"));
  delete []elmdist;
  delete []tpwgts;
  struct partconndata *retval = new struct partconndata;
  retval->nelem = numindices;
  retval->eind = eind;
  retval->eptr = eptr;
  retval->part = parts;
  retval->startindex = startindex;
  return retval;
}

/*
  Write the partition number of the nodes to the msa array nodepart
  A node might belong to more than one partition
*/
void FEM_write_nodepart(MSA1DINTLIST::Accum &nodepart,struct partconndata *data,MPI_Comm comm_context){
  for(int i=0;i<data->nelem;i++){
    int start=data->eptr[i];
    int end = data->eptr[i+1];
    for(int j=start;j<end;j++){
	nodepart(data->eind[j]) += data->part[i];
     	DEBUG(printf(" write_nodepart %d %d \n",data->eind[j],data->part[i]));
    }
  }
}

/*
  Read the msa array written in FEM_write_nodepart and for each node
  write it to the msa array containing the nodes for each partition
*/
void FEM_write_part2node(MSA1DINTLIST::Read &nodepart,
			 MSA1DNODELIST::Accum &part2node,
			 struct partconndata *data,
			 MPI_Comm comm_context)
{
  int nodes = nodepart.length();
  int myRank,numChunks;
  /*
    Read in the data from the msa array containing the node to partition mapping
    Next write the data to the partition to node mapping
  */
  MPI_Comm_rank(comm_context,&myRank);
  MPI_Comm_size(comm_context,&numChunks);
  int start = (nodes*myRank)/numChunks;
  int end = (nodes*(myRank+1))/numChunks;
  for(int i=start;i<end;i++){
    IntList t = nodepart.get(i);
		t.uniquify();
    int num=0;
    if(t.vec->size()>1){
      num = t.vec->size();
    }
    NodeElem n(i,num);
    if(num != 0){
      for(int k=0;k<t.vec->size();k++){
	n.shared[k] =(*t.vec)[k];
      }
    }	

    for(int j=0;j<t.vec->size();j++){
      UniqElemList <NodeElem> en(n);
      part2node((*t.vec)[j]) += en;
    }
  }
  DEBUG(printf("done write_part2node\n"));
}

/*
  Read the element partition data and write it to the msa
*/
void FEM_write_part2elem(MSA1DINTLIST::Accum &part2elem,struct partconndata *data,MPI_Comm comm_context)
{
  for(int i=0;i<data->nelem;i++){
      part2elem(data->part[i]) += data->startindex+i;
  }
}

/*
  Break the mesh up into numChunks pieces randomly.
  Pass  nEl/numChunks elements to each 
  chunk.  Next pass numNodes/numChunks to each mesh
*/
FEM_Mesh * FEM_break_mesh(FEM_Mesh *m,int numElements,int numChunks){
  FEM_Mesh *mesh_array = new FEM_Mesh[numChunks];
  int *elmdist = new int[numChunks+1];
  int *nodedist = new int[numChunks+1];
  int numNodes = m->node.size();
	
  for(int i=0;i<=numChunks;i++){
    elmdist[i] = (i*numElements)/numChunks;
    nodedist[i] = (i*numNodes)/numChunks;
  }

  int elcount=0;
  int mcount=0;
  mesh_array[mcount].copyShape(*m);
  for(int t=0;t<m->elem.size();t++){
    if(m->elem.has(t)){
      FEM_Elem &k=m->elem[t];
      for(int e=0;e<k.size();e++){
	mesh_array[mcount].elem[t].push_back(k,e);
	elcount++;
	if(elcount == elmdist[mcount+1]){
	  mcount++;
	  if(mcount != numChunks){
	    mesh_array[mcount].copyShape(*m);
	  }
	}

      }
    }
  }
  mcount=0;
  for(int i=0;i<m->node.size();i++){
    if(i == nodedist[mcount+1]){
      mcount++;
    }
    mesh_array[mcount].node.push_back(m->node,i);
  }
  delete [] elmdist;
  delete [] nodedist;
  return mesh_array;
}
/*
  Send out the meshes to all the different chunks except the master 
*/
void sendBrokenMeshes(FEM_Mesh *mesh_array,FEM_Comm_t comm_context){
  int numChunks;
  MPI_Comm_size((MPI_Comm)comm_context,&numChunks);
  for(int i=0;i<numChunks;i++){
    MPI_Send_pup(mesh_array[i],i,MESH_CHUNK_TAG,(MPI_Comm)comm_context);
  }
}

void FEM_write_part2mesh(MSA1DFEMMESH::Accum &part2mesh,
			 struct partconndata *partdata,
			 struct conndata *data,
			 MSA1DINTLIST::Read &nodepart,
			 int numChunks,
			 int myChunk,
			 FEM_Mesh *m)
{
  int count=0;
  // reading my part of the broken mesh and sending the element data to the
  // mesh that actually should have it according to parmetis
  for(int t=0;t<m->elem.size();t++){
    if(m->elem.has(t)){
      const FEM_Elem &k=(m)->elem[t];
      for(int e=0;e<k.size();e++){
	int chunkID = partdata->part[count];
        MeshElem::ElemInfo ei(m, e, t);
        part2mesh(chunkID) += ei;
        count++;
      }
    }
  }
  // send out the nodes that I have the data for to the meshes that have them
  int startnode=(myChunk * data->nnode)/numChunks;
  for(int i=0;i<m->node.size();i++){
    IntList chunks = nodepart.get(i+startnode);
		chunks.uniquify();
    for(int j=0;j<chunks.vec->size();j++){
      MeshElem::NodeInfo ni(m, i);
      part2mesh((*(chunks.vec))[j]) += ni;
    }
  }
}

void sortNodeList(NodeList &lnodes){
  CkVec<NodeElem> *vec = lnodes.vec;
  vec->quickSort();
}


void addIDXLists(FEM_Mesh *m,NodeList &lnodes,int myChunk){
	double startAddIDXL = CkWallTimer();	
  /*
    Sort the nodelist by global number
  */
  sortNodeList(lnodes);
	DEBUG(printf("[%d] Sorting finished %.6lfs after starting addIDXL\n",myChunk,CkWallTimer()-startAddIDXL));
	
  CkVec<NodeElem> *vec = lnodes.vec;
  /*
    create Hashtable that maps global node number to local number
  */
  CkHashtableT<CkHashtableAdaptorT<int>,int> global2local;
  for(int i=0;i<m->node.size();i++){
    int nodeno = m->node.getGlobalno(i);
    global2local.put(nodeno)=i;
  }
  /*
    For each node, if it is shared, add it to the IDXL_List of 
    the correct chunk in my mesh 
    At the same time, set the primary flag for each node
    A node is defined as primary if it is not shared with any
    other chunk, which has a lower chunk number
  */
  /// go through each node
  for(int i=0;i<vec->size();i++){
    ///global number of this node
    int global = (*vec)[i].global;
    ///find the local number of this node
    int local = global2local.get(global);
    //by default set the node as primary
    m->node.setPrimary(local,true);
    // check if it is shared with other nodes		
    if(((*vec)[i]).numShared > 0){
      ///chunk numbers to which this node belongs
//      int *shared = &(((*vec)[i].shared)[0]);
			DEBUG(printf("[%d] Global node %d local %d node is shared \n",myChunk,global,local));
      for(int j=0;j<((*vec)[i]).numShared;j++){
	if((*vec)[i].shared[j] != myChunk){
	  //add this node to my CommunicatioList for this globalChunk
	  m->node.shared.addList((*vec)[i].shared[j]).push_back(local);
	}
	if((*vec)[i].shared[j] < myChunk){
	  m->node.setPrimary(local,false);
	}
      }
    }
  }
  DEBUG(m->node.shared.print());

  /*
    Fix the connectivity of the elements, replace global numbers by local
    numbers
  */
  for(int i=0;i<m->elem.size();i++){
    if(m->elem.has(i)){
      FEM_Elem &el=m->elem[i];
      for(int j=0;j<el.size();j++){
	int *conn = el.connFor(j);
	for(int k=0;k<el.getNodesPer();k++){
	  int gnode = conn[k];
	  int lnode = global2local.get(gnode);
	  conn[k] = lnode;
	}
      }
    }
  }
	DEBUG(printf("[%d] Time for addIDXL %.6lfs\n",myChunk,CkWallTimer()-startAddIDXL));
}

/*
  gather up the ghost layers on the masterRank and send it to 
  the remote processors. At the moment not doing ghost stencils
*/

struct ghostdata *gatherGhosts(){
  struct ghostdata *res = new struct ghostdata;
  FEM_Partition &part = FEM_curPartition();
  res->numLayers = part.getRegions();
  res->layers = new FEM_Ghost_Layer *[res->numLayers];
  int count=0;
	
  for(int i=0;i<res->numLayers;i++){
    const FEM_Ghost_Region &region = part.getRegion(i);
    if(region.layer != NULL){
      res->layers[count]=region.layer;
      count++;
    }
  }	
  res->numLayers = count;
	DEBUG(printf("gatherGhosts found %d layers \n",res->numLayers));
	printf("gatherGhosts found %d layers \n",res->numLayers);
  return res;
}

/*
  Generate the ghost elements for the given mesh.
  The Mesh has elements and nodes, which have been
  globally numbered and IDXL Lists for the shared nodes
  between the different chunks.
  The number of ghost layers and the ghost layers also need
  to be passed in
*/

double listSearchTime=0;
double sharedSearchTime=0;

void makeGhosts(FEM_Mesh *m, MPI_Comm comm, int masterRank, int numLayers, FEM_Ghost_Layer **layers)
{
  int myChunk;
  int numChunks;
  MPI_Comm_rank((MPI_Comm)comm,&myChunk);
  MPI_Comm_size((MPI_Comm)comm,&numChunks);

	double _startTime=CkWallTimer();
	
  if(numChunks == 1){
    return;
  }
	
  /*
    Go through the shared node list and count the number of unique shared nodes owned
    by this chunk. If no chunk with a lower id shares a node it is owned by this chunk
  */
  FEM_Comm *shared = &m->node.shared;
  int count=0;
  CkHashtableT<CkHashtableAdaptorT<int>,char>	countedSharedNode;
  for(int i=0;i<shared->size();i++){
    const FEM_Comm_List &list = shared->getLocalList(i);
    /* count a shared node only if your chunk number is less than 
       the other chunk with which the node is shared and its value  
       in countSharedNode is 0. Mark its value in countSharedNode as 1.
       If the other chunk has a  lower id and countSharedNode is 1
       subtract 1 from count 
       mark countedSharedNode as 2.
    */
    if(list.getDest() > myChunk){
      for(int j=0;j<list.size();j++){
	if(countedSharedNode.get(list[j]) == 0){
	  count++;
	  countedSharedNode.put(list[j]) = 1;
	}
      }	
    }else{
      for(int j=0;j<list.size();j++){
	if(countedSharedNode.get(list[j]) == 1){
	  count--;
	}
	countedSharedNode.put(list[j]) = 2;
      }
    }
  }
  int totalShared; // total number of shared nodes over all chunks
  MPI_Allreduce(&count, &totalShared,1, MPI_INT,
		MPI_SUM, comm);
  //printf("[%d] Total number of shared nodes %d \n",myChunk,totalShared);
  /*
    Add the FEM_CHUNK attribute to all the nodes and elements in the mesh
  */
  FEM_IndexAttribute *nchunkattr = (FEM_IndexAttribute *)m->node.lookup(FEM_CHUNK,"makeGhosts");
  int *nchunk = nchunkattr->get().getData();
  for(int i=0;i<nchunkattr->getLength();i++){
    nchunk[i] = myChunk;
  }
  for(int e = 0; e < m->elem.size();e++){
    if(m->elem.has(e)){
      FEM_IndexAttribute *echunkattr = (FEM_IndexAttribute *)m->elem[e].lookup(FEM_CHUNK,"makeGhosts");
      int *echunk = echunkattr->get().getData();
      for(int i=0;i<echunkattr->getLength();i++){
	echunk[i] = myChunk;
      }
    }
  }
	
  /*create a hashtable that maps from global node number to local 
    node number. 
    A node with global number i and local number j is stored as global2local(i)=j+1.
    This is done so that 0 might be used to represent a node whose local number is not
    known. 
  */	
  CkHashtableT<CkHashtableAdaptorT<int>,int> global2local;
  for(int i=0;i<m->node.size();i++){
    global2local.put(m->node.getGlobalno(i))=i+1;
  }
	if(myChunk == 0){
		printf("Time to do preprocessing for ghosts %.6lf \n",CkWallTimer()-_startTime);
	}
  for(int i=0;i<numLayers;i++){
    if(myChunk == 0){printf("[%d] Starting ghost layer %d \n",myChunk,i);}
    _startTime = CkWallTimer();
    makeGhost(m,comm,masterRank,totalShared,layers[i],countedSharedNode,global2local); 
    if(myChunk == 0){
       printf("[%d] Making ghost layer %d took %.6lf listSearch %.6lf sharedSearchTime %.6lf \n",myChunk,i,CkWallTimer()-_startTime,listSearchTime,sharedSearchTime);
       listSearchTime = 0;
       sharedSearchTime=0;
    }
  }
}

/* 
   Does this list contain this entry
*/
bool listContains(FEM_Comm_List &list,int entry){
	double _startTime=CkWallTimer();
  for(int i=0;i<list.size();i++){
    if(entry == list[i]){
			listSearchTime += (CkWallTimer()-_startTime);
      return true;
    }
  }
	listSearchTime += (CkWallTimer()-_startTime);
  return false;
}

void makeGhost(FEM_Mesh *m, 
	       MPI_Comm comm,
	       int masterRank,
	       int totalShared,
	       FEM_Ghost_Layer *layer,
	       CkHashtableT<CkHashtableAdaptorT<int>,char> &sharedNode,
	       CkHashtableT<CkHashtableAdaptorT<int>,int> &global2local)
{
  int myChunk;
  int numChunks;
  MPI_Comm_rank((MPI_Comm)comm,&myChunk);
  MPI_Comm_size((MPI_Comm)comm,&numChunks);

  /*
    The master should create the distributed hashtable and tell
    all the other chunks about it
  */
  MsaHashtable *distTab;
  if(myChunk == masterRank){
    distTab = new MsaHashtable(totalShared, numChunks);	
  }else{
    distTab = new MsaHashtable;
  }
  MPI_Bcast_pup(*distTab,masterRank,comm);
  distTab->enroll(numChunks);
  DEBUG(printf("[%d] distributed table calling sync \n",myChunk));


  //	distTab->table.sync((numChunks == 1));
  MsaHashtable::Add aDistTab = distTab->getInitialAdd();

  DEBUG(printf("Chunk %d Mesh: *********************************** \n",myChunk));
  //DEBUG(m->print(0));
  DEBUG(printf("**********************************\n"));
	
  //printf("Chunk %d Ghost layer nodesPerTuple %d numSlots %d \n",myChunk,layer->nodesPerTuple,distTab->numSlots);
  /*
    Go through all the elements and enumerate the tuples and 
    check if all the nodes in a tuple are shared. If so add it
    to the hashtable
  */
  CkVec<Hashnode::tupledata> tupleVec; //store the tuples that are possible ghosts and are added to the table
  CkVec<int> indexVec; // the indices to which these tuples are added
  CkVec<int> elementVec; //store the type and element no to which the tuple belonged ith tuple -> 2*i = type 2*i+1 = number

  for(int i=0;i<m->elem.size();i++){
    if(m->elem.has(i)){
      //does this type of element have ghosts in this layer
      if(layer->elem[i].add){
	//for each element of this type
	for(int e=0;e<m->elem[i].size();e++){
	  const int *conn = m->elem[i].connFor(e);
	  // for each tuple in this element
	  for(int t=0;t< layer->elem[i].tuplesPerElem;t++){
	    bool possibleGhost=true;
	    // the tuple with the global number of the nodes
	    int globalNodeTuple[Hashnode::tupledata::MAX_TUPLE]; 
						
	    //for each node in a tuple check if it is shared
	    // unless all the nodes in the tuple are shared
	    // this tuple cant make the element containing it
	    // a ghost element
	    int nodesPerTuple = layer->nodesPerTuple;
	    for(int n = 0;n<layer->nodesPerTuple;n++){
	      int nodeindex=(layer->elem[i].elem2tuple)[t*layer->nodesPerTuple+n];
	      /*
		The number of nodes in a tuple might be less than the layer->nodesPerTuple
		This is represented by having the last nodes marked as -1
	      */
	      if(nodeindex == -1){
		nodesPerTuple--;
		globalNodeTuple[n] =-1;
		continue;
	      }
	      int node=conn[nodeindex];
	      globalNodeTuple[n]=m->node.getGlobalno(node);
	      if(sharedNode.get(node) == 0){
		possibleGhost = false;
		break;
	      }
	    }
	    // if the tuple is a possible ghost add it to the distributed hashtable
	    if(possibleGhost){
	      int index = aDistTab.addTuple(globalNodeTuple,nodesPerTuple,myChunk,m->nElems(i)+e);
	      tupleVec.push_back(Hashnode::tupledata(globalNodeTuple));
	      indexVec.push_back(index);
	      elementVec.push_back(i);
	      elementVec.push_back(e);
	    }
	  }
	}
      }
    }
  }
  //ghosts added in previous layers shall share tuples with real nodes on other chunks
  //to form more ghosts for this chunk. However since we never send ghost elements as
  //ghosts to other chunks, they are not added to the tupleVec or indexVec


  int ghostcount=0;
  for(int i=0;i<m->elem.size();i++){
    if(m->elem.has(i)){
      if(layer->elem[i].add){
	FEM_Elem *ghostElems = (FEM_Elem *)m->elem[i].getGhost();
	for(int e=0;e<ghostElems->size();e++){
	  ghostcount++;
	  const int *conn = ghostElems->connFor(e);
	  for(int t=0;t<layer->elem[i].tuplesPerElem;t++){
	    int globalNodeTuple[Hashnode::tupledata::MAX_TUPLE];
	    FEM_Node *ghostNodes = (FEM_Node *)m->node.getGhost();
	    int nodesPerTuple = layer->nodesPerTuple;
	    for(int n=0;n<layer->nodesPerTuple;n++){
	      int nodeindex=(layer->elem[i].elem2tuple)[t*layer->nodesPerTuple+n];
	      if(nodeindex == -1){
		nodesPerTuple--;
		globalNodeTuple[n] = -1;
		continue;
	      }
	      int node=conn[nodeindex];
	      if(FEM_Is_ghost_index(node)){
		globalNodeTuple[n]=ghostNodes->getGlobalno(FEM_From_ghost_index(node));
	      }else{
		globalNodeTuple[n]=m->node.getGlobalno(node);
	      }
	    }
	    //all the tuples of a ghost element are possible generators of ghosts
	    aDistTab.addTuple(globalNodeTuple,nodesPerTuple,myChunk,ghostcount);
	  }
	}
      }
    }
  }
  MsaHashtable::Read rDistTab = aDistTab.syncToRead();

  //debug - print the whole table
  /*	printf("Ghosts chunk %d \n",myChunk);*/
  if(myChunk == masterRank){
    DEBUG(rDistTab.print());
  }

  DEBUG(printf("[%d] id %d says Ghost distributed hashtable printed \n",CkMyPe(),myChunk));
  /* create a new FEM_Mesh msa to transfer the ghost elements from the original mesh to target meshes */
  MSA1DFEMMESH *ghostmeshes;
  if(myChunk == masterRank){
    ghostmeshes = new MSA1DFEMMESH(numChunks,numChunks);	
  }else{
    ghostmeshes = new MSA1DFEMMESH;	
  }
  MPI_Bcast_pup(*ghostmeshes,masterRank,comm);
  ghostmeshes->enroll(numChunks);
  DEBUG(printf("[%d] id %d says ghostmeshes enroll done \n",CkMyPe(),myChunk));

  MSA1DFEMMESH::Accum aGhostMeshes = ghostmeshes->getInitialAccum();

  DEBUG(printf("[%d] id %d says ghostmeshes sync done \n",CkMyPe(),myChunk));
  /*
    For each shared tuple (a tuple in which all nodes are shared) find the other chunks that share this 
    tuple. Add the element corresponding to this tuple, as a ghost to the other chunk's meshes. While adding 
    it the connectivity is changed to use global node numbers. If ghost nodes are to be added the nodes are
    added to the ghost mesh of the chunk. 
  */
  char str[100];
  for(int i=0;i<tupleVec.size();i++){
    const Hashtuple &listTuple = rDistTab.get(indexVec[i]);
    //		printf("[%d] Elements for index %d tuple< %s> number %d \n",myChunk,indexVec[i],tupleVec[i].toString(layer->nodesPerTuple,str),listTuple.vec->size());
    int elType = elementVec[2*i];
    int elNo = elementVec[2*i+1];
    for(int j=0;j<listTuple.vec->size();j++){
			
      // if the entry  ((*listTuple.vec)[j]) in this slot refers to the same tuple
      if((*listTuple.vec)[j].equals(tupleVec[i])){
	int destChunk=(*listTuple.vec)[j].chunk;
	//	printf("[%d] chunk %d element %d \n",myChunk,destChunk,(*listTuple.vec)[j].elementNo);
				
	//never add a ghost element to your own chunk
	if(destChunk != myChunk){
	  //Check if the element to be marked a ghost is already shared as a ghost with that 
	  //chunk. This is necessary in the case of multiple ghost layers to make sure that the
	  //same element doesnt get added as a ghost twice in the same chunk.
					
	  FEM_Comm &sendGhostSide = m->elem[elType].setGhostSend();
	  FEM_Comm_List &sendGhostList = sendGhostSide.addList(destChunk);
	  if(listContains(sendGhostList,elNo)){
	    //if this element already exists as a ghost in the destChunk
	    //dont send it again. However even adding ghost nodes are also
	    //being skipped. The assumption is that if there are multiple
	    //ghost layers, the lower layer would have added such a node.
	    continue;
	  }

	  //add element elNo of type elType to the sendGhost List for chunk (*listTuple.vec)[j].chunk
	  sendGhostList.push_back(elNo);

					
	  //add an element to the ghost mesh for this chunk
          MeshElem::ElemInfo ei(m, elNo, elType);
          aGhostMeshes(destChunk) += ei;
	  int globalelem = m->elem[elType].getGlobalno(elNo);
	  DEBUG(printf("[%d] For chunk %d ghost element global no %d \n",myChunk,destChunk,globalelem));

					
	  //go through the connectivity of the ghost element (for destChunk) and check
	  //if any of the nodes need to be added as ghost nodes on the destination
	  //int *conn = myme.m->elem[elType].connFor(index);
          int* conn = m->elem[elType].connFor(elNo);
	  for(int k=0;k<m->elem[elType].getNodesPer();k++){
	    int lnode = conn[k];
	    //if a node is a ghost node, we dont send it
	    if(lnode < 0){
	      continue;
	    }
	    // if this real node belongs to an element that is a ghost on
	    // another chunk, then this node can be part of a tuple for 
	    // identifying ghosts in the next layer
	    if(sharedNode.get(lnode) ==  0){
	      sharedNode.put(lnode)=3;
	    }
	    int globalnode = m->node.getGlobalno(lnode);
	    conn[k] = globalnode;
	    assert(conn[k] >= 0);
			
	    //if ghost nodes must be added for this layer and it is not a ghost node
	    //itself then add it to the node array
	    // of the ghost mesh for this chunk
	    if(layer->addNodes && lnode >= 0){
						
	      //check if this node is shared with with this chunk, if it is 
	      //then dont send it as a ghost node
	      //dont send a node that has already been sent, it can mess up the
	      //ghost IDXL
	      if(!sharedWith(lnode,(*listTuple.vec)[j].chunk,m)){
		FEM_Comm &sendNodeGhostSide = m->node.setGhostSend();
		FEM_Comm_List &sendNodeGhostList = sendNodeGhostSide.addList((*listTuple.vec)[j].chunk); 
		if(!listContains(sendNodeGhostList,lnode)){
                  MeshElem::NodeInfo ni(m, lnode);
                  aGhostMeshes(destChunk) += ni;
		  DEBUG(printf("[%d] Ghost node (send) global no %d \n",myChunk,globalnode));
		  //add node lnode to the sendGhost list for the destination chunk 
		  sendNodeGhostList.push_back(lnode);
		}	
	      } 
	    }	 
	  }
	}
      }
    }
    //	printf("Elements equivalent ----------- \n");
  }


  DEBUG(printf("[%d] finished creating ghost mesh \n",myChunk));

  MSA1DFEMMESH::Read rGhostMeshes = aGhostMeshes.syncToRead();

  /*
    Go through the ghost nodes and check for nodes that dont exist in the hashtable
    already. Add them to the hashtable as -j, where j is the local ghost number
    In the same routine, if they have not been seen earlier, add them to the 
    ghost nodes.
  */	

  FEM_Mesh *gmesh = rGhostMeshes.get(myChunk).m;
  DEBUG(printf("[%d] my ghost mesh is at %p \n",myChunk,gmesh));
	
  FEM_Node *gnodes = (FEM_Node *)m->node.getGhost();
  gnodes->copyShape(m->node);
  FEM_IndexAttribute *nodeSrcChunkAttr = (FEM_IndexAttribute *)gmesh->node.lookup(FEM_CHUNK,"makeGhosts");
  int *nodeSrcChunkNo = nodeSrcChunkAttr->get().getData();
	
  DEBUG(printf("[%d] gmesh->node.size() = %d \n",myChunk,gmesh->node.size()));
  for(int i=0;i<gmesh->node.size();i++){
    int gnum = gmesh->node.getGlobalno(i);
    int lindex = global2local.get(gnum);
		
    if(lindex == 0){
      int countgnodes = gnodes->push_back(gmesh->node,i);
      global2local.put(gnum) = -(countgnodes+1);
      lindex = -(countgnodes+1);			
    }
    DEBUG(CkPrintf("[%d] Ghost node (recv) global node %d lindex %d \n",myChunk,gnum,lindex));
    //: set RecvGhost for node
    FEM_Comm &recvNodeGhostSide = m->node.setGhostRecv();
    FEM_Comm_List &recvNodeGhostList = recvNodeGhostSide.addList(nodeSrcChunkNo[i]); 
    recvNodeGhostList.push_back(-lindex-1);

  }
	
  /*
    Go through the ghost elements and convert their connectivity from global to local
    node numbers. The connectivity elements can refer to real nodes (shared nodes, 
    +ve in global2local), ghost nodes (-ve) or no node (0). 
    Set the connectivity using the FEM node numbering convention.
    Add the ghost elements to the correct ghost arrays.
  */
  for(int elType = 0;elType < gmesh->elem.size();elType++){
    if(gmesh->elem.has(elType)){
      FEM_IndexAttribute *elemSrcChunkAttr = (FEM_IndexAttribute *)gmesh->elem[elType].lookup(FEM_CHUNK,"makeGhosts");
      int *elemSrcChunkNo = elemSrcChunkAttr->get().getData();

      for(int e=0;e<gmesh->elem[elType].size();e++){
	m->elem[elType].getGhost()->copyShape(gmesh->elem[elType]);
	int lghostelem = m->elem[elType].getGhost()->push_back(gmesh->elem[elType],e);
	int *conn = ((FEM_Elem *)m->elem[elType].getGhost())->connFor(lghostelem);
	for(int n=0;n<gmesh->elem[elType].getNodesPer();n++){
	  int gnode = conn[n];
	  assert(gnode>=0);
	  if(gnode >= 0){
	    int lnode = global2local.get(gnode);
	    //unknown node (neither ghost nor real)
	    if(lnode == 0){
	      if(layer->addNodes){
		CkPrintf("[%d]Unknown global number %d \n",myChunk,gnode);
		CkAbort("Unknown global number for node in ghost element connectivity");
	      }	
	      conn[n] = -1;
	    }
	    //real node
	    if(lnode > 0){
	      conn[n] = lnode -1;
	    }
	    //ghost node
	    if(lnode < 0){
	      conn[n] = FEM_To_ghost_index((-lnode-1));
	    }
	  }else{
	    conn[n] = -1;
	  }
	}
	FEM_Comm &recvGhostSide = m->elem[elType].setGhostRecv();
	FEM_Comm_List &recvGhostList = recvGhostSide.addList(elemSrcChunkNo[e]); 
	recvGhostList.push_back(lghostelem);

      }
    }
  }
  DEBUG(printf("[%d] Send ghost nodes \n",myChunk));
  DEBUG(m->node.getGhostSend().print());
  DEBUG(printf("[%d] Recv ghost nodes \n",myChunk));
  DEBUG(m->node.getGhostRecv().print());

  delete &rDistTab;
  delete &rGhostMeshes;
  delete distTab;
  delete ghostmeshes;
  MPI_Barrier(comm);
}



bool sharedWith(int lnode,int chunk,FEM_Mesh *m){
  double _startTime=CkWallTimer();
  int lindex = m->node.shared.findLocalList(chunk);
  if(lindex != -1){
    const IDXL_List & llist = m->node.shared.getLocalList(lindex);
    for(int i=0;i<llist.size();i++){
      if(llist[i] == lnode){
      	sharedSearchTime += CkWallTimer()-_startTime;
	return true;
      }
    }
    sharedSearchTime += CkWallTimer()-_startTime;
    return false;
  }else{
    sharedSearchTime += CkWallTimer()-_startTime;
    return false;
  }
}
#include "ParFUM.def.h"
/*@}*/

