"""Paired comparison of two seeded measurements.

threadpool.rb plays run j with "/seed <base + j + 1>", so run j of two measurements taken with the
same seed base is the same game. Comparing the builds game by game removes the game-to-game spread
from the difference, which is most of the noise of an unpaired comparison.

Usage (in the game directory):
    python compare_paired.py 'dataREF_*.csv' 'dataBOT_*.csv' [airline=HA] [day=59] [column=SaldoGesamt] [witness=FL]
"""

import glob
import math
import re
import sys
from io import StringIO

import pandas as pd


def load(pattern, airline):
    prefix = "BotStatistics/" + airline + ": "
    runs = {}
    for filename in glob.glob(pattern):
        match = re.search(r"_(\d+)\.csv$", filename)
        if not match:
            continue
        with open(filename, errors="ignore") as file:
            lines = [line[len(prefix):].strip() for line in file if line.startswith(prefix)]
        if len(lines) < 2:
            continue
        runs[int(match.group(1))] = pd.read_csv(StringIO("\n".join(lines)), skipinitialspace=True).set_index("Tag")
    return runs


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        sys.exit(1)
    pattern_a, pattern_b = sys.argv[1], sys.argv[2]
    options = dict(arg.split("=", 1) for arg in sys.argv[3:])
    airline = options.get("airline", "HA")
    day = int(options.get("day", 59))
    column = options.get("column", "SaldoGesamt")

    runs_a = load(pattern_a, airline)
    runs_b = load(pattern_b, airline)
    common = sorted(j for j in runs_a.keys() & runs_b.keys() if day in runs_a[j].index and day in runs_b[j].index)
    if not common:
        print("No run index has day %d in both measurements." % day)
        sys.exit(1)

    a = pd.Series([runs_a[j].loc[day, column] for j in common], index=common, dtype=float)
    b = pd.Series([runs_b[j].loc[day, column] for j in common], index=common, dtype=float)
    diff = b - a
    n = len(common)

    # Pairing only helps if run j really is the same game in both. A computer player's first day is
    # decided before the bot has had much effect on it, so in a seeded measurement it matches in
    # nearly every pair whatever the two bots do - and between two unrelated games it never does
    # (day 0 would not tell: it only holds the starting values, identical in every game).
    witness = options.get("witness", "FL")
    witness_a = load(pattern_a, witness)
    witness_b = load(pattern_b, witness)
    first_day = 1
    same_start = sum(1 for j in common
                     if j in witness_a and j in witness_b and first_day in witness_a[j].index and first_day in witness_b[j].index
                     and witness_a[j].loc[first_day].equals(witness_b[j].loc[first_day]))

    se_paired = diff.std(ddof=1) / math.sqrt(n) if n > 1 else float("nan")
    se_unpaired = math.sqrt(a.var(ddof=1) / n + b.var(ddof=1) / n) if n > 1 else float("nan")
    delta = diff.mean()

    print("%s, %s, day %d: %d paired runs (%d only in A, %d only in B)" % (airline, column, day, n, len(runs_a) - n, len(runs_b) - n))
    print("  A: mean %.4e  median %.4e   (%s)" % (a.mean(), a.median(), pattern_a))
    print("  B: mean %.4e  median %.4e   (%s)" % (b.mean(), b.median(), pattern_b))
    print("  B - A: %+.4e  (%+.2f%%)" % (delta, 100.0 * delta / a.mean() if a.mean() else float("nan")))
    print("  paired:   se %.3e  t %+.2f" % (se_paired, delta / se_paired if se_paired > 0 else float("nan")))
    print("  unpaired: se %.3e  t %+.2f   (what the same data would say without pairing)" %
          (se_unpaired, delta / se_unpaired if se_unpaired > 0 else float("nan")))
    print("  B better in %d, worse in %d, identical in %d of %d games" % ((diff > 0).sum(), (diff < 0).sum(), (diff == 0).sum(), n))
    print("  correlation of the two measurements across games: %.3f" % a.corr(b))
    print("  %s's day-%d row identical in %d of %d pairs" % (witness, first_day, same_start, n))
    if same_start < 0.9 * n:
        print("  WARNING: most pairs start differently - were both measurements seeded with the same --seed-base?")


if __name__ == "__main__":
    main()
