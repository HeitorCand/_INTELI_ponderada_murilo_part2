package main

type Telemetry struct {
	DeviceID    string      `json:"device_id"`
	Timestamp   string      `json:"timestamp"`
	SensorType  string      `json:"sensor_type"`
	ReadingType string      `json:"reading_type"` // analog | discrete
	Value       interface{} `json:"value"`
}
