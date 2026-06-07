__version__ = "0.1.0"

from enum import IntEnum

class CameraMode(IntEnum):
    FPS = 0
    Orbit = 1

from ._angeloid import init, glInit, dispose, initArgs, Model, Camera

__all__ = ['init', 'glInit', 'dispose', 'initArgs', 'Model', 'Camera', 'CameraMode']
