#!/usr/bin/env python3
"""
鸟类检测数据集预处理脚本

功能:
  - 从原始图片目录按比例划分为 train/val/test
  - 标签格式转换 (JSON/XML/VOC → YOLO txt)
  - 数据集统计分析 (类别分布、目标尺寸分布)
  - 自动生成 dataset.yaml 配置文件

Usage:
    python dataset_prep.py --images ./raw_images --labels ./raw_labels --output ./dataset
    python dataset_prep.py --images ./raw --split 0.7 0.2 0.1
    python dataset_prep.py --images ./raw --stats-only

Author: BioControl Team
Version: 2.0.0
"""

from __future__ import annotations

import argparse
import json
import logging
import os
import random
import shutil
import sys
from collections import Counter, defaultdict
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple
from xml.etree import ElementTree as ET

import cv2
import yaml
import numpy as np
from tqdm import tqdm

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s] %(message)s",
)
logger = logging.getLogger(__name__)

# 固定随机种子确保可复现
SEED = 42
random.seed(SEED)
np.random.seed(SEED)


# -----------------------------------------------------------
# 配置文件
# -----------------------------------------------------------

# 支持的图像格式
IMAGE_EXTENSIONS = {".jpg", ".jpeg", ".png", ".bmp", ".tiff", ".webp"}

# 类别映射表 (类别ID → 名称)
CLASS_NAMES = {
    0: "sparrow",
    1: "crow",
    2: "magpie",
    3: "pigeon",
    4: "swallow",
    5: "owl",
    6: "woodpecker",
}


# -----------------------------------------------------------
# 数据验证
# -----------------------------------------------------------

def validate_dataset(
    images_dir: Path,
    labels_dir: Path,
) -> Tuple[List[Path], List[Path], List[str]]:
    """验证数据集完整性，检查图像和标签的对应关系。

    Args:
        images_dir: 图像目录
        labels_dir: 标签目录

    Returns:
        (有效图像列表, 有效标签列表, 警告信息列表)
    """
    warnings: List[str] = []

    # 收集所有图像文件
    all_images: List[Path] = []
    for ext in IMAGE_EXTENSIONS:
        all_images.extend(images_dir.glob(f"*{ext}"))
        all_images.extend(images_dir.glob(f"*{ext.upper()}"))

    all_images = sorted(all_images)

    if not all_images:
        raise ValueError(f"图像目录为空: {images_dir}")

    logger.info(f"找到 {len(all_images)} 张图像")

    # 验证标签
    valid_images: List[Path] = []
    valid_labels: List[Path] = []
    missing_labels = 0
    empty_labels = 0
    invalid_labels = 0

    for img_path in all_images:
        label_name = img_path.stem + ".txt"
        label_path = labels_dir / label_name

        if not label_path.exists():
            missing_labels += 1
            warnings.append(f"缺少标签: {label_name}")
            continue

        # 验证标签格式
        try:
            with open(label_path, "r") as f:
                lines = f.readlines()
            if not lines:
                empty_labels += 1
                warnings.append(f"空标签: {label_name}")
                continue

            # 验证每行格式: class_id cx cy w h (归一化坐标)
            for line_num, line in enumerate(lines, 1):
                parts = line.strip().split()
                if len(parts) != 5:
                    invalid_labels += 1
                    warnings.append(f"标签格式错误: {label_name}:{line_num}")
                    continue
                cid = int(float(parts[0]))
                if cid not in CLASS_NAMES:
                    warnings.append(f"未知类别ID: {cid} 在 {label_name}:{line_num}")
                for val in parts[1:]:
                    v = float(val)
                    if v < 0 or v > 1:
                        warnings.append(
                            f"归一化坐标超出范围: {v} 在 {label_name}:{line_num}"
                        )

        except Exception as e:
            invalid_labels += 1
            warnings.append(f"标签读取失败 {label_name}: {e}")
            continue

        # 验证图像可读
        img = cv2.imread(str(img_path))
        if img is None:
            warnings.append(f"图像读取失败: {img_path.name}")
            continue

        valid_images.append(img_path)
        valid_labels.append(label_path)

    logger.info(f"  有效: {len(valid_images)}, 缺标签: {missing_labels}, "
                f"空标签: {empty_labels}, 无效: {invalid_labels}")

    return valid_images, valid_labels, warnings


# -----------------------------------------------------------
# 数据集划分
# -----------------------------------------------------------

def split_dataset(
    images: List[Path],
    labels: List[Path],
    output_dir: Path,
    train_ratio: float = 0.7,
    val_ratio: float = 0.2,
    test_ratio: float = 0.1,
    stratify_by_class: bool = True,
) -> Dict[str, int]:
    """将数据集划分为训练/验证/测试集，支持分层抽样。

    Args:
        images: 图像路径列表
        labels: 标签路径列表
        output_dir: 输出目录
        train_ratio: 训练集比例
        val_ratio: 验证集比例
        test_ratio: 测试集比例
        stratify_by_class: 是否按类别分层抽样

    Returns:
        各划分的图像数量统计
    """
    assert abs(train_ratio + val_ratio + test_ratio - 1.0) < 0.001, \
        "划分比例之和必须为 1.0"

    # 创建输出目录
    for split in ["train", "val", "test"]:
        (output_dir / split / "images").mkdir(parents=True, exist_ok=True)
        (output_dir / split / "labels").mkdir(parents=True, exist_ok=True)

    # 获取每个图像的主要类别(用于分层抽样)
    img_classes: Dict[str, int] = {}
    for img, lbl in zip(images, labels):
        with open(lbl, "r") as f:
            line = f.readline().strip()
        cid = int(line.split()[0]) if line else -1
        img_classes[img.stem] = cid

    # 生成索引并打乱
    indices = list(range(len(images)))
    random.shuffle(indices)

    n_total = len(images)
    n_train = int(n_total * train_ratio)
    n_val = int(n_total * val_ratio)

    if stratify_by_class:
        # 按类别分层抽样
        class_buckets = defaultdict(list)
        for i in range(len(images)):
            cid = img_classes.get(images[i].stem, -1)
            class_buckets[cid].append(i)

        train_idx, val_idx, test_idx = [], [], []
        for cid, bucket in class_buckets.items():
            random.shuffle(bucket)
            n = len(bucket)
            n_t = int(n * train_ratio)
            n_v = int(n * val_ratio)

            train_idx.extend(bucket[:n_t])
            val_idx.extend(bucket[n_t:n_t + n_v])
            test_idx.extend(bucket[n_t + n_v:])

        random.shuffle(train_idx)
        random.shuffle(val_idx)
        random.shuffle(test_idx)

        logger.info("✅ 使用分层抽样(按类别)划分数据集")
    else:
        train_idx = indices[:n_train]
        val_idx = indices[n_train:n_train + n_val]
        test_idx = indices[n_train + n_val:]

    # 复制文件
    def copy_files(idx_list: List[int], split_name: str) -> int:
        target_img_dir = output_dir / split_name / "images"
        target_lbl_dir = output_dir / split_name / "labels"

        count = 0
        for i in tqdm(idx_list, desc=f"复制 {split_name}"):
            shutil.copy2(images[i], target_img_dir / images[i].name)
            shutil.copy2(labels[i], target_lbl_dir / labels[i].name)
            count += 1
        return count

    stats = {
        "train": copy_files(train_idx, "train"),
        "val": copy_files(val_idx, "val"),
        "test": copy_files(test_idx, "test"),
    }

    return stats


# -----------------------------------------------------------
# 格式转换
# -----------------------------------------------------------

def convert_voc_to_yolo(
    xml_path: Path,
    img_width: int,
    img_height: int,
    class_map: Dict[str, int],
) -> List[str]:
    """将Pascal VOC XML标注转换为YOLO txt格式。

    Args:
        xml_path: VOC XML文件路径
        img_width: 图像宽度
        img_height: 图像高度
        class_map: VOC类别名 → YOLO类别ID映射

    Returns:
        每行一个YOLO格式标注的字符串列表
    """
    tree = ET.parse(xml_path)
    root = tree.getroot()

    yolo_lines: List[str] = []
    for obj in root.findall("object"):
        name = obj.find("name")
        bbox = obj.find("bndbox")
        if name is None or bbox is None:
            continue

        class_name = name.text
        if class_name not in class_map:
            logger.warning(f"跳过未知类别: {class_name} 在 {xml_path.name}")
            continue

        xmin = float(bbox.find("xmin").text)  # type: ignore
        ymin = float(bbox.find("ymin").text)  # type: ignore
        xmax = float(bbox.find("xmax").text)  # type: ignore
        ymax = float(bbox.find("ymax").text)  # type: ignore

        # 转换为YOLO归一化格式: cx, cy, w, h
        cx = (xmin + xmax) / 2 / img_width
        cy = (ymin + ymax) / 2 / img_height
        w = (xmax - xmin) / img_width
        h = (ymax - ymin) / img_height

        # 裁剪到 [0, 1] 范围
        cx = max(0, min(1, cx))
        cy = max(0, min(1, cy))
        w = max(0, min(1, w))
        h = max(0, min(1, h))

        cid = class_map[class_name]
        yolo_lines.append(f"{cid} {cx:.6f} {cy:.6f} {w:.6f} {h:.6f}")

    return yolo_lines


def convert_json_to_yolo(
    json_path: Path,
    img_width: int,
    img_height: int,
    class_map: Dict[str, int],
) -> List[str]:
    """将COCO JSON标注转换为YOLO txt格式。

    Args:
        json_path: JSON文件路径
        img_width: 图像宽度
        img_height: 图像高度
        class_map: COCO类别名 → YOLO类别ID映射

    Returns:
        每行一个YOLO格式标注的字符串列表
    """
    with open(json_path, "r") as f:
        data = json.load(f)

    yolo_lines: List[str] = []

    # COCO格式: annotations[{category_id, bbox[x,y,w,h]}]
    for ann in data.get("annotations", []):
        cat_id = str(ann.get("category_id", ""))
        bbox = ann.get("bbox", [0, 0, 0, 0])

        if cat_id not in class_map:
            continue

        x, y, bw, bh = bbox
        cx = (x + bw / 2) / img_width
        cy = (y + bh / 2) / img_height
        w = bw / img_width
        h = bh / img_height

        cx = max(0, min(1, cx))
        cy = max(0, min(1, cy))
        w = max(0, min(1, w))
        h = max(0, min(1, h))

        cid = class_map[cat_id]
        yolo_lines.append(f"{cid} {cx:.6f} {cy:.6f} {w:.6f} {h:.6f}")

    return yolo_lines


# -----------------------------------------------------------
# 数据集统计
# -----------------------------------------------------------

def compute_statistics(
    images_dir: Path,
    labels_dir: Path,
) -> Dict[str, Any]:
    """计算数据集统计信息。

    Args:
        images_dir: 图像目录
        labels_dir: 标签目录

    Returns:
        统计信息字典
    """
    images = sorted([
        p for p in images_dir.iterdir()
        if p.suffix.lower() in IMAGE_EXTENSIONS
    ])
    labels = sorted(labels_dir.glob("*.txt"))

    stats: Dict[str, Any] = {
        "total_images": len(images),
        "total_labels": len(labels),
        "class_distribution": Counter(),
        "bbox_sizes": {"width": [], "height": []},
        "objects_per_image": [],
        "image_sizes": [],
    }

    for lbl_path in tqdm(labels, desc="统计标签"):
        with open(lbl_path, "r") as f:
            lines = f.readlines()

        obj_count = 0
        for line in lines:
            parts = line.strip().split()
            if len(parts) != 5:
                continue
            cid = int(float(parts[0]))
            bw = float(parts[3])
            bh = float(parts[4])

            stats["class_distribution"][cid] += 1
            stats["bbox_sizes"]["width"].append(bw)
            stats["bbox_sizes"]["height"].append(bh)
            obj_count += 1

        stats["objects_per_image"].append(obj_count)

    # 图像尺寸统计
    for img_path in tqdm(images[:100], desc="统计图像尺寸"):  # 采样100张
        img = cv2.imread(str(img_path))
        if img is not None:
            stats["image_sizes"].append(img.shape[:2])  # (H, W)

    # 聚合计算
    if stats["bbox_sizes"]["width"]:
        stats["bbox_width_mean"] = np.mean(stats["bbox_sizes"]["width"])
        stats["bbox_width_std"] = np.std(stats["bbox_sizes"]["width"])
        stats["bbox_height_mean"] = np.mean(stats["bbox_sizes"]["height"])
        stats["bbox_height_std"] = np.std(stats["bbox_sizes"]["height"])

    if stats["objects_per_image"]:
        stats["avg_objects_per_image"] = np.mean(stats["objects_per_image"])
        stats["max_objects_per_image"] = np.max(stats["objects_per_image"])
        stats["min_objects_per_image"] = np.min(stats["objects_per_image"])

    if stats["image_sizes"]:
        sizes = np.array(stats["image_sizes"])
        stats["avg_image_size"] = np.mean(sizes, axis=0).tolist()
        stats["image_size_std"] = np.std(sizes, axis=0).tolist()

    # 类别名称映射
    stats["class_names"] = {
        cid: CLASS_NAMES.get(cid, f"class_{cid}")
        for cid in stats["class_distribution"].keys()
    }

    return stats


def print_statistics(stats: Dict[str, Any]) -> None:
    """格式化打印统计信息。

    Args:
        stats: 统计字典
    """
    print("\n" + "=" * 60)
    print("📊 数据集统计报告")
    print("=" * 60)

    print(f"\n基本统计:")
    print(f"  总图像数: {stats['total_images']}")
    print(f"  总标签数: {stats['total_labels']}")
    print(f"  平均目标/图: {stats.get('avg_objects_per_image', 'N/A'):.1f}")
    print(f"  目标框平均宽度(归一化): {stats.get('bbox_width_mean', 'N/A')}")

    print(f"\n类别分布:")
    for cid, count in sorted(stats["class_distribution"].items()):
        name = stats["class_names"].get(cid, f"class_{cid}")
        pct = count / sum(stats["class_distribution"].values()) * 100
        bar = "█" * int(pct / 2)
        print(f"  [{cid}] {name:15s}: {count:5d} ({pct:5.1f}%) {bar}")

    print(f"\n图像尺寸统计:")
    print(f"  平均尺寸: {stats.get('avg_image_size', 'N/A')}")
    print(f"  标准差:   {stats.get('image_size_std', 'N/A')}")


# -----------------------------------------------------------
# 生成 dataset.yaml
# -----------------------------------------------------------

def generate_dataset_yaml(
    output_dir: Path,
    class_names: Dict[int, str],
) -> Path:
    """生成YOLO格式的数据集配置文件。

    Args:
        output_dir: 数据集输出目录
        class_names: 类别映射

    Returns:
        生成的yaml文件路径
    """
    dataset_config = {
        "path": str(output_dir.absolute()),
        "train": "train/images",
        "val": "val/images",
        "test": "test/images",
        "nc": len(class_names),
        "names": {
            cid: name for cid, name in class_names.items()
        },
    }

    yaml_path = output_dir / "dataset.yaml"
    with open(yaml_path, "w", encoding="utf-8") as f:
        yaml.dump(dataset_config, f, allow_unicode=True, default_flow_style=False)

    logger.info(f"✅ 已生成数据集配置: {yaml_path}")
    return yaml_path


# -----------------------------------------------------------
# 命令行入口
# -----------------------------------------------------------

def main() -> None:
    """命令行入口。"""
    parser = argparse.ArgumentParser(
        description="鸟类检测数据集预处理工具",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
示例:
  python dataset_prep.py --images ./raw_imgs --labels ./raw_lbls --output ./bird_dataset
  python dataset_prep.py --images ./raw --split 0.7 0.2 0.1 --stats
  python dataset_prep.py --images ./dataset/train/images --labels ./dataset/train/labels --stats-only
        """,
    )

    parser.add_argument(
        "--images", "-i",
        type=str,
        required=True,
        help="原始图像目录",
    )
    parser.add_argument(
        "--labels", "-l",
        type=str,
        default="",
        help="原始标签目录 (YOLO txt格式)",
    )
    parser.add_argument(
        "--output", "-o",
        type=str,
        default="./bird_dataset",
        help="输出数据集目录",
    )
    parser.add_argument(
        "--split",
        type=float,
        nargs=3,
        default=[0.7, 0.2, 0.1],
        metavar=("TRAIN", "VAL", "TEST"),
        help="训练/验证/测试划分比例 (默认: 0.7 0.2 0.1)",
    )
    parser.add_argument(
        "--no-stratify",
        action="store_true",
        help="不使用分层抽样",
    )
    parser.add_argument(
        "--stats",
        action="store_true",
        help="计算并打印数据集统计",
    )
    parser.add_argument(
        "--stats-only",
        action="store_true",
        help="仅统计，不划分数据集",
    )

    args = parser.parse_args()

    images_dir = Path(args.images)
    labels_dir = Path(args.labels) if args.labels else images_dir

    if not images_dir.exists():
        logger.error(f"❌ 图像目录不存在: {images_dir}")
        sys.exit(1)

    # 仅统计模式
    if args.stats_only:
        stats = compute_statistics(images_dir, labels_dir)
        print_statistics(stats)
        sys.exit(0)

    output_dir = Path(args.output)

    # 验证数据集
    logger.info("=" * 60)
    logger.info("📋 数据集预处理")
    logger.info("=" * 60)

    logger.info("\n🔍 验证数据集完整性...")
    valid_images, valid_labels, warnings = validate_dataset(
        images_dir, labels_dir
    )

    if warnings:
        logger.warning(f"⚠️ 发现 {len(warnings)} 个警告:")
        for w in warnings[:10]:  # 只显示前10个
            logger.warning(f"  - {w}")
        if len(warnings) > 10:
            logger.warning(f"  ... 还有 {len(warnings) - 10} 个警告")

    if not valid_images:
        logger.error("❌ 没有有效的数据样本")
        sys.exit(1)

    # 划分数据集
    train_r, val_r, test_r = args.split
    logger.info(
        f"\n📂 划分数据集 (train:{train_r:.0%} val:{val_r:.0%} test:{test_r:.0%})"
    )
    stats = split_dataset(
        valid_images,
        valid_labels,
        output_dir,
        train_ratio=train_r,
        val_ratio=val_r,
        test_ratio=test_r,
        stratify_by_class=not args.no_stratify,
    )

    logger.info(f"  train: {stats['train']} 张")
    logger.info(f"  val:   {stats['val']} 张")
    logger.info(f"  test:  {stats['test']} 张")

    # 生成配置文件
    generate_dataset_yaml(output_dir, CLASS_NAMES)

    # 统计信息
    if args.stats:
        train_stats = compute_statistics(
            output_dir / "train" / "images",
            output_dir / "train" / "labels",
        )
        print_statistics(train_stats)

    logger.info("\n🏁 数据集预处理完成!")


if __name__ == "__main__":
    main()
