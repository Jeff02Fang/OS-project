import random

def split_cpu_burst(cpu_id, duration, io_ids, max_cpu_time):
    """Split a long CPU burst if it exceeds max_cpu_time."""
    bursts = []
    while duration > max_cpu_time:
        bursts += [cpu_id, max_cpu_time]
        # Insert a short I/O burst between splits to simulate context switch or blocking
        bursts += [random.choice(io_ids), random.randint(10, 30)]
        duration -= max_cpu_time
    bursts += [cpu_id, duration]
    return bursts


def generate_task(task_id, max_cpu=2, max_cpu_burst=400, max_io_burst=150, max_cpu_time=300):
    """
    Generate a single simulated task with realistic Linux-style burst behavior.
    - First burst always CPU.
    - No consecutive bursts of same device type (CPU/I/O).
    - CPU bursts longer than max_cpu_time are split into smaller ones.
    """
    t = random.choices(
        ["cpu_bound", "interactive", "realtime", "background"],
        weights=[0.4, 0.3, 0.2, 0.1]
    )[0]

    rt_priority, nice, policy = 0, 0, 0
    bursts = []

    cpu_ids = list(range(max_cpu))
    io_ids = [4, 5]

    # ---------- Task Type Setup ----------
    if t == "cpu_bound":
        policy = 0  # SCHED_OTHER
        nice = random.randint(-20, 0)
        num_bursts = random.randint(1, 3)
        for i in range(num_bursts):
            cpu_id = random.choice(cpu_ids)
            duration = random.randint(100, max_cpu_burst)
            bursts += split_cpu_burst(cpu_id, duration, io_ids, max_cpu_time)
            if i < num_bursts - 1:
                bursts += [random.choice(io_ids), random.randint(20, max_io_burst)]

    elif t == "interactive":
        policy = 0  # SCHED_OTHER
        nice = random.randint(0, 10)
        num_bursts = random.randint(3, 6)
        for i in range(num_bursts):
            if i % 2 == 0:  # CPU first, alternating
                cpu_id = random.choice(cpu_ids)
                duration = random.randint(10, max_cpu_burst // 4)
                bursts += split_cpu_burst(cpu_id, duration, io_ids, max_cpu_time)
            else:
                bursts += [random.choice(io_ids), random.randint(5, max_io_burst // 2)]

    elif t == "realtime":
        policy = random.choice([1, 2])  # SCHED_FIFO or SCHED_RR
        rt_priority = random.randint(80, 99)
        num_bursts = random.randint(1, 3)
        for i in range(num_bursts):
            cpu_id = random.choice(cpu_ids)
            duration = random.randint(20, max_cpu_burst // 2)
            bursts += split_cpu_burst(cpu_id, duration, io_ids, max_cpu_time)
            if i < num_bursts - 1:
                bursts += [random.choice(io_ids), random.randint(10, max_io_burst // 2)]

    elif t == "background":
        policy = 0  # SCHED_OTHER
        nice = random.randint(10, 19)
        num_bursts = random.randint(1, 3)
        for i in range(num_bursts):
            if i == 0:
                cpu_id = random.choice(cpu_ids)
                duration = random.randint(50, max_cpu_burst // 2)
                bursts += split_cpu_burst(cpu_id, duration, io_ids, max_cpu_time)
            else:
                if random.random() < 0.5:
                    cpu_id = random.choice(cpu_ids)
                    duration = random.randint(50, max_cpu_burst // 2)
                    bursts += split_cpu_burst(cpu_id, duration, io_ids, max_cpu_time)
                else:
                    bursts += [random.choice(io_ids), random.randint(50, max_io_burst)]

    # ---------- Sanity Checks ----------
    if not bursts:
        bursts = [random.choice(cpu_ids), random.randint(10, max_cpu_burst)]

    # Ensure first burst is CPU
    if bursts[0] not in cpu_ids:
        bursts = [random.choice(cpu_ids), random.randint(10, max_cpu_burst)] + bursts

    # Ensure no consecutive bursts have same type
    final_bursts = [bursts[0], bursts[1]]
    for i in range(2, len(bursts), 2):
        last_dev = final_bursts[-2]
        current_dev = bursts[i]
        if (last_dev in cpu_ids and current_dev in cpu_ids) or (last_dev in io_ids and current_dev in io_ids):
            # Replace with opposite type
            if last_dev in cpu_ids:
                current_dev = random.choice(io_ids)
            else:
                current_dev = random.choice(cpu_ids)
        final_bursts += [current_dev, bursts[i + 1]]

    parts = [task_id, rt_priority, nice, policy] + final_bursts
    return " ".join(map(str, parts))


def generate_tasks(
    n=10,
    max_cpu=2,
    max_cpu_burst=400,
    max_io_burst=150,
    max_cpu_time=300,
    seed=None,
    output_file="tasks.txt"
):
    """Generate n random tasks and write them to a text file."""
    if seed is not None:
        random.seed(seed)
        print(f"[Seed set to {seed}]")

    tasks = [
        generate_task(i, max_cpu, max_cpu_burst, max_io_burst, max_cpu_time)
        for i in range(n)
    ]

    with open(output_file, "w") as f:
        f.write("\n".join(tasks))

    print(f"[{n} tasks written to {output_file}]")
    return tasks


if __name__ == "__main__":
    num_tasks = 2048
    max_cpu = 4
    max_cpu_burst = 400     # typical CPU burst upper bound
    max_io_burst = 150      # typical I/O burst upper bound
    max_cpu_time = 300      # split threshold for long CPU bursts
    seed_value = 777
    output_filename = rf"tasks/task{num_tasks}.txt"

    generate_tasks(num_tasks, max_cpu, max_cpu_burst, max_io_burst, max_cpu_time, seed_value, output_filename)
