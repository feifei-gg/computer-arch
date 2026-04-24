import os
import glob
import matplotlib.pyplot as plt

def parse_out_file(filepath):
    results = {}
    if not os.path.exists(filepath):
        return results
        
    with open(filepath, 'r') as f:
        for line in f:
            if ':' in line:
                parts = line.split(':')
                model = parts[0].strip()
                stats = parts[1].strip()
                if not stats:
                    continue
                req_r, req_w, hit_r, hit_w = map(int, stats.split(','))
                total_req = req_r + req_w
                total_hit = hit_r + hit_w
                hit_rate = (total_hit / total_req) * 100.0 if total_req > 0 else 0.0
                results[model] = hit_rate
    return results

def get_benchmark_name(filename):
    return filename.split('___')[0]

def main():
    all_dirs = sorted(glob.glob("results_*"), key=os.path.getmtime)
    
    if len(all_dirs) < 9:
        print(f"Error: Need at least 9 directories. Found {len(all_dirs)}.")
        return
        
    result_dirs = all_dirs[-9:]
    
    exp_a_configs = [
        {'cap_kb': 4, 'label': '4KB'},
        {'cap_kb': 16, 'label': '16KB'},
        {'cap_kb': 64, 'label': '64KB'},
        {'cap_kb': 256, 'label': '256KB'},
        {'cap_kb': 1024, 'label': '1MB'},
        {'cap_kb': 4096, 'label': '4MB'}
    ]
    
    exp_b_configs = [
        {'label': '256KB, 1-Way\n(Base Area)'},
        {'label': '256KB, 2-Way\n(More Area)'},
        {'label': '256KB, 4-Way\n(Most Area)'}
    ]

    working_set_data = {}
    tradeoff_data = {}

    for i in range(6):
        current_dir = result_dirs[i]
        cap_val = exp_a_configs[i]['cap_kb']
        out_files = glob.glob(os.path.join(current_dir, "*.out"))
        
        for filepath in out_files:
            b_name = get_benchmark_name(os.path.basename(filepath))
            parsed = parse_out_file(filepath)
            if 'physical index physical tag' in parsed:
                hr = parsed['physical index physical tag']
                if b_name not in working_set_data:
                    working_set_data[b_name] = {'x': [], 'y': []}
                working_set_data[b_name]['x'].append(cap_val)
                working_set_data[b_name]['y'].append(hr)

    for i in range(3):
        current_dir = result_dirs[6 + i]
        config_label = exp_b_configs[i]['label']
        out_files = glob.glob(os.path.join(current_dir, "*.out"))
        
        for filepath in out_files:
            b_name = get_benchmark_name(os.path.basename(filepath))
            parsed = parse_out_file(filepath)
            if 'physical index physical tag' in parsed:
                hr = parsed['physical index physical tag']
                if b_name not in tradeoff_data:
                    tradeoff_data[b_name] = []
                tradeoff_data[b_name].append(hr)

    plt.figure(figsize=(10, 6))
    for b_name, values in working_set_data.items():
        plt.plot(values['x'], values['y'], marker='o', label=b_name)
    
    plt.xscale('log', base=2)
    plt.title('Working Set Analysis: Hit Rate vs. Cache Capacity')
    plt.xlabel('Cache Capacity (KB) [Log2 Scale]')
    plt.ylabel('Hit Rate (%)')
    plt.xticks([4, 16, 64, 256, 1024, 4096], ['4KB', '16KB', '64KB', '256KB', '1MB', '4MB'])
    plt.legend(bbox_to_anchor=(1.05, 1), loc='upper left')
    plt.grid(True)
    plt.tight_layout()
    plt.savefig('Q2_Working_Set.png')
    plt.close()

    b_names = list(tradeoff_data.keys())
    x = range(len(exp_b_configs))
    
    plt.figure(figsize=(12, 6))
    for b_name in b_names:
        plt.plot(x, tradeoff_data[b_name], marker='s', label=b_name)

    plt.title('Area Trade-off: Impact of Associativity at 256KB Capacity')
    plt.xlabel('Cache Configuration')
    plt.ylabel('Hit Rate (%)')
    plt.xticks(x, [c['label'] for c in exp_b_configs])
    
    all_tradeoff_vals = [val for sublist in tradeoff_data.values() for val in sublist]
    if all_tradeoff_vals:
        min_val = min(all_tradeoff_vals)
        max_val = max(all_tradeoff_vals)
        plt.ylim(max(0, min_val - 2), min(100, max_val + 2))
        
    plt.legend(bbox_to_anchor=(1.05, 1), loc='upper left')
    plt.grid(True, linestyle='--', alpha=0.7)
    plt.tight_layout()
    plt.savefig('Q2_Tradeoff_Area.png')
    plt.close()

if __name__ == "__main__":
    main()