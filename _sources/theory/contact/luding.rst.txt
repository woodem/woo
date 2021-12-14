.. _luding-contact-model:

Luding contact model
====================



This contact model, published in :cite:`Luding2008`, is suitable for flexible handling and testing of various adhesion/plasticity/viscosity parameters.

This model separately treats 4 contact senses, each including mostly-decoupled elastic-plastic-viscous behavior.

1. *Normal* sense, where compressive plasticity during loading increases contact stiffness (up to value defined by plasticity limit), unloading/reloading following elastic path. Adhesion effects are proportional to historically maximum compression, thus suitable for modeling stickiness induced by prior compression (e.g. during impact). Further energy is dissipated by viscosity.

2. *Tangential* sense, with Coulomb-style friction with different limits for static/dynamic friction (related via a dimensionless parameter) and viscosity. This sense is 2-dimensional, perpendicular to the normal sense.

3. *Rolling* sense, describing rotation along the same 2 axes as tangential sliding/slipping, with Coulomb-style friction and viscosity.

4. *Twisting* sense, describing rotation along the normal axis, with Coulomb-style friciton and viscosity.

The model is designed in such a way that all tangential, rolling and twisting senses can use the same routine, only with different parameters and different dimensionality: 2d for tangential and rolling, 1d for twisting).

Formulation
-----------

The contact exists if there is geometrical overlap (:math:`\delta\geq0`) and is dissolved for :math:`\delta<0`.

Normal
""""""

Elastic stiffness :math:`k_{n2}` varies based on the historically maximum normal overlap :math:`\delta_{\rm max}` and is interpolated between :math:`k_{n1}` (for :math:`\delta_{\rm max}=0`) and :math:`\hat{k}_{n2}` (for :math:`\delta_{\rm max}\geq\delta_{\rm lim}`). This is shown in :numref:`fig-luding-normal`. Algebraically,

.. math:: f_n=\underbrace{\gamma_n v_n}_{\hbox{viscosity}}+\begin{cases}k_{n1}\delta & k_{n2}(\delta-\delta_0)\geq k_{n1}\delta\quad\text{(plastic loading)} \\ -k_{nc}\delta & -k_{nc}\delta\geq k_{n2}(\delta-\delta_0)\quad\text{(adhesive)} \\ k2(\delta-\delta_0)&\text{otherwise (in-between unloading/reloading)} \end{cases}

with variable stiffness

.. math:: k_{n2}(\delta_{\rm max})&=\begin{cases} \hat{k}_{n2} & \delta_{\rm max}\geq\delta_{\rm lim} \\ k_{n1}+(\hat{k}_{2}-k_{n1})\frac{\delta_{\rm max}}{\delta_{\rm lim}} & \delta_{\rm max}<\delta_{\rm lim} \end{cases}
   :label: eq-luding-kn2

and the plastic limit overlap (constat for each contact) being defined as

.. math:: \delta_{\rm lim}=\frac{\hat{k}_{n2}}{\hat{k}_{n2}-k_{n1}}\phi_f a_{12}.

where :math:`a_1`, :math:`a_2` are equivalent radii of contacting particles and

.. math:: a_{12}=\frac{2a_1 a_2}{a_1+a_2}

is "reduced radius" of the contact.


.. _fig-luding-normal:
.. figure:: fig/luding-normal.*

   Diagram showing contact behavior in the normal sense: plastic loading with stiffness :math:`k_{n1}`, elastic unloading/reloading with stiffness :math:`k_{n2}(\delta_{\rm max})`, adhesion with :math:`k_{na}` stiffness.



Dissipation (work) has two components:

1. viscous contribution, computed incrementally as 

   .. math:: \Delta W_{nv}=\gamma_n v_n^2 \Dt,

2. plastic contribution, which is the easiest to compute in a total manner when the contact breaks, using :math:`\delta_{\rm max}` as history variable and current elastic stiffness :math:`k_{n2}\equiv k_{n2}(\delta_{\rm max})`; we compute :math:`\delta_0` (elastically unloaded zero-force overlap) as auxiliary, and :math:`\delta_{\rm min}` to evaluate adhesive contribution (see :numref:`fig-luding-normal` which shows the work shaded):

   .. math::

      \delta_0&=\delta_{\rm max}\left(1-\frac{k_{n1}}{k_{n2}}\right)

      -k_{na}\delta_{\rm min}&=-(\delta_0-\delta_{\rm min})k_{n2}

      \delta_{\min}&=\delta_0\frac{k_{n2}}{k_{n2}+k_{na}}

      W_{np}&=(k_{n1}\delta_{\rm max}+k_{na}\delta_{\rm min})\delta_0


Rescaled damping coefficient, typical contact duration and restitution coefficient can be respectively computed as

.. math::


   \eta_0&=\frac{\gamma_n}{2m_{12}},

   t_c&=\frac{\pi}{\omega}=\frac{\pi}{\sqrt{k_n/m_{12}-\eta_0^2}},

   r&=v_n'/v_n=\exp(-\pi \eta_0/\omega)=\exp(-\eta_0 t_c)

with reduced mass :math:`m_{12}=m_1 m_2 /(m_1+m_2)`.

..
   # compute contact time and restitution coefficient
   from math import *
   d=12e-3
   l1=l2=.5*d
   V=(4/3)*pi*(d/2)**3
   rho=4200 #kg/m3
   m1=m2=rho*V
   m12=m1*m2/(m1+m2)
   A=pi*(d/2)**2 # contact surface
   E=1e6
   k=1/(1/(E*A/l1)+1/(E*A/l2))
   gamma0=2.5 # kg/s, gammaN
   eta0=gamma0/(2*m12)
   print(eta0)
   omega=sqrt(k/m12-eta0**2)
   print(omega)
   tc=pi/omega
   r=exp(-eta0*tc)
   print('Contact time tc=%g s'%tc)
   print('Restitution coefficient r=%g'%r)



Tangential
""""""""""


Sliding (yield) force is defined in dependence on the normal force (including adhesion) as

.. math:: f_t^y=\tan\phi_t(f_n+k_{nc}\delta),

trial force (2d vector in tangential plane) being computed from tangential stiffness :math:`k_t`, elastic displacement :math:`\vec{\xi}_t` (2d vector), viscous coefficient :math:`\gamma_t` and relative velocity at contact :math:`\vec{v}_t` (2d vector) as

.. math:: \vec{f}_t^t=-k_t\pprev{\vec{\xi}}_t-\gamma_t\vec{v}_t

where tangential stiffness is defined as :math:`k_{t}=\hat{k}_{n2}\frac{k_t}{k_n}`.

1. If :math:`|\vec{f}_t|\leq f_y^y`, static friction is active (zero plastic dissipation) and

   .. math::
   
      \vec{f}_t&=\vec{f}_t^t,
      
      \nnext{\vec{\xi}}_t&=\pprev{\vec{\xi}}_t+\vec{v}_t\Dt.


2. Otherwise, dynamic friction is activated as

   .. math::

      \vec{f}_t&=\frac{\vec{f}_t^t}{|\vec{f}_t^t|}f_t^y\phi_d,

      \nnext{\vec{\xi}}_t&=-\frac{1}{k_t}\left(\vec{f}_t+\gamma_t\vec{v}_t\right).

   where :math:`\phi_d` is the dynamic/static friction coefficients ratio. Plastic dissipation (work) is equal to

   .. math::

      \Delta W_{tp}=\frac{1}{2}(|\vec{f}_t^t|+f_t^y\phi_d)\frac{1}{k_t}(|\vec{f}_t^t|-f_t^y\phi_d).

Viscous dissipation is equal to

.. math:: \Delta W_{tv}=\gamma_v|\vec{v}_t|^2\Dt.

Rolling
""""""""

Rolling and twisting are computed in terms of torques (rather than forces) but the same equations apply; reduced radius :math:`a_{12}` is used as length to convert dimensionalities of forces/torques.

Rolling stiffness is :math:`k_r=\hat{k}_{n2}\frac{k_r}{k_n}a_{12}`, yield torque :math:`m_r^y=\tan\phi_r(f_n+k_{nc}\delta)a_{12}`. Elastic rotation :math:`\vec{\xi}_r` is initially zero, the contact is damped using viscous coefficient :math:`\gamma_r`. Plastic and viscous work (:math:`\Delta W_{rp}`, :math:`\Delta W_{rv}`) are computed the same as in the tangential sense, with indices replaced.

Twisting
"""""""""

Twisting is identical to rolling, only by demoting 2d-vectors to 1d-vectors (scalars) with indices replaced, i.e. twisting stiffness :math:`k_w=\hat{k}_{n2}\frac{k_w}{k_n}a_{12}`, yield torque :math:`m_w^y=\tan\phi_w(f_n+k_{nc}\delta)a_{12}`, elastic twist :math:`\xi_w`, viscous coefficient :math:`\gamma_w`. Work contributions are :math:`\Delta W_{wp}`, :math:`\Delta W_{wv}`.


Parameters
-----------

The following table summarizes all model parameters:

.. csv-table::
   :header: ,normal,tangent,rolling,twisting

   stiffness,:math:`\hat{k}_{n2}`,:math:`k_t/k_n`,:math:`(k_r/k_n)a_{12}`,:math:`(k_w/k_n)a_{12}`
   normal plasticity,:math:`\frac{k_{n1}}{\hat{k}_{n2}}` :math:`\delta_{\rm lim}`,--,--,--
   adhesion,:math:`k_{nc}/k_n`,--,--,--
   static friction,--,:math:`\tan\phi_t`,:math:`\tan\phi_r`,:math:`\tan\phi_w`
   dynamic friction,--,:math:`\phi_d`,:math:`\phi_d`,:math:`\phi_d`
   viscous coefficient,:math:`\gamma_n`,:math:`\gamma_t`,:math:`\gamma_r`,:math:`\gamma_w`

This table summarizes corresponding attributes in the WooDEM code. Since properties of every comtact are computed from contacting materials, these values are marked as * for averaging, † for minimum and # for averaging with scaling by :math:`a_{12}`:

.. csv-table::
   :header: ,normal,tangent,rolling,twisting

   stiffness,:obj:`~woo.dem.LudingPhys.k2hat`,:obj:`~woo.dem.FrictMat.ktDivKn` *,:obj:`~woo.dem.LudingMat.krDivKn` #,:obj:`~woo.dem.LudingMat.kwDivKn` #
   normal plasticity, :obj:`~woo.dem.LudingMat.k1DivKn` * :obj:`~woo.dem.LudingMat.deltaLimRel` *,--,--,--
   adhesion,:obj:`~woo.dem.LudingMat.kaDivKn` *,--,--,--
   static friction,--,:obj:`~woo.dem.FrictMat.tanPhi` †,:obj:`~woo.dem.LudingMat.statR` †,:obj:`~woo.dem.LudingMat.statW` †
   dynamic friciton,--,:obj:`~woo.dem.LudingMat.dynDivStat` †,:obj:`~woo.dem.LudingMat.dynDivStat` †,:obj:`~woo.dem.LudingMat.dynDivStat` †
   viscous coefficient,:obj:`~woo.dem.LudingMat.viscN` †,:obj:`~woo.dem.LudingMat.viscT` †,:obj:`~woo.dem.LudingMat.viscR` †,:obj:`~woo.dem.LudingMat.viscW` †


