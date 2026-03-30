CREATE TABLE IF NOT EXISTS telemetry_readings (
    id SERIAL PRIMARY KEY,
    device_id VARCHAR(100),
    timestamp TIMESTAMP,
    sensor_type VARCHAR(50),
    reading_type VARCHAR(20),
    value_numeric DOUBLE PRECISION,
    value_boolean BOOLEAN,
    created_at TIMESTAMP DEFAULT NOW()
);