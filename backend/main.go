package main

import (
	"log"
	"net/http"
)

func main() {
	InitRabbitMQ()

	http.HandleFunc("/telemetry", TelemetryHandler)

	log.Println("Server running on :8080")
	log.Fatal(http.ListenAndServe(":8080", nil))
}
