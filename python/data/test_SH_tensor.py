import numpy as np

def getSHCoeff():

    shCoeff = np.array([0.7, 0, 0, -0.5, 0, 0, 0, 0, 0, 0.7, 0, 0, -0.5, 0, 0, 0, 0, 0, 0.7, 0, 0, -0.5, 0, 0, 0, 0, 0])
    shCoeff = shCoeff.reshape([1, 27])
    return shCoeff 