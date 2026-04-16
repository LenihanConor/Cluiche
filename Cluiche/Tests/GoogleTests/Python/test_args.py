# Test script: Print command-line arguments
import sys
print("Arguments:")
for i, arg in enumerate(sys.argv):
    print(f"  argv[{i}] = {arg}")
