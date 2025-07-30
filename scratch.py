import re
from pathlib import Path

CPP_ROOT = Path("cpp")

# Regex patterns
begin_fields_re = re.compile(r"ROBOTICK_REGISTER_STRUCT_BEGIN\((\w+)\)")
define_workload_re = re.compile(r"ROBOTICK_REGISTER_WORKLOAD(?:_[1-4])?\(([^)]*)\)")
workload_struct_re = re.compile(r"struct\s+(\w+Workload)\b")

def classify_field_type(name: str):
    lname = name.lower()
    if "config" in lname:
        return "config"
    elif "input" in lname:
        return "inputs"
    elif "output" in lname:
        return "outputs"
    return None

def main():
    for cpp_file in CPP_ROOT.rglob("*.cpp"):
        content = cpp_file.read_text(encoding="utf-8")

        # Find defined structs
        field_structs = begin_fields_re.findall(content)
        workload_structs = workload_struct_re.findall(content)
        define_macros = define_workload_re.findall(content)

        # Classify config/input/output by naming
        role_map = {}
        for typename in field_structs:
            role = classify_field_type(typename)
            if role:
                role_map[role] = typename

        if not role_map:
            continue  # no known roles to match

        for macro in define_macros:
            args = [arg.strip() for arg in macro.split(",")]
            if len(args) == 1 and args[0] in workload_structs:
                print(f"[MISSING TYPES] {cpp_file}")
                print(f"  Workload: {args[0]}")
                suggested = [args[0]]
                for role in ("config", "inputs", "outputs"):
                    suggested.append(role_map.get(role, "void"))
                print(f"  Suggest replacing with: ROBOTICK_REGISTER_WORKLOAD({', '.join(suggested)})\n")

if __name__ == "__main__":
    main()
