from past.builtins import execfile
execfile('basic-1.py')                                  # re-use the simple setup for brevity
S.plot.plots={'S.time': ('S.dem.par[1].pos[2]',)}       # define what to plot
S.engines=S.engines+[PyRunner(10,'S.plot.autoData()')]  # add the engine
S.throttle=0                                            # run at full speed now
S.run(2000,True)                                        # run 2000 steps
S.plot.plot()                                           # show the plot