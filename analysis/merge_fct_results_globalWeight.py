import csv
import os
import argparse

def merge_fct_csvs(util_value, input_dir, output_dir, ABLATIONS):
    """
    Merges FCT slowdown CSV files from different ablation for a given util value.
    """
    merged_data = []
    headers = ['Percentile', 'FlowSize']

    ablation_data = {}
    for ablation in ABLATIONS:
        file_path = os.path.join(input_dir, f"{ablation}-FCTslowdown.csv")
        if not os.path.exists(file_path):
            print(f"Warning: File {file_path} does not exist. Skipping.")
            continue

        with open(file_path, 'r', newline='') as csvfile:
            reader = csv.reader(csvfile)
            csv_headers = next(reader)
            for col in csv_headers[2:]:
                headers.append(col)
            ablation_data[ablation] = list(reader)

    if len(ablation_data) == 0:
        print(f"Error: No data files found for {util_value}")
        return False

    base_ablation = next(iter(ablation_data))
    base_data = ablation_data[base_ablation]

    for i, row in enumerate(base_data):
        merged_row = [row[0], row[1]]
        for ablation in ABLATIONS:
            if ablation in ablation_data:
                merged_row.extend(ablation_data[ablation][i][2:])
        merged_data.append(merged_row)

    os.makedirs(output_dir, exist_ok=True)
    fileName = f"merged_{util_value}-FCTslowdown.csv"
    output_file = os.path.join(output_dir, fileName)
    with open(output_file, 'w', newline='') as csvfile:
        writer = csv.writer(csvfile)
        writer.writerow(headers)
        writer.writerows(merged_data)

    print(f"Merged file created: {fileName}")
    return True

def main():
    parser = argparse.ArgumentParser(description='Merge FCT slowdown CSV files from different ablation methods')
    parser.add_argument('-i', '--input_dir', required=True, help='Input directory containing CSV files')
    parser.add_argument('-o', '--output_dir', required=True, help='Output directory for merged CSV files')
    args = parser.parse_args()

    UTILs = [
        # '0.3util',
        # '0.5util',
        # '0.8util'
        'globalWeight'
    ]
    ABLATIONS = [
       "alpha=1-beta=3",
		"alpha=2-beta=2",
		"alpha=3-beta=1",
    ]

    output_dir = args.output_dir
    os.makedirs(output_dir, exist_ok=True)
    success_count = 0
    for util in UTILs:
        if merge_fct_csvs(util, args.input_dir, output_dir, ABLATIONS):
            success_count += 1

    print(f"Process completed. Created {success_count} merged files.")

if __name__ == "__main__":
    main()