from past.builtins import execfile
execfile('basic-1.py')
S.engines=S.engines+[PyRunner(5,'myAddPlotData(S)')]
# define function which will be called by the PyRunner above
def myAddPlotData(S):
   uf=woo.utils.unbalancedForce(S)
   S.plot.addData(t=S.time,i=S.step,z1=S.dem.par[1].pos[1],Ek=S.dem.par[1].Ek,unbalanced=uf)
## hack to make this possible when generating docs
__builtins__['myAddPlotData']=myAddPlotData
S.plot.plots={'t':('z1',None,('Ek','r.-')),'i':('unbalanced',)}
S.throttle=0
S.run(2000,True)
S.plot.saveDataTxt('basic-1.data.txt') # for the example below
S.plot.plot()