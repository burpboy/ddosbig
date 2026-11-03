# BrutalDDoS Architecture Overview

## 1. Design Philosophy
BrutalDDoS is designed as a high-performance, single-binary network stress tool. It prioritizes raw throughput over complex state management, utilizing asynchronous I/O to maximize socket utilization per CPU core. The architecture is modular, separating protocol logic from I/O handling, allowing for easy addition of new attack vectors (e.g., DNS amplification) without touching the core engine.

## 2. High-Level Component Diagram

```mermaid
graph TD
    CLI[CLI Parser] --> Config[ConfigParser]
    Config --> Main[Main Entry Point]
    Main --> Engine[AttackEngine Orchestrator]
    
    subgraph "Core Logic"
        Engine --> Scheduler[Load Balancer & Scheduling]
        Engine --> PacketGen[PacketGenerator]
        Engine --> Fuzzer[Fuzzer Module]
    end
    
    subgraph "Network Layer"
        Scheduler --> SocketMgr[SocketManager]
        SocketMgr --> AsyncIO[AsyncIO (epoll/io_uring)]
        
        subgraph "Protocol Handlers"
            Scheduler --> HttpFlood[HttpFlood Handler]
            Scheduler --> UdpFlood[UdpFlood Handler]
            Scheduler --> SynFlood[SynFlood Handler]
        end
    end
    
    subgraph "Utilities"
        Logger[Logger] -.-> Engine
        Timer[Timer] -.-> Scheduler
        RandomUtils[RandomUtils] -.-> PacketGen
    end
