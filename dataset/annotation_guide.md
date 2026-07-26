# YOLO 标注指南

## 标注原则
1. **紧贴目标**：标注框应紧贴鸟体轮廓，包含头、身体、尾羽
2. **遮挡处理**：若鸟被枝叶遮挡超过50%，标注可见部分
3. **小目标**：鸟体小于 20×20 像素时仍要标注
4. **多鸟**：每只鸟单独标注，不合并为一个大框
5. **模糊**：运动模糊但仍可辨识物种的图片，正常标注

## 标注工具使用 (LabelImg)

`ash
# 安装
pip install labelImg

# 启动
labelImg dataset/images/train dataset/classes.txt
`

快捷键：
- W - 创建标注框
- D - 下一张图片
- A - 上一张图片
- Ctrl+S - 保存
- Del - 删除选中框

## YOLO 标注格式

每行一个目标：class_id center_x center_y width height

所有坐标归一化到 [0, 1] 范围：
`
class_id:  类别索引（0-based，对应 classes.txt 顺序）
center_x:  标注框中心 x 坐标 / 图片宽度
center_y:  标注框中心 y 坐标 / 图片高度
width:     标注框宽度 / 图片宽度
height:    标注框高度 / 图片高度
`

示例 label .txt:
`
0 0.5125 0.3875 0.0875 0.1200
4 0.6875 0.5625 0.0625 0.0850
`

## 质量检查
- 标注框面积 ≥ 图片面积的 0.1%
- 每张图片至少 1 个标注框
- class_id 不超过类别总数
- 坐标值必须在 [0, 1] 范围内
