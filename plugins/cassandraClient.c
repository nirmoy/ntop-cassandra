/*
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
  
#include "cassandraClient.h"
int initialize;
PyInterpreterState * mainInterpreterState;
PyThreadState * myThreadState;
PyThreadState * mainThreadState;

/*
 * This function act as a "import sys" in python
 */


PyObject* importModule(char * modulename)
{
  
 PyObject	*module = PyImport_ImportModule(modulename);
 
 if (module) {
  PyModule_AddObject(mainModule, modulename, module);
  return module;
 }

 return NULL;
}

/*
 * 
 * this function acts as "from pycassa import *"
 */


int ImportAll(char * modulename)
{
  
 PyObject *module = PyImport_ImportModule(modulename);

 if (module==NULL) {
  PyErr_Print();
   return -1;
 }

 PyObject * moduleAttributes = PyObject_Dir(module);
 Py_ssize_t pos = 0;
 Py_ssize_t attrSize = PyList_Size(moduleAttributes);
 
 while (pos < attrSize) {
   PyObject * attrValue = PyList_GetItem(moduleAttributes, pos);//borrowed ref
   PyModule_AddObject(mainModule, PyString_AsString(attrValue), 
			PyObject_GetAttr(module,attrValue));
   pos++;
 }

 Py_DecRef(module);
 Py_DecRef(moduleAttributes);
 return 0;
}

/*
 * it retrive a object from python mainmodule
 */


PyObject* pythonGetFunct (char *funName)
{
 PyObject * pythonFunct = PyObject_GetAttrString(mainModule, funName);
 
 if (pythonFunct == NULL)
   return NULL;
 
 return pythonFunct;
}
/*
 * this initialize python interpreter and
 * set system path to specific path
 */


int cassandraInit(char *path)
{
/*
PyThreadState *mainThreadState, *myThreadState, *tempState;
PyInterpreterState *mainInterpreterState;
if ( !Py_IsInitialized()) {
Py_Initialize();  
PyEval_InitThreads();//for thread support 
PyThreadState * py_tstate = PyEval_SaveThread();
PyEval_ReleaseThread(py_tstate);
}
*/
  
  char buffer[550];

  if (!Py_IsInitialized())
	  return -1;

  LOCK_GIL
  mainModule = PyImport_AddModule ("__main__");
  
  sprintf(buffer, "sys.path.append('%s/')", path);
  PyRun_SimpleString("import sys");
  PyRun_SimpleString(buffer);
  
  cassandraClient = importModule("cassandraPythonApi");
  
  if (cassandraClient == NULL) {
	PyErr_Print();
	PyGILState_Release(gilstate);
    return -1;
  }
  initialize = 1;
  UNLOCK_GIL;//PyGILState_Release(gilstate);
  return 0;
}


/*        cassandraConnect()
 *	  @ name of the keyspace
 *	  @ name of the columnfamily
 *	  @ name of the server and port ex. "127.0.0.1:9160" 
 * 
 *	Ex cassandraConnect("testkeyspace", "testcf", "127.0.0.1:3128")      
 * 
 */


int cassandraConnect(char *keyspace, char *columnfamily, char *server)
{
  int ret;
  LOCK_GIL

  PyObject *func = PyObject_GetAttrString(cassandraClient, "cassandraConnect");
  PyObject *args = Py_BuildValue("(sss)", keyspace, columnfamily, server);
  PyObject *fun_ret = PyEval_CallObject(func, args);
  PyArg_Parse(fun_ret, "i", &ret);
  
  Py_XDECREF(args);
  Py_XDECREF(fun_ret);

  UNLOCK_GIL
  return ret;
}

/* cassandraInsert()
 * @ index return by cassandra connect should not pass 
 *   wrong index no error testing
 * @ key of the colimn
 * @ colName
cquireLock();
// swap my thread state out of the interpreter
PyThreadState_Swap(NULL);
// clear out any cruft from thread state object
PyThreadState_Clear(myThreadState);
// delete my thread state object
PyThreadState_Delete(myThreadState);
// release the lock
PyEval_ReleaseLock();Py_XDECREF(myThreadState);
 * @ colValue
 */

int compositeInsert(int cfindex, char *key, char *colValue, char *colname)
{
  int ret = 0;
  LOCK_GIL

  PyObject *func = PyObject_GetAttrString(cassandraClient, "compositeInsert");
  PyObject *args = Py_BuildValue("isss", cfindex, key, colValue, colname);
  PyObject *fun_ret =  PyObject_Call(func, args, NULL);
  PyErr_Print();
  PyArg_Parse(fun_ret, "i", &ret);

  Py_XDECREF(args);
  Py_XDECREF(fun_ret);

  UNLOCK_GIL
  return ret;

}

/* cassandraGet()
 * @ index return by cassandra connect should not pass 
 *   wrong index no error testing
 * @ key of the row
 */

int cassandraGet(int cfindex, char *key)
{
  int ret;
  LOCK_GIL

  PyObject *func = PyObject_GetAttrString(cassandraClient, "cassandraGet");
  PyObject *args = Py_BuildValue("(is)", cfindex, key);
  PyObject *fun_ret = PyEval_CallObject(func, args);
  PyArg_Parse(fun_ret, "i", &ret);

  Py_XDECREF(args);
  Py_XDECREF(fun_ret);
  
  UNLOCK_GIL
  return 1;
}

/* cassandraCreateKeyspace()
 * @ name keyspace
 * @ strategy   ---SimpleStrategy
 * 		---NetworkTopologyStrategy    not supported
 * 		---OldNetworkTopologyStrategy not supported
 * @replicationfactor
 */


int cassandraCreateKeyspace(char *keyspace, char *strategy, int rf, char *server)
{
  int ret;
  LOCK_GIL

  PyObject *func = PyObject_GetAttrString(cassandraClient, 
			"cassandraCreateKeyspace");
  PyObject *args = Py_BuildValue("(ssis)", keyspace, strategy, rf, server);
  PyObject *fun_ret = PyEval_CallObject(func, args);
  PyArg_Parse(fun_ret, "i", &ret);
 
  Py_XDECREF(args);
  Py_XDECREF(fun_ret);   
 
  UNLOCK_GIL
  return ret;
}
/*
 * comparator type
Name          Description
----            ---      -----------
BytesType     Arbitrary hexadecimal bytes (no validation)
AsciiType     US-ASCII character string
UTF8Type 	UTF-8 encoded string
IntegerType 	Arbitrary-precision integer
LongType 	8-byte long
UUIDType 	Type 1 or type 4 UUID
DateType 	Date plus time, encoded as 8 bytes since epoch
BooleanType 	true or false
FloatType 	4-byte floating point
DoubleType 		8-byte floating point
DecimalType 	Variable-precision decimal
CounterColumnType 	counter 	Distributed counter value (8-byte long)
Ex cassandraCreateColumnFamily('ksp', 'cf', "AsciiType IntegerType")
*/


int cassandraCreateColumnFamily(char *keyspace, char *cf,
				 char *comparator, char *server)
{
  int ret;
  LOCK_GIL

  PyObject *func = PyObject_GetAttrString(cassandraClient,
					 "cassandraCreateColumnFamily");
  PyObject *args = Py_BuildValue("(ssss)", keyspace, cf, comparator, server);
  PyObject *fun_ret = PyEval_CallObject(func, args);
  if (fun_ret == NULL) {
    PyErr_Print();
    return -1;
  }
  PyArg_Parse(fun_ret, "i", &ret);
  Py_XDECREF(args);
  Py_XDECREF(fun_ret);   
  
  UNLOCK_GIL
  return ret;
}

/* *******************************/

int cassandraDropColumnFamily(char *keyspace, char *cf, char *server)
{
  int ret;
  LOCK_GIL

  PyObject *func = PyObject_GetAttrString(cassandraClient,
					 "cassandraDropColumnFamily");
  PyObject *args = Py_BuildValue("(sss)", keyspace, cf, server);
  PyObject *fun_ret = PyEval_CallObject(func, args);
  if (fun_ret == NULL) {
    PyErr_Print();
    return -1;
  }
  PyArg_Parse(fun_ret, "i", &ret);

  Py_XDECREF(args);
  Py_XDECREF(fun_ret);   

  UNLOCK_GIL
  return ret;
}

/* *******************************/

 int cassandraDropKeyspace(char *keyspace, char *server)
{
  int ret;
  LOCK_GIL

  PyObject *func = PyObject_GetAttrString(cassandraClient, 
					"cassandraDropKeyspace");
  PyObject *args = Py_BuildValue("(ss)", keyspace, server);
  PyObject *fun_ret = PyEval_CallObject(func, args);
  PyArg_Parse(fun_ret, "i", &ret);

  Py_XDECREF(args);
  Py_XDECREF(fun_ret);   

  UNLOCK_GIL
  return ret;
}

/**********************************
 */
int isExistPycassa(void)
{


 LOCK_GIL

 PyObject	*module = PyImport_ImportModule("pycassa"); 
 
 if ( module != NULL) {
   UNLOCK_GIL
   return 1;
 }

UNLOCK_GIL
 return 0;
}

int isExistKeyspace(char *keyspace, char *server)
{
  int ret;
  
  if ( !isExistPycassa())
    return -1;
  LOCK_GIL

  PyObject *func = PyObject_GetAttrString(cassandraClient, "isExistKeyspace");
  PyObject *args = Py_BuildValue("(ss)", keyspace, server);
  PyObject *fun_ret = PyEval_CallObject(func, args);
  PyArg_Parse(fun_ret, "i", &ret);

  Py_XDECREF(args);
  Py_XDECREF(fun_ret);   

  UNLOCK_GIL
  return ret; 
}

int isExistColumnfamily(char *keyspace, char *columnfamily, char *server)
{
  int ret;
 
  if ( !isExistKeyspace(keyspace, server))
    return -1;

  LOCK_GIL
  PyObject *func = PyObject_GetAttrString(cassandraClient, 
						"isExistColumnfamily");
  PyObject *args = Py_BuildValue("(sss)", keyspace, columnfamily, server);
  PyObject *fun_ret = PyEval_CallObject(func, args);
  PyArg_Parse(fun_ret, "i", &ret);

  Py_XDECREF(args);
  Py_XDECREF(fun_ret);   

  UNLOCK_GIL
  return ret; 
}

char  *getargs (char *fmt, va_list argp)
{
  const char *p;
  int i = 0;
  char value[1024] ;
  memset(value, '\0', 1024);
  for (p = fmt; *p != '\0'; p++)
    {
      if (*p != '%')
	{
	  putchar (*p);
	  continue;
	}

      switch (*++p)
	{
	case 'd':
	  sprintf(value + i, "i%i:", va_arg(argp, int));
	  i = strlen(value);
	  break;

	case 'i':
	  sprintf(value + i, "i%i:",(int) va_arg(argp, int));
	  i = strlen(value);
	  break;

	case 'l':
	  sprintf(value + i, "l%lld:", va_arg(argp, long long));
	  i = strlen(value);
	  break;

	case 's':
	  sprintf(value +i , "s%s:", va_arg(argp, char *));
	  i = strlen(value);
	  break;



	case 'f':
	  sprintf(value + i, "f%f:", (double)va_arg(argp, double));
	  i = strlen(value);
	  break;

	case '%':
	  //putchar ('%');
	  break;
	}
    }
    return strdup(value);
}

//And to call it
/*
 * cassandraInsert()
 * @index return by cassandraConnect
 * @key key to be store by
 */
int	
cassandraInsert (int cfindex, char *key, char *value, char *fmt, ...)
{
  char *colname;
  va_list list;
  va_start (list, fmt);

  colname = getargs (fmt, list);

  va_end (list);
  return compositeInsert(cfindex, key, value, colname);
}



int
cassandraEasyInsert(char *ksp, char *cf, char *server, 
					char *row, char *col_name, char * val)
{
/*
PyThreadState *tempState;
PyEval_AcquireLock();
PyInterpreterState * mainInterpreterState = mainThreadState->interp;
PyThreadState * myThreadState = PyThreadState_New(mainInterpreterState);
PyEval_ReleaseLock();  

PyEval_AcquireLock();
tempState = PyThreadState_Swap(myThreadState);
printf("%s--%p--%p\n", cf, myThreadState, mainInterpreterState);
*/
LOCK_GIL

  int ret;

  if (cassandraClient == NULL) {
	  return -1;
  }

  PyObject *func = PyObject_GetAttrString(cassandraClient, 
								"cassandraEasyInsert");
  PyObject *buffer = PyBuffer_FromReadWriteMemory((void *)val, 48);
  PyObject *args = Py_BuildValue("(sssssO)", ksp, cf, server, row, 
								 col_name, buffer);
  if (args == NULL) {
	  UNLOCK_GIL
	  return -1;
   }
  PyObject *fun_ret = PyObject_Call(func, args, NULL);
  
  if (fun_ret == NULL) {
	 PyGILState_Release(gilstate);
	 return -1;
  }

  PyArg_Parse(fun_ret, "i", &ret);

  Py_XDECREF(args);
  Py_XDECREF(fun_ret);
UNLOCK_GIL
  return ret;
}



char * getError(void)
{
  char *error;

  if (!Py_IsInitialized())
	  return "Python is not Initialized\n";

  LOCK_GIL
 
  PyObject *func = PyObject_GetAttrString(cassandraClient, "getError");
  PyObject *fun_ret = PyEval_CallObject(func, NULL);
  
  if (fun_ret == NULL) {
		UNLOCK_GIL
		return NULL;
  }
  
  error = strdup(PyString_AsString(fun_ret));/* TODO solve mem-leak */
  Py_XDECREF(fun_ret);

  UNLOCK_GIL
  return error; //free up the memeory 
}


void print_Error(void)
{
  char * error = getError();
  if ( error != NULL) {
	printf("%s\n", error);
	free(error);
  }
}
int checkCassandra(char *server)
{
  int ret;
  LOCK_GIL

  PyObject *func = PyObject_GetAttrString(cassandraClient, "checkCassandra");
  if (func == NULL) {
	UNLOCK_GIL
	return -1;
  }

  PyObject *args = Py_BuildValue("(s)", server);
  PyObject *fun_ret = PyEval_CallObject(func, args);

  if (fun_ret == NULL) {
	UNLOCK_GIL
	return -1;
  }
  PyArg_Parse(fun_ret, "i", &ret);

  Py_XDECREF(args);
  Py_XDECREF(fun_ret);

  UNLOCK_GIL
  return ret;
}
