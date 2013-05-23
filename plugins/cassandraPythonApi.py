cfIndex = []
errorStr = []
Server = ''
EasyInsert = {}

import pycassa
import numpy as np
from matplotlib import pyplot as plt
from matplotlib import dates
import matplotlib
import datetime
import gc
import os

SERVER="10.5.0.187:9160"
DEF_RF = {'replication_factor': '1'}
DEF_STRATEGY = "SimpleStrategy"

def cassandraConnect(keyspace, columnfamily, server):

	systemManager = pycassa.system_manager.SystemManager(server)
	keyspaces    	= systemManager.list_keyspaces()
	if keyspace not in keyspaces:
		setError("keyspace not created\n")
		return -1
	pool         = pycassa.pool.ConnectionPool(keyspace, server_list=[server])
	columnfamily = pycassa.columnfamily.ColumnFamily(pool, columnfamily)
	cfIndex.append(columnfamily)
	return cfIndex.index(columnfamily)
	#index = index + 1
	Server = server	

def cassandraCreateKeyspace(keyspace, strategy, replicationfactor,server):

	systemManager = pycassa.system_manager.SystemManager(server)
	keyspaces     = systemManager.list_keyspaces()
	if keyspace not in keyspaces:

		rf = {'replication_factor':str(replicationfactor)}
		systemManager.create_keyspace(keyspace, strategy, rf)
		return 1
	else:
			setError("keyspace is already exist\n")
			return -1

def cassandraDropKeyspace(keyspace, server):

	sys = pycassa.system_manager.SystemManager(server)
	if isExistKeyspace(keyspace):
		sys.drop_keyspace(keyspace)
		return 0
	else:
		setError(keyspace + "not exist")
		return -1


def cassandraCreateColumnFamily(keyspace, cf, comparator, server):

		systemManager = pycassa.system_manager.SystemManager()
		ctype = pycassa.types.CompositeType(comparator)
		systemManager.create_column_family(keyspace, cf, comparator_type=ctype)
		return 1

def cassandraDropColumnFamily(keyspace, columnfamily, server):

	sys = pycassa.system_manager.SystemManager(server)
	if isExistColumnfamily(keyspace, columnfamily, server):
		sys.drop_column_family(keyspace, columnfamily)
		return 0
	else:
		setError(columnfamily + "not exist in " + keyspace)
		return -1


def cassandraInsert(index, key, colName, colValue):

	cfIndex[index].insert(key, {colName:colValue})
	return 1


def isExistKeyspace(keyspace, server):
	systemManager = pycassa.system_manager.SystemManager(server)
	keyspaces     = systemManager.list_keyspaces()
	#print keyspaces, keyspace	
	if keyspace not in keyspaces:
		return 0
	else:
		return 1

def isExistColumnfamily(keyspace, columnfamily, server):
	if not isExistKeyspace(keyspace, server):
		return 0
	sys = pycassa.system_manager.SystemManager(server)
	#print columnfamily
	try:
		sys.get_keyspace_column_families(keyspace)[columnfamily]
		return 1
	except KeyError:
		return 0


def compositeInsert(index, key, value,  colname):
	parsedCol = parseColname(colname)
	setError(str(index))
	#print parsedCol 
	return cfIndex[index].insert(key, {parsedCol:value})

def cassandraEasyInsert(ksp, cf, svr, row, col_name, col_val):
	setError(os.getcwd()+str(col_val)+" " + str(len(EasyInsert)) +"\n");
	try :
		try :
			cfRef = EasyInsert[ksp+cf]
			cfRef.insert(row, {col_name:str(col_val)})
			return 0

		except :
			systemManager = pycassa.system_manager.SystemManager(svr)
			keyspaces     = systemManager.list_keyspaces()

			if ksp not in keyspaces:
				systemManager.create_keyspace(ksp, DEF_STRATEGY, DEF_RF)
			try:
				systemManager.get_keyspace_column_families(ksp)[cf]
				pool = pycassa.pool.ConnectionPool(ksp, server_list=[svr])
				cfRef = pycassa.columnfamily.ColumnFamily(pool, cf)
				EasyInsert[ksp+cf] = cfRef
			except:
				try:
					systemManager.create_column_family(ksp, cf)
					pool         = pycassa.pool.ConnectionPool(ksp)
					cfRef = pycassa.columnfamily.ColumnFamily(pool, cf)
					EasyInsert[ksp+cf] = cfRef
				except Exception as e:
					setError(str(e))
			cfRef.insert(row, {col_name:str(col_val)})
	except Exception as e:
		setError(str(e))
	return 0
	

def cassandraGet(index, key):
	return cfIndex[index].get(key)

def cassandraInit():
	import pycassa as a
	pycassa = a 


def setError(errstr):
	errorStr.append(errstr)

def getError():
	return str(errorStr.pop())

def parseColname(colname):
	colnames = colname.split(':')
	retval = []
	for i in colnames:
		if i is '' :
			break
		if i[0] == 's':
			retval.append(str(i[1:]))
		elif i[0] == 'l':
			retval.append(long(i[1:]))
		elif i[0] == 'i':
			retval.append(int(i[1:]))
		elif i[0] == 'f':
			retval.append(float(i[1:]))
	return tuple (retval)

def checkCassandra(server):
	try :
		sys  = pycassa.system_manager.SystemManager(server)
		return 0
	except Exception as e:
		setError(str(e))
		return -1

def createGraph(index, key, path, xlabel, ylabel):
	try:
		import os
	except OverflowError:
		return -1
	x, y = getCounterData(index, key)
	#print os.getcwd()
	
	y = y/10
	ax = plt.gca()
	plt.cla()
	mkfunc = lambda x, pos: '%1.1fM' % (x * 1e-6) if x >= 1e6 else '%1.1fK' % (x * 1e-3) if x >= 1e3 else '%1.1f' % x
	mkformatter = matplotlib.ticker.FuncFormatter(mkfunc)
	ax.yaxis.set_major_formatter(mkformatter)
	plt.autoscale()
	#plt.format_xdata = dates.DateFormatter('%Y-%m-%d')
	plt.xticks(rotation=25)
	plt.grid(True)
	plt.xlabel(xlabel)
	plt.ylabel(ylabel)
	plt.plot(x, y)
	plt.savefig("./html/"+key+".png")
	del x
	del y
	
def getCounterData(index, key):

	x = []
	y = []

	for i,j in cfIndex[index].get(key,column_reversed=True, column_count=360).iteritems():
		x.append(i[0])
		y.append(j)
	s = np.array(x, dtype = np.int64)
	dates = [datetime.datetime.fromtimestamp(ts) for ts in s]
	value = np.array(y, dtype=np.int64)
	value = value[::-1]
	value2 = value[1:]
	value2=list(value2)
	popped = value2.pop()
	value2.append(popped)
	value2.append(popped)
	value2 = np.array(value2)
	diff= value2 - value
	diff[diff < 0 ] = 0
	return dates[::-1], diff


