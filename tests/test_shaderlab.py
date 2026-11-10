"""Hardware-free ShaderLab source provenance and safety regression tests."""
import tempfile
import unittest
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
import shaderlab

REPO = Path(__file__).resolve().parents[1]


class ShaderLabTests(unittest.TestCase):
    def test_exact_arena_source_derivation(self):
        shaders = shaderlab.extract_arena(REPO)
        self.assertEqual(set(shaders), {"structured", "typed"})
        self.assertIn(shaderlab.STRUCT_DECL, shaders["structured"])
        self.assertIn(shaderlab.STRUCT_STORE, shaders["structured"])
        self.assertNotIn(shaderlab.STRUCT_DECL, shaders["typed"])
        self.assertIn(shaderlab.TEX_DECL, shaders["typed"])
        self.assertIn(shaderlab.TEX_STORE, shaders["typed"])
        self.assertEqual(shaders["structured"].count("[numthreads(8,8,1)]"), 1)

    def test_shader_manifest_hashes_match_source_bytes(self):
        data = shaderlab.manifest(REPO)
        for name, source in shaderlab.extract_arena(REPO).items():
            expected = shaderlab.sha(source.encode("utf-8"))
            self.assertEqual(data["variants"][name]["sha256"], expected)
        self.assertEqual(data["variants"]["structured"]["target"], "cs_5_1")

    def test_rejects_changed_shader_without_known_marker(self):
        with tempfile.TemporaryDirectory() as d:
            p = Path(d) / shaderlab.ARENA
            p.parent.mkdir(parents=True)
            p.write_text('const char* shader=R"SHADER(float x;)SHADER";', encoding="utf-8")
            with self.assertRaises(ValueError):
                shaderlab.extract_arena(Path(d))

    def test_inventory_is_nonexecuting_and_never_assumes_compatibility(self):
        with tempfile.TemporaryDirectory() as d:
            game = Path(d) / "Sample"
            (game / "03_RUNTIME").mkdir(parents=True)
            report = shaderlab.game_inventory(Path(d))
            self.assertEqual(len(report["games"]), 1)
            item = report["games"][0]
            self.assertTrue(item["game_dir_present"])
            self.assertEqual(item["shaderlab_integration"], "not_tested")
            self.assertEqual(item["classification"], "candidate_inventory_only")
            self.assertTrue(item["has_runtime_tree"])


if __name__ == "__main__":
    unittest.main()
