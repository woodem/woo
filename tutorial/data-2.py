from past.builtins import execfile
execfile('basic-1.py')
S.plot.plots={'t=S.time': ('$z_1$=S.dem.par[1].pos[2]','$z_0$=S.dem.par[0].pos[2]',None,'$E_k$=S.dem.par[1].Ek')}
S.engines=S.engines+[PyRunner(10,'S.plot.autoData()')]
S.throttle=0
S.run(2000,True)
S.plot.plot()