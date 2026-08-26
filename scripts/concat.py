import glob
from io import StringIO
import matplotlib.pyplot as plt
from multiprocessing import Pool
from natsort import natsorted
import numpy as np
import pandas as pd
import re
import sys

def get_db(filename, prefix):
    with open(filename) as file:
        lines = [line[len(prefix):].lstrip().rstrip() for line in file if line.startswith(prefix)]
        return pd.read_csv(StringIO('\n'.join(lines)), skipinitialspace=True)

planeNames = {
        100: 'Boeing 737-400',
        101: 'Boeing 737-800',
        102: 'Boeing 747-100',
        103: 'Boeing 747-400 Domestic',
        104: 'Boeing 747-400',
        105: 'Boeing 757-300',
        106: 'Boeing 777-300',
        107: 'McDonnell Douglas DC 10',
        108: 'Airbus Industrie A 320',
        109: '---',
        110: 'Airbus Industrie A 310',
        111: 'Airbus Industrie A 300',
        112: 'Ilyushin Sowjetunion Il 62',
        113: 'Ilyushin Sowjetunion Il 86',
        114: 'Tupolev  Tu 154 B',
        115: 'Lockheed Aircraft Corp. L-1011 Tristar 500',
        116: 'BAC Aerospatiale Concorde',
        117: 'Boeing 707-320C',
        118: 'Boeing 720',
        119: 'Grumman Engineering Corp. Gulfstream II',
        120: 'Boeing 767-300 ER',
        121: 'McDonnell Douglas MD 81',
        122: 'McDonnell Douglas DC 8 Super 70',
        123: 'de Havilland Canada DHC 8 Dash 8',
        125: 'Boeing 727-200',
        126: 'Lockheed Corp. C5A Galaxy',
        124: 'NxT LvL Engineering'
        }

def run_for_file(file):
    #print(file)
    match = re.findall(r'_([\d\w]+)_(\d+)(_\w+)?.csv', file)
    if not match:
        return
    match = match[0]

    param = str(match[0])
    run = int(match[1])
    #print('Param: '+param+', run: '+str(run))

    detailledStat = pd.DataFrame()
    for i in ["SA", "FL", "PT", "HA"]:
        a = get_db(file, 'BotStatistics/'+i+': ')
        a = a.set_index('Tag')

        a['KerosinSaldo1'] = a.KerosinGespart - a.ExpansionTanks
        a['KerosinSaldo2'] = a.KerosinVorrat + a.KerosinFlug - a.ExpansionTanks

        a['Param'] = param
        a['Airline'] = i

        detailledStat = pd.concat([detailledStat, a])

    return detailledStat

if __name__ == '__main__':
    pool = Pool()

    filepattern = 'data_*.csv'
    airlines = ['HA']
    columns = ['SaldoGesamt', 'Firmenwert']
    if len(sys.argv) > 1:
        filepattern = sys.argv[1]
    if len(sys.argv) > 2:
        airlines = sys.argv[2].split(',')
    if len(sys.argv) > 3:
        columns = sys.argv[3].split(',')

    files = glob.glob(filepattern)
    files = natsorted(files)

    results = pool.map(run_for_file, files)

    overall = pd.DataFrame()
    for detailledStat in results:
        overall = pd.concat([overall, detailledStat])

    agg = {}
    for i in columns:
        #agg[i] = np.std
        agg[i] = 'mean'

    data = (overall.groupby(['Tag', 'Airline', 'Param']).agg(agg))
    data.reset_index(inplace=True)
    data.set_index('Tag', inplace=True)
    print(data)
    print("Day 99 / SaldoGesamt / Airline HA: ", data[(data.index == 99) & (data['Airline'] == 'HA')]['SaldoGesamt'].to_list()[0])
    print("Day 99 / Firmenwert / Airline HA: ", data[(data.index == 99) & (data['Airline'] == 'HA')]['Firmenwert'].to_list()[0])

    for c in columns:
        ax = None

        for p in data['Param'].unique():
            df1 = data.loc[data['Param'] == p]
            for a in airlines:
                df2 = df1.loc[df1['Airline'] == a]
                name = '_'.join([a,p])
                ax = df2[[c]].rename(columns={c: name}).plot(title=c, ax=ax)

    plt.show()

