package main

import (
	"encoding/json"
	"fmt"
	"math/rand"
	"os"
	"strconv"
)

type NodeConfig struct {
	Port         int    `json:"port"`
	AdminPort    int    `json:"admin_port"`
	AdminKey     string `json:"admin_key"`
	TargetIP     string `json:"target_ip"`
	TargetPort   int    `json:"target_port"`
	NumThreads   int    `json:"num_threads"`
	MaxQueueSize int    `json:"max_queue_size"`
}

// Global config instance that all Go files can read
var (
	GlobalConfig NodeConfig
	NodeID       string
)

func defaultConfig() NodeConfig {
	return NodeConfig{
		Port:         8080,
		AdminPort:    9000,
		AdminKey:     randomKey(16),
		TargetIP:     "127.0.0.1",
		TargetPort:   8081,
		NumThreads:   4,
		MaxQueueSize: 100,
	}
}

func randomKey(length int) string {
	const chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
	key := make([]byte, length)
	for i := range key {
		key[i] = chars[rand.Intn(len(chars))]
	}
	return string(key)
}

func loadConfig(filepath string) {
	// 2. Read the JSON file
	fileData, err := os.ReadFile(filepath)
	if err != nil {
		fmt.Printf("[!] Could not open %s. Generating a new config\n", filepath)

		GlobalConfig = defaultConfig()

		data, _ := json.MarshalIndent(GlobalConfig, "", "  ")
		if err := os.WriteFile(filepath, data, 0o644); err != nil {
			fmt.Println("[!] Failed to create config file:", err)
			return
		}

		fmt.Printf("[*] Default config created at %s\n", filepath)
	} else {
		// 3. The Magic: Parse the JSON straight into the struct!
		if err := json.Unmarshal(fileData, &GlobalConfig); err != nil {
			fmt.Printf("[!] JSON Parse Error in %s.\n", filepath)
		} else {
			fmt.Printf("[*] Loaded config from %s\n", filepath)
		}
	}

	mode := os.Getenv("RUNTIME_MODE")

	if mode == "docker" {
		isDocker = true

		fmt.Println("[Engine] Running in Docker Mode")
		swarmPort := os.Getenv("SWARM_PORT")
		if swarmPort == "" {
			swarmPort = "8080"
		}

		adminPort := os.Getenv("ADMIN_PORT")
		if adminPort == "" {
			adminPort = "9000"
		}

		GlobalConfig.Port, _ = strconv.Atoi(swarmPort)
		GlobalConfig.AdminPort, _ = strconv.Atoi(adminPort)
	} else {
		fmt.Println("[Engine] Running in host Mode")
	}

	// Generate a purely random, anonymous Node ID (e.g., "node-7A2B-9C4F")
	NodeID = fmt.Sprintf("node-%04X-%04X", rand.Intn(0xFFFF), rand.Intn(0xFFFF))
	fmt.Printf("[*] Node ID Generated: %s\n", NodeID)
}
