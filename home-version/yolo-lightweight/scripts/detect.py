#!/usr/bin/env python3
"""
YOLOv8 鸟类实时检测推理脚本

支持摄像头实时检测、图片/视频检测、串口信号输出控制STM32驱离装置。
集成鸟类分类判断（害鸟/益鸟），自动决策是否触发驱离动作。

Usage:
    python detect.py --source 0                 # 摄像头实时检测
    python detect.py --source test.jpg          # 单张图片检测
    python detect.py --source birds.mp4         # 视频检测
    python detect.py --source 0 --serial COM3   # 摄像头检测+串口控制

Author: BioControl Team
Version: 2.0.0
"""

from __future__ import annotations

import argparse
import logging
import os
import sys
import time
from datetime import datetime
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple

import cv2
import numpy as np
import serial
import torch
import yaml

# 添加项目根目录到path
sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from models.bird_classes import (
    BIRD_SPECIES,
    evaluate_detection,
    get_name_cn,
    is_beneficial,
    is_pest,
)

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s] %(message)s",
)
logger = logging.getLogger(__name__)


# -----------------------------------------------------------
# 串口通信模块
# -----------------------------------------------------------

class SerialController:
    """STM32串口控制器 — 通过UART发送驱离指令到主控板。

    Protocol:
        起始字节 0xAA | 命令类型 1B | 参数长度 1B | 参数 N字节 | 校验和 1B | 结束字节 0xBB

    命令类型:
        0x01 — 启动蜂鸣器 (参数: 频率Hz(2B) + 持续时间ms(2B))
        0x02 — 停止蜂鸣器
        0x03 — 启动爆闪灯 (参数: 闪烁频率Hz(1B) + 持续时间ms(2B))
        0x04 — 停止爆闪灯
        0x05 — 启动全部驱离 (紧急模式)
        0x06 — 停止全部
        0x07 — 心跳/PING
        0x08 — 激活超声波模块
        0x09 — 关闭超声波模块
    """

    def __init__(
        self,
        port: str,
        baudrate: int = 115200,
        timeout: float = 0.5,
    ) -> None:
        """初始化串口连接。

        Args:
            port: 串口号 (Windows: COM3, Linux: /dev/ttyUSB0)
            baudrate: 波特率
            timeout: 读取超时(秒)
        """
        self.port = port
        self.baudrate = baudrate
        self.timeout = timeout
        self.ser: Optional[serial.Serial] = None
        self._connect()

    def _connect(self) -> bool:
        """建立串口连接。

        Returns:
            连接是否成功
        """
        try:
            self.ser = serial.Serial(
                port=self.port,
                baudrate=self.baudrate,
                timeout=self.timeout,
                bytesize=serial.EIGHTBITS,
                parity=serial.PARITY_NONE,
                stopbits=serial.STOPBITS_ONE,
            )
            logger.info(f"✅ 串口已连接: {self.port} @ {self.baudrate}bps")
            return True
        except serial.SerialException as e:
            logger.warning(f"⚠️ 串口连接失败 ({self.port}): {e}")
            self.ser = None
            return False

    def _checksum(self, data: bytes) -> int:
        """计算校验和 (XOR所有字节)。

        Args:
            data: 要计算校验和的数据

        Returns:
            校验和字节
        """
        result = 0
        for b in data:
            result ^= b
        return result & 0xFF

    def _send_command(self, cmd_type: int, params: bytes = b"") -> bool:
        """发送命令帧到STM32。

        Args:
            cmd_type: 命令类型
            params: 参数数据

        Returns:
            发送是否成功
        """
        if self.ser is None:
            return False

        frame = bytearray()
        frame.append(0xAA)           # 帧头
        frame.append(cmd_type)       # 命令类型
        frame.append(len(params))    # 参数长度
        frame.extend(params)         # 参数
        frame.append(self._checksum(
            bytes([cmd_type, len(params)]) + params
        ))                           # 校验和
        frame.append(0xBB)           # 帧尾

        try:
            self.ser.write(bytes(frame))
            self.ser.flush()
            return True
        except serial.SerialException as e:
            logger.error(f"❌ 串口发送失败: {e}")
            return False

    def trigger_buzzer(self, freq_hz: int = 2000, duration_ms: int = 3000) -> bool:
        """触发蜂鸣器驱离。

        Args:
            freq_hz: 蜂鸣器频率(Hz)，建议1000-4000
            duration_ms: 持续时间(ms)

        Returns:
            发送是否成功
        """
        params = struct.pack(">HH", freq_hz, duration_ms) if hasattr(__import__("struct"), "pack") else bytes([freq_hz >> 8, freq_hz & 0xFF, duration_ms >> 8, duration_ms & 0xFF])
        return self._send_command(0x01, params)

    def trigger_strobe(self, flash_freq: int = 5, duration_ms: int = 5000) -> bool:
        """触发挥爆闪灯。

        Args:
            flash_freq: 闪烁频率(Hz)，1-20
            duration_ms: 持续时间(ms)

        Returns:
            发送是否成功
        """
        params = bytes([flash_freq, duration_ms >> 8, duration_ms & 0xFF])
        return self._send_command(0x03, params)

    def trigger_full_deterrence(self) -> bool:
        """紧急模式：同时启动所有驱离设备。

        Returns:
            发送是否成功
        """
        return self._send_command(0x05)

    def stop_all(self) -> bool:
        """停止所有驱离设备。

        Returns:
            发送是否成功
        """
        return self._send_command(0x06)

    def ping(self) -> bool:
        """心跳检测。

        Returns:
            是否收到回应
        """
        if not self._send_command(0x07):
            return False
        try:
            if self.ser and self.ser.in_waiting >= 2:
                resp = self.ser.read(2)
                return resp == b"\xAA\xBB"
        except Exception:
            pass
        return False

    def close(self) -> None:
        """关闭串口连接。"""
        if self.ser and self.ser.is_open:
            self.ser.close()
            logger.info("🔌 串口已关闭")


# 导入struct (used in SerialController)
import struct


# -----------------------------------------------------------
# 检测结果可视化
# -----------------------------------------------------------

COLOR_PEST = (0, 0, 255)       # 红色 — 害鸟
COLOR_BENEFICIAL = (0, 255, 0)  # 绿色 — 益鸟
COLOR_NEUTRAL = (255, 255, 0)   # 青色 — 未知


def draw_detections(
    image: np.ndarray,
    detections: List[Dict[str, Any]],
    show_labels: bool = True,
    show_confidence: bool = True,
) -> np.ndarray:
    """在图像上绘制检测框和标签。

    Args:
        image: 原始图像 (BGR)
        detections: 检测结果列表
        show_labels: 是否显示标签
        show_confidence: 是否显示置信度

    Returns:
        带标注的图像
    """
    img = image.copy()
    h, w = img.shape[:2]

    for det in detections:
        cid = det.get("class_id", -1)
        conf = det.get("confidence", 0.0)
        bbox = det.get("bbox", [0, 0, 0, 0])  # xyxy

        x1, y1, x2, y2 = [int(v) for v in bbox]

        # 颜色选择
        if is_pest(cid):
            color = COLOR_PEST
            label_prefix = "⚠️ 害鸟"
        elif is_beneficial(cid):
            color = COLOR_BENEFICIAL
            label_prefix = "✅ 益鸟"
        else:
            color = COLOR_NEUTRAL
            label_prefix = "❓"

        # 绘制边框
        thickness = 2 if is_pest(cid) else 1
        cv2.rectangle(img, (x1, y1), (x2, y2), color, thickness)

        # 绘制标签
        if show_labels:
            name = get_name_cn(cid)
            if show_confidence:
                label = f"{label_prefix} {name} {conf:.2f}"
            else:
                label = f"{label_prefix} {name}"

            (label_w, label_h), baseline = cv2.getTextSize(
                label, cv2.FONT_HERSHEY_SIMPLEX, 0.5, 1
            )
            cv2.rectangle(
                img,
                (x1, y1 - label_h - baseline - 5),
                (x1 + label_w, y1),
                color,
                -1,
            )
            cv2.putText(
                img,
                label,
                (x1, y1 - 5),
                cv2.FONT_HERSHEY_SIMPLEX,
                0.5,
                (255, 255, 255),
                1,
                cv2.LINE_AA,
            )

    return img


# -----------------------------------------------------------
# 检测主逻辑
# -----------------------------------------------------------

class BirdDetector:
    """鸟类检测器 — 封装YOLO模型加载和推理。"""

    def __init__(
        self,
        model_path: str,
        conf_threshold: float = 0.25,
        iou_threshold: float = 0.45,
        device: str = "cpu",
    ) -> None:
        """初始化检测器。

        Args:
            model_path: 模型权重路径 (.pt 或 .onnx)
            conf_threshold: 置信度阈值
            iou_threshold: NMS IoU阈值
            device: 推理设备
        """
        self.conf_threshold = conf_threshold
        self.iou_threshold = iou_threshold
        self.device = device
        self.model_path = model_path
        self.model: Any = None
        self.is_onnx = model_path.endswith(".onnx")

        self._load_model()

    def _load_model(self) -> None:
        """加载YOLO模型。"""
        model_path = Path(self.model_path)
        if not model_path.exists():
            raise FileNotFoundError(f"模型文件不存在: {model_path}")

        if self.is_onnx:
            import onnxruntime as ort
            logger.info(f"📦 加载ONNX模型: {model_path}")
            providers = (
                ["CUDAExecutionProvider", "CPUExecutionProvider"]
                if "cuda" in self.device
                else ["CPUExecutionProvider"]
            )
            self.model = ort.InferenceSession(
                str(model_path), providers=providers
            )
        else:
            from ultralytics import YOLO
            logger.info(f"📦 加载PyTorch模型: {model_path}")
            self.model = YOLO(str(model_path))
            self.model.to(self.device)

        logger.info("✅ 模型加载成功")

    def detect(self, image: np.ndarray) -> List[Dict[str, Any]]:
        """对单帧图像执行鸟类检测。

        Args:
            image: 输入图像 (BGR, HxWx3)

        Returns:
            检测结果列表，每项包含 class_id, confidence, bbox
        """
        if self.is_onnx:
            return self._detect_onnx(image)
        else:
            return self._detect_torch(image)

    def _detect_torch(self, image: np.ndarray) -> List[Dict[str, Any]]:
        """PyTorch推理。"""
        results = self.model(
            image,
            conf=self.conf_threshold,
            iou=self.iou_threshold,
            verbose=False,
        )

        detections: List[Dict[str, Any]] = []
        for result in results:
            boxes = result.boxes
            if boxes is None or len(boxes) == 0:
                continue

            for box in boxes:
                xyxy = box.xyxy[0].cpu().numpy()
                conf = float(box.conf[0])
                cls_id = int(box.cls[0])

                detections.append({
                    "class_id": cls_id,
                    "confidence": conf,
                    "bbox": xyxy.tolist(),
                    "center": [
                        (xyxy[0] + xyxy[2]) / 2,
                        (xyxy[1] + xyxy[3]) / 2,
                    ],
                })

        return detections

    def _detect_onnx(self, image: np.ndarray) -> List[Dict[str, Any]]:
        """ONNX Runtime推理 (含预处理和后处理)。"""
        # 预处理: resize + normalize
        input_img, ratio, (dw, dh) = self._preprocess_onnx(image)

        # ONNX推理
        ort_inputs = {self.model.get_inputs()[0].name: input_img}
        ort_outputs = self.model.run(None, ort_inputs)
        outputs = ort_outputs[0]  # shape: (1, 84, 8400) for YOLOv8n

        # 后处理
        detections = self._postprocess_onnx(
            outputs, ratio, dw, dh, image.shape[:2]
        )
        return detections

    def _preprocess_onnx(
        self, image: np.ndarray
    ) -> Tuple[np.ndarray, float, Tuple[int, int]]:
        """ONNX预处理 — letterbox + normalize。

        Returns:
            (preprocessed, ratio, (dw, dh))
        """
        input_size = 640
        h, w = image.shape[:2]

        # Letterbox
        ratio = min(input_size / h, input_size / w)
        new_h, new_w = int(h * ratio), int(w * ratio)
        resized = cv2.resize(image, (new_w, new_h), interpolation=cv2.INTER_LINEAR)

        # Pad
        dw = (input_size - new_w) // 2
        dh = (input_size - new_h) // 2
        padded = np.full((input_size, input_size, 3), 114, dtype=np.uint8)
        padded[dh:dh + new_h, dw:dw + new_w] = resized

        # Normalize + transpose
        blob = padded.astype(np.float32) / 255.0
        blob = blob.transpose(2, 0, 1)   # HWC to CHW
        blob = np.expand_dims(blob, axis=0)  # batch dim

        return blob, ratio, (dw, dh)

    def _postprocess_onnx(
        self,
        outputs: np.ndarray,
        ratio: float,
        dw: int,
        dh: int,
        orig_shape: Tuple[int, int],
    ) -> List[Dict[str, Any]]:
        """ONNX输出后处理 — NMS + 坐标还原。

        Args:
            outputs: ONNX输出
            ratio: 缩放比例
            dw, dh: padding偏移
            orig_shape: 原始图像尺寸

        Returns:
            检测结果列表
        """
        outputs = np.squeeze(outputs)  # (84, 8400)
        outputs = outputs.T  # (8400, 84)

        # 取前84列 = 4(bbox) + 80(cls)，但我们是7类
        boxes = outputs[:, :4]
        scores = outputs[:, 4:]

        # 获取每个anchor的最大置信度和类别
        class_ids = np.argmax(scores, axis=1)
        confidences = np.max(scores, axis=1)

        # 置信度过滤
        mask = confidences > self.conf_threshold
        boxes = boxes[mask]
        confidences = confidences[mask]
        class_ids = class_ids[mask]

        if len(boxes) == 0:
            return []

        # 将cxcywh转换为xyxy
        cx, cy, bw, bh = boxes[:, 0], boxes[:, 1], boxes[:, 2], boxes[:, 3]
        x1 = cx - bw / 2
        y1 = cy - bh / 2
        x2 = cx + bw / 2
        y2 = cy + bh / 2

        # 还原到原始图像坐标
        x1 = (x1 - dw) / ratio
        y1 = (y1 - dh) / ratio
        x2 = (x2 - dw) / ratio
        y2 = (y2 - dh) / ratio

        # 裁剪到图像范围
        oh, ow = orig_shape
        x1 = np.clip(x1, 0, ow)
        y1 = np.clip(y1, 0, oh)
        x2 = np.clip(x2, 0, ow)
        y2 = np.clip(y2, 0, oh)

        # NMS (by class)
        indices = cv2.dnn.NMSBoxes(
            bboxes=np.column_stack([x1, y1, x2 - x1, y2 - y1]).tolist(),
            scores=confidences.tolist(),
            score_threshold=self.conf_threshold,
            nms_threshold=self.iou_threshold,
        )

        detections: List[Dict[str, Any]] = []
        if len(indices) > 0:
            for i in indices.flatten():
                detections.append({
                    "class_id": int(class_ids[i]),
                    "confidence": float(confidences[i]),
                    "bbox": [float(x1[i]), float(y1[i]), float(x2[i]), float(y2[i])],
                    "center": [
                        float((x1[i] + x2[i]) / 2),
                        float((y1[i] + y2[i]) / 2),
                    ],
                })

        return detections


# -----------------------------------------------------------
# 主运行循环
# -----------------------------------------------------------

def run_detection(
    source: str,
    model_path: str,
    serial_port: str = "",
    conf: float = 0.25,
    iou: float = 0.45,
    device: str = "cpu",
    save_output: bool = False,
    show_display: bool = True,
) -> None:
    """运行检测主循环。

    Args:
        source: 输入源 (摄像头ID/图片路径/视频路径)
        model_path: 模型路径
        serial_port: 串口号
        conf: 置信度阈值
        iou: IoU阈值
        device: 推理设备
        save_output: 是否保存输出
        show_display: 是否显示画面
    """
    # 初始化检测器
    detector = BirdDetector(
        model_path=model_path,
        conf_threshold=conf,
        iou_threshold=iou,
        device=device,
    )

    # 初始化串口(可选)
    serial_ctrl: Optional[SerialController] = None
    if serial_port:
        serial_ctrl = SerialController(serial_port)

    # 确定输入源类型
    is_camera = source.isdigit()
    is_image = (
        not is_camera
        and Path(source).suffix.lower() in {".jpg", ".jpeg", ".png", ".bmp"}
    )

    # 打开输入源
    if is_camera:
        cap = cv2.VideoCapture(int(source))
        logger.info(f"📷 打开摄像头: {source}")
    elif is_image:
        image = cv2.imread(source)
        if image is None:
            logger.error(f"❌ 无法读取图片: {source}")
            return
        cap = None
    else:
        cap = cv2.VideoCapture(source)
        logger.info(f"🎬 打开视频: {source}")

    # 视频写入器
    writer: Optional[cv2.VideoWriter] = None
    if save_output and cap is not None:
        fps = cap.get(cv2.CAP_PROP_FPS) or 30
        w = int(cap.get(cv2.CAP_PROP_FRAME_WIDTH))
        h = int(cap.get(cv2.CAP_PROP_FRAME_HEIGHT))
        out_path = f"output_{datetime.now().strftime('%Y%m%d_%H%M%S')}.mp4"
        fourcc = cv2.VideoWriter_fourcc(*"mp4v")
        writer = cv2.VideoWriter(out_path, fourcc, fps, (w, h))
        logger.info(f"💾 输出保存到: {out_path}")

    # 状态跟踪
    last_deterrence_time = 0.0
    deterrence_cooldown = 10.0  # 驱离冷却时间(秒)

    frame_count = 0
    fps_start = time.time()
    fps_count = 0

    try:
        if is_image:
            _process_single_image(
                image, detector, serial_ctrl, show_display, save_output
            )
        else:
            assert cap is not None
            while True:
                ret, frame = cap.read()
                if not ret:
                    break

                frame_count += 1
                fps_count += 1

                # 每30帧检测一次(摄像头模式) 或 每帧检测(视频)
                detect_every = 3 if is_camera else 1
                if frame_count % detect_every != 0 and frame_count > 1:
                    # 使用上一次的检测框渲染
                    if show_display:
                        cv2.imshow("Bird Detection", last_annotated)
                        if cv2.waitKey(1) & 0xFF == ord("q"):
                            break
                    continue

                # 执行检测
                detections = detector.detect(frame)

                # 评估威胁
                threat = evaluate_detection(detections)

                # 驱离决策
                now = time.time()
                if (
                    threat["action"] in ("deter", "deter_strong", "deter_urgent")
                    and serial_ctrl is not None
                    and (now - last_deterrence_time) > deterrence_cooldown
                ):
                    logger.info(
                        f"🚨 {threat['action_description']} "
                        f"({threat['pest_count']}只害鸟)"
                    )
                    if threat["action"] == "deter_urgent":
                        serial_ctrl.trigger_full_deterrence()
                    elif threat["action"] == "deter_strong":
                        serial_ctrl.trigger_buzzer(freq_hz=3000, duration_ms=5000)
                        serial_ctrl.trigger_strobe(flash_freq=8, duration_ms=5000)
                    else:
                        serial_ctrl.trigger_buzzer(freq_hz=2000, duration_ms=3000)
                    last_deterrence_time = now

                # 可视化
                annotated = draw_detections(frame, detections)
                last_annotated = annotated

                # 叠加威胁信息
                cv2.putText(
                    annotated,
                    threat["action_description"],
                    (10, 30),
                    cv2.FONT_HERSHEY_SIMPLEX,
                    0.7,
                    (0, 0, 255) if threat["pest_count"] > 0 else (0, 255, 0),
                    2,
                    cv2.LINE_AA,
                )

                # 叠加FPS
                if fps_count >= 30:
                    elapsed = time.time() - fps_start
                    fps_val = fps_count / elapsed
                    fps_start = time.time()
                    fps_count = 0
                else:
                    fps_val = 0
                cv2.putText(
                    annotated,
                    f"FPS: {fps_val:.1f}",
                    (10, 60),
                    cv2.FONT_HERSHEY_SIMPLEX,
                    0.5,
                    (255, 255, 255),
                    1,
                )

                # 写视频
                if writer is not None:
                    writer.write(annotated)

                # 显示
                if show_display:
                    cv2.imshow("Bird Detection", annotated)
                    if cv2.waitKey(1) & 0xFF == ord("q"):
                        break

    except KeyboardInterrupt:
        logger.info("\n⏹️ 用户中断")
    finally:
        if cap is not None:
            cap.release()
        if writer is not None:
            writer.release()
        if serial_ctrl is not None:
            serial_ctrl.stop_all()
            serial_ctrl.close()
        cv2.destroyAllWindows()
        logger.info("🏁 检测结束")


def _process_single_image(
    image: np.ndarray,
    detector: BirdDetector,
    serial_ctrl: Optional[SerialController],
    show_display: bool,
    save_output: bool,
) -> None:
    """处理单张图片检测。

    Args:
        image: 输入图像
        detector: 检测器
        serial_ctrl: 串口控制器
        show_display: 是否显示
        save_output: 是否保存
    """
    logger.info("🔍 检测图片...")
    detections = detector.detect(image)
    threat = evaluate_detection(detections)

    logger.info(f"检测到 {len(detections)} 个目标")
    logger.info(f"  {threat['action_description']}")

    annotated = draw_detections(image, detections)

    if show_display:
        cv2.imshow("Bird Detection", annotated)
        logger.info("按任意键关闭...")
        cv2.waitKey(0)

    if save_output:
        out_path = f"detect_result_{datetime.now().strftime('%Y%m%d_%H%M%S')}.jpg"
        cv2.imwrite(out_path, annotated)
        logger.info(f"💾 结果已保存: {out_path}")

    cv2.destroyAllWindows()


# -----------------------------------------------------------
# 命令行入口
# -----------------------------------------------------------

def main() -> None:
    """命令行入口。"""
    parser = argparse.ArgumentParser(
        description="YOLOv8 鸟类实时检测推理",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
示例:
  python detect.py --source 0 --model weights/best.pt
  python detect.py --source birds.mp4 --model weights/best.onnx
  python detect.py --source 0 --serial COM3 --conf 0.3
        """,
    )

    parser.add_argument(
        "--source", "-s",
        type=str,
        required=True,
        help="输入源 (0=摄像头, 图片路径, 视频路径)",
    )
    parser.add_argument(
        "--model", "-m",
        type=str,
        default="weights/best.pt",
        help="模型权重路径 (.pt 或 .onnx)",
    )
    parser.add_argument(
        "--serial",
        type=str,
        default="",
        help="串口号 (控制STM32驱离设备)",
    )
    parser.add_argument(
        "--conf",
        type=float,
        default=0.25,
        help="置信度阈值 (默认: 0.25)",
    )
    parser.add_argument(
        "--iou",
        type=float,
        default=0.45,
        help="NMS IoU阈值 (默认: 0.45)",
    )
    parser.add_argument(
        "--device",
        type=str,
        default="cpu",
        help="推理设备 (cpu/cuda:0)",
    )
    parser.add_argument(
        "--save",
        action="store_true",
        help="保存检测结果",
    )
    parser.add_argument(
        "--no-display",
        action="store_true",
        help="不显示画面 (无头模式)",
    )

    args = parser.parse_args()

    run_detection(
        source=args.source,
        model_path=args.model,
        serial_port=args.serial,
        conf=args.conf,
        iou=args.iou,
        device=args.device,
        save_output=args.save,
        show_display=not args.no_display,
    )


if __name__ == "__main__":
    main()
