package main

import (
	"bufio"
	"fmt"
	"net"
	"strconv"
	"strings"
)

func handleSwarmConnection(conn net.Conn) {
	peerAddr := conn.RemoteAddr().String()

	peerMutex.Lock()
	activePeers[peerAddr] = &Peer{Conn: conn, ListenPort: 0}
	peerMutex.Unlock()

	defer func() {
		fmt.Printf("[-] Swarm Peer disconnected: %s\n", peerAddr)
		peerMutex.Lock()
		delete(activePeers, peerAddr)
		peerMutex.Unlock()
		_ = conn.Close()
	}()

	// 1. FIRE HANDSHAKE IMMEDIATELY ON CONNECT
	handshake := fmt.Sprintf("HANDSHAKE|%d\n", localListenPort)
	_, _ = conn.Write([]byte(handshake))

	reader := bufio.NewReader(conn)

	for {
		rawMsg, err := reader.ReadString('\n')
		if err != nil {
			return
		}

		rawMsg = strings.TrimSpace(rawMsg)
		if rawMsg == "" {
			continue
		}

		parts := strings.Split(rawMsg, "|")
		packetType := parts[0]

		// ==========================================
		// PEX PROTOCOL: THE HANDSHAKE
		// ==========================================
		if packetType == "HANDSHAKE" && len(parts) >= 2 {
			port, _ := strconv.Atoi(parts[1])
			fmt.Printf("\n[PEX] 🤝 Caught Handshake! Peer is listening on Port %d\n", port)

			peerMutex.Lock()
			if p, exists := activePeers[peerAddr]; exists {
				p.ListenPort = port
			}
			peerMutex.Unlock()

			// THE SEED REPLY: Build a list of all OTHER peers we know
			var knownPorts []string
			peerMutex.RLock()
			for _, p := range activePeers {
				if p.ListenPort > 0 && p.ListenPort != port && p.ListenPort != localListenPort {
					knownPorts = append(knownPorts, strconv.Itoa(p.ListenPort))
				}
			}
			peerMutex.RUnlock()

			// Fire the address book back to the new node!
			if len(knownPorts) > 0 {
				reply := fmt.Sprintf("PEERLIST|%s\n", strings.Join(knownPorts, ","))
				_, _ = conn.Write([]byte(reply))
				fmt.Printf("[PEX] 📖 Sent Peer List (%d peers) back to Port %d!\n", len(knownPorts), port)
			}
		} else

		// ==========================================
		// PEX PROTOCOL: AUTO-DISCOVERY
		// ==========================================
		if packetType == "PEERLIST" && len(parts) >= 2 {
			ports := strings.Split(parts[1], ",")
			fmt.Printf("\n[PEX] 🔭 Caught Peer List! Swarm gave us %d new ports to dial.\n", len(ports))

			for _, portStr := range ports {
				targetPort, _ := strconv.Atoi(portStr)

				// Don't dial ourselves, and don't dial people we already know!
				if targetPort == localListenPort || alreadyKnowsPort(targetPort) {
					continue
				}

				fmt.Printf("[PEX] 🚀 Auto-Dialing new Swarm friend on Port: %d...\n", targetPort)
				go dialPeer(targetPort)
			}
		} else

		// ==========================================
		// ROUTING: THE BOOMERANG PROTOCOL
		// ==========================================
		if packetType == "THOUGHT" && len(parts) >= 5 {
			// FORMAT: THOUGHT | MSG_ID | ENERGY | IS_RETURN | PAYLOAD
			msgID := parts[1]
			energy, _ := strconv.Atoi(parts[2])
			isReturn := parts[3]
			payload := parts[4]

			// --- ROUTE A: THE RETURN RIPPLE ---
			if isReturn == "1" {
				cacheMutex.RLock()
				originConn, exists := breadcrumbCache[msgID]
				cacheMutex.RUnlock()

				if exists {
					fmt.Printf("[🪃 Boomerang] Routing %s backwards along breadcrumb trail...\n", msgID)
					// Forward it EXACTLY as it came in (is_return is still 1)
					_, _ = originConn.Write([]byte(rawMsg + "\n"))
				} else {
					fmt.Printf("[🪃 Boomerang] Trail went cold for %s. Dropping.\n", msgID)
				}
				continue // Skip the rest of the loop!
			}

			// --- ROUTE B: THE FORWARD RIPPLE ---
			// 1. Check for Duplicate/Echo
			cacheMutex.RLock()
			_, duplicate := breadcrumbCache[msgID]
			cacheMutex.RUnlock()

			if duplicate {
				continue // We already saw this! Drop it.
			}

			// 2. Drop the Breadcrumb (Save the socket so we can boomerang later)
			cacheMutex.Lock()
			breadcrumbCache[msgID] = conn
			cacheMutex.Unlock()

			// 3. Process locally in C Brain
			// 3. Process locally in C Brain
			thoughtQueue <- ThoughtTask{MsgID: msgID, Payload: payload} // 4. Energy Decay & Propagation

			relayEnergy := energy - 1
			if relayEnergy > 0 {
				// Relay to the Swarm
				relayPacket := fmt.Sprintf("THOUGHT|%s|%d|0|%s\n", msgID, relayEnergy, payload)
				broadcastRaw(relayPacket, conn)
			} else {
				// ENERGY DEPLETED! Flip the flag and bounce it back!
				fmt.Printf("[🪃 Boomerang] Energy depleted for %s! Bouncing back to sender.\n", msgID)
				returnPacket := fmt.Sprintf("THOUGHT|%s|0|1|%s (Swarm Reply)\n", msgID, payload)
				_, _ = conn.Write([]byte(returnPacket))
			}
		}
	}
}

func dialPeer(port int) {
	addr := fmt.Sprintf("127.0.0.1:%d", port)
	conn, err := net.Dial("tcp", addr)
	if err == nil {
		fmt.Printf("[PEX] ✅ Successfully connected to %d!\n", port)
		go handleSwarmConnection(conn)
	} else {
		fmt.Printf("[PEX] ❌ Failed to auto-dial %d.\n", port)
	}
}

func alreadyKnowsPort(port int) bool {
	peerMutex.RLock()
	defer peerMutex.RUnlock()
	for _, p := range activePeers {
		if p.ListenPort == port {
			return true
		}
	}
	return false
}

func broadcastRaw(packet string, exclude net.Conn) {
	peerMutex.RLock()
	defer peerMutex.RUnlock()
	for _, p := range activePeers {
		if p.Conn != exclude {
			_, _ = p.Conn.Write([]byte(packet))
		}
	}
}
