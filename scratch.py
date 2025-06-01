import os
import re
from pathlib import Path

# Path to your /cpp directory
CPP_ROOT = Path("cpp")

# Regex patterns
begin_fields_re = re.compile(r"ROBOTICK_BEGIN_FIELDS\((\w+)\)")
define_workload_re = re.compile(r"ROBOTICK_DEFINE_WORKLOAD(?:_[1-4])?\(([^)]*)\)")
workload_struct_re = re.compile(r"struct\s+(\w+Workload)\b")

# Classifier helper
def classify_struct(name: str):
    lname = name.lower()
    if "config" in lname:
        return "config"
    elif "input" in lname:
        return "inputs"
    elif "output" in lname:
        return "outputs"
    return None

# Main loop
for cpp_file in CPP_ROOT.rglob("*.cpp"):
    with open(cpp_file, "r", encoding="utf-8") as f:
        content = f.read()

    original_content = content  # Keep for later comparison

    # Extract all the things
    structs = begin_fields_re.findall(content)
    workload_names = workload_struct_re.findall(content)
    workload_macros = define_workload_re.findall(content)

    # Classify fields by role
    role_map = {}
    for s in structs:
        role = classify_struct(s)
        if role:
            role_map[role] = s

    if not workload_names or not workload_macros:
        continue

    print(f"\n📄 Processing: {cpp_file.relative_to(CPP_ROOT.parent)}")

    # Rewrite each defined workload
    for workload in workload_names:
        for full_macro in workload_macros:
            if workload not in full_macro:
                continue  # skip mismatches

            # Determine args
            config = role_map.get("config", "void")
            inputs = role_map.get("inputs", "void")
            outputs = role_map.get("outputs", "void")

            args = [workload]
            if config != "void":
                args.append(config)
            if inputs != "void":
                args.append(inputs)
            if outputs != "void":
                args.append(outputs)

            new_macro = f"ROBOTICK_DEFINE_WORKLOAD({', '.join(args)})"
            old_macro = f"ROBOTICK_DEFINE_WORKLOAD({full_macro})"
            content = content.replace(old_macro, new_macro)
            print(f"  ✔ Replaced: {old_macro} → {new_macro}")

    # Save file only if changed
    if content != original_content:
        with open(cpp_file, "w", encoding="utf-8") as f:
            f.write(content)
