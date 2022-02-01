#include"ContactHook.hpp"
#include"Particle.hpp"
#include"Contact.hpp"

WOO_PLUGIN(dem,(ContactHook)(CountContactsHook));

WOO_IMPL__CLASS_BASE_DOC_ATTRS(woo_dem_ContactHook__CLASS_BASE_DOC_ATTRS);
WOO_IMPL__CLASS_BASE_DOC_ATTRS(woo_dem_CountContactsHook__CLASS_BASE_DOC_ATTRS);

void CountContactsHook::hookNew(DemField& dem, const shared_ptr<Contact>& C){ nNew+=1; }
void CountContactsHook::hookDel(DemField& dem, const shared_ptr<Contact>& C){ nDel+=1; }

