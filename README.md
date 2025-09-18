<p align="center">
  <img src="LetsGoAppOverview.drawio.svg" alt="Lets Go Architecture" width="400">
</p>



List Technologies Used, C++, CMake, MongoDB, GRPC, Protobuf, etc...  
Stateless Server  
Server Administration  
File and Folder Organization  


flowchart LR
  A[Android Client (Kotlin)] -- gRPC/Protobuf --> S[(C++ Stateless Server)]
  Q[Qt Admin (archived)] -. gRPC/Protobuf .-> S
  S --> M[(MongoDB)]
  subgraph Matching
    J[Mongo Aggregation Pipeline (JS)]
    C[Converter (C++): JS -> C++ pipeline]
  end
  S <-- uses --> C
  C --> J
