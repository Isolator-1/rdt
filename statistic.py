# 统计脚本
import os
import json
import csv
import matplotlib.pyplot as plt
from matplotlib.pyplot import MultipleLocator

print("Enter the dir to be counted:")
bin_path = input()

config_path = os.path.join(bin_path, "config.json")
if not os.path.isfile(config_path):
    print(f"dir {bin_path} is not valid")
else:

    with open(config_path, "r", encoding="utf-8") as configfile:
        config = json.load(configfile)

    for k in config.keys():
        if isinstance(config[k], list):
            variate_key = k
            break

    statistic_path = os.path.join(bin_path, "statictic.csv")
    params = []
    throughput_rates = []
    with open(statistic_path, "w", encoding="utf-8") as statfile:
        fieldnames = [variate_key, "filesize", "costtime", "throughput-rate"]
        writer = csv.DictWriter(statfile, fieldnames=fieldnames)
        for sub_bin_path in os.listdir(bin_path):
            result = {}
            ind = sub_bin_path.rfind("_")
            if ind == -1:
                continue
            result[variate_key] = sub_bin_path[ind + 1:]
            result_path = os.path.join(bin_path, sub_bin_path, "result.txt")
            if os.path.isfile(result_path):
                with open(result_path, "r", encoding="utf-8") as f2:
                    ll = f2.readline().split()
                    result["filesize"] = int(ll[0])
                    result["costtime"] = float(ll[1])
                    result["throughput-rate"] = float(ll[2])
                    params.append(eval(sub_bin_path[ind + 1:]))
                    throughput_rates.append(float(ll[2]))
                    writer.writerow(result)
    plot_path = os.path.join(bin_path, "plot.png")
    if len(params) > 0:
        plt.xlabel(variate_key)
        plt.ylabel("throughput rate (mbps)")
        # x_major_locator = MultipleLocator(1)
        # ax = plt.gca()
        # ax.xaxis.set_major_locator(x_major_locator)
        plt.scatter(params, throughput_rates)
        plt.savefig(plot_path)
        plt.show()
