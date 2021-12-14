Performance issues
===================



Timing
-------

Timing services can be used to determine bottlenecks of a simulation, for the purposes of changing its setup, or improving the c++ code. Woo provides two services to measure time spent in different parts of the code. The first one is on per-engine level, the other on arbitrary fine c++-source level (requiring re-compilation).

.. _engine-timing:

Engine timing
^^^^^^^^^^^^^^

The coarser timing works by merely accumulating numebr of invocations and time (with the precision of the ``clock_gettime`` function) spent in each engine, which can be then post-processed by associated Python module :obj:`woo.timing`. There is the bool variable :obj:`woo.master.timingEnabled <woo.core.Master.timingEnabled>` controlling whether such measurements take place (disabled by default). If the script sets this flag to ``True``::

   woo.master.timingEnabled=True

then the data can be reported in aggregated form by :obj:`woo.timing.stats` and reset to zero using :obj:`woo.timing.reset`. 

.. note:: For legacy reasons, this timing functions operate on the master scene only, which should not be an issue in normal circumstances.

Let's see the output from the :obj:`~woo.pre.horse.FallingHorse` preprocessor::

   Woo [1]: import woo.pre.horse, woo.timing
   Woo [2]: S=woo.master.scene=woo.pre.horse.FallingHorse()()
   Woo [3]: woo.master.timingEnabled=True
   Woo [4]: S.run(500,wait=True)
   Woo [5]: import woo.timing
   Woo [6]: woo.timing.stats()

   Name                                                  Count            Time [ms]        Rel. time [%]
   -----------------------------------------------------------------------------------------------------
   "leapfrog"                                        500                831.3                21.85      
   "collider"                                  (175) 500               1478.2                38.86      
   "contactLoop"                                     500                839.1                22.06      
   "dynDt"                                             5                  6.7                 0.18      
   PyRunner                                           50                 13.0                 0.34      
   PyRunner                                           10                  1.8                 0.05      
   BoxOutlet                                           5                  0.3                 0.01      
   VtkExport                                          12                595.4                15.65      
   "flowAnalysis"                                     13                 25.8                 0.68      
   "tracer"                                           25                 12.7                 0.33      
   TOTAL                                                               3804.2               100.00      

Interpretation of the table is straight-forward. Total running time was 3.8s, out of which ≈20% was motion integration (labels are shown for engines which do have a label, and are thus accessible as e.g. ``S.lab.leapfrog``), ≈40% collision detection, ≈20% contact resolution, ≈15% export to VTK and then negligible amount of time for the rest. The number ``(175)`` for collider signifies how many times it did the full run (as opposed only to checking that it does not have to run in that step).


.. _timing-deltas-timing:

In-engine and in-functor timing
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. note:: Refer also to `Yade documentation on timing <https://yade-dem.org/doc/prog.html#timing>`__ for documentation of the older interface; those documents could still be useful despite changes.

The ``woo::TimingDeltas`` class is responsible for tracking hi-resolution timing information; it can handle parallel execution of code, which is often the case with functors, which are run in parallel. There is no locking involved in parallel execution, thus the impact of taking time measurements should be small. This type of analysis requires the respective section of code to be recompiled, as this type of timing is not enabled in production code and is used for developing only.

.. note:: Even when compiled-in, also this detailed timing framework will be active at runtime only when :obj:`woo.master.timingEnabled <woo.core.Master.timingEnabled>` is set, as explained above.

It is advisable to guard the timing calls by locally-defined macros, so that it can be enabled-disabled with a single change in the source code. Checkpoints are identified by a number (often ``__LINE__`` is used for that, as it is unique as long as the timed code stays within one single file, but any other number is permissible) and also has a human-readable name which appears in the output.

Every :obj:`~woo.core.Engine` and :obj:`~woo.core.Functor` contains the ``timingDeltas`` member, but it is by default unallocated. Thus it should be conditionally allocated when the engine is called. 

This is your hypothetical header:

.. code-block:: c++

   #define MYENGINE_TIMING // comment this definition to disable timing altogether
   #ifdef MYENGINE_TIMING
      #define MYENGINE_CHECKPOINT(name) timingDeltas->checkpoint(__LINE__,name)
   #else
      #define MYENGINE_CHECKPOINT(name) // expand to nothing unless MYENGINE_TIMING is defined
   #else

   struct MyEngine: public Engine{
      void run() override;
      // ...
   };
   WOO_REGISTER_OBJECT(MyEngine);

And this would be the implementation file:

.. code-block:: c++

   void MyEngine::run(){
      #ifdef MYENGINE_TIMING
         if(!timingDeltas) timingDeltas=make_shared<TimingDeltas>();
         timingDeltas->start();
      #endif
      // some code
      MYENGINE_CHECKPOINT("step-1")
      // some code
      MYENGINE_CHECKPOINT("step-2")
      // some code
      MYENGINE_CHECKPOINT("finito")
   }


For functors, allocating ``timingDeltas`` is mandatorily done in the constructor, since the very first call might be parallel already, leading to race conditions (this could be done for engines as well, but it is not necessary); we are omiting the marco-guard here for simplicity:

.. code-block:: c++

   struct Law2_L6Geom_FrictPhys_MyLaw: public LawFunctor{
      bool go(const shared_ptr<CGeom>&, const shared_ptr<CPhys>&, const shared_ptr<Contact>&) override;
      FUNCTOR2D(L6Geom,FrictPhys);
      #define woo_dem_Law2_L6Geom_FrictPhys_MyLaw__CLASS_BASE_DOC_ATTRS_CTOR \
         Law2_L6Geom_FrictPhys_MyLaw,LawFunctor,"Some documentation" \
         ((int,someVar,-1,,"Document someVar")) \
         ,/*ctor*/ timingDeltas=make_shared<TimingDeltas>(); /* <--- this constructs the TimingDeltas */

      WOO_DECL__CLASS_BASe_DOC_ATTRS_CTOR(woo_dem_Law2_L6Geom_FrictPhys_MyLaw__CLASS_BASE_DOC_ATTRS_CTOR);
   };
   WOO_REGISTER_OBJECT(Law2_L6Geom_FrictPhys_MyLaw);

   /*
   meanwhile in the .cpp file:
   */

   bool Law2_L6Geom_FrictPhys_IdealElPl::go(const shared_ptr<CGeom>& cg, const shared_ptr<CPhys>& cp, const shared_ptr<Contact>& C){
      timingDeltas->start(); // important to check time at the very beginning!
      /* ... */
      timingDeltas->checkpoint(__LINE__,"init");
      /* ... */
      timingDeltas->checkpoint(__LINE__,"finito");
   }


