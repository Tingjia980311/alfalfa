import sys
from client import CoordClient
from proto.common_pb2 import *
from proto.operation_pb2 import *
import time

client = CoordClient('ae0a61f8f05704ed9b543325cf14f7a8-102497153.us-east-1.elb.amazonaws.com', '172.20.47.124', thread_id=0)
functions = ['xc-dump','xc-enc']
dependency = ([('xc-dump', 1), ('xc-dump', 0)], [('xc-enc',0)], MANY_TO_ONE)
dependencies = []
chunk_num = 2
for i in range(1, chunk_num):
    for j in range(i):
        tgt = [('xc-enc', (i*i - i) / 2 + j)]
        if j == 0:
            src = [('xc-dump', i), ('xc-dump', i-1)]
        elif j == 1:
            src = [('xc-enc', (i*i - i) / 2 ), ('xc-enc', (i-1)*(i-1)/2-(i-1)/2), ('xc-dump', i - 1)]
        else:
            src = [('xc-enc', (i*i - i) / 2 + j - 1), ('xc-enc', (i-1)*(i-1)/2-(i-1)/2 + j - 1), ('xc-enc', (i-1)*(i-1)/2-(i-1)/2 + j - 2)]
        dependencies.append((src, tgt, MANY_TO_ONE))


client.register_app('video-encoder', functions, [dependency])

client.call_app('video-encoder', {('xc-dump', i): [str(i)] for i in range(chunk_num)})
