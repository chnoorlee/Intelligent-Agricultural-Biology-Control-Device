#!/usr/bin/env python3
"""
后处理/NMS模块 — 智能农业生物防控

提供YOLO模型输出后处理功能：
  - 非极大值抑制 (NMS)
  - 坐标格式转换 (xywh ↔ xyxy)
  - 坐标缩放还原
  - 多类别NMS
  - Soft-NMS

Author: BioControl Team
Version: 2.0.0
"""

from __future__ import annotations

from typing import List, Optional, Tuple

import numpy as np


# -----------------------------------------------------------
# NMS处理器类
# -----------------------------------------------------------

class NMSProcessor:
    """可配置的非极大值抑制处理器。

    Supports: standard NMS, class-aware NMS, Soft-NMS

    Example:
        >>> nms = NMSProcessor(iou_threshold=0.45, conf_threshold=0.25)
        >>> kept = nms(pred_boxes, pred_scores, pred_classes)
    """

    def __init__(
        self,
        iou_threshold: float = 0.45,
        conf_threshold: float = 0.25,
        max_detections: int = 300,
        method: str = "nms",
        soft_sigma: float = 0.5,
        class_agnostic: bool = False,
    ) -> None:
        """初始化NMS处理器。

        Args:
            iou_threshold: IoU阈值，高于此值的框被抑制
            conf_threshold: 置信度阈值，低于此值的框被过滤
            max_detections: 最大保留检测数
            method: NMS方法 ("nms", "soft_nms")
            soft_sigma: Soft-NMS的高斯sigma参数
            class_agnostic: 是否跨类别NMS
        """
        self.iou_threshold = iou_threshold
        self.conf_threshold = conf_threshold
        self.max_detections = max_detections
        self.method = method
        self.soft_sigma = soft_sigma
        self.class_agnostic = class_agnostic

    def __call__(
        self,
        boxes: np.ndarray,
        scores: np.ndarray,
        class_ids: Optional[np.ndarray] = None,
    ) -> Tuple[np.ndarray, np.ndarray, np.ndarray]:
        """执行NMS。

        Args:
            boxes: 边界框 (N, 4) xyxy格式
            scores: 置信度分数 (N,)
            class_ids: 类别ID (N,) (可选)

        Returns:
            (kept_boxes, kept_scores, kept_class_ids)
        """
        # 置信度过滤
        conf_mask = scores >= self.conf_threshold
        boxes = boxes[conf_mask]
        scores = scores[conf_mask]
        if class_ids is not None:
            class_ids = class_ids[conf_mask]

        if len(boxes) == 0:
            if class_ids is not None:
                return (
                    np.zeros((0, 4)),
                    np.zeros((0,)),
                    np.zeros((0,), dtype=np.int64),
                )
            return np.zeros((0, 4)), np.zeros((0,)), None  # type: ignore

        if class_ids is not None and not self.class_agnostic:
            # 按类别分别NMS
            kept_indices = _multiclass_nms(
                boxes, scores, class_ids,
                self.iou_threshold,
                self.max_detections,
                self.method,
                self.soft_sigma,
            )
        else:
            kept_indices = _single_nms(
                boxes, scores,
                self.iou_threshold,
                self.max_detections,
                self.method,
                self.soft_sigma,
            )

        kept_boxes = boxes[kept_indices]
        kept_scores = scores[kept_indices]
        kept_class_ids = class_ids[kept_indices] if class_ids is not None else None

        if kept_class_ids is None:
            return kept_boxes, kept_scores, None  # type: ignore

        return kept_boxes, kept_scores, kept_class_ids


# -----------------------------------------------------------
# NMS实现
# -----------------------------------------------------------

def _compute_iou(box1: np.ndarray, box2: np.ndarray) -> np.ndarray:
    """计算两组框之间的IoU。

    Args:
        box1: (N, 4) xyxy
        box2: (M, 4) xyxy

    Returns:
        IoU矩阵 (N, M)
    """
    # 计算交集
    inter_x1 = np.maximum(box1[:, None, 0], box2[None, :, 0])
    inter_y1 = np.maximum(box1[:, None, 1], box2[None, :, 1])
    inter_x2 = np.minimum(box1[:, None, 2], box2[None, :, 2])
    inter_y2 = np.minimum(box1[:, None, 3], box2[None, :, 3])

    inter_w = np.maximum(0, inter_x2 - inter_x1)
    inter_h = np.maximum(0, inter_y2 - inter_y1)
    inter_area = inter_w * inter_h

    # 各自面积
    area1 = (box1[:, 2] - box1[:, 0]) * (box1[:, 3] - box1[:, 1])
    area2 = (box2[:, 2] - box2[:, 0]) * (box2[:, 3] - box2[:, 1])

    # IoU = inter / (area1 + area2 - inter)
    union = area1[:, None] + area2[None, :] - inter_area
    iou = inter_area / np.maximum(union, 1e-7)

    return iou


def _single_nms(
    boxes: np.ndarray,
    scores: np.ndarray,
    iou_threshold: float,
    max_detections: int,
    method: str = "nms",
    soft_sigma: float = 0.5,
) -> np.ndarray:
    """单类别NMS。

    Args:
        boxes: (N, 4) xyxy
        scores: (N,)
        iou_threshold: IoU阈值
        max_detections: 最大保留数
        method: "nms" 或 "soft_nms"
        soft_sigma: Soft-NMS sigma

    Returns:
        保留的索引数组
    """
    if len(boxes) == 0:
        return np.array([], dtype=np.int64)

    # 按分数降序排列
    order = np.argsort(scores)[::-1]
    boxes = boxes[order]
    scores = scores[order]

    kept: List[int] = []

    while len(boxes) > 0:
        # 保留分数最高的框
        kept.append(order[0])

        if len(boxes) == 1:
            break

        # 计算当前最高分框与剩余框的IoU
        ious = _compute_iou(boxes[:1], boxes[1:])[0]  # shape: (remaining,)

        if method == "soft_nms":
            # Soft-NMS: 根据IoU衰减分数而不是完全移除
            weights = np.exp(-(ious ** 2) / soft_sigma)
            scores[1:] *= weights
            # 移除衰减后分数过低的框
            mask = scores[1:] >= 0.001
            boxes = boxes[1:][mask]
            scores = scores[1:][mask]
            order = order[1:][mask]
            # 重新排序
            sort_idx = np.argsort(scores)[::-1]
            scores = scores[sort_idx]
            boxes = boxes[sort_idx]
            order = order[sort_idx]
        else:
            # Standard NMS
            mask = ious < iou_threshold
            boxes = boxes[1:][mask]
            scores = scores[1:][mask]
            order = order[1:][mask]

        if len(kept) >= max_detections:
            break

    return np.array(kept[:max_detections], dtype=np.int64)


def _multiclass_nms(
    boxes: np.ndarray,
    scores: np.ndarray,
    class_ids: np.ndarray,
    iou_threshold: float,
    max_detections: int,
    method: str = "nms",
    soft_sigma: float = 0.5,
) -> np.ndarray:
    """多类别NMS — 每个类别独立执行NMS。

    Args:
        boxes: (N, 4) xyxy
        scores: (N,)
        class_ids: (N,)
        iou_threshold: IoU阈值
        max_detections: 最大保留数
        method: NMS方法
        soft_sigma: Soft-NMS sigma

    Returns:
        保留的全局索引
    """
    unique_classes = np.unique(class_ids)
    all_kept: List[int] = []

    for cls in unique_classes:
        cls_mask = class_ids == cls
        cls_boxes = boxes[cls_mask]
        cls_scores = scores[cls_mask]
        cls_indices = np.where(cls_mask)[0]

        # 每类最多保留 max_det // num_classes 个
        per_class_max = max(1, max_detections // len(unique_classes))

        kept = _single_nms(
            cls_boxes, cls_scores, iou_threshold, per_class_max, method, soft_sigma
        )

        all_kept.extend(cls_indices[kept].tolist())

    # 按分数重新排序
    all_kept_scores = scores[np.array(all_kept)]
    final_order = np.argsort(all_kept_scores)[::-1]
    result = np.array(all_kept)[final_order]

    return result[:max_detections]


# -----------------------------------------------------------
# 公开NMS接口
# -----------------------------------------------------------

def non_max_suppression(
    prediction: np.ndarray,
    conf_threshold: float = 0.25,
    iou_threshold: float = 0.45,
    max_detections: int = 300,
    classes: Optional[np.ndarray] = None,
    agnostic: bool = False,
    multi_label: bool = False,
    method: str = "nms",
) -> List[np.ndarray]:
    """对YOLO原始输出执行NMS后处理。

    兼容YOLOv8/v5输出格式。

    Args:
        prediction: YOLO输出 (1, 84, 8400) 或其他格式
        conf_threshold: 置信度阈值
        iou_threshold: IoU阈值
        max_detections: 最大检测数
        classes: 只保留指定类别的检测
        agnostic: 类别无关NMS
        multi_label: 多标签模式
        method: NMS方法

    Returns:
        检测结果列表，每个元素 (N, 6) [x1, y1, x2, y2, conf, cls]
    """
    nc = prediction.shape[1] - 4  # 类别数
    xc = prediction[:, 4:].max(1) > conf_threshold
    prediction = prediction[xc]

    if not len(prediction):
        return [np.zeros((0, 6))]

    # 分离box和class
    box = xywh2xyxy(prediction[:, :4])
    cls_scores = prediction[:, 4:]

    output = []
    for i, score in enumerate(cls_scores):
        # 获取大于阈值的类别
        if multi_label:
            i_pos = np.where(score > conf_threshold)[0]
            s = score[i_pos]
        else:
            j = np.argmax(score)
            if score[j] > conf_threshold:
                i_pos = np.array([j])
                s = np.array([score[j]])
            else:
                continue

        if classes is not None:
            valid = np.isin(i_pos, classes)
            i_pos = i_pos[valid]
            s = s[valid]

        for c, sc in zip(i_pos, s):
            b = box[i]
            output.append(np.concatenate([b, [sc], [c]]))

    if not output:
        return [np.zeros((0, 6))]

    output = np.array(output)

    # NMS
    if len(output) > max_detections:
        # 先按置信度取top-k
        order = np.argsort(output[:, 4])[::-1]
        output = output[order[:max_detections * 5]]

    boxes_nms = output[:, :4]
    scores_nms = output[:, 4]
    class_ids_nms = output[:, 5].astype(np.int32)

    if agnostic:
        kept = _single_nms(boxes_nms, scores_nms, iou_threshold, max_detections)
    else:
        kept = _multiclass_nms(
            boxes_nms, scores_nms, class_ids_nms,
            iou_threshold, max_detections, method
        )

    return [output[kept]]


# -----------------------------------------------------------
# 坐标转换
# -----------------------------------------------------------

def xywh2xyxy(x: np.ndarray) -> np.ndarray:
    """将 [cx, cy, w, h] 转换为 [x1, y1, x2, y2]。

    Args:
        x: (N, 4) [cx, cy, w, h]

    Returns:
        (N, 4) [x1, y1, x2, y2]
    """
    y = np.copy(x) if isinstance(x, np.ndarray) else np.array(x)
    y[..., 0] = x[..., 0] - x[..., 2] / 2  # x1
    y[..., 1] = x[..., 1] - x[..., 3] / 2  # y1
    y[..., 2] = x[..., 0] + x[..., 2] / 2  # x2
    y[..., 3] = x[..., 1] + x[..., 3] / 2  # y2
    return y


def xyxy2xywh(x: np.ndarray) -> np.ndarray:
    """将 [x1, y1, x2, y2] 转换为 [cx, cy, w, h]。

    Args:
        x: (N, 4) [x1, y1, x2, y2]

    Returns:
        (N, 4) [cx, cy, w, h]
    """
    y = np.copy(x) if isinstance(x, np.ndarray) else np.array(x)
    y[..., 0] = (x[..., 0] + x[..., 2]) / 2  # cx
    y[..., 1] = (x[..., 1] + x[..., 3]) / 2  # cy
    y[..., 2] = x[..., 2] - x[..., 0]        # w
    y[..., 3] = x[..., 3] - x[..., 1]        # h
    return y


def scale_coords(
    coords: np.ndarray,
    img_shape: Tuple[int, int],
    orig_shape: Tuple[int, int],
    ratio_pad: Optional[Tuple[float, float, float, float]] = None,
) -> np.ndarray:
    """将模型输入尺度上的坐标还原到原始图像尺度。

    Args:
        coords: 坐标 (N, 4) xyxy 在模型输入尺度上
        img_shape: 模型输入尺寸 (h, w)
        orig_shape: 原始图像尺寸 (h, w)
        ratio_pad: (ratio_h, ratio_w, pad_w, pad_h) 从letterbox获得

    Returns:
        还原后的坐标 (N, 4) xyxy
    """
    if ratio_pad is None:
        gain = min(img_shape[0] / orig_shape[0], img_shape[1] / orig_shape[1])
        pad_w = (img_shape[1] - orig_shape[1] * gain) / 2
        pad_h = (img_shape[0] - orig_shape[0] * gain) / 2
    else:
        ratio_h, ratio_w, pad_w, pad_h = ratio_pad
        gain = ratio_h  # 使用h的比例(两者应该相同)

    coords[:, [0, 2]] -= pad_w   # x padding
    coords[:, [1, 3]] -= pad_h   # y padding
    coords[:, :4] /= gain

    # 裁剪到图像范围内
    coords[:, 0] = np.clip(coords[:, 0], 0, orig_shape[1])
    coords[:, 1] = np.clip(coords[:, 1], 0, orig_shape[0])
    coords[:, 2] = np.clip(coords[:, 2], 0, orig_shape[1])
    coords[:, 3] = np.clip(coords[:, 3], 0, orig_shape[0])

    return coords


def clip_boxes(boxes: np.ndarray, shape: Tuple[int, int]) -> np.ndarray:
    """将边界框裁剪到图像范围内。

    Args:
        boxes: (N, 4) xyxy格式
        shape: 图像尺寸 (h, w)

    Returns:
        裁剪后的框
    """
    boxes[:, [0, 2]] = np.clip(boxes[:, [0, 2]], 0, shape[1])
    boxes[:, [1, 3]] = np.clip(boxes[:, [1, 3]], 0, shape[0])
    return boxes


# -----------------------------------------------------------
# 模块自检
# -----------------------------------------------------------

if __name__ == "__main__":
    print("后处理/NMS模块 — 自检")

    # 1. 坐标转换
    cxcywh = np.array([[320, 240, 100, 80], [160, 120, 50, 40]])
    xyxy = xywh2xyxy(cxcywh)
    print(f"  xywh2xyxy: {cxcywh[0]} → {xyxy[0]}")
    assert xyxy.shape == (2, 4)

    back = xyxy2xywh(xyxy)
    print(f"  xyxy2xywh: {xyxy[0]} → {back[0]}")
    assert np.allclose(cxcywh, back, atol=1e-6)

    # 2. NMS
    nms_boxes = np.array([
        [100, 100, 200, 200],
        [105, 105, 195, 195],
        [300, 300, 400, 400],
        [110, 110, 190, 190],
    ], dtype=np.float32)
    nms_scores = np.array([0.9, 0.8, 0.95, 0.7], dtype=np.float32)
    nms_cls = np.array([0, 0, 0, 0], dtype=np.int32)

    processor = NMSProcessor(iou_threshold=0.5, conf_threshold=0.25)
    kept_boxes, kept_scores, kept_cls = processor(nms_boxes, nms_scores, nms_cls)
    print(f"  NMS: {len(nms_boxes)} → {len(kept_boxes)} boxes")
    assert len(kept_boxes) >= 2, "NMS结果过少"

    # 3. Scale coords
    coords = np.array([[100, 50, 200, 150]], dtype=np.float32)
    scaled = scale_coords(coords, (640, 640), (480, 640))
    print(f"  Scale coords: {coords[0]} → {scaled[0]}")

    print("✅ 后处理模块自检通过!")
