"""Hardware-free tests for REA/D3D ABI inference and driver identity boundaries."""
import json
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0,str(Path(__file__).resolve().parents[1]/"tools"))
import shaderlab_rea_graphics_v02 as ab

class GraphicsABIInferenceTests(unittest.TestCase):
    def evidence(self,root,operation,normalized,filename="sample.json"):
        path=Path(root)/filename
        payload={"operation":operation,"normalized_result":normalized,
                 "subject":{"digest":{"sha256":"f"*64}}}
        path.write_text(json.dumps(payload),encoding="utf-8")
        return path

    def test_d3d9_constant_table_hint_not_runtime_confirmation(self):
        with tempfile.TemporaryDirectory() as td:
            path=self.evidence(td,"inspect_pe_imports",
                {"architecture":"x86","imports":[
                    {"name":"d3d9.dll","functions":[{"name":"Direct3DCreate9"}]},
                    {"name":"d3dx9_43.dll","functions":[{"name":"D3DXGetShaderConstantTable"}]}
                ],"delay_imports":[]})
            result=ab.game_report("MGRR",path)
            self.assertEqual(len(result["feature_entrypoint_hints"]),2)
            self.assertEqual(result["status"],"read_only_static_import_evidence")
            self.assertIn("actual shader program content",result["unknowns"])

    def test_d3d8_import_not_assumed_d3d9(self):
        with tempfile.TemporaryDirectory() as td:
            path=self.evidence(td,"inspect_pe_imports",
                {"architecture":"x86","imports":[{"name":"d3d8.dll","functions":[{"name":"Direct3DCreate8"}]}]})
            result=ab.game_report("UT2004",path)
            self.assertEqual(result["graphics_imports"][0]["api"],"D3D8")
            self.assertFalse(any("D3D9" in x["feature"] for x in result["feature_entrypoint_hints"]))

    def test_export_delta_is_not_functionality_loss(self):
        with tempfile.TemporaryDirectory() as td:
            old=self.evidence(td,"inspect_native_exports",
                {"exports":[{"name":"OpenAdapter12"},{"name":"OldCompilerEntry"}]},"old.json")
            new=self.evidence(td,"inspect_native_exports",
                {"exports":[{"name":"OpenAdapter12"}]},"new.json")
            delta=ab.driver_report("igd12umd64.dll",old,new)
            self.assertEqual(delta["removed_exports"],["OldCompilerEntry"])
            self.assertIn("internal semantics",delta["feature_inference"])

    def test_rejects_wrong_rea_operation(self):
        with tempfile.TemporaryDirectory() as td:
            path=self.evidence(td,"binary_overview",{"imports":[]})
            with self.assertRaises(ValueError):
                ab.game_report("MGRR",path)

if __name__=="__main__":
    unittest.main()
