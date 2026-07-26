"""Real-time gimbal bird tracking system using OpenCV and PID control.

Continuously tracks pest birds with a servo-driven gimbal (pan/tilt)
using computer vision tracking algorithms combined with periodic
YOLO re-detection for robustness.
"""

import cv2
import time
import logging
import signal
import sys
from typing import Optional, Deque
from collections import deque

import numpy as np

from .config import gimbal_cfg, tracker_cfg
from .servo_control import ServoController, MockServoController
from .bird_detector import BirdDetector, Detection

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s] %(name)s: %(message)s",
)
logger = logging.getLogger(__name__)


class PIDController:
    """Simple PID controller for gimbal servo positioning."""

    def __init__(self, kp: float, ki: float, kd: float, output_limit: float = 100.0) -> None:
        """Initialize PID controller.

        Args:
            kp: Proportional gain.
            ki: Integral gain.
            kd: Derivative gain.
            output_limit: Max absolute output value.
        """
        self.kp = kp
        self.ki = ki
        self.kd = kd
        self.output_limit = output_limit
        self._integral = 0.0
        self._prev_error = 0.0
        self._prev_time: Optional[float] = None

    def reset(self) -> None:
        """Reset integral and previous error."""
        self._integral = 0.0
        self._prev_error = 0.0
        self._prev_time = None

    def update(self, error: float) -> float:
        """Calculate PID output for given error.

        Args:
            error: Current error (pixels from center).

        Returns:
            Control output (angular velocity or position delta).
        """
        now = time.time()
        dt = now - self._prev_time if self._prev_time else 0.016  # ~60fps default

        # Proportional
        p = self.kp * error

        # Integral (with anti-windup)
        self._integral += error * dt
        self._integral = np.clip(self._integral, -self.output_limit, self.output_limit)
        i = self.ki * self._integral

        # Derivative (with low-pass)
        d = self.kd * (error - self._prev_error) / max(dt, 0.001)

        self._prev_error = error
        self._prev_time = now

        output = p + i + d
        return np.clip(output, -self.output_limit, self.output_limit)


class GimbalTracker:
    """Main gimbal tracking system.

    Captures frames from camera, runs bird detection, tracks selected
    target with OpenCV tracker, and controls pan/tilt servos via PID.
    """

    def __init__(
        self,
        use_mock_servo: bool = False,
        camera_index: Optional[int] = None,
    ) -> None:
        """Initialize gimbal tracker.

        Args:
            use_mock_servo: Use mock servo for testing without hardware.
            camera_index: Camera device index.
        """
        # Initialize components
        self.servo = MockServoController() if use_mock_servo else ServoController()
        self.detector = BirdDetector()
        self.cap: Optional[cv2.VideoCapture] = None

        # PID controllers for pan and tilt
        self.pid_pan = PIDController(
            gimbal_cfg.pid_pan_kp,
            gimbal_cfg.pid_pan_ki,
            gimbal_cfg.pid_pan_kd,
        )
        self.pid_tilt = PIDController(
            gimbal_cfg.pid_tilt_kp,
            gimbal_cfg.pid_tilt_ki,
            gimbal_cfg.pid_tilt_kd,
        )

        # OpenCV tracker
        self.ocv_tracker: Optional[cv2.Tracker] = None
        self._tracker_type = tracker_cfg.tracker_type

        # State
        self._running = False
        self._target: Optional[Detection] = None
        self._frame_count = 0
        self._tracking_quality: Deque[float] = deque(maxlen=30)  # Rolling quality
        self._lost_count = 0
        self._max_lost_frames = 60  # Re-detect after 2 seconds of tracking loss

    # -------------------- Lifecycle --------------------

    def start(self) -> None:
        """Start the tracking loop."""
        if not self.servo.connect():
            logger.error("Failed to connect servo controller")
            return

        camera_idx = tracker_cfg.camera_index
        self.cap = cv2.VideoCapture(camera_idx)
        self.cap.set(cv2.CAP_PROP_FRAME_WIDTH, tracker_cfg.camera_width)
        self.cap.set(cv2.CAP_PROP_FRAME_HEIGHT, tracker_cfg.camera_height)
        self.cap.set(cv2.CAP_PROP_FPS, tracker_cfg.camera_fps)

        if not self.cap.isOpened():
            logger.error(f"Failed to open camera {camera_idx}")
            self.servo.disconnect()
            return

        self._running = True
        logger.info("Gimbal tracker started")
        self._run_loop()

    def stop(self) -> None:
        """Stop tracking and release resources."""
        self._running = False
        if self.cap:
            self.cap.release()
        self.servo.disconnect()
        logger.info("Gimbal tracker stopped")

    # -------------------- Main Loop --------------------

    def _run_loop(self) -> None:
        """Main tracking loop."""
        signal.signal(signal.SIGINT, self._signal_handler)
        signal.signal(signal.SIGTERM, self._signal_handler)

        try:
            while self._running:
                ret, frame = self.cap.read()
                if not ret:
                    logger.warning("Failed to read frame")
                    time.sleep(0.01)
                    continue

                self._frame_count += 1

                # Periodic re-detection or initial detection
                if (
                    self._frame_count % tracker_cfg.redetect_interval == 0
                    or self._target is None
                ):
                    self._run_detection(frame)

                # Update tracking
                if self._target is not None and self.ocv_tracker is not None:
                    self._update_tracking(frame)

                # Control gimbal
                self._control_gimbal(frame)

                # Display
                self._draw_debug(frame)
                cv2.imshow("Gimbal Tracker", frame)

                if cv2.waitKey(1) & 0xFF == ord("q"):
                    break

        except Exception as e:
            logger.exception(f"Tracking loop error: {e}")
        finally:
            self.stop()

    # -------------------- Detection --------------------

    def _run_detection(self, frame: np.ndarray) -> None:
        """Run YOLO bird detection on current frame.

        Args:
            frame: Current camera frame.
        """
        detections = self.detector.detect(frame)
        if not detections:
            self._target = None
            self.ocv_tracker = None
            return

        target = self.detector.select_primary_target(detections)
        if target is None:
            self._target = None
            self.ocv_tracker = None
            return

        self._target = target
        self._init_ocv_tracker(frame, target)
        self._lost_count = 0
        logger.info(
            f"New target: {target.class_name} conf={target.confidence:.2f} "
            f"bbox={target.bbox}"
        )

    def _init_ocv_tracker(self, frame: np.ndarray, detection: Detection) -> None:
        """Initialize OpenCV tracker on detected bird.

        Args:
            frame: Frame to initialize tracker on.
            detection: Detected bird to track.
        """
        x1, y1, x2, y2 = detection.bbox
        w, h = x2 - x1, y2 - y1

        tracker_types = {
            "KCF": cv2.TrackerKCF_create,
            "CSRT": cv2.TrackerCSRT_create,
            "MOSSE": cv2.legacy.TrackerMOSSE_create,
        }

        creator = tracker_types.get(self._tracker_type, cv2.TrackerCSRT_create)
        self.ocv_tracker = creator()
        self.ocv_tracker.init(frame, (x1, y1, w, h))

    # -------------------- Tracking --------------------

    def _update_tracking(self, frame: np.ndarray) -> None:
        """Update OpenCV tracker and target state.

        Args:
            frame: Current camera frame.
        """
        success, bbox = self.ocv_tracker.update(frame)

        if success:
            x, y, w, h = [int(v) for v in bbox]
            cx = x + w // 2
            cy = y + h // 2
            self._target.center = (cx, cy)
            self._target.bbox = (x, y, x + w, y + h)
            self._lost_count = 0

            # Track quality: bbox size ratio as heuristic
            area_ratio = (w * h) / (tracker_cfg.camera_width * tracker_cfg.camera_height)
            self._tracking_quality.append(area_ratio)
        else:
            self._lost_count += 1
            if self._lost_count > self._max_lost_frames:
                logger.info("Tracking lost, will re-detect")
                self._target = None
                self.ocv_tracker = None

    # -------------------- Gimbal Control --------------------

    def _control_gimbal(self, frame: np.ndarray) -> None:
        """Calculate and apply gimbal servo movements.

        Args:
            frame: Current camera frame (for debug overlay).
        """
        if self._target is None:
            return

        cx, cy = self._target.center
        frame_cx, frame_cy = tracker_cfg.frame_center

        # Calculate pixel error from frame center
        error_x = cx - frame_cx
        error_y = frame_cy - cy  # Inverted: image y-axis vs gimbal tilt

        # Dead zone check
        if abs(error_x) < gimbal_cfg.dead_zone_px and abs(error_y) < gimbal_cfg.dead_zone_px:
            return

        # PID control
        pan_delta = self.pid_pan.update(error_x)
        tilt_delta = self.pid_tilt.update(error_y)

        # Apply to servos
        pan, tilt = self.servo.current_position
        self.servo.move_to(pan + pan_delta, tilt + tilt_delta)

    # -------------------- Display --------------------

    def _draw_debug(self, frame: np.ndarray) -> None:
        """Draw debug overlay on frame.

        Args:
            frame: Frame to draw on.
        """
        # Crosshair at center
        cx, cy = tracker_cfg.frame_center
        cv2.line(frame, (cx - 20, cy), (cx + 20, cy), (0, 255, 0), 1)
        cv2.line(frame, (cx, cy - 20), (cx, cy + 20), (0, 255, 0), 1)

        # Dead zone
        dz = gimbal_cfg.dead_zone_px
        cv2.rectangle(
            frame,
            (cx - dz, cy - dz),
            (cx + dz, cy + dz),
            (0, 255, 255),
            1,
        )

        # Target bounding box
        if self._target:
            x1, y1, x2, y2 = self._target.bbox
            color = (0, 0, 255) if self._target.is_pest else (255, 0, 0)
            cv2.rectangle(frame, (x1, y1), (x2, y2), color, 2)
            cv2.circle(frame, self._target.center, 4, color, -1)

            label = f"{self._target.class_name} {self._target.confidence:.2f}"
            cv2.putText(
                frame, label, (x1, y1 - 10),
                cv2.FONT_HERSHEY_SIMPLEX, 0.5, color, 2,
            )

        # Status text
        status_lines = [
            f"Frame: {self._frame_count}",
            f"Target: {self._target.class_name if self._target else 'None'}",
            f"Servo: P={self.servo.current_position[0]:.1f} T={self.servo.current_position[1]:.1f}",
            f"Quality: {np.mean(self._tracking_quality) if self._tracking_quality else 0:.3f}",
        ]
        for i, line in enumerate(status_lines):
            cv2.putText(
                frame, line, (10, 20 + i * 20),
                cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 255, 0), 1,
            )

    def _signal_handler(self, signum, frame) -> None:
        """Handle OS signals for graceful shutdown."""
        logger.info(f"Received signal {signum}, stopping...")
        self.stop()
        sys.exit(0)


def main() -> None:
    """Entry point for gimbal tracker."""
    import argparse

    parser = argparse.ArgumentParser(description="Gimbal Bird Tracker")
    parser.add_argument("--mock", action="store_true", help="Use mock servo (no hardware)")
    parser.add_argument("--camera", type=int, default=0, help="Camera index")
    args = parser.parse_args()

    tracker = GimbalTracker(
        use_mock_servo=args.mock,
        camera_index=args.camera,
    )
    tracker.start()


if __name__ == "__main__":
    main()
