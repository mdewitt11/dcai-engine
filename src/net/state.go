package main

import (
	"net"
	"sync"
)

type Peer struct {
	Conn       net.Conn
	ListenPort int
}

type ThoughtTask struct {
	MsgID   string
	Payload string
}

var (
	activePeers = make(map[string]*Peer)
	peerMutex   sync.RWMutex

	breadcrumbCache = make(map[string]net.Conn)
	cacheMutex      sync.RWMutex

	// NEW: A map of channels to hold Python scripts waiting for answers!
	adminWaiters = make(map[string]chan string)

	// Updated to use the struct
	thoughtQueue = make(chan ThoughtTask, 1000)

	localListenPort int
)
