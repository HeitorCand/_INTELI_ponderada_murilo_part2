package main

import (
	"encoding/json"
	"log"
	"time"

	"github.com/streadway/amqp"
)

var channel *amqp.Channel

func InitRabbitMQ() {
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
		log.Fatal(err)
	}

	ch, err := conn.Channel()
	if err != nil {
		log.Fatal(err)
	}

	_, err = ch.QueueDeclare(
		"telemetry_queue",
		true,
		false,
		false,
		false,
		nil,
	)
	if err != nil {
		log.Fatal(err)
	}

	channel = ch
}

func PublishToQueue(t Telemetry) error {
	body, err := json.Marshal(t)
	if err != nil {
		return err
	}

	return channel.Publish(
		"",
		"telemetry_queue",
		false,
		false,
		amqp.Publishing{
			ContentType: "application/json",
			Body:        body,
		},
	)
}
