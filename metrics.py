import re
from collections import defaultdict

LOG_FILE = "merged.log"

pattern = re.compile(r"(\d+)\s+(\S+)\s+(\d+)\s+(\d+)\s+(\S+)(?:\s+(\d+))?")

events = defaultdict(list)

with open(LOG_FILE, "r") as f:
    for line in f:
        m = pattern.match(line.strip())
        if not m:
            continue
        timestamp = int(m.group(1))
        device = m.group(2)
        device_id = int(m.group(3))
        task_id = int(m.group(4))
        event = m.group(5)
        events[task_id].append((timestamp, event))

# Sort events by timestamp
for task_id in events:
    events[task_id].sort(key=lambda x: x[0])

results = []

for task_id, ev_list in events.items():
    sched_times = [t for t, e in ev_list if e == "ENTER_SCHED"]
    cpu_enters = [t for t, e in ev_list if e == "ENTER_CPU"]
    finishes = [t for t, e in ev_list if e in ("FINISH_CPU", "FINISH_IO")]

    if not sched_times or not cpu_enters:
        continue  # cannot calculate anything without at least one SCHED and CPU

    first_sched = sched_times[0]
    last_event_time = max(t for t, _ in ev_list)

    # Turnaround: use FINISH if exists, otherwise last event as approximation
    if finishes:
        turnaround = finishes[-1] - first_sched
    else:
        turnaround = last_event_time - first_sched

    # Response time = first CPU - first sched
    response = cpu_enters[0] - first_sched

    # Waiting time: sum of (ENTER_CPU - previous ENTER_SCHED)
    waiting_time = 0
    for enter_cpu in cpu_enters:
        # most recent ENTER_SCHED before this CPU
        sched_before_cpu = max((s for s in sched_times if s < enter_cpu), default=None)
        if sched_before_cpu is not None:
            waiting_time += enter_cpu - sched_before_cpu

    results.append({
        "task_id": task_id,
        "waiting_time": waiting_time,
        "turnaround_time": turnaround,
        "response_time": response
    })

# Print results
#print(f"{'Task':>6} {'Waiting':>10} {'Turnaround':>12} {'Response':>10}")
#for r in sorted(results, key=lambda x: x["task_id"]):
#    print(f"{r['task_id']:>6} {r['waiting_time']:>10} {r['turnaround_time']:>12} {r['response_time']:>10}")

# Compute averages
if results:
    avg_wait = sum(r["waiting_time"] for r in results) / len(results)
    avg_turn = sum(r["turnaround_time"] for r in results) / len(results)
    avg_resp = sum(r["response_time"] for r in results if r["response_time"] is not None) / len(
        [r for r in results if r["response_time"] is not None]
    )
    #print(f"\nAverage Waiting Time: {avg_wait:.2f}")
    #print(f"Average Turnaround Time: {avg_turn:.2f}")
    #print(f"Average Response Time: {avg_resp:.2f}")
    #print(f"{avg_wait:.2f}")
    #print(f"{avg_turn:.2f}")
    #print(f"{avg_resp:.2f}")
    

print()

import re
from collections import defaultdict

LOG_FILE = "merged.log"

cpu_pattern = re.compile(r"\s*CPU\s+(\d+)")

# First pass: detect all CPU IDs
cpu_ids = set()

with open(LOG_FILE, "r") as f:
    for line in f:
        m = cpu_pattern.search(line)
        if m:
            cpu_ids.add(int(m.group(1)))

if not cpu_ids:
    print("No CPU lines found.")
    exit()

num_cpu = max(cpu_ids) + 1
#print(f"Detected num_cpu = {num_cpu}")

# Data structure: cpu_id -> list of (line, timestamp)
cpu_lines = {cpu_id: [] for cpu_id in range(num_cpu)}

# Second pass: collect lines for each CPU
with open(LOG_FILE, "r") as f:
    for line in f:
        parts = line.strip().split()
        if len(parts) < 3:
            continue

        if parts[1] == "CPU":
            cpu_id = int(parts[2])
            if cpu_id in cpu_lines:
                timestamp = int(parts[0])
                cpu_lines[cpu_id].append((line.strip(), timestamp))


total_exec_times = []

for cpu_id in sorted(cpu_lines.keys()):
    total_exec_time = 0

    with open(f"cpu{cpu_id}.log", "r") as f:
        for line in f:
            parts = line.strip().split()
            # Check if last part is a number
            if parts[-1].isdigit():
                total_exec_time += int(parts[-1])

    total_exec_times.append(total_exec_time)

print("CPU exec time: ", total_exec_times)



total_cpu_times = []

# Output results
for cpu_id in sorted(cpu_lines.keys()):
    lines = cpu_lines[cpu_id]

    if len(lines) < 2:
        total_cpu_times.append(0)
        continue
    second_line, second_ts = lines[1]
    last_line, last_ts = lines[-1]

    total_cpu_time = last_ts - second_ts
    total_cpu_times.append(total_cpu_time)

print("CPU total time:", total_cpu_times)
    
print()
if results:
    avg_wait = sum(r["waiting_time"] for r in results) / len(results)
    avg_turn = sum(r["turnaround_time"] for r in results) / len(results)
    avg_resp = sum(r["response_time"] for r in results if r["response_time"] is not None) / len(
        [r for r in results if r["response_time"] is not None]
    )
    #print(f"\nAverage Waiting Time: {avg_wait:.2f}")
    #print(f"Average Turnaround Time: {avg_turn:.2f}")
    #print(f"Average Response Time: {avg_resp:.2f}")
    print(f"{avg_wait:.2f}")
    print(f"{avg_turn:.2f}")
    print(f"{avg_resp:.2f}")
    
#print("CPU Utilization")
for cpu_id in sorted(cpu_lines.keys()):
    if total_cpu_times[cpu_id] != 0:
        #print(f"CPU {cpu_id}: {total_exec_times[cpu_id]/total_cpu_times[cpu_id]*100:.2f}%")
        print(f"{total_exec_times[cpu_id]/total_cpu_times[cpu_id]*100:.2f}")
    else:
        #print(f"CPU {cpu_id}: {0:.2f}%")
        print(f"{0:.2f}")
        