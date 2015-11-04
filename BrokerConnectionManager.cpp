/* 
 *  Copyright (c) 2015, Next Generation Intelligent Networks (nextGIN), RC.
 *  Institute of Space Technology
 *  All rights reserved.
 * 
 *  This source code is licensed under the BSD-style license found in the
 *  LICENSE file in the root directory of this source tree. An additional grant
 *  of patent rights can be found in the PATENTS file in the same directory.
 *
 */

#include "BrokerConnectionManager.h"




BrokerConnectionManager::BrokerConnectionManager(std::string hostName,
        std::string btp,int bport)
{
    if ( ! btp.size() || btp[btp.size() - 1] != '/' )
	btp += "/";

    btp += "host/" + hostName;

    //initialize broker API
    broker::init();
    this->b_port = bport;
    this->connected = false;
    //local host object
    this->ptlocalhost = new broker::endpoint(hostName);
    // broker messages queue
    this->ptmq = new broker::message_queue(btp,*ptlocalhost);
    // pooling for message queue
    ptpfd = new pollfd{this->ptmq->fd(), POLLIN, 0};
    // Query Manager Object
    this->qm = new BrokerQueryManager(ptlocalhost,ptmq,btp);
}

BrokerConnectionManager::~BrokerConnectionManager()
{
    // query manager object deletion
    delete this->qm;
    //local host object deletion
    delete this->ptlocalhost;
    // pooling object linked with message queue deletion
    delete this->ptpfd;
    // message queue deletion
    delete this->ptmq;
}

bool BrokerConnectionManager::listenForBrokerConnection()
{
    LOG(WARNING) <<"listening for new Connection";
    this->connected = false;
    //listen for new connection. wait until at-least one connection is found
    ptlocalhost->listen(b_port,getLocalHostIp().c_str());
    //pop new connection request
    auto conn_status = 
    this->ptlocalhost->incoming_connection_status().need_pop();
    for(auto cs: conn_status)
    {
        if(cs.status == broker::incoming_connection_status::tag::established)
        {
            LOG(WARNING) <<"Connection Established"<<std::endl;
            this->connected = true;
            break;
        }
    }
    return this->connected;
}

bool BrokerConnectionManager::connectToMaster(std::string master_ip,
        std::chrono::duration<double> retry_interval, SignalHandler* handler)
{
    LOG(WARNING) <<"Connecting to Master at "<<master_ip ;
    this->connected = false;
    this->peer = ptlocalhost->peer(master_ip,b_port);
    
    while(!connected && !(handler->gotExitSignal()))
    {
        auto conn_status = 
        this->ptlocalhost->outgoing_connection_status().want_pop();

        for(auto cs: conn_status)
        {
            if(cs.status == broker::outgoing_connection_status::tag::established)
            {
                LOG(WARNING) <<"Connection Established";
                this->connected = true;
                break;
            }
        }
    }
    return (!handler->gotExitSignal())? true: false;
}

int BrokerConnectionManager::getAndSetTopic()
{ 
    //get topic form message queue
    std::string temp = qm->getBrokerTopic(this->ptpfd,connected);
    
    if (temp.empty())
        return -1;
    
    delete this->ptpfd;
    // pooling for message queue
    ptpfd = new pollfd{this->ptmq->fd(), POLLIN, 0};
    delete this->qm;
    this->qm = NULL;
    this->qm = new BrokerQueryManager(ptlocalhost,ptmq,temp);
    
    qm->sendReadytoBro();
    
    return (this->isConnectionAlive())?1:0;
}

bool BrokerConnectionManager::getAndProcessQuery()
{
    //get queries form message queue
    bool temp = qm->getQueriesFromBrokerMessage(this->ptpfd,connected);
    //if success
    if(temp)
    {
        //then extract columns form query strings
      temp = qm->queryColumnExtractor();
    }
    else
    {
        //send warning to bro.
        qm->sendWarningtoBro("No SQL query Registered... or"
                " query was unformated");
        ptlocalhost->unpeer(this->peer);
        this->connected = false;
        return false;
    }
    // extract event add/removed/both form event part if success
    if(qm->getEventsFromBrokerMessage())
    {
        // then fill the out_query_vector with query data
        temp = qm->queryDataResultVectorInit();
    }
    else
    {
        qm->sendErrortoBro("* is unexpected write columns instead");
        return false;
    }
    return temp;
}

int BrokerConnectionManager::trackResponseChangesAndSendResponseToMaster(
                    SignalHandler *handle)
{
    int local;
    //send a pointer to signal handler object created in main.cpp
    qm->setSignalHandle(handle);
    // start tracking updates
    qm->queriesUpdateTrackingHandler();
    //check for new subscription messages
    local = qm->getLaterSubscriptionEvents(ptpfd,&peer);
    return local;
}



bool BrokerConnectionManager::isConnectionAlive()
{  
    //check connection queue if there is update
    auto conn_status =
    this->ptlocalhost->outgoing_connection_status().want_pop();
    for(auto cs: conn_status)
    {
        // if connection object found the check if there is disconnect flag
        if(cs.status == broker::outgoing_connection_status::tag::disconnected)
        {
            //if disconnected then break the connection.
            LOG(WARNING) <<"Connection Broken";
            closeBrokerConnection();
            this->connected = false;
            //return true;
        }
    }
    return this->connected;
}

 
BrokerQueryManager* BrokerConnectionManager::getQueryManagerPointer()
{
    return this->qm;
}

void BrokerConnectionManager::closeBrokerConnection()
{
    ptlocalhost->unpeer(this->peer);
	this->connected = false;
}
