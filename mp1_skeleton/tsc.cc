#include <iostream>
#include <memory>
#include <thread>
#include <vector>
#include <string>
#include <unistd.h>
#include <csignal>
#include <grpc++/grpc++.h>
#include "client.h"
#include "utils.h"

#include "sns.grpc.pb.h"
using grpc::Channel;
using grpc::ClientContext;
using grpc::ClientReader;
using grpc::ClientReaderWriter;
using grpc::ClientWriter;
using grpc::Status;
using grpc::StatusCode;
using csce438::Message;
using csce438::ListReply;
using csce438::Request;
using csce438::Reply;
using csce438::SNSService;
using namespace std;

void sig_ignore(int sig) {
  std::cout << "Signal caught " + sig;
}

Message MakeMessage(const std::string& username, const std::string& msg) {
    Message m;
    m.set_username(username);
    m.set_msg(msg);
    google::protobuf::Timestamp* timestamp = new google::protobuf::Timestamp();
    timestamp->set_seconds(time(NULL));
    timestamp->set_nanos(0);
    m.set_allocated_timestamp(timestamp);
    return m;
}


class Client : public IClient
{
public:
  Client(const std::string& hname,
	 const std::string& uname,
	 const std::string& p)
    :hostname(hname), username(uname), port(p) {}

  
protected:
  virtual int connectTo();
  virtual IReply processCommand(std::string& input);
  virtual void processTimeline();

private:
  std::string hostname;
  std::string username;
  std::string port;
  
  // You can have an instance of the client stub
  // as a member variable.
  std::unique_ptr<SNSService::Stub> stub_;
  
  IReply Login();
  IReply List();
  IReply Follow(const std::string &username);
  IReply UnFollow(const std::string &username);
  void   Timeline(const std::string &username);
};


///////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////
int Client::connectTo()
{
  // ------------------------------------------------------------
  // In this function, you are supposed to create a stub so that
  // you call service methods in the processCommand/porcessTimeline
  // functions. That is, the stub should be accessible when you want
  // to call any service methods in those functions.
  // Please refer to gRpc tutorial how to create a stub.
  // ------------------------------------------------------------
    
///////////////////////////////////////////////////////////
  string target = hostname + ":" + port;
  // Create a channel.
  std::shared_ptr<Channel> channel = grpc::CreateChannel(target, grpc::InsecureChannelCredentials());
  // Create a client stub.
  stub_ = SNSService::NewStub(channel,grpc::StubOptions());
  cout << "Complete creating a client stub." << endl;

  // Login
  ClientContext context;
  Request request;
  Reply reply;
  request.set_username(username);
  stub_->Login(&context, request, &reply);

  if(reply.msg().empty()){
    cout<< "Server reply empty." << endl;
    return -1;
  }else{
    cout<< reply.msg() << endl;
    if(reply.msg() == "invalid username(Already exists).") {
      return -1;
    }else{
      return 1;
    }
  }

}

IReply Client::processCommand(std::string& input)
{
  // ------------------------------------------------------------
  // GUIDE 1:
  // In this function, you are supposed to parse the given input
  // command and create your own message so that you call an 
  // appropriate service method. The input command will be one
  // of the followings:
  //
  // FOLLOW <username>
  // UNFOLLOW <username>
  // LIST
  // TIMELINE
  // ------------------------------------------------------------
  
  // ------------------------------------------------------------
  // GUIDE 2:
  // Then, you should create a variable of IReply structure
  // provided by the client.h and initialize it according to
  // the result. Finally you can finish this function by returning
  // the IReply.
  // ------------------------------------------------------------
  
  
  // ------------------------------------------------------------
  // HINT: How to set the IReply?
  // Suppose you have "FOLLOW" service method for FOLLOW command,
  // IReply can be set as follow:
  // 
  //     // some codes for creating/initializing parameters for
  //     // service method
  //     IReply ire;
  //     grpc::Status status = stub_->FOLLOW(&context, /* some parameters */);
  //     ire.grpc_status = status;
  //     if (status.ok()) {
  //         ire.comm_status = SUCCESS;
  //     } else {
  //         ire.comm_status = FAILURE_NOT_EXISTS;
  //     }
  //      
  //      return ire;
  // 
  // IMPORTANT: 
  // For the command "LIST", you should set both "all_users" and 
  // "following_users" member variable of IReply.
  // ------------------------------------------------------------

    IReply ire;
    
    string cmd = input.substr(0, input.find(" "));
    cout<<"processCommand: "<< cmd << endl;
    if(cmd == "LIST"){
      return List();
    }else if(cmd == "FOLLOW"){
      string username = input.substr(input.find_first_of(" ")+1, input.length());
      return Follow(username);
    }else if(cmd == "UNFOLLOW"){

    }else if(cmd == "TIMELINE"){
      
    }else{
      // invalid command
      ire.grpc_status = grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "invalid command");
    }
    
    return ire;
}


void Client::processTimeline()
{
    Timeline(username);
}

// List Command
IReply Client::List() {

  IReply ire;

  ClientContext context;
  Request request;
  request.set_username(username);
  ListReply listReply;

  // using stub to call server List() method
  Status status = stub_->List(&context, request, &listReply);
  ire.grpc_status = status;
  if(status.ok()){
    // set iReply.all_users, followers
    for(string s: listReply.all_users()){
      ire.all_users.push_back(s);
    }
    for(string s: listReply.followers()){
      ire.followers.push_back(s);
    }
    ire.comm_status = SUCCESS;
  }

  return ire;
}

// Follow Command        
IReply Client::Follow(const std::string& follow_username) {

  IReply ire;

  // prepare request
  ClientContext context;
  Request request;
  request.set_username(username);
  request.add_arguments(follow_username);
  Reply reply;
  // using stub to call server Follow() method
  Status status = stub_->Follow(&context, request, &reply);
  ire.grpc_status = status;
  if(status.ok()){
    ire.comm_status = SUCCESS;
    cout<< "You are now following " << follow_username << endl;
  }

  return ire;
}

// UNFollow Command  
IReply Client::UnFollow(const std::string& username2) {

    IReply ire;

    /***
    YOUR CODE HERE
    ***/

    return ire;
}

// Login Command  
IReply Client::Login() {

    IReply ire;

    // Clientcontext context;
    // Request request;
    // Reply reply;

    // Status status = stub_->Login(&context, request, &reply); // use stub to call server-side Login method.
    // if (reply.msg() == "you have already joined") {
    //     Connection failed;
    // } else {
    //     Connection succeeded;
    // }

    return ire;
}

// Timeline Command
void Client::Timeline(const std::string& username) {

    // ------------------------------------------------------------
    // In this function, you are supposed to get into timeline mode.
    // You may need to call a service method to communicate with
    // the server. Use getPostMessage/displayPostMessage functions 
    // in client.cc file for both getting and displaying messages 
    // in timeline mode.
    // ------------------------------------------------------------

    // ------------------------------------------------------------
    // IMPORTANT NOTICE:
    //
    // Once a user enter to timeline mode , there is no way
    // to command mode. You don't have to worry about this situation,
    // and you can terminate the client program by pressing
    // CTRL-C (SIGINT)
    // ------------------------------------------------------------
  
    /***
    YOUR CODE HERE
    ***/

}



//////////////////////////////////////////////
// Main Function
/////////////////////////////////////////////
int main(int argc, char** argv) {

  std::string hostname = "localhost";
  std::string username = "default";
  std::string port = "3010";
    
  int opt = 0;
  while ((opt = getopt(argc, argv, "h:u:p:")) != -1){
    switch(opt) {
    case 'h':
      hostname = optarg;break;
    case 'u':
      username = optarg;break;
    case 'p':
      port = optarg;break;
    default:
      std::cout << "Invalid Command Line Argument\n";
    }
  }
      
  std::cout << "Logging Initialized. Client starting...";
  
  Client myc(hostname, username, port);
  
  myc.run();
  
  return 0;
}
