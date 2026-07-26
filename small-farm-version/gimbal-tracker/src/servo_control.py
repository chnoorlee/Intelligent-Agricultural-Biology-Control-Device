"""Serial servo control interface for gimbal pan/tilt mechanism."""

import serial
import time
import logging
from typing import Optional
from .config import servo_cfg, gimbal_cfg

logger = logging.getLogger(__name__)


class ServoController:
    """Controls pan/tilt servos via serial communication.

    Communicates with an Arduino/ESP32-based servo driver that translates
    angle commands to PCA9685 PWM signals.
    """

    def __init__(self, port: Optional[str] = None, baud_rate: Optional[int] = None) -> None:
        """Initialize servo controller with serial connection.

        Args:
            port: Serial port path. Defaults to config value.
            baud_rate: Baud rate. Defaults to config value.
        """
        self.port = port or servo_cfg.port
        self.baud_rate = baud_rate or servo_cfg.baud_rate
        self.timeout = servo_cfg.timeout
        self._serial: Optional[serial.Serial] = None
        self._pan_angle: float = gimbal_cfg.pan_home
        self._tilt_angle: float = gimbal_cfg.tilt_home

    def connect(self) -> bool:
        """Establish serial connection to servo controller.

        Returns:
            True if connection successful, False otherwise.
        """
        try:
            self._serial = serial.Serial(
                port=self.port,
                baudrate=self.baud_rate,
                timeout=self.timeout,
            )
            time.sleep(2.0)  # Wait for Arduino bootloader
            logger.info(f"Connected to servo controller on {self.port}")
            self._go_home()
            return True
        except serial.SerialException as e:
            logger.error(f"Failed to connect servo controller: {e}")
            return False

    def disconnect(self) -> None:
        """Close serial connection."""
        if self._serial and self._serial.is_open:
            self._serial.close()
            logger.info("Servo controller disconnected")

    def _go_home(self) -> None:
        """Move servos to home position."""
        self.move_to(gimbal_cfg.pan_home, gimbal_cfg.tilt_home)

    def move_to(self, pan_angle: float, tilt_angle: float) -> bool:
        """Move pan/tilt servos to specified angles.

        Args:
            pan_angle: Pan angle in degrees (0-180).
            tilt_angle: Tilt angle in degrees (30-150).

        Returns:
            True if command sent successfully.
        """
        # Clamp angles to valid range
        pan = max(gimbal_cfg.pan_min, min(gimbal_cfg.pan_max, pan_angle))
        tilt = max(gimbal_cfg.tilt_min, min(gimbal_cfg.tilt_max, tilt_angle))

        # Smooth transition
        self._pan_angle = gimbal_cfg.smoothing * pan + (1 - gimbal_cfg.smoothing) * self._pan_angle
        self._tilt_angle = gimbal_cfg.smoothing * tilt + (1 - gimbal_cfg.smoothing) * self._tilt_angle

        command = servo_cfg.command_template.format(
            pan=self._pan_angle, tilt=self._tilt_angle
        )

        if self._serial and self._serial.is_open:
            try:
                self._serial.write(command.encode())
                return True
            except serial.SerialException as e:
                logger.error(f"Serial write error: {e}")
                return False
        return False

    def move_relative(self, dpan: float, dtilt: float) -> bool:
        """Move servos relative to current position.

        Args:
            dpan: Delta pan angle.
            dtilt: Delta tilt angle.

        Returns:
            True if command sent successfully.
        """
        return self.move_to(self._pan_angle + dpan, self._tilt_angle + dtilt)

    @property
    def current_position(self) -> tuple:
        """Get current pan/tilt angles."""
        return (self._pan_angle, self._tilt_angle)

    def is_connected(self) -> bool:
        """Check if serial connection is active."""
        return self._serial is not None and self._serial.is_open

    def __enter__(self):
        self.connect()
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.disconnect()


class MockServoController(ServoController):
    """Mock servo controller for testing without hardware."""

    def connect(self) -> bool:
        logger.info("MockServoController connected (no hardware)")
        self._pan_angle = gimbal_cfg.pan_home
        self._tilt_angle = gimbal_cfg.tilt_home
        return True

    def disconnect(self) -> None:
        logger.info("MockServoController disconnected")

    def move_to(self, pan_angle: float, tilt_angle: float) -> bool:
        pan = max(gimbal_cfg.pan_min, min(gimbal_cfg.pan_max, pan_angle))
        tilt = max(gimbal_cfg.tilt_min, min(gimbal_cfg.tilt_max, tilt_angle))
        self._pan_angle = pan
        self._tilt_angle = tilt
        logger.debug(f"Mock move: pan={pan:.1f}, tilt={tilt:.1f}")
        return True

    def is_connected(self) -> bool:
        return True
