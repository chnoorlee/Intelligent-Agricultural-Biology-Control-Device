# Cruise Planner - AI巡航路径规划

## 功能
- **A*/RRT 路径规划**: 考虑障碍物和鸟群密度风险
- **区域覆盖**: 牛耕法/螺旋覆盖，确保无遗漏
- **密度热力图**: 基于历史数据的KDE鸟群密度估计
- **遗传算法优化**: 优化航点序列，最大化驱鸟效率
- **GPS管理**: KML/GPX/Mission Planner 格式导入导出

## 安装
`ash
pip install -r requirements.txt
`

## 使用
`python
from src.path_planner import AStarPlanner, Waypoint

planner = AStarPlanner()
path = planner.find_path(start_wp, goal_wp)
`

## 算法说明
| 算法 | 适用场景 | 特点 |
|------|----------|------|
| A* | 网格化环境 | 最优路径保证 |
| RRT | 复杂障碍环境 | 快速探索 |
| 遗传算法 | 多航点优化 | 全局优化 |
| KDE | 密度估计 | 自适应带宽 |
