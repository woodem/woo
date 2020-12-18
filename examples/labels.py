import woo.core, woo.dem
from minieigen import *
S=woo.core.Scene()
lm=S.labels
o1,o2,o3=woo.core.Object(),woo.core.Engine(),woo.core.Scene()
lm['o1']=o1
lm['o2']=o2
lm['o3']=o3
# assign from sequence of woo.Object's
lm['oo']=[o1,o2]
lm['oo[2]']=o3
print((lm['o1'],lm['o2'],lm['o3']))
print(lm['oo'])

lm['p1']=23423
lm['p2']=[Vector3(1,2,3),'absedfr',Vector3(1,2,3)]

# create empty seq
lm['pp[1]']='def'
lm['pp[0]']='abcd'
lm['pp[0]']='abd'

print((lm['p1'],lm['p2']))
print(lm['pp'])

print(dict(lm))
print(list(lm['oo']))
lm2=lm.deepcopy()
print(dict(lm))
print(list(lm['oo']))

print(S.lab.o1)
print(S.lab.oo)
S.lab.qq=[1,2,3]
print(S.labels['qq'])
del S.lab.p1
print(('p1' in S.labels))
