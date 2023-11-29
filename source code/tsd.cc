/*
 *
 * Copyright 2015, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <ctime>

#include <google/protobuf/timestamp.pb.h>
#include <google/protobuf/duration.pb.h>

#include <thread>
#include <fstream>
#include <iostream>
#include <filesystem>
#include <memory>
#include <string>
#include <stdlib.h>
#include <unistd.h>
#include <google/protobuf/util/time_util.h>
#include <grpc++/grpc++.h>
#include<glog/logging.h>
#define log(severity, msg) LOG(severity) << msg; google::FlushLogFiles(google::severity); 

#include "sns.grpc.pb.h"
#include "utils.h"
#include <regex>

#include "coordinator.grpc.pb.h" // mp2.1

using google::protobuf::Timestamp;
using google::protobuf::Duration;
using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerReader;
using grpc::ServerReaderWriter;
using grpc::ServerWriter;
using grpc::Status;
using grpc::ClientReaderWriter;
using csce438::Message;
using csce438::ListReply;
using csce438::Request;
using csce438::Reply;
using csce438::SNSService;
using csce438::CoordService;
using csce438::ServerInfo;
using csce438::Confirmation;
using csce438::ID;
using csce438::UserList;
using grpc::Channel;
using grpc::ClientContext;
using namespace std;

struct Client { // after mp2.2, this obj is used to store timeline connection info
  std::string username;
  bool timeline_connected = false;
  int following_file_size = 0;
  std::vector<Client*> client_followers;
  std::vector<Client*> client_following;
  ServerReaderWriter<Message, Message>* stream = 0;
  bool operator==(const Client& c1) const{
    return (username == c1.username);
  }
};

//Vector that stores every client that has been created
std::vector<Client> client_db;

// Have an instance of the coordinator stub as a member variable.
std::unique_ptr<CoordService::Stub> coord_stub_;

string server_directroy_path;
// Master-Slave stuff
int clusterID;
int serverID;
bool isMaster;
bool slave_connected = false;
string slaveAddr;
std::unique_ptr<SNSService::Stub> slave_stub_; // master use this to pass request to slave
vector<string> readLines(int userID, string fileType);
bool registerSlave(int clusterID);
bool sync_with_Master();
std::mutex v_mutex;

// find the index of the user in the client db by username
int findClientIdx(string username){
    for(int i=0; i<client_db.size(); i++){
      if(client_db[i].username == username){
        return i;
      }
    }
    // user not found
    return -1;
}


class SNSServiceImpl final : public SNSService::Service {
  
  Status List(ServerContext* context, const Request* request, ListReply* list_reply) override {
    string username = request->username(); // equals userID
    cout<< username << " use List cmd."<< endl;
    // get all users
    ClientContext context2;
    UserList userList;
    ID id;
    coord_stub_->GetAllUsers(&context2, id, &userList);
    for(int userID : userList.users()){
      list_reply->add_all_users(to_string(userID));
      cout<< "user: " << userID << ", ";
    }
    cout<< endl;
    // get followes of the current user
    vector<string> followers = readLines(stoi(username), "followers");
    for(string follower : followers){
      list_reply->add_followers(follower);
    }

    return Status::OK;
  }

  Status Follow(ServerContext* context, const Request* request, Reply* reply) override {

    string username = request->username(); // username = userid
    string follow_username = request->arguments(0); // the user client want to follow
    cout<< username << " wants to follow "<< follow_username << endl;

    // add follow_user to client's following.txt
    string user_following = server_directroy_path + "/" + username+"_following.txt";
    fstream file(user_following, ios::app|ios::out);
    if(!file.is_open()){
      std::cerr << "Failed to open file: "<< username<<"_following.txt" << std::endl;
    }
    // Append the new line to the file
    file << follow_username << "||N" << endl;
    file.close();

    cout<< username << " is now following "<< follow_username << endl;

    // Master server should pass the request to Slave server
    if(isMaster){
      cout<< "Pass follow request to slave server" << slaveAddr << endl;
      if(slaveAddr.empty()){
        cout<< "Slave server addr is empty!" << endl;
        // try to register slave server
        if(registerSlave(clusterID)){
          // Follow
          ClientContext context;
          Request request;
          Reply reply;
          request.set_username(username);
          request.add_arguments(follow_username);
          Status status2 = slave_stub_->Follow(&context, request, &reply);
          if(status2.ok()){
            cout<< "Slave server follow success." << endl;
          }else{
            cout<< "Slave server follow failed." << endl;
          }
        }else{
          cout<< "Slave server not available now." << endl;
          return Status::OK;
        }
      }else{
        // Follow
        ClientContext context;
        Request request;
        Reply reply;
        request.set_username(username);
        request.add_arguments(follow_username);
        Status status2 = slave_stub_->Follow(&context, request, &reply);
        if(status2.ok()){
          cout<< "Slave server follow success." << endl;
        }else{
          cout<< "Slave server follow failed." << endl;
        }
      }
      
    }

    return Status::OK;
  }

  Status UnFollow(ServerContext* context, const Request* request, Reply* reply) override {

    string username = request->username();
    string unfollow_username = request->arguments(0);
    cout<< username << " wants to unfollow "<< unfollow_username << endl;

    int user_idx = findClientIdx(username);
    int unfollow_user_idx = findClientIdx(unfollow_username);
    Client* user = &client_db[user_idx];
    Client* unfollow_user = NULL;
    string errMsg;

    // check if unfollow_user is in user's following
    for(Client* c : user->client_following){
      if(c->username == unfollow_username){
        unfollow_user = c;
        break;
      }
    }
    if(NULL == unfollow_user){ // not in user's following list
      errMsg = "Error: " + unfollow_username + " is not in " + username + "'s following list.";
      return Status(grpc::StatusCode::NOT_FOUND, errMsg);
    }else{ // in following list
      std::vector<Client*>::iterator pos;
      // remove from following list
      pos = find((user->client_following).begin(), (user->client_following).end(), unfollow_user);
      cout<< "unfollow user addr:" << unfollow_user << endl;
      cout<< "find addr:" << *pos << endl;
      (user->client_following).erase(pos);
      // remove user from unfollow_user's followers
      pos = find(unfollow_user->client_followers.begin(), unfollow_user->client_followers.end(), user);
      cout<< "follower user addr:" << user << endl;
      cout<< "find addr:" << *pos << endl;
      unfollow_user->client_followers.erase(pos);

      return Status::OK;
    }

  }

  // RPC Login
  Status Login(ServerContext* context, const Request* request, Reply* reply) override {

    // get the username from the request
    string username = request->username();
    cout<<username<<" is trying to connect."<<endl;
    string reply_msg;
    // check if the user is already in client_db
    int user_idx = findClientIdx(username);

    if(user_idx == -1){ // user not found in client_db
      reply_msg = "registered new user successfully.";
      reply->set_msg(reply_msg);
      // create files for the user
      string user_timeline = server_directroy_path + "/" + username+"_timeline.txt";
      string user_following = server_directroy_path + "/" + username+"_following.txt";
      string user_followers = server_directroy_path + "/" + username+"_followers.txt";
      createFile(user_timeline);
      createFile(user_following);
      createFile(user_followers);
      // also record client connection in the client_db
      Client new_client;
      new_client.username = username;
      new_client.timeline_connected = false;
      client_db.push_back(new_client);
    }else{
      reply_msg = "logged in successfully.";
      reply->set_msg(reply_msg);
      // update client timeline connection status
      Client* user = &client_db[user_idx];
      user->timeline_connected = false;
    }
    cout<< username + " " + reply_msg << endl;

    if(isMaster){ // Master server should pass the request to Slave server
      // ask the coordinator for the slave server's address
      ClientContext context;
      ServerInfo serverInfo;
      ID id;
      id.set_id(clusterID);
      Status status = coord_stub_->GetSlaveInfo(&context, id, &serverInfo);

      if(status.ok()){
        slaveAddr = serverInfo.hostname() + ":" + serverInfo.port();
        cout<< "Got slave server addr: " << slaveAddr << endl;
        // passes the request to Slave server
        std::shared_ptr<Channel> channel2 = grpc::CreateChannel(slaveAddr, grpc::InsecureChannelCredentials());
        slave_stub_ = SNSService::NewStub(channel2,grpc::StubOptions());
        slave_connected = true;
        cout << "Complete creating a slave server stub." << endl;
        // Login
        ClientContext context;
        Request request;
        Reply reply;
        request.set_username(username);
        Status status2 = slave_stub_->Login(&context, request, &reply);
        if(status2.ok()){
          cout<< "Slave server login success." << endl;
        }else{
          cout<< "Slave server login failed." << endl;
        }
      }else{
        cout<< "Failed to get SlaveServer address." << endl;
      }
    }

    return Status::OK;
  }

  Status Timeline(ServerContext* context, 
		ServerReaderWriter<Message, Message>* stream) override {

    Message msg;
    vector<Message> msg_buffer;
    int userID;
    // bool check_upd_started = false;
    // use a message buffer
    shared_ptr<ClientReaderWriter<Message, Message>> slave_stream;
    ClientContext c_context;
    if(isMaster){
      if(slaveAddr.empty()){
        slave_connected = registerSlave(clusterID);
      }
      if(slave_connected){
        slave_stream = shared_ptr<ClientReaderWriter<Message, Message>>(slave_stub_->Timeline(&c_context));
      }
    }

    // thread that keeps reading messages from the client
    thread reader([&msg, stream, &msg_buffer, &slave_stream](){
      while(stream->Read(&msg)){
        cout <<"[Timeline] Receive Post From["+msg.username()+"]: " <<msg.msg()<<endl;
        // use a message buffer to store the posts from user
        v_mutex.lock();
        Message temp_msg;
        google::protobuf::Timestamp* timestamp = new google::protobuf::Timestamp();
        timestamp->set_seconds(std::time(nullptr));
        timestamp->set_nanos(0);
        string time = google::protobuf::util::TimeUtil::ToString(*timestamp);
        temp_msg.set_time_str(time);
        cout<< "time_str:" << temp_msg.time_str() << endl;
        temp_msg.set_username(msg.username());
        temp_msg.set_msg(msg.msg());
        msg_buffer.push_back(temp_msg);
        v_mutex.unlock();
        if(isMaster){
          if(slaveAddr.empty()){
            slave_connected = registerSlave(clusterID);
            if(slave_connected){
              ClientContext c_context;
              slave_stream = shared_ptr<ClientReaderWriter<Message, Message>>(slave_stub_->Timeline(&c_context));
              slave_stream->Write(temp_msg);
            }
          }else{
            slave_stream->Write(temp_msg);
          }
        }

      }
    });

    // thread that keeps processing messages in the buffer every 5 sec
    thread process([&userID, stream, &msg_buffer](){
      sleep(2); // sleep first so that the post can be stored into the msg buffer
      while(1){
        cout<< "[Timeline] Start check timeline message buffer." << endl;
        while (!msg_buffer.empty()){        
          Message msg = msg_buffer.front();
          msg_buffer.erase(msg_buffer.begin());
          // Start processing the message
          // > Check if it's first time into timeline mode?
          if(msg.msg() == "First_time_timeline"){
            userID = stoi(msg.username()); // record user's id
            // set user's timeline connection status
            int user_idx = findClientIdx(msg.username());
            Client* user = &client_db[user_idx];
            user->timeline_connected = true;

            // get newest posts from user's timeline.txt
            // read file in the server directory
            string line;
            vector<string> newest_twenty;
            string user_timeline = server_directroy_path + "/" + msg.username()+"_timeline.txt";
            ifstream in(user_timeline);
            cout<< "Get newest posts from user's timeline.txt" << endl;
            while(getline(in, line)){
              // skip user's own posts
              if(starts_with(line, "@||")){
                continue;
              }else{
                newest_twenty.push_back(line);
                cout<< "get line:" << line << endl;
              }
            }
            cout<< "Posts size: " << newest_twenty.size() << endl;

            //Send the newest messages to the client
            Message new_msg;
            int output_position = newest_twenty.size(); // record the position of the output post (from sync)
            //for(int i = newest_twenty.size(); i>=0; --i){
            for(string str : newest_twenty){
              if(str.empty()) continue; // skip empty line
              cout<< "newest:" << str << endl;
              string post = str.substr(0, str.size()-3); // remove "||F"
              cout<< "processed post:" << post<< endl;
              vector<string> post_data = splitString(post, '|');
              cout<< "time_str:" << post_data[0] << " , username:" << post_data[1] << ", msg:" << post_data[2] << endl;
              new_msg.set_time_str(post_data[0]);
              new_msg.set_username(post_data[1]);
              new_msg.set_msg(post_data[2]);
              stream->Write(new_msg);
            }

            // start a new thread to check if there is new posts
            try {
              thread writer([&output_position, userID, stream](){
                // start checking user's timeline every 5 sec
                while(1){
                  sleep(5);
                  // chek user timeline connection status
                  int user_idx = findClientIdx(to_string(userID));
                  Client* user = &client_db[user_idx];
                  if(!user->timeline_connected){ // user is NOT in timeline mode
                    cout<< "[Timeline] User " << userID << " NOT in timeline mode." << endl;
                    break;
                  }
                  cout<< "[Timeline] Check user "<< userID <<"'s timeline every 5 sec." << endl;
                  cout<< userID << " output_position:" << output_position << endl;
                  // read user's timeline file
                  vector<string> lines = readLines(userID, "timeline");
                  // start output new posts to client
                  int position = 0;
                  for(string line : lines){
                    if(starts_with(line, "@||")) continue; // skip user's own posts
                    if(ends_with(line, "||F")){ // come from sync but not updated
                      position++;
                      if(position <= output_position){
                        continue;
                      }else{
                        output_position++;
                        cout<< "line:" << line<< endl;
                        string post = line.substr(0, line.size()-3); // remove "||F"
                        // send new post to client
                        vector<string> post_data = splitString(post, '|');
                        cout<< "time_str:" << post_data[0] << " , username:" << post_data[1] << ", msg:" << post_data[2] << endl;
                        Message new_msg;
                        new_msg.set_time_str(post_data[0]);
                        new_msg.set_username(post_data[1]);
                        new_msg.set_msg(post_data[2]);
                        stream->Write(new_msg);
                      }
                    }
                  } // END checking user's timeline file

                }// END of while(1), sleep 5 sec and check again.
                cout<< "[Timeline] End checking user "<< userID <<"'s timeline." << endl;
              });
              writer.detach();
            } catch (const std::exception& e) {
              std::cerr << "Exception: " << e.what() << std::endl;
            }
            continue; 
            // end of first timeline mode processing
          }else{ // (Not first time into timeline mode)
            // format the message before storing it.
            string time = msg.time_str();
            string post = msg.msg();
            string str = post.erase(post.length() - 1); // Remove newline character
            string formatted_msg = "@||" + time+"|"+msg.username()+"|"+str+"||N\n"; // use '|' as delimeter
            cout<< "formatted message: " << formatted_msg;
            // store user's post to user_timeline.txt
            string user_timeline = server_directroy_path + "/" + msg.username()+"_timeline.txt";
            ofstream user_file(user_timeline, ios::app|ios::out|ios::in);
            user_file << formatted_msg; // write new post to user's timeline file
            user_file.close();
          }

        }// END of while(!msg_buffer.empty()), sleep 5 sec and check again.
        // chek user timeline connection status
        int user_idx = findClientIdx(to_string(userID));
        Client* user = &client_db[user_idx];
        if(!user->timeline_connected){ // user is not in timeline mode
          cout<< "[Timeline] User " << to_string(userID) << " reconnect and NOT in timeline mode." << endl;
          break;
        }
        sleep(5);
      }// END of while(1)
      cout<< "[Timeline] End process() timeline msg buffer." << endl;
    });

    reader.join();
    process.join();
    
    return Status::OK;
    
  }

}; // end of class SNSServiceImpl

// send the message to the coordinator every 5 sec.
void sendHeartbeat(string coordinatorAddr, string clusterId, string serverId){

  // Create a channel.
  std::shared_ptr<Channel> channel = grpc::CreateChannel(coordinatorAddr, grpc::InsecureChannelCredentials());
  // Create a coordinator stub.
  coord_stub_ = CoordService::NewStub(channel,grpc::StubOptions());
  //cout << "Complete creating a coordinator stub." << endl;

  while(true){
    // sleep for 8 sec
    sleep(8);

    // preapre heartbeat message
    ClientContext context;
    ServerInfo serverInfo;
    serverInfo.set_clusterid(clusterID);
    serverInfo.set_serverid(serverID);
    serverInfo.set_type("Active");
    Confirmation confirmation;
    // send heartbeat to the coordinator
    // cout<< coordinatorAddr<< "|| "<<clusterId<< "|| "<<serverId<< endl;
    // cout<< "stub:" << &stub_ <<" context:" << &context << " serverinfo:"<< &serverInfo << endl;
    cout<< "Sent a heartbeat to the coordinator ("<< coordinatorAddr<< ")"<< endl;
    coord_stub_->Heartbeat(&context, serverInfo, &confirmation);

    
    if(!isMaster){
      // check if the type has became the Master
      // which means the original Master failed, and the coord reassign the Slave
      // to become a new Master
      if(confirmation.type() == "M"){
        isMaster = true;
        cout<< "Server has became the Master!" << endl;
        // change the server directory 
        string old_path = server_directroy_path;
        server_directroy_path = "./server_M_" + clusterId + "_" + serverId;
        copyFiles(old_path, server_directroy_path);
        fs::remove_all(old_path);
      }
    }

  }
}

// when server starts, create its own directory (if not exist!)
void createFolder(string foldername) {
  std::filesystem::path folderpath(foldername);

  // Check if the folder exists.
  if (!std::filesystem::exists(folderpath)) {
    // Create the folder if it does not exist.
    std::filesystem::create_directories(folderpath);
  }
}

void RunServer(string clusterId, string serverId, string coordinatorIP, string coordinatorPort, std::string port_no) {
  string serverIP = "0.0.0.0";
  std::string server_address = serverIP + ":" + port_no;
  SNSServiceImpl service;
  clusterID = stoi(clusterId);
  serverID = stoi(serverId);

  cout<< "Server start running. "<< endl<< "ClusterID: " << clusterId << ", ServerId: "<< serverId
    << ", CoordinatorIP: "<< coordinatorIP<< ", CoordinatorPort:" << coordinatorPort << endl;

  ServerBuilder builder;
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  builder.RegisterService(&service);
  std::unique_ptr<Server> server(builder.BuildAndStart());
  std::cout << "Server listening on " << server_address << std::endl;
  log(INFO, "Server listening on "+server_address);

  // when server start, it connect to the coordinator and start sending Heartbeat messages
  string coordinatorAddr = coordinatorIP + ":" + coordinatorPort;
  // Create a channel.
  std::shared_ptr<Channel> channel = grpc::CreateChannel(coordinatorAddr, grpc::InsecureChannelCredentials());
  // Create a coordinator stub.
  coord_stub_ = CoordService::NewStub(channel,grpc::StubOptions());

  // send Register message
  ClientContext context;
  ServerInfo serverInfo;
  serverInfo.set_clusterid(clusterID);
  serverInfo.set_serverid(serverID);
  serverInfo.set_hostname(serverIP);
  serverInfo.set_port(port_no);
  serverInfo.set_type("");
  Confirmation confirmation;
  // Status Heartbeat(ServerContext* context, const ServerInfo* serverinfo, Confirmation* confirmation) override {
  Status status = coord_stub_->Heartbeat(&context, serverInfo, &confirmation);
  if(status.ok()){
    cout<< "Server registered as: " << confirmation.type() << endl;
    if(confirmation.type() == "M"){
      isMaster = true;
      server_directroy_path = "./server_M_" + clusterId + "_" + serverId;
    }else{
      isMaster = false;
      server_directroy_path = "./server_S_" + clusterId + "_" + serverId;
      // sync all the data with the Master server
      cout<< "Sync all data with the Master server." << endl;
      // TODO: cahnge the implementation
      sync_with_Master();
      cout<< "Sync complete." << endl;
    }
  }

  // create a new thread to send heartbeat periodically
  thread thread(sendHeartbeat, coordinatorAddr, clusterId, serverId);

  // create its own directory
  createFolder(server_directroy_path);

  server->Wait();
}// end RunServer()

int main(int argc, char** argv) {

  string clusterId;
  string serverId;
  string coordinatorIP = "localhost";
  string coordinatorPort = "9090";
  string port = "3010";
  
  int opt = 0;
  while ((opt = getopt(argc, argv, "c:s:h:k:p:")) != -1){
    switch(opt) {
      case 'c':
        clusterId = optarg;break;
      case 's':
        serverId = optarg;break;
      case 'h':
        coordinatorIP = optarg;break;
      case 'k':
        coordinatorPort = optarg;break;
      case 'p':
        port = optarg;break;
      default:
	  std::cerr << "Invalid Command Line Argument\n";
    }
  }
  
  std::string log_file_name = std::string("server-") + port;
  google::InitGoogleLogging(log_file_name.c_str());
  log(INFO, "Logging Initialized. Server starting...");
  RunServer(clusterId, serverId, coordinatorIP, coordinatorPort, port);

  return 0;
}

// read and get all lines in the file under the server directory
// params:
//   1. userID
//   2. fileType (timeline, following, followers)
vector<string> readLines(int userID, string fileType) {
  string filePath = server_directroy_path+ "/"+ to_string(userID)+ "_" + fileType + ".txt";
  vector<string> lines;

  ifstream file(filePath);
  if (!file.is_open()) {
    cerr << "Failed to open file: " << filePath << endl;
    return lines;
  }

  string line;
  while (getline(file, line)) {
    lines.push_back(line);
  }

  file.close();

  return lines;
}

// ask the coordinator for the slave server's address
// and then create the slave server stub
bool registerSlave(int clusterID){
  ClientContext context;
  ServerInfo serverInfo;
  ID id;
  id.set_id(clusterID);

  Status status = coord_stub_->GetSlaveInfo(&context, id, &serverInfo);
  if(status.ok()){
    slaveAddr = serverInfo.hostname() + ":" + serverInfo.port();
    cout<< "Got slave server addr: " << slaveAddr << endl;
  }else{
    cout<< "Failed to get SlaveServer address." << endl;
    return false;
  }

  // Register the Slave server
  std::shared_ptr<Channel> channel2 = grpc::CreateChannel(slaveAddr, grpc::InsecureChannelCredentials());
  slave_stub_ = SNSService::NewStub(channel2,grpc::StubOptions());
  slave_connected = true;
  cout << "Complete creating a slave server stub." << endl;
  return true;
}

bool sync_with_Master(){
  // find the master server's folder
  string folder = "server_M_" + to_string(clusterID) + "_.*";
  regex pattern(folder);
  bool targetFolderFound = false;
  fs::path targetFolder;

  // Iterate over all entries in the current directory
  for (const auto& entry : fs::recursive_directory_iterator(".")) {

      // Check if the entry is a directory
      if (entry.is_directory()) {
          const fs::path path = entry.path();

          // Check if the directory name matches the pattern
          if (regex_match(path.filename().string(), pattern)) {
              // Store the path of the target folder
              targetFolder = path;
              targetFolderFound = true;
              break;
          }
      }
  }

  if (targetFolderFound) {
      // Copy the target folder to "server_b"
      fs::copy(targetFolder, server_directroy_path, fs::copy_options::recursive);
      std::cout << "Sync folder: " << targetFolder << " to " << server_directroy_path << std::endl;
  } else {
      std::cout << "Master folder not found" << std::endl;
      return false;
  }

  return true;
}

