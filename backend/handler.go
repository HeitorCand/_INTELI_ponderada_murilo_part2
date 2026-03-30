package main

import (
	"encoding/json"
	"log"
	"net/http"
)

func TelemetryHandler(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		w.WriteHeader(http.StatusMethodNotAllowed)
		return
	}

	log.Printf("Received request: %+v", r)

	var telemetry Telemetry

	err := json.NewDecoder(r.Body).Decode(&telemetry)
	if err != nil {
		http.Error(w, "Invalid payload", http.StatusBadRequest)
		return
	}

	err = PublishToQueue(telemetry)
	if err != nil {
		http.Error(w, "Failed to enqueue", http.StatusInternalServerError)
		return
	}

	w.WriteHeader(http.StatusAccepted)
}
