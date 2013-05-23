
#include <Python.h>
PyObject *mainModule ;
PyObject *cassandraClient;
PyObject* importModule(char * modulename);
PyObject* pythonGetFunct (char *funName);
PyGILState_STATE gilstate;

#define NAME_BUFFER_SIZE 512
#define DEFAULT_KEYSPACE "default"
#define DEFAULT_STRATEGY "SimpleStrategy"
#define DEFAULT_REPLICATION_FACTOR 1
#define DEFAULT_SERVER_PORT "127.0.0.1:9160"
#define LOCK_GIL  \
		gilstate = PyGILState_Ensure();

#define UNLOCK_GIL \
		PyGILState_Release(gilstate);
		
 
typedef struct CassandraclientNetflowDevice {
  int index;
  unsigned int initialized;
  char columnfamily[NAME_BUFFER_SIZE];
  unsigned long long numwrite;
  char server[20];
  char keyspace[512];
} cassandraNFDevice;

int ImportAll(char * modulename);
int cassandraInit(char *path);
int cassandraConnect(char *ksp, char *columnfamily, char *server );
int cassandraCreateKeyspace(char* ksp, char* strategy, int rf, char* svr);
int cassandraCreateColumnFamily(char *ksp, char *cf, char *cmptr, char *svr);
int cassandraGet(int index, char *key);
int cassandraInsert (int index, char *key, char *value, char *fmt, ...);
int compositeInsert(int index, char *key, char *colValue, char *colname);
int cassandraEasyInsert(char *ksp, char *cf, char *svr, char *row, 
						char *colname, char *colvalue);
int cassandraDropColumnFamily(char* keyspace, char* cf, char* server);
int cassandraDropKeyspace(char* keyspace, char* server);
int isExistColumnfamily(char *keyspace, char *columnfamily, char * server);
int isExistKeyspace(char* keyspace, char* server);
int isExistPycassa(void);
char  *getargs (char *fmt, va_list argp);
char * getError(void);
void print_Error(void);
int getDate(char *date, char*timestamp, int size, int dump);
int checkCassandra(char *server);
