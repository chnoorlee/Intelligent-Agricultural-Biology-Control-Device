#!/usr/bin/env python3
"""
图像预处理模块 — 智能农业生物防控

提供YOLO模型所需的图像预处理功能：
  - Letterbox resize (保持纵横比缩放+填充)
  - 归一化 (HWC→CHW, /255)
  - 数据增强 (训练时使用)
  - 批量预处理

Author: BioControl Team
Version: 2.0.0
"""

from __future__ import annotations

from typing import List, Optional, Tuple

import cv2
import numpy as np


# -----------------------------------------------------------
# 预处理类
# -----------------------------------------------------------

class Preprocessor:
    """可配置的图像预处理器。

    支持链式调用，可按需组合各种预处理操作。

    Example:
        >>> prep = Preprocessor(target_size=(640, 640))
        >>> processed = prep(image).numpy()
    """

    def __init__(
        self,
        target_size: Tuple[int, int] = (640, 640),
        mean: Tuple[float, float, float] = (0.0, 0.0, 0.0),
        std: Tuple[float, float, float] = (1.0, 1.0, 1.0),
        color_space: str = "RGB",
        auto_pad: bool = True,
        scale_fill: bool = False,
        stride: int = 32,
    ) -> None:
        """初始化预处理器。

        Args:
            target_size: 目标尺寸 (h, w)
            mean: 均值
            std: 标准差
            color_space: 颜色空间 ("RGB" 或 "BGR")
            auto_pad: 自动计算最小padding
            scale_fill: 缩放填充模式
            stride: 模型步长倍数
        """
        self.target_size = target_size
        self.mean = np.array(mean, dtype=np.float32).reshape(3, 1, 1)
        self.std = np.array(std, dtype=np.float32).reshape(3, 1, 1)
        self.color_space = color_space
        self.auto_pad = auto_pad
        self.scale_fill = scale_fill
        self.stride = stride

    def __call__(
        self, image: np.ndarray
    ) -> Tuple[np.ndarray, Tuple[float, float], Tuple[float, float]]:
        """完整预处理流程。

        Args:
            image: 输入图像 (H, W, C) BGR

        Returns:
            (processed, ratio, (dw, dh))
            - processed: 预处理后的tensor (1, C, H, W)
            - ratio: 缩放比例 (h_ratio, w_ratio)
            - (dw, dh): padding偏移量
        """
        # Letterbox resize
        img, ratio, (dw, dh) = letterbox_resize(
            image,
            self.target_size,
            stride=self.stride,
            auto=self.auto_pad,
            scale_fill=self.scale_fill,
        )

        # 颜色空间转换
        if self.color_space == "RGB":
            img = cv2.cvtColor(img, cv2.COLOR_BGR2RGB)

        # 归一化
        img = img.astype(np.float32) / 255.0

        # 标准化
        img = (img - self.mean.reshape(3)) / self.std.reshape(3)

        # HWC → CHW → BCHW
        img = img.transpose(2, 0, 1)
        img = np.expand_dims(img, axis=0)
        img = np.ascontiguousarray(img)

        return img, ratio, (dw, dh)


# -----------------------------------------------------------
# Letterbox Resize
# -----------------------------------------------------------

def letterbox_resize(
    image: np.ndarray,
    target_size: Tuple[int, int] = (640, 640),
    stride: int = 32,
    auto: bool = True,
    scale_fill: bool = False,
    color: Tuple[int, int, int] = (114, 114, 114),
) -> Tuple[np.ndarray, Tuple[float, float], Tuple[float, float]]:
    """将图像resize并padding到目标尺寸，保持纵横比。

    Args:
        image: 输入图像 (H, W, C)
        target_size: 目标尺寸 (h, w)
        stride: 步长对齐
        auto: 自动调整padding到stride倍数
        scale_fill: True=拉伸填充, False=letterbox
        color: 填充颜色 (B, G, R)

    Returns:
        (resized_image, (ratio_h, ratio_w), (pad_w, pad_h))
    """
    h, w = image.shape[:2]
    th, tw = target_size

    # 计算缩放比例
    r = min(th / h, tw / w)
    if scale_fill:
        r = max(th / h, tw / w)

    # 不放大 (可选)
    # r = min(r, 1.0)

    new_h, new_w = int(round(h * r)), int(round(w * r))
    ratio = (r, r)

    # Resize
    resized = cv2.resize(
        image, (new_w, new_h), interpolation=cv2.INTER_LINEAR
    )

    # Padding
    dw = tw - new_w
    dh = th - new_h

    if auto:
        # 对齐到stride
        dw = dw % stride
        dh = dh % stride

    dw /= 2
    dh /= 2

    top = int(round(dh - 0.1))
    bottom = int(round(dh + 0.1))
    left = int(round(dw - 0.1))
    right = int(round(dw + 0.1))

    padded = cv2.copyMakeBorder(
        resized, top, bottom, left, right,
        borderType=cv2.BORDER_CONSTANT,
        value=color,
    )

    return padded, ratio, (left, top)


# -----------------------------------------------------------
# 归一化
# -----------------------------------------------------------

def normalize_image(
    image: np.ndarray,
    mean: Tuple[float, float, float] = (0.485, 0.456, 0.406),
    std: Tuple[float, float, float] = (0.229, 0.224, 0.225),
    to_rgb: bool = True,
) -> np.ndarray:
    """归一化图像 [0,255] → 标准化。

    Args:
        image: 输入图像 (H, W, 3) BGR, uint8
        mean: 各通道均值
        std: 各通道标准差
        to_rgb: 是否先转换为RGB

    Returns:
        标准化后的图像 (H, W, 3) float32
    """
    img = image.astype(np.float32) / 255.0

    if to_rgb:
        img = img[:, :, ::-1]  # BGR → RGB

    # 标准化
    img = (img - np.array(mean, dtype=np.float32)) / np.array(std, dtype=np.float32)

    return img.astype(np.float32)


# -----------------------------------------------------------
# 数据增强
# -----------------------------------------------------------

def augment_image(
    image: np.ndarray,
    bboxes: Optional[np.ndarray] = None,
    hsv_h: float = 0.015,
    hsv_s: float = 0.7,
    hsv_v: float = 0.4,
    flip_lr: float = 0.5,
    scale: float = 0.5,
    translate: float = 0.1,
) -> Tuple[np.ndarray, Optional[np.ndarray]]:
    """对图像和标注框应用数据增强。

    增强策略与YOLOv8训练配置兼容。

    Args:
        image: 输入图像 (H, W, C) BGR
        bboxes: 标注框 (N, 5) [cls, cx, cy, w, h] 归一化坐标
        hsv_h: HSV色调扰动比例
        hsv_s: HSV饱和度扰动比例
        hsv_v: HSV亮度扰动比例
        flip_lr: 随机左右翻转概率
        scale: 缩放范围
        translate: 平移范围

    Returns:
        (augmented_image, augmented_bboxes)
    """
    img = image.copy()
    out_bboxes = bboxes.copy() if bboxes is not None else None

    # HSV 颜色增强
    if np.random.random() < 0.5:
        hsv = cv2.cvtColor(img, cv2.COLOR_BGR2HSV).astype(np.float32)

        dh = np.random.uniform(-hsv_h, hsv_h) * 179
        ds = np.random.uniform(-hsv_s, hsv_s)
        dv = np.random.uniform(-hsv_v, hsv_v)

        hsv[:, :, 0] = (hsv[:, :, 0] + dh) % 180
        hsv[:, :, 1] = np.clip(hsv[:, :, 1] * (1 + ds), 0, 255)
        hsv[:, :, 2] = np.clip(hsv[:, :, 2] * (1 + dv), 0, 255)

        img = cv2.cvtColor(hsv.astype(np.uint8), cv2.COLOR_HSV2BGR)

    # 随机水平翻转
    if np.random.random() < flip_lr:
        img = cv2.flip(img, 1)
        if out_bboxes is not None:
            out_bboxes[:, 1] = 1.0 - out_bboxes[:, 1]  # 翻转cx

    # 随机缩放
    if np.random.random() < 0.5:
        s = np.random.uniform(1 - scale, 1 + scale)
        h, w = img.shape[:2]
        new_w, new_h = int(w * s), int(h * s)
        img = cv2.resize(img, (new_w, new_h))

        if new_h > h:
            img = img[(new_h - h) // 2:(new_h - h) // 2 + h, :, :]
        else:
            pad = (h - new_h) // 2
            img = cv2.copyMakeBorder(img, pad, h - new_h - pad, 0, 0,
                                      cv2.BORDER_CONSTANT, value=(114, 114, 114))

        if new_w > w:
            img = img[:, (new_w - w) // 2:(new_w - w) // 2 + w, :]
        else:
            pad = (w - new_w) // 2
            img = cv2.copyMakeBorder(img, 0, 0, pad, w - new_w - pad,
                                      cv2.BORDER_CONSTANT, value=(114, 114, 114))

    return img, out_bboxes


# -----------------------------------------------------------
# 批量预处理
# -----------------------------------------------------------

def preprocess_batch(
    images: List[np.ndarray],
    target_size: Tuple[int, int] = (640, 640),
    normalize: bool = True,
) -> np.ndarray:
    """批量预处理多张图像。

    Args:
        images: 图像列表 (每张: H, W, C, BGR)
        target_size: 目标尺寸
        normalize: 是否归一化

    Returns:
        预处理后的batch tensor (B, C, H, W)
    """
    batch = []

    for img in images:
        # Letterbox resize
        resized, _, _ = letterbox_resize(img, target_size)

        if normalize:
            resized = resized.astype(np.float32) / 255.0

        # HWC → CHW
        resized = resized.transpose(2, 0, 1)
        batch.append(resized)

    return np.stack(batch, axis=0).astype(np.float32)


# -----------------------------------------------------------
# 模块自检
# -----------------------------------------------------------

if __name__ == "__main__":
    print("图像预处理模块 — 自检")

    # 创建测试图像
    test_img = np.random.randint(0, 255, (480, 640, 3), dtype=np.uint8)

    # 1. Letterbox resize
    resized, ratio, (dw, dh) = letterbox_resize(test_img, (640, 640))
    print(f"  Letterbox: {test_img.shape} → {resized.shape}, ratio={ratio}, pad=({dw},{dh})")
    assert resized.shape == (640, 640, 3), "Letterbox尺寸不对"

    # 2. Preprocessor
    prep = Preprocessor(target_size=(640, 640))
    output, r, (d_w, d_h) = prep(test_img)
    print(f"  Preprocessor output shape: {output.shape}")
    assert output.shape == (1, 3, 640, 640), f"预处理输出shape不对: {output.shape}"

    # 3. 归一化
    norm = normalize_image(test_img)
    print(f"  Normalize: min={norm.min():.3f}, max={norm.max():.3f}")

    # 4. 数据增强
    augmented, _ = augment_image(test_img)
    print(f"  Augment: shape={augmented.shape}")

    # 5. 批量预处理
    batch = preprocess_batch([test_img, test_img, test_img])
    print(f"  Batch: shape={batch.shape}")

    print("✅ 预处理模块自检通过!")
