import sys
import os

parent_dir = os.path.join(os.path.abspath(os.path.dirname(__file__)),
                          os.path.pardir)
sys.path.append(os.path.join(parent_dir, 'src'))
sys.path.append(parent_dir)


# Run test:
import test
