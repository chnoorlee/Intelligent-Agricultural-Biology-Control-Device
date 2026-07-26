"""
YOLO轻量化鸟类检测工具模块

包含图像预处理、后处理、NMS、摄像头采集等功能。
"""

from .preprocess import (
    Preprocessor,
    letterbox_resize,
    normalize_image,
    augment_image,
    preprocess_batch,
)

from .postprocess import (
    NMSProcessor,
    non_max_suppression,
    scale_coords,
    xywh2xyxy,
    xyxy2xywh,
)

from .camera import (
    CameraCapture,
    USBCamera,
    IPCamera,
    list_cameras,
    CameraConfig,
)

__all__ = [
    # Preprocess
    "Preprocessor",
    "letterbox_resize",
    "normalize_image",
    "augment_image",
    "preprocess_batch",
    # Postprocess
    "NMSProcessor",
    "non_max_suppression",
    "scale_coords",
    "xywh2xyxy",
    "xyxy2xywh",
    # Camera
    "CameraCapture",
    "USBCamera",
    "IPCamera",
    "list_cameras",
    "CameraConfig",
]

__version__ = "2.0.0"
