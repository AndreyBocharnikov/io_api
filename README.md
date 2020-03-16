#### A server that processes each client in his own thread, disconnects clients after 10 minutes of inaction.
A method worker::execute scrolls cycle that interacts with clients: connect and disconnect them, receives their requests and responds 
to them. Class task is a wrapper class for clients. Class server does server logic.  
