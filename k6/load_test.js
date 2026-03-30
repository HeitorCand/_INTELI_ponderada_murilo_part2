import http from 'k6/http';
import { sleep } from 'k6';

export const options = {
  vus: 50,
  duration: '30s',
};

export default function () {
  const payload = JSON.stringify({
    device_id: `device-${Math.floor(Math.random() * 100)}`,
    timestamp: new Date().toISOString(),
    sensor_type: "temperature",
    reading_type: "analog",
    value: Math.random() * 100
  });

  http.post('http://localhost:8080/telemetry', payload, {
    headers: { 'Content-Type': 'application/json' },
  });

  sleep(0.1);
}