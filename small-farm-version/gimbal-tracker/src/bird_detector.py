"""Bird detection interface - communicates with YOLO detection service."""

import requests
import numpy as np
import logging
from typing import List, Optional, Tuple
from dataclasses import dataclass
from .config import detector_cfg

logger = logging.getLogger(__name__)


@dataclass
class Detection:
    """Single detection result."""

    bbox: Tuple[int, int, int, int]  # x1, y1, x2, y2
    confidence: float
    class_name: str
    is_pest: bool
    center: Tuple[int, int]  # cx, cy

    @property
    def area(self) -> int:
        """Bounding box area in pixels."""
        x1, y1, x2, y2 = self.bbox
        return (x2 - x1) * (y2 - y1)


class BirdDetector:
    """Interface to the YOLO bird detection model.

    Supports both remote API endpoint and local ONNX model inference.
    """

    def __init__(self, endpoint: Optional[str] = None) -> None:
        """Initialize bird detector.

        Args:
            endpoint: YOLO API endpoint URL. Defaults to config value.
        """
        self.endpoint = endpoint or detector_cfg.model_endpoint
        self.pest_classes = set(detector_cfg.pest_classes)
        self.timeout = detector_cfg.timeout
        self._local_model = None
        self.session = requests.Session()

    def detect(self, image: np.ndarray) -> List[Detection]:
        """Detect birds in an image frame.

        Args:
            image: BGR image as numpy array (H, W, 3).

        Returns:
            List of Detection objects sorted by confidence descending.
        """
        try:
            return self._detect_remote(image)
        except (requests.RequestException, ConnectionError) as e:
            logger.warning(f"Remote detection failed: {e}, falling back to local")
            return self._detect_local(image)

    def _detect_remote(self, image: np.ndarray) -> List[Detection]:
        """Send image to remote YOLO API for detection."""
        import cv2

        _, img_encoded = cv2.imencode(".jpg", image)
        files = {"image": ("frame.jpg", img_encoded.tobytes(), "image/jpeg")}

        response = self.session.post(
            self.endpoint,
            files=files,
            timeout=self.timeout,
        )
        response.raise_for_status()
        results = response.json()

        detections = []
        for det in results.get("detections", []):
            detections.append(Detection(
                bbox=tuple(det["bbox"]),
                confidence=det["confidence"],
                class_name=det["class"],
                is_pest=det["class"] in self.pest_classes,
                center=(det["center_x"], det["center_y"]),
            ))

        return sorted(detections, key=lambda d: d.confidence, reverse=True)

    def _detect_local(self, image: np.ndarray) -> List[Detection]:
        """Run local ONNX model inference as fallback.

        Args:
            image: BGR image.

        Returns:
            List of Detection objects.
        """
        import cv2

        if self._local_model is None:
            try:
                import onnxruntime as ort
                model_path = detector_cfg.local_model_path
                self._local_model = ort.InferenceSession(model_path)
                logger.info(f"Loaded local ONNX model from {model_path}")
            except Exception as e:
                logger.error(f"Failed to load local model: {e}")
                return []

        # Preprocess: resize to 640x640, normalize
        input_img = cv2.resize(image, (640, 640))
        input_img = input_img.astype(np.float32) / 255.0
        input_img = np.transpose(input_img, (2, 0, 1))  # HWC -> CHW
        input_img = np.expand_dims(input_img, axis=0)  # Add batch dim

        input_name = self._local_model.get_inputs()[0].name
        outputs = self._local_model.run(None, {input_name: input_img})

        # Parse YOLO output (simplified)
        detections = self._parse_yolo_output(outputs[0], image.shape[:2])
        return sorted(detections, key=lambda d: d.confidence, reverse=True)

    def _parse_yolo_output(
        self, output: np.ndarray, original_shape: Tuple[int, int]
    ) -> List[Detection]:
        """Parse raw YOLO model output into Detection objects.

        Args:
            output: Raw model output tensor.
            original_shape: (height, width) of original image.

        Returns:
            List of Detection objects.
        """
        detections = []
        h, w = original_shape

        # Standard YOLOv8 output: [N, 84+class_count] or [N, 6]
        # Simplified parsing: output shape [1, num_dets, 6] -> [cx, cy, w, h, conf, cls]
        if len(output.shape) == 3:
            output = output[0]

        for det in output:
            if len(det) < 6:
                continue
            cx, cy, bw, bh = det[:4]
            conf = float(det[4])
            cls_id = int(det[5])

            if conf < detector_cfg.yolo_conf_threshold:
                continue

            # Convert center-format to corner-format, scale to original size
            x1 = int((cx - bw / 2) * w / 640)
            y1 = int((cy - bh / 2) * h / 640)
            x2 = int((cx + bw / 2) * w / 640)
            y2 = int((cy + bh / 2) * h / 640)

            class_name = self._get_class_name(cls_id)
            detections.append(Detection(
                bbox=(x1, y1, x2, y2),
                confidence=conf,
                class_name=class_name,
                is_pest=class_name in self.pest_classes,
                center=(int(cx * w / 640), int(cy * h / 640)),
            ))

        return detections

    def _get_class_name(self, cls_id: int) -> str:
        """Map class ID to class name."""
        class_names = [
            "sparrow", "pigeon", "crow", "magpie", "starling",
            "blackbird", "myna", "bulbul", "swallow", "robin",
            "eagle", "hawk", "owl", "egret", "heron",
        ]
        if 0 <= cls_id < len(class_names):
            return class_names[cls_id]
        return f"bird_{cls_id}"

    def is_pest_bird(self, class_name: str) -> bool:
        """Check if a bird class is considered a pest.

        Args:
            class_name: Bird species name.

        Returns:
            True if classified as pest.
        """
        return class_name in self.pest_classes

    def select_primary_target(
        self, detections: List[Detection]
    ) -> Optional[Detection]:
        """Select the primary pest bird to track.

        Priority: largest pest bird with highest confidence.

        Args:
            detections: List of all detections.

        Returns:
            Best target Detection, or None if no pest detected.
        """
        pest_detections = [d for d in detections if d.is_pest]
        if not pest_detections:
            return None

        # Score: area * confidence
        best = max(pest_detections, key=lambda d: d.area * d.confidence)
        return best
