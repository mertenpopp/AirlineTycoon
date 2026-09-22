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

missions = {
        0: "0: 10 Aufträge",
        1: "1: 2500 Passagiere",
        2: "2: 5 Mio. Gewinn",
        3: "3: 5/6 Routen",
        4: "4: 75% Image",
        5: "5: 10 Raketenteile",
        11: "A0: 10 Mio. Schulden",
        12: "A1: 1000 t Fracht",
        13: "A2: FreeFracht in 21 Tagen",
        14: "A3: Meilen in 30 Tagen",
        15: "A4: 150 Servicepunkte",
        16: "A5: Firmenwert in 21 Tagen",
        17: "A6: 2 Flugzeuge auf 90% Zustand",
        18: "A7: Aktienkurs auf 220 $",
        19: "A8: 200 Aufträge von Uhrig",
        20: "A9: 10 Stationsteile",
        41: "B0: 15 Mio. auf Konto",
        42: "B1: 5 Flugzeuge mit Sicherheit",
        43: "B2: 500 Passagiere pro Flugzeug und Tag",
        44: "B3: Keine Sabotage für 15 Tage",
        45: "B4: Design: Großes Flugzeug",
        46: "B5: Keine Sabotage für 15 Tage #2",
        47: "B6: Aktienkurs und Eigenanteil",
        48: "B7: Design: Eco-Flugzeug",
        49: "B8: Firmenwert in 45 Tagen",
        50: "B9: Firmenwert in 60 Tagen"
        }

def run_for_file(file):
    match = re.findall(r'_(\d+)_mission_?(\w+)?_(\d+).csv', file)
    if not match:
        return
    match = match[0]

    param = str(match[1])
    run = int(match[2])
    #print('Param: '+param+', run: '+str(run))

    missionStat = get_db(file, 'BotMission: ')

    missionID = missionStat.loc[0, 'Mission']
    missionStat['Mission'] = missions[missionID]
    missionStat = missionStat.set_index('Mission')

    missionStat['Param'] = param 

    detailledStat = pd.DataFrame()
    for i in ["SA", "FL", "PT", "HA"]:
    #for i in ["HA"]:
        # read different statistics
        dayStat = get_db(file, 'BotStatistics/'+i+': ')
        endStat = get_db(file, 'BotStatistics2/'+i+': ')
        #dayStat = get_db(file, 'BotStatistics: ')
        #endStat = get_db(file, 'BotStatistics2: ')

        endStat['Tag'] += 1
        dayStat = dayStat.set_index('Tag')
        endStat = endStat.set_index('Tag')
        allDays = pd.concat([dayStat, endStat])

        allDays['Mission'] = missions[missionID]
        allDays['Param'] = param
        allDays['Airline'] = i

        detailledStat = pd.concat([detailledStat, allDays])

    return missionStat, detailledStat

if __name__ == '__main__':
    pool = Pool()

    filepattern = 'dataNEW_*.csv'
    airlines = ['HA']
    columns = ['Saldo']
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
    overallDetails = pd.DataFrame()
    for missionStat, detailledStat in results:
        overall = pd.concat([overall, missionStat])
        overallDetails = pd.concat([overallDetails, detailledStat])

    # Plot 1
    data = (overall.groupby(['Mission', 'Param'])
                   .agg(winSA=('SiegSA', 'mean'),
                        winFL=('SiegFL', 'mean'),
                        winPT=('SiegPT', 'mean'),
                        winHA=('SiegHA', 'mean'))
            )
    print(data)
    data.plot.bar(color=['blue', 'green', 'red', 'yellow'])

    # Plot 2
    data = (overall.groupby(['Mission', 'Param'])
                   .agg(bester=('BesterGegner', 'mean'))
            )
    data.plot.bar()

    # Plot 3
    data = (overallDetails.groupby(['Tag', 'Airline', 'Mission', 'Param']).mean(numeric_only=True))
    data.reset_index(inplace=True)
    data.set_index('Tag', inplace=True)

    for c in columns:
        for m in data['Mission'].unique():
            df1 = data.loc[data['Mission'] == m]

            ax = None

            for p in df1['Param'].unique():
                df2 = df1.loc[df1['Param'] == p]
                for a in airlines:
                    df3 = df2.loc[df2['Airline'] == a]
                    titleName = ': '.join([m,c])
                    plotName = '_'.join([a,p])
                    ax = df3[[c]].rename(columns={c: plotName}).plot(title=titleName, ax=ax)

    plt.show()
