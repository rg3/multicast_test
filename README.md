This repo is forked from https://github.com/rg3/multicast_test.
New features includes:

1. ssm support
2. add log file multicast_receive.log

How to Use

1. run 'make' in source folder, it will gerenate 2 executable file: multicast_receive, multicast_send

2. to send multicast: ./multicast_send 239.1.1.1 1234 192.168.1.132 "test message"

    239.1.1.1 is multicast group as destination ip,  1234 is udp port  
    
    192.168.1.132 is interface ip where this multicast packet is sent
    
3.  to receive ssm multicast: 

     ./multicast_receive 239.1.1.1:1234:192.168.1.132:192.168.2.100
     
     239.1.1.1 is multicast group as destination ip,  1234 is udp port
     
     192.168.1.132 is source ip where this multicast packet is originated
     
     192.168.2.100 is interface ip where this multicast packet is incoming
