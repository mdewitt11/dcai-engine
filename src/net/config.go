package main

import (
	"encoding/json"
	"fmt"
	"math/rand"
	"os"
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

func loadConfig(filepath string) {
	// 1. Set Defaults (Just like your old C code)
	GlobalConfig = NodeConfig{
		Port:         8000,
		AdminPort:    8080,
		AdminKey:     "default_secret_key",
		NumThreads:   4,
		MaxQueueSize: 1000,
	}

	// 2. Read the JSON file
	fileData, err := os.ReadFile(filepath)
	if err != nil {
		fmt.Printf("[!] Could not open %s. Using defaults.\n", filepath)
	} else {
		// 3. The Magic: Parse the JSON straight into the struct!
		if err := json.Unmarshal(fileData, &GlobalConfig); err != nil {
			fmt.Printf("[!] JSON Parse Error in %s. Using defaults.\n", filepath)
		} else {
			fmt.Printf("[*] Loaded config from %s\n", filepath)
		}
	}

	// Generate a purely random, anonymous Node ID (e.g., "node-7A2B-9C4F")
	NodeID = fmt.Sprintf("node-%04X-%04X", rand.Intn(0xFFFF), rand.Intn(0xFFFF))
	fmt.Printf("[*] Node ID Generated: %s\n", NodeID)
}
