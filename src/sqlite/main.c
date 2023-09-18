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
** Main file for the SQLite library.  The routines in this file
** implement the programmer interface to the library.  Routines in
** other files are for internal use by SQLite and should not be
** accessed by users of the library.
**
** $Id: main.c,v 1.80 2002/06/16 18:21:44 drh Exp $
*/
#include "sqliteInt.h"
#include "os.h"
#include <ctype.h>

/*
** This is the callback routine for the code that initializes the
** database.  See sqliteInit() below for additional information.
**
** Each callback contains the following information:
**
**     argv[0] = "file-format" or "schema-cookie" or "table" or "index"
**     argv[1] = table or index name or meta statement type.
**     argv[2] = root page number for table or index.  NULL for meta.
**     argv[3] = SQL create statement for the table or index
**
*/
int sqliteInitCallback(void *pDb, int argc, char **argv, char **azColName){
  sqlite *db = (sqlite*)pDb;
  Parse sParse;
  int nErr = 0;

  /* TODO: Do some validity checks on all fields.  In particular,
  ** make sure fields do not contain NULLs. Otherwise we might core
  ** when attempting to initialize from a corrupt database file. */

  assert( argc==4 );
  switch( argv[0][0] ){
    case 'c': {  /* Recommended pager cache size */
      int size = atoi(argv[3]);
      if( size==0 ){ size = MAX_PAGES; }
      db->cache_size = size;
      sqliteBtreeSetCacheSize(db->pBe, size);
      break;
    }
    case 'f': {  /* File format */
      db->file_format = atoi(argv[3]);
      break;
    }
    case 's': { /* Schema cookie */
      db->schema_cookie = atoi(argv[3]);
      db->next_cookie = db->schema_cookie;
      break;
    }
    case 'v':
    case 'i':
    case 't': {  /* CREATE TABLE, CREATE INDEX, or CREATE VIEW statements */
      if( argv[3] && argv[3][0] ){
        /* Call the parser to process a CREATE TABLE, INDEX or VIEW.
        ** But because sParse.initFlag is set to 1, no VDBE code is generated
        ** or executed.  All the parser does is build the internal data
        ** structures that describe the table, index, or view.
        */
        memset(&sParse, 0, sizeof(sParse));
        sParse.db = db;
        sParse.initFlag = 1;
        sParse.newTnum = atoi(argv[2]);
        sqliteRunParser(&sParse, argv[3], 0);
      }else{
        /* If the SQL column is blank it means this is an index that
        ** was created to be the PRIMARY KEY or to fulfill a UNIQUE
        ** constraint for a CREATE TABLE.  The index should have already
        ** been created when we processed the CREATE TABLE.  All we have
        ** to do here is record the root page number for that index.
        */
        Index *pIndex = sqliteFindIndex(db, argv[1]);
        if( pIndex==0 || pIndex->tnum!=0 ){
          /* This can occur if there exists an index on a TEMP table which
          ** has the same name as another index on a permanent index.  Since
          ** the permanent table is hidden by the TEMP table, we can also
          ** safely ignore the index on the permanent table.
          */
          /* Do Nothing */;
        }else{
          pIndex->tnum = atoi(argv[2]);
        }
      }
      break;
    }
    default: {
      /* This can not happen! */
      nErr = 1;
      assert( nErr==0 );
    }
  }
  return nErr;
}

/*
** Attempt to read the database schema and initialize internal
** data structures.  Return one of the SQLITE_ error codes to
** indicate success or failure.
**
** After the database is initialized, the SQLITE_Initialized
** bit is set in the flags field of the sqlite structure.  An
** attempt is made to initialize the database as soon as it
** is opened.  If that fails (perhaps because another process
** has the sqlite_master table locked) than another attempt
** is made the first time the database is accessed.
*/
static int sqliteInit(sqlite *db, char **pzErrMsg){
  Vdbe *vdbe;
  int rc;

  /*
  ** The master database table has a structure like this
  */
  static char master_schema[] = 
     "CREATE TABLE " MASTER_NAME " (\n"
     "  type text,\n"
     "  name text,\n"
     "  tbl_name text,\n"
     "  rootpage integer,\n"
     "  sql text\n"
     ")"
  ;

  /* The following VDBE program is used to initialize the internal
  ** structure holding the tables and indexes of the database.
  ** The database contains a special table named "sqlite_master"
  ** defined as follows:
  **
  **    CREATE TABLE sqlite_master (
  **        type       text,    --  Either "table" or "index" or "meta"
  **        name       text,    --  Name of table or index
  **        tbl_name   text,    --  Associated table 
  **        rootpage   integer, --  The integer page number of root page
  **        sql        text     --  The CREATE statement for this object
  **    );
  **
  ** The sqlite_master table contains a single entry for each table
  ** and each index.  The "type" column tells whether the entry is
  ** a table or index.  The "name" column is the name of the object.
  ** The "tbl_name" is the name of the associated table.  For tables,
  ** the tbl_name column is always the same as name.  For indices, the
  ** tbl_name column contains the name of the table that the index
  ** indexes.  The "rootpage" column holds the number of the root page
  ** for the b-tree for the table or index.  Finally, the "sql" column
  ** contains the complete text of the CREATE TABLE or CREATE INDEX
  ** statement that originally created the table or index.  If an index
  ** was created to fulfill a PRIMARY KEY or UNIQUE constraint on a table,
  ** then the "sql" column is NULL.
  **
  ** In format 1, entries in the sqlite_master table are in a random
  ** order.  Two passes must be made through the table to initialize
  ** internal data structures.  The first pass reads table definitions
  ** and the second pass read index definitions.  Having two passes
  ** insures that indices appear after their tables.
  **
  ** In format 2, entries appear in chronological order.  Only a single
  ** pass needs to be made through the table since everything will be
  ** in the write order.  VIEWs may only occur in format 2.
  **
  ** The following program invokes its callback on the SQL for each
  ** table then goes back and invokes the callback on the
  ** SQL for each index.  The callback will invoke the
  ** parser to build the internal representation of the
  ** database scheme.
  */
  static VdbeOp initProg[] = {
    /* Send the file format to the callback routine
    */
    { OP_Open,       0, 2,  0},
    { OP_String,     0, 0,  "file-format"},
    { OP_String,     0, 0,  0},
    { OP_String,     0, 0,  0},
    { OP_ReadCookie, 0, 1,  0},
    { OP_Callback,   4, 0,  0},

    /* Send the recommended pager cache size to the callback routine
    */
    { OP_String,     0, 0,  "cache-size"},
    { OP_String,     0, 0,  0},
    { OP_String,     0, 0,  0},
    { OP_ReadCookie, 0, 2,  0},
    { OP_Callback,   4, 0,  0},

    /* Send the initial schema cookie to the callback
    */
    { OP_String,     0, 0,  "schema_cookie"},
    { OP_String,     0, 0,  0},
    { OP_String,     0, 0,  0},
    { OP_ReadCookie, 0, 0,  0},
    { OP_Callback,   4, 0,  0},

    /* Check the file format.  If the format number is 2 or more,
    ** then do a single pass through the SQLITE_MASTER table.  For
    ** a format number of less than 2, jump forward to a different
    ** algorithm that makes two passes through the SQLITE_MASTER table,
    ** once for tables and a second time for indices.
    */
    { OP_ReadCookie, 0, 1,  0},
    { OP_Integer,    2, 0,  0},
    { OP_Lt,         0, 28, 0},

    /* This is the code for doing a single scan through the SQLITE_MASTER
    ** table.  This code runs for format 2 and greater.
    */
    { OP_Rewind,     0, 26, 0},
    { OP_Column,     0, 0,  0},           /* 20 */
    { OP_Column,     0, 1,  0},
    { OP_Column,     0, 3,  0},
    { OP_Column,     0, 4,  0},
    { OP_Callback,   4, 0,  0},
    { OP_Next,       0, 20, 0},
    { OP_Close,      0, 0,  0},           /* 26 */
    { OP_Halt,       0, 0,  0},

    /* This is the code for doing two passes through SQLITE_MASTER.  This
    ** code runs for file format 1.
    */
    { OP_Rewind,     0, 48, 0},           /* 28 */
    { OP_Column,     0, 0,  0},           /* 29 */
    { OP_String,     0, 0,  "table"},
    { OP_Ne,         0, 37, 0},
    { OP_Column,     0, 0,  0},
    { OP_Column,     0, 1,  0},
    { OP_Column,     0, 3,  0},
    { OP_Column,     0, 4,  0},
    { OP_Callback,   4, 0,  0},
    { OP_Next,       0, 29, 0},           /* 37 */
    { OP_Rewind,     0, 48, 0},           /* 38 */
    { OP_Column,     0, 0,  0},           /* 39 */
    { OP_String,     0, 0,  "index"},
    { OP_Ne,         0, 47, 0},
    { OP_Column,     0, 0,  0},
    { OP_Column,     0, 1,  0},
    { OP_Column,     0, 3,  0},
    { OP_Column,     0, 4,  0},
    { OP_Callback,   4, 0,  0},
    { OP_Next,       0, 39, 0},           /* 47 */
    { OP_Close,      0, 0,  0},           /* 48 */
    { OP_Halt,       0, 0,  0},
  };

  /* Create a virtual machine to run the initialization program.  Run
  ** the program.  Then delete the virtual machine.
  */
  vdbe = sqliteVdbeCreate(db);
  if( vdbe==0 ){
    sqliteSetString(pzErrMsg, "out of memory", 0);
    return SQLITE_NOMEM;
  }
  sqliteVdbeAddOpList(vdbe, sizeof(initProg)/sizeof(initProg[0]), initProg);
  rc = sqliteVdbeExec(vdbe, sqliteInitCallback, db, pzErrMsg, 
                      db->pBusyArg, db->xBusyCallback);
  sqliteVdbeDelete(vdbe);
  if( rc==SQLITE_OK && db->nTable==0 ){
    db->file_format = 2;
  }
  if( rc==SQLITE_OK && db->file_format>2 ){
    sqliteSetString(pzErrMsg, "unsupported file format", 0);
    rc = SQLITE_ERROR;
  }

  /* The schema for the SQLITE_MASTER table is not stored in the
  ** database itself.  We have to invoke the callback one extra
  ** time to get it to process the SQLITE_MASTER table defintion.
  */
  if( rc==SQLITE_OK ){
    Table *pTab;
    char *azArg[6];
    azArg[0] = "table";
    azArg[1] = MASTER_NAME;
    azArg[2] = "2";
    azArg[3] = master_schema;
    azArg[4] = 0;
    sqliteInitCallback(db, 4, azArg, 0);
    pTab = sqliteFindTable(db, MASTER_NAME);
    if( pTab ){
      pTab->readOnly = 1;
    }
    db->flags |= SQLITE_Initialized;
    sqliteCommitInternalChanges(db);
  }
  return rc;
}

/*
** The version of the library
*/
const char sqlite_version[] = SQLITE_VERSION;

/*
** Does the library expect data to be encoded as UTF-8 or iso8859?  The
** following global constant always lets us know.
*/
#ifdef SQLITE_UTF8
const char sqlite_encoding[] = "UTF-8";
#else
const char sqlite_encoding[] = "iso8859";
#endif

/*
** Open a new SQLite database.  Construct an "sqlite" structure to define
** the state of this database and return a pointer to that structure.
**
** An attempt is made to initialize the in-memory data structures that
** hold the database schema.  But if this fails (because the schema file
** is locked) then that step is deferred until the first call to
** sqlite_exec().
*/
sqlite *sqlite_open(const char *zFilename, int mode, char **pzErrMsg){
  sqlite *db;
  int rc;

  /* Allocate the sqlite data structure */
  db = sqliteMalloc( sizeof(sqlite) );
  if( pzErrMsg ) *pzErrMsg = 0;
  if( db==0 ) goto no_mem_on_open;
  sqliteHashInit(&db->tblHash, SQLITE_HASH_STRING, 0);
  sqliteHashInit(&db->idxHash, SQLITE_HASH_STRING, 0);
  sqliteHashInit(&db->trigHash, SQLITE_HASH_STRING, 0);
  sqliteHashInit(&db->trigDrop, SQLITE_HASH_STRING, 0);
  sqliteHashInit(&db->tblDrop, SQLITE_HASH_POINTER, 0);
  sqliteHashInit(&db->idxDrop, SQLITE_HASH_POINTER, 0);
  sqliteHashInit(&db->aFunc, SQLITE_HASH_STRING, 1);
  sqliteRegisterBuiltinFunctions(db);
  db->onError = OE_Default;
  db->priorNewRowid = 0;
  db->magic = SQLITE_MAGIC_BUSY;
  
  /* Open the backend database driver */
  rc = sqliteBtreeOpen(zFilename, mode, MAX_PAGES, &db->pBe);
  if( rc!=SQLITE_OK ){
    switch( rc ){
      default: {
        sqliteSetString(pzErrMsg, "unable to open database: ", zFilename, 0);
      }
    }
    sqliteFree(db);
    sqliteStrRealloc(pzErrMsg);
    return 0;
  }

  /* Attempt to read the schema */
  rc = sqliteInit(db, pzErrMsg);
  db->magic = SQLITE_MAGIC_OPEN;
  if( sqlite_malloc_failed ){
    sqlite_close(db);
    goto no_mem_on_open;
  }else if( rc!=SQLITE_OK && rc!=SQLITE_BUSY ){
    sqlite_close(db);
    sqliteStrRealloc(pzErrMsg);
    return 0;
  }else if( pzErrMsg ){
    sqliteFree(*pzErrMsg);
    *pzErrMsg = 0;
  }
  return db;

no_mem_on_open:
  sqliteSetString(pzErrMsg, "out of memory", 0);
  sqliteStrRealloc(pzErrMsg);
  return 0;
}

/*
** Erase all schema information from the schema hash table.  Except
** tables that are created using CREATE TEMPORARY TABLE are preserved
** if the preserveTemps flag is true.
**
** The database schema is normally read in once when the database
** is first opened and stored in a hash table in the sqlite structure.
** This routine erases the stored schema.  This erasure occurs because
** either the database is being closed or because some other process
** changed the schema and this process needs to reread it.
*/
static void clearHashTable(sqlite *db, int preserveTemps){
  HashElem *pElem;
  Hash temp1;
  Hash temp2;

  /* Make sure there are no uncommited DROPs */
  assert( sqliteHashFirst(&db->tblDrop)==0 || sqlite_malloc_failed );
  assert( sqliteHashFirst(&db->idxDrop)==0 || sqlite_malloc_failed );
  assert( sqliteHashFirst(&db->trigDrop)==0 || sqlite_malloc_failed );
  temp1 = db->tblHash;
  temp2 = db->trigHash;
  sqliteHashInit(&db->trigHash, SQLITE_HASH_STRING, 0);
  sqliteHashClear(&db->idxHash);

  for(pElem=sqliteHashFirst(&temp2); pElem; pElem=sqliteHashNext(pElem)){
    Trigger * pTrigger = sqliteHashData(pElem);
    Table *pTab = sqliteFindTable(db, pTrigger->table);
    assert(pTab);
    if( pTab->isTemp && preserveTemps ){ 
      sqliteHashInsert(&db->trigHash, pTrigger->name, strlen(pTrigger->name), 
          pTrigger);
    }else{
      sqliteDeleteTrigger(pTrigger);
    }
  }
  sqliteHashClear(&temp2);

  sqliteHashInit(&db->tblHash, SQLITE_HASH_STRING, 0);

  for(pElem=sqliteHashFirst(&temp1); pElem; pElem=sqliteHashNext(pElem)){
    Table *pTab = sqliteHashData(pElem);
    if( preserveTemps && pTab->isTemp ){
      Index *pIdx;
      int nName = strlen(pTab->zName);
      Table *pOld = sqliteHashInsert(&db->tblHash, pTab->zName, nName+1, pTab);
      if( pOld!=0 ){
        assert( pOld==pTab );   /* Malloc failed on the HashInsert */
        sqliteDeleteTable(db, pOld);
        continue;
      }
      for(pIdx=pTab->pIndex; pIdx; pIdx=pIdx->pNext){
        int n = strlen(pIdx->zName)+1;
        Index *pOldIdx;
        pOldIdx = sqliteHashInsert(&db->idxHash, pIdx->zName, n, pIdx);
        if( pOld ){
          assert( pOldIdx==pIdx );
          sqliteUnlinkAndDeleteIndex(db, pOldIdx);
        }
      }
    }else{
      sqliteDeleteTable(db, pTab);
    }
  }
  sqliteHashClear(&temp1);
  db->flags &= ~SQLITE_Initialized;
}

/*
** Return the ROWID of the most recent insert
*/
int sqlite_last_insert_rowid(sqlite *db){
  return db->lastRowid;
}

/*
** Return the number of changes in the most recent call to sqlite_exec().
*/
int sqlite_changes(sqlite *db){
  return db->nChange;
}

/*
** Close an existing SQLite database
*/
void sqlite_close(sqlite *db){
  HashElem *i;
  if( sqliteSafetyCheck(db) || sqliteSafetyOn(db) ){ return; }
  db->magic = SQLITE_MAGIC_CLOSED;
  sqliteBtreeClose(db->pBe);
  clearHashTable(db, 0);
  if( db->pBeTemp ){
    sqliteBtreeClose(db->pBeTemp);
  }
  for(i=sqliteHashFirst(&db->aFunc); i; i=sqliteHashNext(i)){
    FuncDef *pFunc, *pNext;
    for(pFunc = (FuncDef*)sqliteHashData(i); pFunc; pFunc=pNext){
      pNext = pFunc->pNext;
      sqliteFree(pFunc);
    }
  }
  sqliteHashClear(&db->aFunc);
  sqliteFree(db);
}

/*
** Return TRUE if the given SQL string ends in a semicolon.
**
** Special handling is require for CREATE TRIGGER statements.
** Whenever the CREATE TRIGGER keywords are seen, the statement
** must end with ";END;".
*/
int sqlite_complete(const char *zSql){
  int isComplete = 1;
  int requireEnd = 0;
  int seenText = 0;
  int seenCreate = 0;
  while( *zSql ){
    switch( *zSql ){
      case ';': {
        isComplete = 1;
        seenText = 1;
        seenCreate = 0;
        break;
      }
      case ' ':
      case '\t':
      case '\n':
      case '\f': {
        break;
      }
      case '[': {
        isComplete = 0;
        seenText = 1;
        seenCreate = 0;
        zSql++;
        while( *zSql && *zSql!=']' ){ zSql++; }
        if( *zSql==0 ) return 0;
        break;
      }
      case '"':
      case '\'': {
        int c = *zSql;
        isComplete = 0;
        seenText = 1;
        seenCreate = 0;
        zSql++;
        while( *zSql && *zSql!=c ){ zSql++; }
        if( *zSql==0 ) return 0;
        break;
      }
      case '-': {
        if( zSql[1]!='-' ){
          isComplete = 0;
          seenCreate = 0;
          break;
        }
        while( *zSql && *zSql!='\n' ){ zSql++; }
        if( *zSql==0 ) return seenText && isComplete && requireEnd==0;
        break;
      }
      case 'c':
      case 'C': {
        seenText = 1;
        if( !isComplete ) break;
        isComplete = 0;
        if( sqliteStrNICmp(zSql, "create", 6)!=0 ) break;
        if( !isspace(zSql[6]) ) break;
        zSql += 5;
        seenCreate = 1;
        while( isspace(zSql[1]) ) zSql++;
        if( sqliteStrNICmp(&zSql[1],"trigger", 7)!=0 ) break;
        zSql += 7;
        requireEnd++;
        break;
      }
      case 't':
      case 'T': {
        seenText = 1;
        if( !seenCreate ) break;
        seenCreate = 0;
        isComplete = 0;
        if( sqliteStrNICmp(zSql, "trigger", 7)!=0 ) break;
        if( !isspace(zSql[7]) ) break;
        zSql += 6;
        requireEnd++;
        break;
      }
      case 'e':
      case 'E': {
        seenCreate = 0;
        seenText = 1;
        if( !isComplete ) break;
        isComplete = 0;
        if( requireEnd==0 ) break;
        if( sqliteStrNICmp(zSql, "end", 3)!=0 ) break;
        zSql += 2;
        while( isspace(zSql[1]) ) zSql++;
        if( zSql[1]==';' ){
          zSql++;
          isComplete = 1;
          requireEnd--;
        }
        break;
      }
      default: {
        seenCreate = 0;
        seenText = 1;
        isComplete = 0;
        break;
      }
    }
    zSql++;
  }
  return seenText && isComplete && requireEnd==0;
}

/*
** Execute SQL code.  Return one of the SQLITE_ success/failure
** codes.  Also write an error message into memory obtained from
** malloc() and make *pzErrMsg point to that message.
**
** If the SQL is a query, then for each row in the query result
** the xCallback() function is called.  pArg becomes the first
** argument to xCallback().  If xCallback=NULL then no callback
** is invoked, even for queries.
*/
int sqlite_exec(
  sqlite *db,                 /* The database on which the SQL executes */
  const char *zSql,           /* The SQL to be executed */
  sqlite_callback xCallback,  /* Invoke this callback routine */
  void *pArg,                 /* First argument to xCallback() */
  char **pzErrMsg             /* Write error messages here */
){
  Parse sParse;

  if( pzErrMsg ) *pzErrMsg = 0;
  if( sqliteSafetyOn(db) ) goto exec_misuse;
  if( (db->flags & SQLITE_Initialized)==0 ){
    int rc = sqliteInit(db, pzErrMsg);
    if( rc!=SQLITE_OK ){
      sqliteStrRealloc(pzErrMsg);
      sqliteSafetyOff(db);
      return rc;
    }
  }
  if( db->recursionDepth==0 ){ db->nChange = 0; }
  db->recursionDepth++;
  memset(&sParse, 0, sizeof(sParse));
  sParse.db = db;
  sParse.pBe = db->pBe;
  sParse.xCallback = xCallback;
  sParse.pArg = pArg;
  sqliteRunParser(&sParse, zSql, pzErrMsg);
  if( sqlite_malloc_failed ){
    sqliteSetString(pzErrMsg, "out of memory", 0);
    sParse.rc = SQLITE_NOMEM;
    sqliteBtreeRollback(db->pBe);
    if( db->pBeTemp ) sqliteBtreeRollback(db->pBeTemp);
    db->flags &= ~SQLITE_InTrans;
    clearHashTable(db, 0);
  }
  sqliteStrRealloc(pzErrMsg);
  if( sParse.rc==SQLITE_SCHEMA ){
    clearHashTable(db, 1);
  }
  db->recursionDepth--;
  if( sqliteSafetyOff(db) ) goto exec_misuse;
  return sParse.rc;

exec_misuse:
  if( pzErrMsg ){
    *pzErrMsg = 0;
    sqliteSetString(pzErrMsg, sqlite_error_string(SQLITE_MISUSE), 0);
    sqliteStrRealloc(pzErrMsg);
  }
  return SQLITE_MISUSE;
}

/*
** Return a static string that describes the kind of error specified in the
** argument.
*/
const char *sqlite_error_string(int rc){
  const char *z;
  switch( rc ){
    case SQLITE_OK:         z = "not an error";                          break;
    case SQLITE_ERROR:      z = "SQL logic error or missing database";   break;
    case SQLITE_INTERNAL:   z = "internal SQLite implementation flaw";   break;
    case SQLITE_PERM:       z = "access permission denied";              break;
    case SQLITE_ABORT:      z = "callback requested query abort";        break;
    case SQLITE_BUSY:       z = "database is locked";                    break;
    case SQLITE_LOCKED:     z = "database table is locked";              break;
    case SQLITE_NOMEM:      z = "out of memory";                         break;
    case SQLITE_READONLY:   z = "attempt to write a readonly database";  break;
    case SQLITE_INTERRUPT:  z = "interrupted";                           break;
    case SQLITE_IOERR:      z = "disk I/O error";                        break;
    case SQLITE_CORRUPT:    z = "database disk image is malformed";      break;
    case SQLITE_NOTFOUND:   z = "table or record not found";             break;
    case SQLITE_FULL:       z = "database is full";                      break;
    case SQLITE_CANTOPEN:   z = "unable to open database file";          break;
    case SQLITE_PROTOCOL:   z = "database locking protocol failure";     break;
    case SQLITE_EMPTY:      z = "table contains no data";                break;
    case SQLITE_SCHEMA:     z = "database schema has changed";           break;
    case SQLITE_TOOBIG:     z = "too much data for one table row";       break;
    case SQLITE_CONSTRAINT: z = "constraint failed";                     break;
    case SQLITE_MISMATCH:   z = "datatype mismatch";                     break;
    case SQLITE_MISUSE:     z = "library routine called out of sequence";break;
    default:                z = "unknown error";                         break;
  }
  return z;
}

/*
** This routine implements a busy callback that sleeps and tries
** again until a timeout value is reached.  The timeout value is
** an integer number of milliseconds passed in as the first
** argument.
*/
static int sqliteDefaultBusyCallback(
 void *Timeout,           /* Maximum amount of time to wait */
 const char *NotUsed,     /* The name of the table that is busy */
 int count                /* Number of times table has been busy */
){
#if SQLITE_MIN_SLEEP_MS==1
  int delay = 10;
  int prior_delay = 0;
  int timeout = (int)Timeout;
  int i;

  for(i=1; i<count; i++){ 
    prior_delay += delay;
    delay = delay*2;
    if( delay>=1000 ){
      delay = 1000;
      prior_delay += 1000*(count - i - 1);
      break;
    }
  }
  if( prior_delay + delay > timeout ){
    delay = timeout - prior_delay;
    if( delay<=0 ) return 0;
  }
  sqliteOsSleep(delay);
  return 1;
#else
  int timeout = (int)Timeout;
  if( (count+1)*1000 > timeout ){
    return 0;
  }
  sqliteOsSleep(1000);
  return 1;
#endif
}

/*
** This routine sets the busy callback for an Sqlite database to the
** given callback function with the given argument.
*/
void sqlite_busy_handler(
  sqlite *db,
  int (*xBusy)(void*,const char*,int),
  void *pArg
){
  db->xBusyCallback = xBusy;
  db->pBusyArg = pArg;
}

/*
** This routine installs a default busy handler that waits for the
** specified number of milliseconds before returning 0.
*/
void sqlite_busy_timeout(sqlite *db, int ms){
  if( ms>0 ){
    sqlite_busy_handler(db, sqliteDefaultBusyCallback, (void*)ms);
  }else{
    sqlite_busy_handler(db, 0, 0);
  }
}

/*
** Cause any pending operation to stop at its earliest opportunity.
*/
void sqlite_interrupt(sqlite *db){
  db->flags |= SQLITE_Interrupt;
}

/*
** Windows systems should call this routine to free memory that
** is returned in the in the errmsg parameter of sqlite_open() when
** SQLite is a DLL.  For some reason, it does not work to call free()
** directly.
**
** Note that we need to call free() not sqliteFree() here, since every
** string that is exported from SQLite should have already passed through
** sqliteStrRealloc().
*/
void sqlite_freemem(void *p){ free(p); }

/*
** Windows systems need functions to call to return the sqlite_version
** and sqlite_encoding strings.
*/
const char *sqlite_libversion(void){ return sqlite_version; }
const char *sqlite_libencoding(void){ return sqlite_encoding; }

/*
** Create new user-defined functions.  The sqlite_create_function()
** routine creates a regular function and sqlite_create_aggregate()
** creates an aggregate function.
**
** Passing a NULL xFunc argument or NULL xStep and xFinalize arguments
** disables the function.  Calling sqlite_create_function() with the
** same name and number of arguments as a prior call to
** sqlite_create_aggregate() disables the prior call to
** sqlite_create_aggregate(), and vice versa.
**
** If nArg is -1 it means that this function will accept any number
** of arguments, including 0.
*/
int sqlite_create_function(
  sqlite *db,          /* Add the function to this database connection */
  const char *zName,   /* Name of the function to add */
  int nArg,            /* Number of arguments */
  void (*xFunc)(sqlite_func*,int,const char**),  /* The implementation */
  void *pUserData      /* User data */
){
  FuncDef *p;
  if( db==0 || zName==0 || sqliteSafetyCheck(db) ) return 1;
  p = sqliteFindFunction(db, zName, strlen(zName), nArg, 1);
  if( p==0 ) return 1;
  p->xFunc = xFunc;
  p->xStep = 0;
  p->xFinalize = 0;
  p->pUserData = pUserData;
  return 0;
}
int sqlite_create_aggregate(
  sqlite *db,          /* Add the function to this database connection */
  const char *zName,   /* Name of the function to add */
  int nArg,            /* Number of arguments */
  void (*xStep)(sqlite_func*,int,const char**), /* The step function */
  void (*xFinalize)(sqlite_func*),              /* The finalizer */
  void *pUserData      /* User data */
){
  FuncDef *p;
  if( db==0 || zName==0 || sqliteSafetyCheck(db) ) return 1;
  p = sqliteFindFunction(db, zName, strlen(zName), nArg, 1);
  if( p==0 ) return 1;
  p->xFunc = 0;
  p->xStep = xStep;
  p->xFinalize = xFinalize;
  p->pUserData = pUserData;
  return 0;
}
