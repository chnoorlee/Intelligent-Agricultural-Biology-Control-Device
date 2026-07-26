"""Configuration parameters for the gimbal tracking system."""

from dataclasses import dataclass, field
from typing import Tuple


@dataclass
class GimbalConfig:
    """Gimbal servo and tracking configuration."""

    # Servo pins (PCA9685 channels)
    pan_channel: int = 0
    tilt_channel: int = 1

    # Servo angle limits (degrees)
    pan_min: float = 0.0
    pan_max: float = 180.0
    tilt_min: float = 30.0
    tilt_max: float = 150.0

    # Home position
    pan_home: float = 90.0
    tilt_home: float = 90.0

    # PID controller gains for gimbal tracking
    pid_pan_kp: float = 0.08
    pid_pan_ki: float = 0.005
    pid_pan_kd: float = 0.02
    pid_tilt_kp: float = 0.08
    pid_tilt_ki: float = 0.005
    pid_tilt_kd: float = 0.02

    # Dead zone (pixels) - don't move if target within this range of center
    dead_zone_px: int = 30

    # Smoothing factor (0-1, higher = more responsive)
    smoothing: float = 0.7

    # Servo pulse width range (microseconds)
    servo_min_us: int = 500
    servo_max_us: int = 2500

    # PCA9685 I2C address
    pca9685_addr: int = 0x40


@dataclass
class TrackerConfig:
    """OpenCV tracker configuration."""

    # Tracker algorithm: 'KCF', 'CSRT', 'MOSSE', 'MIL'
    tracker_type: str = "CSRT"

    # Re-detection interval (frames) - re-run YOLO detection every N frames
    redetect_interval: int = 30

    # Minimum confidence threshold for YOLO detection
    yolo_conf_threshold: float = 0.5

    # Camera settings
    camera_index: int = 0
    camera_width: int = 640
    camera_height: int = 480
    camera_fps: int = 30

    # Frame center coordinates (computed from resolution)
    @property
    def frame_center(self) -> Tuple[int, int]:
        return (self.camera_width // 2, self.camera_height // 2)


@dataclass
class ServoConfig:
    """Serial servo communication config."""

    # Serial port (UART for servo controller)
    port: str = "/dev/ttyUSB0"
    baud_rate: int = 115200
    timeout: float = 0.1

    # Command format: "P<pan_angle> T<tilt_angle>\n"
    command_template: str = "P{pan:.1f} T{tilt:.1f}\n"


@dataclass
class BirdDetectorConfig:
    """Bird detection interface config."""

    # YOLO model endpoint
    model_endpoint: str = "http://localhost:5000/detect"

    # Fallback to local model if endpoint unavailable
    local_model_path: str = "../../home-version/yolo-lightweight/weights/bird_detect.onnx"

    # Classes considered as pest birds
    pest_classes: list = field(default_factory=lambda: [
        "sparrow", "pigeon", "crow", "magpie", "starling",
        "blackbird", "myna", "bulbul"
    ])

    # Request timeout (seconds)
    timeout: float = 2.0


# Default config instances
gimbal_cfg = GimbalConfig()
tracker_cfg = TrackerConfig()
servo_cfg = ServoConfig()
detector_cfg = BirdDetectorConfig()
