#!/usr/bin/env python3
"""
YOLOv8 鸟类检测模型 — 训练脚本

支持从头训练、迁移学习、K折交叉验证，自动记录训练指标到Wandb/TensorBoard。

Usage:
    python train.py --config config/bird_detect.yaml [--resume weights/last.pt]
    python train.py --config config/bird_detect.yaml --kfold 5

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

import yaml
import torch
import numpy as np

# 配置日志
logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s] %(message)s",
    handlers=[
        logging.StreamHandler(sys.stdout),
        logging.FileHandler("train_bird_detect.log", encoding="utf-8"),
    ],
)
logger = logging.getLogger(__name__)


# -----------------------------------------------------------
# 配置加载
# -----------------------------------------------------------

def load_config(config_path: str) -> Dict[str, Any]:
    """加载YAML配置文件。

    Args:
        config_path: YAML配置文件路径

    Returns:
        配置字典
    """
    config_path = Path(config_path)
    if not config_path.exists():
        raise FileNotFoundError(f"配置文件不存在: {config_path}")

    with open(config_path, "r", encoding="utf-8") as f:
        config = yaml.safe_load(f)

    logger.info(f"✅ 已加载配置文件: {config_path}")
    return config


# -----------------------------------------------------------
# 数据集验证
# -----------------------------------------------------------

def validate_dataset(config: Dict[str, Any]) -> bool:
    """验证数据集目录结构和内容完整度。

    Args:
        config: 配置字典

    Returns:
        验证是否通过
    """
    ds_cfg = config.get("dataset", {})
    required_dirs = [
        ds_cfg.get("train", ""),
        ds_cfg.get("val", ""),
        ds_cfg.get("labels_train", ""),
        ds_cfg.get("labels_val", ""),
    ]

    all_valid = True
    for d in required_dirs:
        path = Path(d) if d else None
        if path is None or not path.exists():
            logger.warning(f"⚠️ 目录不存在: {d}")
            all_valid = False
        else:
            count = len(list(path.glob("*")))
            logger.info(f"  📁 {d}: {count} 个文件")

    return all_valid


def create_dataset_yaml(config: Dict[str, Any], output_path: str = "bird_dataset.yaml") -> str:
    """生成YOLO格式的数据集配置文件。

    Args:
        config: 配置字典
        output_path: 输出路径

    Returns:
        输出文件路径
    """
    ds_cfg = config["dataset"]
    model_cfg = config["model"]

    yolo_ds = {
        "path": str(Path(ds_cfg["train"]).parent.parent),
        "train": "train/images",
        "val": "val/images",
        "test": "test/images",
        "nc": model_cfg["num_classes"],
        "names": config.get("names", {}),
    }

    with open(output_path, "w", encoding="utf-8") as f:
        yaml.dump(yolo_ds, f, allow_unicode=True, default_flow_style=False)

    logger.info(f"✅ 数据集配置已写入: {output_path}")
    return output_path


# -----------------------------------------------------------
# 训练准备
# -----------------------------------------------------------

def prepare_training_env(config: Dict[str, Any]) -> Tuple[Path, str]:
    """准备训练环境，创建输出目录，检测设备。

    Args:
        config: 配置字典

    Returns:
        (输出目录, 设备字符串)
    """
    train_cfg = config.get("train", {})
    output_cfg = config.get("output", {})

    # 创建输出目录
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    save_dir = Path(output_cfg.get("save_dir", "./runs/bird_detect")) / f"train_{timestamp}"
    save_dir.mkdir(parents=True, exist_ok=True)

    # 检测设备
    infer_cfg = config.get("inference", {})
    device = infer_cfg.get("device", "cpu")
    if device == "cuda:0" and not torch.cuda.is_available():
        logger.warning("⚠️ CUDA不可用，回退到CPU")
        device = "cpu"
    elif device == "cpu" and torch.cuda.is_available():
        device = "cuda:0"

    logger.info(f"🖥️ 训练设备: {device}")
    logger.info(f"📂 输出目录: {save_dir}")

    return save_dir, device


# -----------------------------------------------------------
# 训练回调
# -----------------------------------------------------------

def on_train_epoch_end(trainer: Any) -> None:
    """每个epoch结束时的回调 — 记录指标和保存checkpoint。

    Args:
        trainer: Ultralytics Trainer对象
    """
    epoch = trainer.epoch + 1
    metrics = trainer.metrics

    # 记录关键指标
    loss_box = metrics.get("box_loss", 0)
    loss_cls = metrics.get("cls_loss", 0)
    loss_dfl = metrics.get("dfl_loss", 0)

    logger.info(
        f"  Epoch {epoch:3d} | "
        f"box_loss: {loss_box:.4f} | "
        f"cls_loss: {loss_cls:.4f} | "
        f"dfl_loss: {loss_dfl:.4f}"
    )

    # 每10个epoch验证一次
    if epoch % 10 == 0:
        mAP50 = metrics.get("metrics/mAP50(B)", 0)
        mAP50_95 = metrics.get("metrics/mAP50-95(B)", 0)
        precision = metrics.get("metrics/precision(B)", 0)
        recall = metrics.get("metrics/recall(B)", 0)
        logger.info(
            f"  📊 验证 Epoch {epoch}: "
            f"P={precision:.3f} R={recall:.3f} "
            f"mAP50={mAP50:.3f} mAP50-95={mAP50_95:.3f}"
        )


def export_best_model(save_dir: Path, device: str) -> None:
    """训练完成后导出最佳模型为ONNX和TFLite。

    Args:
        save_dir: 训练输出目录
        device: 设备字符串
    """
    best_pt = save_dir / "weights" / "best.pt"
    if not best_pt.exists():
        logger.warning(f"⚠️ 最佳模型不存在: {best_pt}")
        return

    try:
        from ultralytics import YOLO
        model = YOLO(str(best_pt))
        model.to(device)

        # 导出ONNX
        onnx_path = str(save_dir / "weights" / "best.onnx")
        model.export(format="onnx", simplify=True, opset=12)
        logger.info(f"✅ ONNX模型已导出: {onnx_path}")

        # 导出TFLite
        tflite_path = str(save_dir / "weights" / "best.tflite")
        model.export(format="tflite", int8=False)
        logger.info(f"✅ TFLite模型已导出: {tflite_path}")

    except ImportError:
        logger.error("❌ ultralytics未安装，无法导出模型")
    except Exception as e:
        logger.error(f"❌ 模型导出失败: {e}")


# -----------------------------------------------------------
# 训练主流程
# -----------------------------------------------------------

def train_model(config: Dict[str, Any]) -> str:
    """执行模型训练的主流程。

    Args:
        config: 配置字典

    Returns:
        最佳模型路径
    """
    try:
        from ultralytics import YOLO, settings
    except ImportError:
        logger.error(
            "❌ 未安装ultralytics。请运行: pip install ultralytics"
        )
        sys.exit(1)

    train_cfg = config.get("train", {})
    model_cfg = config.get("model", {})
    augment_cfg = train_cfg.get("augment", {})

    # 准备环境
    save_dir, device = prepare_training_env(config)

    # 创建数据集配置
    ds_yaml = create_dataset_yaml(config, str(save_dir / "dataset.yaml"))

    # 选择模型
    model_name = model_cfg.get("name", "yolov8n")
    logger.info(f"🚀 开始训练模型: {model_name}")

    # 加载模型
    try:
        model_path = f"{model_name}.pt"
        model = YOLO(model_path)
    except Exception:
        logger.warning(f"⚠️ 未找到{model_name}.pt，将随机初始化")
        model = YOLO(f"{model_name}.yaml")

    model.to(device)

    # 训练参数
    results = model.train(
        data=ds_yaml,
        epochs=train_cfg.get("epochs", 300),
        batch=train_cfg.get("batch_size", 16),
        imgsz=train_cfg.get("img_size", 640),
        lr0=train_cfg.get("learning_rate", 0.01),
        lrf=train_cfg.get("lr_final", 0.0001),
        momentum=train_cfg.get("momentum", 0.937),
        weight_decay=train_cfg.get("weight_decay", 0.0005),
        warmup_epochs=train_cfg.get("warmup_epochs", 3),
        warmup_momentum=train_cfg.get("warmup_momentum", 0.8),

        # 数据增强
        hsv_h=augment_cfg.get("hsv_h", 0.015),
        hsv_s=augment_cfg.get("hsv_s", 0.7),
        hsv_v=augment_cfg.get("hsv_v", 0.4),
        degrees=augment_cfg.get("degrees", 10.0),
        translate=augment_cfg.get("translate", 0.1),
        scale=augment_cfg.get("scale", 0.5),
        shear=augment_cfg.get("shear", 2.0),
        perspective=augment_cfg.get("perspective", 0.0),
        flipud=augment_cfg.get("flipud", 0.2),
        fliplr=augment_cfg.get("fliplr", 0.5),
        mosaic=augment_cfg.get("mosaic", 1.0),
        mixup=augment_cfg.get("mixup", 0.1),

        # 其他
        device=device,
        project=str(config.get("output", {}).get("save_dir", "./runs/bird_detect")),
        name=f"train_{datetime.now().strftime('%Y%m%d_%H%M%S')}",
        exist_ok=True,
        pretrained=True,
        optimizer="AdamW",
        verbose=True,
        seed=42,
        deterministic=True,
        single_cls=False,
        cos_lr=True,
        close_mosaic=10,
        amp=True,
        plots=True,
        save=True,
        save_period=config.get("output", {}).get("save_period", 10),
    )

    # 训练后导出
    save_dir_result = Path(results.save_dir)
    export_best_model(save_dir_result, device)

    logger.info(f"✅ 训练完成! 最佳模型: {save_dir_result / 'weights' / 'best.pt'}")

    return str(save_dir_result / "weights" / "best.pt")


def train_kfold(config: Dict[str, Any], k: int = 5) -> List[str]:
    """K折交叉验证训练，提高模型稳定性和泛化能力。

    Args:
        config: 配置字典
        k: 折数

    Returns:
        各折最佳模型路径列表
    """
    logger.info(f"🔀 开始{k}折交叉验证训练")

    ds_cfg = config.get("dataset", {})
    train_img_dir = Path(ds_cfg.get("train", ""))
    train_lbl_dir = Path(ds_cfg.get("labels_train", ""))

    if not train_img_dir.exists() or not train_lbl_dir.exists():
        logger.error("❌ 数据集目录不存在，无法进行K折训练")
        return []

    # 获取所有图像文件
    from sklearn.model_selection import KFold
    all_images = sorted(train_img_dir.glob("*"))
    all_labels = sorted(train_lbl_dir.glob("*.txt"))

    # 确保图像和标签数量一致
    assert len(all_images) == len(all_labels), \
        f"图像({len(all_images)})和标签({len(all_labels)})数量不匹配!"

    images_array = np.array(all_images)
    labels_array = np.array(all_labels)
    kfold = KFold(n_splits=k, shuffle=True, random_state=42)

    fold_models: List[str] = []

    for fold, (train_idx, val_idx) in enumerate(kfold.split(images_array)):
        logger.info(f"\n{'='*50}")
        logger.info(f"📂 第 {fold + 1}/{k} 折训练")
        logger.info(f"{'='*50}")

        # 为每折创建临时目录
        fold_dir = Path(f"kfold_fold{fold}")
        fold_dir.mkdir(exist_ok=True)
        (fold_dir / "train" / "images").mkdir(parents=True, exist_ok=True)
        (fold_dir / "train" / "labels").mkdir(parents=True, exist_ok=True)
        (fold_dir / "val" / "images").mkdir(parents=True, exist_ok=True)
        (fold_dir / "val" / "labels").mkdir(parents=True, exist_ok=True)

        # 创建符号链接或拷贝
        import shutil
        for idx in train_idx:
            shutil.copy2(
                images_array[idx],
                fold_dir / "train" / "images" / images_array[idx].name,
            )
            shutil.copy2(
                labels_array[idx],
                fold_dir / "train" / "labels" / labels_array[idx].name,
            )
        for idx in val_idx:
            shutil.copy2(
                images_array[idx],
                fold_dir / "val" / "images" / images_array[idx].name,
            )
            shutil.copy2(
                labels_array[idx],
                fold_dir / "val" / "labels" / labels_array[idx].name,
            )

        # 修改配置并训练
        fold_config = config.copy()
        fold_config["dataset"] = {
            **ds_cfg,
            "train": str(fold_dir / "train" / "images"),
            "labels_train": str(fold_dir / "train" / "labels"),
            "val": str(fold_dir / "val" / "images"),
            "labels_val": str(fold_dir / "val" / "labels"),
        }

        best_model = train_model(fold_config)
        fold_models.append(best_model)

        # 清理临时文件
        shutil.rmtree(fold_dir)

    logger.info(f"\n✅ {k}折交叉验证完成! 共训练{k}个模型")
    return fold_models


# -----------------------------------------------------------
# 命令行入口
# -----------------------------------------------------------

def main() -> None:
    """命令行入口。"""
    parser = argparse.ArgumentParser(
        description="YOLOv8 鸟类检测模型训练脚本",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
示例:
  python train.py --config config/bird_detect.yaml
  python train.py --config config/bird_detect.yaml --kfold 5
  python train.py --config config/bird_detect.yaml --resume weights/last.pt
        """,
    )

    parser.add_argument(
        "--config", "-c",
        type=str,
        default="config/bird_detect.yaml",
        help="YAML配置文件路径 (默认: config/bird_detect.yaml)",
    )
    parser.add_argument(
        "--kfold", "-k",
        type=int,
        default=0,
        help="K折交叉验证 (0=不使用, 常用值: 5)",
    )
    parser.add_argument(
        "--resume", "-r",
        type=str,
        default="",
        help="从指定checkpoint恢复训练",
    )
    parser.add_argument(
        "--no-validate",
        action="store_true",
        help="跳过数据集验证",
    )

    args = parser.parse_args()

    # 加载配置
    logger.info("=" * 60)
    logger.info("🐦 智能农业生物防控 — YOLOv8 鸟类检测训练")
    logger.info("=" * 60)

    config = load_config(args.config)

    # 验证数据集
    if not args.no_validate:
        logger.info("\n📋 验证数据集...")
        if not validate_dataset(config):
            logger.warning(
                "⚠️ 数据集验证有警告，但将继续训练。"
                "请确保目录路径正确。"
            )

    # 开始训练
    start_time = time.time()

    if args.kfold > 0:
        models = train_kfold(config, args.kfold)
        logger.info(f"\n📦 所有模型: {models}")
    else:
        best_model = train_model(config)
        logger.info(f"\n📦 最佳模型: {best_model}")

    elapsed = time.time() - start_time
    hours = int(elapsed // 3600)
    minutes = int((elapsed % 3600) // 60)
    seconds = int(elapsed % 60)
    logger.info(f"\n⏱️ 总耗时: {hours}h {minutes}m {seconds}s")
    logger.info("🏁 训练完成!")


if __name__ == "__main__":
    main()
