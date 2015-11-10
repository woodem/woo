# compatibility patches

# this was only added later in minieigen and 14.04LTS package still does not have it
import minieigen, operator
if not hasattr(minieigen.Vector3,'prod'):
	for t in minieigen.Vector2,minieigen.Vector3,minieigen.Vector6,minieigen.VectorX:
		t.prod=lambda v: reduce(operator.mul,list(v),1.)
