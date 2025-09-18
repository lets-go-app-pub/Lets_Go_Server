# Lets_Go_Server — Stateless C++ Application Server

A stateless C++ gRPC server for the Lets Go platform. It exposes typed APIs to mobile/desktop/web clients, persists to MongoDB, and embeds a matching engine (Mongo aggregation → C++ pipeline converter). I led the architecture and implementation.

> **At a glance**
> - **Stack:** C++ · gRPC/Protobuf · MongoDB (Replica Set) · SSL/TLS
> - **Clients:** Android app, Desktop Admin (Qt), Website (Wordpress)
> - **Hosting:** Separate Linux servers for app + MongoDB replica set (e.g., Linode)

<p align="center">
  <img src="LetsGoAppOverview.drawio.svg" alt="Lets Go Architecture" width="700">
</p>

---

## Why this server is interesting
- **Stateless hub:** horizontal scale behavior; all session data in DB/tokens.
- **Matching engine:** declarative Mongo aggregation compiled to C++ for low overhead.
- **Typed contracts:** gRPC/Protobuf across Android + Desktop Admin.
- **Operational separation:** app nodes and Mongo replica set on independent Linux hosts.

---


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
