// Copyright (C) 2020-2025 Free Software Foundation, Inc.

// This file is part of GCC.

// GCC is free software; you can redistribute it and/or modify it under
// the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 3, or (at your option) any later
// version.

// GCC is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
// for more details.

// You should have received a copy of the GNU General Public License
// along with GCC; see the file COPYING3.  If not see
// <http://www.gnu.org/licenses/>.

#include "rust-derive-clone.h"
#include "rust-item.h"

namespace Rust {
namespace AST {

std::unique_ptr<Expr>
DeriveClone::clone_call (std::unique_ptr<Expr> &&to_clone)
{
  // $crate::core::clone::Clone::clone for the fully qualified path - we don't
  // link with `core` yet so that might be an issue. Use `Clone::clone` for now?
  // TODO: Factor this function inside the DeriveAccumulator
  auto path = std::unique_ptr<Expr> (
    new PathInExpression (builder.path_in_expression ({"Clone", "clone"})));

  auto args = std::vector<std::unique_ptr<Expr>> ();
  args.emplace_back (std::move (to_clone));

  return builder.call (std::move (path), std::move (args));
}

/**
 * Create the actual "clone" function of the implementation, so
 *
 * fn clone(&self) -> Self { <clone_expr> }
 *
 */
std::unique_ptr<AssociatedItem>
DeriveClone::clone_fn (std::unique_ptr<Expr> &&clone_expr)
{
  auto block = std::unique_ptr<BlockExpr> (
    new BlockExpr ({}, std::move (clone_expr), {}, {}, AST::LoopLabel::error (),
		   loc, loc));
  auto big_self_type = builder.single_type_path ("Self");

  std::unique_ptr<SelfParam> self (new SelfParam (Lifetime::error (),
						  /* is_mut */ false, loc));

  std::vector<std::unique_ptr<Param>> params;
  params.push_back (std::move (self));

  return std::unique_ptr<AssociatedItem> (
    new Function ({"clone"}, builder.fn_qualifiers (), /* generics */ {},
		  /* function params */ std::move (params),
		  std::move (big_self_type), WhereClause::create_empty (),
		  std::move (block), Visibility::create_private (), {}, loc));
}

/**
 * Create the Clone trait implementation for a type
 *
 * impl Clone for <type> {
 *     <clone_fn>
 * }
 *
 */
std::unique_ptr<Item>
DeriveClone::clone_impl (
  std::unique_ptr<AssociatedItem> &&clone_fn, std::string name,
  const std::vector<std::unique_ptr<GenericParam>> &type_generics)
{
  // should that be `$crate::core::clone::Clone` instead?
  auto segments = std::vector<std::unique_ptr<TypePathSegment>> ();
  segments.emplace_back (builder.type_path_segment ("Clone"));
  auto clone = TypePath (std::move (segments), loc);

  auto trait_items = std::vector<std::unique_ptr<AssociatedItem>> ();
  trait_items.emplace_back (std::move (clone_fn));

  // we need to build up the generics for this impl block which will be just a
  // clone of the types specified ones
  //
  // for example:
  //
  // #[derive(Clone)]
  // struct Be<T: Clone> { ... }
  //
  // we need to generate the impl block:
  //
  // impl<T: Clone> Clone for Be<T>

  std::vector<Lifetime> lifetime_args;
  std::vector<GenericArg> generic_args;
  std::vector<std::unique_ptr<GenericParam>> impl_generics;
  for (const auto &generic : type_generics)
    {
      switch (generic->get_kind ())
	{
	  case GenericParam::Kind::Lifetime: {
	    LifetimeParam &lifetime_param = (LifetimeParam &) *generic.get ();

	    Lifetime l = builder.new_lifetime (lifetime_param.get_lifetime ());
	    lifetime_args.push_back (std::move (l));

	    auto impl_lifetime_param
	      = builder.new_lifetime_param (lifetime_param);
	    impl_generics.push_back (std::move (impl_lifetime_param));
	  }
	  break;

	  case GenericParam::Kind::Type: {
	    TypeParam &type_param = (TypeParam &) *generic.get ();

	    std::unique_ptr<Type> associated_type = builder.single_type_path (
	      type_param.get_type_representation ().as_string ());

	    GenericArg type_arg
	      = GenericArg::create_type (std::move (associated_type));
	    generic_args.push_back (std::move (type_arg));

	    auto impl_type_param = builder.new_type_param (type_param);
	    impl_generics.push_back (std::move (impl_type_param));
	  }
	  break;

	  case GenericParam::Kind::Const: {
	    rust_unreachable ();

	    // TODO
	    // const ConstGenericParam *const_param
	    //   = (const ConstGenericParam *) generic.get ();
	    // std::unique_ptr<Expr> const_expr = nullptr;

	    // GenericArg type_arg
	    //   = GenericArg::create_const (std::move (const_expr));
	    // generic_args.push_back (std::move (type_arg));
	  }
	  break;
	}
    }

  GenericArgs generic_args_for_self (lifetime_args, generic_args,
				     {} /*binding args*/, loc);
  std::unique_ptr<Type> self_type_path
    = impl_generics.empty ()
	? builder.single_type_path (name)
	: builder.single_generic_type_path (name, generic_args_for_self);

  return std::unique_ptr<Item> (
    new TraitImpl (clone, /* unsafe */ false,
		   /* exclam */ false, std::move (trait_items),
		   std::move (impl_generics), std::move (self_type_path),
		   WhereClause::create_empty (), Visibility::create_private (),
		   {}, {}, loc));
}

// TODO: Create new `make_qualified_call` helper function

DeriveClone::DeriveClone (location_t loc)
  : DeriveVisitor (loc), expanded (nullptr)
{}

std::unique_ptr<AST::Item>
DeriveClone::go (Item &item)
{
  item.accept_vis (*this);

  rust_assert (expanded);

  return std::move (expanded);
}

void
DeriveClone::visit_tuple (TupleStruct &item)
{
  auto cloned_fields = std::vector<std::unique_ptr<Expr>> ();

  for (size_t idx = 0; idx < item.get_fields ().size (); idx++)
    cloned_fields.emplace_back (
      clone_call (builder.ref (builder.tuple_idx ("self", idx))));

  auto path = std::unique_ptr<Expr> (new PathInExpression (
    builder.path_in_expression ({item.get_identifier ().as_string ()})));
  auto constructor = builder.call (std::move (path), std::move (cloned_fields));

  expanded = clone_impl (clone_fn (std::move (constructor)),
			 item.get_identifier ().as_string (),
			 item.get_generic_params ());
}

void
DeriveClone::visit_struct (StructStruct &item)
{
  if (item.is_unit_struct ())
    {
      auto unit_ctor
	= builder.struct_expr_struct (item.get_struct_name ().as_string ());
      expanded = clone_impl (clone_fn (std::move (unit_ctor)),
			     item.get_struct_name ().as_string (),
			     item.get_generic_params ());
      return;
    }

  auto cloned_fields = std::vector<std::unique_ptr<StructExprField>> ();
  for (auto &field : item.get_fields ())
    {
      auto name = field.get_field_name ().as_string ();
      auto expr = clone_call (
	builder.ref (builder.field_access (builder.identifier ("self"), name)));

      cloned_fields.emplace_back (
	builder.struct_expr_field (std::move (name), std::move (expr)));
    }

  auto ctor = builder.struct_expr (item.get_struct_name ().as_string (),
				   std::move (cloned_fields));
  expanded = clone_impl (clone_fn (std::move (ctor)),
			 item.get_struct_name ().as_string (),
			 item.get_generic_params ());
}

void
DeriveClone::visit_enum (Enum &item)
{
  rust_sorry_at (item.get_locus (), "cannot derive %qs for these items yet",
		 "Clone");
}

void
DeriveClone::visit_union (Union &item)
{
  // FIXME: Should be $crate::core::clone::AssertParamIsCopy (or similar)

  // <Self>
  auto arg = GenericArg::create_type (builder.single_type_path ("Self"));

  // AssertParamIsCopy::<Self>
  auto type = std::unique_ptr<TypePathSegment> (
    new TypePathSegmentGeneric (PathIdentSegment ("AssertParamIsCopy", loc),
				false, GenericArgs ({}, {arg}, {}, loc), loc));
  auto type_paths = std::vector<std::unique_ptr<TypePathSegment>> ();
  type_paths.emplace_back (std::move (type));

  auto full_path
    = std::unique_ptr<Type> (new TypePath ({std::move (type_paths)}, loc));

  auto stmts = std::vector<std::unique_ptr<Stmt>> ();
  stmts.emplace_back (
    builder.let (builder.wildcard (), std::move (full_path), nullptr));
  auto tail_expr = builder.deref (builder.identifier ("self"));

  auto block = builder.block (std::move (stmts), std::move (tail_expr));

  expanded = clone_impl (clone_fn (std::move (block)),
			 item.get_identifier ().as_string (),
			 item.get_generic_params ());
}

} // namespace AST
} // namespace Rust
