# 后端 API 接口文档

Base URL: http://<server>:8000/api/v1

## 1. 检测结果上报

### POST /detections
上报鸟类检测结果。

`json
{
  "device_id": "LF-DRONE-001",
  "timestamp": "2026-07-27T08:30:00Z",
  "detections": [
    {
      "class": "sparrow",
      "confidence": 0.92,
      "bbox": [120, 80, 200, 160],
      "count": 5
    }
  ],
  "location": {
    "lat": 29.863,
    "lng": 121.541
  }
}
`

Response:
`json
{
  "status": "ok",
  "detection_id": "det-abc123",
  "deter_action": "ultrasound"
}
`

## 2. 统计查询

### GET /statistics?device_id=LF-DRONE-001&start=2026-07-20&end=2026-07-27

Response:
`json
{
  "total_detections": 847,
  "total_deterred": 796,
  "deter_success_rate": 0.94,
  "top_species": [
    {"class": "sparrow", "count": 312},
    {"class": "pigeon", "count": 189}
  ],
  "peak_hours": [6, 7, 17, 18]
}
`

## 3. 告警查询

### GET /alerts?status=active

## 4. 设备状态

### GET /devices/{device_id}

### POST /devices/{device_id}/heartbeat

## 5. 巡航路线

### GET /routes?device_id=LF-DRONE-001

### POST /routes
