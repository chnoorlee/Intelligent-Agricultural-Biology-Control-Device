#!/usr/bin/env python3
"""
YOLOv8模型导出脚本 — ONNX / TensorRT / TFLite / OpenVINO

将训练好的PyTorch模型导出为多种推理格式，支持各种推理平台部署。

Usage:
    python export_onnx.py --model weights/best.pt --formats onnx,tflite
    python export_onnx.py --model weights/best.pt --formats all --dynamic
    python export_onnx.py --model weights/best.pt --formats onnx --int8 --calib-dir ./calib_images

Author: BioControl Team
Version: 2.0.0
"""

from __future__ import annotations

import argparse
import logging
import os
import sys
import time
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple

import torch
import yaml

# 添加项目根目录
sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s] %(message)s",
)
logger = logging.getLogger(__name__)


# -----------------------------------------------------------
# ONNX 导出
# -----------------------------------------------------------

def export_onnx(
    model_path: str,
    output_path: str,
    input_size: Tuple[int, int] = (640, 640),
    opset: int = 12,
    simplify: bool = True,
    dynamic: bool = False,
    half: bool = False,
) -> str:
    """导出YOLOv8模型为ONNX格式。

    Args:
        model_path: PyTorch模型路径 (.pt)
        output_path: ONNX输出路径
        input_size: 输入尺寸 (H, W)
        opset: ONNX算子集版本
        simplify: 是否使用onnx-simplifier简化
        dynamic: 是否支持动态batch
        half: 是否使用FP16

    Returns:
        导出的ONNX模型路径
    """
    logger.info("=" * 50)
    logger.info("📦 导出 ONNX 格式")
    logger.info("=" * 50)

    from ultralytics import YOLO

    model = YOLO(model_path)

    # 导出参数
    export_kwargs: Dict[str, Any] = {
        "format": "onnx",
        "imgsz": input_size,
        "opset": opset,
        "simplify": simplify,
        "half": half,
    }

    if dynamic:
        export_kwargs["dynamic"] = True

    # 执行导出
    result_path = model.export(**export_kwargs)

    # 复制到目标路径
    if result_path and output_path:
        import shutil
        shutil.copy2(result_path, output_path)
        logger.info(f"✅ ONNX模型已导出: {output_path}")

    # 验证ONNX模型
    _validate_onnx(output_path, input_size, dynamic)

    return output_path


def _validate_onnx(
    onnx_path: str,
    input_size: Tuple[int, int],
    dynamic: bool = False,
) -> bool:
    """验证ONNX模型的有效性。

    Args:
        onnx_path: ONNX模型路径
        input_size: 输入尺寸
        dynamic: 是否动态batch

    Returns:
        验证是否通过
    """
    try:
        import onnx
        import onnxruntime as ort

        # 加载模型
        onnx_model = onnx.load(onnx_path)
        onnx.checker.check_model(onnx_model)
        logger.info("  ✅ ONNX模型结构验证通过")

        # 推理测试
        session = ort.InferenceSession(onnx_path)
        input_name = session.get_inputs()[0].name
        batch_size = 4 if dynamic else 1
        dummy_input = torch.randn(
            batch_size, 3, input_size[0], input_size[1]
        ).numpy()
        outputs = session.run(None, {input_name: dummy_input})
        logger.info(f"  ✅ ONNX推理测试通过 (输出shape: {outputs[0].shape})")

        # 报告模型信息
        input_info = session.get_inputs()
        output_info = session.get_outputs()
        logger.info(f"  📐 输入: {[(i.name, i.shape, i.type) for i in input_info]}")
        logger.info(f"  📐 输出: {[(o.name, o.shape, o.type) for o in output_info]}")

        return True
    except ImportError:
        logger.warning("  ⚠️ onnx/onnxruntime未安装，跳过验证")
        return False
    except Exception as e:
        logger.error(f"  ❌ ONNX模型验证失败: {e}")
        return False


# -----------------------------------------------------------
# TFLite 导出
# -----------------------------------------------------------

def export_tflite(
    model_path: str,
    output_path: str,
    input_size: Tuple[int, int] = (640, 640),
    int8_quantize: bool = False,
    calib_path: str = "",
) -> str:
    """导出YOLOv8模型为TFLite格式 (用于树莓派/Jetson Nano等边缘设备)。

    Args:
        model_path: PyTorch模型路径
        output_path: TFLite输出路径
        input_size: 输入尺寸
        int8_quantize: 是否INT8量化
        calib_path: 校准数据集目录

    Returns:
        TFLite模型路径
    """
    logger.info("=" * 50)
    logger.info("📦 导出 TFLite 格式")
    logger.info("=" * 50)

    from ultralytics import YOLO

    model = YOLO(model_path)

    export_kwargs: Dict[str, Any] = {
        "format": "tflite",
        "imgsz": input_size,
    }

    if int8_quantize:
        export_kwargs["int8"] = True
        if calib_path:
            export_kwargs["data"] = calib_path
        logger.info("  🎯 启用INT8量化")

    result_path = model.export(**export_kwargs)

    if result_path and output_path:
        import shutil
        shutil.copy2(result_path, output_path)
        logger.info(f"✅ TFLite模型已导出: {output_path}")

    # 报告大小
    size_mb = Path(output_path).stat().st_size / 1024 / 1024
    logger.info(f"  📏 模型大小: {size_mb:.2f} MB")

    return output_path


# -----------------------------------------------------------
# TensorRT 导出 (需要NVIDIA GPU)
# -----------------------------------------------------------

def export_tensorrt(
    model_path: str,
    output_path: str,
    input_size: Tuple[int, int] = (640, 640),
    fp16: bool = True,
    workspace_gb: int = 4,
) -> Optional[str]:
    """导出为TensorRT引擎 (用于Jetson系列/GPU高速推理)。

    Args:
        model_path: 模型路径
        output_path: 输出路径
        input_size: 输入尺寸
        fp16: 是否使用FP16
        workspace_gb: TensorRT工作空间(GB)

    Returns:
        TensorRT引擎路径，失败返回None
    """
    logger.info("=" * 50)
    logger.info("📦 导出 TensorRT 引擎")
    logger.info("=" * 50)

    try:
        from ultralytics import YOLO
        model = YOLO(model_path)

        result_path = model.export(
            format="engine",
            imgsz=input_size,
            half=fp16,
            workspace=workspace_gb,
        )

        if result_path:
            import shutil
            shutil.copy2(result_path, output_path)
            logger.info(f"✅ TensorRT引擎已导出: {output_path}")
            return output_path
    except ImportError:
        logger.warning("⚠️ tensorrt未安装，跳过TensorRT导出")
    except Exception as e:
        logger.error(f"❌ TensorRT导出失败: {e}")

    return None


# -----------------------------------------------------------
# OpenVINO 导出 (Intel设备)
# -----------------------------------------------------------

def export_openvino(
    model_path: str,
    output_dir: str,
    input_size: Tuple[int, int] = (640, 640),
) -> Optional[str]:
    """导出为OpenVINO IR格式 (Intel CPU/VPU/GPU)。

    Args:
        model_path: 模型路径
        output_dir: 输出目录
        input_size: 输入尺寸

    Returns:
        OpenVINO模型目录
    """
    logger.info("=" * 50)
    logger.info("📦 导出 OpenVINO IR")
    logger.info("=" * 50)

    try:
        from ultralytics import YOLO
        model = YOLO(model_path)

        result_path = model.export(
            format="openvino",
            imgsz=input_size,
            half=False,
        )

        if result_path:
            logger.info(f"✅ OpenVINO IR已导出: {result_path}")
            return result_path
    except ImportError:
        logger.warning("⚠️ openvino未安装，跳过OpenVINO导出")
    except Exception as e:
        logger.error(f"❌ OpenVINO导出失败: {e}")

    return None


# -----------------------------------------------------------
# 批量导出
# -----------------------------------------------------------

def export_all_formats(
    model_path: str,
    output_dir: str,
    input_size: Tuple[int, int] = (640, 640),
    formats: Optional[List[str]] = None,
    dynamic: bool = False,
    int8: bool = False,
    calib_dir: str = "",
) -> Dict[str, str]:
    """批量导出所有或指定格式。

    Args:
        model_path: PyTorch模型路径
        output_dir: 输出目录
        input_size: 输入尺寸
        formats: 要导出的格式列表 (默认: onnx,tflite)
        dynamic: 动态batch
        int8: INT8量化
        calib_dir: 校准数据目录

    Returns:
        {格式: 路径} 字典
    """
    if formats is None:
        formats = ["onnx", "tflite"]

    output_dir = Path(output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)

    model_stem = Path(model_path).stem
    results: Dict[str, str] = {}

    for fmt in formats:
        fmt = fmt.lower().strip()
        try:
            if fmt == "onnx":
                out = str(output_dir / f"{model_stem}.onnx")
                path = export_onnx(model_path, out, input_size, dynamic=dynamic)
                results["onnx"] = path

            elif fmt == "tflite":
                out = str(output_dir / f"{model_stem}.tflite")
                path = export_tflite(
                    model_path, out, input_size,
                    int8_quantize=int8, calib_path=calib_dir
                )
                results["tflite"] = path

            elif fmt == "engine" or fmt == "tensorrt":
                out = str(output_dir / f"{model_stem}.engine")
                path = export_tensorrt(model_path, out, input_size)
                if path:
                    results["tensorrt"] = path

            elif fmt == "openvino":
                path = export_openvino(model_path, str(output_dir), input_size)
                if path:
                    results["openvino"] = path

            elif fmt == "torchscript":
                from ultralytics import YOLO
                model = YOLO(model_path)
                out = str(output_dir / f"{model_stem}.torchscript")
                model.export(format="torchscript", imgsz=input_size)
                results["torchscript"] = out
                logger.info(f"✅ TorchScript已导出: {out}")

            elif fmt == "coreml":
                from ultralytics import YOLO
                model = YOLO(model_path)
                out = str(output_dir / f"{model_stem}.mlpackage")
                model.export(format="coreml", imgsz=input_size)
                results["coreml"] = out
                logger.info(f"✅ CoreML已导出: {out}")

            else:
                logger.warning(f"⚠️ 不支持的格式: {fmt}")

        except Exception as e:
            logger.error(f"❌ 导出 {fmt} 失败: {e}")

    return results


# -----------------------------------------------------------
# 模型性能基准测试
# -----------------------------------------------------------

def benchmark_model(
    model_path: str,
    input_size: Tuple[int, int] = (640, 640),
    num_runs: int = 100,
    device: str = "cpu",
) -> Dict[str, Any]:
    """对模型进行性能基准测试。

    Args:
        model_path: 模型路径
        input_size: 输入尺寸
        num_runs: 测试次数
        device: 设备

    Returns:
        性能指标字典
    """
    logger.info("=" * 50)
    logger.info("📊 性能基准测试")
    logger.info("=" * 50)

    results: Dict[str, Any] = {}
    ext = Path(model_path).suffix.lower()

    if ext == ".pt":
        from ultralytics import YOLO
        model = YOLO(model_path)
        model.to(device)

        # 预热
        for _ in range(10):
            dummy = torch.randn(1, 3, input_size[0], input_size[1])
            _ = model(dummy, verbose=False)

        # 计时
        times = []
        for _ in range(num_runs):
            dummy = torch.randn(1, 3, input_size[0], input_size[1])
            start = time.perf_counter()
            with torch.no_grad():
                _ = model(dummy, verbose=False)
            times.append(time.perf_counter() - start)

    elif ext == ".onnx":
        import onnxruntime as ort
        import numpy as np
        session = ort.InferenceSession(model_path)
        input_name = session.get_inputs()[0].name

        # 预热
        for _ in range(10):
            dummy = np.random.randn(1, 3, input_size[0], input_size[1]).astype(np.float32)
            _ = session.run(None, {input_name: dummy})

        times = []
        for _ in range(num_runs):
            dummy = np.random.randn(1, 3, input_size[0], input_size[1]).astype(np.float32)
            start = time.perf_counter()
            _ = session.run(None, {input_name: dummy})
            times.append(time.perf_counter() - start)

    else:
        logger.error(f"不支持的模型格式: {ext}")
        return {}

    times_sorted = sorted(times)
    results["format"] = ext
    results["num_runs"] = num_runs
    results["device"] = device
    results["input_size"] = input_size
    results["min_ms"] = round(times_sorted[0] * 1000, 2)
    results["max_ms"] = round(times_sorted[-1] * 1000, 2)
    results["avg_ms"] = round(sum(times) / len(times) * 1000, 2)
    results["median_ms"] = round(times_sorted[len(times) // 2] * 1000, 2)
    results["fps"] = round(1000 / results["avg_ms"], 1)
    results["p50_ms"] = round(
        times_sorted[int(len(times) * 0.5)] * 1000, 2
    )
    results["p95_ms"] = round(
        times_sorted[int(len(times) * 0.95)] * 1000, 2
    )
    results["p99_ms"] = round(
        times_sorted[int(len(times) * 0.99)] * 1000, 2
    )

    logger.info(f"  平均延迟: {results['avg_ms']} ms")
    logger.info(f"  吞吐量:   {results['fps']} FPS")
    logger.info(f"  P95延迟:  {results['p95_ms']} ms")
    logger.info(f"  P99延迟:  {results['p99_ms']} ms")

    return results


# -----------------------------------------------------------
# 命令行入口
# -----------------------------------------------------------

def main() -> None:
    """命令行入口。"""
    parser = argparse.ArgumentParser(
        description="YOLOv8 模型导出工具",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
示例:
  python export_onnx.py --model weights/best.pt --formats onnx,tflite
  python export_onnx.py --model weights/best.pt --formats all --dynamic
  python export_onnx.py --model weights/best.pt --benchmark
        """,
    )

    parser.add_argument(
        "--model", "-m",
        type=str,
        required=True,
        help="训练好的模型路径 (.pt)",
    )
    parser.add_argument(
        "--output", "-o",
        type=str,
        default="./exported_models",
        help="输出目录",
    )
    parser.add_argument(
        "--formats", "-f",
        type=str,
        default="onnx,tflite",
        help="导出格式，逗号分隔 (onnx,tflite,tensorrt,openvino,coreml,torchscript,all)",
    )
    parser.add_argument(
        "--img-size",
        type=int,
        nargs=2,
        default=[640, 640],
        help="输入尺寸 (H W)",
    )
    parser.add_argument(
        "--opset",
        type=int,
        default=12,
        help="ONNX opset版本",
    )
    parser.add_argument(
        "--dynamic",
        action="store_true",
        help="支持动态batch size",
    )
    parser.add_argument(
        "--half",
        action="store_true",
        help="FP16半精度",
    )
    parser.add_argument(
        "--int8",
        action="store_true",
        help="INT8量化 (TFLite)",
    )
    parser.add_argument(
        "--calib-dir",
        type=str,
        default="",
        help="INT8校准数据集目录",
    )
    parser.add_argument(
        "--benchmark",
        action="store_true",
        help="导出后进行性能基准测试",
    )
    parser.add_argument(
        "--no-simplify",
        action="store_true",
        help="不简化ONNX模型",
    )

    args = parser.parse_args()

    input_size = tuple(args.img_size)  # type: ignore
    formats = args.formats.split(",")

    if "all" in formats:
        formats = ["onnx", "tflite", "tensorrt", "openvino"]

    # 执行导出
    results = export_all_formats(
        model_path=args.model,
        output_dir=args.output,
        input_size=input_size,
        formats=formats,
        dynamic=args.dynamic,
        int8=args.int8,
        calib_dir=args.calib_dir,
    )

    # 汇总
    logger.info("\n" + "=" * 50)
    logger.info("📋 导出汇总")
    logger.info("=" * 50)
    for fmt, path in results.items():
        size_mb = Path(path).stat().st_size / 1024 / 1024 if Path(path).is_file() else "N/A"
        logger.info(f"  {fmt:15s}: {path} ({size_mb}MB)")

    # 性能测试
    if args.benchmark:
        for fmt, path in results.items():
            if path:
                benchmark_model(path, input_size, num_runs=50, device="cpu")

    logger.info("🏁 导出完成!")


if __name__ == "__main__":
    main()
