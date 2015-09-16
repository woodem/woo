from woo.core import *
from woo.dem import *
import woo.triangulated
from minieigen import *

cs=Node(pos=(1,1,1),ori=Quaternion((0,1,1),.5))
S=woo.master.scene=Scene(fields=[DemField()])
nan=float('nan')
S.dem.par.add(
    woo.triangulated.sweep2d([Vector2(0,0),Vector2(1,0),Vector2(1,1),Vector2(0,1),Vector2(nan,0),None,Vector2(2,0),Vector2(2,1),Vector2(3,1),Vector2(3,0),Vector2(nan,-3),None,Vector2(4,0),Vector2(5,0),Vector2(4,1),None,Vector2(4,0),Vector2(4,1),Vector2(5,0)],node=cs,zz=(0,.5),halfThick=.1)
)
