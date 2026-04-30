# 🧬 DCAI Polyglot Engine

[![Docker Multi-Arch](https://img.shields.io/badge/Docker-Multi--Arch-blue?logo=docker)](https://github.com/mdewitt11/dcai-engine/pkgs/container/dcai-engine)
[![Go Version](https://img.shields.io/badge/Go-1.22+-00ADD8?logo=go)](https://golang.org/)
[![C-Brain](https://img.shields.io/badge/C-Pure_Math-A8B9CC?logo=c)](https://en.wikipedia.org/wiki/C_(programming_language))
[![Security: CodeQL](https://github.com/mdewitt11/dcai-engine/actions/workflows/codeql.yml/badge.svg)](https://github.com/mdewitt11/dcai-engine/actions)

A lightweight, experimental peer-to-peer compute network built in Go.

DCAI explores how computation—eventually including AI workloads—can be distributed across a network of independent nodes instead of relying on centralized APIs and infrastructure.

Today, most access to advanced AI systems is tied to usage-based pricing, API keys, and centralized control. This project investigates an alternative approach of sharing compute across peers in a decentralized network


## 🔧 Build

Clone the repository and build:

```bash
git clone https://github.com/mdewitt11/dcai-engine.git
cd dcai-engine
make
```

## 🚀 Run

Start a node:

```./dcai```

On first run:

- If no config file is provided    haring compute across peers in a decentralized network

- the program will look for config.json
- If config.json does not exist, it will generate one automatically
## 🧾 Configuration

Each node uses a ``config.json`` file.

Example:
```json
{
  "port": 8080,
  "admin_port": 9000,
  "admin_key": "change_this_key",
  "target_ip": "127.0.0.1",
  "target_port": 8081,
  "num_threads": 4,
  "max_queue_size": 100
}
```
Field Overview
- port → main communication port
- admin_port → admin interface port
- target_ip / target_port → peer node to connect to
- num_threads → worker threads
- admin_key → admin access key
- max_queue_size → task queue limit
## 🌐 Connecting Nodes

To form a network:

Start the first node (no target)
Configure another node to connect:
```json
{
  "port": 8081,
  "target_ip": "127.0.0.1",
  "target_port": 8080
}
```
Start the second node

### 🧠 Expected Output
```bash
[Node] Listening on port 8080
[Node] Connecting to 127.0.0.1:8080
[Node] Peer connected
```

## ⚠️ Notes
Each node must use a unique port
Nodes connect using target_ip and target_port
This is an early-stage system under active development
