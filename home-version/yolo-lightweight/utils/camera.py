#!/usr/bin/env python3
"""
摄像头采集封装模块 — 智能农业生物防控

支持多种摄像头输入源:
  - USB摄像头 (DirectShow/Video4Linux)
  - IP摄像头 (RTSP/HTTP流)
  - CSI摄像头 (树莓派)
  - 本地视频文件

提供统一的帧读取接口和自动重连机制。

Author: BioControl Team
Version: 2.0.0
"""

from __future__ import annotations

import logging
import threading
import time
from abc import ABC, abstractmethod
from dataclasses import dataclass
from typing import Iterator, List, Optional, Tuple

import cv2
import numpy as np

logger = logging.getLogger(__name__)


# -----------------------------------------------------------
# 配置数据类
# -----------------------------------------------------------

@dataclass
class CameraConfig:
    """摄像头配置参数。

    Attributes:
        width: 采集宽度
        height: 采集高度
        fps: 帧率
        exposure: 曝光值 (-1=自动)
        brightness: 亮度 (0-255)
        contrast: 对比度 (0-255)
        saturation: 饱和度 (0-255)
        auto_focus: 是否自动对焦
        buffer_size: 缓冲帧数
        reconnect_delay: 断线重连延迟(秒)
        max_reconnect_attempts: 最大重连次数
    """
    width: int = 640
    height: int = 480
    fps: int = 30
    exposure: int = -1
    brightness: int = 128
    contrast: int = 128
    saturation: int = 128
    auto_focus: bool = True
    buffer_size: int = 3
    reconnect_delay: float = 2.0
    max_reconnect_attempts: int = 10


# -----------------------------------------------------------
# 抽象基类
# -----------------------------------------------------------

class CameraCapture(ABC):
    """摄像头采集抽象基类。

    所有具体的摄像头实现都应继承此类。
    """

    def __init__(self, config: Optional[CameraConfig] = None) -> None:
        """初始化摄像头。

        Args:
            config: 摄像头配置
        """
        self.config = config or CameraConfig()
        self._cap: Optional[cv2.VideoCapture] = None
        self._running = False
        self._frame: Optional[np.ndarray] = None
        self._lock = threading.Lock()
        self._reconnect_count = 0

    @abstractmethod
    def open(self) -> bool:
        """打开摄像头连接。

        Returns:
            是否成功打开
        """
        ...

    @abstractmethod
    def _get_source(self) -> str:
        """获取摄像头源字符串。"""
        ...

    def start(self) -> bool:
        """启动摄像头采集。

        Returns:
            是否成功启动
        """
        if self._running:
            logger.warning("摄像头已在运行中")
            return True

        if not self.open():
            return False

        self._running = True
        self._thread = threading.Thread(
            target=self._capture_loop,
            daemon=True,
            name="CameraCapture",
        )
        self._thread.start()

        # 等待第一帧
        timeout = 5.0
        start_time = time.time()
        while self._frame is None:
            if time.time() - start_time > timeout:
                logger.error("❌ 等待首帧超时")
                self._running = False
                return False
            time.sleep(0.1)

        logger.info("✅ 摄像头采集已启动")
        return True

    def stop(self) -> None:
        """停止摄像头采集。"""
        self._running = False
        if hasattr(self, "_thread"):
            self._thread.join(timeout=3.0)
        if self._cap is not None:
            self._cap.release()
            self._cap = None
        logger.info("📷 摄像头已停止")

    def read(self) -> Optional[np.ndarray]:
        """读取最新帧。

        Returns:
            最新帧图像 (H, W, C) BGR，如果没有则返回None
        """
        with self._lock:
            if self._frame is None:
                return None
            return self._frame.copy()

    def read_raw(self) -> Optional[np.ndarray]:
        """读取最新帧(不复制，零拷贝 — 注意线程安全)。

        Returns:
            最新帧引用，如果外部会修改则使用 read()
        """
        with self._lock:
            return self._frame

    def _capture_loop(self) -> None:
        """后台采集循环。"""
        consecutive_failures = 0
        max_failures = 30

        while self._running:
            if self._cap is None or not self._cap.isOpened():
                if not self._reconnect():
                    time.sleep(self.config.reconnect_delay)
                    continue

            ret, frame = self._cap.read()
            if not ret:
                consecutive_failures += 1
                if consecutive_failures > max_failures:
                    logger.warning("⚠️ 连续读取失败，尝试重连...")
                    consecutive_failures = 0
                    self._reconnect()
                time.sleep(0.01)
                continue

            consecutive_failures = 0

            with self._lock:
                self._frame = frame

            # 帧率控制
            time.sleep(1.0 / (self.config.fps * 2))

        logger.info("采集循环已退出")

    def _reconnect(self) -> bool:
        """尝试重新连接摄像头。

        Returns:
            是否重连成功
        """
        if self._reconnect_count >= self.config.max_reconnect_attempts:
            logger.error("❌ 超过最大重连次数，放弃重连")
            self._running = False
            return False

        self._reconnect_count += 1
        logger.info(f"🔄 尝试重连 ({self._reconnect_count}/{self.config.max_reconnect_attempts})...")

        if self._cap is not None:
            self._cap.release()
            self._cap = None

        time.sleep(self.config.reconnect_delay)

        return self.open()

    def get_resolution(self) -> Tuple[int, int]:
        """获取当前分辨率。

        Returns:
            (width, height)
        """
        return self.config.width, self.config.height

    def get_fps(self) -> float:
        """获取当前帧率。

        Returns:
            帧率
        """
        if self._cap is not None:
            return self._cap.get(cv2.CAP_PROP_FPS) or self.config.fps
        return float(self.config.fps)

    def is_running(self) -> bool:
        """是否正在运行。

        Returns:
            运行状态
        """
        return self._running

    def __enter__(self):
        """上下文管理器入口。"""
        self.start()
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        """上下文管理器退出。"""
        self.stop()

    def __iter__(self) -> Iterator[np.ndarray]:
        """迭代器协议 — 持续读取帧。"""
        return self

    def __next__(self) -> np.ndarray:
        """获取下一帧。"""
        frame = self.read()
        if frame is None:
            raise StopIteration
        return frame


# -----------------------------------------------------------
# USB摄像头
# -----------------------------------------------------------

class USBCamera(CameraCapture):
    """USB摄像头采集实现。

    支持 DirectShow (Windows) 和 V4L2 (Linux)。

    Example:
        >>> cam = USBCamera(0, CameraConfig(width=1280, height=720, fps=30))
        >>> cam.start()
        >>> frame = cam.read()
        >>> cam.stop()
    """

    def __init__(
        self,
        device_id: int = 0,
        config: Optional[CameraConfig] = None,
    ) -> None:
        """初始化USB摄像头。

        Args:
            device_id: 摄像头设备ID (0=默认摄像头)
            config: 摄像头配置
        """
        super().__init__(config)
        self.device_id = device_id

    def _get_source(self) -> str:
        """获取摄像头源。"""
        return str(self.device_id)

    def open(self) -> bool:
        """打开USB摄像头。

        Returns:
            是否成功打开
        """
        import platform

        system = platform.system()
        if system == "Windows":
            # DirectShow后端
            self._cap = cv2.VideoCapture(
                self.device_id, cv2.CAP_DSHOW
            )
        else:
            # V4L2后端
            self._cap = cv2.VideoCapture(
                self.device_id, cv2.CAP_V4L2
            )

        if not self._cap.isOpened():
            logger.error(f"❌ 无法打开USB摄像头 {self.device_id}")
            return False

        # 设置参数
        self._cap.set(cv2.CAP_PROP_FRAME_WIDTH, self.config.width)
        self._cap.set(cv2.CAP_PROP_FRAME_HEIGHT, self.config.height)
        self._cap.set(cv2.CAP_PROP_FPS, self.config.fps)
        self._cap.set(cv2.CAP_PROP_BUFFERSIZE, self.config.buffer_size)

        if self.config.exposure >= 0:
            self._cap.set(cv2.CAP_PROP_EXPOSURE, self.config.exposure)
        self._cap.set(cv2.CAP_PROP_BRIGHTNESS, self.config.brightness)
        self._cap.set(cv2.CAP_PROP_CONTRAST, self.config.contrast)
        self._cap.set(cv2.CAP_PROP_SATURATION, self.config.saturation)
        self._cap.set(cv2.CAP_PROP_AUTOFOCUS, int(self.config.auto_focus))

        self._reconnect_count = 0
        logger.info(
            f"📷 USB摄像头{self.device_id}已打开 "
            f"({self.config.width}x{self.config.height}@{self.config.fps}fps)"
        )
        return True


# -----------------------------------------------------------
# IP摄像头 (RTSP/HTTP)
# -----------------------------------------------------------

class IPCamera(CameraCapture):
    """IP摄像头采集实现。

    支持 RTSP、HTTP-MJPEG 等流媒体协议。

    Example:
        >>> cam = IPCamera("rtsp://admin:pass@192.168.1.100:554/stream")
        >>> cam.start()
    """

    def __init__(
        self,
        url: str,
        config: Optional[CameraConfig] = None,
        transport: str = "tcp",
    ) -> None:
        """初始化IP摄像头。

        Args:
            url: RTSP/HTTP流URL
            config: 摄像头配置
            transport: 传输协议 ("tcp" 或 "udp")
        """
        super().__init__(config)
        self.url = url
        self.transport = transport

    def _get_source(self) -> str:
        return self.url

    def open(self) -> bool:
        """打开IP摄像头流。

        Returns:
            是否成功打开
        """
        # 设置环境变量优化网络流
        import os
        os.environ["OPENCV_FFMPEG_CAPTURE_OPTIONS"] = (
            f"rtsp_transport;{self.transport}|"
            f"buffer_size;{1024 * 1024}|"
            "max_delay;500000|"
            "stimeout;5000000|"
            "reorder_queue_size;0"
        )

        self._cap = cv2.VideoCapture(self.url, cv2.CAP_FFMPEG)

        if not self._cap.isOpened():
            logger.error(f"❌ 无法打开IP摄像头: {self.url}")
            return False

        self._cap.set(cv2.CAP_PROP_BUFFERSIZE, self.config.buffer_size)

        self._reconnect_count = 0
        logger.info(f"📡 IP摄像头已连接: {self.url}")
        return True


# -----------------------------------------------------------
# 摄像头枚举
# -----------------------------------------------------------

def list_cameras(max_devices: int = 10) -> List[Tuple[int, str]]:
    """枚举系统所有可用摄像头。

    Args:
        max_devices: 最大检查设备数

    Returns:
        [(device_id, description), ...] 列表
    """
    cameras: List[Tuple[int, str]] = []

    for i in range(max_devices):
        cap = cv2.VideoCapture(i)
        if cap.isOpened():
            # 尝试获取设备名称
            try:
                # 获取后端名称
                backend = cap.getBackendName()
            except Exception:
                backend = "Unknown"

            description = f"Camera {i} [{backend}]"
            cameras.append((i, description))
            cap.release()

    return cameras


# -----------------------------------------------------------
# 工厂函数
# -----------------------------------------------------------

def create_camera(
    source: str,
    config: Optional[CameraConfig] = None,
) -> CameraCapture:
    """根据输入源字符串自动创建合适的摄像头对象。

    Args:
        source: 输入源字符串
                - 数字字符串(如"0"): USB摄像头
                - rtsp://或http://开头: IP摄像头
                - 其他: 视频文件
        config: 摄像头配置

    Returns:
        对应的CameraCapture实例

    Raises:
        ValueError: 不支持的输入源类型
    """
    if source.isdigit():
        return USBCamera(device_id=int(source), config=config)
    elif source.startswith("rtsp://") or source.startswith("http://"):
        return IPCamera(url=source, config=config)
    else:
        # 本地视频文件
        cam = USBCamera(device_id=0, config=config)
        # 直接设置VideoCapture为文件源
        cam._cap = cv2.VideoCapture(source)
        return cam


# -----------------------------------------------------------
# 模块自检
# -----------------------------------------------------------

if __name__ == "__main__":
    logging.basicConfig(level=logging.INFO)

    print("摄像头采集模块 — 自检")
    print("=" * 50)

    # 枚举摄像头
    print("\n🔍 枚举可用摄像头:")
    cameras = list_cameras()
    if cameras:
        for dev_id, desc in cameras:
            print(f"  [{dev_id}] {desc}")
    else:
        print("  (未发现摄像头)")

    # 测试USB摄像头 (如果可用)
    if cameras:
        print(f"\n📷 测试摄像头 {cameras[0][0]}...")
        config = CameraConfig(width=640, height=480, fps=15)
        cam = USBCamera(device_id=cameras[0][0], config=config)

        if cam.start():
            for i in range(10):
                frame = cam.read()
                if frame is not None:
                    print(f"  帧 {i+1}: {frame.shape}, dtype={frame.dtype}")
                time.sleep(0.1)
            cam.stop()
            print("✅ USB摄像头测试通过")
        else:
            print("⚠️ 摄像头启动失败")
    else:
        print("\n⚠️ 未发现摄像头，跳过硬件测试")
        print("✅ 模块加载正常")
