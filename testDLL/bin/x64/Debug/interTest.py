import ctypes
import os
import glob

work_dir = r"D:\123pan\Downloads\testDLL\bin\x64\Debug"

# 设置工作目录
kernel32 = ctypes.windll.kernel32
result = kernel32.SetCurrentDirectoryA(work_dir.encode('utf-8'))

print("="*60)
print("工作目录中的所有 DLL 文件")
print("="*60)

dll_files = glob.glob(os.path.join(work_dir, "*.dll"))
for dll in sorted(dll_files):
    size = os.path.getsize(dll)
    print(f"  {os.path.basename(dll):<30} {size:>10} bytes")

print(f"\n共找到 {len(dll_files)} 个 DLL 文件")

# 尝试加载主 DLL
print("\n" + "="*60)
print("尝试加载 mridll.dll")
print("="*60)

dll_path = os.path.join(work_dir, "mridll.dll")

try:
    lib = ctypes.WinDLL(dll_path)
    print("✅ 主 DLL 加载成功")
    
    # 尝试调用 Init
    print("\n尝试调用 Init 函数...")
    init_file = r"D:\123pan\Downloads\testDLL\bin\x64\Debug\hw_cfg\init.ini".encode('utf-8')
    
    lib.Init.argtypes = [ctypes.c_char_p]
    lib.Init.restype = ctypes.c_int
    
    ret = lib.Init(init_file)
    print(f"Init 返回值: {ret}")
    
    if ret == 0:
        print("✅ Init 成功")
    else:
        print(f"❌ Init 失败，返回码: {ret}")
        
except Exception as e:
    print(f"❌ 错误: {e}")
    print(f"\n错误类型: {type(e).__name__}")
    import traceback
    traceback.print_exc()