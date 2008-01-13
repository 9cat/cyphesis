// Cyphesis Online RPG Server and AI Engine
// Copyright (C) 2007 Alistair Riddoch
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation,
// Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA

// $Id: OperationRouter.h,v 1.2 2008-01-13 01:32:55 alriddoch Exp $

#ifndef COMMON_OPERATION_ROUTER_H
#define COMMON_OPERATION_ROUTER_H

#include <Atlas/Objects/ObjectsFwd.h>

#include <map>
#include <vector>

#define OP_INVALID (-1)

class Entity;

typedef Atlas::Objects::Operation::RootOperation Operation;

typedef std::vector<Operation> OpVector;
typedef int OpNo;

typedef enum {
    OPERATION_BLOCKED, // Handler has determined that op should stop here
    OPERATION_HANDLED, // Handler has done something, but op should continue
    OPERATION_IGNORED, // Handler has done nothing
} HandlerResult;

typedef HandlerResult (*Handler)(Entity *, const Operation &, OpVector &);
typedef std::map<int, Handler> HandlerMap;

/// \brief Interface class for all objects that can route operations around.
///
/// This class basically provides in interface for delivering operations to
/// an object.
class OperationRouter {
  public:
    virtual ~OperationRouter() = 0;
    virtual void operation(const Operation &, OpVector &) = 0;
};

#endif // COMMON_OPERATION_ROUTER_H
