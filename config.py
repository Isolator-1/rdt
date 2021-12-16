# 生成可执行文件脚本
# author: zqs
# date: 2021年12月16日

import time
import os
import json

RECV = ".\\rdtrecv_tcp.exe"
SEND = ".\\rdtsend_tcp.exe"

RECV_FLAGS = " 3.jpg 9999 127.0.0.1 >recv.out"
SEND_FLAGS = " D:\\files\\Projects\\202111\\reliable_udp\\input\\3.jpg 8888 127.0.0.1 9997 >send.out"

create_time = time.localtime(time.time())

time_str = time.asctime(create_time)

heads = f"""\
/**
 * @file def.hpp
 * @author zqs
 * @brief autogeneration
 * @version 0.1
 * @date {time_str}
 *
 * @copyright Copyright (c) 2021
 *
 */
#ifndef DEF_HPP
#define DEF_HPP
#define SENDER_PORT 9997
#define RECVER_PORT 9999
#define DEFAULT_ADDR "127.0.0.1"
"""
tails = """
#ifdef LOG
#undef LOG
#define LOG(s) \\
    do         \\
    {          \\
        s;     \\
    } while (0);
#else
#define LOG(s) \\
    do         \\
    {          \\
    } while (0);
#endif
#endif
"""

configs_in = {
    "N": 8,
    "NUM_SEQNUM": 65536,
    "TIMEOUT": 200,
    "INTERVAL": 50,
    "FLOW_CONTROL": True,
    "DELAY": True,
    "ACK_DELAY": 500,
    "SENDER_RECV_BUF": 16,
    "RECVER_RECV_BUF": 16,
    "CONGESTION_CONTROL": True,
    "INIT_SSTHRESH": 4,
    "DUP_ACK": 3,
    "FAST_RETRANSMIT": True,
    "LOG": False
}

variate_key = None
while variate_key not in configs_in:
    print(configs_in)
    print("Enter the parameter that you want to compare:")
    variate_key = input().strip()
print("Enter a list of values of this variate: (split with <space>)")
configs_in[variate_key] = list(map(type(configs_in[variate_key]), map(eval, input().split())))

configs_out = configs_in

changed_key = ""
while changed_key != "ok":
    print(configs_out)
    print(
        "Enter a parameter that need to be changed, enter 'ok' to finish configuration:"
    )
    changed_key = input().strip()
    if changed_key in configs_out:
        print("Enter a new value:")
        value = input()
        configs_out[changed_key] = type(configs_out[changed_key])(eval(value))

output_dir = os.path.join(
    "bin", f"{variate_key}_{time.strftime('%Y%m%d_%H%M%S', create_time)}")
os.makedirs(output_dir)
def_path = os.path.join("include", "def.hpp")

config_path = os.path.join(output_dir, "config.json")

with open(config_path, 'w', encoding='utf-8') as f:
    json.dump(configs_out, f)

for varv in configs_out[variate_key]:
    sub_output_dir = os.path.join(output_dir, variate_key + "_" + str(varv))
    os.mkdir(sub_output_dir)
    with open(def_path, "w", encoding="utf-8") as f:
        f.write(heads)
        for k in configs_out.keys():
            line = ""
            if k == variate_key:
                v = varv
            else:
                v = configs_out[k]
            if type(v) == int:
                line += "#define " + k + " " + str(v) + "\n"
            elif type(v) == bool:
                if v:
                    line += "#define " + k + "\n"
            f.write(line)
        f.write(tails)
    def_path_copy = os.path.join(sub_output_dir, "def.hpp")
    cmd = f"copy {def_path} {def_path_copy}"
    print(cmd)
    os.system(cmd)
    cmd = f"make BIN_DIR={sub_output_dir}/"
    os.system(cmd)
    send_bat_path = os.path.join(sub_output_dir, "send.bat")
    recv_bat_path = os.path.join(sub_output_dir, "recv.bat")
    with open(send_bat_path, "w", encoding="utf-8") as f2:
        f2.write(SEND + SEND_FLAGS + "\n")
    with open(recv_bat_path, "w", encoding="utf-8") as f2:
        f2.write(RECV + RECV_FLAGS + "\n")
