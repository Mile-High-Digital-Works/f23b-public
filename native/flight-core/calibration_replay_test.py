# SPDX-License-Identifier: MIT
"""Regression: repeated recovery labels must not join unrelated manoeuvres."""
import unittest
import numpy as np
from calibration_replay import short_replay

class Driver:
    def step(self,y,e,u,dt):
        return np.r_[dt,np.zeros(27)]

class Segments(unittest.TestCase):
    def test_repeated_names_require_bounds(self):
        d={k:np.zeros(6) for k in ('pitch_deg','bank_deg','hdg_deg','vx','vy','vz','alt_ft','wx','wy','wz')}
        d['t']=np.arange(6.)
        d['step']=np.array(['recover','recover','roll','roll','recover','recover'])
        source={'d':d,'a':np.zeros((6,31))}
        with self.assertRaisesRegex(ValueError,'Repeated segment'):
            short_replay(Driver(),source,'recover')
        first=short_replay(Driver(),source,'recover',(0,2))
        second=short_replay(Driver(),source,'recover',(4,6))
        self.assertEqual(list(first[:,0]),[0,1,2])  # Includes the closing boundary only.
        self.assertEqual(list(second[:,0]),[0,1])

if __name__=='__main__':unittest.main()
