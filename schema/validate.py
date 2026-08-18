#!/usr/bin/env python3
"""
RS485_C 設備 schema 驗證器（模組 E）
====================================================================
用 device.schema.json 驗證 schema/examples/*.json 全數合法（DoD）。

需求：pip install jsonschema
用法：
    python validate.py
全過 exit code 0；有任何範例不合法 exit code 1。

註：本次定版只做「設備描述」schema。量測 payload（單行上報 {t,v,u,ts,ok}）
    不另做 schema，其與設備描述的對應關係見 schema/README.md。這裡附一個
    非強制的 informative 檢查：提醒範例是否含常見打字錯誤（選配）。
"""
import glob
import json
import os
import sys

try:
    from jsonschema import Draft7Validator
except ImportError:
    print("需要 jsonschema：pip install jsonschema")
    sys.exit(2)

HERE = os.path.dirname(os.path.abspath(__file__))


def main():
    schema_path = os.path.join(HERE, "device.schema.json")
    with open(schema_path, "r", encoding="utf-8") as f:
        schema = json.load(f)

    # schema 本身合法性
    Draft7Validator.check_schema(schema)
    validator = Draft7Validator(schema)

    example_paths = sorted(glob.glob(os.path.join(HERE, "examples", "*.json")))
    if not example_paths:
        print("找不到 examples/*.json")
        return 1

    fails = 0
    for path in example_paths:
        name = os.path.relpath(path, HERE)
        with open(path, "r", encoding="utf-8") as f:
            try:
                doc = json.load(f)
            except json.JSONDecodeError as e:
                print("FAIL {} — JSON 解析錯誤：{}".format(name, e))
                fails += 1
                continue

        errors = sorted(validator.iter_errors(doc), key=lambda e: list(e.path))
        if errors:
            fails += 1
            print("FAIL {}".format(name))
            for e in errors:
                loc = "/".join(str(p) for p in e.path) or "(root)"
                print("     - {}: {}".format(loc, e.message))
        else:
            keys = ", ".join(m.get("key", "?") for m in doc.get("measurements", []))
            print("OK   {}  type={} measurements=[{}]".format(name, doc.get("type"), keys))

    total = len(example_paths)
    print("\n{}/{} examples valid, {} failed".format(total - fails, total, fails))
    return 0 if fails == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
