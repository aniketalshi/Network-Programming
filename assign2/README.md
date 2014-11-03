Readme
------

CSE - 533 -- ASSIGNMENT 2
-------------------------

Group Members
-------------
        
        Aniket Alshi        -       109114640
        Swapnil Dinkar      -       109157070

Objective
---------

    Develop a reliable file transfer protocol with Flow Control and
    Congestion Control using UDP.

DESIGN
------

= TCP MECHANISMS
  --------------

    Header
    ------

    -------------------------
    |       MSG_TYPE        |
    |-----------------------|
    |       SEQ_NUM         |
    |-----------------------|
    |       TIMESTAMP       |
    |-----------------------|
    |       WIN_SIZE        |
    |-----------------------|
    |                       |
    |       DATA            |
    |                       |
    |_______________________|


    Reliable Data Transmission
    --------------------------
        The server waits for every ACK, and retransmits if timeout
        occurs - i.e. it does not receive an ACK for a particular
        packet. The retransmit might occur because of 2 reasons:
            
            1.  The client sent 3 duplicate ACKS, i.e. the client 
                received 3 packets other than the next in sequence packet.
                In this case it sends all the packets in its window 
                (depending on server congestion window size and the 
                advertising windoe received from client).
            2.  The server does not hear from the client for the current
                RTO secons; in this case it updates the RTO, and retransmits
                the packet. It goes on retransmitting until it receives 
                the correct ACK from the client. The maximum number of 
                retransmits that server will do before giving up is 12.

    Flow Control
    ------------

        FLow control makes sure that the server only sends the client 
        only those many packets which the client can store in its buffer.
        The client sends its initial window size to server in the third
        handshake. From there on, it sends the window size to server 
        along with every ACK. The server uses this window size and only sends
        that many packets to the client.

    Congestion Control
    ------------------

        The server uses a congestion window to handle congestion in network.
        The 'cwnd' and 'ssthresh' define and control how much packets
        are sent over the network. 
        
            1.  Slow Start
                Here, the 'cwnd' starts with 1, and goes on increasing as
                we receive successful ACKS. We increase the 'cwnd' size 
                by 1 for every successful ACK received, until 'cwnd' becomes
                equal to 'ssthresh'.
            2.  Congestion Avoidance
                Here, the 'cwnd' increases by 1, when 'cwnd' is more than
                'ssthresh' and the server has received ACKS for all the 
                sent packets. Also, if a timeout occurs while receiving
                an ACK, this makes the 'ssthresh' equals to half of 
                'cwnd' and 'cwnd' equal to 1.
            3.  Fast Recovery
                Here, the 'cwnd' and 'ssthresh' becomes half of 'cwnd' when
                we receive three ACKs for a packet.
        
    Probing
    -------

        Probing occurs when the server sends the last packet, and gets the 
        ACK from client with window size 0. In this case, the server cannot
        send the packet to the client, so it has to wait untill client 
        buffer becomes empty. It does so by sending a PROBE packet, which
        basically asks client to send an ACK with its current window size,
        which we can use to check if we can send more packets. The server
        probes the client after every 5 seconds, and this goes on until
        it receives a window size greater than 10.

    Fin-Ack
    -------
        In order to handle the case when the server FIN packet is dropped,
        the server sends FIN 

    File Not Found
    --------------
        The server sends FIN when it does not find the file client looking
        for. The client comes to know about this error if the first packet
        it receives is FIN.

->CLIENT
  ------

    

= MODIFICATIONS
  -------------

    RTT Library:

    1.  We have used itimers here since we are dealing with timeout values
        which are in sub-second reange.
    2.  rtt_init(), rtt_ts(), rtt_start() and rtt_stop() measure time in
        milliseconds and not seconds. They also deal with integers rather
        than floats.
        
    Client

    1.  The client drops the packets received with some probability received
        from client.in file and the consumer sleeps for random ms.



