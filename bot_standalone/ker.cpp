#include <cmath>
#include <iostream>
#include <utility>

using SLONG = long int;
using DOUBLE = double;

struct Player {
    SLONG Tank;
    SLONG TankInhalt;
    DOUBLE KerosinQuali;
};
Player qPlayer;

DOUBLE HoleKerosinPreis(int type) {
    DOUBLE preis = 337;
    if (type == 0) {
        return preis * 2;
    }
    if (type == 1) {
        return preis;
    }
    if (type == 2) {
        return preis / 2;
    }
    return 0;
}

std::pair<SLONG, SLONG> kerosineQualiOptimization(SLONG moneyAvailable) {
    DOUBLE qualiZiel = 4.0 / 3.0;
    DOUBLE qualiStart = qPlayer.KerosinQuali;
    DOUBLE amountGood = 0;
    DOUBLE amountBad = 0;
    DOUBLE priceGood = HoleKerosinPreis(1);
    DOUBLE priceBad = HoleKerosinPreis(2);
    DOUBLE tankContent = qPlayer.TankInhalt;
    DOUBLE tankMax = qPlayer.Tank;

    // Definitions:
    // aG := amountGood, aB := amountBad, mA := moneyAvailable, pG := priceGood, pB := priceBaD, T := qPlayer.Tank, Ti := qPlayer.TankInhalt, qS := qualiStart,
    // qZ := qualiZiel Given are the following two equations: I: aG*pG + aB*pB = mA    (spend all money for either good are bad kerosine) II: qZ = (Ti*qS + aB*2
    // + aG) / (Ti + aB + aG)   (new quality qZ depends on amounts bought) Solve I for aG: aG = (mA - aB*pB) / pG Solve II for aB: qZ*(Ti + aB + aG) = Ti*qS +
    // aB*2 + aG; qZ*Ti + qZ*(aB + aG) = Ti*qS + aB*2 + aG; qZ*Ti - Ti*qS = aB*2 + aG - qZ*(aB + aG); Ti*(qZ - qS) = aB*(2 - qZ) + aG*(1 - qZ); Insert I in II
    // to eliminate aG Ti*(qZ - qS) = aB*(2 - qZ) + ((mA - aB*pB) / pG)*(1 - qZ); Ti*(qZ - qS) = aB*(2 - qZ) + mA*(1 - qZ) / pG - aB*pB*(1 - qZ) / pG; Ti*(qZ -
    // qS) - mA*(1 - qZ) / pG = aB*((2 - qZ) - pB*(1 - qZ) / pG); (Ti*(qZ - qS) - mA*(1 - qZ) / pG) / ((2 - qZ) - pB*(1 - qZ) / pG) = aB;

    DOUBLE nominator = (tankContent * (qualiZiel - qualiStart) - moneyAvailable * (1 - qualiZiel) / priceGood);
    DOUBLE denominator = ((2 - qualiZiel) - priceBad * (1 - qualiZiel) / priceGood);

    // Limit:
    amountBad = std::max(0.0, nominator / denominator);
    amountGood = (moneyAvailable - amountBad * priceBad) / priceGood;
    if (amountGood < 0) {
        amountGood = 0;
        amountBad = moneyAvailable / priceBad;
    }

    // Round:
    std::pair<SLONG, SLONG> res;
    res.first = static_cast<SLONG>(std::floor(amountGood));
    res.second = static_cast<SLONG>(std::floor(amountBad));

    // we have more than enough money to fill the tank, calculate again using this for equation I:
    // I: aG = (T - Ti - aB)  (cannot exceed tank capacity)
    // Insert I in II
    // Ti*(qZ - qS) = aB*(2 - qZ) + (T - Ti - aB)*(1 - qZ);
    // Ti*(qZ - qS) = aB*(2 - qZ) + (T - Ti)*(1 - qZ) - aB*(1 - qZ);
    // Ti*(qZ - qS) - (T - Ti)*(1 - qZ) = aB*(2 - qZ) - aB*(1 - qZ);
    // Ti*(qZ - qS) - (T - Ti)*(1 - qZ) = aB*((2 - qZ) - (1 - qZ));
    // (Ti*(qZ - qS) - (T - Ti)*(1 - qZ)) / ((2 - qZ) - (1 - qZ)) = aB;
    if (res.first + res.second + tankContent > tankMax) {
        DOUBLE nominator = (tankContent * (qualiZiel - qualiStart) - (tankMax - tankContent) * (1 - qualiZiel));
        DOUBLE denominator = ((2 - qualiZiel) - (1 - qualiZiel));

        // Limit:
        amountBad = std::min(tankMax - tankContent, std::max(0.0, nominator / denominator));
        amountGood = (tankMax - tankContent - amountBad);

        // Round:
        res.first = static_cast<SLONG>(std::floor(amountGood));
        res.second = static_cast<SLONG>(std::floor(amountBad));
    }

    return res;
}

int main(void) {
    for (int i = 1; i <= 30; i++) {
        qPlayer.Tank = 1000;
        qPlayer.TankInhalt = 210;
        qPlayer.KerosinQuali = 1.6;

        SLONG money = 10000 * i;
        auto res = kerosineQualiOptimization(money);
        std::cout << "Good: " << res.first << ", bad: " << res.second;

        auto InhaltAlt = qPlayer.TankInhalt;
        qPlayer.TankInhalt += res.first + res.second;
        qPlayer.KerosinQuali = (InhaltAlt * qPlayer.KerosinQuali + res.second * 2 + res.first) / (qPlayer.TankInhalt);
        money -= HoleKerosinPreis(1) * res.first;
        money -= HoleKerosinPreis(2) * res.second;

        std::cout << " (Fuellstand: " << (DOUBLE)qPlayer.TankInhalt / qPlayer.Tank << ", Quali: " << qPlayer.KerosinQuali << ", Geld: " << money << ")"
                  << std::endl;
    }
}
