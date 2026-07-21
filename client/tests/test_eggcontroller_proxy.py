import importlib.util
import tempfile
import unittest
from pathlib import Path


PROXY_PATH = Path(__file__).parents[1] / "tools" / "eggcontroller_proxy.py"


class FakeCheckBox:
    def __init__(self):
        self.checked = True

    def setChecked(self, checked):
        self.checked = checked


class FakeLabel:
    def text(self):
        return "Scan OK"


class FakeConsole:
    def __init__(self, output_path):
        self.OUTPUT_PATH = str(output_path)


class FakeController:
    def __init__(self, output_path):
        self.c = FakeConsole(output_path)


class FakeUi:
    def __init__(self, root):
        self.root = root
        self.calls = 0
        self.demoCheckbox = FakeCheckBox()
        self.status_label = FakeLabel()
        self.samplingSuccessCount = 0
        self.hc = FakeController(root / "raw")
        self.registerList = []

    def samplingBtn_click_sync(self):
        self.calls += 1
        self.samplingSuccessCount += 1
        raw_path = self.root / "raw" / "PTMRIData00_1.raw"
        result_root = self.root / "webdata" / "data" / "123"
        result_root.mkdir(parents=True, exist_ok=True)
        raw_path.parent.mkdir(parents=True, exist_ok=True)
        raw_path.write_bytes(b"raw")
        kspace = result_root / "kspace_123.png"
        final = result_root / "rgb._123.png"
        kspace.write_bytes(b"kspace")
        final.write_bytes(b"final")
        self.registerList = [{
            "id": 123,
            "kspace_image": [str(kspace.relative_to(self.root))],
            "rgb_image": [str(final.relative_to(self.root))],
        }]


class EggControllerProxyTest(unittest.TestCase):
    def test_calls_verified_entry_once_and_returns_original_outputs(self):
        self.assertTrue(PROXY_PATH.is_file(), "eggcontroller proxy script must exist")
        spec = importlib.util.spec_from_file_location("eggcontroller_proxy", PROXY_PATH)
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)

        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            ui = FakeUi(root)
            result = module.run_once(root, lambda: ui)

            self.assertEqual(ui.calls, 1)
            self.assertFalse(ui.demoCheckbox.checked)
            self.assertEqual(result["task_id"], "123")
            self.assertEqual(Path(result["raw_path"]), root / "raw" / "PTMRIData00_1.raw")
            self.assertEqual(Path(result["kspace_image_path"]), root / "webdata/data/123/kspace_123.png")
            self.assertEqual(Path(result["final_image_path"]), root / "webdata/data/123/rgb._123.png")


if __name__ == "__main__":
    unittest.main()
