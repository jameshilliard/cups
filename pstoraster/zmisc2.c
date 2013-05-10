/* Copyright (C) 1992, 1995, 1996, 1997, 1998 Aladdin Enterprises.  All rights reserved.
  
  This file is part of GNU Ghostscript.
  
  GNU Ghostscript is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY.  No author or distributor accepts responsibility
  to anyone for the consequences of using it or for whether it serves any
  particular purpose or works at all, unless he says so in writing.  Refer
  to the GNU General Public License for full details.
  
  Everyone is granted permission to copy, modify and redistribute GNU
  Ghostscript, but only under the conditions described in the GNU General
  Public License.  A copy of this license is supposed to have been given
  to you along with GNU Ghostscript so you can know your rights and
  responsibilities.  It should be in a file named COPYING.  Among other
  things, the copyright notice and this notice must be preserved on all
  copies.
  
  Aladdin Enterprises supports the work of the GNU Project, but is not
  affiliated with the Free Software Foundation or the GNU Project.  GNU
  Ghostscript, as distributed by Aladdin Enterprises, does not require any
  GNU software to build or run it.
*/

/*$Id$ */
/* Miscellaneous Level 2 operators */
#include "memory_.h"
#include "string_.h"
#include "ghost.h"
#include "oper.h"
#include "estack.h"
#include "idict.h"
#include "idparam.h"
#include "iparam.h"
#include "dstack.h"
#include "ilevel.h"
#include "iname.h"
#include "iutil2.h"
#include "ivmspace.h"
#include "store.h"

/* Forward references */
private int set_language_level(P1(int));

/* ------ Language level operators ------ */

/* - .languagelevel <int> */
private int
zlanguagelevel(register os_ptr op)
{
    push(1);
    ref_assign(op, &ref_language_level);
    return 0;
}

/* <int> .setlanguagelevel - */
private int
zsetlanguagelevel(register os_ptr op)
{
    int code = 0;

    check_type(*op, t_integer);
    if (op->value.intval != ref_language_level.value.intval) {
	code = set_language_level((int)op->value.intval);
	if (code < 0)
	    return code;
    }
    pop(1);
    ref_assign_old(NULL, &ref_language_level, op, "setlanguagelevel");
    return code;
}

/* ------ The 'where' hack ------ */

private int
z2where(register os_ptr op)
{	/*
	 * Aldus Freehand versions 2.x check for the presence of the
	 * setcolor operator, and if it is missing, substitute a procedure.
	 * Unfortunately, the procedure takes different parameters from
	 * the operator.  As a result, files produced by this application
	 * cause an error if the setcolor operator is actually defined
	 * and 'bind' is ever used.  Aldus fixed this bug in Freehand 3.0,
	 * but there are a lot of files created by the older versions
	 * still floating around.  Therefore, at Adobe's suggestion,
	 * we implement the following dreadful hack in the 'where' operator:
	 *      If the key is /setcolor, and
	 *        there is a dictionary named FreeHandDict, and
	 *        currentdict is that dictionary,
	 *      then "where" consults only that dictionary and not any other
	 *        dictionaries on the dictionary stack.
	 */
    ref rkns, rfh;
    const ref *pdref = dsp;
    ref *pvalue;

    if (!r_has_type(op, t_name) ||
	(name_string_ref(op, &rkns), r_size(&rkns)) != 8 ||
	memcmp(rkns.value.bytes, "setcolor", 8) != 0 ||
	name_ref((const byte *)"FreeHandDict", 12, &rfh, -1) < 0 ||
	(pvalue = dict_find_name(&rfh)) == 0 ||
	!obj_eq(pvalue, pdref)
	)
	return zwhere(op);
    check_dict_read(*pdref);
    if (dict_find(pdref, op, &pvalue) > 0) {
	ref_assign(op, pdref);
	push(1);
	make_true(op);
    } else
	make_false(op);
    return 0;
}

/* ------ Initialization procedure ------ */

/* The level setting ops are recognized even in Level 1 mode. */
const op_def zmisc2_op_defs[] =
{
    {"0.languagelevel", zlanguagelevel},
    {"1.setlanguagelevel", zsetlanguagelevel},
		/* The rest of the operators are defined only in Level 2. */
    op_def_begin_level2(),
    /* Note that this overrides the definition in zdict.c. */
    {"1where", z2where},
    op_def_end(0)
};

/* ------ Internal procedures ------ */

/*
 * Adjust the interpreter for a change in language level.
 * This is used for the .setlanguagelevel operator,
 * and (perhaps someday) after a restore.
 */
private int swap_level_dict(P1(const char *dict_name));
private int swap_entry(P3(ref elt[2], ref * pdict, ref * pdict2));
private int
set_language_level(int new_level)
{
    int old_level = ref_language_level.value.intval;
    ref *pgdict =		/* globaldict, if present */
    ref_stack_index(&d_stack, ref_stack_count(&d_stack) - 2);
    ref *level2dict;
    int code = 0;

    if (new_level < 1 ||
	new_level >
	(dict_find_string(systemdict, "ll3dict", &level2dict) > 0 ? 3 : 2)
	)
	return_error(e_rangecheck);
    if (dict_find_string(systemdict, "level2dict", &level2dict) <= 0)
	return_error(e_undefined);
    /*
     * As noted in dstack.h, we allocate the extra d-stack entry for
     * globaldict even in Level 1 mode; in Level 1 mode, this entry
     * holds an extra copy of systemdict, and [count]dictstack omit the
     * very bottommost entry.
     */
    while (new_level != old_level) {
	switch (old_level) {
	    case 1: {		/* 1 => 2 or 3 */
				/* Put globaldict in the dictionary stack. */
		ref *pdict;

		/*
		 * This might be called so early in initialization that
		 * globaldict hasn't been defined yet.  If so, just skip
		 * this step.
		 */
		code = dict_find_string(level2dict, "globaldict", &pdict);
		if (code > 0) {
		    if (!r_has_type(pdict, t_dictionary))
			return_error(e_typecheck);
		    *pgdict = *pdict;
		}
		/* Set other flags for Level 2 operation. */
		dict_auto_expand = true;
		}
		code = swap_level_dict("level2dict");
		if (code < 0)
		    return code;
		++old_level;
		continue;
	    case 3:		/* 3 => 1 or 2 */
		code = swap_level_dict("ll3dict");
		if (code < 0)
		    return code;
		--old_level;
		continue;
	    default:		/* 2 => 1 or 3 */
		break;
	}
	switch (new_level) {
	    case 1: {		/* 2 => 1 */
		/*
		 * Clear the cached definition pointers of all names defined
		 * in globaldict.  This will slow down future lookups, but
		 * we don't care.
		 */
		int index = dict_first(pgdict);
		ref elt[2];

		while ((index = dict_next(pgdict, index, &elt[0])) >= 0)
		    if (r_has_type(&elt[0], t_name))
			name_invalidate_value_cache(&elt[0]);
		/* Overwrite globaldict in the dictionary stack. */
		*pgdict = *systemdict;
		/* Set other flags for Level 1 operation. */
		dict_auto_expand = false;
		}
		code = swap_level_dict("level2dict");
		break;
	    case 3:		/* 2 => 3 */
		code = swap_level_dict("ll3dict");
		break;
	    default:		/* not possible */
		return_error(e_Fatal);
	}
	break;
    }
    dict_set_top();		/* reload dict stack cache */
    return code;
}

/*
 * Swap the contents of a level dictionary (level2dict or ll3dict) and
 * systemdict.  If a value in the level dictionary is itself a dictionary,
 * and it contains a key/value pair referring to itself, swap its contents
 * with the contents of the same dictionary in systemdict.  (This is a hack
 * to swap the contents of statusdict.)
 */
private int
swap_level_dict(const char *dict_name)
{
    ref *leveldict;
    int index;
    ref elt[2];			/* key, value */
    ref *subdict;

    if (dict_find_string(systemdict, dict_name, &leveldict) <= 0)
	return_error(e_undefined);
    index = dict_first(leveldict);
    while ((index = dict_next(leveldict, index, &elt[0])) >= 0)
	if (r_has_type(&elt[1], t_dictionary) &&
	    dict_find(&elt[1], &elt[0], &subdict) > 0 &&
	    obj_eq(&elt[1], subdict)
	    ) {
	    /* elt[1] is the 2nd-level sub-dictionary */
	    int isub = dict_first(&elt[1]);
	    ref subelt[2];
	    int found = dict_find(systemdict, &elt[0], &subdict);

	    if (found <= 0)
		continue;
	    while ((isub = dict_next(&elt[1], isub, &subelt[0])) >= 0)
		if (!obj_eq(&subelt[0], &elt[0])) {
		    /* don't swap dict itself */
		    int code = swap_entry(subelt, subdict, &elt[1]);

		    if (code < 0)
			return code;
		}
	} else {
	    int code = swap_entry(elt, systemdict, leveldict);

	    if (code < 0)
		return code;
	}
    return 0;
}

/*
 * Swap an entry from a higher level dictionary into a base dictionary.
 * elt[0] is the key, elt[1] is the current value in the Level 2 dictionary
 * (*pdict2).
 */
private int
swap_entry(ref elt[2], ref * pdict, ref * pdict2)
{
    ref *pvalue;
    ref old_value;		/* current value in *pdict */
    int found = dict_find(pdict, &elt[0], &pvalue);

    switch (found) {
	default:		/* <0, error */
	    return found;
	case 0:		/* missing */
	    make_null(&old_value);
	    break;
	case 1:		/* present */
	    old_value = *pvalue;
    }
    /*
     * Temporarily flag the dictionaries as local, so that we don't
     * get invalidaccess errors.  (We know that they are both
     * referenced from systemdict, so they are allowed to reference
     * local objects even if they are global.)
     */
    {
	uint space2 = r_space(pdict2);
	int code;

	r_set_space(pdict2, avm_local);
	dict_put(pdict2, &elt[0], &old_value);
	if (r_has_type(&elt[1], t_null)) {
	    code = dict_undef(pdict, &elt[0]);
	    if (code == e_undefined &&
		r_has_type(&old_value, t_null)
		)
		code = 0;
	} else {
	    uint space = r_space(pdict);

	    r_set_space(pdict, avm_local);
	    code = dict_put(pdict, &elt[0], &elt[1]);
	    r_set_space(pdict, space);
	}
	r_set_space(pdict2, space2);
	return code;
    }
}