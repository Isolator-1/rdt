# from multiprocessing import Process
# import os


# def exec(dir_path, exe_name, flags):
#     # cmd = "cd " + dir_path
#     # os.system(cmd)
#     cmd = exe_name + flags
#     print(cmd)
#     os.system(cmd)


# if __name__ == '__main__':
#     RECV = "rdtrecv_tcp.exe"
#     SEND = "rdtsend_tcp.exe"

#     bin_dir = "bin\\N_20211216_090450"
#     RECV_FLAGS = " 3.jpg 9999 127.0.0.1 > rdtrecv_tcp.out"
#     SEND_FLAGS = " D:\\files\\Projects\\202111\\reliable_udp\\input\\3.jpg 9999 127.0.0.1 9997 > rdtsend_tcp.out"

#     if not os.path.isdir(bin_dir):
#         print("Not a dir: " + bin_dir)
#     else:
#         for root, dirs, files in os.walk(bin_dir):
#             for dir in dirs:
#                 sub_bin_dir = os.path.join(root, dir)
#                 recver = os.path.join(sub_bin_dir, RECV)
#                 sender = os.path.join(sub_bin_dir, SEND)
#                 if not os.path.isfile(recver) or not os.path.isfile(sender):
#                     print(recver + " or " + sender + "is not a file")
#                     continue
#                 p1 = Process(target=exec, args=(sub_bin_dir, recver, RECV_FLAGS))
#                 p1.start()
#                 p2 = Process(target=exec, args=(sub_bin_dir, sender, SEND_FLAGS))
#                 p2.start()
#                 p1.join()
#                 p2.join()
