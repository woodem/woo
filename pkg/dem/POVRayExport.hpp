#pragma once

#include<woo/core/Engine.hpp>
#include<woo/pkg/dem/Particle.hpp>

struct POVRayExport: public PeriodicEngine{
	bool acceptsField(Field* f) override { return dynamic_cast<DemField*>(f); }
	void run() override;

	void exportParticle(std::ofstream& os, const shared_ptr<Particle>& p);
	string makeTexture(const shared_ptr<Particle>& p, const string& tex="");

	void writeParticleInc(const string& frameInc, bool doStatic);
	void writeMasterPov(const string& masterPov);

	WOO_DECL_LOGGER;

	#define woo_dem_POVRayExport__CLASS_BASE_DOC_ATTRS \
		POVRayExport,PeriodicEngine,"Export DEM simulation to POV-Ray input files (work in progress) for ray-tracing.", \
		((string,out,,AttrTrait<>().buttons({"Show last in POVRay","import subprocess, os.path; dir=os.path.dirname(self.out); subprocess.call(['povray','-F','+KFI%d'%self.nDone,'+KFI%d'%self.nDone,'+P',self.out+'_master.pov'],cwd=(dir if dir else '.'))",""},/*showBefore*/true),"Filename prefix to write into; :obj:`woo.core.Scene.tags` written as ``{tagName}`` are expanded at the first run.")) \
		((int,mask,0,,"If non-zero, only particles matching the mask will be exported.")) \
		((bool,skipInvisible,true,,"Skip invisible particles")) \
		((AlignedBox3r,clip,AlignedBox3r(),,"Only export particles of which first node is in the clip box (if given)."))  \
		((int,frameCounter,0,,"Counts exported frames and uses the number to name output files.")) \
		((shared_ptr<ScalarRange>,colorRange,make_shared<ScalarRange>(),,"Range to map :obj:`woo.dem.Shape.color` to RGB for pigment specification.")) \
		((Real,colorFuzz,.2,,"Relative fuzz for particle color; the texture is called with RGB colors mapped (via :obj:`colorRange`) from :obj:`Shape.color` - colorFuzz and :obj:`Shape.color` + colorFuzz, clamped to (0,1)).")) \
		((int,staticMask,DemField::defaultStaticBit,,"Mask for static particles, which will be exported only once into a special incude file separate from the per-frame include files.")) \
		((vector<int>,masks,vector<int>({DemField::defaultStaticBit,DemField::defaultOutletBit,DemField::defaultMovableBit}),,"Masks which will be tried one after another (first match counts) on each particle to determine which texture will be applied to it. Texture names are taken from :obj:`textures` and must be defined in the master file.")) \
		((vector<string>,textures,vector<string>({"static","outlet","movable"}),,"Texture names applied to particles matching :obj:`masks` -- the first matching mask counts, textures are named ``woo_tex_*`` and must be modified in the master file by the user (only an example definition is written by Woo). Particles not matching any mask will be assigned texture ``default`` (``woo_tex_default``). Each texture receives two rgb arguments which are colormapped from :obj:`woo.dem.Shape.color` using :obj:`colorRange` and :obj:`colorFuzz`. ")) \
		((string,cylCapTexture,"",,"If non-empty, :obj:`InfCylinder` objects will be drawn as several objects, with caps having this separate texture type.")) \
		((string,wallTexture,"",,"If non-empty, :obj:`Wall` objects will have this texture applied, regardless of their mask. Meant for easily assigning checkerboard texture to wall objects."))

	WOO_DECL__CLASS_BASE_DOC_ATTRS(woo_dem_POVRayExport__CLASS_BASE_DOC_ATTRS);
};
WOO_REGISTER_OBJECT(POVRayExport);
