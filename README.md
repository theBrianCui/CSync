# CSync

CSync is an implementation of [Cristian's clock synchronization algorithm](https://en.wikipedia.org/wiki/Cristian%27s_algorithm) in C,
for use on Unix systems over the Internet. CSync features virtual hardware clocks with selectable drift rates, and the 
continuously adjustable software clocks described in Cristian's paper.

Two applications are provided: a **client** and **server**, which communicate via a simple UDP protocol. 
The client runs a software clock and occasionally queries the server for its clock value. The server replies to queries with its own
clock value. The client then adjusts its clock accordingly to mach an estimate of the server's clock value.

### Client Usage
```
Usage: client [server IP] [server port]
              [server simulated drift (PPM)] [client VH drift (PPM)]
              [client-server RHW relative drift (PPM)]
              [simulation runtime (seconds)]
              [rapport period (usec)]
              [timeout (usec)]
              [amortization period (usec)]
              [print frequency (usec)]
```

### Server Usage
```
Usage: server [port] [master drift (PPM)]
```
