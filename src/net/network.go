package main

// Copyright (c) 2026 mdewitt11. All Rights Reserved.

/*
#include <stdlib.h>
// Make sure it has BOTH arguments here!
extern void c_receive_thought(char* msg_id, char* msg);
*/
import "C"

import (
	"crypto/tls" // Add this!
	"fmt"
	"net"
	"unsafe"
)

//export StartSwarmNetwork
func StartSwarmNetwork(c_config_path *C.char) {
	configPath := C.GoString(c_config_path)
	loadConfig(configPath)
	localListenPort = GlobalConfig.Port
	localMultiaddr = fmt.Sprintf("/ip4/127.0.0.1/tcp/%d/p2p/%s", localListenPort, NodeID)

	// ====================================================
	// 1. BOOT CRYPTO ENGINE
	// ====================================================
	fmt.Println("[Engine] 🔐 Forging Ephemeral TLS Certificates...")
	cert := generateEphemeralCert()
	swarmTLSConfig = &tls.Config{
		Certificates: []tls.Certificate{cert},
		// In P2P, we use self-signed certs, so we bypass traditional CA checks.
		// (A true Level 3 node would verify this against the Multiaddr PeerID!)
		InsecureSkipVerify: true,
	}

	for i := 1; i <= GlobalConfig.NumThreads; i++ {
		go aiWorker()
	}

	if GlobalConfig.TargetIP != "" && GlobalConfig.TargetPort > 0 {
		fmt.Printf("[*] Attempting to connect to Seed Node at %s:%d...\n", GlobalConfig.TargetIP, GlobalConfig.TargetPort)
		go dialPeer(GlobalConfig.TargetIP, GlobalConfig.TargetPort)
	}

	// 2. BOOT LISTENERS (Pass TLS to Swarm, nil to Admin!)
	go startListener(GlobalConfig.Port, "SWARM", handleSwarmConnection, swarmTLSConfig)
	go startListener(GlobalConfig.AdminPort, "ADMIN", handleAdminConnection, nil)

	select {}
}

// Updated to accept an optional TLS configuration
func startListener(port int, name string, handler func(net.Conn), tlsConf *tls.Config) {
	addr := fmt.Sprintf("0.0.0.0:%d", port)

	var listener net.Listener
	var err error

	// If a TLS Config is provided, wrap the socket in crypto!
	if tlsConf != nil {
		listener, err = tls.Listen("tcp", addr, tlsConf)
	} else {
		listener, err = net.Listen("tcp", addr)
	}

	if err != nil {
		fmt.Printf("[Engine] Fatal error starting %s port: %v\n", name, err)
		return
	}
	defer listener.Close()

	if tlsConf != nil {
		fmt.Printf("[Engine] 🛡️  %s Port %d bound and ENCRYPTED.\n", name, port)
	} else {
		fmt.Printf("[Engine] ⚠️  %s Port %d bound (Plaintext).\n", name, port)
	}

	for {
		conn, err := listener.Accept()
		if err != nil {
			continue
		}
		go handler(conn)
	}
}

func aiWorker() {
	for task := range thoughtQueue {
		cMsgID := C.CString(task.MsgID)
		cPayload := C.CString(task.Payload)

		// Hand the ID and the Text to C
		C.c_receive_thought(cMsgID, cPayload)

		C.free(unsafe.Pointer(cMsgID))
		C.free(unsafe.Pointer(cPayload))
	}
}

// ==========================================
// THE C-TO-GO RETURN BRIDGE
// ==========================================
//

//export GoRouteAnswer
func GoRouteAnswer(CmsgID *C.char, cAnswer *C.char) {
	msgID := C.GoString(CmsgID)
	answer := C.GoString(cAnswer)

	cacheMutex.RLock()
	adminChan, isAdmin := adminWaiters[msgID]
	originConn, isSwarm := breadcrumbCache[msgID]
	cacheMutex.RUnlock()

	if isAdmin {
		// Route A: It came from start! Send it down the channel.
		adminChan <- answer
	} else if isSwarm {
		// Route B: It came from the Mesh! Wrap it in a Boomerang and fire it back.
		fmt.Printf("[🪃 Boomerang] Routing C Brain answer back to Swarm for %s\n", msgID)
		returnPacket := fmt.Sprintf("THOUGHT|%s|0|1|%s\n", msgID, answer)
		_, _ = originConn.Write([]byte(returnPacket))
	}
}

func main() {}
