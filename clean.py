# clean.py

import sys
import shutil
from pathlib import Path

build_dir = Path.cwd().joinpath("System_Info").joinpath("build")

if __name__ == "__main__":
    if not build_dir.is_dir():
        print("Cannot find build directory")
        sys.exit(1)

    try:
        shutil.rmtree(build_dir)
    except shutil.Error as e:
        print(f'Could not delete directory "{build_dir}": {e}')
