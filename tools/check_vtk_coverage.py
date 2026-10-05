"""Audit the project's 100-scenario checklist, not VTK's public API coverage."""
import argparse
import json
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("directory", type=Path)
parser.add_argument("--minimum", type=int, default=80)
args = parser.parse_args()
implemented = set(range(1, 91))
passed = set()
evidence = {}
for path in sorted(args.directory.glob("vtk_coverage_*.json")):
    if path.name == "vtk_coverage_report.json":
        continue
    ids = json.loads(path.read_text())["passed_cases"]
    if any(type(item) is not int or item not in implemented for item in ids):
        raise SystemExit(f"Invalid case ids in {path}")
    evidence[path.name] = ids
    passed.update(ids)
report = {
    "scope": "SindreCpp project VTK functional checklist; NOT official VTK API coverage",
    "total_scenarios": 100,
    "implemented_scenarios": len(implemented),
    "execution_verified_scenarios": len(passed),
    "implemented_percent": len(implemented),
    "execution_verified_percent": len(passed),
    "unimplemented_ids": sorted(set(range(1, 101)) - implemented),
    "unverified_ids": sorted(set(range(1, 101)) - passed),
    "evidence": evidence,
}
(args.directory / "vtk_coverage_report.json").write_text(json.dumps(report, indent=2) + "\n")
print(f"Project checklist: {len(implemented)}/100 implemented; {len(passed)}/100 execution-verified")
if len(passed) < args.minimum:
    raise SystemExit(f"Required {args.minimum} execution-verified scenarios")
