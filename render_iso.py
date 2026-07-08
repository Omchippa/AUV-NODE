import cadquery as cq
from cadquery import exporters
import os
import runpy

OUT = os.path.dirname(os.path.abspath(__file__))
ns = runpy.run_path(os.path.join(OUT, "enclosure_model.py"))

tube = ns["tube"]
rear_cap = ns["rear_cap"]
front_cap = ns["front_cap"]
window = ns["window"]
TUBE_LENGTH = ns["TUBE_LENGTH"]
CAP_LIP_LENGTH = ns["CAP_LIP_LENGTH"]

assy = cq.Assembly(name="iso")
assy.add(tube, name="housing_tube", color=cq.Color(0.6, 0.6, 0.65, 1.0))
assy.add(rear_cap, name="rear_cap", loc=cq.Location(cq.Vector(0, 0, -CAP_LIP_LENGTH)), color=cq.Color(0.2, 0.2, 0.25, 1.0))
assy.add(front_cap, name="front_cap", loc=cq.Location(cq.Vector(0, 0, TUBE_LENGTH + CAP_LIP_LENGTH), cq.Vector(1, 0, 0), 180), color=cq.Color(0.2, 0.2, 0.25, 1.0))

exporters.export(assy.toCompound(), os.path.join(OUT, "render_iso_mated.svg"),
                  opt={"projectionDir": (1, -1, 1), "showAxes": False, "width": 900, "height": 700})

assy2 = cq.Assembly(name="iso_exp")
assy2.add(tube, name="housing_tube", color=cq.Color(0.6, 0.6, 0.65, 1.0))
assy2.add(rear_cap, name="rear_cap", loc=cq.Location(cq.Vector(0, 0, -CAP_LIP_LENGTH - 90)), color=cq.Color(0.2, 0.2, 0.25, 1.0))
assy2.add(front_cap, name="front_cap", loc=cq.Location(cq.Vector(0, 0, TUBE_LENGTH + CAP_LIP_LENGTH + 90), cq.Vector(1, 0, 0), 180), color=cq.Color(0.2, 0.2, 0.25, 1.0))
assy2.add(window, name="window", loc=cq.Location(cq.Vector(0, 0, TUBE_LENGTH + CAP_LIP_LENGTH + 90 + 25), cq.Vector(1, 0, 0), 180), color=cq.Color(0.6, 0.85, 0.95, 0.6))

exporters.export(assy2.toCompound(), os.path.join(OUT, "render_iso_exploded.svg"),
                  opt={"projectionDir": (1, -1, 1), "showAxes": False, "width": 900, "height": 900})

print("done")
