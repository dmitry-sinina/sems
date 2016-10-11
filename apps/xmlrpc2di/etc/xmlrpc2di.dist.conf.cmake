# server_ip : IP to bind XMLRPC server to
# leave empty for ANY interface
server_ip=127.0.0.1

# port to bind XMLRPC server to 
xmlrpc_port=8090

# run multi-threaded server? 
# Default: yes
# 
multithreaded = yes

# threads to run - this many requests can be processed in parallel
# Default: 5
#
threads=5

# export all DI functions with the function call 'di'?
# defaults to: yes
export_di=yes

#
# these DI interfaces are searched for functions to 
# export under their proper function names
# defaults to: none
# direct_export=di_dial;registrar_client
direct_export=sbc

# run the XMLRPC server at all (default: yes)
#
run_server=yes 

# timeout for client requests, in milliseconds (0 to disable)
#
server_timeout=500

# timeout for waiting request from the client in mutlithread mode, in seconds (0 to disable)
#
client_request_timeout=0


# print parameters of XMLRPC server calls into debug log [yes|no]
debug_server_params=yes
#
# print result of XMLRPC server calls into debug log [yes|no]
#  debug_server_result=yes
#
