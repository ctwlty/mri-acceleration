import time
import numpy as np
import os
import nibabel as nib

MOCK_MODE = True
MOCK_NII_PATH = r'Iface\mockData.nii.gz'

class console(object):
    DLL_PATH = r"Iface\mriRely\mridll.dll"
    INIT_INI_PATH = r"Iface\mriRely\hw_cfg\init.ini"
    OUTPUT_PATH = r"D:\mri_data\par0423-3"
    PAR_FILE_SUFFIX = ".par"
    PAR_PTH = r'Iface\mriRely\par0423.par'
    is_connected = False
    current_par_file = None

    MOCK_MODE = True
    MOCK_NII_PATH = r"Iface\mockData.nii.gz"

    def __init__(self, mock_nii_path: str = None):
        if mock_nii_path:
            self.MOCK_NII_PATH = mock_nii_path

        print(r'[MOCK] console初始化 (Mock模式，无硬件连接)')
        print(f'[MOCK] 模拟数据路径: {self.MOCK_NII_PATH}')
        self._mock_load_data()
        self.is_connected = True

    def _mock_load_data(self):
        if os.path.exists(self.MOCK_NII_PATH):
            print(f"[MOCK] 加载模拟数据: {self.MOCK_NII_PATH}")
            img = nib.load(self.MOCK_NII_PATH)
            self._mock_data = img.get_fdata()
            print(f"[MOCK] 数据形状: {self._mock_data.shape}")
            
            print(f"[MOCK] 将图像空间数据转换到k空间...")
            kspace = np.fft.fftn(self._mock_data)
            self._mock_data = np.fft.fftshift(kspace)
            print(f"[MOCK] 转换完成")
        else:
            print(f"[MOCK] 警告: 模拟数据文件不存在 {self.MOCK_NII_PATH}")
            print(f"[MOCK] 创建示例数据用于测试...")
            self._mock_data = self._generate_sample_data()

    def _generate_sample_data(self):
        nx, ny, nz = 256, 256, 1
        data = np.zeros((nx, ny, nz), dtype=np.complex128)

        cx, cy, cz = nx // 2, ny // 2, nz // 2
        
        a = nx // 3.5
        b = ny // 2.5
        x = np.arange(nx) - cx
        y = np.arange(ny) - cy
        X, Y = np.meshgrid(x, y, indexing='ij')
        
        mask = ((X / a) ** 2 + (Y / b) ** 2) < 1.0
        
        phantom = np.zeros((nx, ny, nz), dtype=np.complex128)
        
        egg_mask = mask.copy()
        yolk_mask = ((X / (a*0.4)) ** 2 + (Y / (b*0.5)) ** 2) < 1.0
        phantom[egg_mask] = 0.6 + 0.1j * np.random.randn(*phantom[egg_mask].shape)
        phantom[yolk_mask & egg_mask] = 1.2 + 0.1j * np.random.randn(*phantom[yolk_mask & egg_mask].shape)

        noise = 0.02 * (np.random.randn(nx, ny, nz) + 1j * np.random.randn(nx, ny, nz))
        kspace = phantom + noise
        
        kspace_shifted = np.fft.fftshift(kspace)
        
        print(f"[MOCK] 已生成k空间数据，形状: {kspace_shifted.shape}")
        return kspace_shifted

    def get_all_fid(self) -> np.ndarray:
        print(f"[MOCK] get_all_fid called")

        mock_data = self._mock_data

        if mock_data is None:
            raise FileNotFoundError(f"[MOCK] 错误: 没有可用的模拟数据")

        result = mock_data.copy()

        print(f"[MOCK] 返回数据形状: {result.shape}")
        return result

    def is_scanning(self) -> bool:
        return False

    def __del__(self):
        if hasattr(self, '_mock_data'):
            print("[MOCK] console 销毁")