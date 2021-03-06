/*
    PowerDNS Versatile Database Driven Nameserver
    Copyright (C) 2005 - 2013  PowerDNS.COM BV

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License version 2
    as published by the Free Software Foundation

    Additionally, the license of this program contains a special
    exception which allows to distribute the program in binary form when
    it is linked against OpenSSL.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/
#include "common_startup.hh"
#include "ws-auth.hh"
#include "secpoll-auth.hh"

bool g_anyToTcp;
typedef Distributor<DNSPacket,DNSPacket,PacketHandler> DNSDistributor;

ArgvMap theArg;
StatBag S;  //!< Statistics are gathered across PDNS via the StatBag class S
PacketCache PC; //!< This is the main PacketCache, shared across all threads
DNSProxy *DP;
DynListener *dl;
CommunicatorClass Communicator;
UDPNameserver *N;
int avg_latency;
TCPNameserver *TN;

ArgvMap &arg()
{
  return theArg;
}

void declareArguments()
{
  ::arg().set("local-port","The port on which we listen")="53";
  ::arg().setSwitch("experimental-dnsupdate","Enable/Disable DNS update (RFC2136) support. Default is no.")="no";
  ::arg().set("allow-dnsupdate-from","A global setting to allow DNS updates from these IP ranges.")="127.0.0.0/8,::1";
  ::arg().setSwitch("forward-dnsupdate","A global setting to allow DNS update packages that are for a Slave domain, to be forwarded to the master.")="yes";
  ::arg().setSwitch("log-dns-details","If PDNS should log DNS non-erroneous details")="no";
  ::arg().setSwitch("log-dns-queries","If PDNS should log all incoming DNS queries")="no";
  ::arg().set("local-address","Local IP addresses to which we bind")="0.0.0.0";
  ::arg().setSwitch("local-address-nonexist-fail","Fail to start if one or more of the local-address's do not exist on this server")="yes";
  ::arg().set("local-ipv6","Local IP address to which we bind")="";
  ::arg().setSwitch("reuseport","Enable higher performance on compliant kernels by using SO_REUSEPORT allowing each receiver thread to open its own socket")="no";
  ::arg().setSwitch("local-ipv6-nonexist-fail","Fail to start if one or more of the local-ipv6 addresses do not exist on this server")="yes";
  ::arg().set("query-local-address","Source IP address for sending queries")="0.0.0.0";
  ::arg().set("query-local-address6","Source IPv6 address for sending queries")="::";
  ::arg().set("overload-queue-length","Maximum queuelength moving to packetcache only")="0";
  ::arg().set("max-queue-length","Maximum queuelength before considering situation lost")="5000";

  ::arg().set("retrieval-threads", "Number of AXFR-retrieval threads for slave operation")="2";
  ::arg().setSwitch("experimental-json-interface", "If the webserver should serve JSON data")="no";
  ::arg().setSwitch("experimental-api-readonly", "If the JSON API should disallow data modification")="no";
  ::arg().set("experimental-api-key", "REST API Static authentication key (required for API use)")="";
  ::arg().setSwitch("experimental-dname-processing", "If we should support DNAME records")="no";

  ::arg().setCmd("help","Provide a helpful message");
  ::arg().setCmd("version","Output version and compilation date");
  ::arg().setCmd("config","Provide configuration file on standard output");
  ::arg().setCmd("list-modules","Lists all modules available");
  ::arg().setCmd("no-config","Don't parse configuration file");
  
  ::arg().set("version-string","PowerDNS version in packets - full, anonymous, powerdns or custom")="full"; 
  ::arg().set("control-console","Debugging switch - don't use")="no"; // but I know you will!
  ::arg().set("loglevel","Amount of logging. Higher is more. Do not set below 3")="4";
  ::arg().set("default-soa-name","name to insert in the SOA record if none set in the backend")="a.misconfigured.powerdns.server";
  ::arg().set("default-soa-mail","mail address to insert in the SOA record if none set in the backend")="";
  ::arg().set("distributor-threads","Default number of Distributor (backend) threads to start")="3";
  ::arg().set("signing-threads","Default number of signer threads to start")="3";
  ::arg().set("receiver-threads","Default number of receiver threads to start")="1";
  ::arg().set("queue-limit","Maximum number of milliseconds to queue a query")="1500"; 
  ::arg().set("recursor","If recursion is desired, IP address of a recursing nameserver")="no"; 
  ::arg().set("allow-recursion","List of subnets that are allowed to recurse")="0.0.0.0/0";
  ::arg().set("pipebackend-abi-version","Version of the pipe backend ABI")="1";
  ::arg().set("udp-truncation-threshold", "Maximum UDP response size before we truncate")="1680";
  ::arg().set("disable-tcp","Do not listen to TCP queries")="no";
  
  ::arg().set("config-name","Name of this virtual configuration - will rename the binary image")="";

  ::arg().set("load-modules","Load this module - supply absolute or relative path")="";
  ::arg().set("launch","Which backends to launch and order to query them in")="";
  ::arg().setSwitch("disable-axfr","Disable zonetransfers but do allow TCP queries")="no";
  ::arg().set("allow-axfr-ips","Allow zonetransfers only to these subnets")="127.0.0.0/8,::1";
  ::arg().set("only-notify", "Only send AXFR NOTIFY to these IP addresses or netmasks")="0.0.0.0/0,::/0";
  ::arg().set("also-notify", "When notifying a domain, also notify these nameservers")="";
  ::arg().set("slave-cycle-interval","Reschedule failed SOA serial checks once every .. seconds")="60";

  ::arg().set("tcp-control-address","If set, PowerDNS can be controlled over TCP on this address")="";
  ::arg().set("tcp-control-port","If set, PowerDNS can be controlled over TCP on this address")="53000";
  ::arg().set("tcp-control-secret","If set, PowerDNS can be controlled over TCP after passing this secret")="";
  ::arg().set("tcp-control-range","If set, remote control of PowerDNS is possible over these networks only")="127.0.0.0/8, 10.0.0.0/8, 192.168.0.0/16, 172.16.0.0/12, ::1/128, fe80::/10";
  
  ::arg().setSwitch("slave","Act as a slave")="no";
  ::arg().setSwitch("master","Act as a master")="no";
  ::arg().setSwitch("disable-axfr-rectify","Disable the rectify step during an outgoing AXFR. Only required for regression testing.")="no";
  ::arg().setSwitch("guardian","Run within a guardian process")="no";
  ::arg().setSwitch("send-root-referral","Send out old-fashioned root-referral instead of ServFail in case of no authority")="no";
  ::arg().setSwitch("prevent-self-notification","Don't send notifications to what we think is ourself")="yes";
  ::arg().setSwitch("webserver","Start a webserver for monitoring")="no"; 
  ::arg().setSwitch("webserver-print-arguments","If the webserver should print arguments")="no"; 
  ::arg().setSwitch("edns-subnet-processing","If we should act on EDNS Subnet options")="no"; 
  ::arg().setSwitch("any-to-tcp","Answer ANY queries with tc=1, shunting to TCP")="no"; 
  ::arg().set("webserver-address","IP Address of webserver to listen on")="127.0.0.1";
  ::arg().set("webserver-port","Port of webserver to listen on")="8081";
  ::arg().set("webserver-password","Password required for accessing the webserver")="";
  ::arg().set("webserver-allow-from","Webserver access is only allowed from these subnets")="0.0.0.0/0,::/0";

  ::arg().setSwitch("out-of-zone-additional-processing","Do out of zone additional processing")="yes";
  ::arg().setSwitch("do-ipv6-additional-processing", "Do AAAA additional processing")="yes";
  ::arg().setSwitch("query-logging","Hint backends that queries should be logged")="no";

  ::arg().set("carbon-ourname", "If set, overrides our reported hostname for carbon stats")="";
  ::arg().set("carbon-server", "If set, send metrics in carbon (graphite) format to this server")="";
  ::arg().set("carbon-interval", "Number of seconds between carbon (graphite) updates")="30";

  ::arg().set("cache-ttl","Seconds to store packets in the PacketCache")="20";
  ::arg().set("recursive-cache-ttl","Seconds to store packets for recursive queries in the PacketCache")="10";
  ::arg().set("negquery-cache-ttl","Seconds to store negative query results in the QueryCache")="60";
  ::arg().set("query-cache-ttl","Seconds to store query results in the QueryCache")="20";
  ::arg().set("soa-minimum-ttl","Default SOA minimum ttl")="3600";
  ::arg().set("server-id", "Returned when queried for 'server.id' TXT or NSID, defaults to hostname - disabled or custom")="";
  ::arg().set("soa-refresh-default","Default SOA refresh")="10800";
  ::arg().set("soa-retry-default","Default SOA retry")="3600";
  ::arg().set("soa-expire-default","Default SOA expire")="604800";

  ::arg().set("trusted-notification-proxy", "IP address of incoming notification proxy")="";
  ::arg().set("slave-renotify", "If we should send out notifications for slaved updates")="no";

  ::arg().set("default-ttl","Seconds a result is valid if not set otherwise")="3600";
  ::arg().set("max-tcp-connections","Maximum number of TCP connections")="10";
  ::arg().setSwitch("no-shuffle","Set this to prevent random shuffling of answers - for regression testing")="off";

  ::arg().set("experimental-logfile", "Filename of the log file for JSON parser" )= "/var/log/pdns.log";
  ::arg().set("setuid","If set, change user id to this uid for more security")="";
  ::arg().set("setgid","If set, change group id to this gid for more security")="";

  ::arg().set("max-cache-entries", "Maximum number of cache entries")="1000000";
  ::arg().set("max-signature-cache-entries", "Maximum number of signatures cache entries")="";
  ::arg().set("max-ent-entries", "Maximum number of empty non-terminals in a zone")="100000";
  ::arg().set("entropy-source", "If set, read entropy from this file")="/dev/urandom";

  ::arg().set("lua-prequery-script", "Lua script with prequery handler")="";

  ::arg().setSwitch("traceback-handler","Enable the traceback handler (Linux only)")="yes";
  ::arg().setSwitch("direct-dnskey","Fetch DNSKEY RRs from backend during DNSKEY synthesis")="no";
  ::arg().set("default-ksk-algorithms","Default KSK algorithms")="rsasha256";
  ::arg().set("default-ksk-size","Default KSK size (0 means default)")="0";
  ::arg().set("default-zsk-algorithms","Default ZSK algorithms")="rsasha256";
  ::arg().set("default-zsk-size","Default ZSK size (0 means default)")="0";
  ::arg().set("max-nsec3-iterations","Limit the number of NSEC3 hash iterations")="500"; // RFC5155 10.3

  ::arg().set("include-dir","Include *.conf files from this directory");
  ::arg().set("security-poll-suffix","Domain name from which to query security update notifications")="secpoll.powerdns.com.";
}

void declareStats(void)
{
  S.declare("udp-queries","Number of UDP queries received");
  S.declare("udp-do-queries","Number of UDP queries received with DO bit");
  S.declare("udp-answers","Number of answers sent out over UDP");
  S.declare("udp-answers-bytes","Total size of answers sent out over UDP");

  S.declare("udp4-answers","Number of IPv4 answers sent out over UDP");
  S.declare("udp4-queries","Number of IPv4 UDP queries received");
  S.declare("udp6-answers","Number of IPv6 answers sent out over UDP");
  S.declare("udp6-queries","Number of IPv6 UDP queries received");

  S.declare("rd-queries", "Number of recursion desired questions");
  S.declare("recursion-unanswered", "Number of packets unanswered by configured recursor");
  S.declare("recursing-answers","Number of recursive answers sent out");
  S.declare("recursing-questions","Number of questions sent to recursor");
  S.declare("corrupt-packets","Number of corrupt packets received");

  S.declare("tcp-queries","Number of TCP queries received");
  S.declare("tcp-answers","Number of answers sent out over TCP");

  S.declare("qsize-q","Number of questions waiting for database attention");

  S.declare("deferred-cache-inserts","Amount of cache inserts that were deferred because of maintenance");
  S.declare("deferred-cache-lookup","Amount of cache lookups that were deferred because of maintenance");

  S.declare("query-cache-hit","Number of hits on the query cache");
  S.declare("query-cache-miss","Number of misses on the query cache");

  S.declare("dnsupdate-queries", "DNS update packets received.");
  S.declare("dnsupdate-answers", "DNS update packets successfully answered.");
  S.declare("dnsupdate-refused", "DNS update packets that are refused.");
  S.declare("dnsupdate-changes", "DNS update changes to records in total.");

  S.declare("servfail-packets","Number of times a server-failed packet was sent out");
  S.declare("latency","Average number of microseconds needed to answer a question");
  S.declare("timedout-packets","Number of packets which weren't answered within timeout set");
  S.declare("security-status", "Security status based on regular polling");
  S.declareRing("queries","UDP Queries Received");
  S.declareRing("nxdomain-queries","Queries for non-existent records within existent domains");
  S.declareRing("noerror-queries","Queries for existing records, but for type we don't have");
  S.declareRing("servfail-queries","Queries that could not be answered due to backend errors");
  S.declareRing("unauth-queries","Queries for domains that we are not authoritative for");
  S.declareRing("logmessages","Log Messages");
  S.declareRing("remotes","Remote server IP addresses");
  S.declareRing("remotes-unauth","Remote hosts querying domains for which we are not auth");
  S.declareRing("remotes-corrupt","Remote hosts sending corrupt packets");

}


int isGuarded(char **argv)
{
  char *p=strstr(argv[0],"-instance");

  return !!p;
}

void sendout(const AnswerData<DNSPacket> &AD)
{
  if(!AD.A)
    return;
  
  N->send(AD.A);

  int diff=AD.A->d_dt.udiff();
  avg_latency=(int)(1023*avg_latency/1024+diff/1024);

  delete AD.A;  
}

//! The qthread receives questions over the internet via the Nameserver class, and hands them to the Distributor for further processing
void *qthread(void *number)
{
  DNSPacket *P;
  DNSDistributor *distributor = DNSDistributor::Create(::arg().asNum("distributor-threads", 1)); // the big dispatcher!
  DNSPacket question;
  DNSPacket cached;

  unsigned int &numreceived=*S.getPointer("udp-queries");
  unsigned int &numreceiveddo=*S.getPointer("udp-do-queries");

  unsigned int &numreceived4=*S.getPointer("udp4-queries");

  unsigned int &numreceived6=*S.getPointer("udp6-queries");

  int diff;
  bool logDNSQueries = ::arg().mustDo("log-dns-queries");
  bool doRecursion = ::arg().mustDo("recursor");
  bool skipfirst=true;
  unsigned int maintcount = 0;
  UDPNameserver *NS = N;

  // If we have SO_REUSEPORT then create a new port for all receiver threads
  // other than the first one.
  if( number != NULL && NS->canReusePort() ) {
    L<<Logger::Notice<<"Starting new listen thread on the same IPs/ports using SO_REUSEPORT"<<endl;
    try {
      NS = new UDPNameserver( true );
    } catch(PDNSException &e) {
      L<<Logger::Error<<"Unable to reuse port, falling back to original bind"<<endl;
      NS = N;
    }
  }

  for(;;) {
    if (skipfirst)
      skipfirst=false;
    else  
      numreceived++;

    if(number==0) { // only run on main thread
      if(!((maintcount++)%250)) { // maintenance tasks - this can conceivably run infrequently
        S.set("latency",(int)avg_latency);
        int qcount, acount;
        distributor->getQueueSizes(qcount, acount);  // this does locking and other things, so don't get smart
        S.set("qsize-q",qcount);
      }
    }

    if(!(P=NS->receive(&question))) { // receive a packet         inline
      continue;                    // packet was broken, try again
    }

    if(P->d_remote.getSocklen()==sizeof(sockaddr_in))
      numreceived4++;
    else
      numreceived6++;

    if(P->d_dnssecOk)
      numreceiveddo++;

     if(P->d.qr)
       continue;

    S.ringAccount("queries", P->qdomain+"/"+P->qtype.getName());
    S.ringAccount("remotes",P->getRemote());
    if(logDNSQueries) {
      string remote;
      if(P->hasEDNSSubnet()) 
        remote = P->getRemote() + "<-" + P->getRealRemote().toString();
      else
        remote = P->getRemote();
      L << Logger::Notice<<"Remote "<< remote <<" wants '" << P->qdomain<<"|"<<P->qtype.getName() << 
            "', do = " <<P->d_dnssecOk <<", bufsize = "<< P->getMaxReplyLen()<<": ";
    }

    if((P->d.opcode != Opcode::Notify && P->d.opcode != Opcode::Update) && P->couldBeCached()) {
      bool haveSomething = false;
      if (doRecursion && P->d.rd && DP->recurseFor(P))
        haveSomething=PC.get(P, &cached, true); // does the PacketCache recognize this ruestion (recursive)?
      if (!haveSomething)
        haveSomething=PC.get(P, &cached, false); // does the PacketCache recognize this question?
      if (haveSomething) {
        if(logDNSQueries)
          L<<"packetcache HIT"<<endl;
        cached.setRemote(&P->d_remote);  // inlined
        cached.setSocket(P->getSocket());                               // inlined
        cached.d_anyLocal = P->d_anyLocal;
        cached.setMaxReplyLen(P->getMaxReplyLen());
        cached.d.rd=P->d.rd; // copy in recursion desired bit
        cached.d.id=P->d.id;
        cached.commitD(); // commit d to the packet                        inlined

        NS->send(&cached);   // answer it then                              inlined
        diff=P->d_dt.udiff();
        avg_latency=(int)(0.999*avg_latency+0.001*diff); // 'EWMA'

        continue;
      }
    }
    
    if(distributor->isOverloaded()) {
      if(logDNSQueries) 
        L<<"Dropped query, db is overloaded"<<endl;
      continue;
    }
        
    if(logDNSQueries) 
      L<<"packetcache MISS"<<endl;

    distributor->question(P, &sendout); // otherwise, give to the distributor
  }
  return 0;
}

void mainthread()
{
  Utility::srandom(time(0));

   int newgid=0;      
   if(!::arg()["setgid"].empty()) 
     newgid=Utility::makeGidNumeric(::arg()["setgid"]);      
   int newuid=0;      
   if(!::arg()["setuid"].empty())        
     newuid=Utility::makeUidNumeric(::arg()["setuid"]); 
   
   g_anyToTcp = ::arg().mustDo("any-to-tcp");

   DNSPacket::s_udpTruncationThreshold = std::max(512, ::arg().asNum("udp-truncation-threshold"));
   DNSPacket::s_doEDNSSubnetProcessing = ::arg().mustDo("edns-subnet-processing");

   doSecPoll(true); // this must be BEFORE chroot

   if(!::arg()["chroot"].empty()) {  
     if(::arg().mustDo("master") || ::arg().mustDo("slave"))
        gethostbyname("a.root-servers.net"); // this forces all lookup libraries to be loaded
     Utility::dropGroupPrivs(newuid, newgid);
     if(chroot(::arg()["chroot"].c_str())<0 || chdir("/")<0) {
       L<<Logger::Error<<"Unable to chroot to '"+::arg()["chroot"]+"': "<<strerror(errno)<<", exiting"<<endl; 
       exit(1);
     }   
     else
       L<<Logger::Error<<"Chrooted to '"<<::arg()["chroot"]<<"'"<<endl;      
   } else {
     Utility::dropGroupPrivs(newuid, newgid);
   }

  AuthWebServer webserver;
  Utility::dropUserPrivs(newuid);

  if(::arg().mustDo("recursor")){
    DP=new DNSProxy(::arg()["recursor"]);
    DP->onlyFrom(::arg()["allow-recursion"]);
    DP->go();
  }
  // NOW SAFE TO CREATE THREADS!
  dl->go();

  pthread_t qtid;

  if(::arg().mustDo("webserver"))
    webserver.go();

  if(::arg().mustDo("slave") || ::arg().mustDo("master"))
    Communicator.go(); 

  if(TN)
    TN->go(); // tcp nameserver launch

  pthread_create(&qtid,0,carbonDumpThread, 0); // runs even w/o carbon, might change @ runtime    

  //  fork(); (this worked :-))
  unsigned int max_rthreads= ::arg().asNum("receiver-threads", 1);
  for(unsigned int n=0; n < max_rthreads; ++n)
    pthread_create(&qtid,0,qthread, reinterpret_cast<void *>(n)); // receives packets

  for(;;) {
    sleep(1800);
    try {
      doSecPoll(false);
    }
    catch(...){}
  }
  
  L<<Logger::Error<<"Mainthread exiting - should never happen"<<endl;
}




