#include <sys/socket.h>   
#include <netinet/in.h>  
#include <arpa/inet.h>    
#include <netdb.h>        
#include <cstdint>        
#include <cstring>        
#include <iomanip>        
#include <iostream>       
#include <memory>         
#include <string>        

using Port = u_int16_t;
using Addrfamily = u_int32_t;
using Ipv4addr = u_int16_t;


struct sockaddr_in4{
   Addrfamily sin4_family;
   Port sin4_port;
   Ipv4addr sin_
}

int main(int argc, char* argv[])
{

    

    return 0;
}