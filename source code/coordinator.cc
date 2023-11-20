#include <ctime>

#include <google/protobuf/timestamp.pb.h>
#include <google/protobuf/duration.pb.h>
#include <chrono>
#include <sys/stat.h>
#include <sys/types.h>
#include <vector>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <mutex>
#include <stdlib.h>
#include <unistd.h>
#include <google/protobuf/util/time_util.h>
#include <grpc++/grpc++.h>

#include "coordinator.grpc.pb.h"

using google::protobuf::Timestamp;
using google::protobuf::Duration;
using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerReader;
using grpc::ServerReaderWriter;
using grpc::ServerWriter;
using grpc::Status;
using csce438::CoordService;
using csce438::ServerInfo;
using csce438::Confirmation;
using csce438::ID;
// using csce438::ServerList;
// using csce438::SynchService;
using namespace std;

struct zNode{
    int serverID;
    std::string hostname;
    std::string port;
    std::string type;
    std::time_t last_heartbeat;
    bool missed_heartbeat;
    bool isActive();

};

//potentially thread safe
std::mutex v_mutex;
std::vector<zNode> cluster1;
std::vector<zNode> cluster2;
std::vector<zNode> cluster3;
vector<vector<zNode>* > cluster_db(3);


//func declarations
int findServerIdx(std::vector<zNode> v, int id);
int findServerIdxByType(std::vector<zNode> v, string type);
std::time_t getTimeNow();
void checkHeartbeat();


// bool ServerStruct::isActive(){
//     bool status = false;
//     if(!missed_heartbeat){
//         status = true;
//     }else if(difftime(getTimeNow(),last_heartbeat) < 10){
//         status = true;
//     }
//     return status;
// }

class CoordServiceImpl final : public CoordService::Service {

  
  Status Heartbeat(ServerContext* context, const ServerInfo* serverinfo, Confirmation* confirmation) override {
    
    // get server info
    int clusterId = serverinfo->clusterid();
    int serverId = serverinfo->serverid();
    string type = serverinfo->type();

    cout<<"Got Heartbeat! (clusterID:"<< clusterId<<",serverID:"<<serverId<<")"<<endl;

    // get the cluster 
    vector<zNode>* cluster = cluster_db[clusterId-1]; // clusterID begins from 1
    if (cluster->empty()) {
      cout<< "Cluster Empty!" << endl;
      // TODO: return fail
    }

    // check if the server is already exist?
    int serverIdx;
    v_mutex.lock();
    serverIdx = findServerIdx(*cluster, serverId); // server's index in the cluster vector
    if(serverIdx != -1){ // exist -> update the heartbeat.
      cout<< "Found server. Updating heartbeat..." << endl;
      // zNode* server = cluster[serverIdx];
      zNode& server = cluster->at(serverIdx);
      server.last_heartbeat = getTimeNow();
      server.missed_heartbeat = false;
      cout<< "=> server timestamp(Update): "<< server.last_heartbeat<< endl;
      v_mutex.unlock();
    }else{ // not found -> register the server
      v_mutex.unlock();
      cout<< "Register for the new server..."<< endl;
      string serverIP = serverinfo->hostname();
      string serverPort = serverinfo->port();
      zNode new_server;
      new_server.serverID = serverId;
      new_server.hostname = serverIP;
      new_server.port = serverPort;
      // decide it's a Master(type="M") or a Slave(type="S")?
      if(findServerIdxByType(*cluster,"M") != -1){
        // assign the sever as Slave
        new_server.type = "S";
        cout<< "register as a Slave." << endl;
      }else{
        // assign it as a Master
        new_server.type = "M";
        cout<< "register as a Master." << endl;
      }
      new_server.last_heartbeat = getTimeNow();
      new_server.missed_heartbeat = false;
      cout<< "=> server timestamp(Register): "<< new_server.last_heartbeat<< endl;

      cluster->push_back(new_server);
    }

    return Status::OK;
  }
  
  //function returns the server information for requested client id
  //this function assumes there are always 3 clusters and has math
  //hardcoded to represent this.
  Status GetServer(ServerContext* context, const ID* id, ServerInfo* serverinfo) override {
    int userId = id->id();
    int clusterId = ((userId-1)%3)+1;
    vector<zNode> cluster = *(cluster_db[clusterId-1]);
    int serverIdx = findServerIdxByType(cluster, "M"); // find Master server for the client
    if(serverIdx == -1){
      cout<< "ERROR: No avaliable server in the cluster right now." << endl; 
      // TODO: return fail to client
    }
    
    zNode server = cluster[serverIdx];
    // check whether the server is available (by miss_heartbeat)
    if(server.missed_heartbeat){
      string errMsg = "Error: Server is missing heartbeat.";
      return Status(grpc::StatusCode::UNAVAILABLE, errMsg);
    }else{
      serverinfo->set_hostname(server.hostname);
      serverinfo->set_port(server.port);
    }
    // If server is active, return serverinfo
    cout<<"Get Server for client: "<<userId<<", clusterId: "<< clusterId << ",serverId: "<< server.serverID <<endl;
     
    return Status::OK;
  }
  

};

void prepareClusters(){
  cluster_db[0] = &cluster1;
  cluster_db[1] = &cluster2;
  cluster_db[2] = &cluster3;
}

// check all servers' heartbeat
void RunServer(std::string port_no){
  // prepare clusters
  prepareClusters();
  //start thread to check heartbeats
  std::thread hb(checkHeartbeat);
  //localhost = 127.0.0.1
  std::string server_address("127.0.0.1:"+port_no);
  CoordServiceImpl service;
  //grpc::EnableDefaultHealthCheckService(true);
  //grpc::reflection::InitProtoReflectionServerBuilderPlugin();
  ServerBuilder builder;
  // Listen on the given address without any authentication mechanism.
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  // Register "service" as the instance through which we'll communicate with
  // clients. In this case it corresponds to an *synchronous* service.
  builder.RegisterService(&service);
  // Finally assemble the server.
  std::unique_ptr<Server> server(builder.BuildAndStart());
  std::cout << "Server listening on " << server_address << std::endl;

  // Wait for the server to shutdown. Note that some other thread must be
  // responsible for shutting down the server for this call to ever return.
  server->Wait();
}

int main(int argc, char** argv) {
  
  std::string port = "3010";
  int opt = 0;
  while ((opt = getopt(argc, argv, "p:")) != -1){
    switch(opt) {
      case 'p':
          port = optarg;
          break;
      default:
	std::cerr << "Invalid Command Line Argument\n";
    }
  }
  RunServer(port);
  return 0;
}



void checkHeartbeat(){
    while(true){
      //check servers for heartbeat > 10
      //if true turn missed heartbeat = true
      // Your code below
      time_t now_time = getTimeNow();
      cout<< "Start checking heartbeat. Now Time: " << now_time << endl;

      for(int i=0; i<cluster_db.size(); i++){
        cout<< ">checking cluster[" << i+1 << "]"<< endl;
        for(auto s = cluster_db[i]->begin(); s != cluster_db[i]->end(); ++s){
          cout<< ">>checking server,ID:" << s->serverID << endl;
          v_mutex.lock();
          if(difftime(now_time,s->last_heartbeat)>10){
            cout<< "Haven't received heartbeat for over 10 sec!"<< endl; 
            cout<< "(ServerId: "<< s->serverID<< ",last_heartbeat: "<< s->last_heartbeat<< ")" << endl;
            if(!s->missed_heartbeat){ // first time missing heartbeat
              s->missed_heartbeat = true;
              s->last_heartbeat = now_time;
            }else{ // second time missing heartbeat
              if(s->type == "M"){
                // remove failed Master
                cout<< "Master server failed! " << endl;
                cluster_db[i]->erase(s);
                // find slave and make it become master
                int serverIdx = findServerIdxByType(*cluster_db[i], "S");
                if(serverIdx != -1){
                  cout<< "Slave will become new Master." << endl;
                  zNode& server = (cluster_db[i])->at(serverIdx);
                  server.type = "M";
                }
              }
              s->last_heartbeat = now_time;
            }
          }
          v_mutex.unlock();
        }
      }
      
      sleep(3); // sleep 3 second
    }
}


std::time_t getTimeNow(){
    return std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
}

// find the server from a cluster by its serverId
int findServerIdx(std::vector<zNode> clusterVector, int id){
  for(int i=0; i<clusterVector.size(); i++){
    if(clusterVector[i].serverID == id){
      //cout<< "find the server in Idx: " << i << endl;
      return i;
    }
  }
  // server not found
  return -1;
}

// find the server's idx in a cluster by its type
// type can be "M"(Master) or "S"(Slave)
int findServerIdxByType(std::vector<zNode> clusterVector, string type){
  for(int i=0; i<clusterVector.size(); i++){
    if(clusterVector[i].type == type){
      cout<< "find the "<< type << " server in Idx: " << i << endl;
      return i;
    }
  }
  // server not found
  return -1;
}