// { dg-do compile { target c++11 } }

// Copyright (C) 2011-2025 Free Software Foundation, Inc.
//
// This file is part of the GNU ISO C++ Library.  This library is free
// software; you can redistribute it and/or modify it under the
// terms of the GNU General Public License as published by the
// Free Software Foundation; either version 3, or (at your option)
// any later version.

// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License along
// with this library; see the file COPYING3.  If not see
// <http://www.gnu.org/licenses/>.

// 20.4.2.1 [tuple.cnstr] Allocator-extended constructors

#include <memory>
#include <tuple>

struct MyAlloc { };

struct Type
{
  typedef MyAlloc allocator_type; // uses_allocator<Type, MyAlloc> is true

  explicit Type(int) { }

  Type(std::allocator_arg_t, MyAlloc) { }
  Type(MyAlloc) { }
};

void test01()
{
  using std::allocator_arg;
  using std::tuple;

  MyAlloc a;

  tuple<Type> t(allocator_arg, a, 1);
}
// { dg-error "failed: .* uses_allocator is true" "" { target *-*-* } 0 }
// { dg-error "no matching function for call" "" { target c++2a } 0 }
