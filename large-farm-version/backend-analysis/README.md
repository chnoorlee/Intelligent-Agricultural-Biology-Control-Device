# Backend Analysis - 后端鸟群分析服务

## 功能
- **FastAPI REST API**: 检测数据接入、统计查询、设备管理
- **鸟群行为分析**: 时空分布、活动趋势、周期性规律
- **DeepSeek AI 分析**: 智能模式识别与策略优化建议
- **自动告警**: 高密度鸟群、低电量、信号丢失、驱离失败
- **SQLite 数据存储**: 轻量高性能，无需外部数据库

## API 端点

| 方法 | 路径 | 说明 |
|------|------|------|
| POST | /api/v1/detections | 上报检测结果 |
| GET | /api/v1/statistics | 统计查询 |
| GET | /api/v1/analysis | 行为分析 |
| GET | /api/v1/predict | 活动预测 |
| POST | /api/v1/devices/heartbeat | 设备心跳 |
| GET | /api/v1/alerts | 告警查询 |
| GET | /api/v1/species | 物种列表 |

## 安装
```bash
cd large-farm-version/backend-analysis
pip install -r requirements.txt
```

## 配置
编辑 `config.yaml`，设置 DeepSeek API Key 或通过环境变量:
```bash
export DEEPSEEK_API_KEY="sk-xxx"
```

## 启动
```bash
uvicorn src.app:app --host 0.0.0.0 --port 8000 --reload
```
