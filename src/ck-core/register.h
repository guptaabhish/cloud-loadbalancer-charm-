/*****************************************************************************
 * $Source$
 * $Author$
 * $Date$
 * $Revision$
 *****************************************************************************/
/**
\file
\brief Charm++: Lists of possible Chares and Entry points.
*/
#ifndef _REGISTER_H
#define _REGISTER_H
/** \addtogroup CkRegister */
/*@{*/

/**
Represents a single entry method or constructor.
EntryInfo's are always stored in the _entryTable, and can
be referred to across processors by their index into the
_entryTable--this is the entry point's index, often abbreviated
"epIdx".

Although this class is always created via a clean, well-defined
API (the CkRegister* routines in charm.h); access to these classes
is completely direct-- the ck.C routines just access, e.g.,
 _entryTable[epIdx]->chareIdx.
*/
class EntryInfo {
  public:
    /// Human-readable name of entry method, including parameters.
    const char *name;
    
    /**
      A "call function" is how Charm++ actually invokes an 
      entry method on an object.  Call functions take two parameters:
        1.) The message to pass to the method.  This may be 
	    a regular message, a CkMarshallMsg for a 
	    parameter-marshalled method, or a "SysMsg" for a void method.
	    For migration constructors, the "message" might
	    even be NULL.
        2.) The object to invoke the method on.
      Call functions are always generated by the translator.
      
      A simple call function to invoke a method foo::bar(fooMsg *)
      might look like this:
        <pre>
        extern "C" void __call_foo_bar(void *msg,void *obj) {
           fooMsg *m=(fooMsg *)msg;
           foo *f=(foo *)obj;
           f->bar(m);
        }
        </pre>
      Call functions are even used to invoke constructors on new
      Chares.
    */
    CkCallFnPtr call;
    /// Our parameters' index into the _msgTable
    int msgIdx;
    /// Our chare's index into the _chareTable
    int chareIdx;
    /// Charm++ Tracing enabled for this ep (can change dynamically)
    CmiBool traceEnabled; 
    /// Method doesn't keep (and delete) message passed in to it.
    CmiBool noKeep; 
    /// true if this EP is charm internal functions
    CmiBool inCharm;
    
    /** 
      A "marshall unpack" function:
        1.) Pups method parameters out of the buffer passed in to it.
	2.) Calls a method on the object passed in.
	3.) Returns the number of bytes of the buffer consumed.
      It can be used for very efficient delivery of 
      a whole set of combined messages.
    */
    CkMarshallUnpackFn marshallUnpack;
    
    /** 
      A "message pup" function pups the message accepted by 
      this entry point.  This is *only* used to display the 
      message in the debugger, not for normal communication.
      
      This is registered with the entry point  
      (not the message) because many entry points take the same
      message type but store different data in it, like parameter
      marshalled messages.
    */
    CkMessagePupFn messagePup;

    EntryInfo(const char *n, CkCallFnPtr c, int m, int ci) : 
      name(n), call(c), msgIdx(m), chareIdx(ci), 
      marshallUnpack(0), messagePup(0)
    { traceEnabled=CmiTrue; noKeep=CmiFalse; inCharm=CmiFalse;}
};

/**
Represents one type of Message.
It is always stored in _msgTable.
*/
class MsgInfo {
  public:
    /// Human-readable name of message, like "CkMarshallMsg".
    const char *name;
    /**
      A message pack function converts the (possibly complex)
      message into a flat array of bytes.  This method is called
      whenever messages are sent or duplicated, so for speed 
      messages normally have a layout which allows this conversion
      to be done in-place, typically by just moving some pointers
      around in the message.
      
      There was once a time when the pack function could be NULL,
      meaning the message can be sent without packing, but today
      the pack function is always set.  Note: pack and unpack have
      nothing to do with the PUP framework.
     */
    CkPackFnPtr pack;
    /**
     A message unpack function converts a flat array of bytes into
     a living message.  It's just the opposite of pack.
     */
    CkUnpackFnPtr unpack;
    /**
     This message body's allocation size.  This does *not* include
     any dynamically allocated portion of the message, so is a lower
     bound on the message size.
     */
    size_t size;

    MsgInfo(const char *n,CkPackFnPtr p,CkUnpackFnPtr u,int s):
      name(n), pack(p), unpack(u), size(s)
    {}
};

/**
Represents a class of Chares (or array or group elements).  
It is always stored in the _chareTable.
*/
class ChareInfo {
  public:
    /// Human-readable name of the chare class, like "MyFoo".
    const char *name;
    /// Size, in bytes, of the body of the chare.
    int size;
    
    /// Constructor epIdx: default constructor and migration constructor (or -1 if none).
    int defCtor,migCtor; 
    /// Number of base classes:
    int numbases;
    /// chareIdx of each base class.
    int bases[16];
    
    /// For groups -- 1 if the group is Irreducible 
    int isIrr;
    
    /// true if this EP is charm internal functions
    CmiBool inCharm;

    ChareInfo(const char *n, int s) : name(n), size(s) {
      defCtor=migCtor=-1;
      isIrr = numbases = 0;
    }
    void setDefaultCtor(int idx) { defCtor = idx; }
    int getDefaultCtor(void) { return defCtor; }
    void setMigCtor(int idx) { migCtor = idx; }
    int getMigCtor(void) { return migCtor; }
    void addBase(int idx) { bases[numbases++] = idx; }
    void setInCharm() { inCharm = CmiTrue; }
    CmiBool isInCharm() { return inCharm; }
};

/// Describes a mainchare's constructor.  These are all executed at startup.
class MainInfo {
  public:
    const char *name;
    int chareIdx;
    int entryIdx;
    int entryMigCtor;
    void* obj; // real type is Chare*
    MainInfo(int c, int e) : name("main"), chareIdx(c), entryIdx(e) {}
    inline void* getObj(void) { return obj; }
    inline void setObj(void *_obj) { obj=_obj; }
};

/// Describes a readonly global variable.
class ReadonlyInfo {
  public:
    /// Human-readable string name of variable (e.g., "nElements") and type (e.g., "int").
    const char *name,*type;
    /// Size in bytes of basic value.
    int size;
    /// Address of basic value.
    void *ptr;
    /// Pup routine for value, or NULL if no pup available.
    CkPupReadonlyFnPtr pup;
    
    /// Pup this global variable.
    void pupData(PUP::er &p) {
      if (pup!=NULL)
        (pup)((void *)&p);
      else
        p((char *)ptr,size);
    }
    ReadonlyInfo(const char *n,const char *t,
	 int s, void *p,CkPupReadonlyFnPtr pf) 
	: name(n), type(t), size(s), ptr(p), pup(pf) {}
};

/**
 Describes a readonly message.  Readonly messages were
 once the only way to get a truly variable-sized readonly,
 but now that readonlies are pup'd they are almost totally useless.
*/
class ReadonlyMsgInfo {
  public:
    const char *name, *type;
    void **pMsg;
    ReadonlyMsgInfo(const char *n, const char *t,
	void **p) : name(n), type(t), pMsg(p) {}
};

/**
 This class stores registered entities, like EntryInfo's,
 in a linear list indexed by index ("idx").
*/
template <class T>
class CkRegisteredInfo {
	CkVec<T *> vec;
	
	void outOfBounds(int idx) {
		const char *exampleName="";
		if (vec.size()>0) exampleName=vec[0]->name;
		CkPrintf("register.h> CkRegisteredInfo<%d,%s> called with invalid index "
			"%d (should be less than %d)\n", sizeof(T),exampleName,
			idx, vec.size());
		CkAbort("Registered idx is out of bounds-- is message or memory corrupted?");
	}
public:
	/**
	Subtle: we *don't* want to call vec's constructor,
	because the order in which constructors for global
	variables get called is undefined.
	Hence we rely on the implicit zero-initialization 
	that all globals get.
	*/
	CkRegisteredInfo() :vec(CkSkipInitialization()) {}
        ~CkRegisteredInfo() {
        	for (int i=0; i<vec.size(); i++) if (vec[i]) delete vec[i];
  	} 

	/// Add a heap-allocated registration record,
	///  returning the index used.
	int add(T *t) {
#ifndef CMK_OPTIMIZE
		/* Make sure registrations only happen from rank 0: */
		if (CkMyRank()!=0)
			CkAbort("Can only do registrations from rank 0 processors");
#endif
		vec.push_back(t);
		return vec.size()-1;
	}
	
	/// Return the number of registered entities in this table.
	/// (This is a reference so the CpdLists can stay up to date).
	int &size(void) {return vec.length();}
	
	/// Return the registered data at this index.
	T *operator[](int idx) {
#ifndef CMK_OPTIMIZE
		/* Bounds-check the index: */
		if (idx<0 || idx>=vec.size()) outOfBounds(idx);
#endif
		return vec[idx];
	}
};

/// These tables are shared between all processors on a node.
extern CkRegisteredInfo<EntryInfo> _entryTable;
extern CkRegisteredInfo<MsgInfo> _msgTable;
extern CkRegisteredInfo<ChareInfo> _chareTable;
extern CkRegisteredInfo<MainInfo> _mainTable;
extern CkRegisteredInfo<ReadonlyInfo> _readonlyTable;
extern CkRegisteredInfo<ReadonlyMsgInfo> _readonlyMsgs;

extern void _registerInit(void);
extern void _registerDone(void);

/*@}*/
#endif
