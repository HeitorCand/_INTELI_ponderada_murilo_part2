package main

import (
	"database/sql"
	"encoding/json"
	"log"
	"time"

	_ "github.com/lib/pq"
	"github.com/streadway/amqp"
)

type Telemetry struct {
	DeviceID    string      `json:"device_id"`
	Timestamp   string      `json:"timestamp"`
	SensorType  string      `json:"sensor_type"`
	ReadingType string      `json:"reading_type"`
	Value       interface{} `json:"value"`
}

func main() {
	var conn *amqp.Connection
	var err error
	for i := 0; i < 20; i++ {
		conn, err = amqp.Dial("amqp://guest:guest@rabbitmq:5672/")
		if err == nil {
			break
		}
		log.Printf("RabbitMQ not ready, retrying in 3s... (%d/20)", i+1)
		time.Sleep(3 * time.Second)
	}
	if err != nil {
		log.Fatal("Failed to connect to RabbitMQ:", err)
	}

	ch, err := conn.Channel()
	if err != nil {
		log.Fatal(err)
	}

	msgs, err := ch.Consume(
		"telemetry_queue",
		"",
		true,
		false,
		false,
		false,
		nil,
	)

	db, err := sql.Open("postgres", "postgres://postgres:postgres@postgres:5432/telemetry?sslmode=disable")
	if err != nil {
		log.Fatal(err)
	}

	for msg := range msgs {
		var t Telemetry
		json.Unmarshal(msg.Body, &t)

		var valueNumeric *float64
		var valueBoolean *bool

		switch v := t.Value.(type) {
		case float64:
			valueNumeric = &v
		case bool:
			valueBoolean = &v
		}

		_, err := db.Exec(`
			INSERT INTO telemetry_readings 
			(device_id, timestamp, sensor_type, reading_type, value_numeric, value_boolean)
			VALUES ($1,$2,$3,$4,$5,$6)
		`,
			t.DeviceID,
			t.Timestamp,
			t.SensorType,
			t.ReadingType,
			valueNumeric,
			valueBoolean,
		)

		if err != nil {
			log.Println("DB error:", err)
		}
	}
}
