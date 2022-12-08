package networks_project;

@license(LGPL);

network Project_Network
{
 	parameters:
 	       int WS = default(5); 		// The sender window
 	       double TO = default(10);		// The timeout interval in seconds for the Go Back N protocol
 	       double PT = default(0.5);	// The sender’s and receivers’ processing time for each frame
 	       double TD = default(1.0);	// The channel’s transmission delay for each frame
 	       double ED = default(4.0);	// The channel’s error delay for any frame
 	       double DD = default(1.0);	// The channel’s duplication delay before sending the second version
 	       double LP = default(0.1);	// ACK/NACK frame loss probability
 	
 	submodules:
 		// TODO: add coordinator submodule
 		// coordinator: Coordinator
 	    node0: Node;
 	    node1: Node;
 	connections:
 	    // TODO: add coordinator connections with nodes
 	    // node0.in_ports[0] <-- coordinator 
 	   	// node1.in_ports[0] <-- coordinator 
 	   	
 		node0.in_ports[1] <-- node1.out_port;
 		node1.in_ports[1] <-- node0.out_port;
}
