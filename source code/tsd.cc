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
using csce438::Message;
using csce438::ListReply;
using csce438::Request;
using csce438::Reply;
using csce438::SNSService;
using csce438::CoordService;
using csce438::ServerInfo;
using csce438::Confirmation;
using csce438::ID;
using grpc::Channel;
using grpc::ClientContext;
using namespace std;

struct Client {
  std::string username;
  bool connected = true;
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

// Have an instance of the coordinator stub as a member variable.
std::unique_ptr<CoordService::Stub> stub_;

class SNSServiceImpl final : public SNSService::Service {
  
  Status List(ServerContext* context, const Request* request, ListReply* list_reply) override {
    string username = request->username();
    cout<< username << " use List cmd."<< endl;
    // get all users
    for(Client c : client_db){
      list_reply->add_all_users(c.username);
    }
    // get followes of the current user
    Client user = client_db[findClientIdx(username)];
    for(Client* c : user.client_followers){
      list_reply->add_followers(c->username);
    }

    return Status::OK;
  }

  Status Follow(ServerContext* context, const Request* request, Reply* reply) override {

    string username = request->username();
    string follow_username = request->arguments(0);
    cout<< username << " wants to follow "<< follow_username << endl;

    int user_idx = findClientIdx(username);
    int follow_user_idx = findClientIdx(follow_username);
    
    string errMsg;
    
    if(user_idx == follow_user_idx){ // check if user want to follow himself
      errMsg = "Error: Cannot follow yourself!";
      cout<< errMsg << endl;
      return Status(grpc::StatusCode::INVALID_ARGUMENT, errMsg);
    }else if(follow_user_idx < 0){  // check if follow_user exists
      // follow_user does not exist
      errMsg = follow_username + " does not exist.";
      cout<< errMsg << endl;
      return Status(grpc::StatusCode::NOT_FOUND, errMsg);
    }else{ // chekck if user is already following follow_user
      Client user = client_db[user_idx];
      for(Client* c : user.client_following){
        if(c->username == follow_username){
          errMsg = "Error: Already following " + follow_username;
          cout<< errMsg << endl;
          return Status(grpc::StatusCode::CANCELLED , errMsg);
        }
      }
      // add follow_user to user's following list
      client_db[user_idx].client_following.push_back(&client_db[follow_user_idx]);
      client_db[follow_user_idx].client_followers.push_back(&client_db[user_idx]);
      cout<< username << " is now following "<< follow_username << endl;
      return Status::OK;
    }
    
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
    // check if the username is already in the database
    int user_idx=-1;
    for(int i=0; i<client_db.size(); i++){
      if(client_db[i].username == username){
        user_idx = i;
        break;
      }
    }
    if(user_idx == -1){ // not exist, create a new client
      Client new_client;
      new_client.username = username;
      client_db.push_back(new_client);
      reply_msg = "registered and logged in successfully.";
      reply->set_msg(reply_msg);
    }else{  // exist
      Client *user = &client_db[user_idx];
      if(user->connected){
        reply_msg = "invalid username(Already exists).";
        reply->set_msg(reply_msg);
      }else{
        user->connected = true;
        reply_msg = "logged in successfully.";
        reply->set_msg(reply_msg);
      }
    }
    cout<< username + " " + reply_msg << endl;

    return Status::OK;
  }

  Status Timeline(ServerContext* context, 
		ServerReaderWriter<Message, Message>* stream) override {

    Message msg;
    string formatted_msg;

    while(stream->Read(&msg)) {
      cout <<"Receive post From["+msg.username()+"]: " <<msg.msg()<<endl;
      int user_idx = findClientIdx(msg.username());
      Client* user = &client_db[user_idx];

      // first time into timeline mode?
      if(msg.msg() == "First_time_timeline"){
        user->stream = stream; // record user's stream

        // get 20 newest posts from user's following.txt
        string line;
        vector<string> newest_twenty;
        ifstream in(msg.username()+"_following.txt");
        int count = 0;
        cout<< "following file size: " << user->following_file_size << endl;
        while(getline(in, line)){
          if(user->following_file_size > 20){
	          if(count <= user->following_file_size-20){
              count++;
	            continue;
            }
          }
          newest_twenty.push_back(line);
        }
        cout<< "size: " << newest_twenty.size() << endl;

        //Send the newest messages to the client
        Message new_msg; 
        for(int i = newest_twenty.size(); i>0; i--){
          if(newest_twenty[i].empty()) continue;
          cout<< "newest:" << newest_twenty[i]<< endl;
          vector<string> post_data = splitString(newest_twenty[i], '|');
          cout<< "time_str:" << post_data[0] << " , username:" << post_data[1] << ", msg:" << post_data[2] << endl;
          new_msg.set_time_str(post_data[0]);
          new_msg.set_username(post_data[1]);
          new_msg.set_msg(post_data[2]);
          stream->Write(new_msg);
        }   
        continue; // end of first timeline mode processing

      }else{ // not the first time into timeline mode
        // format the message before storing it.
        string time = google::protobuf::util::TimeUtil::ToString(msg.timestamp());
        msg.set_time_str(time);
        formatted_msg = time+"|"+msg.username()+"|"+msg.msg()+"\n"; // user '|' as delimeter
        cout<< formatted_msg;
        // store user's post to user_timeline.txt
        ofstream user_file(msg.username()+"_timeline.txt", ios::app|ios::out|ios::in);
        user_file << formatted_msg;
        user_file.close();
      }

      // get all followers of the current user
      for(Client* follower : user->client_followers){
        cout<< "write post from["+user->username+"] to ["+follower->username +"]"<< endl;
        // write the message to the follower's stream(if connected and in chat mode)
        if(follower->connected && follower->stream != 0){
          cout<< "write to stream:" << follower->stream << endl;
          follower->stream->Write(msg);
        }
        // write the message to the follower's following file
        string fileName = follower->username + "_following.txt";
        ofstream following_file(fileName, ios::app|ios::out|ios::in);
        following_file << formatted_msg;
        following_file.close();
        follower->following_file_size++; // record current following file size
      }

    }

    return Status::OK;
  }

}; // end of class SNSServiceImpl

// send the message to the coordinator every 5 sec.
void Heartbeat(string coordinatorAddr, string clusterId, string serverId){

  // Create a channel.
  std::shared_ptr<Channel> channel = grpc::CreateChannel(coordinatorAddr, grpc::InsecureChannelCredentials());
  // Create a coordinator stub.
  stub_ = CoordService::NewStub(channel,grpc::StubOptions());
  cout << "Complete creating a coordinator stub." << endl;

  while(true){
    // preapre heartbeat message
    ClientContext context;
    ServerInfo serverInfo;
    serverInfo.set_clusterid(stoi(clusterId));
    serverInfo.set_serverid(stoi(serverId));
    serverInfo.set_type("Active");
    Confirmation confirmation;
    // send heartbeat to the coordinator
    // cout<< coordinatorAddr<< "|| "<<clusterId<< "|| "<<serverId<< endl;
    // cout<< "stub:" << &stub_ <<" context:" << &context << " serverinfo:"<< &serverInfo << endl;
    stub_->Heartbeat(&context, serverInfo, &confirmation);
    cout<< "Sent a heartbeat to the coordinator ("<< coordinatorAddr<< ")"<< endl;

    //this_thread::sleep_for(chrono::seconds(5));
    sleep(5);
  }
}

void RunServer(string clusterId, string serverId, string coordinatorIP, string coordinatorPort, std::string port_no) {
  string serverIP = "0.0.0.0";
  std::string server_address = serverIP + ":" + port_no;
  SNSServiceImpl service;

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
  stub_ = CoordService::NewStub(channel,grpc::StubOptions());

  // send Register message
  ClientContext context;
  ServerInfo serverInfo;
  serverInfo.set_clusterid(stoi(clusterId));
  serverInfo.set_serverid(stoi(serverId));
  serverInfo.set_hostname(serverIP);
  serverInfo.set_port(port_no);
  serverInfo.set_type("Register");
  Confirmation confirmation;
  // Status Heartbeat(ServerContext* context, const ServerInfo* serverinfo, Confirmation* confirmation) override {
  stub_->Heartbeat(&context, serverInfo, &confirmation);

  // create a new thread to send heartbeat periodically
  thread thread(Heartbeat, coordinatorAddr, clusterId, serverId);

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
