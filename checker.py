import re, sys
from typing import List, Dict

RE_SERVER_START = re.compile(r'\[SERVER-(\d+)\] Starting')
RE_WRITE = re.compile(r'\[(SERVER-\d+)\] Apply Write \| From: \[(CLIENT-\d+)\] \| <(\w+),\s*(\d+)> \| Vector Clock: (\[.*?\])')
RE_READ = re.compile(r'\[(SERVER-\d+)\] Read Performed \| From: \[(CLIENT-\d+)\] \| <(\w+),\s*(\d+)> \| Vector Clock: (\[.*?\])')
RE_VECTOR_CLOCK = re.compile(r'(\d+),(\d+)')

def extract_server_ids(path: str) -> List[int]:
    server_ids = set()
    with open(path, "r") as f:
        for line in f:
            m = RE_SERVER_START.match(line)
            if m:
                server_ids.add(int(m.group(1)))
    return sorted(server_ids)

def parse_vector_clock(vc_str: str, server_ids: List[int]) -> List[int]:
    vc_dict = {int(sid): int(clock) for sid, clock in RE_VECTOR_CLOCK.findall(vc_str)}
    return [vc_dict.get(sid, 0) for sid in server_ids]

def parse_log_file(path: str, server_ids: List[int]) -> List[Dict]:
    parsed = []
    with open(path, "r") as f:
        for i, line in enumerate(f):
            m = RE_WRITE.search(line)
            if m:
                parsed.append({
                    "op": "WRITE",
                    "server": m.group(1),
                    "client": m.group(2),
                    "key": m.group(3),
                    "value": m.group(4),
                    "vc": parse_vector_clock(m.group(5), server_ids),
                    "id": i
                })
                continue

            m = RE_READ.search(line)
            if m:
                parsed.append({
                    "op": "READ",
                    "server": m.group(1),
                    "client": m.group(2),
                    "key": m.group(3),
                    "value": m.group(4),
                    "vc": parse_vector_clock(m.group(5), server_ids),
                    "id": i
                })
    return parsed


def dominates(vc1: List[int], vc2: List[int]) -> bool:
    return all(x >= y for x, y in zip(vc1, vc2))

def equals(vc1: List[int], vc2: List[int]) -> bool:
    return all(x == y for x, y in zip(vc1, vc2))

def check_causal_consistency(log: List[Dict]):
    violation_found = False

    # Save the list of (ordered) writes for each key
    writes_by_key = {}
    for entry in log:
        if entry["op"] == "WRITE":
            key = entry["key"]
            if key not in writes_by_key:
                writes_by_key[key] = []
            writes_by_key[key].append(entry)

    # Check that causal consistency is respected for reads
    for read in log:
        if read["op"] != "READ":
            continue

        key = read["key"]
        val = read["value"]
        read_vc = read["vc"]
        all_writes_for_key = writes_by_key.get(key, [])

        # Find writes that are causally consistent
        preceding_writes = [w for w in all_writes_for_key if dominates(read_vc, w["vc"])]

        if not preceding_writes:
            if val != "0":
                violation_found = True
                print(
                    f"Causal violation: READ at {read['server']} from {read['client']} "
                    f"sees value '{val}' with VC {read_vc}, saw non-default value but no preceding write found."
                )
            continue
        
        # Here we check that W causally precedes the read R and they have the same value.
        # Also there musn't be another write W' s.t. W' causally follows W  W' also causally precedes the read R.
        maximal_writes = []

        for w1 in preceding_writes:
            is_maximal = True

            for w2 in preceding_writes:
                if w1['id'] != w2['id']: # distinct writes check
                    if dominates(w2["vc"], w1["vc"]) and not equals(w1["vc"], w2["vc"]):
                        # w1 is dominated by w2, so w1 is not maximal
                        is_maximal = False
                        break

            if is_maximal:
                maximal_writes.append(w1)

        maximal_values = {w['value'] for w in maximal_writes}
        
        if val not in maximal_values:
            violation_found = True
            print(
                f"Causal violation: READ at {read['server']} from {read['client']} "
                f"sees value '{val}' with VC {read_vc}, but no consistent write exists."
            )
    
    if(not violation_found):
        print("No causal violations found")

def clean_log(input_log_path: str, output_log_path: str):
    prefix = "- INFO:"
    try:
        with open(input_log_path, 'r') as infile, open(output_log_path, 'w') as outfile:
            for line in infile:
                if line.startswith(prefix):
                    # Remove the prefix and potential leading space
                    cleaned_line = line[len(prefix):].lstrip()
                    outfile.write(cleaned_line)
    except FileNotFoundError:
        print(f"Error: Input log file '{input_log_path}' not found.")
        return False
    return True

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python checker.py <raw_log_path> [final_log_path]")
        sys.exit(1)
    
    raw_log_path = sys.argv[1]
    log_path = sys.argv[2] if len(sys.argv) > 2 else "output.txt"

    clean_log(raw_log_path, log_path)

    server_ids = extract_server_ids(log_path)

    operations_list = parse_log_file(log_path, server_ids)

    check_causal_consistency(operations_list)
