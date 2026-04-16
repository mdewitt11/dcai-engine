package main

import (
	"encoding/json"
	"fmt"
	"math/rand"
	"net"
)

type AdminPayload struct {
	Pass   string `json:"pass"`
	Text   string `json:"text"`
	Energy int    `json:"energy"`
}

const (
	AdminPassword  = " "
	MaxSwarmEnergy = 10
)

func handleAdminConnection(conn net.Conn) {
	defer conn.Close()
	fmt.Printf("\n[Airlock] 🔒 Admin Script connected from %s\n", conn.RemoteAddr().String())

	decoder := json.NewDecoder(conn)
	var payload AdminPayload

	if err := decoder.Decode(&payload); err != nil {
		fmt.Println("[-] Admin payload malformed.")
		_, _ = conn.Write([]byte("ERROR: Invalid JSON Format\n"))
		return
	}

	if payload.Pass != GlobalConfig.AdminKey {
		fmt.Println("[-] Admin Auth Failed! Incorrect password.")
		_, _ = conn.Write([]byte("ERROR: Unauthorized\n"))
		return
	}

	if payload.Energy <= 0 {
		payload.Energy = 5
	}
	if payload.Energy > MaxSwarmEnergy {
		payload.Energy = MaxSwarmEnergy
	}

	// ... (Keep the JSON parsing and password checks the same) ...

	msgID := fmt.Sprintf("msg_%d", rand.Intn(1000000))
	fmt.Printf("[Airlock] 💉 Injecting Genesis Thought: '%s' (Energy: %d)\n", payload.Text, payload.Energy)

	// 1. Create a waiting channel for this specific Python script
	answerChan := make(chan string)

	cacheMutex.Lock()
	adminWaiters[msgID] = answerChan
	cacheMutex.Unlock()

	// 2. Queue it to the C Brain AND Broadcast to the Swarm!
	thoughtQueue <- ThoughtTask{MsgID: msgID, Payload: payload.Text}

	packet := fmt.Sprintf("THOUGHT|%s|%d|0|%s\n", msgID, payload.Energy, payload.Text)
	broadcastRaw(packet, nil)

	// 3. FREEZE AND WAIT! (Goroutine sleeps here until the answer arrives)
	finalAnswer := <-answerChan

	// 4. Send the real answer to Python and close the airlock
	_, _ = conn.Write([]byte(finalAnswer + "\n"))
	_ = conn.Close()

	// Clean up memory
	cacheMutex.Lock()
	delete(adminWaiters, msgID)
	cacheMutex.Unlock()
}
