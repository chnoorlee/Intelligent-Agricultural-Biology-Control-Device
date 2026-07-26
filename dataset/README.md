# 常见害鸟标注数据集

## 数据来源
- 百度图片/必应图片爬取: ~2000 张
- 自采田间照片: ~800 张（宁波地区）
- Roboflow 公开鸟类数据集: Birds Species Classification
- ImageNet 鸟类子集: ~1500 张

## 数据集规模
| 类别 | 中文名 | 图片数 | 标注框数 |
|------|--------|--------|----------|
| sparrow | 麻雀 | 600 | 1200+ |
| pigeon | 鸽子 | 500 | 800+ |
| crow | 乌鸦 | 350 | 500+ |
| magpie | 喜鹊 | 400 | 600+ |
| starling | 椋鸟 | 300 | 450+ |
| blackbird | 乌鸫 | 250 | 380+ |
| myna | 八哥 | 280 | 420+ |
| bulbul | 白头鹎 | 320 | 480+ |
| **总计** | | **3000** | **4800+** |

## 标注格式
YOLO 格式: class_id cx cy w h (归一化坐标 0-1)
`
0 0.523 0.415 0.089 0.123
3 0.687 0.532 0.076 0.098
`

## 目录结构
`
dataset/
├── images/
│   ├── train/     # 训练集 80%
│   ├── val/       # 验证集 10%
│   └── test/      # 测试集 10%
├── labels/
│   ├── train/
│   ├── val/
│   └── test/
└── scripts/
    └── data_augment.py
`

## 推荐标注工具
- LabelImg (YOLO 格式)
- Roboflow Annotate (在线)
- CVAT (大规模标注)
