"""Diagnose the CI ICD using the real Vulkan C ABI before GUI stderr redirection."""
import ctypes as c
import json
import os
from pathlib import Path
import sys

print("ICD manifest:", os.environ.get("VK_ICD_FILENAMES"), flush=True)
manifest = Path(os.environ["VK_ICD_FILENAMES"])
print(manifest.read_text(encoding="utf-8-sig"), flush=True)
driver = Path(json.loads(manifest.read_text(encoding="utf-8-sig"))["ICD"]["library_path"])
print("Driver exists:", driver.is_file(), driver, flush=True)
loader_path = Path(sys.argv[1]).resolve()
print("Vulkan loader:", loader_path, flush=True)
# Explicit loader path: Python's secure DLL search intentionally ignores PATH.
loader = c.WinDLL(str(loader_path))

class Extension(c.Structure):
    _fields_ = [("name", c.c_char * 256), ("version", c.c_uint32)]

enumerate_extensions = loader.vkEnumerateInstanceExtensionProperties
enumerate_extensions.argtypes = [c.c_char_p, c.POINTER(c.c_uint32), c.POINTER(Extension)]
enumerate_extensions.restype = c.c_int32
count = c.c_uint32()
result = enumerate_extensions(None, c.byref(count), None)
if result != 0:
    raise RuntimeError(f"Enumerate Vulkan extensions failed: {result}")
items = (Extension * count.value)()
result = enumerate_extensions(None, c.byref(count), items)
if result != 0:
    raise RuntimeError(f"Read Vulkan extensions failed: {result}")
names = {item.name.decode("ascii") for item in items[:count.value]}
print("Instance extensions:", ", ".join(sorted(names)), flush=True)
missing = {"VK_KHR_surface", "VK_KHR_win32_surface"} - names
if missing:
    raise RuntimeError(f"CI ICD is not Win32 presentation capable: {sorted(missing)}")
