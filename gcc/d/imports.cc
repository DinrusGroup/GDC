/* imports.cc -- Build imported modules/declarations.
   Copyright (C) 2011-2017 Free Software Foundation, Inc.

GCC is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 3, or (at your option)
any later version.

GCC is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GCC; see the file COPYING3.  If not see
<http://www.gnu.org/licenses/>.  */

#include "config.h"
#include "system.h"
#include "coretypes.h"

#include "dfrontend/aggregate.h"
#include "dfrontend/arraytypes.h"
#include "dfrontend/declaration.h"
#include "dfrontend/enum.h"
#include "dfrontend/module.h"

#include "tree.h"
#include "stringpool.h"

#include "d-tree.h"
#include "d-objfile.h"

/* Implements the visitor interface to build debug trees for all
   module and import declarations, where ISYM holds the cached
   backend representation to be returned.  */
class ImportVisitor : public Visitor
{
  /* Build the declaration DECL as an imported symbol.  */
  tree make_import (tree decl)
  {
    tree import = make_node (IMPORTED_DECL);
    TREE_TYPE (import) = void_type_node;

    gcc_assert (decl != NULL_TREE);
    IMPORTED_DECL_ASSOCIATED_DECL (import) = decl;
    d_keep (import);

    return import;
  }

public:
  ImportVisitor (void) {}

  /* This should be overridden by each symbol class.  */
  void visit (Dsymbol *)
  {
    gcc_unreachable ();
  }

  /* Build the module decl for M, this is considered toplevel, regardless
     of whether there are any parent packages in the module system.  */
  void visit (Module *m)
  {
    m->isym = build_decl (UNKNOWN_LOCATION, NAMESPACE_DECL,
			  get_identifier (m->toPrettyChars ()),
			  void_type_node);
    d_keep (m->isym);

    Loc loc = (m->md != NULL) ? m->md->loc
      : Loc (m->srcfile->toChars (), 1, 0);
    set_decl_location (m->isym, loc);

    if (!m->isRoot ())
      DECL_EXTERNAL (m->isym) = 1;

    TREE_PUBLIC (m->isym) = 1;
    DECL_CONTEXT (m->isym) = NULL_TREE;
  }

  /* Build an import for any kind of user defined type.
     Use the TYPE_DECL associated with the type symbol.  */
  void visit (EnumDeclaration *d)
  {
    tree type = build_ctype (d->type);
    /* Not all kinds of D enums create a TYPE_DECL.  */
    if (TREE_CODE (type) == ENUMERAL_TYPE)
      d->isym = this->make_import (TYPE_STUB_DECL (type));
  }

  void visit (AggregateDeclaration *d)
  {
    tree type = build_ctype (d->type);
    d->isym = this->make_import (TYPE_STUB_DECL (type));
  }

  void visit (ClassDeclaration *d)
  {
    /* Want the RECORD_TYPE, not POINTER_TYPE.  */
    tree type = TREE_TYPE (build_ctype (d->type));
    d->isym = this->make_import (TYPE_STUB_DECL (type));
  }

  /* For now, ignore importing other kinds of dsymbols.  */
  void visit (ScopeDsymbol *)
  {
  }

  /* Alias symbols aren't imported, but their targets are.  */
  void visit (AliasDeclaration *d)
  {
    Dsymbol *dsym = d->toAlias ();

    if (dsym == d)
      {
	Type *type = d->getType ();

	/* Type imports should really be part of their own visit method.  */
	if (type != NULL)
	  {
	    if (type->ty == Tenum)
	      dsym = ((TypeEnum *) type)->sym;
	    else if (type->ty == Tstruct)
	      dsym = ((TypeStruct *) type)->sym;
	    else if (type->ty == Tclass)
	      dsym = ((TypeClass *) type)->sym;
	  }
      }

    /* This symbol is really an alias for another, visit the other.  */
    if (dsym != d)
      {
	dsym->accept (this);
	d->isym = dsym->isym;
      }
  }

  /* Skip over importing templates and tuples.  */
  void visit (TemplateDeclaration *)
  {
  }

  void visit (TupleDeclaration *)
  {
  }

  /* Import any other kind of declaration.  If the class does not implement
     symbol generation routines, the compiler will throw an error.  */
  void visit (Declaration *d)
  {
    d->isym = this->make_import (get_symbol_decl (d));
  }
};


/* Build a declaration for the symbol D that can be used for the
   debug_hook imported_module_or_decl.  */
tree
build_import_decl (Dsymbol *d)
{
  if (!d->isym)
    {
      ImportVisitor v;
      d->accept (&v);
    }

  /* Not all visitors set 'isym'.  */
  return d->isym ? d->isym : NULL_TREE;
}

