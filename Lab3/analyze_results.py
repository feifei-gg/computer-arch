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
    result_dirs = sorted(glob.glob("results_*"), key=os.path.getmtime)

    configs = [
        {'exp': 'Assoc', 'x_val': 1, 'x_label': '1-Way'},
        {'exp': 'Assoc', 'x_val': 2, 'x_label': '2-Way'},
        {'exp': 'Assoc', 'x_val': 4, 'x_label': '4-Way'},
        {'exp': 'Assoc', 'x_val': 8, 'x_label': '8-Way'},

        {'exp': 'Block', 'x_val': 4, 'x_label': '4B'},
        {'exp': 'Block', 'x_val': 8, 'x_label': '8B'},
        {'exp': 'Block', 'x_val': 16, 'x_label': '16B'},
        {'exp': 'Block', 'x_val': 32, 'x_label': '32B'},
        {'exp': 'Block', 'x_val': 64, 'x_label': '64B'},

        {'exp': 'Capacity', 'x_val': 128 * 4 * 1, 'x_label': '0.5KB'},
        {'exp': 'Capacity', 'x_val': 256 * 4 * 1, 'x_label': '1KB'},
        {'exp': 'Capacity', 'x_val': 512 * 4 * 1, 'x_label': '2KB'},
        {'exp': 'Capacity', 'x_val': 1024 * 4 * 1, 'x_label': '4KB'},
        {'exp': 'Capacity', 'x_val': 2048 * 4 * 1, 'x_label': '8KB'}
    ]

    if len(result_dirs) < len(configs):
        print(f"Error: Found {len(result_dirs)} dirs, expected at least {len(configs)}.")
        return

    data = {'Assoc': {}, 'Block': {}, 'Capacity': {}}
    model_compare_data = {}

    for i, config in enumerate(configs):
        current_dir = result_dirs[i]
        exp_type = config['exp']
        x_val = config['x_val']

        out_files = glob.glob(os.path.join(current_dir, "*.out"))

        for filepath in out_files:
            b_name = get_benchmark_name(os.path.basename(filepath))
            parsed_data = parse_out_file(filepath)

            if 'physical index physical tag' not in parsed_data:
                continue

            pipt_hit_rate = parsed_data['physical index physical tag']

            if b_name not in data[exp_type]:
                data[exp_type][b_name] = {'x': [], 'y': []}

            data[exp_type][b_name]['x'].append(x_val)
            data[exp_type][b_name]['y'].append(pipt_hit_rate)

            if i == 0:
                model_compare_data[b_name] = parsed_data

    for exp_type, title, xlabel in [
        ('Capacity', 'Effect of Cache Capacity on Hit Rate (PIPT)', 'Cache Capacity (Bytes)'),
        ('Assoc', 'Effect of Associativity on Hit Rate (PIPT)', 'Associativity (Ways)'),
        ('Block', 'Effect of Block Size on Hit Rate (PIPT)', 'Block Size (Bytes)')
    ]:
        plt.figure(figsize=(10, 6))
        for b_name, values in data[exp_type].items():
            plt.plot(values['x'], values['y'], marker='o', label=b_name)

        plt.title(title)
        plt.xlabel(xlabel)
        plt.ylabel('Hit Rate (%)')
        plt.legend(bbox_to_anchor=(1.05, 1), loc='upper left')
        plt.grid(True)
        plt.tight_layout()
        plt.savefig(f'{exp_type}_effect.png')
        plt.close()

    if model_compare_data:
        b_names = list(model_compare_data.keys())
        pipt_vals = [model_compare_data[b]['physical index physical tag'] for b in b_names]
        vipt_vals = [model_compare_data[b]['virtual index physical tag'] for b in b_names]
        vivt_vals = [model_compare_data[b]['virtual index virtual tag'] for b in b_names]

        x = range(len(b_names))
        width = 0.25

        plt.figure(figsize=(12, 6))
        plt.bar([pos - width for pos in x], pipt_vals, width, label='PIPT')
        plt.bar(x, vipt_vals, width, label='VIPT')
        plt.bar([pos + width for pos in x], vivt_vals, width, label='VIVT')

        plt.xlabel('Benchmarks')
        plt.ylabel('Hit Rate (%)')
        plt.title('Comparison of Cache Models (Base Config: 512 Rows, 4B Block, 1-Way)')
        plt.xticks(x, b_names, rotation=45)
        plt.legend()
        plt.tight_layout()
        plt.savefig('Model_Comparison.png')
        plt.close()

if __name__ == "__main__":
    main()
