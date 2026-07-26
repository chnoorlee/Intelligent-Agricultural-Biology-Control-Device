"""Data augmentation pipeline for bird detection dataset.

Supports geometric transforms, color jitter, noise injection,
and Mosaic augmentation for improving YOLO model robustness.
"""

import cv2
import numpy as np
import random
import os
import argparse
from pathlib import Path
from typing import List, Tuple, Optional


class BirdDataAugmentor:
    """Data augmentation for bird detection dataset.

    Reads YOLO-format images + labels, applies augmentations,
    and saves augmented pairs.
    """

    def __init__(
        self,
        image_dir: str,
        label_dir: str,
        output_image_dir: str,
        output_label_dir: str,
        augment_per_image: int = 3,
        seed: int = 42,
    ) -> None:
        """Initialize augmentor.

        Args:
            image_dir: Path to original images.
            label_dir: Path to YOLO-format labels.
            output_image_dir: Where to save augmented images.
            output_label_dir: Where to save augmented labels.
            augment_per_image: Number of augmented variants per original.
            seed: Random seed for reproducibility.
        """
        self.image_dir = Path(image_dir)
        self.label_dir = Path(label_dir)
        self.output_image_dir = Path(output_image_dir)
        self.output_label_dir = Path(output_label_dir)
        self.augment_per_image = augment_per_image

        self.output_image_dir.mkdir(parents=True, exist_ok=True)
        self.output_label_dir.mkdir(parents=True, exist_ok=True)

        random.seed(seed)
        np.random.seed(seed)

    # -------------------- Geometric Transforms --------------------

    @staticmethod
    def flip_horizontal(
        image: np.ndarray, labels: List[List[float]]
    ) -> Tuple[np.ndarray, List[List[float]]]:
        """Horizontal flip.

        Args:
            image: Input image (H, W, C).
            labels: YOLO labels [class_id, cx, cy, w, h] normalized.

        Returns:
            Flipped image and updated labels.
        """
        flipped = cv2.flip(image, 1)
        new_labels = []
        for label in labels:
            cls_id, cx, cy, w, h = label
            new_cx = 1.0 - cx  # Flip x coordinate
            new_labels.append([cls_id, new_cx, cy, w, h])
        return flipped, new_labels

    @staticmethod
    def flip_vertical(
        image: np.ndarray, labels: List[List[float]]
    ) -> Tuple[np.ndarray, List[List[float]]]:
        """Vertical flip."""
        flipped = cv2.flip(image, 0)
        new_labels = []
        for label in labels:
            cls_id, cx, cy, w, h = label
            new_cy = 1.0 - cy
            new_labels.append([cls_id, cx, new_cy, w, h])
        return flipped, new_labels

    @staticmethod
    def rotate(
        image: np.ndarray, labels: List[List[float]], angle: float
    ) -> Tuple[np.ndarray, List[List[float]]]:
        """Rotate image and adjust bounding boxes.

        Args:
            image: Input image.
            labels: YOLO labels.
            angle: Rotation angle in degrees.

        Returns:
            Rotated image and labels.
        """
        h, w = image.shape[:2]
        center = (w // 2, h // 2)
        matrix = cv2.getRotationMatrix2D(center, angle, 1.0)

        # Compute new bounds
        cos = abs(matrix[0, 0])
        sin = abs(matrix[0, 1])
        new_w = int(h * sin + w * cos)
        new_h = int(h * cos + w * sin)

        # Adjust translation
        matrix[0, 2] += new_w / 2 - center[0]
        matrix[1, 2] += new_h / 2 - center[1]

        rotated = cv2.warpAffine(image, matrix, (new_w, new_h),
                                  borderMode=cv2.BORDER_CONSTANT,
                                  borderValue=(114, 114, 114))

        new_labels = []
        for label in labels:
            cls_id, cx, cy, bw, bh = label
            # Convert normalized -> pixel
            px_cx, px_cy = cx * w, cy * h
            px_bw, px_bh = bw * w, bh * h

            # Rotate center point
            point = np.array([px_cx, px_cy, 1.0])
            new_point = matrix @ point

            # Re-normalize
            new_cx = new_point[0] / new_w
            new_cy = new_point[1] / new_h
            new_bw = bw * w / new_w
            new_bh = bh * h / new_h

            # Keep if still in bounds
            if 0 <= new_cx <= 1.0 and 0 <= new_cy <= 1.0:
                new_labels.append([cls_id, new_cx, new_cy, new_bw, new_bh])

        return rotated, new_labels

    # -------------------- Color Jitter --------------------

    @staticmethod
    def adjust_brightness(image: np.ndarray, factor: float) -> np.ndarray:
        """Adjust image brightness.

        Args:
            image: Input image.
            factor: Brightness factor (0.5-1.5).

        Returns:
            Brightness-adjusted image.
        """
        hsv = cv2.cvtColor(image, cv2.COLOR_BGR2HSV)
        hsv[:, :, 2] = np.clip(hsv[:, :, 2] * factor, 0, 255).astype(np.uint8)
        return cv2.cvtColor(hsv, cv2.COLOR_HSV2BGR)

    @staticmethod
    def adjust_contrast(image: np.ndarray, factor: float) -> np.ndarray:
        """Adjust image contrast."""
        mean = np.mean(image)
        adjusted = (image - mean) * factor + mean
        return np.clip(adjusted, 0, 255).astype(np.uint8)

    @staticmethod
    def adjust_saturation(image: np.ndarray, factor: float) -> np.ndarray:
        """Adjust image saturation."""
        hsv = cv2.cvtColor(image, cv2.COLOR_BGR2HSV)
        hsv[:, :, 1] = np.clip(hsv[:, :, 1] * factor, 0, 255).astype(np.uint8)
        return cv2.cvtColor(hsv, cv2.COLOR_HSV2BGR)

    # -------------------- Noise --------------------

    @staticmethod
    def add_gaussian_noise(image: np.ndarray, sigma: float = 15.0) -> np.ndarray:
        """Add Gaussian noise to image."""
        noise = np.random.normal(0, sigma, image.shape).astype(np.int16)
        noisy = image.astype(np.int16) + noise
        return np.clip(noisy, 0, 255).astype(np.uint8)

    @staticmethod
    def add_salt_pepper_noise(image: np.ndarray, amount: float = 0.02) -> np.ndarray:
        """Add salt & pepper noise."""
        noisy = image.copy()
        h, w = image.shape[:2]
        num_pixels = int(h * w * amount)

        # Salt (white)
        coords = [np.random.randint(0, i - 1, num_pixels // 2) for i in (h, w)]
        noisy[coords[0], coords[1]] = 255

        # Pepper (black)
        coords = [np.random.randint(0, i - 1, num_pixels // 2) for i in (h, w)]
        noisy[coords[0], coords[1]] = 0

        return noisy

    # -------------------- Mosaic --------------------

    @staticmethod
    def mosaic(
        images: List[np.ndarray],
        labels_list: List[List[List[float]]],
        output_size: Tuple[int, int] = (640, 640),
    ) -> Tuple[np.ndarray, List[List[float]]]:
        """Mosaic augmentation: combine 4 images into one.

        Args:
            images: List of 4 images.
            labels_list: Labels for each image.
            output_size: (width, height) of output.

        Returns:
            Mosaic image and merged labels.
        """
        if len(images) != 4:
            raise ValueError("Mosaic requires exactly 4 images")

        w, h = output_size
        center_x = int(random.uniform(w * 0.3, w * 0.7))
        center_y = int(random.uniform(h * 0.3, h * 0.7))

        mosaic = np.full((h, w, 3), 114, dtype=np.uint8)  # Gray background
        all_labels = []

        # Placement: top-left, top-right, bottom-left, bottom-right
        placements = [
            (0, 0, center_x, center_y),                # TL
            (center_x, 0, w - center_x, center_y),      # TR
            (0, center_y, center_x, h - center_y),      # BL
            (center_x, center_y, w - center_x, h - center_y),  # BR
        ]

        for idx, (img, labels) in enumerate(zip(images, labels_list)):
            px, py, pw, ph = placements[idx]

            # Resize image to fit placement area
            resized = cv2.resize(img, (pw, ph))
            mosaic[py:py+ph, px:px+pw] = resized

            # Adjust labels
            for label in labels:
                cls_id, cx, cy, bw, bh = label
                new_cx = (cx * pw + px) / w
                new_cy = (cy * ph + py) / h
                new_bw = bw * pw / w
                new_bh = bh * ph / h
                all_labels.append([cls_id, new_cx, new_cy, new_bw, new_bh])

        return mosaic, all_labels

    # -------------------- Augmentation Pipeline --------------------

    def augment_single(
        self, image: np.ndarray, labels: List[List[float]]
    ) -> List[Tuple[np.ndarray, List[List[float]]]]:
        """Generate multiple augmented variants of a single image.

        Args:
            image: Input image.
            labels: YOLO labels.

        Returns:
            List of (augmented_image, augmented_labels) tuples.
        """
        variants = []

        # Always include original
        variants.append((image.copy(), [l.copy() for l in labels]))

        # 1. Horizontal flip
        if random.random() < 0.5:
            img, lbl = self.flip_horizontal(image, labels)
            variants.append((img, lbl))

        # 2. Rotation (±15°)
        angle = random.uniform(-15, 15)
        img, lbl = self.rotate(image, labels, angle)
        variants.append((img, lbl))

        # 3. Brightness adjustment
        factor = random.uniform(0.6, 1.4)
        img = self.adjust_brightness(image, factor)
        variants.append((img, [l.copy() for l in labels]))

        # 4. Contrast + Saturation
        img = self.adjust_contrast(image, random.uniform(0.7, 1.3))
        img = self.adjust_saturation(img, random.uniform(0.6, 1.4))
        variants.append((img, [l.copy() for l in labels]))

        # 5. Gaussian noise
        if random.random() < 0.3:
            img = self.add_gaussian_noise(image, random.uniform(5, 20))
            variants.append((img, [l.copy() for l in labels]))

        # Limit to augment_per_image
        random.shuffle(variants)
        return variants[:self.augment_per_image]

    def process(self) -> int:
        """Process all images in the directory.

        Returns:
            Total number of augmented images generated.
        """
        image_files = list(self.image_dir.glob("*.jpg")) + \
                       list(self.image_dir.glob("*.png")) + \
                       list(self.image_dir.glob("*.jpeg"))

        total_generated = 0

        for img_path in image_files:
            # Load image
            image = cv2.imread(str(img_path))
            if image is None:
                print(f"Warning: Could not read {img_path}")
                continue

            # Load labels
            label_path = self.label_dir / f"{img_path.stem}.txt"
            if not label_path.exists():
                print(f"Warning: No label for {img_path.stem}")
                continue

            labels = []
            with open(label_path, "r") as f:
                for line in f:
                    parts = line.strip().split()
                    if len(parts) >= 5:
                        labels.append([float(x) for x in parts])

            if not labels:
                continue

            # Augment
            variants = self.augment_single(image, labels)

            for i, (aug_img, aug_labels) in enumerate(variants):
                suffix = f"_aug{i}" if i > 0 else ""
                out_name = f"{img_path.stem}{suffix}"

                # Save image
                out_img_path = self.output_image_dir / f"{out_name}.jpg"
                cv2.imwrite(str(out_img_path), aug_img)

                # Save labels
                out_label_path = self.output_label_dir / f"{out_name}.txt"
                with open(out_label_path, "w") as f:
                    for lbl in aug_labels:
                        f.write(" ".join(f"{x:.6f}" for x in lbl) + "\n")

                total_generated += 1

            print(f"Processed {img_path.stem}: {len(variants)} variants")

        return total_generated


def main() -> None:
    """Entry point."""
    parser = argparse.ArgumentParser(description="Bird dataset augmentation")
    parser.add_argument("--images", required=True, help="Input image directory")
    parser.add_argument("--labels", required=True, help="Input label directory")
    parser.add_argument("--out-images", required=True, help="Output image directory")
    parser.add_argument("--out-labels", required=True, help="Output label directory")
    parser.add_argument("--per-image", type=int, default=4,
                        help="Augmented variants per image")
    parser.add_argument("--seed", type=int, default=42, help="Random seed")
    args = parser.parse_args()

    augmentor = BirdDataAugmentor(
        image_dir=args.images,
        label_dir=args.labels,
        output_image_dir=args.out_images,
        output_label_dir=args.out_labels,
        augment_per_image=args.per_image,
        seed=args.seed,
    )

    total = augmentor.process()
    print(f"\nDone! Generated {total} augmented images.")


if __name__ == "__main__":
    main()
