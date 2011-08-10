#This file is distributed under the terms of the GNU General Public license.
#Copyright (C) 2006 Al Riddoch (See the file COPYING for details).
#Copyright (C) 2011 Jekin Trivedi <jekintrivedi@gmail.com> (See the file COPYING for details).

from atlas import *
from physics import *
from physics import Quaternion
from physics import Point3D
from physics import Vector3D

import server

class Earthwall(server.Task):
    """ A task for creating Earthwall with a shovel."""

    materials = { 0: 'boulder', 1: 'sand', 2: 'earth', 3: 'silt', 4: 'ice' }
    def walls_operation(self, op):
        """ Op handler for cut op which activates this task """
        # print "Earthwall.walls"

        if len(op) < 1:
            sys.stderr.write("Earthwall task has no target in earthwall op")

        # FIXME Use weak references, once we have them
        self.target = op[0].id
        self.tool = op.to

        self.pos = Point3D(op[0].pos)

    def tick_operation(self, op):
        """ Op handler for regular tick op """
        # print "Earthwall.tick"

        target=server.world.get_object(self.target)
        self.pos = self.character.location.coordinates
        if not target:
            # print "Target is no more"
            self.irrelevant()
            return


        old_rate = self.rate

        self.rate = 0.5 / 0.75
        self.progress += 0.5

        if old_rate < 0.01:
            self.progress = 0

        # print "%s" % self.pos

        if self.progress < 1:
            # print "Not done yet"
            return self.next_tick(0.75)

        self.progress = 0


        res=Oplist()

        chunk_loc = Location(self.character.location.parent)
        chunk_loc.velocity = Vector3D()

        chunk_loc.coordinates = self.pos

        if not hasattr(self, 'terrain_mod'):
            mods = target.terrain.find_mods(self.pos)
            if len(mods) == 0:
                # There is no terrain mod where we are making wall,
                # so we check if it is in the materials , and if so create
                # a wall
                surface = target.terrain.get_surface(self.pos)
                # print "SURFACE %d at %s" % (surface, self.pos)
                if surface not in Earthwall.materials:
                    print "Not in material"
                    self.irrelevant()
                    return
                self.surface = surface

                z=self.character.location.coordinates.z + 1.0
                modmap = {
                          'height': z,
                          'shape': {
                                    'points': [[ -1.0, -1.0 ],
                                               [ -1.0, 1.0 ],
                                               [ 1.0, 1.0 ],
                                               [ 1.0, -1.0 ]],
                                    'type': 'polygon'
                                    },
                          'type': 'levelmod'                                    
                          }
                walls_create=Operation("create",
                                        Entity(name="walls",
                                               type="path",
                                               location = chunk_loc,
                                               terrainmod = modmap),
                                        to=target)
                res.append(walls_create)
            else:
                print mods
                for mod in mods:
                    if not hasattr(mod, 'name') or mod.name != 'walls':
                        print "%s is no good" % mod.id
                        continue
                    print "%s looks good" % mod.id
                    print mod.terrainmod
                    mod.terrainmod.height += 1.0
                    # We have modified the attribute in place, so must send an update op to propagate
                    res.append(Operation("update", to=mod.id))
                    break
            # self.terrain_mod = "moddy_mod_mod"



        create=Operation("create",
                         Entity(name = Earthwall.materials[self.surface],
                                type = Earthwall.materials[self.surface],
                                location = chunk_loc), to = target)
        res.append(create)

        res.append(self.next_tick(0.75))

        return res