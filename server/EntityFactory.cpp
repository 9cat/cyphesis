// Cyphesis Online RPG Server and AI Engine
// Copyright (C) 2000-2005 Alistair Riddoch
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

// $Id: EntityFactory.cpp,v 1.108 2007-07-05 17:51:41 alriddoch Exp $

#include <Python.h>

#include "EntityFactory.h"

#include "CorePropertyManager.h"
#include "PersistantThingFactory.h"
#include "ScriptFactory.h"
#include "TaskFactory.h"
#include "ArithmeticFactory.h"
#include "Persistance.h"
#include "Persistor.h"
#include "Player.h"

#include "rulesets/Thing.h"
#include "rulesets/MindFactory.h"
#include "rulesets/Character.h"
#include "rulesets/Creator.h"
#include "rulesets/Plant.h"
#include "rulesets/Stackable.h"
#include "rulesets/Structure.h"
#include "rulesets/World.h"

#include "rulesets/Python_Script_Utils.h"

#include "common/id.h"
#include "common/log.h"
#include "common/debug.h"
#include "common/globals.h"
#include "common/const.h"
#include "common/inheritance.h"
#include "common/AtlasFileLoader.h"
#include "common/random.h"
#include "common/compose.hpp"

#include <Atlas/Message/Element.h>
#include <Atlas/Objects/Entity.h>
#include <Atlas/Objects/RootOperation.h>

#include <sys/types.h>
#ifdef HAVE_DIRENT_H
#include <dirent.h>
#endif // HAS_DIRENT_H


using Atlas::Message::Element;
using Atlas::Message::MapType;
using Atlas::Message::ListType;
using Atlas::Objects::Root;
using Atlas::Objects::Entity::RootEntity;

static const bool debug_flag = false;

EntityFactory * EntityFactory::m_instance = NULL;

EntityFactory::EntityFactory(BaseWorld & w) : m_world(w)
{
    if (consts::enable_persistence && database_flag) {
        installFactory("game_entity", "world", new ForbiddenThingFactory<World>());
        PersistantThingFactory<Thing> * tft = new PersistantThingFactory<Thing>();
        installFactory("game_entity", "thing", tft);
        installFactory("thing", "character",
                       new PersistantThingFactory<Character>());
        installFactory("character", "creator",
                       new PersistantThingFactory<Creator>());
        installFactory("thing", "plant", new PersistantThingFactory<Plant>());
        installFactory("thing", "stackable",
                       new PersistantThingFactory<Stackable>());
        installFactory("thing", "structure",
                       new PersistantThingFactory<Structure>());
    } else {
        installFactory("game_entity", "world", new ThingFactory<World>());
        ThingFactory<Thing> * tft = new ThingFactory<Thing>();
        installFactory("game_entity", "thing", tft);
        installFactory("thing", "character", new ThingFactory<Character>());
        installFactory("character", "creator", new ThingFactory<Creator>());
        installFactory("thing", "plant", new ThingFactory<Plant>());
        installFactory("thing", "stackable", new ThingFactory<Stackable>());
        installFactory("thing", "structure", new ThingFactory<Structure>());
    }

    m_statisticsFactories["settler"] = new PythonArithmeticFactory("world.statistics.Statistics", "Statistics");

    // The property manager instance installs itself at construction time.
    new CorePropertyManager();
}

EntityFactory::~EntityFactory()
{
    delete PropertyManager::instance();
}

void EntityFactory::initWorld()
{
    FactoryDict::const_iterator I = m_factories.find("world");
    if (I == m_factories.end()) {
        log(CRITICAL, "No world factory");
        return;
    }
    ForbiddenThingFactory<World> * wft = dynamic_cast<ForbiddenThingFactory<World> *>(I->second);
    if (wft == 0) {
        log(CRITICAL, "Its not a world factory");
        return;
    }
    wft->m_p.persist((World&)m_world.m_gameWorld);
}

Entity * EntityFactory::newEntity(const std::string & id, long intId,
                                  const std::string & type,
                                  const RootEntity & attributes) const
{
    debug(std::cout << "EntityFactor::newEntity()" << std::endl << std::flush;);
    Entity * thing = 0;
    FactoryDict::const_iterator I = m_factories.find(type);
    PersistorBase * pc = 0;
    if (I == m_factories.end()) {
        return 0;
    }
    FactoryBase * factory = I->second;
    thing = factory->newPersistantThing(id, intId, &pc);
    if (thing == 0) {
        return 0;
    }
    debug( std::cout << "[" << type << " " << thing->getName() << "]"
                     << std::endl << std::flush;);
    thing->setType(type);
    // Sort out python object
    if (factory->m_scriptFactory != 0) {
        debug(std::cout << "Class " << type << " has a python class"
                        << std::endl << std::flush;);
        factory->m_scriptFactory->addScript(thing);
    }
    //
    factory->populate(*thing);

    // Read the defaults
    thing->merge(factory->m_attributes);
    // And then override with the values provided for this entity.
    thing->merge(attributes->asMessage());
    // Get location from entity, if it is present
    // The default attributes cannot contain info on location
    if (attributes->hasAttrFlag(Atlas::Objects::Entity::LOC_FLAG)) {
        const std::string & loc_id = attributes->getLoc();
        thing->m_location.m_loc = m_world.getEntity(loc_id);
    }
    if (thing->m_location.m_loc == 0) {
        // If no info was provided, put the entity in the game world
        thing->m_location.m_loc = &m_world.m_gameWorld;
    }
    thing->m_location.readFromEntity(attributes);
    if (!thing->m_location.pos().isValid()) {
        // If no position coords were provided, put it somewhere near origin
        thing->m_location.m_pos = Point3D(uniform(-8,8), uniform(-8,8), 0);
    }
    if (thing->m_location.velocity().isValid()) {
        if (attributes->hasAttrFlag(Atlas::Objects::Entity::VELOCITY_FLAG)) {
            log(ERROR, String::compose("EntityFactory::newEntity(%1, %2): Entity has velocity set from the attributes given by the creator", id, type).c_str());
        } else {
            log(ERROR, String::compose("EntityFactory::newEntity(%1, %2): Entity has velocity set from an unknown source", id, type).c_str());
        }
        thing->m_location.m_velocity.setValid(false);
    }
    if (pc != 0) {
        pc->persist();
        thing->clearUpdateFlags();
    }
    delete pc;
    return thing;
}

Task * EntityFactory::newTask(const std::string & name, Character & owner) const
{
    TaskFactoryDict::const_iterator I = m_taskFactories.find(name);
    if (I == m_taskFactories.end()) {
        return 0;
    }
    return I->second->newTask(owner);
}

Task * EntityFactory::activateTask(const std::string & tool,
                                   const std::string & op,
                                   const std::string & target,
                                   Character & owner) const
{
    TaskFactoryActivationDict::const_iterator I = m_taskActivations.find(tool);
    if (I == m_taskActivations.end()) {
        return 0;
    }
    const TaskFactoryMultimap & dict = I->second;
    TaskFactoryMultimap::const_iterator J = dict.lower_bound(op);
    if (J == dict.end()) {
        return 0;
    }
    TaskFactoryMultimap::const_iterator Jend = dict.upper_bound(op);
    for (; J != Jend; ++J) {
        if (!J->second->m_target.empty()) {
            if (!Inheritance::instance().isTypeOf(target, J->second->m_target)) {
                debug( std::cout << target << " is not a " << J->second->m_target
                                 << std::endl << std::flush; );
                continue;
            }
        }
        return J->second->newTask(owner);
    }
    return 0;
}

int EntityFactory::addStatisticsScript(Character & chr) const
{
    StatisticsFactoryDict::const_iterator I = m_statisticsFactories.begin();
    if (I == m_statisticsFactories.end()) {
        return -1;
    }
    I->second->newScript(chr);
    return 0;
}

void EntityFactory::flushFactories()
{
    FactoryDict::const_iterator Iend = m_factories.end();
    for (FactoryDict::const_iterator I = m_factories.begin(); I != Iend; ++I) {
        delete I->second;
    }
    m_factories.clear();
    StatisticsFactoryDict::const_iterator J = m_statisticsFactories.begin();
    StatisticsFactoryDict::const_iterator Jend = m_statisticsFactories.end();
    for (; J != Jend; ++J) {
        delete J->second;
    }
    m_statisticsFactories.clear();
    TaskFactoryDict::const_iterator K = m_taskFactories.begin();
    TaskFactoryDict::const_iterator Kend = m_taskFactories.end();
    for (; K != Kend; ++K) {
        delete K->second;
    }
    m_taskFactories.clear();
}

void EntityFactory::populateFactory(const std::string & className,
                                    FactoryBase * factory,
                                    const MapType & classDesc)
{
    // Establish whether this rule has an associated script, and
    // if so, use it.
    MapType::const_iterator J = classDesc.find("script");
    MapType::const_iterator Jend = classDesc.end();
    if ((J != Jend) && (J->second.isMap())) {
        const MapType & script = J->second.asMap();
        J = script.find("name");
        if ((J != script.end()) && (J->second.isString())) {
            const std::string & script_name = J->second.String();
            J = script.find("language");
            if ((J != script.end()) && (J->second.isString())) {
                const std::string & script_language = J->second.String();
                if (factory->m_scriptFactory != 0) {
                    if (factory->m_scriptFactory->package() != script_name) {
                        delete factory->m_scriptFactory;
                        factory->m_scriptFactory = 0;
                    }
                }
                if (factory->m_scriptFactory == 0) {
                    if (script_language == "python") {
                        factory->m_scriptFactory = new PythonScriptFactory(script_name, className);
                    } else {
                        log(ERROR, "Unknown script language.");
                    }
                }
            }
        }
    }

    // Establish whether this rule has an associated mind rule,
    // and handle it.
    J = classDesc.find("mind");
    if ((J != Jend) && (J->second.isMap())) {
        const MapType & script = J->second.asMap();
        J = script.find("name");
        if ((J != script.end()) && (J->second.isString())) {
            const std::string & mindType = J->second.String();
            // language is unused. might need it one day
            // J = script.find("language");
            // if ((J != script.end()) && (J->second.isString())) {
                // const std::string & mindLang = J->second.String();
            // }
            MindFactory::instance()->addMindType(className, mindType);
        }
    }

    // Store the default attribute for entities create by this rule.
    J = classDesc.find("attributes");
    if ((J != Jend) && (J->second.isMap())) {
        const MapType & attrs = J->second.asMap();
        MapType::const_iterator Kend = attrs.end();
        for (MapType::const_iterator K = attrs.begin(); K != Kend; ++K) {
            if (!K->second.isMap()) {
                log(ERROR, String::compose("Attribute description in rule %1 is not a map.", className).c_str());
                continue;
            }
            const MapType & attr = K->second.asMap();
            MapType::const_iterator L = attr.find("default");
            if (L != attr.end()) {
                // Store this value in the defaults for this class
                factory->m_classAttributes[K->first] = L->second;
                // and merge it with the defaults inherited from the parent
                factory->m_attributes[K->first] = L->second;
            }
        }
    }

    // Check whether it should be available to players as a playable character.
    J = classDesc.find("playable");
    if ((J != Jend) && (J->second.isInt())) {
        Player::playableTypes.insert(className);
    }
}

bool EntityFactory::isTask(const std::string & className)
{
    if (className == "task") {
        return true;
    }
    return (m_taskFactories.find(className) != m_taskFactories.end());
}

int EntityFactory::installTaskClass(const std::string & className,
                                    const std::string & parent,
                                    const MapType & classDesc)
{
    Inheritance & i = Inheritance::instance();

    TaskFactoryDict::const_iterator I = m_taskFactories.find(className);
    if (I != m_taskFactories.end()) {
        log(ERROR, String::compose("Attempt to install task \"%1\" which is already installed", className).c_str());
    }
    
    TaskFactory * factory = 0;
    // Establish whether this rule has an associated script, and
    // if so, use it.
    MapType::const_iterator J = classDesc.find("script");
    MapType::const_iterator Jend = classDesc.end();
    if ((J != Jend) && J->second.isMap()) {
        const MapType & script = J->second.Map();
        J = script.find("name");
        if ((J != script.end()) && (J->second.isString())) {
            const std::string & script_name = J->second.String();
            J = script.find("language");
            if ((J != script.end()) && (J->second.isString())) {
                const std::string & script_language = J->second.String();
                if (script_language == "python") {
                    factory = new PythonTaskScriptFactory(script_name, className);
                } else {
                    log(ERROR, String::compose("Unknown script language \"%1\" for task \"%2\"", script_language, className).c_str());
                }
            }
        }
    }

    if (factory == 0) {
        return -1;
    }

    J = classDesc.find("activation");
    if ((J != Jend) && J->second.isMap()) {
        const MapType & activation = J->second.Map();
        MapType::const_iterator act_end = activation.end();
        J = activation.find("tool");
        if (J != act_end && J->second.isString()) {
            const std::string & activation_tool = J->second.String();
            J = activation.find("operation");
            if (J != act_end && J->second.isString()) {
                const std::string & activation_op = J->second.String();
                if (!i.hasClass(activation_op)) {
                    log(WARNING, String::compose("Activation op_definition \"%1\" does not exist for task class \"%2\".", activation_op, className).c_str());
                }
                m_taskActivations[activation_tool].insert(std::make_pair(activation_op, factory));
            }
        }
        J = activation.find("target");
        if (J != act_end && J->second.isString()) {
            const std::string & target_base = J->second.String();
            factory->m_target = target_base;
        }
    }

    // std::cout << "Attempting to install " << className << " which is a "
              // << parent << std::endl << std::flush;
    m_taskFactories.insert(std::make_pair(className, factory));

    i.addChild(atlasClass(className, parent));

    return 0;
}

int EntityFactory::installEntityClass(const std::string & className,
                                      const std::string & parent,
                                      const MapType & classDesc)
{
    // Get the new factory for this rule
    FactoryDict::const_iterator I = m_factories.find(parent);
    if (I == m_factories.end()) {
        debug(std::cout << "class \"" << className
                        << "\" has non existant parent \"" << parent
                        << "\". Waiting." << std::endl << std::flush;);
        m_waitingRules.insert(make_pair(parent, make_pair(className, classDesc)));
        return 1;
    }
    FactoryBase * parent_factory = I->second;
    FactoryBase * factory = parent_factory->duplicateFactory();
    if (factory == 0) {
        log(ERROR, String::compose("Attempt to install rule \"%1\" which has parent \"%2\" which cannot be instantiated", className, parent).c_str());
        return -1;
    }

    assert(factory->m_parent == parent_factory);

    // Copy the defaults from the parent. In populateFactory this may be
    // overriden with the defaults for this class.
    factory->m_attributes = parent_factory->m_attributes;

    populateFactory(className, factory, classDesc);

    debug(std::cout << "INSTALLING " << className << ":" << parent
                    << std::endl << std::flush;);

    // Install the factory in place.
    installFactory(parent, className, factory);

    // Add it as a child to its parent.
    parent_factory->m_children.insert(factory);

    return 0;
}

int EntityFactory::installOpDefinition(const std::string & opDefName,
                                       const std::string & parent,
                                       const MapType & opDefDesc)
{
    Inheritance & i = Inheritance::instance();

    if (!i.hasClass(parent)) {
        debug(std::cout << "op_definition \"" << opDefName
                        << "\" has non existant parent \"" << parent
                        << "\". Waiting." << std::endl << std::flush;);
        m_waitingRules.insert(make_pair(parent, make_pair(opDefName, opDefDesc)));
        return 1;
    }

    Atlas::Objects::Root r = atlasOpDefinition(opDefName, parent);

    if (i.addChild(r) != 0) {
        return -1;
    }

    int op_no = Atlas::Objects::Factories::instance()->addFactory(opDefName, &Atlas::Objects::generic_factory);
    i.opInstall(opDefName, op_no);

    return 0;
}

int EntityFactory::installRule(const std::string & className,
                               const MapType & classDesc)
{
    MapType::const_iterator J = classDesc.find("objtype");
    MapType::const_iterator Jend = classDesc.end();
    if (J == Jend || !J->second.isString()) {
        std::string msg = std::string("Rule \"") + className 
                          + "\" has no objtype. Skipping.";
        log(ERROR, msg.c_str());
        return -1;
    }
    const std::string & objtype = J->second.String();
    J = classDesc.find("parents");
    if (J == Jend) {
        std::string msg = std::string("Rule \"") + className 
                          + "\" has no parents. Skipping.";
        log(ERROR, msg.c_str());
        return -1;
    }
    if (!J->second.isList()) {
        std::string msg = std::string("Rule \"") + className 
                          + "\" has malformed parents. Skipping.";
        log(ERROR, msg.c_str());
        return -1;
    }
    const ListType & parents = J->second.asList();
    if (parents.empty()) {
        std::string msg = std::string("Rule \"") + className 
                          + "\" has empty parents. Skipping.";
        log(ERROR, msg.c_str());
        return -1;
    }
    const Element & p1 = parents.front();
    if (!p1.isString() || p1.String().empty()) {
        std::string msg = std::string("Rule \"") + className 
                          + "\" has malformed first parent. Skipping.";
        log(ERROR, msg.c_str());
        return -1;
    }
    const std::string & parent = p1.String();
    if (objtype == "class") {
        if (isTask(parent)) {
            int ret = installTaskClass(className, parent, classDesc);
            if (ret != 0) {
                return ret;
            }
        } else {
            int ret = installEntityClass(className, parent, classDesc);
            if (ret != 0) {
                return ret;
            }
        }
    } else if (objtype == "op_definition") {
        int ret = installOpDefinition(className, parent, classDesc);
        if (ret != 0) {
            return ret;
        }
    } else {
        std::string msg = std::string("Rule \"") + className 
                          + "\" has unknown objtype=\"" + objtype
                          + "\". Skipping.";
        log(ERROR, msg.c_str());
        return -1;
    }

    // Install any rules that were waiting for this rule before they
    // could be installed
    RuleWaitList::iterator I = m_waitingRules.lower_bound(className);
    RuleWaitList::iterator Iend = m_waitingRules.upper_bound(className);
    for (; I != Iend; ++I) {
        const std::string & wClassName = I->second.first;
        const MapType & wClassDesc = I->second.second;
        debug(std::cout << "WAITING rule " << wClassName
                        << " now ready" << std::endl << std::flush;);
        installRule(wClassName, wClassDesc);
    }
    m_waitingRules.erase(className);
    return 0;
}

static void updateChildren(FactoryBase * factory)
{
    std::set<FactoryBase *>::const_iterator I = factory->m_children.begin();
    std::set<FactoryBase *>::const_iterator Iend = factory->m_children.end();
    for (; I != Iend; ++I) {
        FactoryBase * child_factory = *I;
        child_factory->m_attributes = factory->m_attributes;
        MapType::const_iterator J = child_factory->m_classAttributes.begin();
        MapType::const_iterator Jend = child_factory->m_classAttributes.end();
        for (; J != Jend; ++J) {
            child_factory->m_attributes[J->first] = J->second;
        }
        updateChildren(child_factory);
    }
}

int EntityFactory::modifyEntityClass(const std::string & className,
                                     const Root & classDesc)
{
    FactoryDict::const_iterator I = m_factories.find(className);
    if (I == m_factories.end()) {
        log(ERROR, String::compose("Could not find factory for existing entity class \"%1\".", className).c_str());
        return -1;
    }
    FactoryBase * factory = I->second;
    assert(factory != 0);
    
    ScriptFactory * script_factory = factory->m_scriptFactory;
    if (script_factory != 0) {
        script_factory->refreshClass();
    }

    // Copy the defaults from the parent. In populateFactory this may be
    // overriden with the defaults for this class.
    if (factory->m_parent != 0) {
        factory->m_attributes = factory->m_parent->m_attributes;
    } else {
        // This is non fatal, but nice to know it has happened.
        // This should only happen if the client attempted to modify the
        // type data for a core hard coded type.
        log(ERROR, String::compose("EntityFactory::modifyEntityClass: \"%1\" modified by client, so has no parent factory", className).c_str());
    }
    factory->m_classAttributes = MapType();

    populateFactory(className, factory, classDesc->asMessage());

    updateChildren(factory);

    return 0;
}

int EntityFactory::modifyTaskClass(const std::string & class_name,
                                   const Root & classDesc)
{
    TaskFactoryDict::const_iterator I = m_taskFactories.find(class_name);
    if (I == m_taskFactories.end()) {
        log(ERROR, String::compose("Could not find factory for existing task class \"%1\"", class_name).c_str());
        return -1;
    }
    // FIXME Actually update the task factory.
    // TaskFactory * factory = I->second;
    // assert(factory != 0);

    return 0;
}

int EntityFactory::modifyOpDefinition(const std::string & class_name,
                                      const Root & classDesc)
{
    // Nothing to actually do
    return 0;
}

int EntityFactory::modifyRule(const std::string & class_name,
                              const Root & classDesc)
{
    Root o = Inheritance::instance().getClass(class_name);
    if (!o.isValid()) {
        log(ERROR, String::compose("Could not find existing type \"%1\" in inheritance", class_name).c_str());
        return -1;
    }
    if (o->getParents().front() == "task") {
        return modifyTaskClass(class_name, classDesc);
    } else if (classDesc->getObjtype() == "op_definition") {
        return modifyOpDefinition(class_name, classDesc);
    } else {
        return modifyEntityClass(class_name, classDesc);
    }
}

void EntityFactory::getRulesFromFiles(MapType & rules)
{
    std::string filename;

    std::string dirname = etc_directory + "/cyphesis/" + ruleset + ".d";
    DIR * rules_dir = ::opendir(dirname.c_str());
    if (rules_dir == 0) {
        filename = etc_directory + "/cyphesis/" + ruleset + ".xml";
        AtlasFileLoader f(filename, rules);
        if (f.isOpen()) {
            log(WARNING, String::compose("Reading legacy rule data from \"%1\".",
                                         filename).c_str());
            f.read();
        }
        return;
    }
    while (struct dirent * rules_entry = ::readdir(rules_dir)) {
        if (rules_entry->d_name[0] == '.') {
            continue;
        }
        filename = dirname + "/" + rules_entry->d_name;
        
        AtlasFileLoader f(filename, rules);
        if (!f.isOpen()) {
            log(ERROR, String::compose("Unable to open rule file \"%1\".",
                                       filename).c_str());
        } else {
            f.read();
        }
    }
}

void EntityFactory::installRules()
{
    MapType ruleTable;

    if (database_flag) {
        Persistance * p = Persistance::instance();
        p->getRules(ruleTable);
    } else {
        getRulesFromFiles(ruleTable);
    }

    if (ruleTable.empty()) {
        log(ERROR, "Rule database table contains no rules.");
        if (consts::enable_database) {
            log(NOTICE, "Attempting to load temporary ruleset from files.");
            getRulesFromFiles(ruleTable);
        }
    }

    MapType::const_iterator Iend = ruleTable.end();
    for (MapType::const_iterator I = ruleTable.begin(); I != Iend; ++I) {
        const std::string & className = I->first;
        const MapType & classDesc = I->second.asMap();
        installRule(className, classDesc);
    }
    // Report on the non-cleared rules.
    // Perhaps we can keep them too?
    // m_waitingRules.clear();
    RuleWaitList::const_iterator J = m_waitingRules.begin();
    RuleWaitList::const_iterator Jend = m_waitingRules.end();
    for (; J != Jend; ++J) {
        const std::string & wParentName = J->first;
        log(ERROR, String::compose("Rule \"%1\" with parent \"%2\" is an orphan", J->second.first, wParentName).c_str());
    }
}

void EntityFactory::installFactory(const std::string & parent,
                                   const std::string & className,
                                   FactoryBase * factory)
{
    assert(factory != 0);

    m_factories[className] = factory;

    Inheritance & i = Inheritance::instance();

    i.addChild(atlasClass(className, parent));
}

FactoryBase * EntityFactory::getNewFactory(const std::string & parent)
{
    FactoryDict::const_iterator I = m_factories.find(parent);
    if (I == m_factories.end()) {
        return 0;
    }
    return I->second->duplicateFactory();
}
