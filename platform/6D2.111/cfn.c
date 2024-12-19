#include <dryos.h>
#include <property.h>
#include <cfn-generic.h>

// look on camera menu or review sites to get custom function numbers

// These are not CFn on 6D2 (see 5D3 cfn.c)
GENERIC_GET_ALO
GENERIC_GET_HTP
GENERIC_GET_MLU
GENERIC_SET_MLU

// These two are in CFn menu 3, but there's no number given
// for each item, it's graphical.  So, I'm unsure.
int cfn_get_af_button_assignment() { return GetCFnData(3, 1); }
void cfn_set_af_button(int value) { SetCFnData(3, 1, value); }

int get_af_star_swap() { return GetCFnData(3, 2); }
void set_af_star_swap(int value) { SetCFnData(3, 2, value); }
