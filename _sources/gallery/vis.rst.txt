.. _gallery-visualization:

Visualization
=============

Woo has a built-in 3d view made for detailed inspection of simulations and seeing what matters. It supportes displacement & rotation scaling, selecting & inspecting objects and many other things.

Force chains
-------------

These are force chains during the cylindrical triaxial test, with scaled (magnified) nodal displacements:

.. image:: fig/triax-force-chains-scaled.png


.. youtube:: o4cS1f4-CEI


Particle traces
---------------

With configurable coloring, length and export to VTK if needed:

.. youtube:: xBCk1-0ZTbM


.. youtube:: nQD8sojqNy0



Flow analysis
--------------

:obj:`woo.dem.FlowAnalysis` is useful e.g. for visualizing average flow rates, which can make important features of the processes really stand out:

.. image:: fig/flow-average-3d.png

This is the average flow for the screed wall of which video is shown below:

.. image:: fig/flow-screed-wall.png


POV-Ray export
---------------

:obj:`woo.dem.POVRayExport` harnesses the power of the `POV-Ray <http://povray.org>`__ raytraycer to visualize simulations with advanced texture, lighting and camera options. This movies uses focal blur for realistic blur and bump-mapping for rough surface appearance. The logo itself is a 3d object placed in the scene.

.. youtube:: p1VxvvNXDQc

In this detail video, particles are colored by diameter:

.. youtube:: 66N2ltj2sFk


Paraview movies
---------------

While Woo itself concentrates on being a superb simulation tool, the possibility to :obj:`export to Paraview <woo.dem.VtkExport>` allows to create beautiful renderings of the scene, which is useful not just for presenting the results to the management, but also as an important aid for understanding the underlying physical processes:

.. youtube:: jXL8qXi780M

Screed wall simulation (flow analysis image is above):

.. youtube:: a1D1Ia9WwDY
