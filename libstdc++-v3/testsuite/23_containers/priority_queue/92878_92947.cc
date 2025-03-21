// { dg-do run { target c++20 } }

// Copyright (C) 2020-2025 Free Software Foundation, Inc.
//
// This file is part of the GNU ISO C++ Library.  This library is free
// software; you can redistribute it and/or modify it under the
// terms of the GNU General Public License as published by the
// Free Software Foundation; either version 3, or (at your option)
// any later version.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along
// with this library; see the file COPYING3.  If not see
// <http://www.gnu.org/licenses/>.

#include <queue>
#include <testsuite_hooks.h>

struct aggressive_aggregate
{
    int a;
    int b;
};

bool operator<(const aggressive_aggregate& a, const aggressive_aggregate& b)
{
  return a.a < b.a;
}

void test01()
{
  std::priority_queue<aggressive_aggregate> x;
  x.emplace();
  VERIFY(x.top().a == 0);
  VERIFY(x.top().b == 0);
  x.emplace(1, 2);
  VERIFY(x.top().a == 1);
  VERIFY(x.top().b == 2);
  x.emplace(2);
  VERIFY(x.top().a == 2);
  VERIFY(x.top().b == 0);
}

int main()
{
  test01();
}
