#include <Atlas/Message/Object.h>
#include <Atlas/Objects/Root.h>
#include <Atlas/Objects/Operation/Login.h>
#include <Atlas/Objects/Operation/Create.h>

#include "Player.h"

oplist Player::character_error(const Create & op, const Message::Object & ent)
{
    Message::Object::MapType entmap = ent.AsMap();

    if ((entmap.find("name")==entmap.end()) || !entmap["name"].IsString()) {
        return error(op, "Object to be created has no name");
    }

    if (!entmap["name"].AsString().compare("admin", 0, 5)) {
        return error(op, "Object to be created cannot start with admin");
    }

    const string & type = entmap["parents"].AsList().front().AsString();
    
    if ((type!="character") && (type!="farmer") && (type!="smith")) {
        return error(op, "Object of that type cannot be created by this account");
    }
    oplist res;
    return(res);
}
