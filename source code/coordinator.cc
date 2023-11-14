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
// vector<vector<zNode>> cluster_db;
// cluster_db.push_back(cluster1);
// cluster_db.push_back(cluster2);
// cluster_db.push_back(cluster3);


//func declarations
int findServerIdx(std::vector<zNode> v, int id);
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

    cout<<"Got Heartbeat! Type:"<<type<<"(clusterID:"<< clusterId<<",serverID:"<<serverId<<")"<<endl;

    // check if the server is already exist?
    int serverIdx =  -1;
    switch (clusterId){
      case 1:
        // check if already exist.
        serverIdx = findServerIdx(cluster1, serverId); // server's index in the cluster vector
        if(serverIdx != -1){ // exist
          cout<< "Found server. Updating heartbeat..." << endl;
          zNode* server = &cluster1[serverIdx];
          server->last_heartbeat = getTimeNow(); // update heartbeat
          server->missed_heartbeat = false;
          cout<< "=>server timestamp(Update): "<< server->last_heartbeat<< endl;
        }else{ // not found
          // register the server
          cout<< "Register for the new server..."<< endl;
          string serverIP = serverinfo->hostname();
          string serverPort = serverinfo->port();
          zNode new_server;
          new_server.serverID = serverId;
          new_server.hostname = serverIP;
          new_server.port = serverPort;
          new_server.type = "";
          new_server.last_heartbeat = getTimeNow();
          cout<< "=>server timestamp(Register): "<< new_server.last_heartbeat<< endl;
          new_server.missed_heartbeat = false;
          cluster1.push_back(new_server);
        }
        break;
      case 2:
        serverIdx = findServerIdx(cluster2, serverId); // server's index in the cluster vector
        if(serverIdx != -1){ // exist
          cout<< "Found server. Updating heartbeat..."<< endl;
          zNode* server = &cluster2[serverIdx];
          server->last_heartbeat = getTimeNow(); // update heartbeat
          server->missed_heartbeat = false;
          cout<< "=>server timestamp(Update): "<< server->last_heartbeat<< endl;
        }else{ // not found
          // register the server
          cout<< "Register for the new server..."<< endl;
          string serverIP = serverinfo->hostname();
          string serverPort = serverinfo->port();
          zNode new_server;
          new_server.serverID = serverId;
          new_server.hostname = serverIP;
          new_server.port = serverPort;
          new_server.type = "";
          new_server.last_heartbeat = getTimeNow();
          cout<< "=>server timestamp(Register): "<< new_server.last_heartbeat<< endl;
          new_server.missed_heartbeat = false;
          cluster2.push_back(new_server);
        }
        break;
      case 3:
        serverIdx = findServerIdx(cluster3, serverId); // server's index in the cluster vector
        if(serverIdx != -1){ // exist
          cout<< "Found server. Updating heartbeat..."<< endl;
          zNode* server = &cluster3[serverIdx];
          server->last_heartbeat = getTimeNow(); // update heartbeat
          server->missed_heartbeat = false;
          cout<< "=>server timestamp(Update): "<< server->last_heartbeat<< endl;
        }else{ // not found
          // register the server
          cout<< "Register for the new server..."<< endl;
          string serverIP = serverinfo->hostname();
          string serverPort = serverinfo->port();
          zNode new_server;
          new_server.serverID = serverId;
          new_server.hostname = serverIP;
          new_server.port = serverPort;
          new_server.type = "";
          new_server.last_heartbeat = getTimeNow();
          cout<< "=>server timestamp(Register): "<< new_server.last_heartbeat<< endl;
          new_server.missed_heartbeat = false;
          cluster3.push_back(new_server);
        }
        break;
      default:
        break;
      }


    // if("Register" == type){  // first heartbeat after server start.
    //   cout<< "Register for the new server..."<< endl;
    //   string serverIP = serverinfo->hostname();
    //   string serverPort = serverinfo->port();
    //   zNode new_server;
    //   new_server.serverID = serverId;
    //   new_server.hostname = serverIP;
    //   new_server.port = serverPort;
    //   new_server.type = "";
    //   new_server.last_heartbeat = getTimeNow();
    //   cout<< "=>server timestamp(Register): "<< new_server.last_heartbeat<< endl;
    //   new_server.missed_heartbeat = false;

    //   // add server to the cluster
    //   // cluster_db[clusterId].push_back(new_server); // TODO
    //   switch (clusterId)
    //   {
    //   case 1:
    //     cluster1.push_back(new_server);
    //     break;
    //   case 2:
    //     cluster2.push_back(new_server);
    //     break;
    //   case 3:
    //     cluster3.push_back(new_server);
    //     break;
    //   default:
    //     break;
    //   }

    // }else if("Active" == type){
    //   // vector<zNode> cluster = cluster_db[clusterId]; //TODO
    //   // update server zNode's timestamp
    //   cout<< "Find the server and update timestamp..."<< endl;
    //   zNode* server;
    //   switch (clusterId)
    //   {
    //   case 1:
    //     server = &cluster1[findServerIdx(cluster1, serverId)];
    //     server->last_heartbeat = getTimeNow();
    //     server->missed_heartbeat = false;
    //     break;
    //   // case 2:
    //   //   server = cluster2[findServerIdx(cluster2, serverId)];
    //   //   server.last_heartbeat = getTimeNow();
    //   //   server.missed_heartbeat = false;
    //   //   break;
    //   // case 3:
    //   //   server = cluster3[findServerIdx(cluster3, serverId)];
    //   //   server.last_heartbeat = getTimeNow();
    //   //   server.missed_heartbeat = false;
    //   //   break;
    //   default:
    //     break;
    //   }
    //   cout<< "=>server timestamp(Update): "<< server->last_heartbeat<< endl;
    // }

    return Status::OK;
  }
  
  //function returns the server information for requested client id
  //this function assumes there are always 3 clusters and has math
  //hardcoded to represent this.
  Status GetServer(ServerContext* context, const ID* id, ServerInfo* serverinfo) override {
    int userId = id->id();
    int clusterId = ((userId-1)%3)+1;
    int serverId = clusterId; // in mp2.1, one cluster only has one server.
    cout<<"Get Server for clientID: "<<userId<< "(clusterID: "<< clusterId <<endl;
    
    // find the server by clusterID + serverID
    zNode server;
    switch (clusterId){
      case 1:
        server = cluster1[findServerIdx(cluster1, serverId)];
        break;
      case 2:
        server = cluster2[findServerIdx(cluster2, serverId)];
        break;
      case 3:
        server = cluster3[findServerIdx(cluster3, serverId)];
        break;
      default:
        break;
    }

    // check whether the server is available (by miss_heartbeat)
    if(server.missed_heartbeat){
      string errMsg = "Error: Server is missing heartbeat.";
      return Status(grpc::StatusCode::UNAVAILABLE, errMsg);
    }else{
      serverinfo->set_hostname(server.hostname);
      serverinfo->set_port(server.port);

    }

    // Your code here
    // If server is active, return serverinfo
     
    return Status::OK;
  }
  

};

// check all servers' heartbeat
void RunServer(std::string port_no){
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
      for(auto& s : cluster1){
        if(difftime(now_time,s.last_heartbeat)>10){
          cout<< "C1: Haven't received heartbeat for over 10 sec!"<< endl; 
          cout<< "(ServerId: "<< s.serverID<< ",last_heartbeat: "<< s.last_heartbeat<< ")" << endl;
          if(!s.missed_heartbeat){
            s.missed_heartbeat = true;
            s.last_heartbeat = now_time;
          }else{
            // already miss heartbeat in last check => remove from routing table?
            s.last_heartbeat = now_time;
          }
        }
      }
      for(auto& s : cluster2){
        if(difftime(now_time,s.last_heartbeat)>10){
          cout<< "C2: Haven't received heartbeat for over 10 sec!"<< endl; 
          cout<< "(ServerId: "<< s.serverID<< ",last_heartbeat: "<< s.last_heartbeat<< ")" << endl;
          if(!s.missed_heartbeat){
            s.missed_heartbeat = true;
            s.last_heartbeat = now_time;
          }else{
            // already miss heartbeat in last check => remove from routing table?
            s.last_heartbeat = now_time;
          }
        }
      }
      for(auto& s : cluster3){
        if(difftime(now_time,s.last_heartbeat)>10){
          cout<< "C3: Haven't received heartbeat for over 10 sec!"<< endl; 
          cout<< "(ServerId: "<< s.serverID<< ",last_heartbeat: "<< s.last_heartbeat<< ")" << endl;
          if(!s.missed_heartbeat){
            s.missed_heartbeat = true;
            s.last_heartbeat = now_time;
          }else{
            // already miss heartbeat in last check => remove from routing table?
            s.last_heartbeat = now_time;
          }
        }
      }
      
      sleep(3);
    }
}


std::time_t getTimeNow(){
    return std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
}

// find the server by clusterId + serverId
int findServerIdx(std::vector<zNode> clusterVector, int id){
  for(int i=0; i<clusterVector.size(); i++){
    if(clusterVector[i].serverID == id){
      cout<< "find the server in Idx: " << i << endl;
      return i;
    }
  }
  // server not found
  return -1;
}