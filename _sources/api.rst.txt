API Changes
============

Framework
----------

Woo keeps track of current API version in :obj:`woo.master.api <woo.core.Master.api>`, which is set from the internal :woosrc:`woo.apiversion <py/apiversion.py>` module.

Every script should state the API it was programmed for by setting :obj:`woo.master.usesApi <woo.core.Master.usesApi>`::

   woo.master.usesApi=10177 # or (1,1,77) which is converted 10000*1+100*1+77

If :obj:`~woo.core.Master.usesApi` is smaller than :obj:`~woo.core.Master.api`, functions which changed their behavior in significant ways (other than fixing bugs) will issue a warning about possibly unexpected result. In that case, this page can be consulted to find out about was to update the code accordingly.

Changes
--------

This page keeps track of major (and some minor) API changes for the purposes of updating the code and also for warning when older API is being used, possibly with different results.


API 10104
"""""""""

.. note:: This API change was commited Oct 3, 2017

* Deprecate ``woo.dem.Cp2_FrictMat_HertzPhys`` and introduce ``woo.dem.HertzMat`` and ``woo.dem.Cp2_HertzMat_HertzPhys``. This means that surface energy ``surfEnergy`` and the Carpick-Ogletree-Solmeron ``alpha`` parameter are passed through :obj:`woo.dem.HertzMat` and can be different for different material, whereas previously they had to be set per-simulation. :obj:`woo.models.ContactModelSelector` will raise an exception when its ``alpha`` and ``surfEnergy`` are assigned, since that menas the user is not aware of this change. 

  Migration to the new API means using :obj:`woo.dem.HertzMat` (instead of :obj:`woo.dem.FrictMat`), assigning :obj:`woo.dem.HertzMat.surfEnergy` and :obj:`woo.dem.HertzMat.alpha` to the material instance (not to the ``woo.dem.Cp2_FrictMat_HertzPhys`` functor) and replacing ``woo.dem.Cp2_FrictMat_HertzPhys`` with :obj:`woo.dem.Cp2_HertzMat_HertzPhys`.

  If :obj:`woo.models.ContactModelSelector` is used, material types have to be changed but new functors will be returned automatically.

API 10103
"""""""""

.. note:: This API change was commited Oct 23, 2015.

* Remove ``woo.dem.Bo1_Sphere_Aabb.distFactor``, ``woo.dem.Cg2_Sphere_Sphere_L6Geom.distFactor``, ``woo.dem.Grid1_Sphere.distFactor``; accessing thsoe from Python (reading or writing) will raise ``ValueError`` immediately. These values were replaced by :obj:`DemField.distFactor <woo.dem.DemField.distFactor>`, which is read from affected functors directly. This change ensures consistency of :obj:`~woo.dem.DemField.distFactor` accross all functors and makes it easier to change everywhere.

  A related change is that :obj:`woo.models.ContactModelSelector.distFactor` is *not* set automatically in engines returned from :obj:`woo.dem.DemField.minimalEngines`; this must be set by hand in :obj:`woo.dem.DemField.distFactor`. A warning will be issued when API is smaller than 10103 and :obj:`~woo.models.ContactModelSelector.distFactor` is set.


API 10102
"""""""""

.. note:: This API change was commited Aug 5, 2015.

* All "static" classes (i.e. classes with static attributes declared with ``WOO_CLASS_BASE_DOC_STATICATTRS`` and similar) were converted to regular classes with per-instance attributes. It was done for architectural reasons and cleaner interfaces (e.g. saving rendering options along with the simulation). Some backwards-incompatible changes had to be introduced:

  * :obj:`woo.dem.Tracer` must be instantiated as any other engine, and multiple independent instances may exist. Since :obj:`~woo.dem.Tracer` was mostly used from the UI (where the internals were adjusted), there are no issues expected.

  * All rendering classes and functors (:obj:`woo.gl.Renderer`, :obj:`woo.gl.Gl1_DemField` and all others) were converted to regular classes. Instances of rendering classes are contained in :obj:`woo.gl.GlSetup` attached to :obj:`S.gl <woo.core.Scene.gl>`. ``S.gl`` is automatically created (with defaults) on-demand if not assigned explicitly. :obj:`woo.gl.GlSetup` can be instantiated with arbitrary OpenGL functors passed as argument (shown below), and functors are accessed as in ``S.gl.cPhys`` (for :obj:`woo.gl.Gl1_CPhys`: leading ``Gl1`` removed and capital converted to lowercase). ``S.gl`` also defines the ``__call__`` operator, which accepts any GL functor instances, which will replace the ones already existing.

  * Assigning static attributes will not report any error, but will not do what you expect::

      try: # try-block for running in headless mode
         import woo.gl
         woo.gl.Renderer.scaleOn=True  # creates new class attribute scaleOn, no effect on rendering
         woo.gl.Gl1_Sphere.quality=5
      except ImportError: pass

      # TODO: show the exception being raised

    and should be replaced with
    
    1. setup of the whole rendering through :obj:`~woo.gl.GlSetup` from scratch::

         try:
            import woo.gl
            S.gl=GlSetup(woo.gl.Renderer(scaleOn=True),woo.gl.Gl1_Sphere(quality=5))
         except ImportError: pass

    2. replacing only some *functors* and keeping the rest of :obj:`~woo.gl.GlSetup` intact::

         S.gl(woo.gl.Renderer(scaleOn=True),woo.gl.Gl1_Sphere(quality=5))

    3. keeping everything (functors *and* :obj:`~woo.gl.GlSetup`) and only change individual parameters::

         S.gl.renderer.scaleOn=True
         S.gl.sphere.quality=5

  * Construction (which used to result in assignment of static attributes) will have no effect, as the object will be immediately discarded::

      woo.gl.Gl1_DemField(shape='spheroids',colorBy='radius')

    and should be again replaced by using :obj:`~woo.gl.GlSetup`::

        S.gl=GlSetup(woo.gl.Gl1_DemField(shape='spheroids',colorBy='radius'))
        ## or
        S.gl.demField.shape='spheroids'
        S.gl.demField.colorBy='radius'

  * Warning will be issued when :obj:`~woo.gl.Renderer` or any GL functor is construted with arguments (such as ``Gl1_Sphere(quality=4)``, but not for ``Gl1_Sphere()``).

* ``Scene.any``, which used to be a list of arbitrary :obj:`objects <woo.core.Object>` was removed; it was mostly used for storing OpenGL renderers with the simulation (which is no longer necessary) and is functionally superseded by labeled objects in :obj:`S.lab <woo.core.Scene.lab>`. Since ``Scene.any`` was only sparsely documented, it was removed directly and an exception will be raised when accessed.


API 10101
"""""""""

.. note:: This API change was commited Jul 28, 2015.

* :obj:`woo.dem.ParticleContainer.add` (as in :obj:`S.dem.par.add <woo.dem.ParticleContainer.add>` newly may add nodes of particles to :obj:`S.dem.nodes <woo.core.Field.nodes>`, depending on the optional argument ``nodes``, which is ``-1`` by default. The default (-1) will add the node if it has mass, or prescribed velocity, angular velocity, or imposition; 0 will not add nodes, 1 will add all nodes. If any node is in :obj:`S.dem.nodes <woo.core.Field.nodes>`, it is silently skipped.
  
  This change makes :obj:`woo.dem.DemField.collectNodes` largely obsolete, since nodes from particles are collected at the time they are added.
  
  When script uses an older API, warning is issued for every use of :obj:`woo.dem.DemField.collectNodes`.

* Introduced API versioning

