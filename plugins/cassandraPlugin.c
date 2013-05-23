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



#include <Python.h>
#include <ntop.h>
#include <globals-core.h>
#include <globals-report.h>
#include <cassandraClient.h>

static unsigned short initialized = 0,
                      active = 0,
                      CassaDumpInterval = 0;

#define CASSANDRA_CONFIG_FILE "cassa.confg"
#define BUFF_SIZE 1024
#define SMALL_BUFFER 100
#define DUMP 20

static pthread_t cassandraMainThread;
char *Keyspace, 
	 *Server_IP,
	 *ReplicationFactor,
     *Strategy;
static long long int totalWrite = 0;
static long long int totalFailWrite = 0;

static int initCassandraFunct (void);
static void termCassandraFunct (u_char termNtop);
static void setPluginStatus (char *status);
static void handlecassandraHTTPrequest (char *_url);
static void *cassandraMainLoop(void* notUsed _UNUSED_);
static  int parser(void );
static int cassandraUpdate(char *dev, char *counter, Counter value, 
							int isCounter);
static int removeSymbols(char *string);

static ExtraPage cassandraExtraPages[] = {
  { NULL, "statistics.html", "Statistics" },
  { NULL, NULL, NULL }
};

static PluginInfo cassandraPluginInfo[] = {
  {
   VERSION,			/* current ntop version */
   "Cassandra",
   "This plugin is used to Store NetFlow data in Cassandra database.<br>",
   ""
   "1.0",			/* version */
   "<a href=\"http://luca.ntop.org/\" alt=\"Luca's home page\">Nirmoy</A>",
   "Cassandra",	/* http://<host>:<port>/plugins/cassandra *///URL plugin
   0,				/* Active by default */
   ViewConfigure,
   1,				/* Inactive setup */
   initCassandraFunct,		/* InitFunc */
   termCassandraFunct,		/* TermFunc */
   NULL,			//handlecassandraPacket,
   handlecassandraHTTPrequest,
   NULL,			/* no host creation/deletion handle */
   NULL,			/* no capture */
   NULL,			/* no status */
   cassandraExtraPages				//cassandraExtraPages
   }
};

/****************************************/

static void
handlecassandraHTTPrequest (char *_url)
{
 
 char buff[1024];

 sendHTTPHeader (FLAG_HTTP_TYPE_HTML, 0, 1);
 printHTMLheader ("Cassandra Statistic", NULL, 0);
 sendString("<center>\n<b>");
 safe_snprintf(__FILE__, __LINE__, buff, sizeof(buff), 
              "<p>Number of Successfull Write:%d<p>", totalWrite);
 sendString(buff); 
 safe_snprintf(__FILE__, __LINE__, buff, sizeof(buff),
			   "<p>Number of Failed  Write:%d</p>", totalFailWrite);
 sendString(buff);
 sendString("<center></b>");
 printHTMLtrailer();
}

/*********************************************/

static int
initCassandraFunct (void)
{
  int ret;
  
  traceEvent(CONST_TRACE_INFO, 
		"CASSANDRA: Welcome to the Cassandra Plugin for NetFlow ");  
 
  #ifndef HAVE_PYTHON
    setPluginStatus("Disabled - Python version > 2.7 not found");
    return -1;
  #endif
  
  if (initialized == 0) {
      traceEvent(CONST_TRACE_INFO, 
			"CASSANDRA: Reading the configuration file at %s.", 
			 myGlobals.pluginDirs[0]);
    
	  ret = parser(); //Read config file
     
      if (ret == -1) {
          traceEvent (CONST_TRACE_ERROR, 
                      "CASSANDRA: Error in configuration file");
          traceEvent (CONST_TRACE_ERROR, "CASSANDRA: Fail to initialize");
          setPluginStatus("Disabled - Error in config file");
          termCassandraFunct(0);
          return -1;
     }
	 ret = cassandraInit(myGlobals.pluginDirs[0]);
     if (ret == -1) {
         traceEvent (CONST_TRACE_ERROR, 
					 "CASSANDRA: cassandraInit() fails: %s", getError());
         traceEvent (CONST_TRACE_ERROR, "CASSANDRA: Fail to initialize");
         setPluginStatus("Disabled -  cassandraInit() fails");
         termCassandraFunct(0);
         return -1;
     }
     ret = isExistPycassa(); //check pycassa
     if (ret == 0) {
         traceEvent (CONST_TRACE_ERROR, "CASSANDRA: pycassa module not found");
         traceEvent (CONST_TRACE_ERROR, "CASSANDRA: Fail to initialize");
         setPluginStatus("Disabled - Pycassa module not found");
         termCassandraFunct(0);
         return -1;
     }
     ret = checkCassandra(Server_IP);
     if (ret == -1) {
		 traceEvent (CONST_TRACE_ERROR, 
					"CASSANDRA: Cassandra Server not running ");
		 traceEvent (CONST_TRACE_ERROR, "CASSANDRA: Fail To initialize");
         setPluginStatus("Cassandra Server is not Running");
		 termCassandraFunct(0);
		 return -1;
	  }
  }

  traceEvent (CONST_TRACE_INFO, "THREADMGMT[t%lu]: Cassandra:"
				" Data collection thread started [p%d]",
                        (long unsigned int) pthread_self (), getpid ());
  createThread(&cassandraMainThread, cassandraMainLoop, NULL);
  initialized = 1;
  setPluginStatus(NULL);
  return 0;
}

/* * * * * * * * * * * * * * * * * */

static void* 
cassandraMainLoop(void* notUsed _UNUSED_) {
  
  int devIdx, idx;
  ProtocolsList *protoList;
  char keyspace[48];
  active = 1;

  traceEvent(CONST_TRACE_INFO,
	 "THREADMGMT[t%lu]: CASSANDRA: Data collection thread starting [p%d]",
	     (long unsigned int)pthread_self(), getpid());
  ntopSleepUntilStateRUN();
  
  traceEvent(CONST_TRACE_INFO,
             "THREADMGMT[t%lu]: CASSANDRA: Throughput data collection:"
	         " Thread running [p%d]",
             (long unsigned int)pthread_self(), getpid());
  
  for(;myGlobals.ntopRunState <= FLAG_NTOPSTATE_RUN;) {

    ntopSleepWhileSameState(20);//hard coded

    if(myGlobals.ntopRunState > FLAG_NTOPSTATE_RUN) {
      traceEvent(CONST_TRACE_INFO,
                 "THREADMGMT[t%lu]: CASSANDRA: Throughput data collection:"
				 " Thread stopping [p%d] State>RUN",
                 (long unsigned int)pthread_self(), getpid());
      break;
    }

    for(devIdx=0; devIdx<myGlobals.numDevices; devIdx++) {

      if((myGlobals.device[devIdx].virtualDevice
	  	&& (!myGlobals.device[devIdx].sflowGlobals)
	  		&& (!myGlobals.device[devIdx].netflowGlobals))
	 			|| (!myGlobals.device[devIdx].activeDevice))
		continue;
    /* Cassandra support 48 char keyspace/columnfamily name */
    snprintf(keyspace, 48, "%s", myGlobals.device[devIdx].uniqueIfName);
    removeSymbols(keyspace);
    
	cassandraUpdate(keyspace, "ethernetPkts",  
					myGlobals.device[devIdx].ethernetPkts.value, 0);
	cassandraUpdate(keyspace, "broadcastPkts", 
					myGlobals.device[devIdx].broadcastPkts.value, 0);
	cassandraUpdate(keyspace, "multicastPkts", 
					myGlobals.device[devIdx].multicastPkts.value, 0);
	cassandraUpdate(keyspace, "ethernetBytes",
					 myGlobals.device[devIdx].ethernetBytes.value, 0);
	cassandraUpdate(keyspace, "ipv4Bytes",     
            	   	myGlobals.device[devIdx].ipv4Bytes.value, 0);

	cassandraUpdate(keyspace, "ipLocalToLocalBytes",  
	    myGlobals.device[devIdx].tcpGlobalTrafficStats.local.value +
	    myGlobals.device[devIdx].udpGlobalTrafficStats.local.value +
        myGlobals.device[devIdx].icmpGlobalTrafficStats.local.value, 0);
	cassandraUpdate(keyspace, "ipLocalToRemoteBytes", 
		myGlobals.device[devIdx].tcpGlobalTrafficStats.local2remote.value +
	    myGlobals.device[devIdx].udpGlobalTrafficStats.local2remote.value +
        myGlobals.device[devIdx].icmpGlobalTrafficStats.local2remote.value,0);
	cassandraUpdate(keyspace, "ipRemoteToLocalBytes", 
		myGlobals.device[devIdx].tcpGlobalTrafficStats.remote2local.value +
		myGlobals.device[devIdx].udpGlobalTrafficStats.remote2local.value +
		myGlobals.device[devIdx].icmpGlobalTrafficStats.remote2local.value, 0);
	cassandraUpdate(keyspace, "ipRemoteToRemoteBytes", 
		myGlobals.device[devIdx].tcpGlobalTrafficStats.remote.value +
		myGlobals.device[devIdx].udpGlobalTrafficStats.remote.value +
		myGlobals.device[devIdx].icmpGlobalTrafficStats.remote.value, 0);

	if(1) {
	  if(myGlobals.device[devIdx].netflowGlobals != NULL) {
	    cassandraUpdate(keyspace, "NF_numFlowPkts", 
			myGlobals.device[devIdx].netflowGlobals->numNetFlowsPktsRcvd, 0);
	    cassandraUpdate(keyspace, "NF_numFlows", 
			myGlobals.device[devIdx].netflowGlobals->numNetFlowsRcvd, 0);
	    cassandraUpdate(keyspace, "NF_numDiscardedFlows",
			myGlobals.device[devIdx].netflowGlobals->numBadFlowPkts+
			myGlobals.device[devIdx].netflowGlobals->numBadFlowBytes+
			myGlobals.device[devIdx].netflowGlobals->numBadFlowReality+
	myGlobals.device[devIdx].netflowGlobals->numNetFlowsV9UnknTemplRcvd, 0);
	  }
	}

	if(1) {
	  cassandraUpdate(keyspace, "droppedPkts", 
					  myGlobals.device[devIdx].droppedPkts.value, 0);
	  cassandraUpdate(keyspace, "fragmentedIpBytes", 
					  myGlobals.device[devIdx].fragmentedIpBytes.value, 0);
	  cassandraUpdate(keyspace, "tcpBytes", 
					  myGlobals.device[devIdx].tcpBytes.value, 0);
	  cassandraUpdate(keyspace, "udpBytes", 
					  myGlobals.device[devIdx].udpBytes.value, 0);
	  cassandraUpdate(keyspace, "otherIpBytes", 
					  myGlobals.device[devIdx].otherIpBytes.value, 0);
	  cassandraUpdate(keyspace, "icmpBytes", 
					  myGlobals.device[devIdx].icmpBytes.value, 0);
	  cassandraUpdate(keyspace, "stpBytes", 
					  myGlobals.device[devIdx].stpBytes.value, 0);
	  cassandraUpdate(keyspace, "ipsecBytes", 
					  myGlobals.device[devIdx].ipsecBytes.value, 0);
	  cassandraUpdate(keyspace, "netbiosBytes", 
					  myGlobals.device[devIdx].netbiosBytes.value, 0);
	  cassandraUpdate(keyspace, "arpRarpBytes", 
					  myGlobals.device[devIdx].arpRarpBytes.value, 0);
	  cassandraUpdate(keyspace, "greBytes", 
					  myGlobals.device[devIdx].greBytes.value, 0);
	  cassandraUpdate(keyspace, "ipv6Bytes", 
					  myGlobals.device[devIdx].ipv6Bytes.value, 0);
	  cassandraUpdate(keyspace, "otherBytes", 
					  myGlobals.device[devIdx].otherBytes.value, 0);
	  cassandraUpdate(keyspace, "upTo64Pkts", 
					  myGlobals.device[devIdx].rcvdPktStats.upTo64.value, 0);
	  cassandraUpdate(keyspace, "upTo128Pkts", 
					  myGlobals.device[devIdx].rcvdPktStats.upTo128.value, 0);
	  cassandraUpdate(keyspace, "upTo256Pkts", 
					  myGlobals.device[devIdx].rcvdPktStats.upTo256.value, 0);
	  cassandraUpdate(keyspace, "upTo512Pkts", 
					  myGlobals.device[devIdx].rcvdPktStats.upTo512.value, 0);
	  cassandraUpdate(keyspace, "upTo1024Pkts", 
					  myGlobals.device[devIdx].rcvdPktStats.upTo1024.value, 0);
	  cassandraUpdate(keyspace, "upTo1518Pkts", 
					  myGlobals.device[devIdx].rcvdPktStats.upTo1518.value, 0);
	  cassandraUpdate(keyspace, "tooLongPkts", 
					  myGlobals.device[devIdx].rcvdPktStats.tooLong.value, 0);

	  if (1) {
	  	 char *prot_long_str[] = { IPOQUE_PROTOCOL_LONG_STRING };
	  	 char rrdPath[512];
	     int j;

	     safe_snprintf(__FILE__, __LINE__, rrdPath, sizeof(rrdPath), 
					"%s/interfaces/%s/IP_",
					myGlobals.rrdPath,  
					myGlobals.device[devIdx].uniqueIfName);

	     for(j=0; j<IPOQUE_MAX_SUPPORTED_PROTOCOLS; j++) {
	     	 TrafficCounter ctr;
	         char tmpStr[128];

	         ctr.value = myGlobals.device[devIdx].l7.protoTraffic[j];
	         safe_snprintf(__FILE__, __LINE__, tmpStr, sizeof(tmpStr), 
			               "%sBytes", prot_long_str[j]);
	         cassandraUpdate(keyspace, tmpStr, ctr.value, 0);
	    }
	}

	  if (myGlobals.device[devIdx].ipProtosList != NULL) {
	      protoList = myGlobals.ipProtosList, idx=0;
	    
		  while(protoList != NULL) {
	      		char protobuf[64];

	      		Counter c = myGlobals.device[devIdx].ipProtosList[idx].value;
	      		safe_snprintf(__FILE__, __LINE__, protobuf, 
				sizeof(protobuf), "%sBytes", protoList->protocolName);
	     
		        if(c > 0) cassandraUpdate(keyspace, protobuf, c, 0);
	            idx++, protoList = protoList->next;
	      }
	  }
	 }
    }
  }

  
  
 
  return(NULL);
}

/* ******************************/

static void
termCassandraFunct (u_char termNtop /* 0=term plugin,1=term ntop */ )
{

  int rc;
  active = 0;

  if (cassandraMainThread)
    {
      rc = killThread (&cassandraMainThread);
      if (rc == 0)
	traceEvent (CONST_TRACE_INFO,
		    "THREADMGMT[t%lu]: Cassandra: \
			killThread(rrdTrafficThread) succeeded",
		    (long unsigned int) pthread_self ());
      else
		traceEvent (CONST_TRACE_ERROR,
		    "THREADMGMT[t%lu]: Cassandra: \
			killThread(rrdTrafficThread) failed, rc %s(%d)",
		    (long unsigned int) pthread_self (), strerror (rc), rc);
    }
}

/* **************************************** */


/* Plugin entry fctn */
#ifdef MAKE_STATIC_PLUGIN
PluginInfo *
cassandraPluginEntryFctn (void)
#else
PluginInfo *
PluginEntryFctn (void)
#endif
{
  traceEvent (CONST_TRACE_ALWAYSDISPLAY,
	      "Cassandra: Welcome to %s.(C) 2002-12 by Luca Deri",
	      cassandraPluginInfo->pluginName);

  return (cassandraPluginInfo);
}

/* This must be here so it can access the struct PluginInfo, above */
static void
setPluginStatus (char *status)
{
  if (cassandraPluginInfo->pluginStatusMessage != NULL)
     free (cassandraPluginInfo->pluginStatusMessage);
  if (status == NULL)
    {
      cassandraPluginInfo->pluginStatusMessage = NULL;
    }
  else
    {
      cassandraPluginInfo->pluginStatusMessage = strdup (status);
    }
}

static int 
cassandraUpdate(char *dev, char *metric, Counter value, int isCounter)
{

 char cf[1024],
	  time_now[32], /* Unix timestamp 32 bit long */
	  col_val[32];/* len of long long int < 32 digits*/
 int ret;

 if (checkCassandra(Server_IP) == -1) {
      traceEvent(CONST_TRACE_ERROR, "THREADMGMT[t%lu]: CASSANDRA:"
                 "cassandra Server is not running", 
                 (long unsigned int)pthread_self());
	  totalFailWrite++;
      return -1;
 }

 safe_snprintf(__FILE__, __LINE__, time_now, sizeof(time_now),"%u", time(NULL));
 safe_snprintf(__FILE__, __LINE__, cf, sizeof(cf), "%s_%s", dev, metric);
 safe_snprintf(__FILE__, __LINE__, col_val, sizeof(col_val), "%llu", value);

 removeSymbols(cf);

 if (value != 0) {
 	ret = cassandraEasyInsert(Keyspace, cf, Server_IP, time_now, 
							  "value", col_val);
    if (ret == -1) {
		traceEvent(CONST_TRACE_INFO, "ERROR: CASSANDRA: cassandraUpdate(): %s",
				   getError());
		totalFailWrite++;
		return -1;
	}

	if (myGlobals.runningPref.debugMode)
		traceEvent(CONST_TRACE_INFO, "CASSANDRA: cassandraUpdate():"
				   " device :%s\n metric: %s\n value :%llu", 
					dev, metric, value);
 }
 totalWrite++;
 return 0;  
}


#ifdef HAVE_PYTHON
int 
parser(void )
{
 char path[512], 
      buffer[BUFF_SIZE],
      key[BUFF_SIZE],
      keyspace[BUFF_SIZE],
      strategy[BUFF_SIZE],
      replicationFactor[BUFF_SIZE];
 int fldRead = 0;
 sprintf(path, "%s/%s", myGlobals.pluginDirs[0],CASSANDRA_CONFIG_FILE);
 //config have to be in plugin dir.
 FILE *ifp = fopen(path, "r");
 
 if (ifp == NULL) {
   traceEvent (CONST_TRACE_ALWAYSDISPLAY,
				"CASSANDRA : configuration file not found");
   return -1;
 }

 while (!feof(ifp)) {
   if (fgets(buffer,BUFF_SIZE,ifp) == NULL && fldRead != 3) {
		return -1;
    }
   
   if (buffer[0] == '\n' || buffer[0] == '#')
     continue;
   
   if (!strncmp(buffer, "cassandraserver", 15)) {
     char server[BUFF_SIZE], port[BUFF_SIZE], temp[BUFF_SIZE];
     sscanf(buffer, "%[^:]:%[^:]:%[^\n]",temp, server, port);

     if (!strlen(temp) || !strlen(server) || !strlen(port))
		 return -1;

     sprintf(temp, "%s:%s", server, port);
     storePrefsValue("cassandra.serverport", temp);
	 Server_IP = strdup(temp);
     fldRead++;
   }

   if (!strncmp(buffer, "keyspace", 8)) {
     sscanf(buffer, "%[^:]:%[^:]:%[^:]:%[^\n]", key, keyspace, strategy, 
			replicationFactor);
     if (!strlen(key) || !strlen(keyspace) || !strlen(replicationFactor))
		return -1;

     storePrefsValue("cassandra.keyspace", keyspace);
     storePrefsValue("cassandra.strategy", strategy);
     storePrefsValue("cassandra.replicationFactor", replicationFactor);
     Keyspace = strdup(keyspace);
	 Strategy = strdup(strategy);
     ReplicationFactor = strdup(replicationFactor);
     fldRead++;
   }
   
   if ( !strncmp(buffer, "datadumpInterval", 16)) {
      char dump[16],
           dumptime[BUFF_SIZE];

      memset(dump, '\0', 16);
      memset(dumptime, '\0', BUFF_SIZE);
      sscanf(buffer, "%[^:]:%[^\n]", dump, dumptime);  
      if (!strlen(dump) || !strlen(dumptime))
		return -1;
      
	  CassaDumpInterval = atoi(dumptime);
      fldRead++;
    }
 }

 if (fldRead != 3)
   return -1;//Bad configuration file
 return 0;
}

#endif

int 
getDate(char *date, char*timestamp, int size, int dump)
{
  time_t timenow;
  struct tm *the_time;
  int delta;
  
  delta = dump*60*50*24;
  if ( date != NULL) {
    
    if (timestamp == NULL)
      timenow = time(NULL);
    else
    timenow = atol(timestamp);
    
    timenow = timenow - timenow%delta;
    
    the_time = localtime(&timenow);
    strftime(date, size, "%Y-%m-%d", the_time);
    return 0;
  }
  else
    return -1;
}

static int 
removeSymbols(char *string)
{
 int len, strIndex;
 if (string == NULL)
   return -1;
 
 len = strlen(string);
  
 for (strIndex = 0; strIndex < len; strIndex++) {
  
   if ( ((int)string[strIndex] >= 48 && (int)string[strIndex] <= 58) ||
        ((int)string[strIndex] >= 65&& (int)string[strIndex] <= 90)  ||
        ((int)string[strIndex] >= 97&& (int)string[strIndex] <= 122) )
    continue;
   else
     string[strIndex] = '_';
 }
 return 0;
}
