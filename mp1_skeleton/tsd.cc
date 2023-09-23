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

    /*********
    YOUR CODE HERE
    **********/

    return Status::OK;
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

    /*********
    YOUR CODE HERE
    **********/
    
    return Status::OK;
  }

};

void RunServer(std::string port_no) {
  std::string server_address = "0.0.0.0:"+port_no;
  SNSServiceImpl service;

  ServerBuilder builder;
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  builder.RegisterService(&service);
  std::unique_ptr<Server> server(builder.BuildAndStart());
  std::cout << "Server listening on " << server_address << std::endl;
  log(INFO, "Server listening on "+server_address);

  server->Wait();
}

int main(int argc, char** argv) {

  std::string port = "3010";
  
  int opt = 0;
  while ((opt = getopt(argc, argv, "p:")) != -1){
    switch(opt) {
      case 'p':
          port = optarg;break;
      default:
	  std::cerr << "Invalid Command Line Argument\n";
    }
  }
  
  std::string log_file_name = std::string("server-") + port;
  google::InitGoogleLogging(log_file_name.c_str());
  log(INFO, "Logging Initialized. Server starting...");
  RunServer(port);

  return 0;
}
