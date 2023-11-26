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
#include <algorithm>
#include <google/protobuf/util/time_util.h>
#include <grpc++/grpc++.h>

#include "coordinator.grpc.pb.h"
#include "synchronizer.grpc.pb.h"
#include "utils.h"

using namespace std::filesystem;

using google::protobuf::Timestamp;
using google::protobuf::Duration;
using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerReader;
using grpc::ServerReaderWriter;
using grpc::ServerWriter;
using grpc::Status;
using grpc::ClientContext;
using csce438::CoordService;
using csce438::ServerInfo;
using csce438::Confirmation;
using csce438::UserList;
using csce438::AddressList;
using csce438::AddressInfo;
using csce438::Req;
using csce438::Res;
using csce438::ID;
using csce438::User_Follower;
// using csce438::ServerList;
using csce438::SynchService;
// using csce438::AllUsers;
// using csce438::TLFL;
using namespace std;

int synchID = 1;
int clusterID = 1; //default
std::vector<std::string> get_lines_from_file(std::string);
void run_synchronizer(std::string,std::string,std::string,int);
std::vector<std::string> get_all_users_func(int);
std::vector<std::string> get_tl_or_fl(int, int, bool);
vector<string> get_files(int clusterID, string filetype);
bool file_contains_user(std::string filename, std::string user);
vector<string> get_user_file(int clusterID, int userID, string filetype);
unique_ptr<SynchService::Stub> createSyncStub(string addr);

std::unique_ptr<CoordService::Stub> coord_stub_;
std::unique_ptr<SynchService::Stub> sync_stub_;

class SynchServiceImpl final : public SynchService::Service {
    // Status GetAllUsers(ServerContext* context, const Confirmation* confirmation, AllUsers* allusers) override{
    //     //std::cout<<"Got GetAllUsers"<<std::endl;
    //     std::vector<std::string> list = get_all_users_func(synchID);
    //     //package list
    //     for(auto s:list){
    //         allusers->add_users(s);
    //     }

    //     //return list
    //     return Status::OK;
    // }

    // Status GetTLFL(ServerContext* context, const ID* id, TLFL* tlfl){
    //     //std::cout<<"Got GetTLFL"<<std::endl;
    //     int clientID = id->id();

    //     std::vector<std::string> tl = get_tl_or_fl(synchID, clientID, true);
    //     std::vector<std::string> fl = get_tl_or_fl(synchID, clientID, false);

    //     //now populate TLFL tl and fl for return
    //     for(auto s:tl){
    //         tlfl->add_tl(s);
    //     }
    //     for(auto s:fl){
    //         tlfl->add_fl(s);
    //     }
    //     tlfl->set_status(true); 

    //     return Status::OK;
    // }

    Status ResynchServer(ServerContext* context, const Req* req, Res* res){
        std::cout<<req->type()<<"("<<req->serverid()<<") just restarted and needs to be resynched with counterpart"<<std::endl;
        std::string backupServerType;

        // YOUR CODE HERE


        return Status::OK;
    }

    Status UpdFollower(ServerContext* context, const User_Follower* user_follower, Res* res){
        std::cout<<"Got UpdFollower()! Start updating followers file."<<std::endl;
        int userID = user_follower->userid();
        // get all [userID]_followers.txt file from Master and Slave server folders
        vector<string> follower_files = get_user_file(clusterID, userID, "followers");
        // add each new followers to the file
        for(int followerID : user_follower->followers()){
            std::cout<<"User "<< followerID <<" follows "<<userID<<std::endl;
            for(string file : follower_files){
                std::cout<<"Updating file: "<<file<<std::endl;
                if(!file_contains_user(file, to_string(followerID))){
                    cout<<"Adding "<<followerID<<" to "<<file<< endl;
                    ofstream ofs(file, std::ios_base::app);
                    ofs << followerID << std::endl;
                    ofs.close();
                }else{
                    cout<<followerID<<" already in "<<file<<std::endl;
                }
            }
        }

        return Status::OK;
    }
};

void RunServer(std::string coordIP, std::string coordPort, std::string port_no, int synchID){
  //localhost = 127.0.0.1
  std::string server_address("127.0.0.1:"+port_no);
  clusterID = synchID; // same

  SynchServiceImpl service;
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
  std::cout << "Server listening on " << server_address << ", cluster:"<< clusterID<< std::endl;

  std::thread t1(run_synchronizer,coordIP, coordPort, port_no, synchID);
  /*
  TODO List:
    -Implement service calls
    -Set up initial single heartbeat to coordinator
    -Set up thread to run synchronizer algorithm
  */

  // Wait for the server to shutdown. Note that some other thread must be
  // responsible for shutting down the server for this call to ever return.
  server->Wait();
}



int main(int argc, char** argv) {
  
  int opt = 0;
  std::string coordIP;
  std::string coordPort;
  std::string port = "3029";

  while ((opt = getopt(argc, argv, "h:k:p:i:")) != -1){
    switch(opt) {
      case 'h':
          coordIP = optarg;
          break;
      case 'k':
          coordPort = optarg;
          break;
      case 'p':
          port = optarg;
          break;
      case 'i':
          synchID = std::stoi(optarg);
          break;
      default:
	         std::cerr << "Invalid Command Line Argument\n";
    }
  }

  RunServer(coordIP, coordPort, port, synchID);
  return 0;
}

void run_synchronizer(std::string coordIP, std::string coordPort, std::string port, int synchID){
    // Send a initial heartbeat to the coordinator for registration
    cout<<"Creating coordinator stub"<<std::endl;
    std::string target_str = coordIP + ":" + coordPort;
    coord_stub_ = std::unique_ptr<CoordService::Stub>(CoordService::NewStub(grpc::CreateChannel(target_str, grpc::InsecureChannelCredentials())));

    ServerInfo serverInfo;
    Confirmation c;
    grpc::ClientContext context;
    serverInfo.set_clusterid(synchID);
    serverInfo.set_serverid(synchID);
    serverInfo.set_hostname("127.0.0.1");
    serverInfo.set_port(port);
    serverInfo.set_type("F");

    // send init heartbeat
    cout<< "Sent a heartbeat to the coordinator ("<< target_str<< ")"<< endl;
    coord_stub_->Heartbeat(&context, serverInfo, &c);

    // Begin synchronization process
    while(true){
        //change this to 30 eventually
        sleep(10);
        cout<< ">> Start synchronization process" << endl;

        // Part1: Check following relationships
        cout<< ">> Part 1: Check following updates" << endl;
        // get all following files in cluster //TODO: use muti threads to accelerate?
        std::map<int, std::vector<int>> followers; // ex: followers[1] = [2, 3, 4] (followed by 2, 3, 4)
        vector<string> following_files = get_files(clusterID, "following");
        // check each file
        for (const string& file : following_files) {
            string filename = file.substr(file.find_last_of("/") + 1);
            char type = file.substr(file.find("server_") + 7, 1)[0]; // M or S
            int follower_userid = stoi(filename.substr(0, filename.find_last_of("_")));
            cout<< "filename: " << filename << ",type: "<< type << endl;

            // start reading the file
            ifstream ifs(file);
            string tempfileName = file + ".mod";
            ofstream ofs(tempfileName);
            if (!ifs.is_open()) {
                cerr << "Failed to open file: " << file << endl;
                continue;
            }
            string line;
            while (getline(ifs, line)) {
                if (ends_with(line, "N")) {
                    cout << "Line ends with N: " << line << endl;
                    int following_userid = stoi(line.substr(0, 1));
                    cout << "Following user id: " << following_userid << endl;
                    // add the following user to the list
                    if(type == 'M'){
                        followers[following_userid].push_back(follower_userid);
                        cout<< "followers[" << following_userid << "]: " << followers[following_userid].size() << endl;
                    }
                    // mark the line as updated
                    line += "||Y";
                }
                ofs << line << endl;
            }

            ifs.close();
            ofs.close();
            // replace the old file with the new one
            remove(file);
            rename(tempfileName.c_str(), file);

        }// END for-loop for checking following files
        cout<< "followers.size(): " << followers.size() << endl;
        if(followers.size() > 0){
            // ask the coordinator for other follower synchronizer's addresses
            ClientContext context;
            AddressList addressList;
            UserList userList;
            for (const auto& [key, value] : followers) {
                cout << "userID: " << key << endl;
                userList.add_users(key);
            } // users that need to ask address

            Status status = coord_stub_->GetUsersAddrs(&context, userList, &addressList);
            if (status.ok()) {
                cout << "GetUsersAddrs RPC succeeded." << endl;
                for(AddressInfo addressInfo : addressList.addressinfo()){
                    cout << "AddressInfo: " << addressInfo.userid() << ":" << addressInfo.syncaddress() << endl;
                    // start sending followers to other follower synchronizers.
                    sync_stub_ = createSyncStub(addressInfo.syncaddress());
                    ClientContext context2;
                    User_Follower user_follower;
                    user_follower.set_userid(addressInfo.userid());
                    for(int follower : followers[addressInfo.userid()]){
                        user_follower.add_followers(follower);
                        cout<< "add follower: "<< follower << endl;
                    }
                    Res res;

                    Status status2 = sync_stub_->UpdFollower(&context2, user_follower, &res);
                    if(status2.ok()){
                        cout << "UpdFollower() RPC succeeded."<<" user("<<addressInfo.userid()<<") updated."<< endl;
                    } else {
                        cout << "UpdFollower RPC failed." << endl;
                    }
                }
            } else {
                cout << "GetUsersAddrs RPC failed." << endl;
            }


        }
        


        // Part2: Check timeline updates
        cout<< ">> Part 2: Check timeline updates" << endl;
        //synch all users file 
            //get list of all followers

            // YOUR CODE HERE
            //set up stub
            //send each a GetAllUsers request
            //aggregate users into a list
            //sort list and remove duplicates

            // YOUR CODE HERE

            //for all the found users
            //if user not managed by current synch
            // ...
 
            // YOUR CODE HERE

	    //force update managed users from newly synced users
            //for all users
            // for(auto i : aggregated_users){
            //     //get currently managed users
            //     //if user IS managed by current synch
            //         //read their follower lists
            //         //for followed users that are not managed on cluster
            //         //read followed users cached timeline
            //         //check if posts are in the managed tl
            //         //add post to tl of managed user    
            
            //          // YOUR CODE HERE
            //     //     }
            //     // }
            // }

        cout<< ">> End synchronization process. Sleep 10 seconds." << endl;
    }// end while-loop

    return;
}

// create a synchronizer stub for given address
unique_ptr<SynchService::Stub> createSyncStub(string addr){
    return SynchService::NewStub(grpc::CreateChannel(addr, grpc::InsecureChannelCredentials()));
}

// get all files in specific server folder "server_M(or S)_[clusterID]_[serverID]"
// with the end "_[filetype].txt 
// filetype can be: 1.followers/ 2.following/ 3.timeline
vector<string> get_files(int clusterID, string filetype) {
  vector<string> files;
  for (path p : recursive_directory_iterator("./")) {
    if (is_directory(p) && (starts_with(p.filename().string(), "server_M_" + to_string(clusterID) + "_")
         || starts_with(p.filename().string(), "server_S_" + to_string(clusterID) + "_"))) {
      cout<< "checking directory: " << p.string() << endl;
      for (path q : recursive_directory_iterator(p)) {
        if (is_regular_file(q) && ends_with(q.filename().string(), "_" + filetype + ".txt")) {
          files.push_back(q.string());
          cout<< q.string() << ", " << endl;
        }
      }
    }
  }

  return files;
}

// get all files in server's folder (both master and slave)
// params:
//  1. clusterID
//  2. userID
//  3. filetype: 1.followers/ 2.following/ 3.timeline
// returns
//  1. vector of file paths
vector<string> get_user_file(int clusterID, int userID, string filetype) {
  vector<string> files;
  for (path p : recursive_directory_iterator("./")) {
    if (is_directory(p) && (starts_with(p.filename().string(), "server_M_" + to_string(clusterID) + "_")
         || starts_with(p.filename().string(), "server_S_" + to_string(clusterID) + "_"))) {
      cout<< "checking directory: " << p.string() << endl;
      for (path q : recursive_directory_iterator(p)) {
        if (is_regular_file(q) && starts_with(q.filename().string(), to_string(userID) + "_")
            && ends_with(q.filename().string(), "_" + filetype + ".txt")) {
          files.push_back(q.string());
          cout<< q.string() << ", " << endl;
        }
      }
    }
  }

  return files;
}

std::vector<std::string> get_lines_from_file(std::string filename){
  std::vector<std::string> users;
  std::string user;
  std::ifstream file; 
  file.open(filename);
  if(file.peek() == std::ifstream::traits_type::eof()){
    //return empty vector if empty file
    //std::cout<<"returned empty vector bc empty file"<<std::endl;
    file.close();
    return users;
  }
  while(file){
    getline(file,user);

    if(!user.empty())
      users.push_back(user);
  } 

  file.close();

  //std::cout<<"File: "<<filename<<" has users:"<<std::endl;
  /*for(int i = 0; i<users.size(); i++){
    std::cout<<users[i]<<std::endl;
  }*/ 

  return users;
}

bool file_contains_user(std::string filename, std::string user){
    std::vector<std::string> users;
    //check username is valid
    users = get_lines_from_file(filename);
    for(int i = 0; i<users.size(); i++){
      //std::cout<<"Checking if "<<user<<" = "<<users[i]<<std::endl;
      if(user == users[i]){
        //std::cout<<"found"<<std::endl;
        return true;
      }
    }
    //std::cout<<"not found"<<std::endl;
    return false;
}

std::vector<std::string> get_all_users_func(int synchID){
    //read all_users file master and client for correct serverID
    std::string master_users_file = "./master"+std::to_string(synchID)+"/all_users";
    std::string slave_users_file = "./slave"+std::to_string(synchID)+"/all_users";
    //take longest list and package into AllUsers message
    std::vector<std::string> master_user_list = get_lines_from_file(master_users_file);
    std::vector<std::string> slave_user_list = get_lines_from_file(slave_users_file);

    if(master_user_list.size() >= slave_user_list.size())
        return master_user_list;
    else
        return slave_user_list;
}

std::vector<std::string> get_tl_or_fl(int synchID, int clientID, bool tl){
    std::string master_fn = "./master"+std::to_string(synchID)+"/"+std::to_string(clientID);
    std::string slave_fn = "./slave"+std::to_string(synchID)+"/" + std::to_string(clientID);
    if(tl){
        master_fn.append("_timeline");
        slave_fn.append("_timeline");
    }else{
        master_fn.append("_follow_list");
        slave_fn.append("_follow_list");
    }

    std::vector<std::string> m = get_lines_from_file(master_fn);
    std::vector<std::string> s = get_lines_from_file(slave_fn);

    if(m.size()>=s.size()){
        return m;
    }else{
        return s;
    }

}
