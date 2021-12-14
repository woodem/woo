Smooth triangulation edge contacts
===================================

Motivation
-----------

In in interaction of rather compliant particles with triangulated surface, internal edges of the surface can influence the simulation. :numref:`fig-facet-surface-sphere-contacts` shows a simple case where such artefacts can be encoutered.

.. _fig-facet-surface-sphere-contacts:
.. figure:: fig/facet-surface-sphere-contacts.*

   Illustration of a sphere rolling accross triangulated surface with velocity :math:`v`; at this moment, there are two contacts with facets representing the surface. The facet :math:`f_1` is contacted on the face; however, facet :math:`f_2` is contacted on edge, which does not have physical meaning (it is inner edge which only representes triangulation of the continuous surface). Both contacts cause force responses (noted :math:`\vec{F}_1`, :math:`\vec{F}_2`); there is also gravity force acting on the sphere :math:`\vec{F}_g`; as we can see, resultant force on the sphere :math:`\vec{F}_{\Sigma}` is obstructing its movement (against :math:`\vec{v}`) thus inducing artifacts into simulation.

Edge smoothing
---------------

To counter this effect, contacts of particles with internal edges (and with internal vertices) are regularized in such way that the contact force is always within the range of facet's both normals. (An edge is considered internal if facet shares both nodes of that respective edge with another single facet; a vertex is considered internal if both adjacent edges are internal.) Normals were parallel in :numref:`fig-facet-surface-sphere-contacts`, but in general case of a curved surface, they are not; such a case is shown in :numref:`fig-smooth-facet-edge-contact`. 

.. _fig-smooth-facet-edge-contact:
.. figure:: fig/smooth-facet-edge-contact.*

   Edge contact smoothing based on 3 zones which are determined from :math:`\vec{n}\cdot\vec{n}_1`, projection of original contact normal to the facet normal.

The original contact normal :math:`\vec{n}` will have be adjusted to the new normal :math:`\vec{n}'` so that motion parallel to the surface is not obstructed. The relationship between :math:`\vec{n}_1` and :math:`\vec{n}_2` (which is constant for non-deforming meshes) is stored in the scalar :math:`n_{\rm lim}=\vec{n}_1\cdot\vec{n}_2` which determines boundary values for 3 zones of adjustment.

1. The blue zone is when contact normal :math:`\vec{n}` is between :math:`\vec{n}_1` and :math:`\vec{n}_2`, i.e. when 

   .. math:: n_{\rm lim}\leq\vec{n}\cdot\vec{n}_1.

   In this case, there is no need for adjustment, thus :math:`\vec{n}\equiv\vec{n}'`.

2. The red zone is convex contact with the edge; the condition is 

   .. math:: -o_{\rm lim}<\vec{n}\cdot\vec{n}_1<n_{\rm lim}

   with the perpendicular component computed simply as

   .. math:: o_{\rm lim}=\sqrt{1-n_{\rm lim}^2}.

   In this case, the adjusted normal will be :math:`\vec{n}'=\vec{n}_2` and the contact point will be contacting particle's center projected onto plane defined by :math:`\mathbf{E}_{12}` and normal :math:`\vec{n}_2`.

3. The green zone is concave contact with the edge; the condition is

   .. math:: \vec{n}\cdot\vec{n}_1\leq-o_{\rm lim},

   the adjusted normal becomes :math:`\vec{n}'=-\vec{n}_1` and contact point is particle's center projected onto plane defined by :math:`\mathbf{E}_{12}` and normal :math:`\vec{n}_1`.

.. note:: In general case, the computation somewhat more complicated due to the fact that (a) normals :math:`\vec{n}_1`, :math:`\vec{n}_2` may not be oriented consistently and (b) both cases of convex and concave edge must be handled, from both sides of possible contact.

