package main

import (
	"crypto/ecdsa"
	"crypto/elliptic"
	"crypto/rand"
	"crypto/rsa"
	"crypto/tls"
	"crypto/x509"
	"crypto/x509/pkix"
	"encoding/pem"
	"fmt"
	"math/big"
	"os"
	"time"
)

// Generates an Ephemeral Elliptic-Curve Certificate in RAM
func generateEphemeralCert() tls.Certificate {
	// 1. Generate a secure private key
	priv, err := ecdsa.GenerateKey(elliptic.P256(), rand.Reader)
	if err != nil {
		panic(err)
	}

	// 2. Create a template for our self-signed P2P Certificate
	template := x509.Certificate{
		SerialNumber: big.NewInt(1),
		Subject: pkix.Name{
			Organization: []string{"DCAI Polyglot Swarm"},
		},
		NotBefore:             time.Now(),
		NotAfter:              time.Now().Add(24 * time.Hour), // Keys expire in 24 hours!
		KeyUsage:              x509.KeyUsageKeyEncipherment | x509.KeyUsageDigitalSignature,
		ExtKeyUsage:           []x509.ExtKeyUsage{x509.ExtKeyUsageServerAuth, x509.ExtKeyUsageClientAuth},
		BasicConstraintsValid: true,
	}

	// 3. Digitally sign the certificate using our own private key
	derBytes, err := x509.CreateCertificate(rand.Reader, &template, &template, &priv.PublicKey, priv)
	if err != nil {
		panic(err)
	}

	// 4. Encode to memory blocks (PEM format)
	certPEM := pem.EncodeToMemory(&pem.Block{Type: "CERTIFICATE", Bytes: derBytes})
	privBytes, err := x509.MarshalECPrivateKey(priv)
	if err != nil {
		panic(err)
	}
	keyPEM := pem.EncodeToMemory(&pem.Block{Type: "EC PRIVATE KEY", Bytes: privBytes})

	// 5. Load into Go's native TLS struct
	cert, err := tls.X509KeyPair(certPEM, keyPEM)
	if err != nil {
		panic(err)
	}

	return cert
}

func ensureAdminCerts() {
	// 1. Create the crypto/ directory if it doesn't exist (0700 = strict permissions)
	if err := os.MkdirAll("crypto", 0o700); err != nil {
		fmt.Printf("Fatal error creating crypto directory: %v\n", err)
		os.Exit(1)
	}

	// 2. Point to the new crypto folder
	certPath := "crypto/admin.crt"
	keyPath := "crypto/admin.key"

	// 3. Check if both files already exist
	if _, err := os.Stat(certPath); err == nil {
		if _, err := os.Stat(keyPath); err == nil {
			return // Keys exist, do nothing!
		}
	}

	fmt.Println("[Engine] 🔑 Admin TLS keys missing. Forging new RSA-4096 keys in crypto/ ...")

	// 2. Generate a heavy 4096-bit RSA Key
	priv, err := rsa.GenerateKey(rand.Reader, 4096)
	if err != nil {
		fmt.Printf("Fatal error generating RSA key: %v\n", err)
		os.Exit(1)
	}

	// 3. Create the Certificate Template (Valid for 1 Year)
	template := x509.Certificate{
		SerialNumber: big.NewInt(1),
		Subject: pkix.Name{
			Organization: []string{"DCAI Polyglot Swarm"},
			CommonName:   "localhost",
		},
		NotBefore:             time.Now(),
		NotAfter:              time.Now().Add(365 * 24 * time.Hour),
		KeyUsage:              x509.KeyUsageKeyEncipherment | x509.KeyUsageDigitalSignature,
		ExtKeyUsage:           []x509.ExtKeyUsage{x509.ExtKeyUsageServerAuth},
		BasicConstraintsValid: true,
	}

	// 4. Create the actual certificate bytes
	certBytes, err := x509.CreateCertificate(rand.Reader, &template, &template, &priv.PublicKey, priv)
	if err != nil {
		fmt.Printf("Fatal error creating certificate: %v\n", err)
		os.Exit(1)
	}

	// 5. Write the Private Key to crypto/admin.key
	keyOut, _ := os.Create(keyPath)
	defer keyOut.Close()
	pem.Encode(keyOut, &pem.Block{Type: "RSA PRIVATE KEY", Bytes: x509.MarshalPKCS1PrivateKey(priv)})

	// 6. Write the Certificate to crypto/admin.crt
	certOut, _ := os.Create(certPath)
	defer certOut.Close()
	pem.Encode(certOut, &pem.Block{Type: "CERTIFICATE", Bytes: certBytes})

	fmt.Println("[Engine] ✅ Admin TLS keys forged and saved to crypto/ folder!")
}
