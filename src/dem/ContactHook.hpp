#pragma once

#include"../supp/base/openmp-accu.hpp"
#include"../supp/object/Object.hpp"
#include"Particle.hpp"

struct Contact;
struct DemField;

struct ContactHook: public Object{
	bool isMatch(int maskA, int maskB){ return mask==0 || ((maskA&mask) && (maskB&mask)); }
	virtual void hookNew(DemField& dem, const shared_ptr<Contact>& C){};
	virtual void hookDel(DemField& dem, const shared_ptr<Contact>& C){};
	#define woo_dem_ContactHook__CLASS_BASE_DOC_ATTRS ContactHook,Object,"Functor called from :obj:`~woo.dem.ContactLoop` for some specific events on contacts; currently, these events are contact creation and contact deletion. This base class does nothing.", \
		((int,mask,0,,"Mask which must be matched by *both* particles in the contact."))
	WOO_DECL__CLASS_BASE_DOC_ATTRS(woo_dem_ContactHook__CLASS_BASE_DOC_ATTRS);
};
WOO_REGISTER_OBJECT(ContactHook);

struct CountContactsHook: public ContactHook{
	void hookNew(DemField& dem, const shared_ptr<Contact>& C) override;
	void hookDel(DemField& dem, const shared_ptr<Contact>& C) override;
	#define woo_dem_CountContactsHook__CLASS_BASE_DOC_ATTRS CountContactsHook,ContactHook,"Functor counting contacts created and deleted in :obj:`ContactLoop` (contacts being deleted e.g. as a result of particle deletion are not counted), restricted by mask.", \
		((OpenMPAccumulator<int>,nNew,,,"Total number of new (real) contacts.")) \
		((OpenMPAccumulator<int>,nDel,,,"Total number of deleted contacts."))
	WOO_DECL__CLASS_BASE_DOC_ATTRS(woo_dem_CountContactsHook__CLASS_BASE_DOC_ATTRS);
};
WOO_REGISTER_OBJECT(CountContactsHook);
