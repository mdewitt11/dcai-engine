package main

import (
	"bufio"
	"crypto/tls"
	"fmt"
	"net"
	"strconv"
	"strings"
)

func handleSwarmConnection(conn net.Conn) {
	var remoteNodeID string

	defer func() {
		if remoteNodeID != "" {
			fmt.Printf("[-] Swarm Peer disconnected: %s\n", remoteNodeID)
			peerMutex.Lock()
			delete(activePeers, remoteNodeID)
			peerMutex.Unlock()
		}
		conn.Close()
	}()

	// 1. FIRE MULTIADDR HANDSHAKE
	handshake := fmt.Sprintf("HANDSHAKE|%s|%d\n", localMultiaddr, version)
	conn.Write([]byte(handshake))

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
		// PEX: HANDSHAKE
		// ==========================================
		// ==========================================
		// PEX: HANDSHAKE (Upgraded)
		// ==========================================
		if packetType == "HANDSHAKE" && len(parts) >= 2 {
			multiaddr := parts[1]
			PeerVersion, err := strconv.Atoi(parts[2])
			_, _, nodeID := parseMultiaddr(multiaddr)
			if nodeID == "" {
				continue
			}

			if err != nil {
				panic(err)
			}

			remoteNodeID = nodeID
			fmt.Printf("\n[PEX] 🤝 Handshake verified. Peer Identity: %s\n", nodeID)

			peerMutex.Lock()
			activePeers[nodeID] = &Peer{NodeID: nodeID, Multiaddr: multiaddr, Conn: conn, PeerVersion: PeerVersion}
			peerMutex.Unlock()

			// SUBSET GOSSIPING: Build a list, but cap it to prevent storms!
			var knownAddrs []string
			peerMutex.RLock()
			for id, p := range activePeers {
				if id != nodeID && id != NodeID {
					knownAddrs = append(knownAddrs, p.Multiaddr)
				}
				// Break early if we've gathered enough peers to share
				if len(knownAddrs) >= MaxGossipPeers {
					break
				}
			}
			peerMutex.RUnlock()

			if len(knownAddrs) > 0 {
				reply := fmt.Sprintf("PEERLIST|%s\n", strings.Join(knownAddrs, ","))
				conn.Write([]byte(reply))
				fmt.Printf("[PEX] 📖 Shared %d known peers with %s!\n", len(knownAddrs), nodeID)
			}
		} else

		// ==========================================
		// PEX: AUTO-DISCOVERY (Upgraded)
		// ==========================================
		if packetType == "PEERLIST" && len(parts) >= 2 {
			addrs := strings.Split(parts[1], ",")

			// THE BRAKE: Do we actually need more friends?
			peerMutex.RLock()
			currentPeerCount := len(activePeers)
			peerMutex.RUnlock()

			if currentPeerCount >= MaxActivePeers {
				fmt.Println("[PEX] 🛑 Peer list received, but we are at MAX capacity. Ignoring.")
				continue
			}

			fmt.Printf("\n[PEX] 🔭 Swarm offered %d Multiaddrs. Filtering...\n", len(addrs))

			for _, addrStr := range addrs {
				ip, port, targetID := parseMultiaddr(addrStr)

				// Don't dial ourselves, people we know, OR dial if we hit the cap mid-loop!
				if targetID == NodeID || alreadyKnowsPeer(targetID) {
					continue
				}

				peerMutex.RLock()
				count := len(activePeers)
				peerMutex.RUnlock()

				if count >= MaxActivePeers {
					break // Stop dialing immediately!
				}

				fmt.Printf("[PEX] 🚀 Auto-Dialing new identity: %s...\n", targetID)
				go dialPeer(ip, port)
			}
		} else
		// ==========================================
		// ROUTING: THE BOOMERANG PROTOCOL
		// ==========================================
		if packetType == "THOUGHT" && len(parts) >= 5 {
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
					originConn.Write([]byte(rawMsg + "\n"))
				}
				continue
			}

			// --- ROUTE B: THE FORWARD RIPPLE ---
			cacheMutex.RLock()
			_, duplicate := breadcrumbCache[msgID]
			cacheMutex.RUnlock()

			if duplicate {
				continue // Drop duplicates to prevent infinite loops
			}

			cacheMutex.Lock()
			breadcrumbCache[msgID] = conn
			cacheMutex.Unlock()

			// Send to the local C Brain
			thoughtQueue <- ThoughtTask{MsgID: msgID, Payload: payload}

			// Energy Decay & Propagation
			relayEnergy := energy - 1
			if relayEnergy > 0 {
				relayPacket := fmt.Sprintf("THOUGHT|%s|%d|0|%s\n", msgID, relayEnergy, payload)
				broadcastRaw(relayPacket, conn)
			} else {
				fmt.Printf("[🪃 Boomerang] Energy depleted for %s! Bouncing back to sender.\n", msgID)
				returnPacket := fmt.Sprintf("THOUGHT|%s|0|1|%s (Swarm Reply)\n", msgID, payload)
				conn.Write([]byte(returnPacket))
			}
		}
	}
}

// Parses: /ip4/127.0.0.1/tcp/8000/p2p/NodeID
func parseMultiaddr(maddr string) (string, int, string) {
	parts := strings.Split(maddr, "/")
	if len(parts) >= 7 && parts[1] == "ip4" && parts[3] == "tcp" && parts[5] == "p2p" {
		ip := parts[2]
		port, _ := strconv.Atoi(parts[4])
		id := parts[6]
		return ip, port, id
	}
	return "", 0, ""
}

func dialPeer(ip string, port int) {
	addr := fmt.Sprintf("%s:%d", ip, port)
	conn, err := tls.Dial("tcp", addr, swarmTLSConfig)
	if err == nil {
		fmt.Printf("[PEX] ✅ Successfully securely connected to %s!\n", addr)
		go handleSwarmConnection(conn)
	}
}

func alreadyKnowsPeer(nodeID string) bool {
	peerMutex.RLock()
	defer peerMutex.RUnlock()
	_, exists := activePeers[nodeID]
	return exists
}

// UPGRADED: Now includes detailed terminal logging!
func broadcastRaw(packet string, exclude net.Conn) {
	peerMutex.RLock()
	defer peerMutex.RUnlock()

	broadcastCount := 0
	for id, p := range activePeers {
		if p.Conn != exclude {
			p.Conn.Write([]byte(packet))
			broadcastCount++
			fmt.Printf("[Broadcast] 📡 Relayed packet to peer: %s\n", id)
		}
	}

	if broadcastCount == 0 {
		fmt.Printf("[Broadcast] ⚠️ Notice: Tried to broadcast, but no eligible peers were found!\n")
	}
}
