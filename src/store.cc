#include "./store.grpc.pb.h"
#include "./vendor.grpc.pb.h"
#include <string>
#include <memory>
#include <grpc/support/log.h>
#include <iostream>
#include <fstream>
#include <grpc++/grpc++.h>
#include "threadpool.h"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;
using grpc::ServerAsyncResponseWriter;
using grpc::ServerCompletionQueue;

using grpc::ClientAsyncResponseReader;
using grpc::CompletionQueue;
using grpc::ClientContext;
using grpc::Channel;

using store::ProductQuery;
using store::ProductReply;
using store::ProductInfo;
using store::Store;

using vendor::BidQuery;
using vendor::BidReply;
using vendor::Vendor;

#include <memory>
#include <string>
ProductQuery query_status;
class StoreImpl final 
{ 
	public: 
	~StoreImpl() 
	{
		server_->Shutdown();
	    cq_->Shutdown();
	}

	void Run(std::string port, std::string vendor_addrs,int threads) 
 	{
		vendor_addr = getVendorAddr(vendor_addrs);    		
		std::string server_address("0.0.0.0:" + port);
	    ServerBuilder builder;
        builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
        builder.RegisterService(&service_);
    	cq_ = builder.AddCompletionQueue();
    	server_ = builder.BuildAndStart();
    	std::cout << "Server listening on " << server_address << std::endl;
    	poolOfThreads = std::make_shared<ThreadPool>(threads);
        HandleRpcs();
  	}

	private:
		class CallData 
		{
	   		public:
			CallData(Store::AsyncService* service, ServerCompletionQueue* cq, std::vector<std::string> addr): service_(service), cq_(cq), responder_(&ctx_),vendor_addr(addr), status_(CREATE) 
		    {
	      		Proceed();
    	    }

			void Proceed() 
   			{
      		    if (status_ == CREATE)
				{
					status_ = PROCESS;
					service_->RequestgetProducts(&ctx_, &request_, &responder_, cq_, cq_,this);
      		    } 
				else if (status_ == PROCESS) 
				{
					for(auto &addr : vendor_addr)
					{
						VendorClient vendor(grpc::CreateChannel(addr, grpc::InsecureChannelCredentials()));
						vendor.getProductBid(request_, reply_);	
					}
				    status_ = FINISH;
        		  	responder_.Finish(reply_, Status::OK, this);
      			} 
				else 
				{
        		 	GPR_ASSERT(status_ == FINISH);
        			delete this;
      		    }
    		}
    			private:
				class VendorClient 
				{
 					public:
  					explicit VendorClient(std::shared_ptr<Channel> channel): stub_(Vendor::NewStub(channel)) {}
  					bool getProductBid(const ProductQuery& request, ProductReply& response) 
    					{
   						BidQuery query;
    					query.set_product_name(request.product_name());
						BidReply reply;     
   						ClientContext context;
   						CompletionQueue cq;
   						Status status;
						std::unique_ptr<ClientAsyncResponseReader<BidReply> > rpc (stub_->AsyncgetProductBid(&context, query, &cq));
    					rpc->Finish(&reply, &status, (void*)1);
						void* got_tag;
    					bool ok = false;
    					GPR_ASSERT(cq.Next(&got_tag, &ok));
    					GPR_ASSERT(got_tag == (void*)1);
    					GPR_ASSERT(ok);
   						if (!status.ok()) 
						{
	    					std::cout << status.error_code() << ": " << status.error_message() << std::endl;
    	    				return false;
  						}
						ProductInfo* prod = response.add_products();
						prod->set_price(reply.price());
						prod->set_vendor_id(reply.vendor_id());
						return true;
					}
					private :
					std::unique_ptr<Vendor::Stub> stub_;
				};
    				Store::AsyncService* service_;
    				ServerCompletionQueue* cq_;
    				ServerContext ctx_;
    				ProductQuery request_;
			        ProductReply reply_;
			        ServerAsyncResponseWriter<ProductReply> responder_;
			        enum CallStatus { CREATE, PROCESS, FINISH };
    				CallStatus status_;  // The current serving state.
    				std::vector<std::string> vendor_addr;
				   
		};

		void HandleRpcs() 
		{ 
		    	while (true) {
                poolOfThreads->enqueueJob([this]{
		    	new CallData(&service_, cq_.get(), vendor_addr);
		    	void* tag; // uniquely identifies a request.
		        bool ok;
      			GPR_ASSERT(cq_->Next(&tag, &ok));
      			GPR_ASSERT(ok);
      			static_cast<CallData*>(tag)->Proceed();			
    		    });
    		    std::this_thread::sleep_for(std::chrono::milliseconds(500));
  		    }
  		}

		std::vector<std::string> getVendorAddr(std::string vendor_addrs)
		{
			std::vector<std::string> AddrVec;
			std::ifstream file(vendor_addrs);
			if(file.is_open())
			{
				std::string ip_addr;
				while(getline(file, ip_addr))
				{
					AddrVec.push_back(ip_addr);
				}
				file.close();
				return AddrVec;
			}
			else
			{
				std::cerr <<"Failed to open file  " << vendor_addrs << std::endl;
				exit(-1);
			}
		} 

 private:
	std::unique_ptr<ServerCompletionQueue> cq_;
	Store::AsyncService service_;
	std::unique_ptr<Server> server_; 
	std::shared_ptr<ThreadPool> poolOfThreads;
	std::vector<std::string> vendor_addr;
};

int main(int argc, char** argv) {
  std::string vendor_addr_file, port;
  int threads;
	if(argc == 4)
	{
		port = std::string(argv[1]);
		vendor_addr_file = std::string(argv[2]);
		threads = atoi(argv[3]);
	}
	else
	{
		std::cerr << "Usage : ./store $server_port $file_path_for_vendor_addrs $numThreads \n"<<std::endl;
		return EXIT_FAILURE;
	}
	StoreImpl server;
	server.Run(port, vendor_addr_file,threads);
	std::cout << "I'm ready now !" << std::endl;
	return EXIT_SUCCESS;
}

