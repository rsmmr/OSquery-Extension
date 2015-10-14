## OSquery-Extension ##

This module has been tested with 
linux: CENTOS7 (3.10.0-229.11.1.el7.x86_64)

Note: actor framework version should be the same at both sides (bro and osquery side)

```singlequerysubscription.bro ``` , ```multiplequerysubscription.bro``` and ```loggingevents.bro``` are the sample script files; which are in https://github.com/sami2316/bro/tree/master/application directory.

-------------------------------------------------------
###Step 1: Follow Osquery Extension Guidelines###
-------------------------------------------------------
We have developed osquery extension which will enable bro users to subscribe to 
(single or group) SQL queries remotely and then get the queries updates till the
broker connection is alive. Once the SQL queries are received from bro then
extension will send an initial dump if the inidump flag is set to true;
otherwise, it will only monitor updates of events and send them to bro.


Broker is a communication library which is used as a communication module 
between osquery extension and bro IDS.

####1.1 Pre-Installation requirements: ####

Here follows the list of libraries requied to build extension
- broker 
- boost_thread
- thrift
- rocksdb
- boost_system
- crypto
- glog
- boost_filesystem
- thriftz
- osquery

Broker link:
```git clone --recursive https://github.com/bro/broker```

The rest of libraries will be readily available with the working osquery install. Clone the latest osquery from here: 
https://github.com/facebook/osquery/ 

####1.2 Installation Steps: ####

* ```git clone https://github.com/sami2316/OSquery-Extension.git```
*	```cd OSquery-Extension```
*	```make```
*	```make install```

####1.3 Application usage guide:####
* Change master IP and broker_topic in var/osquery/broker.ini
*	```osqueryd --extensions_autoload=/etc/osquery/extensions.load ```

-------------------------------------------------				
###Step 2: Follow Bro Extension Guideline###
-------------------------------------------------

We have added osquery query subscription module with a broker functionality in 
bro IDS. This module is about subscribing SQL queries from bro (master) to 
osquery hosts and then receiving updates of subscribed events. 
Default subscription behavior is for update events only but you can request an 
initial dump by setting inidump
flag to true during the subscription process. 

This module enables following modes of connections and monitoring:
*  A master to a single remote host monitoring with a single query subscription
*  A master to a single remote host monitoring with multiple queries subscription
*  A master to a remote group of hosts monitoring with a single query subscription
*  A master to a remote group of hosts monitoring with multiple queries subscription

####2.1 Installation steps:####
*	install actor-framework from github
*	```git clone --recursive https://github.com/sami2316/bro```
*	```./configure```
*	```make```
*	```make install```

Note: actor framework version should be the same at both sides (bro and 
       osquery side)

----------------------------------------------
###Step 3: Start Using Monitoring Application###
----------------------------------------------

####3.1 Scenario 1: A master to a single remote host monitoring with a single query subscription####

First you need to run osqueryd on both hosts. Then at bro side write the 
following script to subscribe to a single query. An example script, extracted 
from singlequerysubscription.bro, to monitor usb_devices is given below:

```
module osquery;

const broker_port: port = 9999/tcp &redef;
redef exit_only_after_terminate = T;
redef osquery::endpoint_name = "Printer";


global usb_devices: event(host: string, user: string, ev_type: string, usb_address: int, vendor: string, model: string);

event bro_init()
{
	osquery::enable();
	osquery::subscribe_to_events("/bro/event/");
	osquery::listen(broker_port,"192.168.0.120"); 
}

event BrokerComm::incoming_connection_established(peer_name: string)
{
	print "BrokerComm::incoming_connection_establisted",  peer_name;
	
	#if we are interested in new usb_devices then
	osquery::subscribe(usb_devices,"SELECT usb_address,vendor,model FROM usb_devices","ADD");
	#if we are interested in removed usb_devices then
	#osquery::subscribe(usb_devices,"SELECT usb_address,vendor,model FROM usb_devices","REMOVED");
	#if you want an initial dump for the requrest query then set inidumpflag to True
	#osquery::subscribe(usb_devices,"SELECT usb_address,vendor,model FROM usb_devices","Removed",T);

}

event BrokerComm::incoming_connection_broken(peer_name: string)
{
	print "BrokerComm::incoming_connection_broken", peer_name;

}

############################# USB DEVICES ###################################################
event usb_devices(host: string, user: string, ev_type: string, usb_address: int, vendor: string, model: string)
{
	print "usb_devices Entry";
 	print fmt("Host = %s user=%s Event_type= %s Usb_address = %d Vendor = %s Model = %s",host, user, ev_type, usb_address, vendor, model);
}
```
Please refer to singlequerysubscription.bro to write scripts to monitor other events.

####3.2 Scenario 2: A master to a single remote host monitoring with multiple queries subscription####
An example script for multiple queries subscription, extracted from multiplequerysubscription.bro,
to monitor multiple OS events is given below:

```
module osquery;

const broker_port: port = 9999/tcp &redef;
redef exit_only_after_terminate = T;
redef osquery::endpoint_name = "Printer";

global acpi_tables: event(host: string, user: string, ev_type: string, name: string, size: int, md5: string);

###################################################################################
global arp_cache: event(host: string, user: string, ev_type: string, address: string, mac: string, interface: string);

###################################################################################
global block_devices: event(host: string, user: string, ev_type: string,name: string,vendor: string, model: string);

global query: table[string] of string;

event bro_init()
{
	osquery::enable();
	osquery::subscribe_to_events("/bro/event/");
	osquery::listen(broker_port,"192.168.0.120");
	
	query["osquery::acpi_tables"] = "SELECT name,size,md5 FROM acpi_tables";

	#######################################################################################
	#query["osquery::arp_cache"] = "SELECT address,mac,interface FROM arp_cache";

	#######################################################################################
	#query["osquery::block_devices"] =  "SELECT name,vendor,model FROM block_devices";
}

event BrokerComm::incoming_connection_established(peer_name: string)
{
	print "BrokerComm::incoming_connection_establisted",  peer_name;
	
	#if we are interested in new events
	osquery::groupsubscribe("/bro/event/",query,"ADD");
	#if we are interested in removed events
	osquery::groupsubscribe("/bro/event/",query,"REMOVED");
	#if we are interested in initial dump as well
	osquery::groupsubscribe("/bro/event/",query,"ADD",T);
	
}

event BrokerComm::incoming_connection_broken(peer_name: string)
{
	print "BrokerComm::incoming_connection_broken", peer_name;
	
}

########################### ACPI TABLES ################################################

event acpi_tables(host: string, user: string, ev_type: string, name: string, size: int, md5: string)
{
	print "acpi_table Entry";
	print fmt("Host = %s user=%s Event_type= %s Table_name = %s size = %d md5 = %s",host, user, ev_type, name, size, md5);
}

############################## ARP CACHE ##############################################
event arp_cache(host: string, user: string, ev_type: string, address: string, mac: string, interface: string)
{
	print "arp_cache Entry";
	print fmt("Host = %s user=%s Event_type= %s Address = %s mac = %s Interface = %s",host, user, ev_type, address, mac, interface);
}

############################## BLOCK DEVICES ###########################################
event block_devices(host: string, user: string, ev_type: string, name: string,vendor: string, model: string)
{
	print "block_devices Entry";
	print fmt("Host = %s user=%s Event_type= %s Name = %s Vendor = %s Model = %s",host, user, ev_type, name, vendor, model);
}
```
Please refer to multiplequerysubscription.bro to have a look at the scripts written to monitor other
events.

####3.3 Scenario 3: A master to a remote group of hosts monitoring with a single query subscription####
Make sure the broker.ini at each osquery host in a group has the same broker_topic. In our example, we are using 
"broker_topic=/bro/event/group1"
An example script for a group of connections and single query subscription,
to monitor usb_devices is given below:

```
module osquery;

const broker_port: port = 9999/tcp &redef;
redef exit_only_after_terminate = T;
redef osquery::endpoint_name = "Printer";


global usb_devices: event(host: string, user: string, ev_type: string, usb_address: int, vendor: string, model: string);

event bro_init()
{
	osquery::enable();
	osquery::subscribe_to_events("/bro/event/");
	osquery::listen(broker_port,"192.168.0.120"); 
}

event BrokerComm::incoming_connection_established(peer_name: string)
{
	print "BrokerComm::incoming_connection_establisted",  peer_name;
	
	#if we are interested in new usb_devices then
	osquery::subscribe(usb_devices,"SELECT usb_address,vendor,model FROM usb_devices","ADD");
	#if we are interested in removed usb_devices then
	#osquery::subscribe(usb_devices,"SELECT usb_address,vendor,model FROM usb_devices","REMOVED");
	#if you want an initial dump for the requrest query then set inidumpflag to True
	#osquery::subscribe(usb_devices,"SELECT usb_address,vendor,model FROM usb_devices","Removed",F,"/bro/event/group1");
	##"bro/event/group1" is the topic mapped to group. 

}

event BrokerComm::incoming_connection_broken(peer_name: string)
{
	print "BrokerComm::incoming_connection_broken", peer_name;

}

############################# USB DEVICES ###################################################
event usb_devices(host: string, user: string, ev_type: string, usb_address: int, vendor: string, model: string)
{
	print "usb_devices Entry";
 	print fmt("Host = %s user=%s Event_type= %s Usb_address = %d Vendor = %s Model = %s",host, user, ev_type, usb_address, vendor, model);
}
```
For multiple groups, you need to change the broker.ini at each osquery host to make it a part of specific group. For example, if there are three hosts and we wana make three groups, simply update broker.ini on each host with different 
broker_topic "/bro/event/group1", "/bro/event/group2", "/bro/event/group3" respectively.
Then subscribe different queries on each host in BrokerComm::outgoing_connection_established event body.

```
event BrokerComm::outgoing_connection_established(peer_address: string, peer_port: port, peer_name: string)
{
	print "BrokerComm::outgoing_connection_establisted", 	peer_address, 
        peer_port, peer_name;

		#osquery::subscribe(usb_devices,"SELECT usb_address,vendor,model FROM usb_devices","Removed",F,"/bro/event/group1");
		#osquery::subscribe(users,"SELECT usb_address,vendor,model FROM usb_devices","Removed",F,"/bro/event/group2");
		#osquery::subscribe(last,"SELECT usb_address,vendor,model FROM usb_devices","Removed",F,"/bro/event/group3");

}
```
####3.4 Scenario 4: A master to a remote group of hosts monitoring with multiple queries subscription####
Make sure the broker.ini at each osquery host in a group has the same broker_topic. In our example, we are using 
"broker_topic=/bro/event/group1"
An example script for a group of connection and multiple queries subscription,
to monitor multiple OS events is given below:

```
module osquery;

const broker_port: port = 9999/tcp &redef;
redef exit_only_after_terminate = T;
redef osquery::endpoint_name = "Printer";

global acpi_tables: event(host: string, user: string, ev_type: string, name: string, size: int, md5: string);

###################################################################################
global arp_cache: event(host: string, user: string, ev_type: string, address: string, mac: string, interface: string);

###################################################################################
global block_devices: event(host: string, user: string, ev_type: string,name: string,vendor: string, model: string);

global query: table[string] of string;

event bro_init()
{
	osquery::enable();
	osquery::subscribe_to_events("/bro/event/");
	osquery::listen(broker_port,"192.168.0.120");
	
	query["osquery::acpi_tables"] = "SELECT name,size,md5 FROM acpi_tables";

	#######################################################################################
	query["osquery::arp_cache"] = "SELECT address,mac,interface FROM arp_cache";

	#######################################################################################
	#query["osquery::block_devices"] =  "SELECT name,vendor,model FROM block_devices";
}

event BrokerComm::incoming_connection_established(peer_name: string)
{
	print "BrokerComm::incoming_connection_establisted",  peer_name;
	
	#if we are interested in new events
	osquery::groupsubscribe("/bro/event/group1",query,"ADD");
	#if we are interested in removed events
	osquery::groupsubscribe("/bro/event/group1",query,"REMOVED");
	#if we are interested in initial dump as well
	osquery::groupsubscribe("/bro/event/group1",query,"ADD",T);
	
}

event BrokerComm::incoming_connection_broken(peer_name: string)
{
	print "BrokerComm::incoming_connection_broken", peer_name;
	
}

########################### ACPI TABLES ################################################

event acpi_tables(host: string, user: string, ev_type: string, name: string, size: int, md5: string)
{
	print "acpi_table Entry";
	print fmt("Host = %s user=%s Event_type= %s Table_name = %s size = %d md5 = %s",host, user, ev_type, name, size, md5);
}

############################## ARP CACHE ##############################################
event arp_cache(host: string, user: string, ev_type: string, address: string, mac: string, interface: string)
{
	print "arp_cache Entry";
	print fmt("Host = %s user=%s Event_type= %s Address = %s mac = %s Interface = %s",host, user, ev_type, address, mac, interface);
}

############################## BLOCK DEVICES ###########################################
event block_devices(host: string, user: string, ev_type: string, name: string,vendor: string, model: string)
{
	print "block_devices Entry";
	print fmt("Host = %s user=%s Event_type= %s Name = %s Vendor = %s Model = %s",host, user, ev_type, name, vendor, model);
}
```

For multiple groups, you need to change the broker.ini at each osquery host to make it a part of specific group. For example, if there are three hosts and we wana make three groups, simply update broker.ini on each host with different 
broker_topic "/bro/event/group1", "/bro/event/group2", "/bro/event/group3" respectively.
And also define three different query tables e.g. query1, query2, query3, respectively.
Then subscribe different group of queries on each group in BrokerComm::outgoing_connection_established event body.
```
event BrokerComm::outgoing_connection_established(peer_address: string, peer_port: port, peer_name: string)
{
	print "BrokerComm::outgoing_connection_establisted", 	peer_address, peer_port, peer_name;
	osquery::groupsubscribe("/bro/event/group1",query1);
	osquery::groupsubscribe("/bro/event/group2",query2);
	osquery::groupsubscribe("/bro/event/group3",query3);
```
----------------------------------------------
###Step 4: Error Handling and Logging###
----------------------------------------------
####4.1 Error Handling####
To reveive erros and warning from osquery side; just add the following events in bro-script
```
global warning: event(warning_msg: string);
global error: event(error_msg: string);

############################# Warning and Errors #########################################
event warning(warning_msg: string)
{
	print fmt("Warning:    %s ", warning_msg);
}

event error(error_msg: string)
{
	print fmt(" %s ", error_msg);
}
###################################################################################
```
####4.2 logging ####
Logging example srcipt is in ```loggingevent.bro```
Please refer to the above mentioned file for detailed example.
