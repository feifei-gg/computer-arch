import os
import re
import glob
import sys


def analyze_tlb_results(directory_path):
    """
    Parses TLB simulation results from PARSEC benchmark .out files.
    """
    print(f"Analyzing results in: {directory_path}\n")

    pattern = re.compile(
        r"Number of TLB hits:\s*(\d+)\s*\|\s*Number of TLB misses:\s*(\d+)\s*\|\s*Number of TLB flushes:\s*(\d+)"
    )

    results = []

    if not os.path.exists(directory_path):
        print(f"Error: Directory '{directory_path}' not found.")
        return

    out_files = glob.glob(os.path.join(directory_path, "*.out"))

    if not out_files:
        print(f"No .out files found in '{directory_path}'.")
        return

    for filepath in out_files:
        filename = os.path.basename(filepath)

        with open(filepath, "r") as file:
            content = file.read().strip()
            match = pattern.search(content)

            if match:
                hits = int(match.group(1))
                misses = int(match.group(2))
                flushes = int(match.group(3))

                total_accesses = hits + misses
                hit_rate = (hits / total_accesses) * 100 if total_accesses > 0 else 0
                miss_rate = (misses / total_accesses) * 100 if total_accesses > 0 else 0

                bench_name = filename.split("___")[0]

                results.append(
                    {
                        "Benchmark": bench_name,
                        "Hits": hits,
                        "Misses": misses,
                        "Flushes": flushes,
                        "Hit Rate (%)": hit_rate,
                        "Miss Rate (%)": miss_rate,
                        "Total Accesses": total_accesses,
                    }
                )
            else:
                print(f"Warning: Could not parse format in {filename}")

    results.sort(key=lambda x: x["Benchmark"])

    # --- Print Formatted ASCII Table ---
    header = f"{'Benchmark':<15} | {'Hit Rate (%)':<12} | {'Miss Rate (%)':<13} | {'Total Accesses':<15} | {'Flushes':<10}"
    print("-" * len(header))
    print(header)
    print("-" * len(header))

    for r in results:
        print(
            f"{r['Benchmark']:<15} | {r['Hit Rate (%)']:>11.2f}% | {r['Miss Rate (%)']:>12.2f}% | {r['Total Accesses']:<15} | {r['Flushes']:<10}"
        )

    print("-" * len(header))


if __name__ == "__main__":
    target_dir = None

    if len(sys.argv) > 1:
        target_dir = sys.argv[1]
    else:
        possible_dirs = [d for d in glob.glob("results_*") if os.path.isdir(d)]

        if not possible_dirs:
            print("Error: no 'results_' dirs")
            sys.exit(1)

        target_dir = max(possible_dirs, key=os.path.getmtime)
        print(f"[*] Auto-detected latest results folder: {target_dir}")

    analyze_tlb_results(target_dir)
