// Cyphesis Online RPG Server and AI Engine
// Copyright (C) 2000-2007 Alistair Riddoch
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

// $Id: LocatedEntity.h,v 1.1 2007-12-02 19:20:55 alriddoch Exp $

#ifndef RULESETS_LOCATED_ENTITY_H
#define RULESETS_LOCATED_ENTITY_H

#include "modules/Location.h"

#include "common/BaseEntity.h"

class LocatedEntity;

typedef std::set<Entity *> LocatedEntitySet;

class LocatedEntity : public BaseEntity {
  private:
    /// Count of references held by other objects to this entity
    int m_refCount;
  protected:
    /// Map of non-hardcoded attributes
    Atlas::Message::MapType m_attributes;

    /// Sequence number
    int m_seq;

  public:
    /// Full details of location
    Location m_location;
    /// List of entities which use this as ref
    LocatedEntitySet m_contains;

    explicit LocatedEntity(const std::string & id, long intId);
    virtual ~LocatedEntity();

    /// \brief Increment the reference count on this entity
    void incRef() {
        ++m_refCount;
    }

    /// \brief Decrement the reference count on this entity
    void decRef() {
        if (m_refCount <= 0) {
            assert(m_refCount == 0);
            delete this;
        } else {
            --m_refCount;
        }
    }

    /// \brief Check the reference count on this entity
    int checkRef() const {
        return m_refCount;
    }

    /// \brief Accessor for soft attribute map
    const Atlas::Message::MapType & getAttributes() const {
        return m_attributes;
    }

    virtual bool hasAttr(const std::string & name) const;
    virtual bool getAttr(const std::string & name,
                         Atlas::Message::Element &) const;
    virtual void setAttr(const std::string & name,
                         const Atlas::Message::Element &);

    void changeContainer(Entity *);
};

#endif // RULESETS_LOCATED_ENTITY_H
