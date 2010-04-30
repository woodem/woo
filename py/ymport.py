"""
Import geometry from various formats ('import' is python keyword, hence the name 'ymport').
"""

from yade.wrapper import *
from miniWm3Wrap import *
from yade import utils


def text(fileName,shift=[0.0,0.0,0.0],scale=1.0,**kw):
	"""Load sphere coordinates from file, create spheres, insert them to the simulation.
	filename is the file which has 4 colums [x, y, z, radius].
	All remaining arguments are passed the the yade.utils.sphere function which creates bodies.
	Lines starting with # are skipped
	"""
	infile = open(fileName,"r")
	lines = infile.readlines()
	infile.close()
	ret=[]
	for line in lines:
		data = line.split()
		if (data[0][0] == "#"): continue
		ret.append(utils.sphere([shift[0]+scale*float(data[0]),shift[1]+scale*float(data[1]),shift[2]+scale*float(data[2])],scale*float(data[3]),**kw))
	return ret

def stl(file, dynamic=False,wire=True,color=None,highlight=False,noBound=False,material=-1):
	""" Import geometry from stl file, return list of created facets."""
	imp = STLImporter()
	facets=imp.ymport(file)
	for b in facets:
		b.dynamic=dynamic
		b.shape.postProcessAttributes(True)
		b.shape.diffuseColor=color if color else utils.randomColor()
		b.shape.wire=wire
		b.shape.highlight=highlight
		utils._commonBodySetup(b,0,Vector3(0,0,0),noBound=noBound,material=material,resetState=False)
	return facets

def gts(meshfile,shift=(0,0,0),scale=1.0,**kw):
	""" Read given meshfile in gts format, apply scale and shift (in this order); return list of corresponding facets. **kw is passed to :yref:`utils.facet`."""
	import gts,yade.pack
	surf=gts.read(open(meshfile))
	surf.scale(scale)
	surf.translate(shift) 
	yade.pack.gtsSurface2Facets(surf,**kw)

def gmsh(meshfile="file.mesh",shift=[0.0,0.0,0.0],scale=1.0,orientation=Quaternion().IDENTITY,**kw):
	""" Imports geometry from mesh file and creates facets.
	shift[X,Y,Z] parameter moves the specimen.
	scale factor scales the given data.
	orientation quaternion: orientation of the imported mesh
	
	Remaining **kw arguments are passed to utils.facet; 
	mesh files can be easily created with `GMSH <http://www.geuz.org/gmsh/>`_.
	Example added to :ysrc:`scripts/test/regular-sphere-pack.py`
	
	Additional examples of mesh-files can be downloaded from 
	http://www-roc.inria.fr/gamma/download/download.php
	"""
	infile = open(meshfile,"r")
	lines = infile.readlines()
	infile.close()

	nodelistVector3=[]
	findVerticesString=0
	
	while (lines[findVerticesString].split()[0]<>'Vertices'): #Find the string with the number of Vertices
		findVerticesString+=1
	findVerticesString+=1
	numNodes = int(lines[findVerticesString].split()[0])
	
	for i in range(numNodes):
		nodelistVector3.append(Vector3(0.0,0.0,0.0))
	id = 0
	
	qTemp = Quaternion(Vector3(orientation[0],orientation[1],orientation[2]),orientation[3])
	for line in lines[findVerticesString+1:numNodes+findVerticesString+1]:
		data = line.split()
		tempNodeVector=Vector3(float(data[0])*scale,float(data[1])*scale,float(data[2])*scale)
		tempNodeVector=qTemp.Rotate(tempNodeVector)
		tempNodeVector+=Vector3(shift[0],shift[1],shift[2])
		nodelistVector3[id] = tempNodeVector
		id += 1
	numTriangles = int(lines[numNodes+findVerticesString+2].split()[0])
	triList = []
	for i in range(numTriangles):
		triList.append([0,0,0,0])
	
	tid = 0
	for line in lines[numNodes+findVerticesString+3:numNodes+findVerticesString+3+numTriangles]:
		data = line.split()
		id1 = int(data[0])-1
		id2 = int(data[1])-1
		id3 = int(data[2])-1
		triList[tid][0] = tid
		triList[tid][1] = id1
		triList[tid][2] = id2
		triList[tid][3] = id3
		tid += 1
		ret=[]
	for i in triList:
		a=nodelistVector3[i[1]]
		b=nodelistVector3[i[2]]
		c=nodelistVector3[i[3]]
		ret.append(utils.facet((nodelistVector3[i[1]],nodelistVector3[i[2]],nodelistVector3[i[3]]),**kw))
	return ret

def gengeoFile(fileName="file.geo",shift=[0.0,0.0,0.0],scale=1.0,**kw):
	""" Imports geometry from LSMGenGeo .geo file and creates spheres.
	shift[X,Y,Z] parameter moves the specimen.
	scale factor scales the given data.
	Remaining **kw arguments are passed to :yref:`yade.utils.sphere`; 
	
	LSMGenGeo library allows to create pack of spheres
	with given [Rmin:Rmax] with null stress inside the specimen.
	Can be useful for Mining Rock simulation.
	
	Example: :ysrc:`scripts/test/regular-sphere-pack.py`, usage of LSMGenGeo library in :ysrc:`scripts/test/genCylLSM.py`.
	
	* https://answers.launchpad.net/esys-particle/+faq/877
	* http://www.access.edu.au/lsmgengeo_python_doc/current/pythonapi/html/GenGeo-module.html
	* https://svn.esscc.uq.edu.au/svn/esys3/lsm/contrib/LSMGenGeo/"""
	from yade.utils import sphere

	infile = open(fileName,"r")
	lines = infile.readlines()
	infile.close()

	numSpheres = int(lines[6].split()[0])
	ret=[]
	for line in lines[7:numSpheres+7]:
		data = line.split()
		ret.append(utils.sphere([shift[0]+scale*float(data[0]),shift[1]+scale*float(data[1]),shift[2]+scale*float(data[2])],scale*float(data[3]),**kw))
	return ret

def gengeo(mntable,shift=Vector3().ZERO,scale=1.0,**kw):
	""" Imports geometry from LSMGenGeo library and creates spheres.
	shift[X,Y,Z] parameter moves the specimen.
	Remaining **kw arguments are passed to :yref:`yade.utils.sphere`; 
	
	LSMGenGeo library allows to create pack of spheres
	with given [Rmin:Rmax] with null stress inside the specimen.
	Can be useful for Mining Rock simulation.
	
	Example: :ysrc:`scripts/test/regular-sphere-pack.py`, usage of LSMGenGeo library in :ysrc:`scripts/test/genCylLSM.py`.
	
	* https://answers.launchpad.net/esys-particle/+faq/877
	* http://www.access.edu.au/lsmgengeo_python_doc/current/pythonapi/html/GenGeo-module.html
	* https://svn.esscc.uq.edu.au/svn/esys3/lsm/contrib/LSMGenGeo/"""
	try:
		from GenGeo import MNTable3D,Sphere
	except ImportError:
		from gengeo import MNTable3D,Sphere
	ret=[]
	sphereList=mntable.getSphereListFromGroup(0)
	for i in range(0, len(sphereList)):
		r=sphereList[i].Radius()
		c=sphereList[i].Centre()
		ret.append(utils.sphere([shift[0]+scale*float(c.X()),shift[1]+scale*float(c.Y()),shift[2]+scale*float(c.Z())],scale*float(r),**kw))
	return ret
