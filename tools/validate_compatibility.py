"""Validate the public hardware compatibility snapshot. Runs without a GPU."""
from pathlib import Path
import json,sys
path=Path(__file__).resolve().parent.parent/"compatibility"/"intel-hd-520.json"
data=json.loads(path.read_text(encoding="utf8"))
allowed={"compatible","in_progress","not_feasible"}
hardware=data["hardware"]
assert hardware["gpu"]=="Intel HD Graphics 520"
assert hardware["device_id"]=="1916"
assert hardware["gpu_driver"]
seen=set()
for game in data["games"]:
 assert game["status"] in allowed,game
 assert game["game_id"] not in seen,game
 seen.add(game["game_id"])
 assert game["title"] and game["native_baseline"] and game["underdog_integration"]
 assert game["next_test"]
 if game["status"]=="compatible":
  assert game["underdog_integration"].startswith("verified_"),game
  assert game["game_id"]=="underdog-arena","No external games can be promoted without new GPU integration receipts"
 if game["game_id"]=="fortnite":
  assert game["status"]=="in_progress"
  assert game["underdog_integration"]=="not_integrated"
counts={s:sum(g["status"]==s for g in data["games"]) for s in allowed}
assert counts==data["counts"],(counts,data["counts"])
assert len(seen)>=10
print("COMPATIBILITY_MATRIX_VALIDATED",len(seen),"entries",counts)
