Contact laws
=============

Contact resolution in Woo is divided into 3 mostly-orthogonal steps:

1. geometry computation, where local contact coordinates are updated and relative velocities and interpenetration are computed. These algorithms are detailed for different shape combinations in :ref:`the respective section <contact_geometry>`; they use data from particle shapes and create/update a :obj:`~woo.dem.CGeom` instance.

2. physical contact properties computation, which take data from :obj:`contacting materials <woo.dem.Material>` and create (usually this is done only when the contact is created the first time, but can be updated at request) a :obj:`~woo.dem.CPhys` instance.

3. contact resolution, which uses data from :obj:`~woo.dem.CGeom` and :obj:`~woo.dem.CPhys` to compute response of the contact  -- this usually entails force and torque, but other effect are possible.

These 3 steps are done by :obj:`~woo.dem.CGeomFunctor`, :obj:`~woo.dem.CPhysFunctor` and :obj:`~woo.dem.LawFunctor`, and the functors used are selected by dispatchers hidden inside the :obj:`~woo.dem.ContactLoop` engine. Moreover, since usually the :obj:`woo.dem.DemField.minimalEngines` is used in conjunction with :obj:`woo.models.ContactModelSelector` to build the engine sequence, the details might not be familiar even to seaseoned Woo users. Let's look at its output (with all defaults, i.e. using the linear contact model):

.. ipython::

   Woo [1]: import woo.dem
   
   Woo [1]: ee=woo.dem.DemField.minimalEngines()

   Woo [2]: loop=[e for e in ee if isinstance(e,woo.dem.ContactLoop)][0] # when inserted into scene, accessible as S.lab.contactLoop by virtue of Engine.label

   Woo [3]: loop.geoDisp.functors

   Woo [3]: loop.phyDisp.functors

   Woo [3]: loop.lawDisp.functors

We can see that functors are named with the ``Cg2``, ``Cp2`` and ``Law2`` prefix respectively (since they take 2 instances as arguments (2 :obj:`shapes <woo.dem.Shape>`, 2 :obj:`materials <woo.dem.Material>` and, finally the couple of :obj:`contact geometry <woo.dem.CGeom>` and :obj:`contact physics <woo.dem.CPhys>` produced by the first 2 functors); the name is further comprised of accepted types of the arguments (for ``Cp2`` functors, if it accepts both materials of the same type, it is writte out just once), and ends with the instance which is produced (except contact law, which does not produce an instance; a suffix defining the algorithm is added instead).

Thus to compute e.g. contact between :obj:`~woo.dem.Sphere` and :obj:`~woo.dem.Capsule` made of :obj:`~woo.dem.FrictMat`, we need

1. :obj:`~woo.dem.Cg2_Sphere_Capsule_L6Geom` which resolves the geometry, and produces/updates :obj:`~woo.dem.L6Geom`;
2. :obj:`~woo.dem.Cp2_FrictMat_FrictPhys` which computes physical contact parameters from :obj:`~woo.dem.FrictMat` and produces :obj:`~woo.dem.FrictPhys`;
3. :obj:`~woo.dem.Law2_L6Geom_FrictPhys_IdealElPl` which applies contact forces, consuming data from the previous steps (we could use any other ``Law2_L6Geom_FrictPhys_*`` class suitable for our task).

Note that types produced by the first two functors match types consumed by the last one; if that is not the case, the dispatch will fail and runtime error will occur.

Thus we can imitate :obj:`woo.dem.DemField.minimalEngines` by hand as follows::

   import woo.core
   from woo.dem import *
   S=woo.master.scene=woo.core.Scene(fields=[DemField()],
      engines=[
         Leapfrog(),
         InsertionSortCollider([Bo1_Sphere_Aabb(),Bo1_Capsule_Aabb(),Bo1_Wall_Aabb()]), # and so on
         ContactLoop(
            [Cg2_Sphere_Capsule_L6Geom(),Cg2_Sphere_Sphere_L6Geom()], # ..., as needed
            [Cp2_FrictMat_FrictPhys()],
            [Law2_L6Geom_FrictPhys_IdealElPl()]
         ),
         DynDt(stepPeriod=100)
      ]
   )

This knowledge is important so that we can write and test our new contact law properly.

New contact law
----------------

Assuming we can use already-existing ``Cg2`` and ``Cp2`` functors, writing a contact law is fairly easy, provided the mathematical formulation is ready. The :obj:`~woo.dem.L6Geom` class provides relative velocity and rotations at contact point, and normal interpenetration (computed -- usually -- non-incrementally), all expressed in local coordinates; ``L6`` means local-coordinates with 6 degrees of freedom (3 translations and 3 rotations).

This would be a simplified but working code for the linear-elastic and plastic law implemented in :obj:`woo.dem.Law2_L6Geom_FrictPhys_IdealElPl`:

.. code-block:: c++

   /* header pkg/dem/MyLaw.hpp */

   #pragma once
   #include<woo/pkg/dem/FrictMat.hpp>
   #include<woo/pkg/dem/L6Geom.hpp>
   #include<woo/pkg/dem/ContactLoop.hpp>

   struct Law2_L6Geom_FrictPhys_MyLaw: public LawFunctor{
      bool go(const shared_ptr<CGeom>&, const shared_ptr<CPhys>&, const shared_ptr<Contact>&) override;
      // declared types that the functor accepts
      FUNCTOR2D(L6Geom,FrictPhys);
      #define woo_dem_Law2_L6Geom_FrictPhys_MyLaw__CLASS_BASE_DOC_ATTRS \
         Law2_L6Geom_FrictPhys_MyLaw,LawFunctor,"Some documentation", \
         ((int,attr,0,,"Some attribute"))
      WOO_DECL__CLASS_BASE_DOC_ATTRS(woo_dem_Law2_L6Geom_FrictPhys_MyLaw__CLASS_BASE_DOC_ATTRS);
   };
   WOO_REGISTER_OBJECT(Law2_L6Geom_FrictPhys_MyLaw);

   /* implementation pkg/dem/MyLaw.cpp */

   #include<woo/pkg/dem/MyLaw.hpp>
   WOO_PLUGIN(dem,(Law2_L6Geom_FrictPhys_MyLaw));
   WOO_IMPL__CLASS_BASE_DOC_ATTRS(woo_dem_Law2_L6Geom_FrictPhys_MyLaw__CLASS_BASE_DOC_ATTRS);

   bool Law2_L6Geom_FrictPhys_MyLaw::go(const shared_ptr<CGeom>& cg, const shared_ptr<CPhys>& cp, const shared_ptr<Contact>& C){
      // shorhands
      const L6Geom& g(cg->cast<L6Geom>()); FrictPhys& ph(cp->cast<FrictPhys>()); 
      // break contact if there is separation between particles
      if(g.uN>0) return false; 
      // normal force
      ph.force[0]=pk.kn*g.uN;
      // work with y-z subvector of force as with a Vector2r
      Eigen::Map<Vector2r> Ft(&ph.force[1]);
      // compute trial tangential force, as increment from relative velocity
      Ft+=scene->dt*ph.kt*Vector2r(g.vel[1],g.vel[2]);
      // Coulomb slip: maximum tangential force norm
      Real maxFt=ph.force[0]*ph.tanPhi;
      if(Ft.squaredNorm()>pow(maxFt,2)){
         Ft*=maxFt/Ft.norm(); // work out floating-point corner-cases
      }
      // keep the contact
      return true;
   }

One can note that both contact geometry (interpenetration and mutual relative velocity at the contact point) and the response (force and torque) are conveniently expressed in contact-local coordinates, thus the normal-tangential separation is simply expressed by axes orthogonality. The force & torque is applied on contacting particles by :obj:`~woo.dem.ContactLoop` (for mono-monal particles, to be precise), it is something the contact law itself is not responsible for.

Energy tracking (which is highly recommended, as it makes it possible to test for energy conservation by the contact law) might make the code a bit more complicated, as elasic and plastic contributions need to be computed if desired. The reader is referred to the source of :obj:`woo.dem.Law2_L6Geom_FrictPhys_IdealElPl` for details. Energy computation should be avoided when not requested, hence the ``if(unlikely(scene->trackEnergy))`` conditionals around those parts of the code.

If the contact law is complex (e.g. entails iterative computation), it is advised to use timing services (:ref:`timing-deltas-timing`) for profiling the performance and determining bottlenecks.

New material class
""""""""""""""""""

More often than not, new contact law will need more information about the particle's materials.

In that case, one needs to define a new material class holding all parameters necessary for the model. This is not complicated in itself, but there are a few ingredients involved (see e.g. :woosrc:`pkg/dem/FrictMat.hpp`):

1. Defining the material class itself (in that case :obj:`woo.dem.FrictMat`);
2. Defining :obj:`woo.dem.CPhysFunctor` functor which can compute contact properties from those 2 materials (or combination with other materials) (:obj:`woo.dem.Cp2_FrictMat_FrictPhys`)
3. Defining :obj:`woo.dem.CPhys` which will hold results of the ``Cp2`` functor and pass them to the contact law itself.

Obviously one has to adjust the engine/functor sequence as exaplined above, so that the new classes are picked up and used by Woo.

.. note:: Often one can start the implementation with :obj:`~woo.dem.FrictPhys` and only add material parameters are attributes of the ``Law2`` functor, hard-coding them for first experimentation, and only later turn them into proper attributes of a new :obj:`~woo.dem.Material` subclass, which will be processed (averaged, whatever) by the new ``Cp2`` functor, to produce the new type of :obj:`~woo.dem.CPhys` required by the contact law.


Testing contact laws
--------------------

There are special tools to see whether the programmed behavior is identical to the one mathematically formulated. One of the most useful one is to prescribe mutual motion on particles (in contact-local coordinates) and record contact response (also in contact-local coordinates). For example, the :woosrc:`examples/law-test-concrete.py` script uses the following::

   LawTester(
      ids=(0,1),abWeight=.5,smooth=1e-4,stages=[
          # compress
          LawTesterStage(values=(-1e-4,0,0,0,0,0),whats='v.....',until='C.phys.sigmaN<-2e7'),
          # shear while not moving in the normal sense
          LawTesterStage(values=(0,1e-3,0,0,0,0),whats='vv....',until='C.phys.epsT.norm()>1e-2'),
          # unload in the normal sense; shear force should follow the plastic surface
          LawTesterStage(values=(1e-4,0,0,0,0,0),whats='v.....',until='not C'),
      ],
      done='S.stop()',label='tester'
   ),

where each :obj:`~woo.dem.LawTesterStage` defines loading stage using 6 :obj:`~woo.dem.LawTesterStage.values` (along 6 DoFs), which can be initial velocity, constant velocity, force or free (:obj:`~woo.dem.LawTesterStage.whats`) and terminating condition :obj:`~woo.dem.LawTesterStage.until` of the respective stage. You can study the full :woosrc:`examples/law-test-concrete.py` and other similar scripts to explore the functionality. Since the concrete law has a rather complex yield surface formulation, it is loaded in compression, then in shear, and unloaded in compression, with the following result:

.. figure:: fig/law-test-concrete.*
   :align: center

   Plot from loading 2-particle contact using the concrete contact law with :obj:`~woo.dem.LawTester` :obj:`stages <woo.dem.LawTesterStage>` as shown above.





