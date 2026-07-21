# -*- coding: utf-8 -*-

import logging

import re
import os
def init_log():
    # log_fmt = '%(asctime)s File "%(filename)s",line %(lineno)s  %(levelname)s: %(message)s'
    # formatter = logging.Formatter(log_fmt)
    # 创建TimedRotatingFileHandler对象
    dir="./log_data"
    logfile = "./log_data/eggController_log.log"
    if not os.path.exists(dir):
        os.mkdir(dir)
    # filesize = 50*1024
    log = logging.getLogger()
    # rotate_handler = ConcurrentRotatingFileHandler(logfile, "a", filesize, encoding="utf-8",backupCount=10)

    file_handler = logging.FileHandler(logfile, encoding="utf-8")  # 指定日志文件名application.log，默认在当前目录下创建
    file_handler.setLevel(logging.INFO)  # 设置日志级别(只输出对应级别INFO的日志信息)

    # datefmt_str = '%Y-%m-%d %H:%M:%S'
    # format_str = '%(asctime)s\t%(levelname)s\t%(message)s '
    # formatter = logging.Formatter(format_str, datefmt_str)
    # file_handler.setFormatter(formatter)
    file_handler.setFormatter(logging.Formatter('%(asctime)s - %(levelname)s - %(name)s - %(message)s', '%m/%d/%Y %H:%M:%S'))
    log.addHandler(file_handler)
    log.setLevel(logging.DEBUG)

    return log

logger=init_log()
