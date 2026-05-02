#!/usr/bin/env python3
"""Run the MMD Demo renderer"""

import sys
from pathlib import Path

# Add src to path
proj_root = Path(__file__).parent
sys.path.insert(0, str(proj_root / "src"))

from renderer import main

if __name__ == "__main__":
    main()
