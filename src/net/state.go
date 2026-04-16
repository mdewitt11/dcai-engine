package main

import (
	"crypto/tls"
	"net"
	"sync"
)

// ==========================================
// SWARM STATE MEMORY
// ==========================================

type Peer struct {
	NodeID    string // The cryptographic identity
	Multiaddr string // /ip4/127.0.0.1/tcp/8000/p2p/NodeID
	Conn      net.Conn
}

type ThoughtTask struct {
	MsgID   string
	Payload string
}

var (
	// The Swarm Address Book (Now keyed by NodeID instead of IP!)
	activePeers = make(map[string]*Peer)
	peerMutex   sync.RWMutex

	breadcrumbCache = make(map[string]net.Conn)
	cacheMutex      sync.RWMutex
	adminWaiters    = make(map[string]chan string)
	thoughtQueue    = make(chan ThoughtTask, 1000)

	// NEW: Prevent broadcast storms!
	MaxActivePeers = 5
	MaxGossipPeers = 3

	localListenPort int
	localMultiaddr  string

	// NEW: The Global TLS Configuration for the Swarm
	swarmTLSConfig *tls.Config
)
