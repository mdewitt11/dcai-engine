package main

// Copyright (c) 2026 mdewitt11. All Rights Reserved.

/*
#include <stdlib.h>
// Make sure it has BOTH arguments here!
extern void c_receive_thought(char* msg_id, char* msg);
*/
import "C"

import (
	"fmt"
	"net"
	"unsafe"
)

//export StartSwarmNetwork
func StartSwarmNetwork(cConfigPath *C.char) {
	// 1. Convert C string to Go string (This is where the error was!)
	configPath := C.GoString(cConfigPath)

	// 2. Load the config using the path we just converted
	loadConfig(configPath)

	localListenPort = GlobalConfig.Port

	// 3. Boot the AI Worker Pool
	for i := 1; i <= GlobalConfig.NumThreads; i++ {
		go aiWorker()
	}

	// 4. If a Target Node is in the config, auto-dial it!
	if GlobalConfig.TargetIP != "" && GlobalConfig.TargetPort > 0 {
		fmt.Printf("[*] Attempting to connect to Seed Node at %s:%d...\n", GlobalConfig.TargetIP, GlobalConfig.TargetPort)
		go dialPeer(GlobalConfig.TargetPort)
	}

	// 5. Boot the Listeners
	go startListener(GlobalConfig.Port, "SWARM", handleSwarmConnection)
	go startListener(GlobalConfig.AdminPort, "ADMIN", handleAdminConnection)

	select {} // Block forever
}

func startListener(port int, name string, handler func(net.Conn)) {
	addr := fmt.Sprintf("0.0.0.0:%d", port)

	listener, err := net.Listen("tcp", addr)
	if err != nil {
		fmt.Printf("[Engine] Fatal error starting %s port: %v\n", name, err)
		return
	}

	fmt.Printf("[Engine] %s Port %d bound and listening.\n", name, port)

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
