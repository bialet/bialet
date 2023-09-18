/*
** 2001 September 15
**
** The author disclaims copyright to this source code.  In place of
** a legal notice, here is a blessing:
**
**    May you do good and not evil.
**    May you find forgiveness for yourself and forgive others.
**    May you share freely, never taking more than you give.
**
*************************************************************************
** The code in this file implements the Virtual Database Engine (VDBE)
**
** The SQL parser generates a program which is then executed by
** the VDBE to do the work of the SQL statement.  VDBE programs are 
** similar in form to assembly language.  The program consists of
** a linear sequence of operations.  Each operation has an opcode 
** and 3 operands.  Operands P1 and P2 are integers.  Operand P3 
** is a null-terminated string.   The P2 operand must be non-negative.
** Opcodes will typically ignore one or more operands.  Many opcodes
** ignore all three operands.
**
** Computation results are stored on a stack.  Each entry on the
** stack is either an integer, a null-terminated string, a floating point
** number, or the SQL "NULL" value.  An inplicit conversion from one
** type to the other occurs as necessary.
** 
** Most of the code in this file is taken up by the sqliteVdbeExec()
** function which does the work of interpreting a VDBE program.
** But other routines are also provided to help in building up
** a program instruction by instruction.
**
** $Id: vdbe.c,v 1.156 2002/06/14 22:38:43 drh Exp $
*/
#include "sqliteInt.h"
#include <ctype.h>

/*
** The following global variable is incremented every time a cursor
** moves, either by the OP_MoveTo or the OP_Next opcode.  The test
** procedures use this information to make sure that indices are
** working correctly.
*/
int sqlite_search_count = 0;

/*
** SQL is translated into a sequence of instructions to be
** executed by a virtual machine.  Each instruction is an instance
** of the following structure.
*/
typedef struct VdbeOp Op;

/*
** Boolean values
*/
typedef unsigned char Bool;

/*
** A cursor is a pointer into a single BTree within a database file.
** The cursor can seek to a BTree entry with a particular key, or
** loop over all entries of the Btree.  You can also insert new BTree
** entries or retrieve the key or data from the entry that the cursor
** is currently pointing to.
** 
** Every cursor that the virtual machine has open is represented by an
** instance of the following structure.
*/
struct Cursor {
  BtCursor *pCursor;    /* The cursor structure of the backend */
  int lastRecno;        /* Last recno from a Next or NextIdx operation */
  Bool recnoIsValid;    /* True if lastRecno is valid */
  Bool keyAsData;       /* The OP_Column command works on key instead of data */
  Bool atFirst;         /* True if pointing to first entry */
  Bool useRandomRowid;  /* Generate new record numbers semi-randomly */
  Bool nullRow;         /* True if pointing to a row with no data */
  Btree *pBt;           /* Separate file holding temporary table */
};
typedef struct Cursor Cursor;

/*
** A sorter builds a list of elements to be sorted.  Each element of
** the list is an instance of the following structure.
*/
typedef struct Sorter Sorter;
struct Sorter {
  int nKey;           /* Number of bytes in the key */
  char *zKey;         /* The key by which we will sort */
  int nData;          /* Number of bytes in the data */
  char *pData;        /* The data associated with this key */
  Sorter *pNext;      /* Next in the list */
};

/* 
** Number of buckets used for merge-sort.  
*/
#define NSORT 30

/*
** Number of bytes of string storage space available to each stack
** layer without having to malloc.  NBFS is short for Number of Bytes
** For Strings.
*/
#define NBFS 32

/*
** A single level of the stack is an instance of the following
** structure.  Except, string values are stored on a separate
** list of of pointers to character.  The reason for storing
** strings separately is so that they can be easily passed
** to the callback function.
*/
struct Stack {
  int i;         /* Integer value */
  int n;         /* Number of characters in string value, including '\0' */
  int flags;     /* Some combination of STK_Null, STK_Str, STK_Dyn, etc. */
  double r;      /* Real value */
  char z[NBFS];  /* Space for short strings */
};
typedef struct Stack Stack;

/*
** Memory cells use the same structure as the stack except that space
** for an arbitrary string is added.
*/
struct Mem {
  Stack s;       /* All values of the memory cell besides string */
  char *z;       /* String value for this memory cell */
};
typedef struct Mem Mem;

/*
** Allowed values for Stack.flags
*/
#define STK_Null      0x0001   /* Value is NULL */
#define STK_Str       0x0002   /* Value is a string */
#define STK_Int       0x0004   /* Value is an integer */
#define STK_Real      0x0008   /* Value is a real number */
#define STK_Dyn       0x0010   /* Need to call sqliteFree() on zStack[*] */
#define STK_Static    0x0020   /* zStack[] points to a static string */

/* The following STK_ value appears only in AggElem.aMem.s.flag fields.
** It indicates that the corresponding AggElem.aMem.z points to a
** aggregate function context that needs to be finalized.
*/
#define STK_AggCtx    0x0040   /* zStack[] points to an agg function context */

/*
** The "context" argument for a installable function.  A pointer to an
** instance of this structure is the first argument to the routines used
** implement the SQL functions.
**
** There is a typedef for this structure in sqlite.h.  So all routines,
** even the public interface to SQLite, can use a pointer to this structure.
** But this file is the only place where the internal details of this
** structure are known.
**
** This structure is defined inside of vdbe.c because it uses substructures
** (Stack) which are only defined there.
*/
struct sqlite_func {
  FuncDef *pFunc;   /* Pointer to function information.  MUST BE FIRST */
  Stack s;          /* Small strings, ints, and double values go here */
  char *z;          /* Space for holding dynamic string results */
  void *pAgg;       /* Aggregate context */
  u8 isError;       /* Set to true for an error */
  u8 isStep;        /* Current in the step function */
  int cnt;          /* Number of times that the step function has been called */
};

/*
** An Agg structure describes an Aggregator.  Each Agg consists of
** zero or more Aggregator elements (AggElem).  Each AggElem contains
** a key and one or more values.  The values are used in processing
** aggregate functions in a SELECT.  The key is used to implement
** the GROUP BY clause of a select.
*/
typedef struct Agg Agg;
typedef struct AggElem AggElem;
struct Agg {
  int nMem;            /* Number of values stored in each AggElem */
  AggElem *pCurrent;   /* The AggElem currently in focus */
  HashElem *pSearch;   /* The hash element for pCurrent */
  Hash hash;           /* Hash table of all aggregate elements */
  FuncDef **apFunc;    /* Information about aggregate functions */
};
struct AggElem {
  char *zKey;          /* The key to this AggElem */
  int nKey;            /* Number of bytes in the key, including '\0' at end */
  Mem aMem[1];         /* The values for this AggElem */
};

/*
** A Set structure is used for quick testing to see if a value
** is part of a small set.  Sets are used to implement code like
** this:
**            x.y IN ('hi','hoo','hum')
*/
typedef struct Set Set;
struct Set {
  Hash hash;             /* A set is just a hash table */
  HashElem *prev;        /* Previously accessed hash elemen */
};

/*
** A Keylist is a bunch of keys into a table.  The keylist can
** grow without bound.  The keylist stores the ROWIDs of database
** records that need to be deleted or updated.
*/
typedef struct Keylist Keylist;
struct Keylist {
  int nKey;         /* Number of slots in aKey[] */
  int nUsed;        /* Next unwritten slot in aKey[] */
  int nRead;        /* Next unread slot in aKey[] */
  Keylist *pNext;   /* Next block of keys */
  int aKey[1];      /* One or more keys.  Extra space allocated as needed */
};

/*
** An instance of the virtual machine
*/
struct Vdbe {
  sqlite *db;         /* The whole database */
  Btree *pBt;         /* Opaque context structure used by DB backend */
  FILE *trace;        /* Write an execution trace here, if not NULL */
  int nOp;            /* Number of instructions in the program */
  int nOpAlloc;       /* Number of slots allocated for aOp[] */
  Op *aOp;            /* Space to hold the virtual machine's program */
  int nLabel;         /* Number of labels used */
  int nLabelAlloc;    /* Number of slots allocated in aLabel[] */
  int *aLabel;        /* Space to hold the labels */
  int tos;            /* Index of top of stack */
  int nStackAlloc;    /* Size of the stack */
  Stack *aStack;      /* The operand stack, except string values */
  char **zStack;      /* Text or binary values of the stack */
  char **azColName;   /* Becomes the 4th parameter to callbacks */
  int nCursor;        /* Number of slots in aCsr[] */
  Cursor *aCsr;       /* One element of this array for each open cursor */
  Keylist *pList;     /* A list of ROWIDs */
  Sorter *pSort;      /* A linked list of objects to be sorted */
  FILE *pFile;        /* At most one open file handler */
  int nField;         /* Number of file fields */
  char **azField;     /* Data for each file field */
  char *zLine;        /* A single line from the input file */
  int nLineAlloc;     /* Number of spaces allocated for zLine */
  int nMem;           /* Number of memory locations currently allocated */
  Mem *aMem;          /* The memory locations */
  Agg agg;            /* Aggregate information */
  int nSet;           /* Number of sets allocated */
  Set *aSet;          /* An array of sets */
  int nCallback;      /* Number of callbacks invoked so far */
  int iLimit;         /* Limit on the number of callbacks remaining */
  int iOffset;        /* Offset before beginning to do callbacks */
  int keylistStackDepth;  /* The size of the "keylist" stack */
  Keylist **keylistStack; /* The stack used by opcodes ListPush & ListPop */
};

/*
** Create a new virtual database engine.
*/
Vdbe *sqliteVdbeCreate(sqlite *db){
  Vdbe *p;
  p = sqliteMalloc( sizeof(Vdbe) );
  if( p==0 ) return 0;
  p->pBt = db->pBe;
  p->db = db;
  return p;
}

/*
** Turn tracing on or off
*/
void sqliteVdbeTrace(Vdbe *p, FILE *trace){
  p->trace = trace;
}

/*
** Add a new instruction to the list of instructions current in the
** VDBE.  Return the address of the new instruction.
**
** Parameters:
**
**    p               Pointer to the VDBE
**
**    op              The opcode for this instruction
**
**    p1, p2          First two of the three possible operands.
**
** Use the sqliteVdbeResolveLabel() function to fix an address and
** the sqliteVdbeChangeP3() function to change the value of the P3
** operand.
*/
int sqliteVdbeAddOp(Vdbe *p, int op, int p1, int p2){
  int i;

  i = p->nOp;
  p->nOp++;
  if( i>=p->nOpAlloc ){
    int oldSize = p->nOpAlloc;
    Op *aNew;
    p->nOpAlloc = p->nOpAlloc*2 + 100;
    aNew = sqliteRealloc(p->aOp, p->nOpAlloc*sizeof(Op));
    if( aNew==0 ){
      p->nOpAlloc = oldSize;
      return 0;
    }
    p->aOp = aNew;
    memset(&p->aOp[oldSize], 0, (p->nOpAlloc-oldSize)*sizeof(Op));
  }
  p->aOp[i].opcode = op;
  p->aOp[i].p1 = p1;
  if( p2<0 && (-1-p2)<p->nLabel && p->aLabel[-1-p2]>=0 ){
    p2 = p->aLabel[-1-p2];
  }
  p->aOp[i].p2 = p2;
  p->aOp[i].p3 = 0;
  p->aOp[i].p3type = P3_NOTUSED;
  return i;
}

/*
** Create a new symbolic label for an instruction that has yet to be
** coded.  The symbolic label is really just a negative number.  The
** label can be used as the P2 value of an operation.  Later, when
** the label is resolved to a specific address, the VDBE will scan
** through its operation list and change all values of P2 which match
** the label into the resolved address.
**
** The VDBE knows that a P2 value is a label because labels are
** always negative and P2 values are suppose to be non-negative.
** Hence, a negative P2 value is a label that has yet to be resolved.
*/
int sqliteVdbeMakeLabel(Vdbe *p){
  int i;
  i = p->nLabel++;
  if( i>=p->nLabelAlloc ){
    int *aNew;
    p->nLabelAlloc = p->nLabelAlloc*2 + 10;
    aNew = sqliteRealloc( p->aLabel, p->nLabelAlloc*sizeof(p->aLabel[0]));
    if( aNew==0 ){
      sqliteFree(p->aLabel);
    }
    p->aLabel = aNew;
  }
  if( p->aLabel==0 ){
    p->nLabel = 0;
    p->nLabelAlloc = 0;
    return 0;
  }
  p->aLabel[i] = -1;
  return -1-i;
}

/*
** Resolve label "x" to be the address of the next instruction to
** be inserted.
*/
void sqliteVdbeResolveLabel(Vdbe *p, int x){
  int j;
  if( x<0 && (-x)<=p->nLabel && p->aOp ){
    if( p->aLabel[-1-x]==p->nOp ) return;
    assert( p->aLabel[-1-x]<0 );
    p->aLabel[-1-x] = p->nOp;
    for(j=0; j<p->nOp; j++){
      if( p->aOp[j].p2==x ) p->aOp[j].p2 = p->nOp;
    }
  }
}

/*
** Return the address of the next instruction to be inserted.
*/
int sqliteVdbeCurrentAddr(Vdbe *p){
  return p->nOp;
}

/*
** Add a whole list of operations to the operation stack.  Return the
** address of the first operation added.
*/
int sqliteVdbeAddOpList(Vdbe *p, int nOp, VdbeOp const *aOp){
  int addr;
  if( p->nOp + nOp >= p->nOpAlloc ){
    int oldSize = p->nOpAlloc;
    Op *aNew;
    p->nOpAlloc = p->nOpAlloc*2 + nOp + 10;
    aNew = sqliteRealloc(p->aOp, p->nOpAlloc*sizeof(Op));
    if( aNew==0 ){
      p->nOpAlloc = oldSize;
      return 0;
    }
    p->aOp = aNew;
    memset(&p->aOp[oldSize], 0, (p->nOpAlloc-oldSize)*sizeof(Op));
  }
  addr = p->nOp;
  if( nOp>0 ){
    int i;
    for(i=0; i<nOp; i++){
      int p2 = aOp[i].p2;
      p->aOp[i+addr] = aOp[i];
      if( p2<0 ) p->aOp[i+addr].p2 = addr + ADDR(p2);
      p->aOp[i+addr].p3type = aOp[i].p3 ? P3_STATIC : P3_NOTUSED;
    }
    p->nOp += nOp;
  }
  return addr;
}

/*
** Change the value of the P1 operand for a specific instruction.
** This routine is useful when a large program is loaded from a
** static array using sqliteVdbeAddOpList but we want to make a
** few minor changes to the program.
*/
void sqliteVdbeChangeP1(Vdbe *p, int addr, int val){
  if( p && addr>=0 && p->nOp>addr && p->aOp ){
    p->aOp[addr].p1 = val;
  }
}

/*
** Change the value of the P2 operand for a specific instruction.
** This routine is useful for setting a jump destination.
*/
void sqliteVdbeChangeP2(Vdbe *p, int addr, int val){
  assert( val>=0 );
  if( p && addr>=0 && p->nOp>addr && p->aOp ){
    p->aOp[addr].p2 = val;
  }
}

/*
** Change the value of the P3 operand for a specific instruction.
** This routine is useful when a large program is loaded from a
** static array using sqliteVdbeAddOpList but we want to make a
** few minor changes to the program.
**
** If n>=0 then the P3 operand is dynamic, meaning that a copy of
** the string is made into memory obtained from sqliteMalloc().
** A value of n==0 means copy bytes of zP3 up to and including the
** first null byte.  If n>0 then copy n+1 bytes of zP3.
**
** If n==P3_STATIC  it means that zP3 is a pointer to a constant static
** string and we can just copy the pointer.  n==P3_POINTER means zP3 is
** a pointer to some object other than a string.
**
** If addr<0 then change P3 on the most recently inserted instruction.
*/
void sqliteVdbeChangeP3(Vdbe *p, int addr, const char *zP3, int n){
  Op *pOp;
  if( p==0 || p->aOp==0 ) return;
  if( addr<0 || addr>=p->nOp ){
    addr = p->nOp - 1;
    if( addr<0 ) return;
  }
  pOp = &p->aOp[addr];
  if( pOp->p3 && pOp->p3type==P3_DYNAMIC ){
    sqliteFree(pOp->p3);
    pOp->p3 = 0;
  }
  if( zP3==0 ){
    pOp->p3 = 0;
    pOp->p3type = P3_NOTUSED;
  }else if( n<0 ){
    pOp->p3 = (char*)zP3;
    pOp->p3type = n;
  }else{
    sqliteSetNString(&pOp->p3, zP3, n, 0);
    pOp->p3type = P3_DYNAMIC;
  }
}

/*
** If the P3 operand to the specified instruction appears
** to be a quoted string token, then this procedure removes 
** the quotes.
**
** The quoting operator can be either a grave ascent (ASCII 0x27)
** or a double quote character (ASCII 0x22).  Two quotes in a row
** resolve to be a single actual quote character within the string.
*/
void sqliteVdbeDequoteP3(Vdbe *p, int addr){
  Op *pOp;
  if( p->aOp==0 || addr<0 || addr>=p->nOp ) return;
  pOp = &p->aOp[addr];
  if( pOp->p3==0 || pOp->p3[0]==0 ) return;
  if( pOp->p3type==P3_POINTER ) return;
  if( pOp->p3type!=P3_DYNAMIC ){
    pOp->p3 = sqliteStrDup(pOp->p3);
    pOp->p3type = P3_DYNAMIC;
  }
  sqliteDequote(pOp->p3);
}

/*
** On the P3 argument of the given instruction, change all
** strings of whitespace characters into a single space and
** delete leading and trailing whitespace.
*/
void sqliteVdbeCompressSpace(Vdbe *p, int addr){
  char *z;
  int i, j;
  Op *pOp;
  if( p->aOp==0 || addr<0 || addr>=p->nOp ) return;
  pOp = &p->aOp[addr];
  if( pOp->p3type==P3_POINTER ){
    return;
  }
  if( pOp->p3type!=P3_DYNAMIC ){
    pOp->p3 = sqliteStrDup(pOp->p3);
    pOp->p3type = P3_DYNAMIC;
  }
  z = pOp->p3;
  if( z==0 ) return;
  i = j = 0;
  while( isspace(z[i]) ){ i++; }
  while( z[i] ){
    if( isspace(z[i]) ){
      z[j++] = ' ';
      while( isspace(z[++i]) ){}
    }else{
      z[j++] = z[i++];
    }
  }
  while( j>0 && isspace(z[j-1]) ){ j--; }
  z[j] = 0;
}

/*
** The following group or routines are employed by installable functions
** to return their results.
**
** The sqlite_set_result_string() routine can be used to return a string
** value or to return a NULL.  To return a NULL, pass in NULL for zResult.
** A copy is made of the string before this routine returns so it is safe
** to pass in a ephemeral string.
**
** sqlite_set_result_error() works like sqlite_set_result_string() except
** that it signals a fatal error.  The string argument, if any, is the
** error message.  If the argument is NULL a generic substitute error message
** is used.
**
** The sqlite_set_result_int() and sqlite_set_result_double() set the return
** value of the user function to an integer or a double.
**
** These routines are defined here in vdbe.c because they depend on knowing
** the internals of the sqlite_func structure which is only defined in that
** one source file.
*/
char *sqlite_set_result_string(sqlite_func *p, const char *zResult, int n){
  assert( !p->isStep );
  if( p->s.flags & STK_Dyn ){
    sqliteFree(p->z);
  }
  if( zResult==0 ){
    p->s.flags = STK_Null;
    n = 0;
    p->z = 0;
    p->s.n = 0;
  }else{
    if( n<0 ) n = strlen(zResult);
    if( n<NBFS-1 ){
      memcpy(p->s.z, zResult, n);
      p->s.z[n] = 0;
      p->s.flags = STK_Str;
      p->z = p->s.z;
    }else{
      p->z = sqliteMalloc( n+1 );
      if( p->z ){
        memcpy(p->z, zResult, n);
        p->z[n] = 0;
      }
      p->s.flags = STK_Str | STK_Dyn;
    }
    p->s.n = n+1;
  }
  return p->z;
}
void sqlite_set_result_int(sqlite_func *p, int iResult){
  assert( !p->isStep );
  if( p->s.flags & STK_Dyn ){
    sqliteFree(p->z);
  }
  p->s.i = iResult;
  p->s.flags = STK_Int;
}
void sqlite_set_result_double(sqlite_func *p, double rResult){
  assert( !p->isStep );
  if( p->s.flags & STK_Dyn ){
    sqliteFree(p->z);
  }
  p->s.r = rResult;
  p->s.flags = STK_Real;
}
void sqlite_set_result_error(sqlite_func *p, const char *zMsg, int n){
  assert( !p->isStep );
  sqlite_set_result_string(p, zMsg, n);
  p->isError = 1;
}

/*
** Extract the user data from a sqlite_func structure and return a
** pointer to it.
**
** This routine is defined here in vdbe.c because it depends on knowing
** the internals of the sqlite_func structure which is only defined in that
** one source file.
*/
void *sqlite_user_data(sqlite_func *p){
  assert( p && p->pFunc );
  return p->pFunc->pUserData;
}

/*
** Allocate or return the aggregate context for a user function.  A new
** context is allocated on the first call.  Subsequent calls return the
** same context that was returned on prior calls.
**
** This routine is defined here in vdbe.c because it depends on knowing
** the internals of the sqlite_func structure which is only defined in that
** one source file.
*/
void *sqlite_aggregate_context(sqlite_func *p, int nByte){
  assert( p && p->pFunc && p->pFunc->xStep );
  if( p->pAgg==0 ){
    if( nByte<=NBFS ){
      p->pAgg = (void*)p->z;
    }else{
      p->pAgg = sqliteMalloc( nByte );
    }
  }
  return p->pAgg;
}

/*
** Return the number of times the Step function of a aggregate has been 
** called.
**
** This routine is defined here in vdbe.c because it depends on knowing
** the internals of the sqlite_func structure which is only defined in that
** one source file.
*/
int sqlite_aggregate_count(sqlite_func *p){
  assert( p && p->pFunc && p->pFunc->xStep );
  return p->cnt;
}

/*
** Reset an Agg structure.  Delete all its contents. 
**
** For installable aggregate functions, if the step function has been
** called, make sure the finalizer function has also been called.  The
** finalizer might need to free memory that was allocated as part of its
** private context.  If the finalizer has not been called yet, call it
** now.
*/
static void AggReset(Agg *pAgg){
  int i;
  HashElem *p;
  for(p = sqliteHashFirst(&pAgg->hash); p; p = sqliteHashNext(p)){
    AggElem *pElem = sqliteHashData(p);
    assert( pAgg->apFunc!=0 );
    for(i=0; i<pAgg->nMem; i++){
      Mem *pMem = &pElem->aMem[i];
      if( pAgg->apFunc[i] && (pMem->s.flags & STK_AggCtx)!=0 ){
        sqlite_func ctx;
        ctx.pFunc = pAgg->apFunc[i];
        ctx.s.flags = STK_Null;
        ctx.z = 0;
        ctx.pAgg = pMem->z;
        ctx.cnt = pMem->s.i;
        ctx.isStep = 0;
        ctx.isError = 0;
        (*pAgg->apFunc[i]->xFinalize)(&ctx);
        if( pMem->z!=0 && pMem->z!=pMem->s.z ){
          sqliteFree(pMem->z);
        }
      }else if( pMem->s.flags & STK_Dyn ){
        sqliteFree(pMem->z);
      }
    }
    sqliteFree(pElem);
  }
  sqliteHashClear(&pAgg->hash);
  sqliteFree(pAgg->apFunc);
  pAgg->apFunc = 0;
  pAgg->pCurrent = 0;
  pAgg->pSearch = 0;
  pAgg->nMem = 0;
}

/*
** Insert a new element and make it the current element.  
**
** Return 0 on success and 1 if memory is exhausted.
*/
static int AggInsert(Agg *p, char *zKey, int nKey){
  AggElem *pElem, *pOld;
  int i;
  pElem = sqliteMalloc( sizeof(AggElem) + nKey +
                        (p->nMem-1)*sizeof(pElem->aMem[0]) );
  if( pElem==0 ) return 1;
  pElem->zKey = (char*)&pElem->aMem[p->nMem];
  memcpy(pElem->zKey, zKey, nKey);
  pElem->nKey = nKey;
  pOld = sqliteHashInsert(&p->hash, pElem->zKey, pElem->nKey, pElem);
  if( pOld!=0 ){
    assert( pOld==pElem );  /* Malloc failed on insert */
    sqliteFree(pOld);
    return 0;
  }
  for(i=0; i<p->nMem; i++){
    pElem->aMem[i].s.flags = STK_Null;
  }
  p->pCurrent = pElem;
  return 0;
}

/*
** Get the AggElem currently in focus
*/
#define AggInFocus(P)   ((P).pCurrent ? (P).pCurrent : _AggInFocus(&(P)))
static AggElem *_AggInFocus(Agg *p){
  HashElem *pElem = sqliteHashFirst(&p->hash);
  if( pElem==0 ){
    AggInsert(p,"",1);
    pElem = sqliteHashFirst(&p->hash);
  }
  return pElem ? sqliteHashData(pElem) : 0;
}

/*
** Convert the given stack entity into a string if it isn't one
** already.  Return non-zero if we run out of memory.
**
** NULLs are converted into an empty string.
*/
#define Stringify(P,I) \
   ((P->aStack[I].flags & STK_Str)==0 ? hardStringify(P,I) : 0)
static int hardStringify(Vdbe *p, int i){
  Stack *pStack = &p->aStack[i];
  char **pzStack = &p->zStack[i];
  int fg = pStack->flags;
  if( fg & STK_Real ){
    sprintf(pStack->z,"%.15g",pStack->r);
  }else if( fg & STK_Int ){
    sprintf(pStack->z,"%d",pStack->i);
  }else{
    pStack->z[0] = 0;
  }
  *pzStack = pStack->z;
  pStack->n = strlen(*pzStack)+1;
  pStack->flags = STK_Str;
  return 0;
}

/*
** Release the memory associated with the given stack level
*/
#define Release(P,I)  if((P)->aStack[I].flags&STK_Dyn){ hardRelease(P,I); }
static void hardRelease(Vdbe *p, int i){
  sqliteFree(p->zStack[i]);
  p->zStack[i] = 0;
  p->aStack[i].flags &= ~(STK_Str|STK_Dyn|STK_Static);
}

/*
** Convert the given stack entity into a integer if it isn't one
** already.
**
** Any prior string or real representation is invalidated.  
** NULLs are converted into 0.
*/
#define Integerify(P,I) \
    if(((P)->aStack[(I)].flags&STK_Int)==0){ hardIntegerify(P,I); }
static void hardIntegerify(Vdbe *p, int i){
  if( p->aStack[i].flags & STK_Real ){
    p->aStack[i].i = (int)p->aStack[i].r;
    Release(p, i);
  }else if( p->aStack[i].flags & STK_Str ){
    p->aStack[i].i = atoi(p->zStack[i]);
    Release(p, i);
  }else{
    p->aStack[i].i = 0;
  }
  p->aStack[i].flags = STK_Int;
}

/*
** Get a valid Real representation for the given stack element.
**
** Any prior string or integer representation is retained.
** NULLs are converted into 0.0.
*/
#define Realify(P,I) \
    if(((P)->aStack[(I)].flags&STK_Real)==0){ hardRealify(P,I); }
static void hardRealify(Vdbe *p, int i){
  if( p->aStack[i].flags & STK_Str ){
    p->aStack[i].r = atof(p->zStack[i]);
  }else if( p->aStack[i].flags & STK_Int ){
    p->aStack[i].r = p->aStack[i].i;
  }else{
    p->aStack[i].r = 0.0;
  }
  p->aStack[i].flags |= STK_Real;
}

/*
** Pop the stack N times.  Free any memory associated with the
** popped stack elements.
*/
static void PopStack(Vdbe *p, int N){
  char **pzStack;
  Stack *pStack;
  if( p->zStack==0 ) return;
  pStack = &p->aStack[p->tos];
  pzStack = &p->zStack[p->tos];
  p->tos -= N;
  while( N-- > 0 ){
    if( pStack->flags & STK_Dyn ){
      sqliteFree(*pzStack);
    }
    pStack->flags = 0;
    *pzStack = 0;
    pStack--;
    pzStack--;
  }
}

/*
** Here is a macro to handle the common case of popping the stack
** once.  This macro only works from within the sqliteVdbeExec()
** function.
*/
#define POPSTACK \
 if( aStack[p->tos].flags & STK_Dyn ) sqliteFree(zStack[p->tos]); \
 p->tos--;

/*
** Make sure space has been allocated to hold at least N
** stack elements.  Allocate additional stack space if
** necessary.
**
** Return 0 on success and non-zero if there are memory
** allocation errors.
*/
#define NeedStack(P,N) (((P)->nStackAlloc<=(N)) ? hardNeedStack(P,N) : 0)
static int hardNeedStack(Vdbe *p, int N){
  int oldAlloc;
  int i;
  if( N>=p->nStackAlloc ){
    Stack *aNew;
    char **zNew;
    oldAlloc = p->nStackAlloc;
    p->nStackAlloc = N + 20;
    aNew = sqliteRealloc(p->aStack, p->nStackAlloc*sizeof(p->aStack[0]));
    zNew = aNew ? sqliteRealloc(p->zStack, p->nStackAlloc*sizeof(char*)) : 0;
    if( zNew==0 ){
      sqliteFree(aNew);
      sqliteFree(p->aStack);
      sqliteFree(p->zStack);
      p->aStack = 0;
      p->zStack = 0;
      p->nStackAlloc = 0;
      p->aStack = 0;
      p->zStack = 0;
      return 1;
    }
    p->aStack = aNew;
    p->zStack = zNew;
    for(i=oldAlloc; i<p->nStackAlloc; i++){
      p->zStack[i] = 0;
      p->aStack[i].flags = 0;
    }
  }
  return 0;
}

/*
** Return TRUE if zNum is a floating-point or integer number.
*/
static int isNumber(const char *zNum){
  if( *zNum=='-' || *zNum=='+' ) zNum++;
  if( !isdigit(*zNum) ) return 0;
  while( isdigit(*zNum) ) zNum++;
  if( *zNum==0 ) return 1;
  if( *zNum!='.' ) return 0;
  zNum++;
  if( !isdigit(*zNum) ) return 0;
  while( isdigit(*zNum) ) zNum++;
  if( *zNum==0 ) return 1;
  if( *zNum!='e' && *zNum!='E' ) return 0;
  zNum++;
  if( *zNum=='-' || *zNum=='+' ) zNum++;
  if( !isdigit(*zNum) ) return 0;
  while( isdigit(*zNum) ) zNum++;
  return *zNum==0;
}

/*
** Return TRUE if zNum is an integer.
*/
static int isInteger(const char *zNum){
  if( *zNum=='-' || *zNum=='+' ) zNum++;
  while( isdigit(*zNum) ) zNum++;
  return *zNum==0;
}

/*
** Delete a keylist
*/
static void KeylistFree(Keylist *p){
  while( p ){
    Keylist *pNext = p->pNext;
    sqliteFree(p);
    p = pNext;
  }
}

/*
** Close a cursor and release all the resources that cursor happens
** to hold.
*/
static void cleanupCursor(Cursor *pCx){
  if( pCx->pCursor ){
    sqliteBtreeCloseCursor(pCx->pCursor);
  }
  if( pCx->pBt ){
    sqliteBtreeClose(pCx->pBt);
  }
  memset(pCx, 0, sizeof(Cursor));
}

/*
** Close all cursors
*/
static void closeAllCursors(Vdbe *p){
  int i;
  for(i=0; i<p->nCursor; i++){
    cleanupCursor(&p->aCsr[i]);
  }
  sqliteFree(p->aCsr);
  p->aCsr = 0;
  p->nCursor = 0;
}

/*
** Remove any elements that remain on the sorter for the VDBE given.
*/
static void SorterReset(Vdbe *p){
  while( p->pSort ){
    Sorter *pSorter = p->pSort;
    p->pSort = pSorter->pNext;
    sqliteFree(pSorter->zKey);
    sqliteFree(pSorter->pData);
    sqliteFree(pSorter);
  }
}

/*
** Clean up the VM after execution.
**
** This routine will automatically close any cursors, lists, and/or
** sorters that were left open.
*/
static void Cleanup(Vdbe *p){
  int i;
  PopStack(p, p->tos+1);
  sqliteFree(p->azColName);
  p->azColName = 0;
  closeAllCursors(p);
  if( p->aMem ){
    for(i=0; i<p->nMem; i++){
      if( p->aMem[i].s.flags & STK_Dyn ){
        sqliteFree(p->aMem[i].z);
      }
    }
  }
  sqliteFree(p->aMem);
  p->aMem = 0;
  p->nMem = 0;
  if( p->pList ){
    KeylistFree(p->pList);
    p->pList = 0;
  }
  SorterReset(p);
  if( p->pFile ){
    if( p->pFile!=stdin ) fclose(p->pFile);
    p->pFile = 0;
  }
  if( p->azField ){
    sqliteFree(p->azField);
    p->azField = 0;
  }
  p->nField = 0;
  if( p->zLine ){
    sqliteFree(p->zLine);
    p->zLine = 0;
  }
  p->nLineAlloc = 0;
  AggReset(&p->agg);
  if( p->aSet ){
    for(i=0; i<p->nSet; i++){
      sqliteHashClear(&p->aSet[i].hash);
    }
  }
  sqliteFree(p->aSet);
  p->aSet = 0;
  p->nSet = 0;
  if( p->keylistStack ){
    int ii;
    for(ii = 0; ii < p->keylistStackDepth; ii++){
      KeylistFree(p->keylistStack[ii]);
    }
    sqliteFree(p->keylistStack);
    p->keylistStackDepth = 0;
    p->keylistStack = 0;
  }
}

/*
** Delete an entire VDBE.
*/
void sqliteVdbeDelete(Vdbe *p){
  int i;
  if( p==0 ) return;
  Cleanup(p);
  if( p->nOpAlloc==0 ){
    p->aOp = 0;
    p->nOp = 0;
  }
  for(i=0; i<p->nOp; i++){
    if( p->aOp[i].p3type==P3_DYNAMIC ){
      sqliteFree(p->aOp[i].p3);
    }
  }
  sqliteFree(p->aOp);
  sqliteFree(p->aLabel);
  sqliteFree(p->aStack);
  sqliteFree(p->zStack);
  sqliteFree(p);
}

/*
** A translation from opcode numbers to opcode names.  Used for testing
** and debugging only.
**
** If any of the numeric OP_ values for opcodes defined in sqliteVdbe.h
** change, be sure to change this array to match.  You can use the
** "opNames.awk" awk script which is part of the source tree to regenerate
** this array, then copy and paste it into this file, if you want.
*/
static char *zOpName[] = { 0,
  "Transaction",       "Checkpoint",        "Commit",            "Rollback",
  "ReadCookie",        "SetCookie",         "VerifyCookie",      "Open",
  "OpenTemp",          "OpenWrite",         "OpenAux",           "OpenWrAux",
  "Close",             "MoveTo",            "NewRecno",          "PutIntKey",
  "PutStrKey",         "Distinct",          "Found",             "NotFound",
  "IsUnique",          "NotExists",         "Delete",            "Column",
  "KeyAsData",         "Recno",             "FullKey",           "NullRow",
  "Last",              "Rewind",            "Next",              "Destroy",
  "Clear",             "CreateIndex",       "CreateTable",       "IntegrityCk",
  "IdxPut",            "IdxDelete",         "IdxRecno",          "IdxGT",
  "IdxGE",             "MemLoad",           "MemStore",          "ListWrite",
  "ListRewind",        "ListRead",          "ListReset",         "ListPush",
  "ListPop",           "SortPut",           "SortMakeRec",       "SortMakeKey",
  "Sort",              "SortNext",          "SortCallback",      "SortReset",
  "FileOpen",          "FileRead",          "FileColumn",        "AggReset",
  "AggFocus",          "AggNext",           "AggSet",            "AggGet",
  "AggFunc",           "AggInit",           "AggPush",           "AggPop",
  "SetInsert",         "SetFound",          "SetNotFound",       "SetFirst",
  "SetNext",           "MakeRecord",        "MakeKey",           "MakeIdxKey",
  "IncrKey",           "Goto",              "If",                "IfNot",
  "Halt",              "ColumnCount",       "ColumnName",        "Callback",
  "NullCallback",      "Integer",           "String",            "Pop",
  "Dup",               "Pull",              "Push",              "MustBeInt",
  "Add",               "AddImm",            "Subtract",          "Multiply",
  "Divide",            "Remainder",         "BitAnd",            "BitOr",
  "BitNot",            "ShiftLeft",         "ShiftRight",        "AbsValue",
  "Eq",                "Ne",                "Lt",                "Le",
  "Gt",                "Ge",                "IsNull",            "NotNull",
  "Negative",          "And",               "Or",                "Not",
  "Concat",            "Noop",              "Function",          "Limit",
  "LimitCk",
};

/*
** Given the name of an opcode, return its number.  Return 0 if
** there is no match.
**
** This routine is used for testing and debugging.
*/
int sqliteVdbeOpcode(const char *zName){
  int i;
  for(i=1; i<=OP_MAX; i++){
    if( sqliteStrICmp(zName, zOpName[i])==0 ) return i;
  }
  return 0;
}

/*
** Give a listing of the program in the virtual machine.
**
** The interface is the same as sqliteVdbeExec().  But instead of
** running the code, it invokes the callback once for each instruction.
** This feature is used to implement "EXPLAIN".
*/
int sqliteVdbeList(
  Vdbe *p,                   /* The VDBE */
  sqlite_callback xCallback, /* The callback */
  void *pArg,                /* 1st argument to callback */
  char **pzErrMsg            /* Error msg written here */
){
  sqlite *db = p->db;
  int i, rc;
  char *azValue[6];
  char zAddr[20];
  char zP1[20];
  char zP2[20];
  char zP3[40];
  static char *azColumnNames[] = {
     "addr", "opcode", "p1", "p2", "p3", 0
  };

  if( xCallback==0 ) return 0;
  azValue[0] = zAddr;
  azValue[2] = zP1;
  azValue[3] = zP2;
  azValue[5] = 0;
  rc = SQLITE_OK;
  for(i=0; rc==SQLITE_OK && i<p->nOp; i++){
    if( db->flags & SQLITE_Interrupt ){
      db->flags &= ~SQLITE_Interrupt;
      if( db->magic!=SQLITE_MAGIC_BUSY ){
        rc = SQLITE_MISUSE;
      }else{
        rc = SQLITE_INTERRUPT;
      }
      sqliteSetString(pzErrMsg, sqlite_error_string(rc), 0);
      break;
    }
    sprintf(zAddr,"%d",i);
    sprintf(zP1,"%d", p->aOp[i].p1);
    sprintf(zP2,"%d", p->aOp[i].p2);
    if( p->aOp[i].p3type==P3_POINTER ){
      sprintf(zP3, "ptr(%#x)", (int)p->aOp[i].p3);
      azValue[4] = zP3;
    }else{
      azValue[4] = p->aOp[i].p3;
    }
    azValue[1] = zOpName[p->aOp[i].opcode];
    if( sqliteSafetyOff(db) ){
      rc = SQLITE_MISUSE;
      break;
    }
    if( xCallback(pArg, 5, azValue, azColumnNames) ){
      rc = SQLITE_ABORT;
    }
    if( sqliteSafetyOn(db) ){
      rc = SQLITE_MISUSE;
    }
  }
  return rc;
}

/*
** The parameters are pointers to the head of two sorted lists
** of Sorter structures.  Merge these two lists together and return
** a single sorted list.  This routine forms the core of the merge-sort
** algorithm.
**
** In the case of a tie, left sorts in front of right.
*/
static Sorter *Merge(Sorter *pLeft, Sorter *pRight){
  Sorter sHead;
  Sorter *pTail;
  pTail = &sHead;
  pTail->pNext = 0;
  while( pLeft && pRight ){
    int c = sqliteSortCompare(pLeft->zKey, pRight->zKey);
    if( c<=0 ){
      pTail->pNext = pLeft;
      pLeft = pLeft->pNext;
    }else{
      pTail->pNext = pRight;
      pRight = pRight->pNext;
    }
    pTail = pTail->pNext;
  }
  if( pLeft ){
    pTail->pNext = pLeft;
  }else if( pRight ){
    pTail->pNext = pRight;
  }
  return sHead.pNext;
}

/*
** Convert an integer in between the native integer format and
** the bigEndian format used as the record number for tables.
**
** The bigEndian format (most significant byte first) is used for
** record numbers so that records will sort into the correct order
** even though memcmp() is used to compare the keys.  On machines
** whose native integer format is little endian (ex: i486) the
** order of bytes is reversed.  On native big-endian machines
** (ex: Alpha, Sparc, Motorola) the byte order is the same.
**
** This function is its own inverse.  In other words
**
**         X == byteSwap(byteSwap(X))
*/
static int byteSwap(int x){
  union {
     char zBuf[sizeof(int)];
     int i;
  } ux;
  ux.zBuf[3] = x&0xff;
  ux.zBuf[2] = (x>>8)&0xff;
  ux.zBuf[1] = (x>>16)&0xff;
  ux.zBuf[0] = (x>>24)&0xff;
  return ux.i;
}

/*
** When converting from the native format to the key format and back
** again, in addition to changing the byte order we invert the high-order
** bit of the most significant byte.  This causes negative numbers to
** sort before positive numbers in the memcmp() function.
*/
#define keyToInt(X)   (byteSwap(X) ^ 0x80000000)
#define intToKey(X)   (byteSwap((X) ^ 0x80000000))

/*
** Code contained within the VERIFY() macro is not needed for correct
** execution.  It is there only to catch errors.  So when we compile
** with NDEBUG=1, the VERIFY() code is omitted.
*/
#ifdef NDEBUG
# define VERIFY(X)
#else
# define VERIFY(X) X
#endif

/*
** Execute the program in the VDBE.
**
** If an error occurs, an error message is written to memory obtained
** from sqliteMalloc() and *pzErrMsg is made to point to that memory.
** The return parameter is the number of errors.
**
** If the callback ever returns non-zero, then the program exits
** immediately.  There will be no error message but the function
** does return SQLITE_ABORT.
**
** A memory allocation error causes this routine to return SQLITE_NOMEM
** and abandon furture processing.
**
** Other fatal errors return SQLITE_ERROR.
**
** If a database file could not be opened because it is locked by
** another database instance, then the xBusy() callback is invoked
** with pBusyArg as its first argument, the name of the table as the
** second argument, and the number of times the open has been attempted
** as the third argument.  The xBusy() callback will typically wait
** for the database file to be openable, then return.  If xBusy()
** returns non-zero, another attempt is made to open the file.  If
** xBusy() returns zero, or if xBusy is NULL, then execution halts
** and this routine returns SQLITE_BUSY.
*/
int sqliteVdbeExec(
  Vdbe *p,                   /* The VDBE */
  sqlite_callback xCallback, /* The callback */
  void *pArg,                /* 1st argument to callback */
  char **pzErrMsg,           /* Error msg written here */
  void *pBusyArg,            /* 1st argument to the busy callback */
  int (*xBusy)(void*,const char*,int)  /* Called when a file is busy */
){
  int pc;                    /* The program counter */
  Op *pOp;                   /* Current operation */
  int rc;                    /* Value to return */
  Btree *pBt = p->pBt;       /* The backend driver */
  sqlite *db = p->db;        /* The database */
  char **zStack;             /* Text stack */
  Stack *aStack;             /* Additional stack information */
  unsigned uniqueCnt = 0;     /* Used by OP_MakeRecord when P2!=0 */
  int errorAction = OE_Abort; /* Recovery action to do in case of an error */
  int undoTransOnError = 0;   /* If error, either ROLLBACK or COMMIT */
  char zBuf[100];             /* Space to sprintf() an integer */


  /* No instruction ever pushes more than a single element onto the
  ** stack.  And the stack never grows on successive executions of the
  ** same loop.  So the total number of instructions is an upper bound
  ** on the maximum stack depth required.
  **
  ** Allocation all the stack space we will ever need.
  */
  NeedStack(p, p->nOp);
  zStack = p->zStack;
  aStack = p->aStack;
  p->tos = -1;
  p->iLimit = 0;
  p->iOffset = 0;

  /* Initialize the aggregrate hash table.
  */
  sqliteHashInit(&p->agg.hash, SQLITE_HASH_BINARY, 0);
  p->agg.pSearch = 0;

  rc = SQLITE_OK;
#ifdef MEMORY_DEBUG
  if( access("vdbe_trace",0)==0 ){
    p->trace = stdout;
  }
#endif
  if( sqlite_malloc_failed ) goto no_mem;
  for(pc=0; !sqlite_malloc_failed && rc==SQLITE_OK && pc<p->nOp
             VERIFY(&& pc>=0); pc++){
    pOp = &p->aOp[pc];

    /* Interrupt processing if requested.
    */
    if( db->flags & SQLITE_Interrupt ){
      db->flags &= ~SQLITE_Interrupt;
      if( db->magic!=SQLITE_MAGIC_BUSY ){
        rc = SQLITE_MISUSE;
      }else{
        rc = SQLITE_INTERRUPT;
      }
      sqliteSetString(pzErrMsg, sqlite_error_string(rc), 0);
      break;
    }

    /* Only allow tracing if NDEBUG is not defined.
    */
#ifndef NDEBUG
    if( p->trace ){
      char *zP3;
      char zPtr[40];
      if( pOp->p3type==P3_POINTER ){
        sprintf(zPtr, "ptr(%#x)", (int)pOp->p3);
        zP3 = zPtr;
      }else{
        zP3 = pOp->p3;
      }
      fprintf(p->trace,"%4d %-12s %4d %4d %s\n",
        pc, zOpName[pOp->opcode], pOp->p1, pOp->p2, zP3 ? zP3 : "");
      fflush(p->trace);
    }
#endif

    switch( pOp->opcode ){

/*****************************************************************************
** What follows is a massive switch statement where each case implements a
** separate instruction in the virtual machine.  If we follow the usual
** indentation conventions, each case should be indented by 6 spaces.  But
** that is a lot of wasted space on the left margin.  So the code within
** the switch statement will break with convention and be flush-left. Another
** big comment (similar to this one) will mark the point in the code where
** we transition back to normal indentation.
*****************************************************************************/

/* Opcode:  Goto * P2 *
**
** An unconditional jump to address P2.
** The next instruction executed will be 
** the one at index P2 from the beginning of
** the program.
*/
case OP_Goto: {
  pc = pOp->p2 - 1;
  break;
}

/* Opcode:  Halt P1 P2 *
**
** Exit immediately.  All open cursors, Lists, Sorts, etc are closed
** automatically.
**
** P1 is the result code returned by sqlite_exec().  For a normal
** halt, this should be SQLITE_OK (0).  For errors, it can be some
** other value.  If P1!=0 then P2 will determine whether or not to
** rollback the current transaction.  Do not rollback if P2==OE_Fail.
** Do the rollback if P2==OE_Rollback.  If P2==OE_Abort, then back
** out all changes that have occurred during this execution of the
** VDBE, but do not rollback the transaction.  (This last case has
** not yet been implemented.  OE_Abort works like OE_Rollback for
** now.  In the future that may change.)
**
** There is an implied "Halt 0 0 0" instruction inserted at the very end of
** every program.  So a jump past the last instruction of the program
** is the same as executing Halt.
*/
case OP_Halt: {
  if( pOp->p1!=SQLITE_OK ){
    rc = pOp->p1;
    errorAction = pOp->p2;
    if( pOp->p3 ){
	sqliteSetString(pzErrMsg, pOp->p3, 0);
	goto cleanup;
    }
    goto abort_due_to_error;
  }else{
    pc = p->nOp-1;
  }
  break;
}

/* Opcode: Integer P1 * P3
**
** The integer value P1 is pushed onto the stack.  If P3 is not zero
** then it is assumed to be a string representation of the same integer.
*/
case OP_Integer: {
  int i = ++p->tos;
  VERIFY( if( NeedStack(p, p->tos) ) goto no_mem; )
  aStack[i].i = pOp->p1;
  aStack[i].flags = STK_Int;
  if( pOp->p3 ){
    zStack[i] = pOp->p3;
    aStack[i].flags |= STK_Str | STK_Static;
    aStack[i].n = strlen(pOp->p3)+1;
  }
  break;
}

/* Opcode: String * * P3
**
** The string value P3 is pushed onto the stack.  If P3==0 then a
** NULL is pushed onto the stack.
*/
case OP_String: {
  int i = ++p->tos;
  char *z;
  VERIFY( if( NeedStack(p, p->tos) ) goto no_mem; )
  z = pOp->p3;
  if( z==0 ){
    zStack[i] = 0;
    aStack[i].n = 0;
    aStack[i].flags = STK_Null;
  }else{
    zStack[i] = z;
    aStack[i].n = strlen(z) + 1;
    aStack[i].flags = STK_Str | STK_Static;
  }
  break;
}

/* Opcode: Pop P1 * *
**
** P1 elements are popped off of the top of stack and discarded.
*/
case OP_Pop: {
  assert( p->tos+1>=pOp->p1 );
  PopStack(p, pOp->p1);
  break;
}

/* Opcode: Dup P1 P2 *
**
** A copy of the P1-th element of the stack 
** is made and pushed onto the top of the stack.
** The top of the stack is element 0.  So the
** instruction "Dup 0 0 0" will make a copy of the
** top of the stack.
**
** If the content of the P1-th element is a dynamically
** allocated string, then a new copy of that string
** is made if P2==0.  If P2!=0, then just a pointer
** to the string is copied.
**
** Also see the Pull instruction.
*/
case OP_Dup: {
  int i = p->tos - pOp->p1;
  int j = ++p->tos;
  VERIFY( if( i<0 ) goto not_enough_stack; )
  VERIFY( if( NeedStack(p, p->tos) ) goto no_mem; )
  memcpy(&aStack[j], &aStack[i], sizeof(aStack[i])-NBFS);
  if( aStack[j].flags & STK_Str ){
    if( pOp->p2 || (aStack[j].flags & STK_Static)!=0 ){
      zStack[j] = zStack[i];
      aStack[j].flags &= ~STK_Dyn;
    }else if( aStack[i].n<=NBFS ){
      memcpy(aStack[j].z, zStack[i], aStack[j].n);
      zStack[j] = aStack[j].z;
      aStack[j].flags &= ~(STK_Static|STK_Dyn);
    }else{
      zStack[j] = sqliteMalloc( aStack[j].n );
      if( zStack[j]==0 ) goto no_mem;
      memcpy(zStack[j], zStack[i], aStack[j].n);
      aStack[j].flags &= ~STK_Static;
    }
  }
  break;
}

/* Opcode: Pull P1 * *
**
** The P1-th element is removed from its current location on 
** the stack and pushed back on top of the stack.  The
** top of the stack is element 0, so "Pull 0 0 0" is
** a no-op.  "Pull 1 0 0" swaps the top two elements of
** the stack.
**
** See also the Dup instruction.
*/
case OP_Pull: {
  int from = p->tos - pOp->p1;
  int to = p->tos;
  int i;
  Stack ts;
  char *tz;
  VERIFY( if( from<0 ) goto not_enough_stack; )
  ts = aStack[from];
  tz = zStack[from];
  for(i=from; i<to; i++){
    aStack[i] = aStack[i+1];
    if( aStack[i].flags & (STK_Dyn|STK_Static) ){
      zStack[i] = zStack[i+1];
    }else{
      zStack[i] = aStack[i].z;
    }
  }
  aStack[to] = ts;
  if( aStack[to].flags & (STK_Dyn|STK_Static) ){
    zStack[to] = tz;
  }else{
    zStack[to] = aStack[to].z;
  }
  break;
}

/* Opcode: Push P1 * *
**
** Overwrite the value of the P1-th element down on the
** stack (P1==0 is the top of the stack) with the value
** of the top of the stack.  The pop the top of the stack.
*/
case OP_Push: {
  int from = p->tos;
  int to = p->tos - pOp->p1;

  VERIFY( if( to<0 ) goto not_enough_stack; )
  if( aStack[to].flags & STK_Dyn ){
    sqliteFree(zStack[to]);
  }
  aStack[to] = aStack[from];
  if( aStack[to].flags & (STK_Dyn|STK_Static) ){
    zStack[to] = zStack[from];
  }else{
    zStack[to] = aStack[to].z;
  }
  aStack[from].flags &= ~STK_Dyn;
  p->tos--;
  break;
}

/* Opcode: ColumnCount P1 * *
**
** Specify the number of column values that will appear in the
** array passed as the 4th parameter to the callback.  No checking
** is done.  If this value is wrong, a coredump can result.
*/
case OP_ColumnCount: {
  char **az = sqliteRealloc(p->azColName, (pOp->p1+1)*sizeof(char*));
  if( az==0 ){ goto no_mem; }
  p->azColName = az;
  p->azColName[pOp->p1] = 0;
  p->nCallback = 0;
  break;
}

/* Opcode: ColumnName P1 * P3
**
** P3 becomes the P1-th column name (first is 0).  An array of pointers
** to all column names is passed as the 4th parameter to the callback.
** The ColumnCount opcode must be executed first to allocate space to
** hold the column names.  Failure to do this will likely result in
** a coredump.
*/
case OP_ColumnName: {
  p->azColName[pOp->p1] = pOp->p3 ? pOp->p3 : "";
  p->nCallback = 0;
  break;
}

/* Opcode: Callback P1 * *
**
** Pop P1 values off the stack and form them into an array.  Then
** invoke the callback function using the newly formed array as the
** 3rd parameter.
*/
case OP_Callback: {
  int i = p->tos - pOp->p1 + 1;
  int j;
  VERIFY( if( i<0 ) goto not_enough_stack; )
  VERIFY( if( NeedStack(p, p->tos+2) ) goto no_mem; )
  for(j=i; j<=p->tos; j++){
    if( aStack[j].flags & STK_Null ){
      zStack[j] = 0;
    }else{
      if( Stringify(p, j) ) goto no_mem;
    }
  }
  zStack[p->tos+1] = 0;
  if( xCallback!=0 ){
    if( sqliteSafetyOff(db) ) goto abort_due_to_misuse; 
    if( xCallback(pArg, pOp->p1, &zStack[i], p->azColName)!=0 ){
      rc = SQLITE_ABORT;
    }
    if( sqliteSafetyOn(db) ) goto abort_due_to_misuse;
    p->nCallback++;
  }
  PopStack(p, pOp->p1);
  if( sqlite_malloc_failed ) goto no_mem;
  break;
}

/* Opcode: NullCallback P1 * *
**
** Invoke the callback function once with the 2nd argument (the
** number of columns) equal to P1 and with the 4th argument (the
** names of the columns) set according to prior OP_ColumnName and
** OP_ColumnCount instructions.  This is all like the regular
** OP_Callback or OP_SortCallback opcodes.  But the 3rd argument
** which normally contains a pointer to an array of pointers to
** data is NULL.
**
** The callback is only invoked if there have been no prior calls
** to OP_Callback or OP_SortCallback.
**
** This opcode is used to report the number and names of columns
** in cases where the result set is empty.
*/
case OP_NullCallback: {
  if( xCallback!=0 && p->nCallback==0 ){
    if( sqliteSafetyOff(db) ) goto abort_due_to_misuse; 
    if( xCallback(pArg, pOp->p1, 0, p->azColName)!=0 ){
      rc = SQLITE_ABORT;
    }
    if( sqliteSafetyOn(db) ) goto abort_due_to_misuse;
    p->nCallback++;
  }
  if( sqlite_malloc_failed ) goto no_mem;
  break;
}

/* Opcode: Concat P1 P2 P3
**
** Look at the first P1 elements of the stack.  Append them all 
** together with the lowest element first.  Use P3 as a separator.  
** Put the result on the top of the stack.  The original P1 elements
** are popped from the stack if P2==0 and retained if P2==1.
**
** If P3 is NULL, then use no separator.  When P1==1, this routine
** makes a copy of the top stack element into memory obtained
** from sqliteMalloc().
*/
case OP_Concat: {
  char *zNew;
  int nByte;
  int nField;
  int i, j;
  char *zSep;
  int nSep;

  nField = pOp->p1;
  zSep = pOp->p3;
  if( zSep==0 ) zSep = "";
  nSep = strlen(zSep);
  VERIFY( if( p->tos+1<nField ) goto not_enough_stack; )
  nByte = 1 - nSep;
  for(i=p->tos-nField+1; i<=p->tos; i++){
    if( aStack[i].flags & STK_Null ){
      nByte += nSep;
    }else{
      if( Stringify(p, i) ) goto no_mem;
      nByte += aStack[i].n - 1 + nSep;
    }
  }
  zNew = sqliteMalloc( nByte );
  if( zNew==0 ) goto no_mem;
  j = 0;
  for(i=p->tos-nField+1; i<=p->tos; i++){
    if( (aStack[i].flags & STK_Null)==0 ){
      memcpy(&zNew[j], zStack[i], aStack[i].n-1);
      j += aStack[i].n-1;
    }
    if( nSep>0 && i<p->tos ){
      memcpy(&zNew[j], zSep, nSep);
      j += nSep;
    }
  }
  zNew[j] = 0;
  if( pOp->p2==0 ) PopStack(p, nField);
  VERIFY( NeedStack(p, p->tos+1); )
  p->tos++;
  aStack[p->tos].n = nByte;
  aStack[p->tos].flags = STK_Str|STK_Dyn;
  zStack[p->tos] = zNew;
  break;
}

/* Opcode: Add * * *
**
** Pop the top two elements from the stack, add them together,
** and push the result back onto the stack.  If either element
** is a string then it is converted to a double using the atof()
** function before the addition.
** If either operand is NULL, the result is NULL.
*/
/* Opcode: Multiply * * *
**
** Pop the top two elements from the stack, multiply them together,
** and push the result back onto the stack.  If either element
** is a string then it is converted to a double using the atof()
** function before the multiplication.
** If either operand is NULL, the result is NULL.
*/
/* Opcode: Subtract * * *
**
** Pop the top two elements from the stack, subtract the
** first (what was on top of the stack) from the second (the
** next on stack)
** and push the result back onto the stack.  If either element
** is a string then it is converted to a double using the atof()
** function before the subtraction.
** If either operand is NULL, the result is NULL.
*/
/* Opcode: Divide * * *
**
** Pop the top two elements from the stack, divide the
** first (what was on top of the stack) from the second (the
** next on stack)
** and push the result back onto the stack.  If either element
** is a string then it is converted to a double using the atof()
** function before the division.  Division by zero returns NULL.
** If either operand is NULL, the result is NULL.
*/
/* Opcode: Remainder * * *
**
** Pop the top two elements from the stack, divide the
** first (what was on top of the stack) from the second (the
** next on stack)
** and push the remainder after division onto the stack.  If either element
** is a string then it is converted to a double using the atof()
** function before the division.  Division by zero returns NULL.
** If either operand is NULL, the result is NULL.
*/
case OP_Add:
case OP_Subtract:
case OP_Multiply:
case OP_Divide:
case OP_Remainder: {
  int tos = p->tos;
  int nos = tos - 1;
  VERIFY( if( nos<0 ) goto not_enough_stack; )
  if( ((aStack[tos].flags | aStack[nos].flags) & STK_Null)!=0 ){
    POPSTACK;
    Release(p, nos);
    aStack[nos].flags = STK_Null;
  }else if( (aStack[tos].flags & aStack[nos].flags & STK_Int)==STK_Int ){
    int a, b;
    a = aStack[tos].i;
    b = aStack[nos].i;
    switch( pOp->opcode ){
      case OP_Add:         b += a;       break;
      case OP_Subtract:    b -= a;       break;
      case OP_Multiply:    b *= a;       break;
      case OP_Divide: {
        if( a==0 ) goto divide_by_zero;
        b /= a;
        break;
      }
      default: {
        if( a==0 ) goto divide_by_zero;
        b %= a;
        break;
      }
    }
    POPSTACK;
    Release(p, nos);
    aStack[nos].i = b;
    aStack[nos].flags = STK_Int;
  }else{
    double a, b;
    Realify(p, tos);
    Realify(p, nos);
    a = aStack[tos].r;
    b = aStack[nos].r;
    switch( pOp->opcode ){
      case OP_Add:         b += a;       break;
      case OP_Subtract:    b -= a;       break;
      case OP_Multiply:    b *= a;       break;
      case OP_Divide: {
        if( a==0.0 ) goto divide_by_zero;
        b /= a;
        break;
      }
      default: {
        int ia = (int)a;
        int ib = (int)b;
        if( ia==0.0 ) goto divide_by_zero;
        b = ib % ia;
        break;
      }
    }
    POPSTACK;
    Release(p, nos);
    aStack[nos].r = b;
    aStack[nos].flags = STK_Real;
  }
  break;

divide_by_zero:
  PopStack(p, 2);
  p->tos = nos;
  aStack[nos].flags = STK_Null;
  break;
}

/* Opcode: Function P1 * P3
**
** Invoke a user function (P3 is a pointer to a Function structure that
** defines the function) with P1 string arguments taken from the stack.
** Pop all arguments from the stack and push back the result.
**
** See also: AggFunc
*/
case OP_Function: {
  int n, i;
  sqlite_func ctx;

  n = pOp->p1;
  VERIFY( if( n<0 ) goto bad_instruction; )
  VERIFY( if( p->tos+1<n ) goto not_enough_stack; )
  for(i=p->tos-n+1; i<=p->tos; i++){
    if( aStack[i].flags & STK_Null ){
      zStack[i] = 0;
    }else{
      if( Stringify(p, i) ) goto no_mem;
    }
  }
  ctx.pFunc = (FuncDef*)pOp->p3;
  ctx.s.flags = STK_Null;
  ctx.z = 0;
  ctx.isError = 0;
  ctx.isStep = 0;
  (*ctx.pFunc->xFunc)(&ctx, n, (const char**)&zStack[p->tos-n+1]);
  PopStack(p, n);
  VERIFY( NeedStack(p, p->tos+1); )
  p->tos++;
  aStack[p->tos] = ctx.s;
  if( ctx.s.flags & STK_Dyn ){
    zStack[p->tos] = ctx.z;
  }else if( ctx.s.flags & STK_Str ){
    zStack[p->tos] = aStack[p->tos].z;
  }else{
    zStack[p->tos] = 0;
  }
  if( ctx.isError ){
    sqliteSetString(pzErrMsg, 
       zStack[p->tos] ? zStack[p->tos] : "user function error", 0);
    rc = SQLITE_ERROR;
  }
  break;
}

/* Opcode: BitAnd * * *
**
** Pop the top two elements from the stack.  Convert both elements
** to integers.  Push back onto the stack the bit-wise AND of the
** two elements.
** If either operand is NULL, the result is NULL.
*/
/* Opcode: BitOr * * *
**
** Pop the top two elements from the stack.  Convert both elements
** to integers.  Push back onto the stack the bit-wise OR of the
** two elements.
** If either operand is NULL, the result is NULL.
*/
/* Opcode: ShiftLeft * * *
**
** Pop the top two elements from the stack.  Convert both elements
** to integers.  Push back onto the stack the top element shifted
** left by N bits where N is the second element on the stack.
** If either operand is NULL, the result is NULL.
*/
/* Opcode: ShiftRight * * *
**
** Pop the top two elements from the stack.  Convert both elements
** to integers.  Push back onto the stack the top element shifted
** right by N bits where N is the second element on the stack.
** If either operand is NULL, the result is NULL.
*/
case OP_BitAnd:
case OP_BitOr:
case OP_ShiftLeft:
case OP_ShiftRight: {
  int tos = p->tos;
  int nos = tos - 1;
  int a, b;
  VERIFY( if( nos<0 ) goto not_enough_stack; )
  if( (aStack[tos].flags | aStack[nos].flags) & STK_Null ){
    POPSTACK;
    Release(p,nos);
    aStack[nos].flags = STK_Null;
    break;
  }
  Integerify(p, tos);
  Integerify(p, nos);
  a = aStack[tos].i;
  b = aStack[nos].i;
  switch( pOp->opcode ){
    case OP_BitAnd:      a &= b;     break;
    case OP_BitOr:       a |= b;     break;
    case OP_ShiftLeft:   a <<= b;    break;
    case OP_ShiftRight:  a >>= b;    break;
    default:   /* CANT HAPPEN */     break;
  }
  POPSTACK;
  Release(p, nos);
  aStack[nos].i = a;
  aStack[nos].flags = STK_Int;
  break;
}

/* Opcode: AddImm  P1 * *
** 
** Add the value P1 to whatever is on top of the stack.  The result
** is always an integer.
**
** To force the top of the stack to be an integer, just add 0.
*/
case OP_AddImm: {
  int tos = p->tos;
  VERIFY( if( tos<0 ) goto not_enough_stack; )
  Integerify(p, tos);
  aStack[tos].i += pOp->p1;
  break;
}

/* Opcode: MustBeInt  * P2 *
** 
** Force the top of the stack to be an integer.  If the top of the
** stack is not an integer and cannot be converted into an integer
** with out data loss, then jump immediately to P2, or if P2==0
** raise an SQLITE_MISMATCH exception.
*/
case OP_MustBeInt: {
  int tos = p->tos;
  VERIFY( if( tos<0 ) goto not_enough_stack; )
  if( aStack[tos].flags & STK_Int ){
    /* Do nothing */
  }else if( aStack[tos].flags & STK_Real ){
    int i = aStack[tos].r;
    double r = i;
    if( r!=aStack[tos].r ){
      goto mismatch;
    }
    aStack[tos].i = i;
  }else if( aStack[tos].flags & STK_Str ){
    if( !isInteger(zStack[tos]) ){
      goto mismatch;
    }
    p->aStack[tos].i = atoi(p->zStack[tos]);
  }else{
    goto mismatch;
  }
  Release(p, tos);
  p->aStack[tos].flags = STK_Int;
  break;

mismatch:
  if( pOp->p2==0 ){
    rc = SQLITE_MISMATCH;
    goto abort_due_to_error;
  }else{
    pc = pOp->p2 - 1;
  }
  break;
}

/* Opcode: Eq P1 P2 *
**
** Pop the top two elements from the stack.  If they are equal, then
** jump to instruction P2.  Otherwise, continue to the next instruction.
**
** If either operand is NULL (and thus if the result is unknown) then
** take the jump if P1 is true.
**
** If P2 is zero, do not jump.  Instead, push an integer 1 onto the
** stack if the jump would have been taken, or a 0 if not.  Push a
** NULL if either operand was NULL.
*/
/* Opcode: Ne P1 P2 *
**
** Pop the top two elements from the stack.  If they are not equal, then
** jump to instruction P2.  Otherwise, continue to the next instruction.
**
** If either operand is NULL (and thus if the result is unknown) then
** take the jump if P1 is true.
**
** If P2 is zero, do not jump.  Instead, push an integer 1 onto the
** stack if the jump would have been taken, or a 0 if not.  Push a
** NULL if either operand was NULL.
*/
/* Opcode: Lt P1 P2 *
**
** Pop the top two elements from the stack.  If second element (the
** next on stack) is less than the first (the top of stack), then
** jump to instruction P2.  Otherwise, continue to the next instruction.
** In other words, jump if NOS<TOS.
**
** If either operand is NULL (and thus if the result is unknown) then
** take the jump if P1 is true.
**
** If P2 is zero, do not jump.  Instead, push an integer 1 onto the
** stack if the jump would have been taken, or a 0 if not.  Push a
** NULL if either operand was NULL.
*/
/* Opcode: Le P1 P2 *
**
** Pop the top two elements from the stack.  If second element (the
** next on stack) is less than or equal to the first (the top of stack),
** then jump to instruction P2. In other words, jump if NOS<=TOS.
**
** If either operand is NULL (and thus if the result is unknown) then
** take the jump if P1 is true.
**
** If P2 is zero, do not jump.  Instead, push an integer 1 onto the
** stack if the jump would have been taken, or a 0 if not.  Push a
** NULL if either operand was NULL.
*/
/* Opcode: Gt P1 P2 *
**
** Pop the top two elements from the stack.  If second element (the
** next on stack) is greater than the first (the top of stack),
** then jump to instruction P2. In other words, jump if NOS>TOS.
**
** If either operand is NULL (and thus if the result is unknown) then
** take the jump if P1 is true.
**
** If P2 is zero, do not jump.  Instead, push an integer 1 onto the
** stack if the jump would have been taken, or a 0 if not.  Push a
** NULL if either operand was NULL.
*/
/* Opcode: Ge P1 P2 *
**
** Pop the top two elements from the stack.  If second element (the next
** on stack) is greater than or equal to the first (the top of stack),
** then jump to instruction P2. In other words, jump if NOS>=TOS.
**
** If either operand is NULL (and thus if the result is unknown) then
** take the jump if P1 is true.
**
** If P2 is zero, do not jump.  Instead, push an integer 1 onto the
** stack if the jump would have been taken, or a 0 if not.  Push a
** NULL if either operand was NULL.
*/
case OP_Eq:
case OP_Ne:
case OP_Lt:
case OP_Le:
case OP_Gt:
case OP_Ge: {
  int tos = p->tos;
  int nos = tos - 1;
  int c;
  int ft, fn;
  VERIFY( if( nos<0 ) goto not_enough_stack; )
  ft = aStack[tos].flags;
  fn = aStack[nos].flags;
  if( (ft | fn) & STK_Null ){
    POPSTACK;
    POPSTACK;
    if( pOp->p2 ){
      if( pOp->p1 ) pc = pOp->p2-1;
    }else{
      p->tos++;
      aStack[nos].flags = STK_Null;
    }
    break;
  }else if( (ft & fn & STK_Int)==STK_Int ){
    c = aStack[nos].i - aStack[tos].i;
  }else if( (ft & STK_Int)!=0 && (fn & STK_Str)!=0 && isInteger(zStack[nos]) ){
    Integerify(p, nos);
    c = aStack[nos].i - aStack[tos].i;
  }else if( (fn & STK_Int)!=0 && (ft & STK_Str)!=0 && isInteger(zStack[tos]) ){
    Integerify(p, tos);
    c = aStack[nos].i - aStack[tos].i;
  }else{
    if( Stringify(p, tos) || Stringify(p, nos) ) goto no_mem;
    c = sqliteCompare(zStack[nos], zStack[tos]);
  }
  switch( pOp->opcode ){
    case OP_Eq:    c = c==0;     break;
    case OP_Ne:    c = c!=0;     break;
    case OP_Lt:    c = c<0;      break;
    case OP_Le:    c = c<=0;     break;
    case OP_Gt:    c = c>0;      break;
    default:       c = c>=0;     break;
  }
  POPSTACK;
  POPSTACK;
  if( pOp->p2 ){
    if( c ) pc = pOp->p2-1;
  }else{
    p->tos++;
    aStack[nos].flags = STK_Int;
    aStack[nos].i = c;
  }
  break;
}

/* Opcode: And * * *
**
** Pop two values off the stack.  Take the logical AND of the
** two values and push the resulting boolean value back onto the
** stack. 
*/
/* Opcode: Or * * *
**
** Pop two values off the stack.  Take the logical OR of the
** two values and push the resulting boolean value back onto the
** stack. 
*/
case OP_And:
case OP_Or: {
  int tos = p->tos;
  int nos = tos - 1;
  int v1, v2;    /* 0==TRUE, 1==FALSE, 2==UNKNOWN or NULL */

  VERIFY( if( nos<0 ) goto not_enough_stack; )
  if( aStack[tos].flags & STK_Null ){
    v1 = 2;
  }else{
    Integerify(p, tos);
    v1 = aStack[tos].i==0;
  }
  if( aStack[nos].flags & STK_Null ){
    v2 = 2;
  }else{
    Integerify(p, nos);
    v2 = aStack[nos].i==0;
  }
  if( pOp->opcode==OP_And ){
    static const unsigned char and_logic[] = { 0, 1, 2, 1, 1, 1, 2, 1, 2 };
    v1 = and_logic[v1*3+v2];
  }else{
    static const unsigned char or_logic[] = { 0, 0, 0, 0, 1, 2, 0, 2, 2 };
    v1 = or_logic[v1*3+v2];
  }
  POPSTACK;
  Release(p, nos);
  if( v1==2 ){
    aStack[nos].flags = STK_Null;
  }else{
    aStack[nos].i = v1==0;
    aStack[nos].flags = STK_Int;
  }
  break;
}

/* Opcode: Negative * * *
**
** Treat the top of the stack as a numeric quantity.  Replace it
** with its additive inverse.  If the top of the stack is NULL
** its value is unchanged.
*/
/* Opcode: AbsValue * * *
**
** Treat the top of the stack as a numeric quantity.  Replace it
** with its absolute value. If the top of the stack is NULL
** its value is unchanged.
*/
case OP_Negative:
case OP_AbsValue: {
  int tos = p->tos;
  VERIFY( if( tos<0 ) goto not_enough_stack; )
  if( aStack[tos].flags & STK_Real ){
    Release(p, tos);
    if( pOp->opcode==OP_Negative || aStack[tos].r<0.0 ){
      aStack[tos].r = -aStack[tos].r;
    }
    aStack[tos].flags = STK_Real;
  }else if( aStack[tos].flags & STK_Int ){
    Release(p, tos);
    if( pOp->opcode==OP_Negative ||  aStack[tos].i<0 ){
      aStack[tos].i = -aStack[tos].i;
    }
    aStack[tos].flags = STK_Int;
  }else if( aStack[tos].flags & STK_Null ){
    /* Do nothing */
  }else{
    Realify(p, tos);
    Release(p, tos);
    if( pOp->opcode==OP_Negative ||  aStack[tos].r<0.0 ){
      aStack[tos].r = -aStack[tos].r;
    }
    aStack[tos].flags = STK_Real;
  }
  break;
}

/* Opcode: Not * * *
**
** Interpret the top of the stack as a boolean value.  Replace it
** with its complement.  If the top of the stack is NULL its value
** is unchanged.
*/
case OP_Not: {
  int tos = p->tos;
  VERIFY( if( p->tos<0 ) goto not_enough_stack; )
  if( aStack[tos].flags & STK_Null ) break;  /* Do nothing to NULLs */
  Integerify(p, tos);
  Release(p, tos);
  aStack[tos].i = !aStack[tos].i;
  aStack[tos].flags = STK_Int;
  break;
}

/* Opcode: BitNot * * *
**
** Interpret the top of the stack as an value.  Replace it
** with its ones-complement.  If the top of the stack is NULL its
** value is unchanged.
*/
case OP_BitNot: {
  int tos = p->tos;
  VERIFY( if( p->tos<0 ) goto not_enough_stack; )
  if( aStack[tos].flags & STK_Null ) break;  /* Do nothing to NULLs */
  Integerify(p, tos);
  Release(p, tos);
  aStack[tos].i = ~aStack[tos].i;
  aStack[tos].flags = STK_Int;
  break;
}

/* Opcode: Noop * * *
**
** Do nothing.  This instruction is often useful as a jump
** destination.
*/
case OP_Noop: {
  break;
}

/* Opcode: If P1 P2 *
**
** Pop a single boolean from the stack.  If the boolean popped is
** true, then jump to p2.  Otherwise continue to the next instruction.
** An integer is false if zero and true otherwise.  A string is
** false if it has zero length and true otherwise.
**
** If the value popped of the stack is NULL, then take the jump if P1
** is true and fall through if P1 is false.
*/
/* Opcode: IfNot P1 P2 *
**
** Pop a single boolean from the stack.  If the boolean popped is
** false, then jump to p2.  Otherwise continue to the next instruction.
** An integer is false if zero and true otherwise.  A string is
** false if it has zero length and true otherwise.
**
** If the value popped of the stack is NULL, then take the jump if P1
** is true and fall through if P1 is false.
*/
case OP_If:
case OP_IfNot: {
  int c;
  VERIFY( if( p->tos<0 ) goto not_enough_stack; )
  if( aStack[p->tos].flags & STK_Null ){
    c = pOp->p1;
  }else{
    Integerify(p, p->tos);
    c = aStack[p->tos].i;
    if( pOp->opcode==OP_IfNot ) c = !c;
  }
  POPSTACK;
  if( c ) pc = pOp->p2-1;
  break;
}

/* Opcode: IsNull P1 P2 *
**
** If any of the top abs(P1) values on the stack are NULL, then jump
** to P2.  The stack is popped P1 times if P1>0.  If P1<0 then all values
** are left unchanged on the stack.
*/
case OP_IsNull: {
  int i, cnt;
  cnt = pOp->p1;
  if( cnt<0 ) cnt = -cnt;
  VERIFY( if( p->tos+1-cnt<0 ) goto not_enough_stack; )
  for(i=0; i<cnt; i++){
    if( aStack[p->tos-i].flags & STK_Null ){
      pc = pOp->p2-1;
      break;
    }
  }
  if( pOp->p1>0 ) PopStack(p, cnt);
  break;
}

/* Opcode: NotNull P1 P2 *
**
** Jump to P2 if the top value on the stack is not NULL.  Pop the
** stack if P1 is greater than zero.  If P1 is less than or equal to
** zero then leave the value on the stack.
*/
case OP_NotNull: {
  VERIFY( if( p->tos<0 ) goto not_enough_stack; )
  if( (aStack[p->tos].flags & STK_Null)==0 ) pc = pOp->p2-1;
  if( pOp->p1>0 ){ POPSTACK; }
  break;
}

/* Opcode: MakeRecord P1 P2 *
**
** Convert the top P1 entries of the stack into a single entry
** suitable for use as a data record in a database table.  The
** details of the format are irrelavant as long as the OP_Column
** opcode can decode the record later.  Refer to source code
** comments for the details of the record format.
**
** If P2 is true (non-zero) and one or more of the P1 entries
** that go into building the record is NULL, then add some extra
** bytes to the record to make it distinct for other entries created
** during the same run of the VDBE.  The extra bytes added are a
** counter that is reset with each run of the VDBE, so records
** created this way will not necessarily be distinct across runs.
** But they should be distinct for transient tables (created using
** OP_OpenTemp) which is what they are intended for.
**
** (Later:) The P2==1 option was intended to make NULLs distinct
** for the UNION operator.  But I have since discovered that NULLs
** are indistinct for UNION.  So this option is never used.
*/
case OP_MakeRecord: {
  char *zNewRecord;
  int nByte;
  int nField;
  int i, j;
  int idxWidth;
  u32 addr;
  int addUnique = 0;   /* True to cause bytes to be added to make the
                       ** generated record distinct */

  /* Assuming the record contains N fields, the record format looks
  ** like this:
  **
  **   -------------------------------------------------------------------
  **   | idx0 | idx1 | ... | idx(N-1) | idx(N) | data0 | ... | data(N-1) |
  **   -------------------------------------------------------------------
  **
  ** All data fields are converted to strings before being stored and
  ** are stored with their null terminators.  NULL entries omit the
  ** null terminator.  Thus an empty string uses 1 byte and a NULL uses
  ** zero bytes.  Data(0) is taken from the lowest element of the stack
  ** and data(N-1) is the top of the stack.
  **
  ** Each of the idx() entries is either 1, 2, or 3 bytes depending on
  ** how big the total record is.  Idx(0) contains the offset to the start
  ** of data(0).  Idx(k) contains the offset to the start of data(k).
  ** Idx(N) contains the total number of bytes in the record.
  */
  nField = pOp->p1;
  VERIFY( if( p->tos+1<nField ) goto not_enough_stack; )
  nByte = 0;
  for(i=p->tos-nField+1; i<=p->tos; i++){
    if( (aStack[i].flags & STK_Null) ){
      addUnique = pOp->p2;
    }else{
      if( Stringify(p, i) ) goto no_mem;
      nByte += aStack[i].n;
    }
  }
  if( addUnique ) nByte += sizeof(uniqueCnt);
  if( nByte + nField + 1 < 256 ){
    idxWidth = 1;
  }else if( nByte + 2*nField + 2 < 65536 ){
    idxWidth = 2;
  }else{
    idxWidth = 3;
  }
  nByte += idxWidth*(nField + 1);
  if( nByte>MAX_BYTES_PER_ROW ){
    rc = SQLITE_TOOBIG;
    goto abort_due_to_error;
  }
  zNewRecord = sqliteMalloc( nByte );
  if( zNewRecord==0 ) goto no_mem;
  j = 0;
  addr = idxWidth*(nField+1) + addUnique*sizeof(uniqueCnt);
  for(i=p->tos-nField+1; i<=p->tos; i++){
    zNewRecord[j++] = addr & 0xff;
    if( idxWidth>1 ){
      zNewRecord[j++] = (addr>>8)&0xff;
      if( idxWidth>2 ){
        zNewRecord[j++] = (addr>>16)&0xff;
      }
    }
    if( (aStack[i].flags & STK_Null)==0 ){
      addr += aStack[i].n;
    }
  }
  zNewRecord[j++] = addr & 0xff;
  if( idxWidth>1 ){
    zNewRecord[j++] = (addr>>8)&0xff;
    if( idxWidth>2 ){
      zNewRecord[j++] = (addr>>16)&0xff;
    }
  }
  if( addUnique ){
    memcpy(&zNewRecord[j], &uniqueCnt, sizeof(uniqueCnt));
    uniqueCnt++;
    j += sizeof(uniqueCnt);
  }
  for(i=p->tos-nField+1; i<=p->tos; i++){
    if( (aStack[i].flags & STK_Null)==0 ){
      memcpy(&zNewRecord[j], zStack[i], aStack[i].n);
      j += aStack[i].n;
    }
  }
  PopStack(p, nField);
  VERIFY( NeedStack(p, p->tos+1); )
  p->tos++;
  aStack[p->tos].n = nByte;
  aStack[p->tos].flags = STK_Str | STK_Dyn;
  zStack[p->tos] = zNewRecord;
  break;
}

/* Opcode: MakeKey P1 P2 *
**
** Convert the top P1 entries of the stack into a single entry suitable
** for use as the key in an index.  The top P1 records are
** converted to strings and merged.  The null-terminators 
** are retained and used as separators.
** The lowest entry in the stack is the first field and the top of the
** stack becomes the last.
**
** If P2 is not zero, then the original entries remain on the stack
** and the new key is pushed on top.  If P2 is zero, the original
** data is popped off the stack first then the new key is pushed
** back in its place.
**
** See also: MakeIdxKey, SortMakeKey
*/
/* Opcode: MakeIdxKey P1 P2 *
**
** Convert the top P1 entries of the stack into a single entry suitable
** for use as the key in an index.  In addition, take one additional integer
** off of the stack, treat that integer as a four-byte record number, and
** append the four bytes to the key.  Thus a total of P1+1 entries are
** popped from the stack for this instruction and a single entry is pushed
** back.  The first P1 entries that are popped are strings and the last
** entry (the lowest on the stack) is an integer record number.
**
** The converstion of the first P1 string entries occurs just like in
** MakeKey.  Each entry is separated from the others by a null.
** The entire concatenation is null-terminated.  The lowest entry
** in the stack is the first field and the top of the stack becomes the
** last.
**
** If P2 is not zero and one or more of the P1 entries that go into the
** generated key is NULL, then jump to P2 after the new key has been
** pushed on the stack.  In other words, jump to P2 if the key is
** guaranteed to be unique.  This jump can be used to skip a subsequent
** uniqueness test.
**
** See also:  MakeKey, SortMakeKey
*/
case OP_MakeIdxKey:
case OP_MakeKey: {
  char *zNewKey;
  int nByte;
  int nField;
  int addRowid;
  int i, j;
  int containsNull = 0;

  addRowid = pOp->opcode==OP_MakeIdxKey;
  nField = pOp->p1;
  VERIFY( if( p->tos+1+addRowid<nField ) goto not_enough_stack; )
  nByte = 0;
  for(i=p->tos-nField+1; i<=p->tos; i++){
    int flags = aStack[i].flags;
    int len;
    char *z;
    if( flags & STK_Null ){
      nByte += 2;
      containsNull = 1;
    }else if( flags & STK_Real ){
      z = aStack[i].z;
      sqliteRealToSortable(aStack[i].r, &z[1]);
      z[0] = 0;
      Release(p, i);
      len = strlen(&z[1]);
      zStack[i] = 0;
      aStack[i].flags = STK_Real;
      aStack[i].n = len+2;
      nByte += aStack[i].n;
    }else if( flags & STK_Int ){
      z = aStack[i].z;
      aStack[i].r = aStack[i].i;
      sqliteRealToSortable(aStack[i].r, &z[1]);
      z[0] = 0;
      Release(p, i);
      len = strlen(&z[1]);
      zStack[i] = 0;
      aStack[i].flags = STK_Int;
      aStack[i].n = len+2;
      nByte += aStack[i].n;
    }else{
      assert( flags & STK_Str );
      if( isNumber(zStack[i]) ){
        aStack[i].r = atof(zStack[i]);
        Release(p, i);
        z = aStack[i].z;
        sqliteRealToSortable(aStack[i].r, &z[1]);
        z[0] = 0;
        len = strlen(&z[1]);
        zStack[i] = 0;
        aStack[i].flags = STK_Real;
        aStack[i].n = len+2;
      }
      nByte += aStack[i].n;
    }
  }
  if( nByte+sizeof(u32)>MAX_BYTES_PER_ROW ){
    rc = SQLITE_TOOBIG;
    goto abort_due_to_error;
  }
  if( addRowid ) nByte += sizeof(u32);
  zNewKey = sqliteMalloc( nByte );
  if( zNewKey==0 ) goto no_mem;
  j = 0;
  for(i=p->tos-nField+1; i<=p->tos; i++){
    if( aStack[i].flags & STK_Null ){
      zNewKey[j++] = 0;
      zNewKey[j++] = 0;
    }else{
      memcpy(&zNewKey[j], zStack[i] ? zStack[i] : aStack[i].z, aStack[i].n);
      j += aStack[i].n;
    }
  }
  if( addRowid ){
    u32 iKey;
    Integerify(p, p->tos-nField);
    iKey = intToKey(aStack[p->tos-nField].i);
    memcpy(&zNewKey[j], &iKey, sizeof(u32));
    PopStack(p, nField+1);
    if( pOp->p2 && containsNull ) pc = pOp->p2 - 1;
  }else{
    if( pOp->p2==0 ) PopStack(p, nField+addRowid);
  }
  VERIFY( NeedStack(p, p->tos+1); )
  p->tos++;
  aStack[p->tos].n = nByte;
  aStack[p->tos].flags = STK_Str|STK_Dyn;
  zStack[p->tos] = zNewKey;
  break;
}

/* Opcode: IncrKey * * *
**
** The top of the stack should contain an index key generated by
** The MakeKey opcode.  This routine increases the least significant
** byte of that key by one.  This is used so that the MoveTo opcode
** will move to the first entry greater than the key rather than to
** the key itself.
*/
case OP_IncrKey: {
  int tos = p->tos;

  VERIFY( if( tos<0 ) goto bad_instruction );
  if( Stringify(p, tos) ) goto no_mem;
  if( aStack[tos].flags & STK_Static ){
    char *zNew = sqliteMalloc( aStack[tos].n );
    memcpy(zNew, zStack[tos], aStack[tos].n);
    zStack[tos] = zNew;
    aStack[tos].flags = STK_Str | STK_Dyn;
  }
  zStack[tos][aStack[tos].n-1]++;
  break;
}

/* Opcode: Checkpoint * * *
**
** Begin a checkpoint.  A checkpoint is the beginning of a operation that
** is part of a larger transaction but which might need to be rolled back
** itself without effecting the containing transaction.  A checkpoint will
** be automatically committed or rollback when the VDBE halts.
*/
case OP_Checkpoint: {
  rc = sqliteBtreeBeginCkpt(pBt);
  if( rc==SQLITE_OK && db->pBeTemp ){
     rc = sqliteBtreeBeginCkpt(db->pBeTemp);
  }
  break;
}

/* Opcode: Transaction * * *
**
** Begin a transaction.  The transaction ends when a Commit or Rollback
** opcode is encountered.  Depending on the ON CONFLICT setting, the
** transaction might also be rolled back if an error is encountered.
**
** A write lock is obtained on the database file when a transaction is
** started.  No other process can read or write the file while the
** transaction is underway.  Starting a transaction also creates a
** rollback journal.  A transaction must be started before any changes
** can be made to the database.
*/
case OP_Transaction: {
  int busy = 0;
  if( db->pBeTemp ){
    rc = sqliteBtreeBeginTrans(db->pBeTemp);
    if( rc!=SQLITE_OK ){
      goto abort_due_to_error;
    }
  }
  do{
    rc = sqliteBtreeBeginTrans(pBt);
    switch( rc ){
      case SQLITE_BUSY: {
        if( xBusy==0 || (*xBusy)(pBusyArg, "", ++busy)==0 ){
          sqliteSetString(pzErrMsg, sqlite_error_string(rc), 0);
          busy = 0;
        }
        break;
      }
      case SQLITE_OK: {
        busy = 0;
        break;
      }
      default: {
        goto abort_due_to_error;
      }
    }
  }while( busy );
  undoTransOnError = 1;
  break;
}

/* Opcode: Commit * * *
**
** Cause all modifications to the database that have been made since the
** last Transaction to actually take effect.  No additional modifications
** are allowed until another transaction is started.  The Commit instruction
** deletes the journal file and releases the write lock on the database.
** A read lock continues to be held if there are still cursors open.
*/
case OP_Commit: {
  if( db->pBeTemp==0 || (rc = sqliteBtreeCommit(db->pBeTemp))==SQLITE_OK ){
    rc = sqliteBtreeCommit(pBt);
  }
  if( rc==SQLITE_OK ){
    sqliteCommitInternalChanges(db);
  }else{
    sqliteRollbackInternalChanges(db);
  }
  break;
}

/* Opcode: Rollback * * *
**
** Cause all modifications to the database that have been made since the
** last Transaction to be undone. The database is restored to its state
** before the Transaction opcode was executed.  No additional modifications
** are allowed until another transaction is started.
**
** This instruction automatically closes all cursors and releases both
** the read and write locks on the database.
*/
case OP_Rollback: {
  if( db->pBeTemp ){
    sqliteBtreeRollback(db->pBeTemp);
  }
  rc = sqliteBtreeRollback(pBt);
  sqliteRollbackInternalChanges(db);
  break;
}

/* Opcode: ReadCookie * P2 *
**
** When P2==0, 
** read the schema cookie from the database file and push it onto the
** stack.  The schema cookie is an integer that is used like a version
** number for the database schema.  Everytime the schema changes, the
** cookie changes to a new random value.  This opcode is used during
** initialization to read the initial cookie value so that subsequent
** database accesses can verify that the cookie has not changed.
**
** If P2>0, then read global database parameter number P2.  There is
** a small fixed number of global database parameters.  P2==1 is the
** database version number.  P2==2 is the recommended pager cache size.
** Other parameters are currently unused.
**
** There must be a read-lock on the database (either a transaction
** must be started or there must be an open cursor) before
** executing this instruction.
*/
case OP_ReadCookie: {
  int i = ++p->tos;
  int aMeta[SQLITE_N_BTREE_META];
  assert( pOp->p2<SQLITE_N_BTREE_META );
  VERIFY( if( NeedStack(p, p->tos) ) goto no_mem; )
  rc = sqliteBtreeGetMeta(pBt, aMeta);
  aStack[i].i = aMeta[1+pOp->p2];
  aStack[i].flags = STK_Int;
  break;
}

/* Opcode: SetCookie * P2 *
**
** When P2==0,
** this operation changes the value of the schema cookie on the database.
** The new value is top of the stack.
** When P2>0, the value of global database parameter
** number P2 is changed.  See ReadCookie for more information about
** global database parametes.
**
** The schema cookie changes its value whenever the database schema changes.
** That way, other processes can recognize when the schema has changed
** and reread it.
**
** A transaction must be started before executing this opcode.
*/
case OP_SetCookie: {
  int aMeta[SQLITE_N_BTREE_META];
  assert( pOp->p2<SQLITE_N_BTREE_META );
  VERIFY( if( p->tos<0 ) goto not_enough_stack; )
  Integerify(p, p->tos)
  rc = sqliteBtreeGetMeta(pBt, aMeta);
  if( rc==SQLITE_OK ){
    aMeta[1+pOp->p2] = aStack[p->tos].i;
    rc = sqliteBtreeUpdateMeta(pBt, aMeta);
  }
  POPSTACK;
  break;
}

/* Opcode: VerifyCookie P1 P2 *
**
** Check the value of global database parameter number P2 and make
** sure it is equal to P1.  P2==0 is the schema cookie.  P1==1 is
** the database version.  If the values do not match, abort with
** an SQLITE_SCHEMA error.
**
** The cookie changes its value whenever the database schema changes.
** This operation is used to detect when that the cookie has changed
** and that the current process needs to reread the schema.
**
** Either a transaction needs to have been started or an OP_Open needs
** to be executed (to establish a read lock) before this opcode is
** invoked.
*/
case OP_VerifyCookie: {
  int aMeta[SQLITE_N_BTREE_META];
  assert( pOp->p2<SQLITE_N_BTREE_META );
  rc = sqliteBtreeGetMeta(pBt, aMeta);
  if( rc==SQLITE_OK && aMeta[1+pOp->p2]!=pOp->p1 ){
    sqliteSetString(pzErrMsg, "database schema has changed", 0);
    rc = SQLITE_SCHEMA;
  }
  break;
}

/* Opcode: Open P1 P2 P3
**
** Open a read-only cursor for the database table whose root page is
** P2 in the main database file.  Give the new cursor an identifier
** of P1.  The P1 values need not be contiguous but all P1 values
** should be small integers.  It is an error for P1 to be negative.
**
** If P2==0 then take the root page number from the top of the stack.
**
** There will be a read lock on the database whenever there is an
** open cursor.  If the database was unlocked prior to this instruction
** then a read lock is acquired as part of this instruction.  A read
** lock allows other processes to read the database but prohibits
** any other process from modifying the database.  The read lock is
** released when all cursors are closed.  If this instruction attempts
** to get a read lock but fails, the script terminates with an
** SQLITE_BUSY error code.
**
** The P3 value is the name of the table or index being opened.
** The P3 value is not actually used by this opcode and may be
** omitted.  But the code generator usually inserts the index or
** table name into P3 to make the code easier to read.
**
** See also OpenAux and OpenWrite.
*/
/* Opcode: OpenAux P1 P2 P3
**
** Open a read-only cursor in the auxiliary table set.  This opcode
** works exactly like OP_Open except that it opens the cursor on the
** auxiliary table set (the file used to store tables created using
** CREATE TEMPORARY TABLE) instead of in the main database file.
** See OP_Open for additional information.
*/
/* Opcode: OpenWrite P1 P2 P3
**
** Open a read/write cursor named P1 on the table or index whose root
** page is P2.  If P2==0 then take the root page number from the stack.
**
** This instruction works just like Open except that it opens the cursor
** in read/write mode.  For a given table, there can be one or more read-only
** cursors or a single read/write cursor but not both.
**
** See also OpWrAux.
*/
/* Opcode: OpenWrAux P1 P2 P3
**
** Open a read/write cursor in the auxiliary table set.  This opcode works
** just like OpenWrite except that the auxiliary table set (the file used
** to store tables created using CREATE TEMPORARY TABLE) is used in place
** of the main database file.
*/
case OP_OpenAux:
case OP_OpenWrAux:
case OP_OpenWrite:
case OP_Open: {
  int busy = 0;
  int i = pOp->p1;
  int tos = p->tos;
  int p2 = pOp->p2;
  int wrFlag;
  Btree *pX;
  switch( pOp->opcode ){
    case OP_Open:        wrFlag = 0;  pX = pBt;          break;
    case OP_OpenWrite:   wrFlag = 1;  pX = pBt;          break;
    case OP_OpenAux:     wrFlag = 0;  pX = db->pBeTemp;  break;
    case OP_OpenWrAux:   wrFlag = 1;  pX = db->pBeTemp;  break;
  }
  assert( pX!=0 );
  if( p2<=0 ){
    if( tos<0 ) goto not_enough_stack;
    Integerify(p, tos);
    p2 = p->aStack[tos].i;
    POPSTACK;
    if( p2<2 ){
      sqliteSetString(pzErrMsg, "root page number less than 2", 0);
      rc = SQLITE_INTERNAL;
      goto cleanup;
    }
  }
  VERIFY( if( i<0 ) goto bad_instruction; )
  if( i>=p->nCursor ){
    int j;
    Cursor *aCsr = sqliteRealloc( p->aCsr, (i+1)*sizeof(Cursor) );
    if( aCsr==0 ) goto no_mem;
    p->aCsr = aCsr;
    for(j=p->nCursor; j<=i; j++){
      memset(&p->aCsr[j], 0, sizeof(Cursor));
    }
    p->nCursor = i+1;
  }
  cleanupCursor(&p->aCsr[i]);
  memset(&p->aCsr[i], 0, sizeof(Cursor));
  p->aCsr[i].nullRow = 1;
  do{
    rc = sqliteBtreeCursor(pX, p2, wrFlag, &p->aCsr[i].pCursor);
    switch( rc ){
      case SQLITE_BUSY: {
        if( xBusy==0 || (*xBusy)(pBusyArg, pOp->p3, ++busy)==0 ){
          sqliteSetString(pzErrMsg, sqlite_error_string(rc), 0);
          busy = 0;
        }
        break;
      }
      case SQLITE_OK: {
        busy = 0;
        break;
      }
      default: {
        goto abort_due_to_error;
      }
    }
  }while( busy );
  break;
}

/* Opcode: OpenTemp P1 P2 *
**
** Open a new cursor that points to a table or index in a temporary
** database file.  The temporary file is opened read/write even if 
** the main database is read-only.  The temporary file is deleted
** when the cursor is closed.
**
** The cursor points to a BTree table if P2==0 and to a BTree index
** if P2==1.  A BTree table must have an integer key and can have arbitrary
** data.  A BTree index has no data but can have an arbitrary key.
**
** This opcode is used for tables that exist for the duration of a single
** SQL statement only.  Tables created using CREATE TEMPORARY TABLE
** are opened using OP_OpenAux or OP_OpenWrAux.  "Temporary" in the
** context of this opcode means for the duration of a single SQL statement
** whereas "Temporary" in the context of CREATE TABLE means for the duration
** of the connection to the database.  Same word; different meanings.
*/
case OP_OpenTemp: {
  int i = pOp->p1;
  Cursor *pCx;
  VERIFY( if( i<0 ) goto bad_instruction; )
  if( i>=p->nCursor ){
    int j;
    Cursor *aCsr = sqliteRealloc( p->aCsr, (i+1)*sizeof(Cursor) );
    if( aCsr==0 ){ goto no_mem; }
    p->aCsr = aCsr;
    for(j=p->nCursor; j<=i; j++){
      memset(&p->aCsr[j], 0, sizeof(Cursor));
    }
    p->nCursor = i+1;
  }
  pCx = &p->aCsr[i];
  cleanupCursor(pCx);
  memset(pCx, 0, sizeof(*pCx));
  pCx->nullRow = 1;
  rc = sqliteBtreeOpen(0, 0, TEMP_PAGES, &pCx->pBt);
  if( rc==SQLITE_OK ){
    rc = sqliteBtreeBeginTrans(pCx->pBt);
  }
  if( rc==SQLITE_OK ){
    if( pOp->p2 ){
      int pgno;
      rc = sqliteBtreeCreateIndex(pCx->pBt, &pgno);
      if( rc==SQLITE_OK ){
        rc = sqliteBtreeCursor(pCx->pBt, pgno, 1, &pCx->pCursor);
      }
    }else{
      rc = sqliteBtreeCursor(pCx->pBt, 2, 1, &pCx->pCursor);
    }
  }
  break;
}

/* Opcode: Close P1 * *
**
** Close a cursor previously opened as P1.  If P1 is not
** currently open, this instruction is a no-op.
*/
case OP_Close: {
  int i = pOp->p1;
  if( i>=0 && i<p->nCursor && p->aCsr[i].pCursor ){
    cleanupCursor(&p->aCsr[i]);
  }
  break;
}

/* Opcode: MoveTo P1 P2 *
**
** Pop the top of the stack and use its value as a key.  Reposition
** cursor P1 so that it points to an entry with a matching key.  If
** the table contains no record with a matching key, then the cursor
** is left pointing at the first record that is greater than the key.
** If there are no records greater than the key and P2 is not zero,
** then an immediate jump to P2 is made.
**
** See also: Found, NotFound, Distinct
*/
case OP_MoveTo: {
  int i = pOp->p1;
  int tos = p->tos;
  Cursor *pC;

  VERIFY( if( tos<0 ) goto not_enough_stack; )
  if( i>=0 && i<p->nCursor && (pC = &p->aCsr[i])->pCursor!=0 ){
    int res;
    if( aStack[tos].flags & STK_Int ){
      int iKey = intToKey(aStack[tos].i);
      sqliteBtreeMoveto(pC->pCursor, (char*)&iKey, sizeof(int), &res);
      pC->lastRecno = aStack[tos].i;
      pC->recnoIsValid = res==0;
    }else{
      if( Stringify(p, tos) ) goto no_mem;
      sqliteBtreeMoveto(pC->pCursor, zStack[tos], aStack[tos].n, &res);
      pC->recnoIsValid = 0;
    }
    pC->nullRow = 0;
    sqlite_search_count++;
    if( res<0 ){
      sqliteBtreeNext(pC->pCursor, &res);
      pC->recnoIsValid = 0;
      if( res && pOp->p2>0 ){
        pc = pOp->p2 - 1;
      }
    }
  }
  POPSTACK;
  break;
}

/* Opcode: Distinct P1 P2 *
**
** Use the top of the stack as a string key.  If a record with that key does
** not exist in the table of cursor P1, then jump to P2.  If the record
** does already exist, then fall thru.  The cursor is left pointing
** at the record if it exists. The key is not popped from the stack.
**
** This operation is similar to NotFound except that this operation
** does not pop the key from the stack.
**
** See also: Found, NotFound, MoveTo, IsUnique, NotExists
*/
/* Opcode: Found P1 P2 *
**
** Use the top of the stack as a string key.  If a record with that key
** does exist in table of P1, then jump to P2.  If the record
** does not exist, then fall thru.  The cursor is left pointing
** to the record if it exists.  The key is popped from the stack.
**
** See also: Distinct, NotFound, MoveTo, IsUnique, NotExists
*/
/* Opcode: NotFound P1 P2 *
**
** Use the top of the stack as a string key.  If a record with that key
** does not exist in table of P1, then jump to P2.  If the record
** does exist, then fall thru.  The cursor is left pointing to the
** record if it exists.  The key is popped from the stack.
**
** The difference between this operation and Distinct is that
** Distinct does not pop the key from the stack.
**
** See also: Distinct, Found, MoveTo, NotExists, IsUnique
*/
case OP_Distinct:
case OP_NotFound:
case OP_Found: {
  int i = pOp->p1;
  int tos = p->tos;
  int alreadyExists = 0;
  Cursor *pC;
  VERIFY( if( tos<0 ) goto not_enough_stack; )
  if( VERIFY( i>=0 && i<p->nCursor && ) (pC = &p->aCsr[i])->pCursor!=0 ){
    int res, rx;
    if( Stringify(p, tos) ) goto no_mem;
    rx = sqliteBtreeMoveto(pC->pCursor, zStack[tos], aStack[tos].n, &res);
    alreadyExists = rx==SQLITE_OK && res==0;
  }
  if( pOp->opcode==OP_Found ){
    if( alreadyExists ) pc = pOp->p2 - 1;
  }else{
    if( !alreadyExists ) pc = pOp->p2 - 1;
  }
  if( pOp->opcode!=OP_Distinct ){
    POPSTACK;
  }
  break;
}

/* Opcode: IsUnique P1 P2 *
**
** The top of the stack is an integer record number.  Call this
** record number R.  The next on the stack is an index key created
** using MakeIdxKey.  Call it K.  This instruction pops R from the
** stack but it leaves K unchanged.
**
** P1 is an index.  So all but the last four bytes of K are an
** index string.  The last four bytes of K are a record number.
**
** This instruction asks if there is an entry in P1 where the
** index string matches K but the record number is different
** from R.  If there is no such entry, then there is an immediate
** jump to P2.  If any entry does exist where the index string
** matches K but the record number is not R, then the record
** number for that entry is pushed onto the stack and control
** falls through to the next instruction.
**
** See also: Distinct, NotFound, NotExists, Found
*/
case OP_IsUnique: {
  int i = pOp->p1;
  int tos = p->tos;
  int nos = tos-1;
  BtCursor *pCrsr;
  int R;

  /* Pop the value R off the top of the stack
  */
  VERIFY( if( nos<0 ) goto not_enough_stack; )
  Integerify(p, tos);
  R = aStack[tos].i;   
  POPSTACK;
  if( VERIFY( i>=0 && i<p->nCursor && ) (pCrsr = p->aCsr[i].pCursor)!=0 ){
    int res, rc;
    int v;         /* The record number on the P1 entry that matches K */
    char *zKey;    /* The value of K */
    int nKey;      /* Number of bytes in K */

    /* Make sure K is a string and make zKey point to K
    */
    if( Stringify(p, nos) ) goto no_mem;
    zKey = zStack[nos];
    nKey = aStack[nos].n;
    assert( nKey >= 4 );

    /* Search for an entry in P1 where all but the last four bytes match K.
    ** If there is no such entry, jump immediately to P2.
    */
    rc = sqliteBtreeMoveto(pCrsr, zKey, nKey-4, &res);
    if( rc!=SQLITE_OK ) goto abort_due_to_error;
    if( res<0 ){
      rc = sqliteBtreeNext(pCrsr, &res);
      if( res ){
        pc = pOp->p2 - 1;
        break;
      }
    }
    rc = sqliteBtreeKeyCompare(pCrsr, zKey, nKey-4, 4, &res);
    if( rc!=SQLITE_OK ) goto abort_due_to_error;
    if( res>0 ){
      pc = pOp->p2 - 1;
      break;
    }

    /* At this point, pCrsr is pointing to an entry in P1 where all but
    ** the last for bytes of the key match K.  Check to see if the last
    ** four bytes of the key are different from R.  If the last four
    ** bytes equal R then jump immediately to P2.
    */
    sqliteBtreeKey(pCrsr, nKey - 4, 4, (char*)&v);
    v = keyToInt(v);
    if( v==R ){
      pc = pOp->p2 - 1;
      break;
    }

    /* The last four bytes of the key are different from R.  Convert the
    ** last four bytes of the key into an integer and push it onto the
    ** stack.  (These bytes are the record number of an entry that
    ** violates a UNIQUE constraint.)
    */
    p->tos++;
    VERIFY( if( NeedStack(p, p->tos) ) goto no_mem; )
    aStack[tos].i = v;
    aStack[tos].flags = STK_Int;
  }
  break;
}

/* Opcode: NotExists P1 P2 *
**
** Use the top of the stack as a integer key.  If a record with that key
** does not exist in table of P1, then jump to P2.  If the record
** does exist, then fall thru.  The cursor is left pointing to the
** record if it exists.  The integer key is popped from the stack.
**
** The difference between this operation and NotFound is that this
** operation assumes the key is an integer and NotFound assumes it
** is a string.
**
** See also: Distinct, Found, MoveTo, NotFound, IsUnique
*/
case OP_NotExists: {
  int i = pOp->p1;
  int tos = p->tos;
  BtCursor *pCrsr;
  VERIFY( if( tos<0 ) goto not_enough_stack; )
  if( VERIFY( i>=0 && i<p->nCursor && ) (pCrsr = p->aCsr[i].pCursor)!=0 ){
    int res, rx, iKey;
    assert( aStack[tos].flags & STK_Int );
    iKey = intToKey(aStack[tos].i);
    rx = sqliteBtreeMoveto(pCrsr, (char*)&iKey, sizeof(int), &res);
    p->aCsr[i].lastRecno = aStack[tos].i;
    p->aCsr[i].recnoIsValid = res==0;
    p->aCsr[i].nullRow = 0;
    if( rx!=SQLITE_OK || res!=0 ){
      pc = pOp->p2 - 1;
      p->aCsr[i].recnoIsValid = 0;
    }
  }
  POPSTACK;
  break;
}

/* Opcode: NewRecno P1 * *
**
** Get a new integer record number used as the key to a table.
** The record number is not previously used as a key in the database
** table that cursor P1 points to.  The new record number is pushed 
** onto the stack.
*/
case OP_NewRecno: {
  int i = pOp->p1;
  int v = 0;
  Cursor *pC;
  if( VERIFY( i<0 || i>=p->nCursor || ) (pC = &p->aCsr[i])->pCursor==0 ){
    v = 0;
  }else{
    /* The next rowid or record number (different terms for the same
    ** thing) is obtained in a two-step algorithm.
    **
    ** First we attempt to find the largest existing rowid and add one
    ** to that.  But if the largest existing rowid is already the maximum
    ** positive integer, we have to fall through to the second
    ** probabilistic algorithm
    **
    ** The second algorithm is to select a rowid at random and see if
    ** it already exists in the table.  If it does not exist, we have
    ** succeeded.  If the random rowid does exist, we select a new one
    ** and try again, up to 1000 times.
    **
    ** For a table with less than 2 billion entries, the probability
    ** of not finding a unused rowid is about 1.0e-300.  This is a 
    ** non-zero probability, but it is still vanishingly small and should
    ** never cause a problem.  You are much, much more likely to have a
    ** hardware failure than for this algorithm to fail.
    **
    ** The analysis in the previous paragraph assumes that you have a good
    ** source of random numbers.  Is a library function like lrand48()
    ** good enough?  Maybe. Maybe not. It's hard to know whether there
    ** might be subtle bugs is some implementations of lrand48() that
    ** could cause problems. To avoid uncertainty, SQLite uses its own 
    ** random number generator based on the RC4 algorithm.
    **
    ** To promote locality of reference for repetitive inserts, the
    ** first few attempts at chosing a random rowid pick values just a little
    ** larger than the previous rowid.  This has been shown experimentally
    ** to double the speed of the COPY operation.
    */
    int res, rx, cnt, x;
    cnt = 0;
    if( !pC->useRandomRowid ){
      rx = sqliteBtreeLast(pC->pCursor, &res);
      if( res ){
        v = 1;
      }else{
        sqliteBtreeKey(pC->pCursor, 0, sizeof(v), (void*)&v);
        v = keyToInt(v);
        if( v==0x7fffffff ){
          pC->useRandomRowid = 1;
        }else{
          v++;
        }
      }
    }
    if( pC->useRandomRowid ){
      v = db->priorNewRowid;
      cnt = 0;
      do{
        if( v==0 || cnt>2 ){
          v = sqliteRandomInteger();
          if( cnt<5 ) v &= 0xffffff;
        }else{
          v += sqliteRandomByte() + 1;
        }
        if( v==0 ) continue;
        x = intToKey(v);
        rx = sqliteBtreeMoveto(pC->pCursor, &x, sizeof(int), &res);
        cnt++;
      }while( cnt<1000 && rx==SQLITE_OK && res==0 );
      db->priorNewRowid = v;
      if( rx==SQLITE_OK && res==0 ){
        rc = SQLITE_FULL;
        goto abort_due_to_error;
      }
    }
    pC->recnoIsValid = 0;
  }
  VERIFY( NeedStack(p, p->tos+1); )
  p->tos++;
  aStack[p->tos].i = v;
  aStack[p->tos].flags = STK_Int;
  break;
}

/* Opcode: PutIntKey P1 P2 *
**
** Write an entry into the database file P1.  A new entry is
** created if it doesn't already exist or the data for an existing
** entry is overwritten.  The data is the value on the top of the
** stack.  The key is the next value down on the stack.  The key must
** be an integer.  The stack is popped twice by this instruction.
**
** If P2==1 then the row change count is incremented.  If P2==0 the
** row change count is unmodified.
*/
/* Opcode: PutStrKey P1 * *
**
** Write an entry into the database file P1.  A new entry is
** created if it doesn't already exist or the data for an existing
** entry is overwritten.  The data is the value on the top of the
** stack.  The key is the next value down on the stack.  The key must
** be a string.  The stack is popped twice by this instruction.
*/
case OP_PutIntKey:
case OP_PutStrKey: {
  int tos = p->tos;
  int nos = p->tos-1;
  int i = pOp->p1;
  VERIFY( if( nos<0 ) goto not_enough_stack; )
  if( VERIFY( i>=0 && i<p->nCursor && ) p->aCsr[i].pCursor!=0 ){
    char *zKey;
    int nKey, iKey;
    if( pOp->opcode==OP_PutStrKey ){
      if( Stringify(p, nos) ) goto no_mem;
      nKey = aStack[nos].n;
      zKey = zStack[nos];
    }else{
      assert( aStack[nos].flags & STK_Int );
      nKey = sizeof(int);
      iKey = intToKey(aStack[nos].i);
      zKey = (char*)&iKey;
      db->lastRowid = aStack[nos].i;
      if( pOp->p2 ) db->nChange++;
    }
    rc = sqliteBtreeInsert(p->aCsr[i].pCursor, zKey, nKey,
                        zStack[tos], aStack[tos].n);
    p->aCsr[i].recnoIsValid = 0;
  }
  POPSTACK;
  POPSTACK;
  break;
}

/* Opcode: Delete P1 P2 *
**
** Delete the record at which the P1 cursor is currently pointing.
**
** The cursor will be left pointing at either the next or the previous
** record in the table. If it is left pointing at the next record, then
** the next Next instruction will be a no-op.  Hence it is OK to delete
** a record from within an Next loop.
**
** The row change counter is incremented if P2==1 and is unmodified
** if P2==0.
*/
case OP_Delete: {
  int i = pOp->p1;
  if( VERIFY( i>=0 && i<p->nCursor && ) p->aCsr[i].pCursor!=0 ){
    rc = sqliteBtreeDelete(p->aCsr[i].pCursor);
  }
  if( pOp->p2 ) db->nChange++;
  break;
}

/* Opcode: KeyAsData P1 P2 *
**
** Turn the key-as-data mode for cursor P1 either on (if P2==1) or
** off (if P2==0).  In key-as-data mode, the Field opcode pulls
** data off of the key rather than the data.  This is useful for
** processing compound selects.
*/
case OP_KeyAsData: {
  int i = pOp->p1;
  if( VERIFY( i>=0 && i<p->nCursor && ) p->aCsr[i].pCursor!=0 ){
    p->aCsr[i].keyAsData = pOp->p2;
  }
  break;
}

/* Opcode: Column P1 P2 *
**
** Interpret the data that cursor P1 points to as
** a structure built using the MakeRecord instruction.
** (See the MakeRecord opcode for additional information about
** the format of the data.)
** Push onto the stack the value of the P2-th column contained
** in the data.
**
** If the KeyAsData opcode has previously executed on this cursor,
** then the field might be extracted from the key rather than the
** data.
*/
case OP_Column: {
  int amt, offset, end, payloadSize;
  int i = pOp->p1;
  int p2 = pOp->p2;
  int tos = p->tos+1;
  Cursor *pC;
  BtCursor *pCrsr;
  int idxWidth;
  unsigned char aHdr[10];
  int (*xRead)(BtCursor*, int, int, char*);

  VERIFY( if( NeedStack(p, tos+1) ) goto no_mem; )
  if( VERIFY( i>=0 && i<p->nCursor && ) (pC = &p->aCsr[i])->pCursor!=0 ){

    /* Use different access functions depending on whether the information
    ** is coming from the key or the data of the record.
    */
    pCrsr = pC->pCursor;
    if( pC->nullRow ){
      payloadSize = 0;
    }else if( pC->keyAsData ){
      sqliteBtreeKeySize(pCrsr, &payloadSize);
      xRead = sqliteBtreeKey;
    }else{
      sqliteBtreeDataSize(pCrsr, &payloadSize);
      xRead = sqliteBtreeData;
    }

    /* Figure out how many bytes in the column data and where the column
    ** data begins.
    */
    if( payloadSize==0 ){
      aStack[tos].flags = STK_Null;
      p->tos = tos;
      break;
    }else if( payloadSize<256 ){
      idxWidth = 1;
    }else if( payloadSize<65536 ){
      idxWidth = 2;
    }else{
      idxWidth = 3;
    }

    /* Figure out where the requested column is stored and how big it is.
    */
    if( payloadSize < idxWidth*(p2+1) ){
      rc = SQLITE_CORRUPT;
      goto abort_due_to_error;
    }
    (*xRead)(pCrsr, idxWidth*p2, idxWidth*2, (char*)aHdr);
    offset = aHdr[0];
    end = aHdr[idxWidth];
    if( idxWidth>1 ){
      offset |= aHdr[1]<<8;
      end |= aHdr[idxWidth+1]<<8;
      if( idxWidth>2 ){
        offset |= aHdr[2]<<16;
        end |= aHdr[idxWidth+2]<<16;
      }
    }
    amt = end - offset;
    if( amt<0 || offset<0 || end>payloadSize ){
      rc = SQLITE_CORRUPT;
      goto abort_due_to_error;
    }

    /* amt and offset now hold the offset to the start of data and the
    ** amount of data.  Go get the data and put it on the stack.
    */
    if( amt==0 ){
      aStack[tos].flags = STK_Null;
    }else if( amt<=NBFS ){
      (*xRead)(pCrsr, offset, amt, aStack[tos].z);
      aStack[tos].flags = STK_Str;
      zStack[tos] = aStack[tos].z;
      aStack[tos].n = amt;
    }else{
      char *z = sqliteMalloc( amt );
      if( z==0 ) goto no_mem;
      (*xRead)(pCrsr, offset, amt, z);
      aStack[tos].flags = STK_Str | STK_Dyn;
      zStack[tos] = z;
      aStack[tos].n = amt;
    }
    p->tos = tos;
  }
  break;
}

/* Opcode: Recno P1 * *
**
** Push onto the stack an integer which is the first 4 bytes of the
** the key to the current entry in a sequential scan of the database
** file P1.  The sequential scan should have been started using the 
** Next opcode.
*/
case OP_Recno: {
  int i = pOp->p1;
  int tos = ++p->tos;
  BtCursor *pCrsr;

  VERIFY( if( NeedStack(p, p->tos) ) goto no_mem; )
  if( VERIFY( i>=0 && i<p->nCursor && ) (pCrsr = p->aCsr[i].pCursor)!=0 ){
    int v;
    if( p->aCsr[i].recnoIsValid ){
      v = p->aCsr[i].lastRecno;
    }else if( p->aCsr[i].nullRow ){
      aStack[tos].flags = STK_Null;
      break;
    }else{
      sqliteBtreeKey(pCrsr, 0, sizeof(u32), (char*)&v);
      v = keyToInt(v);
    }
    aStack[tos].i = v;
    aStack[tos].flags = STK_Int;
  }
  break;
}

/* Opcode: FullKey P1 * *
**
** Extract the complete key from the record that cursor P1 is currently
** pointing to and push the key onto the stack as a string.
**
** Compare this opcode to Recno.  The Recno opcode extracts the first
** 4 bytes of the key and pushes those bytes onto the stack as an
** integer.  This instruction pushes the entire key as a string.
*/
case OP_FullKey: {
  int i = pOp->p1;
  int tos = ++p->tos;
  BtCursor *pCrsr;

  VERIFY( if( NeedStack(p, p->tos) ) goto no_mem; )
  VERIFY( if( !p->aCsr[i].keyAsData ) goto bad_instruction; )
  if( VERIFY( i>=0 && i<p->nCursor && ) (pCrsr = p->aCsr[i].pCursor)!=0 ){
    int amt;
    char *z;

    sqliteBtreeKeySize(pCrsr, &amt);
    if( amt<=0 ){
      rc = SQLITE_CORRUPT;
      goto abort_due_to_error;
    }
    if( amt>NBFS ){
      z = sqliteMalloc( amt );
      aStack[tos].flags = STK_Str | STK_Dyn;
    }else{
      z = aStack[tos].z;
      aStack[tos].flags = STK_Str;
    }
    sqliteBtreeKey(pCrsr, 0, amt, z);
    zStack[tos] = z;
    aStack[tos].n = amt;
  }
  break;
}

/* Opcode: NullRow P1 * *
**
** Move the cursor P1 to a null row.  Any OP_Column operations
** that occur while the cursor is on the null row will always push 
** a NULL onto the stack.
*/
case OP_NullRow: {
  int i = pOp->p1;
  BtCursor *pCrsr;

  if( VERIFY( i>=0 && i<p->nCursor && ) (pCrsr = p->aCsr[i].pCursor)!=0 ){
    p->aCsr[i].nullRow = 1;
  }
  break;
}

/* Opcode: Last P1 P2 *
**
** The next use of the Recno or Column or Next instruction for P1 
** will refer to the last entry in the database table or index.
** If the table or index is empty and P2>0, then jump immediately to P2.
** If P2 is 0 or if the table or index is not empty, fall through
** to the following instruction.
*/
case OP_Last: {
  int i = pOp->p1;
  BtCursor *pCrsr;

  if( VERIFY( i>=0 && i<p->nCursor && ) (pCrsr = p->aCsr[i].pCursor)!=0 ){
    int res;
    sqliteBtreeLast(pCrsr, &res);
    p->aCsr[i].nullRow = res;
    if( res && pOp->p2>0 ){
      pc = pOp->p2 - 1;
    }
  }
  break;
}

/* Opcode: Rewind P1 P2 *
**
** The next use of the Recno or Column or Next instruction for P1 
** will refer to the first entry in the database table or index.
** If the table or index is empty and P2>0, then jump immediately to P2.
** If P2 is 0 or if the table or index is not empty, fall through
** to the following instruction.
*/
case OP_Rewind: {
  int i = pOp->p1;
  BtCursor *pCrsr;

  if( VERIFY( i>=0 && i<p->nCursor && ) (pCrsr = p->aCsr[i].pCursor)!=0 ){
    int res;
    sqliteBtreeFirst(pCrsr, &res);
    p->aCsr[i].atFirst = res==0;
    p->aCsr[i].nullRow = res;
    if( res && pOp->p2>0 ){
      pc = pOp->p2 - 1;
    }
  }
  break;
}

/* Opcode: Next P1 P2 *
**
** Advance cursor P1 so that it points to the next key/data pair in its
** table or index.  If there are no more key/value pairs then fall through
** to the following instruction.  But if the cursor advance was successful,
** jump immediately to P2.
*/
case OP_Next: {
  int i = pOp->p1;
  BtCursor *pCrsr;

  if( VERIFY( i>=0 && i<p->nCursor && ) (pCrsr = p->aCsr[i].pCursor)!=0 ){
    int res;
    if( p->aCsr[i].nullRow ){
      res = 1;
    }else{
      rc = sqliteBtreeNext(pCrsr, &res);
      p->aCsr[i].nullRow = res;
    }
    if( res==0 ){
      pc = pOp->p2 - 1;
      sqlite_search_count++;
    }
    p->aCsr[i].recnoIsValid = 0;
  }
  break;
}

/* Opcode: IdxPut P1 P2 P3
**
** The top of the stack hold an SQL index key made using the
** MakeIdxKey instruction.  This opcode writes that key into the
** index P1.  Data for the entry is nil.
**
** If P2==1, then the key must be unique.  If the key is not unique,
** the program aborts with a SQLITE_CONSTRAINT error and the database
** is rolled back.  If P3 is not null, then it because part of the
** error message returned with the SQLITE_CONSTRAINT.
*/
case OP_IdxPut: {
  int i = pOp->p1;
  int tos = p->tos;
  BtCursor *pCrsr;
  VERIFY( if( tos<0 ) goto not_enough_stack; )
  if( VERIFY( i>=0 && i<p->nCursor && ) (pCrsr = p->aCsr[i].pCursor)!=0 ){
    int nKey = aStack[tos].n;
    const char *zKey = zStack[tos];
    if( pOp->p2 ){
      int res, n;
      assert( aStack[tos].n >= 4 );
      rc = sqliteBtreeMoveto(pCrsr, zKey, nKey-4, &res);
      if( rc!=SQLITE_OK ) goto abort_due_to_error;
      while( res!=0 ){
        int c;
        sqliteBtreeKeySize(pCrsr, &n);
        if( n==nKey
           && sqliteBtreeKeyCompare(pCrsr, zKey, nKey-4, 4, &c)==SQLITE_OK
           && c==0
        ){
          rc = SQLITE_CONSTRAINT;
          if( pOp->p3 && pOp->p3[0] ){
            sqliteSetString(pzErrMsg, "duplicate index entry: ", pOp->p3,0);
          }
          goto abort_due_to_error;
        }
        if( res<0 ){
          sqliteBtreeNext(pCrsr, &res);
          res = +1;
        }else{
          break;
        }
      }
    }
    rc = sqliteBtreeInsert(pCrsr, zKey, nKey, "", 0);
  }
  POPSTACK;
  break;
}

/* Opcode: IdxDelete P1 * *
**
** The top of the stack is an index key built using the MakeIdxKey opcode.
** This opcode removes that entry from the index.
*/
case OP_IdxDelete: {
  int i = pOp->p1;
  int tos = p->tos;
  BtCursor *pCrsr;
  VERIFY( if( tos<0 ) goto not_enough_stack; )
  if( VERIFY( i>=0 && i<p->nCursor && ) (pCrsr = p->aCsr[i].pCursor)!=0 ){
    int rx, res;
    rx = sqliteBtreeMoveto(pCrsr, zStack[tos], aStack[tos].n, &res);
    if( rx==SQLITE_OK && res==0 ){
      rc = sqliteBtreeDelete(pCrsr);
    }
  }
  POPSTACK;
  break;
}

/* Opcode: IdxRecno P1 * *
**
** Push onto the stack an integer which is the last 4 bytes of the
** the key to the current entry in index P1.  These 4 bytes should
** be the record number of the table entry to which this index entry
** points.
**
** See also: Recno, MakeIdxKey.
*/
case OP_IdxRecno: {
  int i = pOp->p1;
  int tos = ++p->tos;
  BtCursor *pCrsr;

  VERIFY( if( NeedStack(p, p->tos) ) goto no_mem; )
  if( VERIFY( i>=0 && i<p->nCursor && ) (pCrsr = p->aCsr[i].pCursor)!=0 ){
    int v;
    int sz;
    sqliteBtreeKeySize(pCrsr, &sz);
    sqliteBtreeKey(pCrsr, sz - sizeof(u32), sizeof(u32), (char*)&v);
    v = keyToInt(v);
    aStack[tos].i = v;
    aStack[tos].flags = STK_Int;
  }
  break;
}

/* Opcode: IdxGT P1 P2 *
**
** Compare the top of the stack against the key on the index entry that
** cursor P1 is currently pointing to.  Ignore the last 4 bytes of the
** index entry.  If the index entry is greater than the top of the stack
** then jump to P2.  Otherwise fall through to the next instruction.
** In either case, the stack is popped once.
*/
/* Opcode: IdxGE P1 P2 *
**
** Compare the top of the stack against the key on the index entry that
** cursor P1 is currently pointing to.  Ignore the last 4 bytes of the
** index entry.  If the index entry is greater than or equal to 
** the top of the stack
** then jump to P2.  Otherwise fall through to the next instruction.
** In either case, the stack is popped once.
*/
case OP_IdxGT:
case OP_IdxGE: {
  int i= pOp->p1;
  int tos = p->tos;
  BtCursor *pCrsr;

  if( VERIFY( i>=0 && i<p->nCursor && ) (pCrsr = p->aCsr[i].pCursor)!=0 ){
    int res, rc;
 
    if( Stringify(p, tos) ) goto no_mem;
    rc = sqliteBtreeKeyCompare(pCrsr, zStack[tos], aStack[tos].n, 4, &res);
    if( rc!=SQLITE_OK ){
      break;
    }
    if( pOp->opcode==OP_IdxGE ){
      res++;
    }
    if( res>0 ){
      pc = pOp->p2 - 1 ;
    }
  }
  POPSTACK;
  break;
}

/* Opcode: Destroy P1 P2 *
**
** Delete an entire database table or index whose root page in the database
** file is given by P1.
**
** The table being destroyed is in the main database file if P2==0.  If
** P2==1 then the table to be clear is in the auxiliary database file
** that is used to store tables create using CREATE TEMPORARY TABLE.
**
** See also: Clear
*/
case OP_Destroy: {
  sqliteBtreeDropTable(pOp->p2 ? db->pBeTemp : pBt, pOp->p1);
  break;
}

/* Opcode: Clear P1 P2 *
**
** Delete all contents of the database table or index whose root page
** in the database file is given by P1.  But, unlike Destroy, do not
** remove the table or index from the database file.
**
** The table being clear is in the main database file if P2==0.  If
** P2==1 then the table to be clear is in the auxiliary database file
** that is used to store tables create using CREATE TEMPORARY TABLE.
**
** See also: Destroy
*/
case OP_Clear: {
  sqliteBtreeClearTable(pOp->p2 ? db->pBeTemp : pBt, pOp->p1);
  break;
}

/* Opcode: CreateTable * P2 P3
**
** Allocate a new table in the main database file if P2==0 or in the
** auxiliary database file if P2==1.  Push the page number
** for the root page of the new table onto the stack.
**
** The root page number is also written to a memory location that P3
** points to.  This is the mechanism is used to write the root page
** number into the parser's internal data structures that describe the
** new table.
**
** The difference between a table and an index is this:  A table must
** have a 4-byte integer key and can have arbitrary data.  An index
** has an arbitrary key but no data.
**
** See also: CreateIndex
*/
/* Opcode: CreateIndex * P2 P3
**
** Allocate a new index in the main database file if P2==0 or in the
** auxiliary database file if P2==1.  Push the page number of the
** root page of the new index onto the stack.
**
** See documentation on OP_CreateTable for additional information.
*/
case OP_CreateIndex:
case OP_CreateTable: {
  int i = ++p->tos;
  int pgno;
  VERIFY( if( NeedStack(p, p->tos) ) goto no_mem; )
  assert( pOp->p3!=0 && pOp->p3type==P3_POINTER );
  if( pOp->opcode==OP_CreateTable ){
    rc = sqliteBtreeCreateTable(pOp->p2 ? db->pBeTemp : pBt, &pgno);
  }else{
    rc = sqliteBtreeCreateIndex(pOp->p2 ? db->pBeTemp : pBt, &pgno);
  }
  if( rc==SQLITE_OK ){
    aStack[i].i = pgno;
    aStack[i].flags = STK_Int;
    *(u32*)pOp->p3 = pgno;
    pOp->p3 = 0;
  }
  break;
}

/* Opcode: IntegrityCk P1 * *
**
** Do an analysis of the currently open database.  Push onto the
** stack the text of an error message describing any problems.
** If there are no errors, push a "ok" onto the stack.
**
** P1 is the index of a set that contains the root page numbers
** for all tables and indices in this database.
**
** This opcode is used for testing purposes only.
*/
case OP_IntegrityCk: {
  int nRoot;
  int *aRoot;
  int tos = ++p->tos;
  int iSet = pOp->p1;
  Set *pSet;
  int j;
  HashElem *i;
  char *z;

  VERIFY( if( iSet<0 || iSet>=p->nSet ) goto bad_instruction; )
  VERIFY( if( NeedStack(p, p->tos) ) goto no_mem; )
  pSet = &p->aSet[iSet];
  nRoot = sqliteHashCount(&pSet->hash);
  aRoot = sqliteMalloc( sizeof(int)*(nRoot+1) );
  for(j=0, i=sqliteHashFirst(&pSet->hash); i; i=sqliteHashNext(i), j++){
    aRoot[j] = atoi((char*)sqliteHashKey(i));
  }
  aRoot[j] = 0;
  z = sqliteBtreeIntegrityCheck(pBt, aRoot, nRoot);
  if( z==0 || z[0]==0 ){
    zStack[tos] = "ok";
    aStack[tos].n = 3;
    aStack[tos].flags = STK_Str | STK_Static;
    if( z ) sqliteFree(z);
  }else{
    zStack[tos] = z;
    aStack[tos].n = strlen(z) + 1;
    aStack[tos].flags = STK_Str | STK_Dyn;
  }
  sqliteFree(aRoot);
  break;
}

/* Opcode:  Limit P1 P2 *
**
** Set a limit and offset on callbacks.  P1 is the limit and P2 is
** the offset.  If the offset counter is positive, no callbacks are
** invoked but instead the counter is decremented.  Once the offset
** counter reaches zero, callbacks are invoked and the limit
** counter is decremented.  When the limit counter reaches zero,
** the OP_Callback or OP_SortCallback instruction executes a jump
** that should end the query.
**
** This opcode is used to implement the "LIMIT x OFFSET y" clause
** of a SELECT statement.
*/
case OP_Limit: {
  p->iLimit = pOp->p1;
  p->iOffset = pOp->p2;
  break;
}

/* Opcode: LimitCk P1 P2 *
**
** If P1 is 1, then check to see if the offset counter (set by the
** P2 argument of OP_Limit) is positive.  If the offset counter is
** positive then decrement the counter and jump immediately to P2.
** Otherwise fall straight through.
**
** If P1 is 0, then check the value of the limit counter (set by the
** P1 argument of OP_Limit).  If the limit counter is negative or
** zero then jump immedately to P2.  Otherwise decrement the limit
** counter and fall through.
*/
case OP_LimitCk: {
  if( pOp->p1 ){
    if( p->iOffset ){
      p->iOffset--;
      pc = pOp->p2 - 1;
    }
  }else{
    if( p->iLimit>0 ){
      p->iLimit--;
    }else{
      pc = pOp->p2 - 1;
    }
  }
  break;
}


/* Opcode: ListWrite * * *
**
** Write the integer on the top of the stack
** into the temporary storage list.
*/
case OP_ListWrite: {
  Keylist *pKeylist;
  VERIFY( if( p->tos<0 ) goto not_enough_stack; )
  pKeylist = p->pList;
  if( pKeylist==0 || pKeylist->nUsed>=pKeylist->nKey ){
    pKeylist = sqliteMalloc( sizeof(Keylist)+999*sizeof(pKeylist->aKey[0]) );
    if( pKeylist==0 ) goto no_mem;
    pKeylist->nKey = 1000;
    pKeylist->nRead = 0;
    pKeylist->nUsed = 0;
    pKeylist->pNext = p->pList;
    p->pList = pKeylist;
  }
  Integerify(p, p->tos);
  pKeylist->aKey[pKeylist->nUsed++] = aStack[p->tos].i;
  POPSTACK;
  break;
}

/* Opcode: ListRewind * * *
**
** Rewind the temporary buffer back to the beginning.
*/
case OP_ListRewind: {
  /* This is now a no-op */
  break;
}

/* Opcode: ListRead * P2 *
**
** Attempt to read an integer from the temporary storage buffer
** and push it onto the stack.  If the storage buffer is empty, 
** push nothing but instead jump to P2.
*/
case OP_ListRead: {
  Keylist *pKeylist;
  pKeylist = p->pList;
  if( pKeylist!=0 ){
    VERIFY(
      if( pKeylist->nRead<0 
        || pKeylist->nRead>=pKeylist->nUsed
        || pKeylist->nRead>=pKeylist->nKey ) goto bad_instruction;
    )
    p->tos++;
    if( NeedStack(p, p->tos) ) goto no_mem;
    aStack[p->tos].i = pKeylist->aKey[pKeylist->nRead++];
    aStack[p->tos].flags = STK_Int;
    zStack[p->tos] = 0;
    if( pKeylist->nRead>=pKeylist->nUsed ){
      p->pList = pKeylist->pNext;
      sqliteFree(pKeylist);
    }
  }else{
    pc = pOp->p2 - 1;
  }
  break;
}

/* Opcode: ListReset * * *
**
** Reset the temporary storage buffer so that it holds nothing.
*/
case OP_ListReset: {
  if( p->pList ){
    KeylistFree(p->pList);
    p->pList = 0;
  }
  break;
}

/* Opcode: ListPush * * * 
**
** Save the current Vdbe list such that it can be restored by a ListPop
** opcode. The list is empty after this is executed.
*/
case OP_ListPush: {
  p->keylistStackDepth++;
  assert(p->keylistStackDepth > 0);
  p->keylistStack = sqliteRealloc(p->keylistStack, 
          sizeof(Keylist *) * p->keylistStackDepth);
  p->keylistStack[p->keylistStackDepth - 1] = p->pList;
  p->pList = 0;
  break;
}

/* Opcode: ListPop * * * 
**
** Restore the Vdbe list to the state it was in when ListPush was last
** executed.
*/
case OP_ListPop: {
  assert(p->keylistStackDepth > 0);
  p->keylistStackDepth--;
  KeylistFree(p->pList);
  p->pList = p->keylistStack[p->keylistStackDepth];
  p->keylistStack[p->keylistStackDepth] = 0;
  if( p->keylistStackDepth == 0 ){
    sqliteFree(p->keylistStack);
    p->keylistStack = 0;
  }
  break;
}

/* Opcode: SortPut * * *
**
** The TOS is the key and the NOS is the data.  Pop both from the stack
** and put them on the sorter.  The key and data should have been
** made using SortMakeKey and SortMakeRec, respectively.
*/
case OP_SortPut: {
  int tos = p->tos;
  int nos = tos - 1;
  Sorter *pSorter;
  VERIFY( if( tos<1 ) goto not_enough_stack; )
  if( Stringify(p, tos) || Stringify(p, nos) ) goto no_mem;
  pSorter = sqliteMalloc( sizeof(Sorter) );
  if( pSorter==0 ) goto no_mem;
  pSorter->pNext = p->pSort;
  p->pSort = pSorter;
  assert( aStack[tos].flags & STK_Dyn );
  assert( aStack[nos].flags & STK_Dyn );
  pSorter->nKey = aStack[tos].n;
  pSorter->zKey = zStack[tos];
  pSorter->nData = aStack[nos].n;
  pSorter->pData = zStack[nos];
  aStack[tos].flags = 0;
  aStack[nos].flags = 0;
  zStack[tos] = 0;
  zStack[nos] = 0;
  p->tos -= 2;
  break;
}

/* Opcode: SortMakeRec P1 * *
**
** The top P1 elements are the arguments to a callback.  Form these
** elements into a single data entry that can be stored on a sorter
** using SortPut and later fed to a callback using SortCallback.
*/
case OP_SortMakeRec: {
  char *z;
  char **azArg;
  int nByte;
  int nField;
  int i, j;

  nField = pOp->p1;
  VERIFY( if( p->tos+1<nField ) goto not_enough_stack; )
  nByte = 0;
  for(i=p->tos-nField+1; i<=p->tos; i++){
    if( (aStack[i].flags & STK_Null)==0 ){
      if( Stringify(p, i) ) goto no_mem;
      nByte += aStack[i].n;
    }
  }
  nByte += sizeof(char*)*(nField+1);
  azArg = sqliteMalloc( nByte );
  if( azArg==0 ) goto no_mem;
  z = (char*)&azArg[nField+1];
  for(j=0, i=p->tos-nField+1; i<=p->tos; i++, j++){
    if( aStack[i].flags & STK_Null ){
      azArg[j] = 0;
    }else{
      azArg[j] = z;
      strcpy(z, zStack[i]);
      z += aStack[i].n;
    }
  }
  PopStack(p, nField);
  VERIFY( NeedStack(p, p->tos+1); )
  p->tos++;
  aStack[p->tos].n = nByte;
  zStack[p->tos] = (char*)azArg;
  aStack[p->tos].flags = STK_Str|STK_Dyn;
  break;
}

/* Opcode: SortMakeKey * * P3
**
** Convert the top few entries of the stack into a sort key.  The
** number of stack entries consumed is the number of characters in 
** the string P3.  One character from P3 is prepended to each entry.
** The first character of P3 is prepended to the element lowest in
** the stack and the last character of P3 is appended to the top of
** the stack.  All stack entries are separated by a \000 character
** in the result.  The whole key is terminated by two \000 characters
** in a row.
**
** See also the MakeKey and MakeIdxKey opcodes.
*/
case OP_SortMakeKey: {
  char *zNewKey;
  int nByte;
  int nField;
  int i, j, k;

  nField = strlen(pOp->p3);
  VERIFY( if( p->tos+1<nField ) goto not_enough_stack; )
  nByte = 1;
  for(i=p->tos-nField+1; i<=p->tos; i++){
    if( Stringify(p, i) ) goto no_mem;
    nByte += aStack[i].n+2;
  }
  zNewKey = sqliteMalloc( nByte );
  if( zNewKey==0 ) goto no_mem;
  j = 0;
  k = 0;
  for(i=p->tos-nField+1; i<=p->tos; i++){
    zNewKey[j++] = pOp->p3[k++];
    memcpy(&zNewKey[j], zStack[i], aStack[i].n-1);
    j += aStack[i].n-1;
    zNewKey[j++] = 0;
  }
  zNewKey[j] = 0;
  assert( j<nByte );
  PopStack(p, nField);
  VERIFY( NeedStack(p, p->tos+1); )
  p->tos++;
  aStack[p->tos].n = nByte;
  aStack[p->tos].flags = STK_Str|STK_Dyn;
  zStack[p->tos] = zNewKey;
  break;
}

/* Opcode: Sort * * *
**
** Sort all elements on the sorter.  The algorithm is a
** mergesort.
*/
case OP_Sort: {
  int i;
  Sorter *pElem;
  Sorter *apSorter[NSORT];
  for(i=0; i<NSORT; i++){
    apSorter[i] = 0;
  }
  while( p->pSort ){
    pElem = p->pSort;
    p->pSort = pElem->pNext;
    pElem->pNext = 0;
    for(i=0; i<NSORT-1; i++){
    if( apSorter[i]==0 ){
        apSorter[i] = pElem;
        break;
      }else{
        pElem = Merge(apSorter[i], pElem);
        apSorter[i] = 0;
      }
    }
    if( i>=NSORT-1 ){
      apSorter[NSORT-1] = Merge(apSorter[NSORT-1],pElem);
    }
  }
  pElem = 0;
  for(i=0; i<NSORT; i++){
    pElem = Merge(apSorter[i], pElem);
  }
  p->pSort = pElem;
  break;
}

/* Opcode: SortNext * P2 *
**
** Push the data for the topmost element in the sorter onto the
** stack, then remove the element from the sorter.  If the sorter
** is empty, push nothing on the stack and instead jump immediately 
** to instruction P2.
*/
case OP_SortNext: {
  Sorter *pSorter = p->pSort;
  if( pSorter!=0 ){
    p->pSort = pSorter->pNext;
    p->tos++;
    VERIFY( NeedStack(p, p->tos); )
    zStack[p->tos] = pSorter->pData;
    aStack[p->tos].n = pSorter->nData;
    aStack[p->tos].flags = STK_Str|STK_Dyn;
    sqliteFree(pSorter->zKey);
    sqliteFree(pSorter);
  }else{
    pc = pOp->p2 - 1;
  }
  break;
}

/* Opcode: SortCallback P1 * *
**
** The top of the stack contains a callback record built using
** the SortMakeRec operation with the same P1 value as this
** instruction.  Pop this record from the stack and invoke the
** callback on it.
*/
case OP_SortCallback: {
  int i = p->tos;
  VERIFY( if( i<0 ) goto not_enough_stack; )
  if( xCallback!=0 ){
    if( sqliteSafetyOff(db) ) goto abort_due_to_misuse;
    if( xCallback(pArg, pOp->p1, (char**)zStack[i], p->azColName)!=0 ){
      rc = SQLITE_ABORT;
    }
    if( sqliteSafetyOn(db) ) goto abort_due_to_misuse;
    p->nCallback++;
  }
  POPSTACK;
  if( sqlite_malloc_failed ) goto no_mem;
  break;
}

/* Opcode: SortReset * * *
**
** Remove any elements that remain on the sorter.
*/
case OP_SortReset: {
  SorterReset(p);
  break;
}

/* Opcode: FileOpen * * P3
**
** Open the file named by P3 for reading using the FileRead opcode.
** If P3 is "stdin" then open standard input for reading.
*/
case OP_FileOpen: {
  VERIFY( if( pOp->p3==0 ) goto bad_instruction; )
  if( p->pFile ){
    if( p->pFile!=stdin ) fclose(p->pFile);
    p->pFile = 0;
  }
  if( sqliteStrICmp(pOp->p3,"stdin")==0 ){
    p->pFile = stdin;
  }else{
    p->pFile = fopen(pOp->p3, "r");
  }
  if( p->pFile==0 ){
    sqliteSetString(pzErrMsg,"unable to open file: ", pOp->p3, 0);
    rc = SQLITE_ERROR;
    goto cleanup;
  }
  break;
}

/* Opcode: FileRead P1 P2 P3
**
** Read a single line of input from the open file (the file opened using
** FileOpen).  If we reach end-of-file, jump immediately to P2.  If
** we are able to get another line, split the line apart using P3 as
** a delimiter.  There should be P1 fields.  If the input line contains
** more than P1 fields, ignore the excess.  If the input line contains
** fewer than P1 fields, assume the remaining fields contain NULLs.
**
** Input ends if a line consists of just "\.".  A field containing only
** "\N" is a null field.  The backslash \ character can be used be used
** to escape newlines or the delimiter.
*/
case OP_FileRead: {
  int n, eol, nField, i, c, nDelim;
  char *zDelim, *z;
  if( p->pFile==0 ) goto fileread_jump;
  nField = pOp->p1;
  if( nField<=0 ) goto fileread_jump;
  if( nField!=p->nField || p->azField==0 ){
    char **azField = sqliteRealloc(p->azField, sizeof(char*)*nField+1);
    if( azField==0 ){ goto no_mem; }
    p->azField = azField;
    p->nField = nField;
  }
  n = 0;
  eol = 0;
  while( eol==0 ){
    if( p->zLine==0 || n+200>p->nLineAlloc ){
      char *zLine;
      p->nLineAlloc = p->nLineAlloc*2 + 300;
      zLine = sqliteRealloc(p->zLine, p->nLineAlloc);
      if( zLine==0 ){
        p->nLineAlloc = 0;
        sqliteFree(p->zLine);
        p->zLine = 0;
        goto no_mem;
      }
      p->zLine = zLine;
    }
    if( fgets(&p->zLine[n], p->nLineAlloc-n, p->pFile)==0 ){
      eol = 1;
      p->zLine[n] = 0;
    }else{
      int c;
      while( (c = p->zLine[n])!=0 ){
        if( c=='\\' ){
          if( p->zLine[n+1]==0 ) break;
          n += 2;
        }else if( c=='\n' ){
          p->zLine[n] = 0;
          eol = 1;
          break;
        }else{
          n++;
        }
      }
    }
  }
  if( n==0 ) goto fileread_jump;
  z = p->zLine;
  if( z[0]=='\\' && z[1]=='.' && z[2]==0 ){
    goto fileread_jump;
  }
  zDelim = pOp->p3;
  if( zDelim==0 ) zDelim = "\t";
  c = zDelim[0];
  nDelim = strlen(zDelim);
  p->azField[0] = z;
  for(i=1; *z!=0 && i<=nField; i++){
    int from, to;
    from = to = 0;
    if( z[0]=='\\' && z[1]=='N' 
       && (z[2]==0 || strncmp(&z[2],zDelim,nDelim)==0) ){
      if( i<=nField ) p->azField[i-1] = 0;
      z += 2 + nDelim;
      if( i<nField ) p->azField[i] = z;
      continue;
    }
    while( z[from] ){
      if( z[from]=='\\' && z[from+1]!=0 ){
        z[to++] = z[from+1];
        from += 2;
        continue;
      }
      if( z[from]==c && strncmp(&z[from],zDelim,nDelim)==0 ) break;
      z[to++] = z[from++];
    }
    if( z[from] ){
      z[to] = 0;
      z += from + nDelim;
      if( i<nField ) p->azField[i] = z;
    }else{
      z[to] = 0;
      z = "";
    }
  }
  while( i<nField ){
    p->azField[i++] = 0;
  }
  break;

  /* If we reach end-of-file, or if anything goes wrong, jump here.
  ** This code will cause a jump to P2 */
fileread_jump:
  pc = pOp->p2 - 1;
  break;
}

/* Opcode: FileColumn P1 * *
**
** Push onto the stack the P1-th column of the most recently read line
** from the input file.
*/
case OP_FileColumn: {
  int i = pOp->p1;
  char *z;
  VERIFY( if( NeedStack(p, p->tos+1) ) goto no_mem; )
  if( VERIFY( i>=0 && i<p->nField && ) p->azField ){
    z = p->azField[i];
  }else{
    z = 0;
  }
  p->tos++;
  if( z ){
    aStack[p->tos].n = strlen(z) + 1;
    zStack[p->tos] = z;
    aStack[p->tos].flags = STK_Str;
  }else{
    aStack[p->tos].n = 0;
    zStack[p->tos] = 0;
    aStack[p->tos].flags = STK_Null;
  }
  break;
}

/* Opcode: MemStore P1 P2 *
**
** Write the top of the stack into memory location P1.
** P1 should be a small integer since space is allocated
** for all memory locations between 0 and P1 inclusive.
**
** After the data is stored in the memory location, the
** stack is popped once if P2 is 1.  If P2 is zero, then
** the original data remains on the stack.
*/
case OP_MemStore: {
  int i = pOp->p1;
  int tos = p->tos;
  char *zOld;
  Mem *pMem;
  VERIFY( if( tos<0 ) goto not_enough_stack; )
  if( i>=p->nMem ){
    int nOld = p->nMem;
    Mem *aMem;
    p->nMem = i + 5;
    aMem = sqliteRealloc(p->aMem, p->nMem*sizeof(p->aMem[0]));
    if( aMem==0 ) goto no_mem;
    p->aMem = aMem;
    if( nOld<p->nMem ){
      memset(&p->aMem[nOld], 0, sizeof(p->aMem[0])*(p->nMem-nOld));
    }
  }
  pMem = &p->aMem[i];
  if( pMem->s.flags & STK_Dyn ){
    zOld = pMem->z;
  }else{
    zOld = 0;
  }
  pMem->s = aStack[tos];
  if( pMem->s.flags & (STK_Static|STK_Dyn) ){
    if( pOp->p2==0 && (pMem->s.flags & STK_Dyn)!=0 ){
      pMem->z = sqliteMalloc( pMem->s.n );
      if( pMem->z==0 ) goto no_mem;
      memcpy(pMem->z, zStack[tos], pMem->s.n);
    }else{
      pMem->z = zStack[tos];
    }
  }else{
    pMem->z = pMem->s.z;
  }
  if( zOld ) sqliteFree(zOld);
  if( pOp->p2 ){
    zStack[tos] = 0;
    aStack[tos].flags = 0;
    POPSTACK;
  }
  break;
}

/* Opcode: MemLoad P1 * *
**
** Push a copy of the value in memory location P1 onto the stack.
**
** If the value is a string, then the value pushed is a pointer to
** the string that is stored in the memory location.  If the memory
** location is subsequently changed (using OP_MemStore) then the
** value pushed onto the stack will change too.
*/
case OP_MemLoad: {
  int tos = ++p->tos;
  int i = pOp->p1;
  VERIFY( if( NeedStack(p, tos) ) goto no_mem; )
  VERIFY( if( i<0 || i>=p->nMem ) goto bad_instruction; )
  memcpy(&aStack[tos], &p->aMem[i].s, sizeof(aStack[tos])-NBFS);;
  if( aStack[tos].flags & STK_Str ){
    zStack[tos] = p->aMem[i].z;
    aStack[tos].flags |= STK_Static;
    aStack[tos].flags &= ~STK_Dyn;
  }
  break;
}

/* Opcode: AggReset * P2 *
**
** Reset the aggregator so that it no longer contains any data.
** Future aggregator elements will contain P2 values each.
*/
case OP_AggReset: {
  AggReset(&p->agg);
  p->agg.nMem = pOp->p2;
  p->agg.apFunc = sqliteMalloc( p->agg.nMem*sizeof(p->agg.apFunc[0]) );
  break;
}

/* Opcode: AggInit * P2 P3
**
** Initialize the function parameters for an aggregate function.
** The aggregate will operate out of aggregate column P2.
** P3 is a pointer to the FuncDef structure for the function.
*/
case OP_AggInit: {
  int i = pOp->p2;
  VERIFY( if( i<0 || i>=p->agg.nMem ) goto bad_instruction; )
  p->agg.apFunc[i] = (FuncDef*)pOp->p3;
  break;
}

/* Opcode: AggFunc * P2 P3
**
** Execute the step function for an aggregate.  The
** function has P2 arguments.  P3 is a pointer to the FuncDef
** structure that specifies the function.
**
** The top of the stack must be an integer which is the index of
** the aggregate column that corresponds to this aggregate function.
** Ideally, this index would be another parameter, but there are
** no free parameters left.  The integer is popped from the stack.
*/
case OP_AggFunc: {
  int n = pOp->p2;
  int i;
  Mem *pMem;
  sqlite_func ctx;

  VERIFY( if( n<0 ) goto bad_instruction; )
  VERIFY( if( p->tos+1<n ) goto not_enough_stack; )
  VERIFY( if( aStack[p->tos].flags!=STK_Int ) goto bad_instruction; )
  for(i=p->tos-n; i<p->tos; i++){
    if( aStack[i].flags & STK_Null ){
      zStack[i] = 0;
    }else{
      if( Stringify(p, i) ) goto no_mem;
    }
  }
  i = aStack[p->tos].i;
  VERIFY( if( i<0 || i>=p->agg.nMem ) goto bad_instruction; )
  ctx.pFunc = (FuncDef*)pOp->p3;
  pMem = &p->agg.pCurrent->aMem[i];
  ctx.z = pMem->s.z;
  ctx.pAgg = pMem->z;
  ctx.cnt = ++pMem->s.i;
  ctx.isError = 0;
  ctx.isStep = 1;
  (ctx.pFunc->xStep)(&ctx, n, (const char**)&zStack[p->tos-n]);
  pMem->z = ctx.pAgg;
  pMem->s.flags = STK_AggCtx;
  PopStack(p, n+1);
  if( ctx.isError ){
    rc = SQLITE_ERROR;
  }
  break;
}

/* Opcode: AggFocus * P2 *
**
** Pop the top of the stack and use that as an aggregator key.  If
** an aggregator with that same key already exists, then make the
** aggregator the current aggregator and jump to P2.  If no aggregator
** with the given key exists, create one and make it current but
** do not jump.
**
** The order of aggregator opcodes is important.  The order is:
** AggReset AggFocus AggNext.  In other words, you must execute
** AggReset first, then zero or more AggFocus operations, then
** zero or more AggNext operations.  You must not execute an AggFocus
** in between an AggNext and an AggReset.
*/
case OP_AggFocus: {
  int tos = p->tos;
  AggElem *pElem;
  char *zKey;
  int nKey;

  VERIFY( if( tos<0 ) goto not_enough_stack; )
  if( Stringify(p, tos) ) goto no_mem;
  zKey = zStack[tos]; 
  nKey = aStack[tos].n;
  pElem = sqliteHashFind(&p->agg.hash, zKey, nKey);
  if( pElem ){
    p->agg.pCurrent = pElem;
    pc = pOp->p2 - 1;
  }else{
    AggInsert(&p->agg, zKey, nKey);
    if( sqlite_malloc_failed ) goto no_mem;
  }
  POPSTACK;
  break; 
}

/* Opcode: AggSet * P2 *
**
** Move the top of the stack into the P2-th field of the current
** aggregate.  String values are duplicated into new memory.
*/
case OP_AggSet: {
  AggElem *pFocus = AggInFocus(p->agg);
  int i = pOp->p2;
  int tos = p->tos;
  VERIFY( if( tos<0 ) goto not_enough_stack; )
  if( pFocus==0 ) goto no_mem;
  if( VERIFY( i>=0 && ) i<p->agg.nMem ){
    Mem *pMem = &pFocus->aMem[i];
    char *zOld;
    if( pMem->s.flags & STK_Dyn ){
      zOld = pMem->z;
    }else{
      zOld = 0;
    }
    pMem->s = aStack[tos];
    if( pMem->s.flags & STK_Dyn ){
      pMem->z = zStack[tos];
      zStack[tos] = 0;
      aStack[tos].flags = 0;
    }else if( pMem->s.flags & (STK_Static|STK_AggCtx) ){
      pMem->z = zStack[tos];
    }else if( pMem->s.flags & STK_Str ){
      pMem->z = pMem->s.z;
    }
    if( zOld ) sqliteFree(zOld);
  }
  POPSTACK;
  break;
}

/* Opcode: AggGet * P2 *
**
** Push a new entry onto the stack which is a copy of the P2-th field
** of the current aggregate.  Strings are not duplicated so
** string values will be ephemeral.
*/
case OP_AggGet: {
  AggElem *pFocus = AggInFocus(p->agg);
  int i = pOp->p2;
  int tos = ++p->tos;
  VERIFY( if( NeedStack(p, tos) ) goto no_mem; )
  if( pFocus==0 ) goto no_mem;
  if( VERIFY( i>=0 && ) i<p->agg.nMem ){
    Mem *pMem = &pFocus->aMem[i];
    aStack[tos] = pMem->s;
    zStack[tos] = pMem->z;
    aStack[tos].flags &= ~STK_Dyn;
  }
  break;
}

/* Opcode: AggNext * P2 *
**
** Make the next aggregate value the current aggregate.  The prior
** aggregate is deleted.  If all aggregate values have been consumed,
** jump to P2.
**
** The order of aggregator opcodes is important.  The order is:
** AggReset AggFocus AggNext.  In other words, you must execute
** AggReset first, then zero or more AggFocus operations, then
** zero or more AggNext operations.  You must not execute an AggFocus
** in between an AggNext and an AggReset.
*/
case OP_AggNext: {
  if( p->agg.pSearch==0 ){
    p->agg.pSearch = sqliteHashFirst(&p->agg.hash);
  }else{
    p->agg.pSearch = sqliteHashNext(p->agg.pSearch);
  }
  if( p->agg.pSearch==0 ){
    pc = pOp->p2 - 1;
  } else {
    int i;
    sqlite_func ctx;
    Mem *aMem;
    int nErr = 0;
    p->agg.pCurrent = sqliteHashData(p->agg.pSearch);
    aMem = p->agg.pCurrent->aMem;
    for(i=0; i<p->agg.nMem; i++){
      int freeCtx;
      if( p->agg.apFunc[i]==0 ) continue;
      if( p->agg.apFunc[i]->xFinalize==0 ) continue;
      ctx.s.flags = STK_Null;
      ctx.z = 0;
      ctx.pAgg = (void*)aMem[i].z;
      freeCtx = aMem[i].z && aMem[i].z!=aMem[i].s.z;
      ctx.cnt = aMem[i].s.i;
      ctx.isStep = 0;
      ctx.pFunc = p->agg.apFunc[i];
      (*p->agg.apFunc[i]->xFinalize)(&ctx);
      if( freeCtx ){
        sqliteFree( aMem[i].z );
      }
      aMem[i].s = ctx.s;
      aMem[i].z = ctx.z;
      if( (aMem[i].s.flags & STK_Str) &&
              (aMem[i].s.flags & (STK_Dyn|STK_Static))==0 ){
        aMem[i].z = aMem[i].s.z;
      }
      nErr += ctx.isError;
    }
  }
  break;
}

/* Opcode: SetInsert P1 * P3
**
** If Set P1 does not exist then create it.  Then insert value
** P3 into that set.  If P3 is NULL, then insert the top of the
** stack into the set.
*/
case OP_SetInsert: {
  int i = pOp->p1;
  if( p->nSet<=i ){
    int k;
    Set *aSet = sqliteRealloc(p->aSet, (i+1)*sizeof(p->aSet[0]) );
    if( aSet==0 ) goto no_mem;
    p->aSet = aSet;
    for(k=p->nSet; k<=i; k++){
      sqliteHashInit(&p->aSet[k].hash, SQLITE_HASH_BINARY, 1);
    }
    p->nSet = i+1;
  }
  if( pOp->p3 ){
    sqliteHashInsert(&p->aSet[i].hash, pOp->p3, strlen(pOp->p3)+1, p);
  }else{
    int tos = p->tos;
    if( tos<0 ) goto not_enough_stack;
    if( Stringify(p, tos) ) goto no_mem;
    sqliteHashInsert(&p->aSet[i].hash, zStack[tos], aStack[tos].n, p);
    POPSTACK;
  }
  if( sqlite_malloc_failed ) goto no_mem;
  break;
}

/* Opcode: SetFound P1 P2 *
**
** Pop the stack once and compare the value popped off with the
** contents of set P1.  If the element popped exists in set P1,
** then jump to P2.  Otherwise fall through.
*/
case OP_SetFound: {
  int i = pOp->p1;
  int tos = p->tos;
  VERIFY( if( tos<0 ) goto not_enough_stack; )
  if( Stringify(p, tos) ) goto no_mem;
  if( VERIFY( i>=0 && i<p->nSet &&) 
       sqliteHashFind(&p->aSet[i].hash, zStack[tos], aStack[tos].n)){
    pc = pOp->p2 - 1;
  }
  POPSTACK;
  break;
}

/* Opcode: SetNotFound P1 P2 *
**
** Pop the stack once and compare the value popped off with the
** contents of set P1.  If the element popped does not exists in 
** set P1, then jump to P2.  Otherwise fall through.
*/
case OP_SetNotFound: {
  int i = pOp->p1;
  int tos = p->tos;
  VERIFY( if( tos<0 ) goto not_enough_stack; )
  if( Stringify(p, tos) ) goto no_mem;
  if(VERIFY( i>=0 && i<p->nSet &&)
       sqliteHashFind(&p->aSet[i].hash, zStack[tos], aStack[tos].n)==0 ){
    pc = pOp->p2 - 1;
  }
  POPSTACK;
  break;
}

/* Opcode: SetFirst P1 P2 *
**
** Read the first element from set P1 and push it onto the stack.  If the
** set is empty, push nothing and jump immediately to P2.  This opcode is
** used in combination with OP_SetNext to loop over all elements of a set.
*/
/* Opcode: SetNext P1 P2 *
**
** Read the next element from set P1 and push it onto the stack.  If there
** are no more elements in the set, do not do the push and fall through.
** Otherwise, jump to P2 after pushing the next set element.
*/
case OP_SetFirst: 
case OP_SetNext: {
  Set *pSet;
  int tos;
  VERIFY( if( pOp->p1<0 || pOp->p1>=p->nSet ) goto bad_instruction; )
  pSet = &p->aSet[pOp->p1];
  if( pOp->opcode==OP_SetFirst ){
    pSet->prev = sqliteHashFirst(&pSet->hash);
    if( pSet->prev==0 ){
      pc = pOp->p2 - 1;
      break;
    }
  }else{
    VERIFY( if( pSet->prev==0 ) goto bad_instruction; )
    pSet->prev = sqliteHashNext(pSet->prev);
    if( pSet->prev==0 ){
      break;
    }else{
      pc = pOp->p2 - 1;
    }
  }
  tos = ++p->tos;
  VERIFY( if( NeedStack(p, p->tos) ) goto no_mem; )
  zStack[tos] = sqliteHashKey(pSet->prev);
  aStack[tos].n = sqliteHashKeysize(pSet->prev);
  aStack[tos].flags = STK_Str | STK_Static;
  break;
}

/* An other opcode is illegal...
*/
default: {
  sprintf(zBuf,"%d",pOp->opcode);
  sqliteSetString(pzErrMsg, "unknown opcode ", zBuf, 0);
  rc = SQLITE_INTERNAL;
  break;
}

/*****************************************************************************
** The cases of the switch statement above this line should all be indented
** by 6 spaces.  But the left-most 6 spaces have been removed to improve the
** readability.  From this point on down, the normal indentation rules are
** restored.
*****************************************************************************/
    }

    /* The following code adds nothing to the actual functionality
    ** of the program.  It is only here for testing and debugging.
    ** On the other hand, it does burn CPU cycles every time through
    ** the evaluator loop.  So we can leave it out when NDEBUG is defined.
    */
#ifndef NDEBUG
    if( pc<-1 || pc>=p->nOp ){
      sqliteSetString(pzErrMsg, "jump destination out of range", 0);
      rc = SQLITE_INTERNAL;
    }
    if( p->trace && p->tos>=0 ){
      int i;
      fprintf(p->trace, "Stack:");
      for(i=p->tos; i>=0 && i>p->tos-5; i--){
        if( aStack[i].flags & STK_Null ){
          fprintf(p->trace, " NULL");
        }else if( (aStack[i].flags & (STK_Int|STK_Str))==(STK_Int|STK_Str) ){
          fprintf(p->trace, " si:%d", aStack[i].i);
        }else if( aStack[i].flags & STK_Int ){
          fprintf(p->trace, " i:%d", aStack[i].i);
        }else if( aStack[i].flags & STK_Real ){
          fprintf(p->trace, " r:%g", aStack[i].r);
        }else if( aStack[i].flags & STK_Str ){
          int j, k;
          char zBuf[100];
          zBuf[0] = ' ';
          if( aStack[i].flags & STK_Dyn ){
            zBuf[1] = 'z';
          }else if( aStack[i].flags & STK_Static ){
            zBuf[1] = 't';
          }else{
            zBuf[1] = 's';
          }
          zBuf[2] = '[';
          k = 3;
          for(j=0; j<20 && j<aStack[i].n; j++){
            int c = zStack[i][j];
            if( c==0 && j==aStack[i].n-1 ) break;
            if( isprint(c) && !isspace(c) ){
              zBuf[k++] = c;
            }else{
              zBuf[k++] = '.';
            }
          }
          zBuf[k++] = ']';
          zBuf[k++] = 0;
          fprintf(p->trace, "%s", zBuf);
        }else{
          fprintf(p->trace, " ???");
        }
      }
      fprintf(p->trace,"\n");
    }
#endif
  }

cleanup:
  Cleanup(p);
  if( rc!=SQLITE_OK ){
    switch( errorAction ){
      case OE_Abort: {
        if( !undoTransOnError ){
          sqliteBtreeRollbackCkpt(pBt);
          if( db->pBeTemp ) sqliteBtreeRollbackCkpt(db->pBeTemp);
          break;
        }
        /* Fall through to ROLLBACK */
      }
      case OE_Rollback: {
        sqliteBtreeRollback(pBt);
        if( db->pBeTemp ) sqliteBtreeRollback(db->pBeTemp);
        db->flags &= ~SQLITE_InTrans;
        db->onError = OE_Default;
        break;
      }
      default: {
        if( undoTransOnError ){
          sqliteBtreeCommit(pBt);
          if( db->pBeTemp ) sqliteBtreeCommit(db->pBeTemp);
          db->flags &= ~SQLITE_InTrans;
          db->onError = OE_Default;
        }
        break;
      }
    }
    sqliteRollbackInternalChanges(db);
  }
  sqliteBtreeCommitCkpt(pBt);
  if( db->pBeTemp ) sqliteBtreeCommitCkpt(db->pBeTemp);
  assert( p->tos<pc );
  return rc;

  /* Jump to here if a malloc() fails.  It's hard to get a malloc()
  ** to fail on a modern VM computer, so this code is untested.
  */
no_mem:
  sqliteSetString(pzErrMsg, "out of memory", 0);
  rc = SQLITE_NOMEM;
  goto cleanup;

  /* Jump to here for an SQLITE_MISUSE error.
  */
abort_due_to_misuse:
  rc = SQLITE_MISUSE;
  /* Fall thru into abort_due_to_error */

  /* Jump to here for any other kind of fatal error.  The "rc" variable
  ** should hold the error number.
  */
abort_due_to_error:
  sqliteSetString(pzErrMsg, sqlite_error_string(rc), 0);
  goto cleanup;

  /* Jump to here if a operator is encountered that requires more stack
  ** operands than are currently available on the stack.
  */
not_enough_stack:
  sprintf(zBuf,"%d",pc);
  sqliteSetString(pzErrMsg, "too few operands on stack at ", zBuf, 0);
  rc = SQLITE_INTERNAL;
  goto cleanup;

  /* Jump here if an illegal or illformed instruction is executed.
  */
VERIFY(
bad_instruction:
  sprintf(zBuf,"%d",pc);
  sqliteSetString(pzErrMsg, "illegal operation at ", zBuf, 0);
  rc = SQLITE_INTERNAL;
  goto cleanup;
)
}
