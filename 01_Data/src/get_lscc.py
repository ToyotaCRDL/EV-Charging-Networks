#coding: utf-8
import sys
import pandas as pd
import numpy as np
import networkx as nx
from scipy import stats

def get_lscc(in_file):
        Data = open(in_file, "r")
        next(Data, None) # 最初の行をスキップ

        Graphtype = nx.Graph() # 無向グラフ
        lines = Data.readlines()
        G = nx.parse_edgelist(lines, delimiter=',', create_using=Graphtype)
        
        max_size = 0
        for component in nx.connected_components(G):
                if len(component) > max_size:
                        max_cluster = component
                        max_size = len(max_cluster)
        return max_cluster

## io
args  = sys.argv
iFile = args[1]
oFile = args[2]

## Main
result = get_lscc(iFile)

## Output
with open(oFile,'w') as f:
        f.write('id\n')
        for ele in result:
                f.write(ele+'\n')
        
