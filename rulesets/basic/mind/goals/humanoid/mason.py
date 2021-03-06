#This file is distributed under the terms of the GNU General Public license.
#Copyright (C) 1999 Aloril (See the file COPYING for details).
#Copyright (C) 2001 Al Riddoch (See the file COPYING for details).

from mind.Goal import Goal
from mind.goals.common.misc_goal import keep
from mind.goals.dynamic.DynamicGoal import DynamicGoal
from mind.goals.dynamic.add_unique_goal import add_unique_goal
from atlas import *

class keep_livestock(keep):
    """Keep livestock that we own in a given location, calling them if 
       required."""
    def __init__(self, what, where, call):
        keep.__init__(self, what, where)
        self.call=call
    def keep_it(self,me):
        thing_all=me.find_thing(self.what)
        for thing in thing_all:
            if thing.location.velocity.is_valid() and thing.location.velocity.square_mag() > 0.1:
                return Operation("talk", Entity(say=self.call))
        return keep.keep_it(self,me)

class welcome(DynamicGoal):
    """Welcome entities of a given type that are created nearby."""
    def __init__(self, message, type, desc="welcome new players"):
        DynamicGoal.__init__(self,
                             trigger="sight_create",
                             desc=desc)
        self.type=type
        self.message=message
    def event(self, me, original_op, op):
        obj = me.map.update(op[0], op.getSeconds())
        if original_op.from_==me.id:
            self.add_thing(obj)
        if obj.type[0]==self.type:
            return Operation("talk", Entity(say=self.message))

class help(Goal):
    """Provide a sequence of help messages to a target."""
    def __init__(self, messages, responses, target):
        Goal.__init__(self,
                      "help",
                      self.message_complete,
                      [self.give_help])
        self.iter=0
        self.count=len(messages)
        self.messages=messages
        self.responses=responses
        self.target=target
        self.vars=["iter", "count", "messages", "responses", "target"]
    def message_complete(self, me):
        if self.iter>=self.count:
            self.irrelevant=1
            return 1
        return 0
    def give_help(self, me):
        #Check that the target hasn't disappeared
        targetEntity = me.map.get(self.target)
        if targetEntity == None:
            self.irrelevant = 1
            return
        
        message = self.messages[self.iter]
        self.iter+=1
        if self.iter == self.count - 1 and len(self.responses) != 0:
            ent=Entity(say=message, responses=self.responses)
        else:
            ent=Entity(say=message)
        return Operation("talk", ent) + me.face(targetEntity)

class add_help(add_unique_goal):
    """Set off a help goal if we get a touch operation."""
    def __init__(self, messages, responses=[], desc="help people out"):
        add_unique_goal.__init__(self,
                                 help,
                                 trigger="touch",
                                 desc=desc)
        self.messages=messages
        self.responses=responses
    def make_goal_instance(self, me, goal_class, original_op, op):
        return goal_class(self.messages, self.responses, original_op.from_)
